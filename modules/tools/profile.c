/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

#define NPROFILE 3

typedef struct {
    GwyUnitoolState *state;
    GtkWidget *graph;
    GtkWidget *interpolation;
    GtkWidget *separation;
    GPtrArray *positions;
    GPtrArray *dtl;
    GPtrArray *str;
    GtkObject *linesize;
    gint size;
    gint interp;
    gboolean separate;
} ToolControls;

static gboolean   module_register     (const gchar *name);
static gboolean   use                 (GwyDataWindow *data_window,
                                       GwyToolSwitchEvent reason);
static void       layer_setup         (GwyUnitoolState *state);
static GtkWidget* dialog_create       (GwyUnitoolState *state);
static void       dialog_update       (GwyUnitoolState *state,
                                       GwyUnitoolUpdateType reason);
static void       dialog_abandon      (GwyUnitoolState *state);
static void       apply               (GwyUnitoolState *state);
static void       interp_changed_cb   (GObject *item,
                                       ToolControls *controls);
static void       separate_changed_cb (GtkToggleButton *button,
                                       ToolControls *controls);
static void       size_changed_cb     (ToolControls *controls);
static void       load_args           (GwyContainer *container,
                                       ToolControls *controls);
static void       save_args           (GwyContainer *container,
                                       ToolControls *controls);



/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "profile",
    N_("Profile tool."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
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
        GWY_STOCK_PROFILE,
        N_("Extract profiles from data."),
        80,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerLines";
    static GwyUnitoolState *state = NULL;

    if (!state) {
        func_slots.layer_type = g_type_from_name(layer_name);
        if (!func_slots.layer_type) {
            g_warning("Layer type `%s' not available", layer_name);
            return FALSE;
        }
        state = g_new0(GwyUnitoolState, 1);
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
    }
    ((ToolControls*)state->user_data)->state = state;
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer, "max_lines", NPROFILE, NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *label, *vbox, *frame, *table2;
    GPtrArray *positions;
    gint i;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Extract profile"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_clear(dialog);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

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

    table2 = gtk_table_new(2, 2, FALSE);
    controls->linesize = gtk_adjustment_new(controls->size, 1, 20, 1, 5, 0);
    gwy_table_attach_spinbutton(table2, 0, _("Thickness"), _("pixels"),
                                controls->linesize);
    gtk_box_pack_start(GTK_BOX(vbox), table2, FALSE, FALSE, 2);

    g_signal_connect_swapped(controls->linesize, "value-changed",
                             G_CALLBACK(size_changed_cb), controls);

    controls->separation
        = gtk_check_button_new_with_mnemonic(_("_Separate profiles"));
    gtk_box_pack_start(GTK_BOX(vbox), controls->separation, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->separation),
                                 controls->separate);
    g_signal_connect(controls->separation, "toggled",
                     G_CALLBACK(separate_changed_cb), controls);

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2);

    controls->interpolation
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        controls, controls->interp);
    gtk_box_pack_start(GTK_BOX(vbox), controls->interpolation, FALSE, FALSE,2);

    gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);

    controls->graph = gwy_graph_new();
    gwy_graph_enable_axis_label_edit(GWY_GRAPH(controls->graph), FALSE);
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
    GwyDataViewLayer *layer;
    gdouble lines[4*NPROFILE];
    GPtrArray *positions;
    gchar buffer[64];
    gint i;
    gint nselected = 0;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    nselected = gwy_vector_layer_get_selection(state->layer, lines);
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    positions = controls->positions;

    gwy_debug("%d lines", nselected);
    for (i = 0; i < NPROFILE; i++) {
        if (i < nselected) {
            g_snprintf(buffer, sizeof(buffer), "x1 = %d, y1 = %d",
                       (gint)gwy_data_field_rtoj(dfield, lines[4*i]),
                       (gint)gwy_data_field_rtoi(dfield, lines[4*i+1]));
            gtk_label_set_markup(GTK_LABEL(positions->pdata[2*i]), buffer);

            g_snprintf(buffer, sizeof(buffer), "x2 = %d, y2 = %d",
                       (gint)gwy_data_field_rtoj(dfield, lines[4*i+2]),
                       (gint)gwy_data_field_rtoi(dfield, lines[4*i+3]));
            gtk_label_set_markup(GTK_LABEL(positions->pdata[2*i+1]), buffer);
        }
        else {
            g_snprintf(buffer, sizeof(buffer), " ");
            gtk_label_set_markup(GTK_LABEL(positions->pdata[2*i]), buffer);
            g_snprintf(buffer, sizeof(buffer), " ");
            gtk_label_set_markup(GTK_LABEL(positions->pdata[2*i+1]), buffer);
        }
    }
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    ToolControls *controls;
    GwySIValueFormat *units;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gdouble lines[4*NPROFILE];
    gboolean is_visible;
    gint nselected, i, j, lineres;
    gint xl1, xl2, yl1, yl2;
    GwyGraphAutoProperties prop;
    gchar *z_unit;
    gdouble z_mag, z_max;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    units = state->coord_format;
    is_visible = state->is_visible;
    nselected = gwy_vector_layer_get_selection(state->layer, lines);
    if (!is_visible && !nselected)
        return;

    gwy_graph_get_autoproperties(GWY_GRAPH(controls->graph), &prop);
    prop.is_point = 0;
    prop.is_line = 1;
    gwy_graph_set_autoproperties(GWY_GRAPH(controls->graph), &prop);

    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    /* TODO: use Unitool's value format */
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
            if (!gwy_data_field_get_data_line_averaged(dfield, controls->dtl->pdata[i],
                                              xl1, yl1, xl2, yl2, lineres, controls->size,
                                              controls->interp))
                continue;
            gwy_graph_add_dataline_with_units(GWY_GRAPH(controls->graph),
                                              controls->dtl->pdata[i],
                                              0, controls->str->pdata[i], NULL,
                                              units->magnitude, z_mag,
                                              units->units, z_unit);
        }
    }
    update_labels(state);
    g_free(z_unit);
    gwy_unitool_apply_set_sensitive(state, nselected);
}

