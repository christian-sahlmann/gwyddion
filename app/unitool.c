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
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include "unitool.h"

static void gwy_unitool_name_changed_cb      (GwyUnitoolState *state);
static void gwy_unitool_disconnect_handlers  (GwyUnitoolState *state);
static void gwy_unitool_dialog_abandon       (GwyUnitoolState *state);
static void gwy_unitool_compute_coord_units  (GwyUnitoolState *state);
static void gwy_unitool_selection_updated_cb (GwyUnitoolState *state);
static void gwy_unitool_data_updated_cb      (GwyUnitoolState *state);
static void gwy_unitool_dialog_response_cb   (GwyUnitoolState *state,
                                              gint response);
static void gwy_unitool_dialog_set_visible   (GwyUnitoolState *state,
                                              gboolean visible);

/***** Public ***************************************************************/

/**
 * gwy_unitool_use:
 * @state: Tool state.
 * @data_window: A data window, as obtained in tool switch module method.
 * @reason: Tool switch reason, as obtained in tool switch module method.
 *
 * Switches a tool.
 *
 * This function is to be called from a tool module @use method. It does all
 * the hard work of changing the layer, connecting or disconnecting callbacks,
 * and showing or hiding the dialog; making all tools using it to behave
 * more-or-less consistently.
 *
 * Returns: Whether the tool switch succeeded. Currently always %TRUE.
 **/
gboolean
gwy_unitool_use(GwyUnitoolState *state,
                GwyDataWindow *data_window,
                GwyToolSwitchEvent reason)
{
    GwyUnitoolSlots *slot;
    GwyVectorLayer *layer;
    GwyDataView *data_view;

    gwy_debug("%p", data_window);
    g_return_val_if_fail(state, FALSE);

    if (!data_window) {
        gwy_unitool_dialog_abandon(state);
        return TRUE;
    }
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), FALSE);
    data_view = GWY_DATA_VIEW(gwy_data_window_get_data_view(data_window));
    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && layer == state->layer) {
        g_assert(state->data_window == data_window);
        if (reason == GWY_TOOL_SWITCH_TOOL)
            gwy_unitool_dialog_set_visible(state, TRUE);
        return TRUE;
    }

    /* disconnect existing handlers */
    gwy_unitool_disconnect_handlers(state);
    state->data_window = data_window;

    /* create or set-up the layer */
    slot = state->func_slots;
    if (layer && G_TYPE_CHECK_INSTANCE_TYPE(layer, slot->layer_type)) {
        state->layer = layer;
        if (slot->layer_setup)
            slot->layer_setup(state);
    }
    else {
        state->layer = GWY_VECTOR_LAYER(g_object_new(slot->layer_type, NULL));
        g_return_val_if_fail(state->layer, FALSE);
        if (slot->layer_setup)
            slot->layer_setup(state);
        gwy_data_view_set_top_layer(data_view, state->layer);
    }

    /* create dialog */
    if (!state->dialog) {
        state->dialog = slot->dialog_create(state);
        g_signal_connect(state->dialog, "delete_event",
                         G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
        gtk_widget_show_all(GTK_DIALOG(state->dialog)->vbox);
        state->is_visible = FALSE;
    }

    /* connect handlers */
    state->layer_updated_id
        = g_signal_connect_swapped(state->layer, "updated",
                                   G_CALLBACK(gwy_unitool_selection_updated_cb),
                                   state);
    state->data_updated_id
        = g_signal_connect_swapped(data_view, "updated",
                                   G_CALLBACK(gwy_unitool_data_updated_cb),
                                   state);
    state->windowname_id
        = g_signal_connect_swapped(data_window, "title_changed",
                                   G_CALLBACK(gwy_unitool_name_changed_cb),
                                   state);
    state->response_id
        = g_signal_connect_swapped(state->dialog, "response",
                                   G_CALLBACK(gwy_unitool_dialog_response_cb),
                                   state);

    /* setup based on switch reason */
    gwy_unitool_compute_coord_units(state);
    if (reason == GWY_TOOL_SWITCH_TOOL)
        gwy_unitool_dialog_set_visible(state, TRUE);
    if (reason == GWY_TOOL_SWITCH_WINDOW)
        gwy_unitool_name_changed_cb(state);

    if (state->is_visible)
        gwy_unitool_selection_updated_cb(state);

    return TRUE;
}

/***** Callbacks *************************************************************/

static void
gwy_unitool_name_changed_cb(GwyUnitoolState *state)
{
    gwy_debug("");
    if (!state->windowname)
        return;

    gtk_label_set_text(GTK_LABEL(state->windowname),
                       gwy_data_window_get_base_name(state->data_window));
}

