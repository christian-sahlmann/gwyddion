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

typedef struct {
    GtkWidget *coords[6];
    GtkWidget *values[3];
    GtkObject *radius;
} ToolControls;

static gboolean   module_register  (const gchar *name);
static void       use              (GwyDataWindow *data_window,
                                    GwyToolSwitchEvent reason);
static void       layer_setup      (GwyUnitoolState *state);
static GtkWidget* dialog_create    (GwyUnitoolState *state);
static void       dialog_update    (GwyUnitoolState *state);
static void       dialog_abandon   (GwyUnitoolState *state);
static void       apply            (GwyUnitoolState *state);

static const gchar *radius_key = "/tool/level3/radius";

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "level3",
    "Level tool.  Allows to level data by fitting a plane through three "
        "selected points.",
    "Yeti <yeti@physics.muni.cz>",
    "1.0",
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
        "gwy_fit_triangle",
        "Level data using three points.",
        50,
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
        func_slots.layer_type = GWY_TYPE_LAYER_POINTS;
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
    }
    gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(GWY_IS_LAYER_POINTS(state->layer));
    g_object_set(state->layer, "max_points", 3, NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *label, *frame;
    gint radius;
    guchar *buffer;
    gint i;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;

    dialog = gtk_dialog_new_with_buttons(_("Level"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(4, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>X</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Y</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Value</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1, GTK_FILL, 0, 2, 2);
    for (i = 0; i < 3; i++) {
        label = gtk_label_new(NULL);
        buffer = g_strdup_printf(_("<b>%d</b>"), i+1);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_label_set_markup(GTK_LABEL(label), buffer);
        g_free(buffer);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, i+1, i+2, 0, 0, 2, 2);
        label = controls->coords[2*i] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 1, 2, i+1, i+2, 0, 0, 2, 2);
        label = controls->coords[2*i + 1] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 2, 3, i+1, i+2, 0, 0, 2, 2);
        label = controls->values[i] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 3, 4, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
    }

    table = gtk_table_new(1, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    settings = gwy_app_settings_get();
    if (gwy_container_contains_by_name(settings, radius_key))
        radius = gwy_container_get_int32_by_name(settings, radius_key);
    else
        radius = 1;
    controls->radius = gtk_adjustment_new((gdouble)radius, 1, 16, 1, 5, 16);
    gwy_table_attach_spinbutton(table, 9, "Averaging radius", "px",
                                controls->radius);
    g_signal_connect_swapped(controls->radius, "value_changed",
                             G_CALLBACK(dialog_update), state);

    return dialog;
}

static void
update_value_label(GtkWidget *label, gdouble value)
{
    gchar buffer[24];

    g_snprintf(buffer, sizeof(buffer), "%g", value);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

static void
dialog_update(GwyUnitoolState *state)
{
    GwyUnitoolUnits *units;
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gdouble points[6];
    gboolean is_visible;
    gdouble val;
    gint nselected, i, radius;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    units = &state->coord_units;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->radius));

    is_visible = state->is_visible;
    nselected = gwy_layer_points_get_points(GWY_LAYER_POINTS(state->layer),
                                            points);
    if (!is_visible && !nselected)
        return;

    for (i = 0; i < 6; i++) {
        if (i < 2*nselected) {
            gwy_unitool_update_label(units, controls->coords[i], points[i]);
            if (i%2 == 0) {
                val = gwy_unitool_get_z_average(dfield, points[i], points[i+1],
                                                radius);
                /* FIXME: get some units... */
                update_value_label(controls->values[i/2], val);
            }
        }
        else {
            gtk_label_set_text(GTK_LABEL(controls->coords[i]), "");
            if (i%2 == 0)
                gtk_label_set_text(GTK_LABEL(controls->values[i/2]), "");
        }
    }
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
    gdouble points[6], z[3];
    gdouble bx, by, c, det;
    gint i, radius;

    if (gwy_layer_points_get_points(GWY_LAYER_POINTS(state->layer), points) < 3)
        return;

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->radius));

    /* find the plane levelling coeffs so that values in the three points
     * will be all zeroes */
    det = bx = by = c = 0;
    for (i = 0; i < 3; i++)
        z[i] = gwy_unitool_get_z_average(dfield, points[2*i], points[2*i+1],
                                         radius);
    det = points[0]*(points[3] - points[5])
          + points[2]*(points[5] - points[1])
          + points[4]*(points[1] - points[3]);
    bx = z[0]*(points[3] - points[5])
         + z[1]*(points[5] - points[1])
         + z[2]*(points[1] - points[3]);
    by = z[0]*(points[4] - points[2])
         + z[1]*(points[0] - points[4])
         + z[2]*(points[2] - points[0]);
    c = z[0]*(points[2]*points[5] - points[3]*points[4])
         + z[1]*(points[1]*points[4] - points[5]*points[0])
         + z[2]*(points[0]*points[3] - points[1]*points[2]);
    gwy_debug("bx = %g, by = %g, c = %g, det = %g", bx, by, c, det);
    bx /= det;
    by /= det;
    c /= det;
    /* to keep mean value intact, the mean value of the plane we add to the
     * data has to be zero, i.e., in the center of the data the value must
     * be zero */
    c = -0.5*(bx*gwy_data_field_get_xreal(dfield)
              + by*gwy_data_field_get_yreal(dfield));
    gwy_debug("bx = %g, by = %g, c = %g", bx, by, c);
    gwy_debug("z[0] = %g, z[1] = %g, z[2] = %g", z[0], z[1], z[2]);
    gwy_debug("zn[0] = %g, zn[1] = %g, zn[2] = %g",
              z[0] - c - bx*points[0] - by*points[1],
              z[1] - c - bx*points[2] - by*points[3],
              z[2] - c - bx*points[4] - by*points[5]);
    gwy_app_undo_checkpoint(data, "/0/data");
    gwy_data_field_plane_level(dfield, c, bx, by);
    gwy_debug("zN[0] = %g, zN[1] = %g, zN[2] = %g",
              gwy_unitool_get_z_average(dfield, points[0], points[1], radius),
              gwy_unitool_get_z_average(dfield, points[2], points[3], radius),
              gwy_unitool_get_z_average(dfield, points[4], points[5], radius));

    gwy_vector_layer_unselect(state->layer);
    gwy_data_view_update(GWY_DATA_VIEW(layer->parent));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

