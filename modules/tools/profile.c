/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <math.h>
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/app.h>

typedef struct {
    gboolean is_visible;
    GPtrArray *positions;
    GtkWidget *graph;
    GtkWidget *interpolation;
    GtkWidget *separation;
    gint interp;
    gboolean separate;
} ProfileControls;

static gboolean   module_register               (const gchar *name);
static void       profile_use                   (GwyDataWindow *data_window,
                                                 GwyToolSwitchEvent reason);
static GtkWidget* profile_dialog_create         (GwyDataView *data_view);
static void       profile_do                    (void);
static void       profile_selection_updated_cb  (void);
static void       profile_dialog_response_cb    (gpointer unused,
                                                 gint response);
static void       profile_dialog_abandon        (void);
static void       profile_dialog_set_visible    (gboolean visible);
static void       interp_changed_cb             (GObject *item,
                                                 ProfileControls *controls);
static void       separate_changed_cb           (GtkToggleButton *button,
                                                 ProfileControls *controls);
static void       profile_load_args             (GwyContainer *container,
                                                 ProfileControls *controls);
static void       profile_save_args             (GwyContainer *container,
                                                 ProfileControls *controls);


static GtkWidget *dialog = NULL;
static ProfileControls controls;
static gulong updated_id = 0;
static gulong response_id = 0;
static GwyDataViewLayer *select_layer = NULL;
static GwyDataField *datafield = NULL;
static GPtrArray *dtl = NULL;
static GPtrArray *str = NULL;

#define MAX_N_OF_PROFILES 3
#define ROUND(x) ((gint)floor((x) + 0.5))

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "profile",
    "Profile tool.",
    "Petr Klapetek <petr@klapetek.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo profile_func_info = {
        "profile",
        "gwy_graph",
        "Extract profiles from data.",
        80,
        profile_use,
    };

    gwy_tool_func_register(name, &profile_func_info);

    return TRUE;
}

static void
profile_use(GwyDataWindow *data_window,
            GwyToolSwitchEvent reason)
{
    GwyDataViewLayer *layer;
    GwyDataView *data_view;
    gint i;

    gwy_debug("%p", data_window);

    if (!data_window) {
        profile_dialog_abandon();
        return;
    }

    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);

    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && layer == select_layer)
        return;

    if (select_layer && updated_id)
        g_signal_handler_disconnect(select_layer, updated_id);

    if (layer && GWY_IS_LAYER_SELECT(layer))
        select_layer = layer;
    else {
        select_layer = (GwyDataViewLayer*)gwy_layer_lines_new();
        gwy_data_view_set_top_layer(data_view, select_layer);
    }

    profile_load_args(gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent)),
                      &controls);

    if (!dialog)
        dialog = profile_dialog_create(data_view);


    if (!dtl)
    {
        dtl = g_ptr_array_new();
        for (i=0; i<5; i++) g_ptr_array_add(dtl, (gpointer)gwy_data_line_new(10, 10, 0));
    }
    if (!str)
    {
        str = g_ptr_array_new();
        g_ptr_array_add(str, (gpointer)g_string_new("Profile 1"));
        g_ptr_array_add(str, (gpointer)g_string_new("Profile 2"));
        g_ptr_array_add(str, (gpointer)g_string_new("Profile 3"));
        g_ptr_array_add(str, (gpointer)g_string_new("Profile 4"));
        g_ptr_array_add(str, (gpointer)g_string_new("Profile 5"));

    }

    updated_id = g_signal_connect(select_layer, "updated",
                                   G_CALLBACK(profile_selection_updated_cb),
                                   NULL);
    profile_selection_updated_cb();
}

