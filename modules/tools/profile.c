/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/unitool.h>

#define NPROFILE 3

#define ROUND(x) ((gint)floor((x) + 0.5))

typedef struct {
    GtkWidget *graph;
    GtkWidget *interpolation;
    GtkWidget *separation;
    GPtrArray *positions;
    GPtrArray *dtl;
    GPtrArray *str;
    gint interp;
    gboolean separate;
} ToolControls;

static gboolean   module_register     (const gchar *name);
static void       use                 (GwyDataWindow *data_window,
                                       GwyToolSwitchEvent reason);
static void       layer_setup         (GwyUnitoolState *state);
static GtkWidget* dialog_create       (GwyUnitoolState *state);
static void       dialog_update       (GwyUnitoolState *state);
static void       dialog_abandon      (GwyUnitoolState *state);
static void       apply               (GwyUnitoolState *state);
static void       interp_changed_cb   (GObject *item,
                                       ToolControls *controls);
static void       separate_changed_cb (GtkToggleButton *button,
                                       ToolControls *controls);
static void       load_args           (GwyContainer *container,
                                       ToolControls *controls);
static void       save_args           (GwyContainer *container,
                                       ToolControls *controls);

static const gchar *separate_key = "/tool/profile/separate";
static const gchar *interp_key = "/tool/profile/interp";

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

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    gwy_layer_lines_new,           /* layer object constructor */
    layer_setup,                   /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    apply,                         /* apply action */
    NULL,                          /* nonstandard response handler */
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo func_info = {
        "profile",
        "gwy_profile",
        "Extract profiles from data.",
        80,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static void
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static GwyUnitoolState *state = NULL;

    if (!state) {
        state = g_new0(GwyUnitoolState, 1);
        func_slots.layer_type = GWY_TYPE_LAYER_LINES;
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
    }
    gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(GWY_IS_LAYER_LINES(state->layer));
    gwy_layer_lines_set_max_lines(GWY_LAYER_LINES(state->layer), NPROFILE);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *label, *vbox, *frame;
    GPtrArray *positions;
    gint i;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Extract profile"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLEAR,
                                         GWY_UNITOOL_RESPONSE_UNSELECT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);

    table = gtk_table_new(2, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    vbox = gtk_vbox_new(FALSE, 0);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Profile positions</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    controls->dtl = g_ptr_array_new();
    controls->str = g_ptr_array_new();
    controls->positions = positions = g_ptr_array_new();
    for (i = 0; i < NPROFILE; i++) {
        GString *s = g_string_new("");
        gchar *buf;

        g_string_printf(s, _("Profile %d"), i+1);
        g_ptr_array_add(controls->str, s);

        g_ptr_array_add(controls->dtl, gwy_data_line_new(10, 10, 0));

        buf = g_strdup_printf(_("Profile %d:"), i+1);
        label = gtk_label_new(buf);
        g_free(buf);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE,5);

        label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        g_ptr_array_add(positions, label);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE,0);

        label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        g_ptr_array_add(positions, label);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE,0);
    }

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Module parameters</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 10);

    controls->separation = gtk_check_button_new_with_label("separate profiles");
    gtk_box_pack_start(GTK_BOX(vbox), controls->separation, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->separation),
                                 controls->separate);
    g_signal_connect(controls->separation, "toggled",
                     G_CALLBACK(separate_changed_cb), &controls);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2);

    controls->interpolation
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        &controls, controls->interp);
    gtk_box_pack_start(GTK_BOX(vbox), controls->interpolation, FALSE, FALSE,2);

    gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);

    controls->graph = gwy_graph_new();
    gtk_table_attach(GTK_TABLE(table), controls->graph, 1, 2, 0, 1,
                     GTK_FILL, 0, 2, 2);

    return dialog;
}

