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

void
gwy_unitool_use(GwyUnitoolState *state,
                GwyDataWindow *data_window,
                GwyToolSwitchEvent reason)
{
    GwyUnitoolSlots *slot;
    GwyVectorLayer *layer;
    GwyDataView *data_view;

    gwy_debug("%p", data_window);
    g_return_if_fail(state);

    if (!data_window) {
        gwy_unitool_dialog_abandon(state);
        return;
    }
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = GWY_DATA_VIEW(gwy_data_window_get_data_view(data_window));
    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && (GwyDataViewLayer*)layer == state->layer) {
        g_assert(state->data_window = data_window);
        if (reason == GWY_TOOL_SWITCH_TOOL)
            gwy_unitool_dialog_set_visible(state, TRUE);
        return;
    }

    /* disconnect existing handlers */
    gwy_unitool_disconnect_handlers(state);
    state->data_window = data_window;

    /* create or set-up the layer */
    slot = state->func_slots;
    if (layer && G_TYPE_CHECK_INSTANCE_TYPE(layer, slot->layer_type)) {
        state->layer = GWY_DATA_VIEW_LAYER(layer);
        if (slot->layer_setup)
            slot->layer_setup(state);
    }
    else {
        state->layer = (GwyDataViewLayer*)slot->layer_constructor();
        if (slot->layer_setup)
            slot->layer_setup(state);
        gwy_data_view_set_top_layer(data_view, GWY_VECTOR_LAYER(state->layer));
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
        gwy_debug("1");
        if (state->layer_updated_id)
            g_signal_handler_disconnect(state->layer,
                                        state->layer_updated_id);
        gwy_debug("2");
        if (state->layer->parent && state->data_updated_id)
            g_signal_handler_disconnect(state->layer->parent,
                                        state->data_updated_id);
    }
    gwy_debug("3");
    if (state->dialog && state->response_id)
        g_signal_handler_disconnect(state->dialog, state->response_id);
    gwy_debug("4");
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
    state->layer = NULL;
    state->dialog = NULL;
    state->windowname = NULL;
    state->data_window = NULL;
    state->is_visible = FALSE;

    g_free(state->coord_units.units);
    state->coord_units.units = NULL;
}

static void
gwy_unitool_compute_coord_units(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GwyUnitoolUnits *cunits;
    gdouble xreal, yreal, max, unit;

    cunits = &state->coord_units;
    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(dfield),
               yreal/gwy_data_field_get_yres(dfield));
    cunits->mag = gwy_math_humanize_numbers(unit, max, &cunits->precision);
    g_free(state->coord_units.units);
    cunits->units = g_strconcat(gwy_math_SI_prefix(cunits->mag), "m", NULL);
}

static void
gwy_unitool_selection_updated_cb(GwyUnitoolState *state)
{
    gint nselected;

    gwy_debug("");
    nselected = gwy_vector_layer_get_nselected(GWY_VECTOR_LAYER(state->layer));
    if (state->func_slots->dialog_update)
        state->func_slots->dialog_update(state);
    if (nselected && !state->is_visible)
        gwy_unitool_dialog_set_visible(state, TRUE);
}

static void
gwy_unitool_data_updated_cb(GwyUnitoolState *state)
{
    gwy_debug("");
    if (!state->is_visible)
        return;
    if (state->func_slots->dialog_update)
        state->func_slots->dialog_update(state);
}

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
        state->func_slots->apply(state);
        gwy_unitool_dialog_set_visible(state, FALSE);
        break;

        case GWY_UNITOOL_RESPONSE_UNSELECT:
        gwy_vector_layer_unselect(GWY_VECTOR_LAYER(state->layer));
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
    gwy_debug("now %d, setting to %d",
              state->is_visible, visible);
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

void
gwy_unitool_update_label(GwyUnitoolUnits *units,
                         GtkWidget *label, gdouble value)
{
    static gchar buffer[32];

    g_return_if_fail(units);
    g_return_if_fail(GTK_IS_LABEL(label));

    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               units->precision, value/units->mag, units->units);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

