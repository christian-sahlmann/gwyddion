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
#include <app/unitool.h>

typedef struct {
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *val;
    GtkObject *radius;
} ToolControls;

static gboolean   module_register  (const gchar *name);
static void       use              (GwyDataWindow *data_window,
                                    GwyToolSwitchEvent reason);
static GtkWidget* dialog_create    (GwyUnitoolState *state);
static void       dialog_update    (GwyUnitoolState *state);
static void       dialog_abandon   (GwyUnitoolState *state);

static const gchar *radius_key = "/tool/pointer/radius";

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "pointer",
    "Pointer tool.",
    "Yeti <yeti@physics.muni.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    gwy_layer_pointer_new,         /* layer object constructor */
    NULL,                          /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    NULL,                          /* apply action */
    NULL,                          /* nonstandard response handler */
};
/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo func_info = {
        "pointer",
        "gwy_pointer_measure",
        "Read value under mouse cursor.",
        0,
        &use,
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
        func_slots.layer_type = GWY_TYPE_LAYER_POINTER;
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
    }
    gwy_unitool_use(state, data_window, reason);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *label, *frame;
    gint radius;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;

    dialog = gtk_dialog_new_with_buttons(_("Show value"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
 
    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(2, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>X</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Y</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Value</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 2, 2);

    label = controls->x = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, 0, 0, 2, 2);
    label = controls->y = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, 0, 0, 2, 2);
    label = controls->val = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2,
                        GTK_EXPAND | GTK_FILL, 0, 2, 2);

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
    gchar buffer[16];

    g_snprintf(buffer, sizeof(buffer), "%g", value);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

static void
dialog_update(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield;
    ToolControls *controls;
    GwyUnitoolUnits *units;
    gdouble x, y, value;
    gboolean is_visible, is_selected;
    gint radius;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    units = &state->coord_units;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(state->layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->radius));

    is_visible = state->is_visible;
    is_selected = gwy_layer_pointer_get_point(state->layer, &x, &y);
    if (!is_visible && !is_selected)
        return;

    if (is_selected) {
        gwy_unitool_update_label(units, controls->x, x);
        gwy_unitool_update_label(units, controls->y, y);
        value = gwy_unitool_get_z_average(dfield, x, y, radius);
        update_value_label(controls->val, value);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls->x), "");
        gtk_label_set_text(GTK_LABEL(controls->y), "");
        gtk_label_set_text(GTK_LABEL(controls->val), "");
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

