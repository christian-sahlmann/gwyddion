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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <string.h>
#include "app.h"
#include "unitool.h"
#include "menu-windowlist.h"

static void       gwy_unitool_name_changed_cb        (GwyUnitoolState *state);
static void       gwy_unitool_disconnect_handlers    (GwyUnitoolState *state);
static void       gwy_unitool_dialog_abandon         (GwyUnitoolState *state,
                                                      gboolean no_window);
static void       gwy_unitool_compute_formats        (GwyUnitoolState *state);
static void       gwy_unitool_selection_updated_cb   (GwyUnitoolState *state);
static void       gwy_unitool_selection_updated_real (GwyUnitoolState *state,
                                                      gboolean make_visible);
static void       gwy_unitool_data_updated_cb        (GwyUnitoolState *state);
static void       gwy_unitool_dialog_response_cb     (GwyUnitoolState *state,
                                                      gint response);
static void       gwy_unitool_dialog_set_visible     (GwyUnitoolState *state,
                                                      gboolean visible);
static void       gwy_unitool_setup_accel_group      (GwyUnitoolState *state);
static void       gwy_unitool_update_thumbnail       (GwyUnitoolState *state);

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
    GwySelection *selection;
    GwyContainer *data;

    gwy_debug("%p", data_window);
    g_return_val_if_fail(state, FALSE);

    if (!data_window) {
        gwy_unitool_dialog_abandon(state,
                                   reason == GWY_TOOL_SWITCH_WINDOW);
        return TRUE;
    }
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), FALSE);
    data_view = GWY_DATA_VIEW(gwy_data_window_get_data_view(data_window));
    data = gwy_data_view_get_data(data_view);
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
        gwy_data_view_set_top_layer(data_view, state->layer);
        if (slot->layer_setup)
            slot->layer_setup(state);
    }

    /* create dialog */
    gwy_unitool_compute_formats(state);
    if (!state->dialog) {
        state->dialog = slot->dialog_create(state);
        g_signal_connect(state->dialog, "delete-event",
                         G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
        gwy_unitool_setup_accel_group(state);
        gtk_dialog_set_has_separator(GTK_DIALOG(state->dialog), FALSE);
        gtk_window_set_position(GTK_WINDOW(state->dialog), GTK_WIN_POS_NONE);
        gtk_widget_show_all(GTK_DIALOG(state->dialog)->vbox);
        state->is_visible = FALSE;
    }

    /* connect handlers */
    selection = gwy_vector_layer_get_selection(state->layer);
    state->selection_updated_id
        = g_signal_connect_swapped(selection, "changed",
                                   G_CALLBACK(gwy_unitool_selection_updated_cb),
                                   state);
    /* FIXME: */
    state->data_updated_id
        = g_signal_connect_swapped(data_view, "redrawn",
                                   G_CALLBACK(gwy_unitool_data_updated_cb),
                                   state);
    state->windowname_id
        = g_signal_connect_swapped(data_window, "title-changed",
                                   G_CALLBACK(gwy_unitool_name_changed_cb),
                                   state);
    state->response_id
        = g_signal_connect_swapped(state->dialog, "response",
                                   G_CALLBACK(gwy_unitool_dialog_response_cb),
                                   state);

    state->thumbnail_id
        = g_signal_connect_swapped(data_view, "redrawn",
                                   G_CALLBACK(gwy_unitool_update_thumbnail),
                                   state);

    /* setup based on switch reason */
    if (reason == GWY_TOOL_SWITCH_TOOL)
        gwy_unitool_dialog_set_visible(state, TRUE);
    if (reason == GWY_TOOL_SWITCH_WINDOW) {
        gwy_unitool_name_changed_cb(state);
        gwy_unitool_update_thumbnail(state);
    }

    gwy_unitool_selection_updated_real(state, FALSE);

    return TRUE;
}

/***** Callbacks *************************************************************/

