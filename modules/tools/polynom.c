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
    GwyUnitoolState *state;
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *w;
    GtkWidget *h;
    GtkWidget *fitting;
    GtkWidget *direction;
    GtkWidget *exclude;
    GwyFitLineType fit;
    GtkOrientation dir;
    gboolean exc;
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

static void       direction_changed_cb (GObject *item,
                                    ToolControls *controls);
static void       fitting_changed_cb (GObject *item,
                                    ToolControls *controls);
static void       exclude_changed_cb (GtkToggleButton *button,
                                    ToolControls *controls);

static void       load_args        (GwyContainer *container,
                                    ToolControls *controls);
static void       save_args        (GwyContainer *container,
                                    ToolControls *controls);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "polynom",
    "Fit X or Y profiles by polynom",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
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
        "Fit X or Y profiles by polynom",
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
    }
    ((ToolControls*)state->user_data)->state = state;
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
    ToolControls *controls;
    GwyContainer *settings;
    GwySIValueFormat *units;
    GtkWidget *dialog, *table, *table2, *label, *frame;

    gwy_debug("");

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

    table = gtk_table_new(6, 3, FALSE);

    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Origin</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("X"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Y"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Size</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6, GTK_FILL, 0, 2, 2);

    controls->x = gtk_label_new("");
    controls->y = gtk_label_new("");
    controls->w = gtk_label_new("");
    controls->h = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->x), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->y), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->w), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->h), 1.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(controls->x), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->y), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->w), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->h), TRUE);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->x, 2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->y, 2, 3, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->w, 2, 3, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->h, 2, 3, 5, 6);

    table2 = gtk_table_new(4, 2, FALSE);

    gtk_container_set_border_width(GTK_CONTAINER(table2), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Fiting mode:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 1, 2, GTK_FILL, 0, 2, 2);

    controls->fitting
        = gwy_option_menu_fit_line(G_CALLBACK(fitting_changed_cb),
                                    controls, controls->fit);

    gtk_table_attach(GTK_TABLE(table2), controls->fitting, 1, 2, 1, 2, GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("direction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 2, 3, GTK_FILL, 0, 2, 2);

    controls->direction
        = gwy_option_menu_direction(G_CALLBACK(direction_changed_cb),
                                                 controls, controls->dir);

    gtk_table_attach(GTK_TABLE(table2), controls->direction, 1, 2, 2, 3, GTK_FILL, 0, 2, 2);

    controls->exclude = gtk_check_button_new_with_label("exclude area if selected");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), controls->exclude,
                       FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->exclude), controls->exc);
    g_signal_connect(controls->exclude, "toggled", G_CALLBACK(exclude_changed_cb), controls);

    label = gtk_label_new("(otherwise will be used for fitting)");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label,
                       FALSE, FALSE, 0);


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
    gboolean is_selected;
    gdouble xy[4];
    gdouble ulcol, brcol, ulrow, brrow;

    gwy_debug("");
    layer = GWY_DATA_VIEW_LAYER(state->layer);

    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    gwy_container_remove_by_name(data, "/0/show");

    gwy_app_clean_up_data(data);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    controls = (ToolControls*)state->user_data;
    is_selected = gwy_vector_layer_get_selection(state->layer, xy);

    if (is_selected)
    {
        ulcol = (gint)gwy_data_field_rtoi(dfield, MIN(xy[0], xy[2]));
        ulrow = (gint)gwy_data_field_rtoj(dfield, MIN(xy[1], xy[3]));
        brcol = (gint)gwy_data_field_rtoi(dfield, MAX(xy[0], xy[2]));
        brrow = (gint)gwy_data_field_rtoj(dfield, MAX(xy[1], xy[3]));
    }
    else
    {
        ulcol = 0;
        ulrow = 0;
        brcol = gwy_data_field_get_xres(dfield);
        brrow = gwy_data_field_get_yres(dfield);
    }

    gwy_app_undo_checkpoint(data, "/0/data", NULL);

    gwy_data_field_fit_lines(dfield, ulcol, ulrow, brcol, brrow,
                             controls->fit, controls->exc, controls->dir);

   gwy_vector_layer_unselect(state->layer);
   gwy_data_view_update(GWY_DATA_VIEW(layer->parent));
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    GwySIValueFormat *units;
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gdouble xy[4];
    gboolean is_visible, is_selected;

    gwy_debug("");
    is_visible = state->is_visible;

    controls = (ToolControls*)state->user_data;
    units = state->coord_format;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    is_selected = gwy_vector_layer_get_selection(state->layer, xy);


    if (!is_visible && !is_selected)
        return;
    if (is_selected) {
        gwy_unitool_update_label(units, controls->x, MIN(xy[0], xy[2]));
        gwy_unitool_update_label(units, controls->y, MIN(xy[1], xy[3]));
        gwy_unitool_update_label(units, controls->w, fabs(xy[2] - xy[0]));
        gwy_unitool_update_label(units, controls->h, fabs(xy[3] - xy[1]));
    }
    else {
        gwy_unitool_update_label(units, controls->x, 0);
        gwy_unitool_update_label(units, controls->y, 0);
        gwy_unitool_update_label(units, controls->w,
                                 gwy_data_field_get_xreal(dfield));
        gwy_unitool_update_label(units, controls->h,
                                 gwy_data_field_get_yreal(dfield));
    }
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
direction_changed_cb (GObject *item, ToolControls *controls)
{
    gwy_debug("");
    controls->dir = GPOINTER_TO_INT(g_object_get_data(item, "direction-type"));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
fitting_changed_cb (GObject *item, ToolControls *controls)
{
    gwy_debug("");
    controls->fit = GPOINTER_TO_INT(g_object_get_data(item, "fit-type"));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
exclude_changed_cb (GtkToggleButton *button, ToolControls *controls)
{
    gwy_debug("");
    controls->exc = gtk_toggle_button_get_active(button);
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
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
    controls->fit = GWY_FIT_POLY_1;
    controls->dir = GTK_ORIENTATION_HORIZONTAL;

    gwy_container_gis_boolean_by_name(container, exc_key, &controls->exc);
    gwy_container_gis_enum_by_name(container, fit_key, &controls->fit);
    gwy_container_gis_enum_by_name(container, dir_key, &controls->dir);

    /* sanitize */
    controls->exc = !!controls->exc;
    controls->fit = MIN(controls->fit, GWY_FIT_POLY_3);
    controls->dir = MIN(controls->dir, GTK_ORIENTATION_VERTICAL);
}
    /* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