static void
gwy_unitool_disconnect_handlers(GwyUnitoolState *state)
{
    if (state->layer) {
        GwyDataViewLayer *layer = GWY_DATA_VIEW_LAYER(state->layer);

        gwy_debug("removing \"layer_updated\" handler");
        if (state->layer_updated_id)
            g_signal_handler_disconnect(state->layer, state->layer_updated_id);
        gwy_debug("removing \"data_updated\" handler");
        if (layer->parent && state->data_updated_id)
            g_signal_handler_disconnect(layer->parent, state->data_updated_id);
    }
    gwy_debug("removing \"response\" handler");
    if (state->dialog && state->response_id)
        g_signal_handler_disconnect(state->dialog, state->response_id);
    gwy_debug("removing \"title_changed\" handler");
    if (state->data_window && state->windowname_id)
        g_signal_handler_disconnect(state->data_window, state->windowname_id);

    state->layer_updated_id = 0;
    state->data_updated_id = 0;
    state->response_id = 0;
    state->windowname_id = 0;
}

static void
gwy_unitool_dialog_abandon(GwyUnitoolState *state)
{
    gwy_debug("");
    gwy_unitool_disconnect_handlers(state);
    if (state->dialog) {
        if (state->func_slots->dialog_abandon)
            state->func_slots->dialog_abandon(state);
        gtk_widget_destroy(state->dialog);
    }
    g_free(state->coord_units);
    state->coord_units = NULL;
    state->layer = NULL;
    state->dialog = NULL;
    state->windowname = NULL;
    state->data_window = NULL;
    state->is_visible = FALSE;
}

static void
gwy_unitool_compute_coord_units(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GwySIUnit *siunits;
    GwySIValueFormat *cunits;
    gdouble xreal, yreal, max, unit;

    /* TODO remove once GwySIUnit works... */
    if (!state->coord_units)
        state->coord_units = g_new(GwySIValueFormat, 1);

    cunits = state->coord_units;
    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    siunits = gwy_data_field_get_si_unit_xy(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(dfield),
               yreal/gwy_data_field_get_yres(dfield));
    gwy_si_unit_get_format_with_resolution(siunits, max, unit, cunits);
}

/*
 * Handle "updated" signal of layer, eventually calling tool's callback.
 */
static void
gwy_unitool_selection_updated_cb(GwyUnitoolState *state)
{
    gint nselected;

    gwy_debug("");
    nselected = gwy_vector_layer_get_selection(state->layer, NULL);
    if (state->func_slots->dialog_update)
        state->func_slots->dialog_update(state, GWY_UNITOOL_UPDATED_SELECTION);
    if (nselected && !state->is_visible)
        gwy_unitool_dialog_set_visible(state, TRUE);
}

/*
 * Handle "updated" signal of DataView, eventually calling tool's callback.
 */
static void
gwy_unitool_data_updated_cb(GwyUnitoolState *state)
{
    gwy_debug("");
    if (!state->is_visible)
        return;
    if (state->func_slots->dialog_update)
        state->func_slots->dialog_update(state, GWY_UNITOOL_UPDATED_DATA);
}

/*
 * Handle standard dialog responses.
 */
static void
gwy_unitool_dialog_response_cb(GwyUnitoolState *state,
                               gint response)
{
    gwy_debug("response %d", response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        gwy_unitool_dialog_set_visible(state, FALSE);
        break;

        case GTK_RESPONSE_NONE:
        g_warning("Tool dialog destroyed.");
        gwy_unitool_use(state, NULL, 0);
        break;

        case GTK_RESPONSE_APPLY:
        gwy_unitool_dialog_set_visible(state, FALSE);
        state->func_slots->apply(state);
        break;

        case GWY_UNITOOL_RESPONSE_UNSELECT:
        gwy_vector_layer_unselect(state->layer);
        break;

        default:
        g_return_if_fail(state->func_slots->response);
        state->func_slots->response(state, response);
        break;
    }
}

static void
gwy_unitool_dialog_set_visible(GwyUnitoolState *state,
                               gboolean visible)
{
    gwy_debug("now %d, setting to %d", state->is_visible, visible);
    if (state->is_visible == visible)
        return;

    g_return_if_fail(state->dialog);
    state->is_visible = visible;
    if (visible)
        gtk_window_present(GTK_WINDOW(state->dialog));
    else
        gtk_widget_hide(state->dialog);
}

/***** Helpers *************************************************************/

/**
 * gwy_unitool_windowname_frame_create:
 * @state: Tool state.
 *
 * Creates a frame displaying the name of currently active data window.
 *
 * The displayed name automatically changes on tool switch or when the file
 * name changes.
 *
 * Returns: The name-displaying frame as a #GtkWidget.
 **/