static void
gwy_unitool_name_changed_cb(GwyUnitoolState *state)
{
    gwy_debug(" ");
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
        GwySelection *selection;

        /* FIXME: */
        gwy_debug("removing \"selection_updated\" handler");
        selection = gwy_vector_layer_get_selection(state->layer);
        if (state->selection_updated_id)
            g_signal_handler_disconnect(selection, state->selection_updated_id);
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

    if (state->thumbnail_id && state->data_window) {
        GwyDataView *data_view;

        gwy_debug("removing \"redrawn\" handler");
        data_view = gwy_data_window_get_data_view(state->data_window);
        g_signal_handler_disconnect(data_view, state->thumbnail_id);
    }

    state->selection_updated_id = 0;
    state->data_updated_id = 0;
    state->response_id = 0;
    state->windowname_id = 0;
    state->thumbnail_id = 0;
}

static void
gwy_unitool_dialog_abandon(GwyUnitoolState *state,
                           gboolean no_window)
{
    gwy_debug(" ");
    gwy_unitool_disconnect_handlers(state);
    if (state->dialog) {
        if (no_window)
            state->data_window = NULL;
        if (state->func_slots->dialog_abandon)
            state->func_slots->dialog_abandon(state);
        gtk_widget_destroy(state->dialog);
    }
    gwy_si_unit_value_format_free(state->coord_format);
    gwy_si_unit_value_format_free(state->value_format);
    gwy_si_unit_value_format_free(state->coord_hformat);
    gwy_si_unit_value_format_free(state->value_hformat);
    state->coord_format = NULL;
    state->value_format = NULL;
    state->coord_hformat = NULL;
    state->value_hformat = NULL;
    state->layer = NULL;
    state->dialog = NULL;
    state->windowname = NULL;
    state->data_window = NULL;
    state->is_visible = FALSE;
}

static void
gwy_unitool_compute_formats(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield;

    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    state->coord_format
        = gwy_data_field_get_value_format_xy(dfield,
                                             GWY_SI_UNIT_FORMAT_VFMARKUP,
                                             state->coord_format);
    state->value_format
        = gwy_data_field_get_value_format_z(dfield,
                                            GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            state->value_format);
    state->coord_hformat
        = gwy_data_field_get_value_format_xy(dfield,
                                             GWY_SI_UNIT_FORMAT_MARKUP,
                                             state->coord_hformat);
    state->value_hformat
        = gwy_data_field_get_value_format_z(dfield,
                                            GWY_SI_UNIT_FORMAT_MARKUP,
                                            state->value_hformat);
}

/*
 * Handle "updated" signal of layer, eventually calling tool's callback.
 */
static void
gwy_unitool_selection_updated_cb(GwyUnitoolState *state)
{
    gwy_debug(" ");
    gwy_unitool_selection_updated_real(state, TRUE);
}

static void
gwy_unitool_selection_updated_real(GwyUnitoolState *state,
                                   gboolean make_visible)
{
    gint nselected;
    GwySelection *selection;

    selection = gwy_vector_layer_get_selection(state->layer);
    nselected = gwy_selection_get_data(selection, NULL);
    if (state->func_slots->dialog_update)
        state->func_slots->dialog_update(state, GWY_UNITOOL_UPDATED_SELECTION);
    if (make_visible && nselected && !state->is_visible)
        gwy_unitool_dialog_set_visible(state, TRUE);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(state->dialog),
                                      GWY_UNITOOL_RESPONSE_UNSELECT,
                                      nselected);
}

/*
 * Handle "redrawn" signal of DataView, eventually calling tool's callback.
 */
static void
gwy_unitool_data_updated_cb(GwyUnitoolState *state)
{
    gwy_debug(" ");
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
        if (!state->apply_doesnt_close)
            gwy_unitool_dialog_set_visible(state, FALSE);
        state->func_slots->apply(state);
        break;

        case GWY_UNITOOL_RESPONSE_UNSELECT:
        gwy_selection_clear(gwy_vector_layer_get_selection(state->layer));
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

static void
gwy_unitool_setup_accel_group(GwyUnitoolState *state)
{
    GtkWidget *toolbox;
    GtkAccelGroup *accel_group;

    g_return_if_fail(GTK_IS_WINDOW(state->dialog));
    toolbox = gwy_app_main_window_get();
    accel_group = GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(toolbox),
                                                    "accel_group"));
    g_return_if_fail(accel_group);
    gtk_window_add_accel_group(GTK_WINDOW(state->dialog), accel_group);
}