static void
profile_do(void)
{
    GtkWidget *data_window;
    GtkWidget *window, *graph;
    GwyContainer *data;
    GwyDataField *datafield;
    gdouble lines[12];
    gint i, j, is_selected;
    gint x1, x2, y1, y2;
    gchar *x_unit, *z_unit;
    gdouble x_mag, z_mag;
    gdouble xreal, yreal, x_max, unit;
    gdouble z_max;
    gint precision;
    GwyGraphAutoProperties prop;

    if (!(is_selected=gwy_layer_lines_get_lines(select_layer, lines)))
        return;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent));
    datafield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    xreal = gwy_data_field_get_xreal(datafield);
    yreal = gwy_data_field_get_yreal(datafield);
    x_max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(datafield),
               yreal/gwy_data_field_get_yres(datafield));
    x_mag = gwy_math_humanize_numbers(unit, x_max, &precision);
    x_unit = g_strconcat(gwy_math_SI_prefix(x_mag), "m", NULL);

    z_max = gwy_data_field_get_max(datafield);
    z_mag = pow(10, (3*ROUND(((gdouble)((gint)(log10(fabs(z_max))))/3.0)))-3);
    z_unit = g_strconcat(gwy_math_SI_prefix(z_mag), "m", NULL);


    j=0;
    if (controls.separate)
    {
        for (i=0; i<is_selected; i++)
        {
            graph = gwy_graph_new();
            gwy_graph_get_autoproperties(graph, &prop);
            prop.is_point = 0;
            prop.is_line = 1;
            gwy_graph_set_autoproperties(graph, &prop);

            gwy_graph_add_dataline_with_units(graph, dtl->pdata[i],
                               0, str->pdata[i], NULL,
                               x_mag, z_mag,
                               x_unit,
                               z_unit
                               );

            window = gwy_app_graph_window_create((GwyGraph *)graph);

        }
    }
    else
    {
        graph = gwy_graph_new();
        gwy_graph_get_autoproperties(graph, &prop);
        prop.is_point = 0;
        prop.is_line = 1;
        gwy_graph_set_autoproperties(graph, &prop);

        for (i=0; i<is_selected; i++)
        {

            gwy_graph_add_dataline_with_units(graph, dtl->pdata[i],
                               0, str->pdata[i], NULL,
                               x_mag, z_mag,
                               x_unit,
                               z_unit
                               );
        }
        window = gwy_app_graph_window_create((GwyGraph *)graph);
    }


    gwy_data_view_update(GWY_DATA_VIEW(select_layer->parent));
}

static void
profile_dialog_abandon(void)
{
    guint i;

    gwy_debug("");
    if (select_layer && updated_id)
        g_signal_handler_disconnect(select_layer, updated_id);
    updated_id = 0;
    select_layer = NULL;
    if (dialog) {
        g_signal_handler_disconnect(dialog, response_id);
        gtk_widget_destroy(dialog);
        dialog = NULL;
        response_id = 0;
        controls.is_visible = FALSE;
    }

    if (dtl) g_ptr_array_free(dtl, 1);
    if (str) g_ptr_array_free(str, 1);
    dtl = NULL;
    str = NULL;

}