GtkWidget*
gwy_unitool_windowname_frame_create(GwyUnitoolState *state)
{
    GtkWidget *frame, *label;

    g_return_val_if_fail(state, NULL);
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(state->data_window), NULL);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);

    label = gtk_label_new(gwy_data_window_get_base_name(state->data_window));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(label), 4, 2);
    gtk_container_add(GTK_CONTAINER(frame), label);
    state->windowname = label;

    return frame;
}

/**
 * gwy_unitool_get_z_average:
 * @dfield: A data field.
 * @xreal: X-coordinate of area center in physical units.
 * @yreal: Y-coordinate of area center in physical units.
 * @radius: Area radius in pixels.
 *
 * Computes average value over a part of data field @dfield.
 *
 * The area is (currently) square with side of 2@radius+1.  It's not an error
 * if part of it lies outside the data field borders, it's simply not counted
 * in.
 *
 * Returns: The average value.
 **/
gdouble
gwy_unitool_get_z_average(GwyDataField *dfield,
                          gdouble xreal,
                          gdouble yreal,
                          gint radius)
{
    gint x, y, xres, yres, uli, ulj, bri, brj;

    if (radius < 1)
        g_warning("Bad averaging radius %d, fixing to 1", radius);
    x = gwy_data_field_rtoj(dfield, xreal);
    y = gwy_data_field_rtoi(dfield, yreal);
    if (radius < 2)
        return gwy_data_field_get_val(dfield, x, y);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    ulj = CLAMP(x - radius, 0, xres - 1);
    uli = CLAMP(y - radius, 0, yres - 1);
    brj = CLAMP(x + radius, 0, xres - 1);
    bri = CLAMP(y + radius, 0, yres - 1);

    return gwy_data_field_get_area_avg(dfield, ulj, uli, brj, bri);
}

/**
 * gwy_unitool_update_label:
 * @units: Units specification.
 * @label: A label to update (a #GtkLabel).
 * @value: A value to show.
 *
 * Sets the text of a label to display @value according to @units.
 **/
void
gwy_unitool_update_label(GwySIValueFormat *units,
                         GtkWidget *label, gdouble value)
{
    static gchar buffer[32];

    g_return_if_fail(units);
    g_return_if_fail(GTK_IS_LABEL(label));

    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               units->precision, value/units->magnitude, units->units);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

/***** Documentation *******************************************************/

/**
 * GwyUnitoolSlots:
 * @layer_type: The type of the active layer this particular tool uses, like
 *              %GWY_LAYER_SELECT.
 * @layer_setup: Function called when the active layer is created or changed
 *               to tune its properties.
 * @dialog_create: Function creating the tool dialog.
 * @dialog_update: Function called when "updated" signal is received from
 *                 the layer and the tool dialog should be updated.
 * @dialog_abandon: Function called when the tool is abadoned.  It should
 *                  namely take care of the @user_data field of tool state.
 * @apply: Function called when user presses the OK button on the dialog.
 *         It should do whatever the tool is supposed to do but don't touch
 *         the dialog itself.
 * @response: A function handling nonstandard dialog responses, i.e. others
 *            than %GTK_RESPONSE_CLOSE, %GTK_RESPONSE_APPLY,
 *            %GTK_RESPONSE_DELETE_EVENT and %GWY_UNITOOL_RESPONSE_UNSELECT,
 *            that are handled by universal tool itself.  It gets the response
 *            id as its second argument.
 *
 * The custom functions constituting a particular tool, called by universal
 * tool on various occasions.
 *
 * Most of the slots (FIXME: all?) can be %NULL, except @layer_type.
 **/

/**
 * GwyUnitoolState:
 * @user_data: Where you should pointer to particular tool state data.
 * @func_slots: Pointer to function slots for the particular tool.
 * @data_window: The data window the tool is active for.
 * @layer: The layer the tool is using.
 * @is_visible: %TRUE if the dialog is visible, %FALSE if it's hidden.
 * @windowname: The name of @data_window.
 * @dialog: The tool dialog.
 * @coord_units: Units specification good for coordinate representation
 *               (to be used in gwy_unitool_update_label() for coordinates).
 *
 * Universal tool state.
 *
 * You should put pointer to particular tool state to the @user_data member
 * and pointer to function slots to @func_slots when creating it and otherwise
 * consider it read-only.
 **/

/**
 * GWY_UNITOOL_RESPONSE_UNSELECT:
 *
 * Response id you should use for "Clear selection" button, if the tool has
 * any.  Universal tool can than handle it itself.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