static void
gwy_unitool_update_thumbnail(GwyUnitoolState *state)
{
    GtkWidget *box, *image = NULL;
    GwyDataView *data_view;
    GdkPixbuf *pixbuf;
    GList *children, *c;

    if (!state->windowname)
        return;

    box = gtk_widget_get_ancestor(state->windowname, GTK_TYPE_HBOX);
    c = children = gtk_container_get_children(GTK_CONTAINER(box));
    while (c) {
        if (GTK_IS_IMAGE(c->data)) {
            image = GTK_WIDGET(c->data);
            break;
        }
        c = g_list_next(c);
    }
    g_assert(image);

    data_view = gwy_data_window_get_data_view(state->data_window);
    pixbuf = gwy_data_view_get_thumbnail(data_view, 16);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
    g_object_unref(pixbuf);
}

/***** Helpers *************************************************************/

/**
 * gwy_unitool_dialog_add_button_apply:
 * @dialog: The tool dialog.
 *
 * Adds a Unitool-partially-managed "Apply" button to the tool dialog.
 *
 * See also gwy_unitool_apply_set_sensitive().
 *
 * Returns: The just added button as a #GtkWidget.
 **/
GtkWidget*
gwy_unitool_dialog_add_button_apply(GtkWidget *dialog)
{
    GtkWidget *button;

    button = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                   GTK_STOCK_APPLY,
                                   GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_APPLY);

    return button;
}

/**
 * gwy_unitool_dialog_add_button_clear:
 * @dialog: The tool dialog.
 *
 * Adds a Unitool-managed "Clear" button to the tool dialog.
 *
 * Returns: The just added button as a #GtkWidget.
 **/
GtkWidget*
gwy_unitool_dialog_add_button_clear(GtkWidget *dialog)
{
    GtkWidget *button;

    button = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                   GTK_STOCK_CLEAR,
                                   GWY_UNITOOL_RESPONSE_UNSELECT);

    return button;
}

/**
 * gwy_unitool_dialog_add_button_hide:
 * @dialog: The tool dialog.
 *
 * Adds a Unitool-managed "Hide" button to the tool dialog.
 *
 * Returns: The just added button as a #GtkWidget.
 **/
GtkWidget*
gwy_unitool_dialog_add_button_hide(GtkWidget *dialog)
{
    GtkWidget *button;

    button = gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Hide"),
                                   GTK_RESPONSE_CLOSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

    return button;
}

/**
 * gwy_unitool_apply_set_sensitive:
 * @state: Tool state.
 * @sensitive: %TRUE to make the "Apply" button sensitive.
 *
 * Makes the "Apply" button sensitive or insensitive.
 **/
void
gwy_unitool_apply_set_sensitive(GwyUnitoolState *state,
                                gboolean sensitive)
{
    gtk_dialog_set_response_sensitive(GTK_DIALOG(state->dialog),
                                      GTK_RESPONSE_APPLY,
                                      sensitive);
}

/**
 * gwy_unitool_windowname_frame_create:
 * @state: Tool state.
 *
 * Creates a frame displaying the name of currently active data window.
 *
 * The displayed name automatically changes on tool switch or when the file
 * name changes.
 *
 * You should not make assumptions about the exact type and structure of the
 * returned widget, its changes are not considered API changes.
 *
 * Returns: The name-displaying frame as a #GtkWidget.
 **/
GtkWidget*
gwy_unitool_windowname_frame_create(GwyUnitoolState *state)
{
    GtkWidget *frame, *label, *image, *hbox;
    GwyDataView *data_view;

    g_return_val_if_fail(state, NULL);
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(state->data_window), NULL);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_container_add(GTK_CONTAINER(frame), hbox);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);

    image = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new(gwy_data_window_get_base_name(state->data_window));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
    state->windowname = label;

    gwy_unitool_update_thumbnail(state);
    data_view = gwy_data_window_get_data_view(state->data_window);
    state->thumbnail_id
        = g_signal_connect_swapped(data_view, "redrawn",
                                   G_CALLBACK(gwy_unitool_update_thumbnail),
                                   state);

    return frame;
    /*
    g_return_val_if_fail(state, NULL);
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(state->data_window), NULL);
    return gwy_option_menu_data_window(NULL, NULL, NULL,
                                       GTK_WIDGET(state->data_window));
    */
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
    brj = CLAMP(x + radius + 1, 0, xres - 1);
    bri = CLAMP(y + radius + 1, 0, yres - 1);

    return gwy_data_field_area_get_avg(dfield, ulj, uli, brj - ulj, bri - uli);
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
    static gchar buffer[64];

    g_return_if_fail(units);
    g_return_if_fail(GTK_IS_LABEL(label));

    g_snprintf(buffer, sizeof(buffer), "%.*f%s%s",
               units->precision, value/units->magnitude,
               (units->units && *units->units) ? " " : "", units->units);
    gtk_label_set_markup(GTK_LABEL(label), buffer);
}

