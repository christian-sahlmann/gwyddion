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
    GtkWidget *graph;
    GtkWidget *interpolation;
    GtkWidget *output;
    GtkWidget *direction;
    GtkWidget *xy;
    GtkWidget *wh;
    gint interp;
    gint out;
    gint dir;
    gdouble mag;
    gint precision;
    gchar *units;
} SFunctionsControls;

static gboolean   module_register               (const gchar *name);
static void       sfunctions_use                   (GwyDataWindow *data_window,
                                                 GwyToolSwitchEvent reason);
static GtkWidget* sfunctions_dialog_create         (GwyDataView *data_view);
static void       sfunctions_do                    (void);
static void       sfunctions_selection_updated_cb  (void);
static void       sfunctions_dialog_response_cb    (gpointer unused,
                                                 gint response);
static void       sfunctions_dialog_abandon        (void);
static void       sfunctions_dialog_set_visible    (gboolean visible);
static void       interp_changed_cb             (GObject *item,
                                                 SFunctionsControls *pcontrols);
static void       output_changed_cb             (GObject *item,
                                                 SFunctionsControls *pcontrols);
static void       direction_changed_cb          (GObject *item,
                                                 SFunctionsControls *pcontrols);
static void       sfunctions_load_args             (GwyContainer *container,
                                                 SFunctionsControls *pcontrols);
static void       sfunctions_save_args             (GwyContainer *container,
                                                 SFunctionsControls *pcontrols);


static GtkWidget *sfunctions_dialog = NULL;
static SFunctionsControls controls;
static gulong updated_id = 0;
static gulong response_id = 0;
static GwyDataViewLayer *select_layer = NULL;

#define ROUND(x) ((gint)floor((x) + 0.5))

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "sfunctions",
    "Statistical functions.",
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
    static GwyToolFuncInfo sfunctions_func_info = {
        "sfunctions",
        "gwy_graph_halfgauss",
        "Compute 1D statistical functions.",
        77,
        sfunctions_use,
    };

    gwy_tool_func_register(name, &sfunctions_func_info);

    return TRUE;
}

static void
sfunctions_use(GwyDataWindow *data_window,
            GwyToolSwitchEvent reason)
{
    GwyVectorLayer *layer;
    GwyDataView *data_view;

    gwy_debug("%p", data_window);

    if (!data_window) {
        sfunctions_dialog_abandon();
        return;
    }

    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);
    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && (GwyDataViewLayer*)layer == select_layer)
        return;

    if (select_layer && updated_id)
        g_signal_handler_disconnect(select_layer, updated_id);

    if (layer && GWY_IS_LAYER_SELECT(layer))
        select_layer = GWY_DATA_VIEW_LAYER(layer);
    else {
        select_layer = (GwyDataViewLayer*)gwy_layer_select_new();
        gwy_data_view_set_top_layer(data_view, GWY_VECTOR_LAYER(select_layer));
    }

    sfunctions_load_args(gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent)),
                      &controls);

    if (!sfunctions_dialog)
    {
        sfunctions_dialog = sfunctions_dialog_create(data_view);
    }


    updated_id = g_signal_connect(select_layer, "updated",
                                   G_CALLBACK(sfunctions_selection_updated_cb),
                                   NULL);
    if (reason == GWY_TOOL_SWITCH_TOOL)
        sfunctions_dialog_set_visible(TRUE);

    if (controls.is_visible)
        sfunctions_selection_updated_cb();


}

