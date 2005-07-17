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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/level.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef struct {
    GtkWidget *coords[6];
    GtkWidget *values[3];
    GtkWidget *xunits;
    GtkWidget *yunits;
    GtkWidget *zunits;
    GtkObject *radius;
} ToolControls;

static gboolean   module_register  (const gchar *name);
static gboolean   use              (GwyDataWindow *data_window,
                                    GwyToolSwitchEvent reason);
static void       layer_setup      (GwyUnitoolState *state);
static GtkWidget* dialog_create    (GwyUnitoolState *state);
static void       dialog_update    (GwyUnitoolState *state,
                                    GwyUnitoolUpdateType reason);
static void       dialog_abandon   (GwyUnitoolState *state);
static void       apply            (GwyUnitoolState *state);

static const gchar *radius_key = "/tool/level3/radius";

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Three-point level tool, levels data by subtracting a plane fitted "
       "through three selected points."),
    "Yeti <yeti@gwyddion.net>",
    "1.2",
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
        "level3",
        GWY_STOCK_FIT_TRIANGLE,
        N_("Level data using three points."),
        50,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerPoints";
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
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer, "max-points", 3, NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *label, *frame;
    gint radius;
    GString *str;
    gint i;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;

    dialog = gtk_dialog_new_with_buttons(_("Level"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_clear(dialog);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(4, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);
    str = g_string_new("");

    controls->xunits = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>X</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 2, 2);

    controls->yunits = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>Y</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 2, 2);

    controls->zunits = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>%s</b> [%s]", _("Value"),
                    state->value_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1, GTK_FILL, 0, 2, 2);

    for (i = 0; i < 3; i++) {
        label = gtk_label_new(NULL);
        g_string_printf(str, "<b>%d</b>", i+1);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_label_set_markup(GTK_LABEL(label), str->str);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, i+1, i+2, 0, 0, 2, 2);
        label = controls->coords[2*i] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 1, 2, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
        label = controls->coords[2*i + 1] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 2, 3, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
        label = controls->values[i] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 3, 4, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
    }
    g_string_free(str, TRUE);

    table = gtk_table_new(1, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    settings = gwy_app_settings_get();
    radius = 1;
    gwy_container_gis_int32_by_name(settings, radius_key, &radius);
    radius = CLAMP(radius, 1, 16);
    controls->radius = gtk_adjustment_new((gdouble)radius, 1, 16, 1, 5, 16);
    gwy_table_attach_spinbutton(table, 9, _("_Averaging radius:"), _("px"),
                                controls->radius);
    g_signal_connect_swapped(controls->radius, "value-changed",
                             G_CALLBACK(dialog_update), state);

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    GString *str;
    gdouble points[6];
    gboolean is_visible;
    gdouble val;
    gint nselected, i, radius;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->radius));

    is_visible = state->is_visible;
    nselected = gwy_vector_layer_get_selection(state->layer, points);
    if (!is_visible && !nselected)
        return;

    str = g_string_new("");
    g_string_printf(str, "<b>X</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->xunits), str->str);

    g_string_printf(str, "<b>Y</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->yunits), str->str);

    g_string_printf(str, "<b>%s</b> [%s]", _("Value"),
                    state->value_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->zunits), str->str);
    g_string_free(str, TRUE);

    for (i = 0; i < 6; i++) {
        if (i < 2*nselected) {
            gwy_unitool_update_label_no_units(state->coord_hformat,
                                              controls->coords[i], points[i]);
            if (i%2 == 0) {
                val = gwy_unitool_get_z_average(dfield, points[i], points[i+1],
                                                radius);
                gwy_unitool_update_label_no_units(state->value_hformat,
                                                  controls->values[i/2], val);
            }
        }
        else {
            gtk_label_set_text(GTK_LABEL(controls->coords[i]), "");
            if (i%2 == 0)
                gtk_label_set_text(GTK_LABEL(controls->values[i/2]), "");
        }
    }
    gwy_unitool_apply_set_sensitive(state, nselected == 3);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    GwyContainer *settings;
    ToolControls *controls;
    gint radius;

    controls = (ToolControls*)state->user_data;
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->radius));
    radius = CLAMP(radius, 1, 16);
    settings = gwy_app_settings_get();
    gwy_container_set_int32_by_name(settings, radius_key, radius);

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
apply(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield;
    ToolControls *controls;
    GwyDataViewLayer *layer;
    gdouble points[9], z[3], coeffs[3];
    gint i, radius;

    if (gwy_vector_layer_get_selection(state->layer, points) < 3)
        return;

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->radius));

    /* find the plane levelling coeffs so that values in the three points
     * will be all zeroes
     *
     *  /       \   /  \     /  \
     * | x1 y1 1 | | bx |   | z1 |
     * | x2 y2 1 | | by | = | z2 |
     * | x3 y3 1 | | c  |   | z3 |
     *  \       /   \  /     \  /
     *
     */
    for (i = 0; i < 3; i++)
        z[i] = gwy_unitool_get_z_average(dfield, points[2*i], points[2*i+1],
                                         radius);
    points[7] = points[5];
    points[6] = points[4];
    points[4] = points[3];
    points[3] = points[2];
    points[2] = points[5] = points[8] = 1.0;
    gwy_math_lin_solve_rewrite(3, points, z, coeffs);
    /* to keep mean value intact, the mean value of the plane we add to the
     * data has to be zero, i.e., in the center of the data the value must
     * be zero */
    coeffs[2] = -0.5*(coeffs[0]*gwy_data_field_get_xreal(dfield)
                      + coeffs[1]*gwy_data_field_get_yreal(dfield));
    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    gwy_data_field_plane_level(dfield, coeffs[2], coeffs[0], coeffs[1]);

    gwy_vector_layer_unselect(state->layer);
    gwy_data_field_data_changed(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