/**
 * gwy_unitool_update_label_no_units:
 * @units: Units specification.
 * @label: A label to update (a #GtkLabel).
 * @value: A value to show.
 *
 * Sets the text of a label to display @value according to @units, but
 * excludes units showing the number only.
 **/
void
gwy_unitool_update_label_no_units(GwySIValueFormat *units,
                                  GtkWidget *label, gdouble value)
{
    static gchar buffer[32];

    g_return_if_fail(units);
    g_return_if_fail(GTK_IS_LABEL(label));

    g_snprintf(buffer, sizeof(buffer), "%.*f",
               units->precision, value/units->magnitude);
    gtk_label_set_markup(GTK_LABEL(label), buffer);
}

/**
 * gwy_unitool_get_selection_or_all:
 * @state: Tool state.
 * @xmin: Where upper-left corner x coordinate should be stored.
 * @ymin: Where upper-left corner y coordinate should be stored.
 * @xmax: Where lower-right corner x coordinate should be stored.
 * @ymax: Where lower-right corner y coordinate should be stored.
 *
 * Stores either current selection or complete field in @xmin, @ymin, @xmax,
 * @ymax.
 *
 * Must not be called when the selection layer is not #GwyLayerSelect.
 *
 * Returns: Whether there is a selection.
 **/
gboolean
gwy_unitool_get_selection_or_all(GwyUnitoolState *state,
                                 gdouble *xmin, gdouble *ymin,
                                 gdouble *xmax, gdouble *ymax)
{
    static GType select_layer_type = 0;
    GwySelection *selection;
    gdouble xy[4];
    gboolean is_selected;

    if (!select_layer_type) {
        select_layer_type = g_type_from_name("GwyLayerRectangle");
        g_return_val_if_fail(select_layer_type, FALSE);
    }
    g_return_val_if_fail(G_TYPE_CHECK_INSTANCE_TYPE((state->layer),
                                                    select_layer_type),
                         FALSE);

    selection = gwy_vector_layer_get_selection(state->layer);
    is_selected = gwy_selection_get_object(selection, 0, xy);

    if (is_selected) {
        if (xy[0] > xy[2])
            GWY_SWAP(gdouble, xy[0], xy[2]);
        if (xy[1] > xy[3])
            GWY_SWAP(gdouble, xy[1], xy[3]);

        *xmin = xy[0];
        *ymin = xy[1];
        *xmax = xy[2];
        *ymax = xy[3];
    }
    else {
        GwyContainer *data;
        GwyDataField *dfield;

        data = gwy_data_window_get_data(state->data_window);
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        *xmin = 0;
        *ymin = 0;
        *xmax = gwy_data_field_get_xreal(dfield);
        *ymax = gwy_data_field_get_yreal(dfield);
    }

    return is_selected;
}

/**
 * gwy_unitool_rect_info_table_setup:
 * @rinfo: A rectangular selection display data.
 * @table: Table to place the widgets to.
 * @col: Starting column in @table.
 * @row: Starting row in @table.
 *
 * Places widgets displaying rectangular selection information to a table.
 *
 * This function initializes the #GwyUnitoolRectLabels widgets fields and thus
 * must be used before gwy_unitool_rect_info_table_fill().
 *
 * Returns: The number of rows taken.
 **/