static void
sfunctions_do(void)
{
    GtkWidget *window, *graph;
    GwyContainer *data;
    GwyDataField *datafield;
    gint is_selected;
    gdouble xmin, ymin, xmax, ymax;
    gchar *x_unit, *z_unit;
    gdouble x_mag, z_mag;
    gdouble xreal, yreal, x_max, unit;
    gdouble xm1, xm2, ym1, ym2;
    gdouble z_max;
    gint precision;
    GwyGraphAutoProperties prop;


    data = gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent));
    datafield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(datafield);
    yreal = gwy_data_field_get_yreal(datafield);


    is_selected = gwy_layer_select_get_selection(GWY_LAYER_SELECT(select_layer),
                                                 &xmin, &ymin, &xmax, &ymax);
    if (!is_selected) {
        xmin = 0;
        ymin = 0;
        xmax = xreal;
        ymax = yreal;
    }

    x_max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(datafield),
               yreal/gwy_data_field_get_yres(datafield));
    x_mag = gwy_math_humanize_numbers(unit, x_max, &precision);
    x_unit = g_strconcat(gwy_math_SI_prefix(x_mag), "m", NULL);

    z_max = gwy_data_field_get_max(datafield);
    z_mag = pow(10, (3*ROUND(((gdouble)((gint)(log10(fabs(z_max))))/3.0)))-3);
    z_unit = g_strconcat(gwy_math_SI_prefix(z_mag), "m", NULL);


    graph = gwy_graph_new();
    gwy_graph_get_autoproperties(GWY_GRAPH(graph), &prop);
    prop.is_point = 0;
    prop.is_line = 1;
    gwy_graph_set_autoproperties(GWY_GRAPH(graph), &prop);

    /* XXX: WFT? */
    xm1 = gwy_data_field_rtoj(datafield, xmin);
    ym1 = gwy_data_field_rtoj(datafield, ymin);
    xm2 = gwy_data_field_rtoj(datafield, xmax);
    ym2 = gwy_data_field_rtoj(datafield, ymax);

        /*
               gwy_graph_add_dataline_with_units(graph, dtl->pdata[i],
                               0, str->pdata[i], NULL,
                               x_mag, z_mag,
                               x_unit,
                               z_unit
                               );*/
    window = gwy_app_graph_window_create(graph);

    gwy_data_view_update(GWY_DATA_VIEW(select_layer->parent));
}

static void
sfunctions_dialog_abandon(void)
{
    gwy_debug("");
    if (select_layer && updated_id)
        g_signal_handler_disconnect(select_layer, updated_id);
    updated_id = 0;
    select_layer = NULL;
    if (sfunctions_dialog) {
        g_signal_handler_disconnect(sfunctions_dialog, response_id);
        gtk_widget_destroy(sfunctions_dialog);
        sfunctions_dialog = NULL;
        response_id = 0;
        controls.is_visible = FALSE;
    }

}

static GtkWidget*
sfunctions_dialog_create(GwyDataView *data_view)
{
    GwyContainer *data;
    GwyDataField *datafield;
    GtkWidget *dialog, *table, *label, *vbox;
    gdouble xreal, yreal, max, unit;

    gwy_debug("");
    data = gwy_data_view_get_data(data_view);
    datafield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                "/0/data"));
    xreal = gwy_data_field_get_xreal(datafield);
    yreal = gwy_data_field_get_yreal(datafield);

    max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(datafield),
                 yreal/gwy_data_field_get_yres(datafield));
    controls.mag = gwy_math_humanize_numbers(unit, max, &controls.precision);
    controls.units = g_strconcat(gwy_math_SI_prefix(controls.mag), "m", NULL);



    dialog = gtk_dialog_new_with_buttons(_("Statistical functions"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    g_signal_connect(dialog, "delete_event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);


    gtk_dialog_add_button(GTK_DIALOG(dialog), "Clear selection", 1);

    response_id = g_signal_connect(dialog, "response",
                      G_CALLBACK(sfunctions_dialog_response_cb), NULL);



    table = gtk_table_new(2, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    vbox = gtk_vbox_new(0,0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Area of computation</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, 0, 0, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Origin: (x, y)"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, 0, 0, 0);

    controls.xy = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.xy), 1.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), controls.xy, 0, 0, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Size: (w x h)"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, 0, 0, 0);

    controls.wh = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.wh), 1.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), controls.wh, 0, 0, 0);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Module parameters</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, 0, 0, 10);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, 0, 0, 2);

    controls.output
        = gwy_option_menu_sfunctions_output(G_CALLBACK(output_changed_cb),
                                        &controls, controls.out);
    gtk_box_pack_start(GTK_BOX(vbox), controls.output, 0, 0, 2);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Computation direction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, 0, 0, 2);

    controls.direction
        = gwy_option_menu_direction(G_CALLBACK(direction_changed_cb),
                                        &controls, controls.dir);
    gtk_box_pack_start(GTK_BOX(vbox), controls.direction, 0, 0, 2);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, 0, 0, 2);

    controls.interpolation
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        &controls, controls.interp);
    gtk_box_pack_start(GTK_BOX(vbox), controls.interpolation, 0, 0, 2);


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
    GwyContainer *data;
    GwyDataField *datafield;
    gdouble xreal, yreal, xmin, xmax, ymin, ymax;
    gchar buffer[50];

    data = gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent));
    datafield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(datafield);
    yreal = gwy_data_field_get_yreal(datafield);


    if (!(gwy_layer_select_get_selection(GWY_LAYER_SELECT(select_layer),
                                         &xmin, &ymin, &xmax, &ymax)))
    {
        xmin = 0;
        ymin = 0;
        xmax = xreal;
        ymax = yreal;
    }

    g_snprintf(buffer, sizeof(buffer), "%.*f, %.*f %s",
               controls.precision, xmin/controls.mag, controls.precision, ymin/controls.mag, controls.units);
    gtk_label_set_text(GTK_LABEL(controls.xy), buffer);

    g_snprintf(buffer, sizeof(buffer), "%.*f x %.*f %s",
               controls.precision, fabs(xmax-xmin)/controls.mag, controls.precision, fabs(ymax-ymin)/controls.mag, controls.units);
    gtk_label_set_text(GTK_LABEL(controls.wh), buffer);




}