static void
update_labels(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble lines[4*NPROFILE];
    GPtrArray *positions;
    gchar buffer[64];
    gint i, j;
    gint nselected = 0;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    nselected = gwy_layer_lines_get_lines(GWY_LAYER_LINES(state->layer), lines);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(state->layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    positions = controls->positions;

    j = 0;
    gwy_debug("%d lines", nselected);
    for (i = 0; i < 2*NPROFILE; i++) {
        if (i < 2*nselected) {
            g_snprintf(buffer, sizeof(buffer), "x2 = %d, y2 = %d",
                       (gint)gwy_data_field_rtoj(dfield, lines[j]),
                       (gint)gwy_data_field_rtoi(dfield, lines[j+1]));
            j += 2;
            gtk_label_set_text(GTK_LABEL(positions->pdata[i+1]), buffer);

            g_snprintf(buffer, sizeof(buffer), "x1 = %d, y1 = %d",
                       (gint)gwy_data_field_rtoj(dfield, lines[j]),
                       (gint)gwy_data_field_rtoi(dfield, lines[j+1]));
            j += 2;
            gtk_label_set_text(GTK_LABEL(positions->pdata[i]), buffer);
            i++;
        }
        else {
            g_snprintf(buffer, sizeof(buffer), " ");
            gtk_label_set_text(GTK_LABEL(positions->pdata[i++]), buffer);
            g_snprintf(buffer, sizeof(buffer), " ");
            gtk_label_set_text(GTK_LABEL(positions->pdata[i]), buffer);
        }
    }
}

static void
dialog_update(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyUnitoolUnits *units;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble lines[4*NPROFILE];
    gboolean is_visible;
    gint nselected, i, j, lineres;
    gint xl1, xl2, yl1, yl2;
    GwyGraphAutoProperties prop;
    gchar *z_unit;
    gdouble z_mag, z_max;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    units = &state->coord_units;
    is_visible = state->is_visible;
    nselected = gwy_layer_lines_get_lines(GWY_LAYER_LINES(state->layer), lines);
    if (!is_visible && !nselected)
        return;

    gwy_graph_get_autoproperties(GWY_GRAPH(controls->graph), &prop);
    prop.is_point = 0;
    prop.is_line = 1;
    gwy_graph_set_autoproperties(GWY_GRAPH(controls->graph), &prop);

    data = gwy_data_view_get_data(GWY_DATA_VIEW(state->layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    z_max = gwy_data_field_get_max(dfield);
    z_mag = pow(10, 3*ROUND(((gint)(log10(fabs(z_max))))/3.0) - 3);
    z_unit = g_strconcat(gwy_math_SI_prefix(z_mag), "m", NULL);

    gwy_graph_clear(GWY_GRAPH(controls->graph));
    if (nselected) {
        j = 0;
        for (i = 0; i < nselected; i++) {
            xl2 = gwy_data_field_rtoj(dfield, lines[j++]);
            yl2 = gwy_data_field_rtoi(dfield, lines[j++]);
            xl1 = gwy_data_field_rtoj(dfield, lines[j++]);
            yl1 = gwy_data_field_rtoi(dfield, lines[j++]);

            /* XXX jaktoze to s timhle pada?*/
            lineres = ROUND(sqrt((xl1 - xl2)*(xl1 - xl2)
                                 + (yl1 - yl2)*(yl1 - yl2)));
            lineres = MAX(lineres, 10);
            printf("interp: %d\n", controls->interp);
            if (!gwy_data_field_get_data_line(dfield, controls->dtl->pdata[i],
                                              xl1, yl1, xl2, yl2, lineres,
                                              controls->interp))
                continue;
            gwy_graph_add_dataline_with_units(GWY_GRAPH(controls->graph),
                                              controls->dtl->pdata[i],
                                              0, controls->str->pdata[i], NULL,
                                              units->mag, z_mag,
                                              units->units, z_unit);
        }
    }
    update_labels(state);
    g_free(z_unit);
}

static void
apply(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyUnitoolUnits *units;
    GtkWidget *window, *graph;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble lines[12];
    gint i, j, nselected;
    gchar *z_unit;
    gdouble z_mag, z_max;
    GwyGraphAutoProperties prop;

    controls = (ToolControls*)state->user_data;
    units = &state->coord_units;
    nselected = gwy_layer_lines_get_lines(GWY_LAYER_LINES(state->layer), lines);
    if (!nselected)
        return;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(state->layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    z_max = gwy_data_field_get_max(dfield);
    z_mag = pow(10, (3*ROUND(((gdouble)((gint)(log10(fabs(z_max))))/3.0)))-3);
    z_unit = g_strconcat(gwy_math_SI_prefix(z_mag), "m", NULL);

    j = 0;
    if (controls->separate) {
        for (i = 0; i < nselected; i++) {
            graph = gwy_graph_new();
            gwy_graph_get_autoproperties(GWY_GRAPH(graph), &prop);
            prop.is_point = 0;
            prop.is_line = 1;
            gwy_graph_set_autoproperties(GWY_GRAPH(graph), &prop);

            gwy_graph_add_dataline_with_units(GWY_GRAPH(graph),
                                              controls->dtl->pdata[i],
                                              0, controls->str->pdata[i], NULL,
                                              units->mag, z_mag,
                                              units->units, z_unit);

            window = gwy_app_graph_window_create(graph);
        }
    }
    else {
        graph = gwy_graph_new();
        gwy_graph_get_autoproperties(GWY_GRAPH(graph), &prop);
        prop.is_point = 0;
        prop.is_line = 1;
        gwy_graph_set_autoproperties(GWY_GRAPH(graph), &prop);

        for (i = 0; i < nselected; i++) {
            gwy_graph_add_dataline_with_units(GWY_GRAPH(graph),
                                              controls->dtl->pdata[i],
                                              0, controls->str->pdata[i], NULL,
                                              units->mag, z_mag,
                                              units->units, z_unit);
        }
        window = gwy_app_graph_window_create(graph);
    }
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    save_args(settings, controls);
    g_ptr_array_free(controls->dtl, TRUE);
    g_ptr_array_free(controls->str, TRUE);
    g_ptr_array_free(controls->positions, TRUE);

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
interp_changed_cb(GObject *item, ToolControls *controls)
{
    gwy_debug("");
    controls->interp
        = GPOINTER_TO_INT(g_object_get_data(item, "interpolation-type"));

    gwy_debug("Interpolation set to %d\n", controls->interp);
}

static void
separate_changed_cb(GtkToggleButton *button, ToolControls *controls)
{
    controls->separate = gtk_toggle_button_get_active(button);
}

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    gwy_debug("");
    if (gwy_container_contains_by_name(container, separate_key))
        controls->separate = gwy_container_get_boolean_by_name(container,
                                                               separate_key);
    else
        controls->separate = FALSE;

    if (gwy_container_contains_by_name(container, interp_key))
        controls->interp = gwy_container_get_int32_by_name(container,
                                                           interp_key);
    else
        controls->interp = GWY_INTERPOLATION_BILINEAR;

    gwy_debug("Interpolation loaded as %d\n", controls->interp);
}

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_boolean_by_name(container, separate_key,
                                      controls->separate);
    gwy_container_set_int32_by_name(container, interp_key,
                                    controls->interp);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