gint
gwy_unitool_rect_info_table_setup(GwyUnitoolRectLabels *rinfo,
                                  GtkTable *table,
                                  gint col,
                                  gint row)
{
    GtkWidget *label;

    g_return_val_if_fail(GTK_IS_TABLE(table), 0);
    g_return_val_if_fail(rinfo, 0);

    gtk_table_set_col_spacing(table, col, 12);
    gtk_table_set_col_spacing(table, col+1, 12);
    if (row)
        gtk_table_set_row_spacing(table, row-1, 8);
    gtk_table_set_row_spacing(table, row+2, 8);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Origin</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, col, col+1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new("X");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, col, col+1, row+1, row+2,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new("Y");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, col, col+1, row+2, row+3,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Size</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, col, col+1, row+3, row+4,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, col, col+1, row+4, row+5,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("Height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, col, col+1, row+5, row+6,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    rinfo->xreal = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(rinfo->xreal), 1.0, 0.5);
    gtk_table_attach(table, rinfo->xreal, col+1, col+2, row+1, row+2,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    rinfo->yreal = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(rinfo->yreal), 1.0, 0.5);
    gtk_table_attach(table, rinfo->yreal, col+1, col+2, row+2, row+3,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    rinfo->wreal = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(rinfo->wreal), 1.0, 0.5);
    gtk_table_attach(table, rinfo->wreal, col+1, col+2, row+4, row+5,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    rinfo->hreal = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(rinfo->hreal), 1.0, 0.5);
    gtk_table_attach(table, rinfo->hreal, col+1, col+2, row+5, row+6,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    rinfo->xpix = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(rinfo->xpix), 1.0, 0.5);
    gtk_table_attach(table, rinfo->xpix, col+2, col+3, row+1, row+2,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    rinfo->ypix = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(rinfo->ypix), 1.0, 0.5);
    gtk_table_attach(table, rinfo->ypix, col+2, col+3, row+2, row+3,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    rinfo->wpix = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(rinfo->wpix), 1.0, 0.5);
    gtk_table_attach(table, rinfo->wpix, col+2, col+3, row+4, row+5,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    rinfo->hpix = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(rinfo->hpix), 1.0, 0.5);
    gtk_table_attach(table, rinfo->hpix, col+2, col+3, row+5, row+6,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    return 6;  /* the number of rows taken */
}

/**
 * gwy_unitool_rect_info_table_fill:
 * @state: Tool state.
 * @rinfo: A rectangular selection display data.
 * @selreal: If not %NULL, must be an array of size at least 4 and will be
 *           filled with selection data xmin, ymin, xmax, ymax in physical
 *           units.
 * @selpix: If not %NULL, must be an array of size at least 4 and will be
 *          filled with selection data xmin, ymin, xmax, ymax in pixels.
 *
 * Updates rectangular selection info display.
 *
 * Returns: %TRUE if a selection is present, %FALSE otherwise.
 **/
gboolean
gwy_unitool_rect_info_table_fill(GwyUnitoolState *state,
                                 GwyUnitoolRectLabels *rinfo,
                                 gdouble *selreal,
                                 gint *selpix)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GwySIValueFormat *units;
    gdouble sel[4];
    gint isel[4];
    gchar buf[16];
    gboolean is_selected;

    data = gwy_data_window_get_data(GWY_DATA_WINDOW(state->data_window));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    is_selected = gwy_unitool_get_selection_or_all(state,
                                                   sel, sel+1, sel+2, sel+3);
    if (is_selected || rinfo->unselected_is_full) {
        units = state->coord_format;
        gwy_unitool_update_label(units, rinfo->xreal, sel[0]);
        gwy_unitool_update_label(units, rinfo->yreal, sel[1]);
        gwy_unitool_update_label(units, rinfo->wreal, sel[2] - sel[0]);
        gwy_unitool_update_label(units, rinfo->hreal, sel[3] - sel[1]);

        isel[0] = gwy_data_field_rtoj(dfield, sel[0]);
        g_snprintf(buf, sizeof(buf), "%d %s", isel[0], _("px"));
        gtk_label_set_text(GTK_LABEL(rinfo->xpix), buf);

        isel[1] = gwy_data_field_rtoi(dfield, sel[1]);
        g_snprintf(buf, sizeof(buf), "%d %s", isel[1], _("px"));
        gtk_label_set_text(GTK_LABEL(rinfo->ypix), buf);

        if (is_selected)
            isel[2] = gwy_data_field_rtoj(dfield, sel[2]) + 1;
        else
            isel[2] = gwy_data_field_get_xres(dfield);
        g_snprintf(buf, sizeof(buf), "%d %s", isel[2] - isel[0], _("px"));
        gtk_label_set_text(GTK_LABEL(rinfo->wpix), buf);

        if (is_selected)
            isel[3] = gwy_data_field_rtoi(dfield, sel[3]) + 1;
        else
            isel[3] = gwy_data_field_get_yres(dfield);
        g_snprintf(buf, sizeof(buf), "%d %s", isel[3] - isel[1], _("px"));
        gtk_label_set_text(GTK_LABEL(rinfo->hpix), buf);

        if (selreal)
            memcpy(selreal, sel, 4*sizeof(gdouble));
        if (selpix)
            memcpy(selpix, isel, 4*sizeof(gint));
    }
    else {
        gtk_label_set_text(GTK_LABEL(rinfo->xreal), "");
        gtk_label_set_text(GTK_LABEL(rinfo->yreal), "");
        gtk_label_set_text(GTK_LABEL(rinfo->wreal), "");
        gtk_label_set_text(GTK_LABEL(rinfo->hreal), "");
        gtk_label_set_text(GTK_LABEL(rinfo->xpix), "");
        gtk_label_set_text(GTK_LABEL(rinfo->ypix), "");
        gtk_label_set_text(GTK_LABEL(rinfo->wpix), "");
        gtk_label_set_text(GTK_LABEL(rinfo->hpix), "");
    }

    return is_selected;
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
 * @coord_format: Format good for coordinate representation
 *                (to be used in gwy_unitool_update_label() for coordinates).
 * @value_format: Format good for value representation
 *                (to be used in gwy_unitool_update_label() for values).
 * @coord_hformat: Format good for standalone coordinate unit representation
 *                 (e.g., in table headers).
 * @value_hformat: Format good for standalone value unit representation
 *                 (e.g., in table headers).
 * @apply_doesnt_close: When set to %TRUE "Apply" button doesn't close (hide)
 *                      the tool dialog.
 *
 * Universal tool state.
 *
 * You should put pointer to particular tool state to the @user_data member
 * and pointer to function slots to @func_slots when creating it and otherwise
 * consider it read-only.
 *
 * Always use g_new0() or zero-fill the memory by other means when creating
 * an unitialized state.
 **/

/**
 * GwyUnitoolRectLabels:
 * @xreal: Selection x-origin in physical units widget.
 * @yreal: Selection y-origin in physical units widget.
 * @wreal: Selection width in physical units widget.
 * @hreal: Selection height in physical units widget.
 * @xpix: Selection x-origin in pixels widget.
 * @ypix: Selection y-origin in pixels widget.
 * @wpix: Selection width in pixels widget.
 * @hpix: Selection height in pixels widget.
 * @unselected_is_full: If %TRUE, no selection is displayed as full data range,
 *                      if %FALSE, labels are cleared when nothing is selected.
 * @rwidget1: Reserved.
 * @rwidget2: Reserved.
 * @rwidget3: Reserved.
 * @rwidget4: Reserved.
 * @reserved1: Reserved.
 * @reserved2: Reserved.
 *
 * Widgets and flags for rectangular selection display.
 *
 * You will probably ever need to access the flag fields only.
 */

/**
 * GWY_UNITOOL_RESPONSE_UNSELECT:
 *
 * Response id you should use for "Clear selection" button, if the tool has
 * any.  Universal tool can than handle it itself.
 **/

/**
 * GwyUnitoolFunc:
 * @state: Tool state.
 *
 * General Unitool slot function signature.
 **/

/**
 * GwyUnitoolCreateFunc:
 * @state: Tool state.
 *
 * Dialog constructor slot function signature.
 *
 * Returns: The newly created dialog.
 **/

/**
 * GwyUnitoolResponseFunc:
 * @state: Tool state.
 * @response: The tool dialog response.
 *
 * Extra dialog response handler slot function signature.
 **/

/**
 * GwyUnitoolUpdateFunc:
 * @state: Tool state.
 * @reason: Update reason.
 *
 * Tool update slot function signature.
 **/

/**
 * GwyUnitoolUpdateType:
 * @GWY_UNITOOL_UPDATED_SELECTION: Selection have changed.
 * @GWY_UNITOOL_UPDATED_DATA: Data have changed.
 * @GWY_UNITOOL_UPDATED_CONTROLS: Unused.
 *
 * Reason why dialog_update() tool function was called.
 */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