static void
sfunctions_selection_updated_cb(void)
{
    GwyContainer *data;
    GwyDataField *datafield;
    GwyDataLine *dataline;
    gboolean is_visible, is_selected;
    gint j;
    gint xm1, xm2, ym1, ym2;
    GwyGraphAutoProperties prop;
    GString *lab;
    gchar *x_unit, *z_unit;
    gdouble x_mag, z_mag;
    gdouble xreal, yreal, x_max, unit;
    gdouble xmin, ymin, xmax, ymax;
    gdouble z_max;
    gint precision;

    gwy_debug("");

    data = gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent));
    datafield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(datafield);
    yreal = gwy_data_field_get_yreal(datafield);


    is_selected = gwy_layer_select_get_selection(GWY_LAYER_SELECT(select_layer),
                                                 &xmin, &ymin, &xmax, &ymax);
    if (!is_selected) {
        xmin = 0;
        ymin = 0;
        xmax = xreal;
        ymax = yreal;
    }


    is_visible = controls.is_visible;

    if (!is_visible)
        return;

    gwy_graph_get_autoproperties(GWY_GRAPH(controls.graph), &prop);
    prop.is_point = 0;
    prop.is_line = 1;
    gwy_graph_set_autoproperties(GWY_GRAPH(controls.graph), &prop);


    gwy_graph_clear(GWY_GRAPH(controls.graph));

    x_max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(datafield),
                   yreal/gwy_data_field_get_yres(datafield));
    x_mag = gwy_math_humanize_numbers(unit, x_max, &precision);
    x_unit = g_strconcat(gwy_math_SI_prefix(x_mag), "m", NULL);

    j = 0;
    xm1 = (gint)floor(gwy_data_field_rtoj(datafield, xmin)+0.5);
    ym1 = (gint)floor(gwy_data_field_rtoj(datafield, ymin)+0.5);
    xm2 = (gint)floor(gwy_data_field_rtoj(datafield, xmax)+0.5);
    ym2 = (gint)floor(gwy_data_field_rtoj(datafield, ymax)+0.5);


    dataline = gwy_data_line_new(10, 10, 0);
    lab = g_string_new("ble");

    if (gwy_data_field_get_line_stat_function(datafield,
                                          dataline,
                                          xm1,
                                          ym1,
                                          xm2,
                                          ym2,
                                          controls.out,
                                          controls.dir,
                                          controls.interp,
                                          GWY_WINDOWING_HANN,
                                          100)) 
    {

        /*this is to prevent problems with numbers as 1e34 in axis widget. FIXME by using appropriate units*/
        gwy_data_line_multiply(dataline, 100.0/gwy_data_line_get_max(dataline));
    
        z_max = gwy_data_line_get_max(dataline) -  gwy_data_line_get_min(dataline);
        z_mag = pow(10, (3*ROUND(((gdouble)((gint)(log10(fabs(z_max))))/3.0)))-3);
        z_unit = g_strconcat(gwy_math_SI_prefix(z_mag), "m", NULL);

        gwy_graph_add_dataline(GWY_GRAPH(controls.graph), dataline, 0, lab, NULL);
        /*
        gwy_graph_add_dataline_with_units(controls.graph, dataline,
                  0, "line", NULL,
                  x_mag, z_mag,
                  x_unit,
                  z_unit
                  );
       */

        gtk_widget_queue_draw(GTK_WIDGET(controls.graph));
        update_labels();
        
        g_free(z_unit);
 
    }
    g_free(x_unit);
    g_string_free(lab, TRUE);

    gwy_data_line_free(dataline); 

    if (!is_visible)
        sfunctions_dialog_set_visible(TRUE);

}

