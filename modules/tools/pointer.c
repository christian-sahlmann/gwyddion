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
#include <app/unitool.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef struct {
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *val;
    GtkObject *radius;
} ToolControls;

static gboolean   module_register  (const gchar *name);
static gboolean   use              (GwyDataWindow *data_window,
                                    GwyToolSwitchEvent reason);
static GtkWidget* dialog_create    (GwyUnitoolState *state);
static void       dialog_update    (GwyUnitoolState *state,
                                    GwyUnitoolUpdateType reason);
static void       dialog_abandon   (GwyUnitoolState *state);

static const gchar *radius_key = "/tool/pointer/radius";

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "pointer",
    N_("Pointer tool."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
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
        N_("Read value under mouse cursor."),
        0,
        &use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerPointer";
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

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *label, *frame;
    GString *str;
    gint radius;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;

    dialog = gtk_dialog_new_with_buttons(_("Read Value"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(2, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);
    str = g_string_new("");

    label = gtk_label_new(NULL);
    g_string_printf(str, "<b>X</b> [%s]", state->coord_format->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);

    label = gtk_label_new(NULL);
    g_string_printf(str, "<b>Y</b> [%s]", state->coord_format->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 2, 2);

    label = gtk_label_new(NULL);
    g_string_printf(str, "<b>%s</b> [%s]", _("Value"),
                    state->value_format->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 2, 2);

    g_string_free(str, TRUE);

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
    gwy_table_attach_spinbutton(table, 9, _("Averaging _radius:"), _("px"),
                                controls->radius);
    g_signal_connect_swapped(controls->radius, "value_changed",
                             G_CALLBACK(dialog_update), state);

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    GwyContainer *data;
    GwyDataField *dfield;
    ToolControls *controls;
    GwyDataViewLayer *layer;
    gdouble value, xy[2];
    gboolean is_visible, is_selected;
    gint radius;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;

    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->radius));

    is_visible = state->is_visible;
    is_selected = gwy_vector_layer_get_selection(state->layer, xy);
    if (!is_visible && !is_selected)
        return;

    if (is_selected) {
        gwy_unitool_update_label_no_units(state->coord_format,
                                          controls->x, xy[0]);
        gwy_unitool_update_label_no_units(state->coord_format,
                                          controls->y, xy[1]);
        value = gwy_unitool_get_z_average(dfield, xy[0], xy[1], radius);
        gwy_unitool_update_label_no_units(state->value_format,
                                          controls->val, value);
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

