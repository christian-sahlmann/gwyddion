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

#include <libgwyddion/gwymacros.h>
#include <string.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/unitool.h>
#include <app/undo.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef enum {
    GRAIN_REMOVE_MASK = 1,
    GRAIN_REMOVE_DATA,
    GRAIN_REMOVE_BOTH
} RemoveMode;

typedef enum {
    GRAIN_REMOVE_LAPLACE = 1,
    GRAIN_REMOVE_FRACTAL
} RemoveAlgorithm;

typedef struct {
    RemoveMode mode;
    RemoveAlgorithm algorithm;
    GtkWidget *algorithm_menu;
    GtkWidget *algorithm_label;
} ToolControls;

static gboolean   module_register       (const gchar *name);
static gboolean   use                   (GwyDataWindow *data_window,
                                         GwyToolSwitchEvent reason);
static GtkWidget* dialog_create         (GwyUnitoolState *state);
static void       dialog_abandon        (GwyUnitoolState *state);
static void       selection_finished_cb (GwyUnitoolState *state);
static void       mode_changed_cb       (GObject *item,
                                         GwyUnitoolState *state);
static void       algorithm_changed_cb  (GObject *item,
                                         GwyUnitoolState *state);
static void       load_args             (GwyContainer *container,
                                         ToolControls *controls);
static void       save_args             (GwyContainer *container,
                                         ToolControls *controls);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "grain_remove_manually",
    N_("Grain (mask) removal tool."),
    "Petr Klapetek <klapetek@gwyddion.net>, Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    NULL,                          /* layer setup func */
    dialog_create,                 /* dialog constructor */
    NULL,                          /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    NULL,                          /* apply action */
    NULL,                          /* nonstandard response handler */
};

static const GwyEnum modes[] = {
    { N_("_Mask"), GRAIN_REMOVE_MASK },
    { N_("_Data"), GRAIN_REMOVE_DATA },
    { N_("_Both"), GRAIN_REMOVE_BOTH },
};

