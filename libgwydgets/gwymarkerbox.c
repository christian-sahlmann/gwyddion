/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
#include <libgwydgets/gwymarkerbox.h>

enum { MARKER_SIZE = 8 };

enum {
    PROP_0,
    PROP_FLIPPED,
    PROP_HIGHLIGHT_SELECTED,
    PROP_SELECTED_MARKER,
    PROP_LAST
};

enum {
    MARKER_SELECTED,
    MARKER_MOVED,
    MARKER_ADDED,
    MARKER_REMOVED,
    MARKERS_SET,
    LAST_SIGNAL
};

static void     gwy_marker_box_finalize         (GObject *object);
static void     gwy_marker_box_set_property     (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_marker_box_get_property     (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_marker_box_realize          (GtkWidget *widget);
static void     gwy_marker_box_size_request     (GtkWidget *widget,
                                                 GtkRequisition *requisition);
static void     gwy_marker_box_paint            (GwyMarkerBox *mbox);
static gboolean gwy_marker_box_expose           (GtkWidget *widget,
                                                  GdkEventExpose *event);
static void     gwy_marker_box_state_changed    (GtkWidget *widget,
                                                  GtkStateType state);

static guint marker_box_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE(GwyMarkerBox, gwy_marker_box, GTK_TYPE_WIDGET)

static void
gwy_marker_box_class_init(GwyMarkerBoxClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    widget_class = (GtkWidgetClass*)klass;

    gobject_class->finalize = gwy_marker_box_finalize;
    gobject_class->set_property = gwy_marker_box_set_property;
    gobject_class->get_property = gwy_marker_box_get_property;

    widget_class->size_request = gwy_marker_box_size_request;
    widget_class->realize = gwy_marker_box_realize;
    widget_class->expose_event = gwy_marker_box_expose;
    widget_class->state_changed = gwy_marker_box_state_changed;

    g_object_class_install_property(
        gobject_class,
        PROP_FLIPPED,
        g_param_spec_boolean("flipped",
                             "Flipped",
                             "Whether marks are drawn upside down.",
                             FALSE,
                             G_PARAM_READWRITE));

    g_object_class_install_property(
        gobject_class,
        PROP_FLIPPED,
        g_param_spec_boolean("highlight-selected",
                             "Highlight selected",
                             "Whether to visually differentiate selected "
                             "marker.",
                             TRUE,
                             G_PARAM_READWRITE));

    g_object_class_install_property(
        gobject_class,
        PROP_SELECTED_MARKER,
        g_param_spec_int("selected-marker",
                         "Selected marker",
                         "The index of selected marker, -1 if none.",
                         -1, 1024, -1,
                         G_PARAM_READWRITE));

    /**
     * GwyMarkerBox::marker-selected:
     * @arg1: The index of selected marker, -1 when marker was unselected.
     * @gwymarkerbox: The #GwyMarkerBox which received the signal.
     *
     * The ::marker-selected signal is emitted when marker selection changes.
     **/
    marker_box_signals[MARKER_SELECTED]
        = g_signal_new("marker-selected",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyMarkerBoxClass, marker_selected),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * GwyMarkerBox::marker-moved:
     * @arg1: The index of moved marker.
     * @gwymarkerbox: The #GwyMarkerBox which received the signal.
     *
     * The ::marker-moved signal is emitted when a marker is moved.
     **/
    marker_box_signals[MARKER_MOVED]
        = g_signal_new("marker-moved",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyMarkerBoxClass, marker_moved),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * GwyMarkerBox::marker-added:
     * @arg1: The index a marker was added at.
     * @gwymarkerbox: The #GwyMarkerBox which received the signal.
     *
     * The ::marker-added signal is emitted when a marker is added.
     **/
    marker_box_signals[MARKER_ADDED]
        = g_signal_new("marker-added",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyMarkerBoxClass, marker_added),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * GwyMarkerBox::marker-removed:
     * @arg1: The index a marker was removed from.
     * @gwymarkerbox: The #GwyMarkerBox which received the signal.
     *
     * The ::marker-removed signal is emitted when a marker is removed.
     **/
    marker_box_signals[MARKER_REMOVED]
        = g_signal_new("marker-removed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyMarkerBoxClass, marker_removed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * GwyMarkerBox::markers-set:
     * @gwymarkerbox: The #GwyMarkerBox which received the signal.
     *
     * The ::markers-set signal is emitted when markers are explicitly set
     * with gwy_marker_box_set_markers().
     **/
    marker_box_signals[MARKERS_SET]
        = g_signal_new("markers-set",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyMarkerBoxClass, markers_set),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_marker_box_init(GwyMarkerBox *mbox)
{
    mbox->markers = g_array_new(FALSE, FALSE, sizeof(gdouble));
    mbox->selected = -1;
    mbox->highlight = TRUE;
}

static void
gwy_marker_box_finalize(GObject *object)
{
    GwyMarkerBox *mbox;

    mbox = GWY_MARKER_BOX(object);
    g_array_free(mbox->markers, TRUE);

    G_OBJECT_CLASS(gwy_marker_box_parent_class)->finalize(object);
}

static void
gwy_marker_box_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyMarkerBox *mbox = GWY_MARKER_BOX(object);

    switch (prop_id) {
        case PROP_FLIPPED:
        gwy_marker_box_set_flipped(mbox, g_value_get_boolean(value));
        break;

        case PROP_HIGHLIGHT_SELECTED:
        gwy_marker_box_set_highlight_selected(mbox, g_value_get_boolean(value));
        break;

        case PROP_SELECTED_MARKER:
        gwy_marker_box_set_selected_marker(mbox, g_value_get_int(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_marker_box_get_property(GObject*object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyMarkerBox *mbox = GWY_MARKER_BOX(object);

    switch (prop_id) {
        case PROP_FLIPPED:
        g_value_set_boolean(value, mbox->flipped);
        break;

        case PROP_HIGHLIGHT_SELECTED:
        g_value_set_boolean(value, mbox->highlight);
        break;

        case PROP_SELECTED_MARKER:
        g_value_set_int(value, mbox->selected);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_marker_box_realize(GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget)
                            | GDK_EXPOSURE_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_POINTER_MOTION_MASK
                            | GDK_POINTER_MOTION_HINT_MASK;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
                                    &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gwy_marker_box_size_request(G_GNUC_UNUSED GtkWidget *widget,
                            GtkRequisition *requisition)
{
    requisition->width = MARKER_SIZE;
    requisition->height = MARKER_SIZE;
}

static void
gwy_marker_box_paint(GwyMarkerBox *mbox)
{
    GwyMarkerBoxClass *klass;
    guint i;

    klass = GWY_MARKER_BOX_GET_CLASS(mbox);
    klass->draw_box(mbox);

    for (i = 0; i < mbox->markers->len; i++) {
        if (mbox->highlight && i == mbox->selected)
            continue;
        klass->draw_marker(mbox, i);
    }
    /* Draw selected last, `over' other markers */
    if (mbox->highlight && mbox->selected >= 0)
        klass->draw_marker(mbox, mbox->selected);
}

static gboolean
gwy_marker_box_expose(GtkWidget *widget,
                      G_GNUC_UNUSED GdkEventExpose *event)
{
    gwy_marker_box_paint(GWY_MARKER_BOX(widget));

    return FALSE;
}

static void
gwy_marker_box_state_changed(GtkWidget *widget,
                              GtkStateType state)
{
    GtkStateType oldstate;

    oldstate = GTK_WIDGET_STATE(widget);
    gwy_debug("state: %d, current: %d", state, oldstate);
    if (state == GTK_STATE_INSENSITIVE
        || oldstate == GTK_STATE_INSENSITIVE)
        gtk_widget_queue_draw(widget);

    if (GTK_WIDGET_CLASS(gwy_marker_box_parent_class)->state_changed)
        GTK_WIDGET_CLASS(gwy_marker_box_parent_class)->state_changed(widget,
                                                                      state);
}

/**
 * gwy_marker_box_get_selected_marker:
 * @mbox: A marker box.
 *
 * Gets the index of the currently selected marker in a marker
 * box.
 *
 * Returns: The index of currently selected marker, -1 when none is
 *          selected.
 **/
gint
gwy_marker_box_get_selected_marker(GwyMarkerBox *mbox)
{
    g_return_val_if_fail(GWY_IS_MARKER_BOX(mbox), -1);

    return mbox->selected;
}

/**
 * gwy_marker_box_set_selected_marker:
 * @mbox: A marker box.
 * @i: The index of marker to select.  Pass -1 to unselect.
 *
 * Selects a marker in a marker box.
 **/
void
gwy_marker_box_set_selected_marker(GwyMarkerBox *mbox,
                                   gint i)
{
    g_return_if_fail(GWY_IS_MARKER_BOX(mbox));
    g_return_if_fail(i < (gint)mbox->markers->len);

    /* Negative value means no selection */
    if (i < 0)
        i = -1;

    if (i == mbox->selected)
        return;

    mbox->selected = i;
    if (GTK_WIDGET_REALIZED(mbox))
        gtk_widget_queue_draw(GTK_WIDGET(mbox));
    g_signal_emit(mbox, marker_box_signals[MARKER_SELECTED], 0, i);
}

/**
 * gwy_marker_box_get_marker_position:
 * @mbox: A marker box.
 * @i: The index of marker to get position of.
 *
 * Gets the position of a marker in a marker box.
 *
 * Returns: The marker position, in the range [0.0, 1.0].
 **/
gdouble
gwy_marker_box_get_marker_position(GwyMarkerBox *mbox,
                                   gint i)
{
    g_return_val_if_fail(GWY_IS_MARKER_BOX(mbox), 0.0);
    g_return_val_if_fail(i >= 0 && i < mbox->markers->len, 0.0);

    return g_array_index(mbox->markers, gdouble, i);
}

/**
 * gwy_marker_box_set_marker_position:
 * @mbox: A marker box.
 * @i: Index of marker to move.
 * @pos: The new marker position, in the range [0.0, 1.0].
 *
 * Moves a marker in a marker box.
 *
 * Returns: %TRUE on success. If the move does not validate, %FALSE is returned
 *          and the marker position does not change.
 **/
gboolean
gwy_marker_box_set_marker_position(GwyMarkerBox *mbox,
                                   gint i,
                                   gdouble pos)
{
    gdouble currpos;
    gboolean needs_redraw;
    gint j, width;

    g_return_val_if_fail(GWY_IS_MARKER_BOX(mbox), FALSE);
    g_return_val_if_fail(i >= 0 && i < mbox->markers->len, FALSE);

    currpos = g_array_index(mbox->markers, gdouble, i);
    j = i;
    if ((mbox->validate
         && !mbox->validate(mbox, GWY_MARKER_OPERATION_MOVE, &j, &pos))
        || pos == currpos)
        return FALSE;

    width = GTK_WIDGET(mbox)->allocation.width;
    needs_redraw = (ROUND(pos*(width - 1)) != ROUND(currpos*(width - 1)));

    g_array_index(mbox->markers, gdouble, i) = pos;
    if (needs_redraw && GTK_WIDGET_REALIZED(mbox))
        gtk_widget_queue_draw(GTK_WIDGET(mbox));
    g_signal_emit(mbox, marker_box_signals[MARKER_MOVED], 0, i);

    return TRUE;
}

/**
 * gwy_marker_box_add_marker:
 * @mbox: A marker box.
 * @i: Index to insert marker at.
 * @pos: Position to insert marker to, in the range [0.0, 1.0].
 *
 * Adds a marker to a marker box.
 *
 * Returns: On success, the index the marker was added at.  If the insertion
 *          does not validate, -1 is returned and no marker is added.
 **/
gint
gwy_marker_box_add_marker(GwyMarkerBox *mbox,
                          gint i,
                          gdouble pos)
{
    gboolean selection_changed = FALSE;

    g_return_val_if_fail(GWY_IS_MARKER_BOX(mbox), -1);
    g_return_val_if_fail(i >= 0 && i <= mbox->markers->len, -1);

    if (pos < 0.0 || pos > 1.0)
        return -1;

    if (mbox->validate
        && !mbox->validate(mbox, GWY_MARKER_OPERATION_ADD, &i, &pos))
        return -1;

    if (i == mbox->markers->len)
        g_array_append_val(mbox->markers, pos);
    else
        g_array_insert_val(mbox->markers, i, pos);

    if (i <= mbox->selected) {
        mbox->selected++;
        selection_changed = TRUE;
    }

    if (GTK_WIDGET_REALIZED(mbox))
        gtk_widget_queue_draw(GTK_WIDGET(mbox));

    g_signal_emit(mbox, marker_box_signals[MARKER_ADDED], 0, i);
    if (selection_changed)
        g_signal_emit(mbox, marker_box_signals[MARKER_SELECTED], 0,
                      mbox->selected);

    return i;
}

/**
 * gwy_marker_box_remove_marker:
 * @mbox: A marker box.
 * @i: Index of marker to remove.
 *
 * Removes a marker from a marker box.
 *
 * Returns: %TRUE on success. If the removal does not validate, %FALSE is
 *          returned and the marker is kept.
 **/
gboolean
gwy_marker_box_remove_marker(GwyMarkerBox *mbox,
                             gint i)
{
    gboolean selection_changed = FALSE;
    gdouble pos;
    gint j;

    g_return_val_if_fail(GWY_IS_MARKER_BOX(mbox), FALSE);
    g_return_val_if_fail(i >= 0 && i < mbox->markers->len, FALSE);

    pos = g_array_index(mbox->markers, gdouble, i);
    j = i;
    if (mbox->validate
        && !mbox->validate(mbox, GWY_MARKER_OPERATION_REMOVE, &j, &pos))
        return FALSE;

    if (i == mbox->selected)
        gwy_marker_box_set_selected_marker(mbox, -1);

    g_array_remove_index(mbox->markers, i);
    if (i < mbox->selected) {
        mbox->selected--;
        selection_changed = TRUE;
    }

    if (GTK_WIDGET_REALIZED(mbox))
        gtk_widget_queue_draw(GTK_WIDGET(mbox));

    g_signal_emit(mbox, marker_box_signals[MARKER_REMOVED], 0, i);
    if (selection_changed)
        g_signal_emit(mbox, marker_box_signals[MARKER_SELECTED], 0,
                      mbox->selected);

    return TRUE;
}

/**
 * gwy_marker_box_get_nmarkers:
 * @mbox: A marker box.
 *
 * Gets the number of markers in a marker box.
 *
 * Returns: The number of markers.
 **/
gint
gwy_marker_box_get_nmarkers(GwyMarkerBox *mbox)
{
    g_return_val_if_fail(GWY_IS_MARKER_BOX(mbox), 0);

    return mbox->markers->len;
}

/**
 * gwy_marker_box_get_markers:
 * @mbox: A marker box.
 *
 * Gets all markers in a marker box.
 *
 * Returns: The markers as an array of positions, owned by @mbox.  It must
 *          not be modified nor freed by caller and it's valid only until
 *          next marker change.
 **/
const gdouble*
gwy_marker_box_get_markers(GwyMarkerBox *mbox)
{
    g_return_val_if_fail(GWY_IS_MARKER_BOX(mbox), NULL);

    return (gdouble*)mbox->markers->data;
}

/**
 * gwy_marker_box_set_markers:
 * @mbox: A marker box.
 * @n: The number of markers to set.  If it is zero, @markers can be %NULL.
 * @markers: Markers position.
 *
 * Sets positions of all markers in a marker box.
 *
 * No validation is performed, even if validator is set.  It's up to caller to
 * set markers that do not logically conflict with the validator.
 **/
void
gwy_marker_box_set_markers(GwyMarkerBox *mbox,
                           gint n,
                           const gdouble *markers)
{
    gint i;

    g_return_if_fail(GWY_IS_MARKER_BOX(mbox));
    g_return_if_fail(n >= 0);
    g_return_if_fail(!n || markers);

    if (n == mbox->markers->len) {
        for (i = 0; i < n; i++) {
            if (g_array_index(mbox->markers, gdouble, i) == markers[i])
                break;
        }
        if (i == n)
            return;
    }

    gwy_marker_box_set_selected_marker(mbox, -1);
    g_array_set_size(mbox->markers, 0);
    g_array_append_vals(mbox->markers, markers, n);
    g_signal_emit(mbox, marker_box_signals[MARKERS_SET], 0);

    if (GTK_WIDGET_REALIZED(mbox))
        gtk_widget_queue_draw(GTK_WIDGET(mbox));
}

/**
 * gwy_marker_box_set_flipped:
 * @mbox: A marker box.
 * @flipped: %TRUE to draw markers upside down.
 *
 * Sets whether a marker box is drawn upside down.
 **/
void
gwy_marker_box_set_flipped(GwyMarkerBox *mbox,
                           gboolean flipped)
{
    g_return_if_fail(GWY_IS_MARKER_BOX(mbox));

    flipped = !!flipped;
    if (flipped == mbox->flipped)
        return;

    mbox->flipped = flipped;
    if (GTK_WIDGET_REALIZED(mbox))
        gtk_widget_queue_draw(GTK_WIDGET(mbox));
    g_object_notify(G_OBJECT(mbox), "flipped");
}

/**
 * gwy_marker_box_get_flipped:
 * @mbox: A marker box.
 *
 * Returns whether a marker box is drawn upside down.
 *
 * Returns: %TRUE if markers are drawn upside down.
 **/
gboolean
gwy_marker_box_get_flipped(GwyMarkerBox *mbox)
{
    g_return_val_if_fail(GWY_IS_MARKER_BOX(mbox), FALSE);

    return mbox->flipped;
}

/**
 * gwy_marker_box_set_highlight_selected:
 * @mbox: A marker box.
 * @highlight: %TRUE to visually differentiate selected marker, %FALSE to
 *             draw markers uniformly.
 *
 * Sets whether a marker box highlights selected marker.
 **/
void
gwy_marker_box_set_highlight_selected(GwyMarkerBox *mbox,
                                      gboolean highlight)
{
    g_return_if_fail(GWY_IS_MARKER_BOX(mbox));

    highlight = !!highlight;
    if (highlight == mbox->highlight)
        return;

    mbox->highlight = highlight;
    if (mbox->selected >= 0 && GTK_WIDGET_REALIZED(mbox))
        gtk_widget_queue_draw(GTK_WIDGET(mbox));
    g_object_notify(G_OBJECT(mbox), "highlight-selected");
}

/**
 * gwy_marker_box_get_highlight_selected:
 * @mbox: A marker box.
 *
 * Returns whether a marker box highlights selected marker.
 *
 * Returns: %TRUE if selected marker is visually differentiated, %FALSE if
 *          markers are drawn uniformly.
 **/
gboolean
gwy_marker_box_get_highlight_selected(GwyMarkerBox *mbox)
{
    g_return_val_if_fail(GWY_IS_MARKER_BOX(mbox), FALSE);

    return mbox->highlight;
}

/**
 * gwy_marker_box_set_validator:
 * @mbox: A marker box.
 * @validate: Marker validation function.  Pass %NULL to disable validation.
 *
 * Sets marker box marker validation function.
 *
 * It is used the next time an attempt to change markers is made, no
 * revalidation is done immediately.  It's up to caller to set a validator
 * that do not logically conflict with the distribution of markers.
 **/
void
gwy_marker_box_set_validator(GwyMarkerBox *mbox,
                             GwyMarkerValidateFunc validate)
{
    g_return_if_fail(GWY_IS_MARKER_BOX(mbox));

    if (validate == mbox->validate)
        return;

    mbox->validate = validate;
}

/**
 * gwy_marker_box_get_validator:
 * @mbox: A marker box.
 *
 * Gets the marker validation function currently in use.
 *
 * Returns: The marker validation function.
 **/
GwyMarkerValidateFunc
gwy_marker_box_get_validator(GwyMarkerBox *mbox)
{
    g_return_val_if_fail(GWY_IS_MARKER_BOX(mbox), NULL);

    return mbox->validate;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymarkerbox
 * @title: GwyMarkerBox
 * @short_description: Base class for box with movable markers.
 *
 * #GwyMarkerBox is a box with triangular markers that can be
 * moved, added, and/or deleted.  One or no marker can be selected.
 *
 * Marker coordinates are always from the range [0.0, 1.0].
 *
 * It is possible to fully control where and how user can move, add, and/or
 * delete markers with marker validation function set with
 * gwy_marker_box_set_validator().  By default, no validator is in effect
 * and user can change markers completely freely.
 **/

/**
 * GwyMarkerValidateFunc:
 * @mbox: The #GwyMarkerBox to validate markers for.
 * @optype: Marker operation to validate.
 * @i: Pointer to index of marker to validate.  For insertion, it's the
 *     position the marker would be inserted to (but it's not there yet),
 *     for removal it's the position where it still is.  The validator can
 *     change the index, but it has an effect only when a marker is being
 *     added.
 * @pos: Pointer to requested marker position. The validator can change the
 *       position and the marker will be then moved or inserted to the changed
 *       position. For removal, its changes have no effect.
 *
 * Marker validation function.
 *
 * It is called for each single-marker change, both by user and by
 * #GwyMarkerBox methods.  However, it is NOT called upon
 * gwy_marker_box_set_markers() as it is unclear how the validation should
 * proceed.
 *
 * The function must not have any side-effects, that is it must not assume the
 * operation will be actually performed when it returns %TRUE.
 *
 * Marker validator that allows free marker movement but disables insertion
 * and removal could look:
 * <informalexample><programlisting>
 * static gboolean
 * validate_marker(GwyMarkerBox *mbox,
 *                 GwyMarkerOperationType optype,
 *                 gint *i,
 *                 gdouble *pos)
 * {
 *     if (optype == GWY_MARKER_OPERATION_ADD
 *         || optype == GWY_MARKER_OPERATION_REMOVE)
 *         return FALSE;
 *     return TRUE;
 * }
 * </programlisting></informalexample>
 *
 * Marker validator that assures markers are sorted and there is always
 * a marker at 0.0 and another at 1.0 could look:
 * <informalexample><programlisting>
 * static gboolean
 * validate_marker(GwyMarkerBox *mbox,
 *                 GwyMarkerOperationType optype,
 *                 gint *i,
 *                 gdouble *pos)
 * {
 *     const gdouble *markers;
 *     gint j, n;
 *     <!-- Hello, gtk-doc! -->
 *     n = gwy_marker_box_get_nmarkers(mbox);
 *     <!-- Hello, gtk-doc! -->
 *     /<!-- -->* Insertions are sorted *<!-- -->/
 *     if (optype == GWY_MARKER_OPERATION_ADD) {
 *         markers = gwy_marker_box_get_markers(mbox);
 *         for (j = 0; j < n; j++) {
 *             if (*pos < markers[j])
 *                 break;
 *         }
 *         if (j == 0 || j == n)
 *             return FALSE;
 *         *i = j;
 *         return TRUE;
 *     }
 *     <!-- Hello, gtk-doc! -->
 *     /<!-- -->* Nothing at all can be done with border markers *<!-- -->/
 *     if (*i == 0 || *i == n-1)
 *         return FALSE;
 *     <!-- Hello, gtk-doc! -->
 *     /<!-- -->* Inner markers can be moved only from previous to next *<!-- -->/
 *     if (optype == GWY_MARKER_OPERATION_MOVE) {
 *         markers = gwy_marker_box_get_markers(mbox);
 *         *pos = CLAMP(*pos, markers[*i - 1], markers[*i + 1]);
 *     }
 *     return TRUE;
 * }
 * </programlisting></informalexample>
 *
 * Returns: %TRUE to allow requested the operation, %FALSE to disallow it.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