static void
sfunctions_clear(void)
{
    gwy_vector_layer_unselect(GWY_VECTOR_LAYER(select_layer));
    gwy_graph_clear(GWY_GRAPH(controls.graph));
    gtk_widget_queue_draw(GTK_WIDGET(controls.graph));
    update_labels();
    sfunctions_save_args(gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent)),
                      &controls);
}

static void
sfunctions_dialog_response_cb(G_GNUC_UNUSED gpointer unused,
                              gint response)
{
    gwy_debug("response %d", response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        sfunctions_clear();
        sfunctions_dialog_set_visible(FALSE);
        break;

        case GTK_RESPONSE_NONE:
        g_warning("Tool dialog destroyed.");
        sfunctions_use(NULL, 0);
        break;

        case GTK_RESPONSE_APPLY:
        sfunctions_do();
        break;

        case 1:
        sfunctions_clear();
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
sfunctions_dialog_set_visible(gboolean visible)
{
    gwy_debug("now %d, setting to %d",
              controls.is_visible, visible);
    if (controls.is_visible == visible)
        return;

    controls.is_visible = visible;
    if (visible)
        gtk_window_present(GTK_WINDOW(sfunctions_dialog));
    else
        gtk_widget_hide(sfunctions_dialog);
}

static void
interp_changed_cb(GObject *item, SFunctionsControls *pcontrols)
{
    gwy_debug("");
    pcontrols->interp = GPOINTER_TO_INT(g_object_get_data(item, "interpolation-type"));
    controls.interp = pcontrols->interp;
    sfunctions_selection_updated_cb();
}

static void
output_changed_cb(GObject *item, SFunctionsControls *pcontrols)
{
    gwy_debug("");
    pcontrols->out = GPOINTER_TO_INT(g_object_get_data(item, "sf-output-type"));
    controls.out = pcontrols->out;
    printf("pc(c)ontrols.out = %d\n", controls.out);
    sfunctions_selection_updated_cb();

}

static void
direction_changed_cb(GObject *item, SFunctionsControls *pcontrols)
{
    gwy_debug("");
    pcontrols->dir = GPOINTER_TO_INT(g_object_get_data(item, "direction-type"));
    controls.dir = pcontrols->dir;
    sfunctions_selection_updated_cb();

}


static const gchar *interp_key = "/tool/sfunctions/interp";
static const gchar *out_key = "/tool/sfunctions/out";
static const gchar *dir_key = "/tool/sfunctions/dir";



static void
sfunctions_load_args(GwyContainer *container, SFunctionsControls *pcontrols)
{
    gwy_debug("");
    if (gwy_container_contains_by_name(container, dir_key))
        pcontrols->dir = gwy_container_get_int32_by_name(container, dir_key);
    else pcontrols->dir = 0;

    if (gwy_container_contains_by_name(container, out_key))
        pcontrols->out = gwy_container_get_int32_by_name(container, out_key);
    else pcontrols->out = 0;

    if (gwy_container_contains_by_name(container, interp_key))
        pcontrols->interp = gwy_container_get_int32_by_name(container, interp_key);
    else pcontrols->interp = 2;
}

static void
sfunctions_save_args(GwyContainer *container, SFunctionsControls *pcontrols)
{
    gwy_container_set_int32_by_name(container, interp_key, controls.interp);
    gwy_container_set_int32_by_name(container, dir_key, controls.dir);
    gwy_container_set_int32_by_name(container, out_key, controls.out);

}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