static GtkWidget*
profile_dialog_create(GwyDataView *data_view)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GtkWidget *dialog, *table, *label, *vbox;
    gdouble xreal, yreal, max, unit;
    gint i;

    gwy_debug("");
    data = gwy_data_view_get_data(data_view);
    datafield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(datafield);
    yreal = gwy_data_field_get_yreal(datafield);

    dialog = gtk_dialog_new_with_buttons(_("Extract profile"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    g_signal_connect(dialog, "delete_event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);


    gtk_dialog_add_button(dialog, "Clear selection", 1);

    response_id = g_signal_connect(dialog, "response",
                                   G_CALLBACK(profile_dialog_response_cb), NULL);



    table = gtk_table_new(2, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    vbox = gtk_vbox_new(0,0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Profile positions</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(vbox, label, 0, 0, 0);


    controls.positions = g_ptr_array_new();
    g_ptr_array_add(controls.positions, (gpointer)gtk_label_new(""));
    g_ptr_array_add(controls.positions, (gpointer)gtk_label_new(""));
    g_ptr_array_add(controls.positions, (gpointer)gtk_label_new(""));
    g_ptr_array_add(controls.positions, (gpointer)gtk_label_new(""));
    g_ptr_array_add(controls.positions, (gpointer)gtk_label_new(""));
    g_ptr_array_add(controls.positions, (gpointer)gtk_label_new(""));

    gtk_misc_set_alignment(GTK_MISC(controls.positions->pdata[0]), 0.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.positions->pdata[1]), 0.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.positions->pdata[2]), 0.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.positions->pdata[3]), 0.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.positions->pdata[4]), 0.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.positions->pdata[5]), 0.0, 0.5);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Profile 1:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(vbox, label, 0, 0, 5);
    gtk_box_pack_start(vbox, GTK_WIDGET(controls.positions->pdata[0]), 0, 0, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(controls.positions->pdata[1]), 0, 0, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Profile 2:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(vbox, label, 0, 0, 5);
    gtk_box_pack_start(vbox, GTK_WIDGET(controls.positions->pdata[2]), 0, 0, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(controls.positions->pdata[3]), 0, 0, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Profile 3:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(vbox, label, 0, 0, 5);
    gtk_box_pack_start(vbox, GTK_WIDGET(controls.positions->pdata[4]), 0, 0, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(controls.positions->pdata[5]), 0, 0, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Module parameters</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(vbox, label, 0, 0, 10);

    controls.separation = gtk_check_button_new_with_label("separate profiles");
    gtk_box_pack_start(vbox, controls.separation, 0, 0, 0);
    gtk_toggle_button_set_active(controls.separation, controls.separate);
    g_signal_connect(controls.separation, "toggled",
                     G_CALLBACK(separate_changed_cb), &controls);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(vbox, label, 0, 0, 2);

    controls.interpolation
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        &controls, controls.interp);
    gtk_box_pack_start(vbox, controls.interpolation, 0, 0, 2);


    gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);

    controls.graph = gwy_graph_new();
    gtk_table_attach(GTK_TABLE(table), controls.graph, 1, 2, 0, 1,
                     GTK_FILL, 0, 2, 2);

    gtk_widget_show_all(table);
    controls.is_visible = FALSE;

    return dialog;
}

static void
update_labels()
{
    gdouble lines[12];
    gchar buffer[50];
    gint i, j;
    gint n_of_lines=0;

    gwy_debug("");
    n_of_lines = gwy_layer_lines_get_lines(select_layer, lines);

    j=0;
    printf("%d lines.\n", n_of_lines);
    for (i=0; i<(2*(MAX_N_OF_PROFILES)); i++)
    {
        if (i<(2*n_of_lines))
        {
            g_snprintf(buffer, sizeof(buffer), "x2 = %d, y2 = %d",
                   (gint)gwy_data_field_rtoj(datafield, lines[j]),
                   (gint)gwy_data_field_rtoj(datafield, lines[j+1])
                   );
            j += 2;
            gtk_label_set_text(GTK_LABEL(controls.positions->pdata[i+1]), buffer);

            g_snprintf(buffer, sizeof(buffer), "x1 = %d, y1 = %d",
                   (gint)gwy_data_field_rtoj(datafield, lines[j]),
                   (gint)gwy_data_field_rtoj(datafield, lines[j+1])
                   );
            j += 2;
            gtk_label_set_text(GTK_LABEL(controls.positions->pdata[i]), buffer);
            i++;
        }
        else
        {
            g_snprintf(buffer, sizeof(buffer), " ");
            gtk_label_set_text(GTK_LABEL(controls.positions->pdata[i++]), buffer);
            g_snprintf(buffer, sizeof(buffer), " ");
            gtk_label_set_text(GTK_LABEL(controls.positions->pdata[i]), buffer);
        }
     }
}