static void
apply(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwySIValueFormat *units;
    GtkWidget *window, *graph;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gdouble lines[12];
    gint i, j, nselected;
    gchar *z_unit;
    gdouble z_mag, z_max;
    GwyGraphAutoProperties prop;

    controls = (ToolControls*)state->user_data;
    units = state->coord_format;
    nselected = gwy_vector_layer_get_selection(state->layer, lines);
    if (!nselected)
        return;

    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
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
                                              units->magnitude, z_mag,
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
                                              units->magnitude, z_mag,
                                              units->units, z_unit);
        }
        window = gwy_app_graph_window_create(graph);
    }
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    gint i;
    ToolControls *controls;
    GwyContainer *settings;

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    save_args(settings, controls);

    for (i=0; i<NPROFILE; i++)
    {
        g_object_unref(controls->dtl->pdata[i]);
    }

    g_ptr_array_free(controls->dtl, TRUE);
    g_ptr_array_free(controls->str, TRUE);
    g_ptr_array_free(controls->positions, TRUE);

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
interp_changed_cb(GObject *item, ToolControls *controls)
{
    controls->interp
        = GPOINTER_TO_INT(g_object_get_data(item, "interpolation-type"));

    gwy_debug("Interpolation set to %d\n", controls->interp);
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
separate_changed_cb(GtkToggleButton *button, ToolControls *controls)
{
    controls->separate = gtk_toggle_button_get_active(button);
}


static void
size_changed_cb(ToolControls *controls)
{
    controls->size = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->linesize));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static const gchar *separate_key = "/tool/profile/separate";
static const gchar *interp_key = "/tool/profile/interp";
static const gchar *size_key = "/tool/profile/size";

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->separate = FALSE;
    controls->interp = GWY_INTERPOLATION_BILINEAR;
    controls->size = 1;

    gwy_container_gis_boolean_by_name(container, separate_key,
                                      &controls->separate);
    gwy_container_gis_enum_by_name(container, interp_key, &controls->interp);
    gwy_container_gis_int32_by_name(container, size_key, &controls->size);
    /* sanitize */
    controls->separate = !!controls->separate;
    controls->interp = CLAMP(controls->interp,
                             GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    controls->size = CLAMP(controls->size, 1, 20);

}

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_boolean_by_name(container, separate_key,
                                      controls->separate);
    gwy_container_set_enum_by_name(container, interp_key, controls->interp);
    gwy_container_set_int32_by_name(container, size_key, controls->size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