static const GwyEnum algorithms[] = {
    { N_("Laplace solver"),     GRAIN_REMOVE_LAPLACE },
    { N_("Fractal correction"), GRAIN_REMOVE_FRACTAL },
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo func_info = {
        "grain_remove_manually",
        GWY_STOCK_GRAINS_REMOVE,
        N_("Manually remove grains (continuous parts of mask)"),
        98,
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
    GtkWidget *dialog, *table, *label, *frame, *omenu;
    ToolControls *controls;
    GwyContainer *settings;
    gboolean sensitive;
    GSList *group;
    gint row;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Remove Grains"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(2, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);
    row = 0;

    label = gtk_label_new(_("Remove:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    group = gwy_radio_buttons_create(modes, G_N_ELEMENTS(modes), "mode",
                                     G_CALLBACK(mode_changed_cb), state,
                                     controls->mode);
    while (group) {
        gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(group->data),
                         0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
        row++;
        group = g_slist_next(group);
    }
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new_with_mnemonic(_("_Data removal method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    controls->algorithm_label = label;
    row++;

    omenu = gwy_option_menu_create(algorithms, G_N_ELEMENTS(algorithms),
                                   "algorithm",
                                   G_CALLBACK(algorithm_changed_cb), state,
                                   controls->algorithm);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    gtk_table_attach(GTK_TABLE(table), omenu, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    controls->algorithm_menu = omenu;
    row++;

    sensitive = (controls->mode == GRAIN_REMOVE_DATA
                 || controls->mode == GRAIN_REMOVE_BOTH);
    gtk_widget_set_sensitive(controls->algorithm_menu, sensitive);
    gtk_widget_set_sensitive(controls->algorithm_label, sensitive);

    g_signal_connect_swapped(state->layer, "selection-finished",
                             G_CALLBACK(selection_finished_cb), state);

    return dialog;
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    save_args(settings, controls);

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
mode_changed_cb(GObject *item, GwyUnitoolState *state)
{
    ToolControls *controls;
    gboolean sensitive;

    gwy_debug(" ");
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)))
        return;

    controls = (ToolControls*)state->user_data;
    controls->mode = GPOINTER_TO_INT(g_object_get_data(item, "mode"));
    sensitive = (controls->mode == GRAIN_REMOVE_DATA
                 || controls->mode == GRAIN_REMOVE_BOTH);
    gtk_widget_set_sensitive(controls->algorithm_menu, sensitive);
    gtk_widget_set_sensitive(controls->algorithm_label, sensitive);
}

static void
algorithm_changed_cb(GObject *item, GwyUnitoolState *state)
{
    ToolControls *controls;

    controls = (ToolControls*)state->user_data;
    controls->algorithm = GPOINTER_TO_INT(g_object_get_data(item, "algorithm"));
}

static void
selection_finished_cb(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield, *tmp, *buffer, *mask = NULL;
    GwyDataViewLayer *layer;
    ToolControls *controls;
    gdouble xy[2];
    gint col, row, i;
    gdouble error, maxer, cor = 0.2;
    gboolean is_visible, is_selected;

    gwy_debug(" ");
    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    if (!gwy_container_gis_object_by_name(data, "/0/mask", (GObject**)&mask)) {
        gwy_debug("No mask");
        return;
    }

    is_visible = state->is_visible;
    is_selected = gwy_vector_layer_get_selection(state->layer, xy);

    if (!is_visible && !is_selected)
        return;

    row = ROUND(gwy_data_field_rtoi(mask, xy[1]));
    col = ROUND(gwy_data_field_rtoj(mask, xy[0]));

    if (!gwy_data_field_get_val(mask, col, row))
        return;

    if (controls->mode == GRAIN_REMOVE_MASK) {
        gwy_app_undo_checkpoint(data, "/0/mask", NULL);
        gwy_data_field_grains_remove_grain(mask, col, row);
    }
    else {
        tmp = gwy_data_field_duplicate(mask);
        gwy_data_field_grains_extract_grain(tmp, col, row);
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        if (controls->mode == GRAIN_REMOVE_BOTH)
            gwy_app_undo_checkpoint(data, "/0/data", "/0/mask", NULL);
        else
            gwy_app_undo_checkpoint(data, "/0/data", NULL);
        switch (controls->algorithm) {
            case GRAIN_REMOVE_LAPLACE:
            maxer = gwy_data_field_get_rms(dfield)/1.0e3;
            buffer = GWY_DATA_FIELD(gwy_data_field_new_alike(mask, FALSE));
            i = 0;
            do {
                gwy_data_field_correct_laplace_iteration(dfield, tmp, buffer,
                                                         &error, &cor);
                i++;
            } while (error >= maxer && i < 1000);
            g_object_unref(buffer);
            break;

            case GRAIN_REMOVE_FRACTAL:
            gwy_data_field_fractal_correction(dfield, tmp,
                                              GWY_INTERPOLATION_BILINEAR);
            break;

            default:
            g_assert_not_reached();
            break;
        }
        g_object_unref(tmp);
        if (controls->mode == GRAIN_REMOVE_BOTH)
            gwy_data_field_grains_remove_grain(mask, col, row);
    }
    gwy_vector_layer_unselect(state->layer);

    gwy_data_view_update(GWY_DATA_VIEW(layer->parent));
}

static const gchar *mode_key = "/tool/grain_remove_manually/mode";
static const gchar *algorithm_key = "/tool/grain_remove_manually/algorithm";

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->mode = GRAIN_REMOVE_MASK;
    controls->algorithm = GRAIN_REMOVE_LAPLACE;

    gwy_container_gis_enum_by_name(container, mode_key, &controls->mode);
    gwy_container_gis_enum_by_name(container, algorithm_key,
                                   &controls->algorithm);

    /* sanitize */
    controls->mode = CLAMP(controls->mode,
                           GRAIN_REMOVE_MASK, GRAIN_REMOVE_BOTH);
    controls->algorithm = CLAMP(controls->mode,
                                GRAIN_REMOVE_LAPLACE, GRAIN_REMOVE_FRACTAL);
}

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_enum_by_name(container, mode_key, controls->mode);
    gwy_container_set_enum_by_name(container, algorithm_key,
                                   controls->algorithm);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