static void
profile_selection_updated_cb(void)
{
    gdouble lines[12];
    gboolean is_visible, is_selected;
    GString *lab1, *lab2, *lab3;
    gint i, xres, j;
    gint x1, x2, y1, y2;
    GwyGraphAutoProperties prop;
    GwyGraph *gr;
    gchar *x_unit, *z_unit;
    gdouble x_mag, z_mag;
    gdouble xreal, yreal, x_max, unit;
    gdouble z_max;
    gint precision;

    gwy_debug("");

    is_visible = controls.is_visible;
    is_selected = gwy_layer_lines_get_lines(select_layer, lines);

    if (!is_visible && !is_selected)
        return;

    gwy_graph_get_autoproperties(controls.graph, &prop);
    prop.is_point = 0;
    prop.is_line = 1;
    gwy_graph_set_autoproperties(controls.graph, &prop);



    if (is_selected) {

        gwy_graph_clear(controls.graph);

        xreal = gwy_data_field_get_xreal(datafield);
        yreal = gwy_data_field_get_yreal(datafield);
        x_max = MAX(xreal, yreal);
        unit = MIN(xreal/gwy_data_field_get_xres(datafield),
                   yreal/gwy_data_field_get_yres(datafield));
        x_mag = gwy_math_humanize_numbers(unit, x_max, &precision);
        x_unit = g_strconcat(gwy_math_SI_prefix(x_mag), "m", NULL);

        z_max = gwy_data_field_get_max(datafield);
        z_mag = pow(10, (3*ROUND(((gdouble)((gint)(log10(fabs(z_max))))/3.0)))-3);
        z_unit = g_strconcat(gwy_math_SI_prefix(z_mag), "m", NULL);

        j = 0;
        for (i=0; i<is_selected; i++)
        {
            x2 = gwy_data_field_rtoj(datafield, lines[j++]);
            y2 = gwy_data_field_rtoj(datafield, lines[j++]);
            x1 = gwy_data_field_rtoj(datafield, lines[j++]);
            y1 = gwy_data_field_rtoj(datafield, lines[j++]);

            if (!gwy_data_field_get_data_line(datafield, dtl->pdata[i],
                                     x1, y1,
                                     x2, y2,
                                     300,/*(gint)sqrt((x1 - x2)*(x1 - x2) + (y1 - y2)*(y1 -y2)), jak to, ze to s timhle pada?*/
                                     GWY_INTERPOLATION_BILINEAR
                                     )) continue;
            gwy_graph_add_dataline_with_units(controls.graph, dtl->pdata[i],
                               0, str->pdata[i], NULL,
                               x_mag, z_mag,
                               x_unit,
                               z_unit
                               );

        }



        gtk_widget_queue_draw(GTK_WIDGET(controls.graph));
        update_labels();

        g_free(x_unit);
        g_free(z_unit);
    }

    if (!is_visible)
        profile_dialog_set_visible(TRUE);

}

static void
profile_clear(void)
{
    gwy_layer_lines_unselect(select_layer);
    gwy_graph_clear(controls.graph);
    gtk_widget_queue_draw(GTK_WIDGET(controls.graph));
    update_labels();
    profile_save_args(gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent)),
                      &controls);
}

static void
profile_dialog_response_cb(gpointer unused, gint response)
{
    gwy_debug("response %d", response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        profile_clear();
        profile_dialog_set_visible(FALSE);
        break;

        case GTK_RESPONSE_NONE:
        g_warning("Tool dialog destroyed.");
        profile_use(NULL, 0);
        break;

        case GTK_RESPONSE_APPLY:
        profile_do();
        break;

        case 1:
        profile_clear();
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
profile_dialog_set_visible(gboolean visible)
{
    gwy_debug("now %d, setting to %d",
              controls.is_visible, visible);
    if (controls.is_visible == visible)
        return;

    controls.is_visible = visible;
    if (visible)
        gtk_window_present(GTK_WINDOW(dialog));
    else
        gtk_widget_hide(dialog);
}

static void
interp_changed_cb(GObject *item, ProfileControls *controls)
{
    gwy_debug("");
    controls->interp = GPOINTER_TO_INT(g_object_get_data(item, "interpolation-type"));

    printf("Interpolation set to %d\n", controls->interp);
}

static void
separate_changed_cb(GtkToggleButton *button, ProfileControls *controls)
{
    controls->separate = gtk_toggle_button_get_active(button);
}

static const gchar *separate_key = "/tool/profile/separate";
static const gchar *interp_key = "/tool/profile/interp";


static void
profile_load_args(GwyContainer *container, ProfileControls *controls)
{
    gwy_debug("");
    if (gwy_container_contains_by_name(container, separate_key))
        controls->separate = gwy_container_get_boolean_by_name(container, separate_key);
    else controls->separate = 0;

    if (gwy_container_contains_by_name(container, interp_key))
        controls->interp = gwy_container_get_int32_by_name(container, interp_key);
    else controls->interp = 2;
}

static void
profile_save_args(GwyContainer *container, ProfileControls *controls)
{
    gwy_container_set_boolean_by_name(container, separate_key, controls->separate);
    gwy_container_set_int32_by_name(container, interp_key, controls->interp);
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

