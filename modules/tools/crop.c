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
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/app.h>
#include <app/settings.h>
#include <app/unitool.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef struct {
    gboolean keep_offsets;
    GtkWidget *ckeep_offsets;
    GwyUnitoolRectLabels labels;
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
static void       keep_offsets_toggled(GwyUnitoolState *state,
                                       GtkWidget *toggle);
static void       load_args           (GwyContainer *container,
                                       ToolControls *controls);
static void       save_args           (GwyContainer *container,
                                       ToolControls *controls);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Crop tool, crops data to smaller size."),
    "Yeti <yeti@gwyddion.net>",
    "1.6",
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
    static GwyToolFuncInfo crop_func_info = {
        "crop",
        GWY_STOCK_CROP,
        N_("Crop data"),
        &use,
    };

    gwy_tool_func_register(name, &crop_func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerRectangle";
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
    g_object_set(state->layer,
                 "selection-key", "/0/select/rectangle",
                 "is-crop", TRUE,
                 NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GtkWidget *dialog, *table, *frame;
    gint row;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    load_args(gwy_app_settings_get(), controls);

    dialog = gtk_dialog_new_with_buttons(_("Crop"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(7, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    row = gwy_unitool_rect_info_table_setup(&controls->labels,
                                            GTK_TABLE(table), 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    controls->ckeep_offsets
        = gtk_check_button_new_with_mnemonic(_("Keep lateral offsets"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->ckeep_offsets),
                                 controls->keep_offsets);
    gtk_table_attach(GTK_TABLE(table), controls->ckeep_offsets,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect_swapped(controls->ckeep_offsets, "toggled",
                             G_CALLBACK(keep_offsets_toggled), state);

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    GwySelection *selection;
    gboolean is_visible, is_selected;
    ToolControls *controls;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    is_visible = state->is_visible;
    selection = gwy_vector_layer_get_selection(state->layer);
    is_selected = gwy_selection_get_object(selection, 0, NULL);
    if (!is_visible && !is_selected)
        return;

    gwy_unitool_rect_info_table_fill(state, &controls->labels, NULL, NULL);
    gwy_unitool_apply_set_sensitive(state, is_selected);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    ToolControls *controls;

    controls = (ToolControls*)state->user_data;
    save_args(gwy_app_settings_get(), controls);
    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
keep_offsets_toggled(GwyUnitoolState *state,
                     GtkWidget *toggle)
{
    ToolControls *controls;

    controls = (ToolControls*)state->user_data;
    controls->keep_offsets
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
}

static void
apply(GwyUnitoolState *state)
{
    static const gchar *field_names[] = { "/0/data", "/0/mask", "/0/show" };
    ToolControls *controls;
    GwySelection *selection;
    GtkWidget *data_window;
    GwyDataView *data_view;
    GwyContainer *data;
    GwyDataField *dfield;
    gint ximin, yimin, ximax, yimax;
    gdouble xoff, yoff, sel[4];
    gsize i;

    controls = (ToolControls*)state->user_data;
    selection = gwy_vector_layer_get_selection(state->layer);
    if (!gwy_selection_get_object(selection, 0, sel))
        return;

    gwy_selection_clear(selection);
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(state->layer)->parent);
    data = gwy_data_view_get_data(data_view);
    data = gwy_container_duplicate(data);
    gwy_app_clean_up_data(data);
    for (i = 0; i < G_N_ELEMENTS(field_names); i++) {
        if (!gwy_container_gis_object_by_name(data, field_names[i], &dfield))
            continue;
        ximin = gwy_data_field_rtoj(dfield, sel[0]);
        yimin = gwy_data_field_rtoi(dfield, sel[1]);
        ximax = gwy_data_field_rtoj(dfield, sel[2]) + 1;
        yimax = gwy_data_field_rtoi(dfield, sel[3]) + 1;
        gwy_data_field_resize(dfield, ximin, yimin, ximax, yimax);
        if (controls->keep_offsets) {
            xoff = gwy_data_field_get_xoffset(dfield);
            yoff = gwy_data_field_get_yoffset(dfield);
            gwy_data_field_set_xoffset(dfield, sel[0] + xoff);
            gwy_data_field_set_yoffset(dfield, sel[1] + yoff);
        }
        else {
            gwy_data_field_set_xoffset(dfield, 0.0);
            gwy_data_field_set_yoffset(dfield, 0.0);
        }
    }
    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    g_object_unref(data);
    gwy_debug("%d %d",
              gwy_data_field_get_xres(dfield), gwy_data_field_get_yres(dfield));
}

static const gchar *keep_offsets_key = "/tool/crop/keep_offsets";

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_boolean_by_name(container, keep_offsets_key,
                                      controls->keep_offsets);
}

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->keep_offsets = FALSE;

    gwy_container_gis_boolean_by_name(container, keep_offsets_key,
                                      &controls->keep_offsets);

    /* sanitize */
    controls->keep_offsets = !!controls->keep_offsets;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

