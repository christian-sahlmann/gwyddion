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
#include <app/gwyapp.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef struct {
    GwyUnitoolRectLabels labels;
    GtkWidget *fitting;
    GtkWidget *direction;
    GtkWidget *exclude;
    gint fit;
    GtkOrientation dir;
    gboolean exc;
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

static void       direction_changed_cb(GObject *item,
                                       GwyUnitoolState *state);
static void       fitting_changed_cb  (GObject *item,
                                       GwyUnitoolState *state);
static void       exclude_changed_cb  (GtkToggleButton *button,
                                       GwyUnitoolState *state);

static void       load_args           (GwyContainer *container,
                                       ToolControls *controls);
static void       save_args           (GwyContainer *container,
                                       ToolControls *controls);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Polynom fit tool, fits polynoms to X or Y profiles and subtracts "
       "them."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
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
        "polynom",
        GWY_STOCK_POLYNOM_REMOVE,
        N_("Fit X or Y profiles by polynom"),
        14,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerSelect";
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
        state->apply_doesnt_close = TRUE;
    }
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer, "is_crop", FALSE, NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    const GwyEnum degrees[] = {
        { N_("Fit height"),    0, },
        { N_("Fit linear"),    1, },
        { N_("Fit quadratic"), 2, },
        { N_("Fit cubic"),     3, },
    };
    const GwyEnum directions[] = {
        { N_("_Horizontal direction"), GWY_ORIENTATION_HORIZONTAL, },
        { N_("_Vertical direction"),   GWY_ORIENTATION_VERTICAL,   },
    };
    ToolControls *controls;
    GwyContainer *settings;
    GwySIValueFormat *units;
    GtkWidget *dialog, *table, *table2, *label, *frame;
    GSList *radio;
    gint row;

    gwy_debug(" ");

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    units = state->coord_format;

    dialog = gtk_dialog_new_with_buttons(_("X/Y Profile Level"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(6, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    gwy_unitool_rect_info_table_setup(&controls->labels,
                                      GTK_TABLE(table), 0, 0);
    controls->labels.unselected_is_full = TRUE;

    table2 = gtk_table_new(4, 2, FALSE);
    row = 0;

    gtk_container_set_border_width(GTK_CONTAINER(table2), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Fiting mode</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table2), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls->fitting = gwy_option_menu_create(degrees, G_N_ELEMENTS(degrees),
                                               "fit-type",
                                               G_CALLBACK(fitting_changed_cb),
                                               state, controls->fit);
    gwy_table_attach_row(table2, row, _("_Type:"), NULL, controls->fitting);
    row++;

    radio = gwy_radio_buttons_create(directions, G_N_ELEMENTS(directions),
                                     "direction-type",
                                     G_CALLBACK(direction_changed_cb), state,
                                     controls->dir);
    while (radio) {
        gtk_table_attach(GTK_TABLE(table2), GTK_WIDGET(radio->data),
                         0, 3, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
        row++;
        radio = g_slist_next(radio);
    }
    gtk_table_set_row_spacing(GTK_TABLE(table2), row-1, 8);

    controls->exclude
        = gtk_check_button_new_with_mnemonic(_("_Exclude area if selected"));
    gtk_table_attach(GTK_TABLE(table2), controls->exclude, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->exclude),
                                 controls->exc);
    g_signal_connect(controls->exclude, "toggled",
                     G_CALLBACK(exclude_changed_cb), state);
    row++;

    label = gtk_label_new(_("(otherwise will be used for fitting)"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table2), label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    return dialog;
}

/* TODO */
static void
apply(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    ToolControls *controls;
    gint isel[4];

    gwy_debug(" ");
    layer = GWY_DATA_VIEW_LAYER(state->layer);

    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    gwy_container_remove_by_name(data, "/0/show");

    gwy_app_clean_up_data(data);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    controls = (ToolControls*)state->user_data;
    gwy_unitool_rect_info_table_fill(state, &controls->labels, NULL, isel);

    gwy_app_undo_checkpoint(data, "/0/data", NULL);

    gwy_data_field_fit_lines(dfield, isel[0], isel[1], isel[2], isel[3],
                             controls->fit, controls->exc, controls->dir);

    gwy_vector_layer_unselect(state->layer);
    gwy_data_view_update(GWY_DATA_VIEW(layer->parent));
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    ToolControls *controls;
    GwyDataViewLayer *layer;
    gboolean is_visible, is_selected;

    gwy_debug("");
    is_visible = state->is_visible;

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    is_selected = gwy_vector_layer_get_selection(state->layer, NULL);
    if (!is_visible && !is_selected)
        return;

    gwy_unitool_rect_info_table_fill(state, &controls->labels, NULL, NULL);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    GwyContainer *settings;
    GwyContainer *data;
    ToolControls *controls;
    GwyDataViewLayer *layer;

    settings = gwy_app_settings_get();

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));

    save_args(settings, controls);
    gwy_container_remove_by_name(data, "/0/show");

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
direction_changed_cb(GObject *item, GwyUnitoolState *state)
{
    ToolControls *controls;

    gwy_debug(" ");
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)))
        return;

    controls = (ToolControls*)state->user_data;
    controls->dir = GPOINTER_TO_INT(g_object_get_data(item, "direction-type"));
    dialog_update(state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
fitting_changed_cb(GObject *item, GwyUnitoolState *state)
{
    ToolControls *controls;

    gwy_debug(" ");
    controls = (ToolControls*)state->user_data;
    controls->fit = GPOINTER_TO_INT(g_object_get_data(item, "fit-type"));
    dialog_update(state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
exclude_changed_cb(GtkToggleButton *button, GwyUnitoolState *state)
{
    ToolControls *controls;

    gwy_debug(" ");
    controls = (ToolControls*)state->user_data;
    controls->exc = gtk_toggle_button_get_active(button);
    dialog_update(state, GWY_UNITOOL_UPDATED_CONTROLS);
}


static const gchar *exc_key = "/tool/polynom/exclude";
static const gchar *fit_key = "/tool/polynom/fitting";
static const gchar *dir_key = "/tool/polynom/direction";

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_boolean_by_name(container, exc_key, controls->exc);
    gwy_container_set_enum_by_name(container, fit_key, controls->fit);
    gwy_container_set_enum_by_name(container, dir_key, controls->dir);
}


static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->exc = FALSE;
    controls->fit = 1;
    controls->dir = GTK_ORIENTATION_HORIZONTAL;

    gwy_container_gis_boolean_by_name(container, exc_key, &controls->exc);
    gwy_container_gis_enum_by_name(container, fit_key, &controls->fit);
    gwy_container_gis_enum_by_name(container, dir_key, &controls->dir);

    /* sanitize */
    controls->exc = !!controls->exc;
    controls->fit = CLAMP(controls->fit, 0, 3);
    controls->dir = MIN(controls->dir, GTK_ORIENTATION_VERTICAL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

