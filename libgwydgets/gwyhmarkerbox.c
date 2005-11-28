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
#include <libgwydgets/gwyhmarkerbox.h>

enum { HMARKER_BOX_HEIGHT = 8 };

enum {
    PROP_0,
    PROP_FLIPPED,
    PROP_SELECTED_MARKER,
    PROP_LAST
};

enum {
    MARKER_SELECTED,
    MARKER_MOVED,
    MARKER_ADDED,
    MARKER_REMOVED,
    LAST_SIGNAL
};

static void     gwy_hmarker_box_finalize         (GObject *object);
static void     gwy_hmarker_box_destroy          (GtkObject *object);
static void     gwy_hmarker_box_set_property     (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_hmarker_box_get_property     (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_hmarker_box_realize          (GtkWidget *widget);
static void     gwy_hmarker_box_unrealize        (GtkWidget *widget);
static void     gwy_hmarker_box_size_request     (GtkWidget *widget,
                                                  GtkRequisition *requisition);
static void     gwy_hmarker_box_size_allocate    (GtkWidget *widget,
                                                  GtkAllocation *allocation);
static void     gwy_hmarker_box_paint            (GwyHMarkerBox *hmbox);
static void     gwy_hmarker_box_draw_marker      (GtkWidget *widget,
                                                  gdouble pos,
                                                  gboolean selected,
                                                  gboolean ghost,
                                                  gboolean flipped);
static gint     gwy_hmarker_box_find_nearest     (GwyHMarkerBox *hmbox,
                                                  gint x,
                                                  gint y);
static gboolean gwy_hmarker_box_expose           (GtkWidget *widget,
                                                  GdkEventExpose *event);
static gboolean gwy_hmarker_box_button_press     (GtkWidget *widget,
                                                  GdkEventButton *event);
static gboolean gwy_hmarker_box_button_release   (GtkWidget *widget,
                                                  GdkEventButton *event);
static gboolean gwy_hmarker_box_motion_notify    (GtkWidget *widget,
                                                  GdkEventMotion *event);
static void     gwy_hmarker_box_state_changed    (GtkWidget *widget,
                                                  GtkStateType state);

static guint hmarker_box_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GwyHMarkerBox, gwy_hmarker_box, GTK_TYPE_WIDGET)

static void
gwy_hmarker_box_class_init(GwyHMarkerBoxClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    gobject_class->finalize = gwy_hmarker_box_finalize;
    gobject_class->set_property = gwy_hmarker_box_set_property;
    gobject_class->get_property = gwy_hmarker_box_get_property;

    object_class->destroy = gwy_hmarker_box_destroy;

    widget_class->realize = gwy_hmarker_box_realize;
    widget_class->expose_event = gwy_hmarker_box_expose;
    widget_class->size_request = gwy_hmarker_box_size_request;
    widget_class->unrealize = gwy_hmarker_box_unrealize;
    widget_class->size_allocate = gwy_hmarker_box_size_allocate;
    widget_class->button_press_event = gwy_hmarker_box_button_press;
    widget_class->button_release_event = gwy_hmarker_box_button_release;
    widget_class->motion_notify_event = gwy_hmarker_box_motion_notify;
    widget_class->state_changed = gwy_hmarker_box_state_changed;

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
        PROP_SELECTED_MARKER,
        g_param_spec_int("selected-marker",
                         "Selected marker",
                         "The index of selected marker, -1 if none.",
                         -1, 1024, -1,
                         G_PARAM_READWRITE));

    /**
     * GwyHMarkerBox::marker-selected:
     * @arg1: The index of selected marker, -1 when marker was unselected.
     * @gwydataview: The #GwyHMarkerBox which received the signal.
     *
     * The ::marker-selected signal is emitted when marker selection changes.
     **/
    hmarker_box_signals[MARKER_SELECTED]
        = g_signal_new("marker-selected",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyHMarkerBoxClass, marker_selected),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * GwyHMarkerBox::marker-moved:
     * @arg1: The index of moved marker.
     * @gwydataview: The #GwyHMarkerBox which received the signal.
     *
     * The ::marker-moved signal is emitted when a marker is moved.
     **/
    hmarker_box_signals[MARKER_MOVED]
        = g_signal_new("marker-moved",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyHMarkerBoxClass, marker_moved),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * GwyHMarkerBox::marker-added:
     * @arg1: The index a marker was added at.
     * @gwydataview: The #GwyHMarkerBox which received the signal.
     *
     * The ::marker-added signal is emitted when a marker is added.
     **/
    hmarker_box_signals[MARKER_ADDED]
        = g_signal_new("marker-added",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyHMarkerBoxClass, marker_added),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * GwyHMarkerBox::marker-removed:
     * @arg1: The index a marker was removed from.
     * @gwydataview: The #GwyHMarkerBox which received the signal.
     *
     * The ::marker-removed signal is emitted when a marker is removed.
     **/
    hmarker_box_signals[MARKER_REMOVED]
        = g_signal_new("marker-removed",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyHMarkerBoxClass, marker_removed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
gwy_hmarker_box_init(GwyHMarkerBox *hmbox)
{
    hmbox->markers = g_array_new(FALSE, FALSE, sizeof(gdouble));
    hmbox->selected = -1;
}

/**
 * gwy_hmarker_box_new:
 *
 * Creates a new horizontal marker box.
 *
 * Returns: The new horizontal marker box as a #GtkWidget.
 **/
GtkWidget*
gwy_hmarker_box_new(void)
{
    GwyHMarkerBox *hmbox;

    hmbox = (GwyHMarkerBox*)g_object_new(GWY_TYPE_HMARKER_BOX, NULL);

    return (GtkWidget*)hmbox;
}

static void
gwy_hmarker_box_finalize(GObject *object)
{
    GwyHMarkerBox *hmbox;

    hmbox = GWY_HMARKER_BOX(object);
    g_array_free(hmbox->markers, TRUE);

    G_OBJECT_CLASS(gwy_hmarker_box_parent_class)->finalize(object);
}

static void
gwy_hmarker_box_destroy(GtkObject *object)
{
    GwyHMarkerBox *hmbox;

    hmbox = GWY_HMARKER_BOX(object);

    GTK_OBJECT_CLASS(gwy_hmarker_box_parent_class)->destroy(object);
}

static void
gwy_hmarker_box_unrealize(GtkWidget *widget)
{
    GwyHMarkerBox *hmbox;

    hmbox = GWY_HMARKER_BOX(widget);

    if (GTK_WIDGET_CLASS(gwy_hmarker_box_parent_class)->unrealize)
        GTK_WIDGET_CLASS(gwy_hmarker_box_parent_class)->unrealize(widget);
}


static void
gwy_hmarker_box_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyHMarkerBox *hmbox = GWY_HMARKER_BOX(object);

    switch (prop_id) {
        case PROP_FLIPPED:
        gwy_hmarker_box_set_flipped(hmbox, g_value_get_boolean(value));
        break;

        case PROP_SELECTED_MARKER:
        gwy_hmarker_box_set_selected_marker(hmbox, g_value_get_int(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_hmarker_box_get_property(GObject*object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyHMarkerBox *hmbox = GWY_HMARKER_BOX(object);

    switch (prop_id) {
        case PROP_FLIPPED:
        g_value_set_boolean(value, hmbox->flipped);
        break;

        case PROP_SELECTED_MARKER:
        g_value_set_int(value, hmbox->selected);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_hmarker_box_realize(GtkWidget *widget)
{
    GwyHMarkerBox *hmbox;
    GdkWindowAttr attributes;
    gint attributes_mask;

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    hmbox = GWY_HMARKER_BOX(widget);

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
gwy_hmarker_box_size_request(G_GNUC_UNUSED GtkWidget *widget,
                             GtkRequisition *requisition)
{
    requisition->width = 2*HMARKER_BOX_HEIGHT;
    requisition->height = HMARKER_BOX_HEIGHT;
}

static void
gwy_hmarker_box_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GwyHMarkerBox *hmbox;

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(widget)) {
        hmbox = GWY_HMARKER_BOX(widget);

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
    }
}

static void
gwy_hmarker_box_paint(GwyHMarkerBox *hmbox)
{
    GtkWidget *widget;
    GtkStateType state, gcstate;
    gint height, width;
    gdouble pos;
    guint i;

    widget = GTK_WIDGET(hmbox);
    state = GTK_WIDGET_STATE(widget);

    width = widget->allocation.width;
    height = widget->allocation.height;

    if (state == GTK_STATE_INSENSITIVE)
        gcstate = state;
    else
        gcstate = GTK_STATE_NORMAL;
    gdk_draw_rectangle(widget->window, widget->style->bg_gc[gcstate],
                       TRUE, 0, 0, width, height);

    for (i = 0; i < hmbox->markers->len; i++) {
        if (i == hmbox->selected)
            continue;
        pos = g_array_index(hmbox->markers, gdouble, i);
        gwy_hmarker_box_draw_marker(widget, pos, FALSE, FALSE, hmbox->flipped);
    }
    /* Draw selected last, `over' other markers */
    if (hmbox->selected >= 0) {
        pos = g_array_index(hmbox->markers, gdouble, hmbox->selected);
        gwy_hmarker_box_draw_marker(widget, pos,
                                    TRUE, hmbox->ghost, hmbox->flipped);
    }
}

static void
gwy_hmarker_box_draw_marker(GtkWidget *widget,
                            gdouble pos,
                            gboolean selected,
                            gboolean ghost,
                            gboolean flipped)
{
    GtkStateType state, gcstate;
    GdkPoint points[3];
    gint height, width;
    gint iw, ipos;

    state = GTK_WIDGET_STATE(widget);

    width = widget->allocation.width;
    height = widget->allocation.height;

    if (ghost)
        gcstate = GTK_STATE_INSENSITIVE;
    else if (state == GTK_STATE_INSENSITIVE)
        gcstate = state;
    else
        gcstate = GTK_STATE_NORMAL;

    ipos = ROUND(pos*(width - 1));
    iw = MAX(ROUND(height/GWY_SQRT3 - 1), 1);
    points[0].x = ipos - iw;
    points[1].x = ipos + iw;
    points[2].x = ipos;
    if (flipped) {
        points[0].y = 0;
        points[1].y = 0;
        points[2].y = height-1;
    }
    else {
        points[0].y = height-1;
        points[1].y = height-1;
        points[2].y = 0;
    }
    if (selected && !ghost)
        gdk_draw_polygon(widget->window,
                         widget->style->bg_gc[GTK_STATE_SELECTED],
                         TRUE, points, G_N_ELEMENTS(points));
    else
        gdk_draw_polygon(widget->window, widget->style->text_gc[gcstate],
                         TRUE, points, G_N_ELEMENTS(points));

    gdk_draw_polygon(widget->window, widget->style->fg_gc[gcstate],
                     FALSE, points, G_N_ELEMENTS(points));
}

static gboolean
gwy_hmarker_box_expose(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventExpose *event)
{
    GwyHMarkerBox *hmbox;

    hmbox = GWY_HMARKER_BOX(widget);

    gwy_hmarker_box_paint(hmbox);

    return FALSE;
}

static gboolean
gwy_hmarker_box_button_press(GtkWidget *widget,
                             GdkEventButton *event)
{
    GwyHMarkerBox *hmbox;
    gint x, y, i, ipos, width;
    gdouble pos;

    /* React to left button only */
    hmbox = GWY_HMARKER_BOX(widget);
    if (event->button != 1)
        return FALSE;

    x = (gint)event->x;
    y = (gint)event->y;
    width = widget->allocation.width;

    i = gwy_hmarker_box_find_nearest(hmbox, x, y);
    if (i < 0) {
        /* Control enforces selection, no markers are added */
        if ((event->state & GDK_CONTROL_MASK))
            return FALSE;
        pos = (x - hmbox->offset)/(widget->allocation.width - 1.0);
        i = gwy_hmarker_box_add_marker(hmbox, hmbox->markers->len, pos);
        if (i < 0)
            return FALSE;
    }
    /* Control deselect a selected marker */
    if ((event->state & GDK_CONTROL_MASK) && i == hmbox->selected) {
        gwy_hmarker_box_set_selected_marker(hmbox, -1);
        return FALSE;
    }

    pos = g_array_index(hmbox->markers, gdouble, i);
    ipos = ROUND(pos*(width - 1));
    hmbox->button = event->button;
    hmbox->offset = x - ipos;
    hmbox->moved = FALSE;
    gwy_hmarker_box_set_selected_marker(hmbox, i);

    return FALSE;
}

static gboolean
gwy_hmarker_box_button_release(GtkWidget *widget,
                               GdkEventButton *event)
{
    GwyHMarkerBox *hmbox;
    gdouble pos;
    gboolean ghost;
    gint x, y;

    /* React to left button only */
    hmbox = GWY_HMARKER_BOX(widget);
    if (event->button != 1 || !hmbox->button)
        return FALSE;

    x = (gint)event->x;
    y = (gint)event->y;
    ghost = (y > 3*widget->allocation.height/2 + 2
             || y < -widget->allocation.height/2 - 2);

    hmbox->ghost = FALSE;
    hmbox->button = 0;

    if (ghost) {
        if (gwy_hmarker_box_remove_marker(hmbox, hmbox->selected))
            return FALSE;
    }
    if (!hmbox->moved)
        return FALSE;

    pos = (x - hmbox->offset)/(widget->allocation.width - 1.0);
    gwy_hmarker_box_set_marker_position(hmbox, hmbox->selected,
                                        CLAMP(pos, 0.0, 1.0));

    return FALSE;
}

static gboolean
gwy_hmarker_box_motion_notify(GtkWidget *widget,
                              GdkEventMotion *event)
{
    GwyHMarkerBox *hmbox;
    gboolean ghost;
    gdouble pos;
    gint x, y, j;

    gwy_debug("motion event: (%f, %f)", event->x, event->y);

    hmbox = GWY_HMARKER_BOX(widget);
    if (!hmbox->button)
        return FALSE;

    if (event->is_hint)
        gdk_window_get_pointer(widget->window, &x, &y, NULL);
    else {
        x = (gint)event->x;
        y = (gint)event->y;
    }

    pos = (x - hmbox->offset)/(widget->allocation.width - 1.0);
    if (gwy_hmarker_box_set_marker_position(hmbox, hmbox->selected,
                                            CLAMP(pos, 0.0, 1.0)))
        hmbox->moved = TRUE;

    ghost = (y > 3*widget->allocation.height/2 + 2
             || y < -widget->allocation.height/2 - 2);
    if (ghost && hmbox->validate) {
        j = hmbox->selected;
        if (!hmbox->validate(hmbox, GWY_MARKER_OPERATION_REMOVE, &j, &pos))
            ghost = FALSE;
    }
    if (ghost != hmbox->ghost
        && GTK_WIDGET_REALIZED(widget)) {
        hmbox->ghost = ghost;
        gtk_widget_queue_draw(GTK_WIDGET(hmbox));
    }

    return FALSE;
}

static void
gwy_hmarker_box_state_changed(GtkWidget *widget,
                              GtkStateType state)
{
    GtkStateType oldstate;

    oldstate = GTK_WIDGET_STATE(widget);
    gwy_debug("state: %d, current: %d", state, oldstate);
    if (state == GTK_STATE_INSENSITIVE
        || oldstate == GTK_STATE_INSENSITIVE)
        gtk_widget_queue_draw(widget);

    if (GTK_WIDGET_CLASS(gwy_hmarker_box_parent_class)->state_changed)
        GTK_WIDGET_CLASS(gwy_hmarker_box_parent_class)->state_changed(widget,
                                                                      state);
}

static gint
gwy_hmarker_box_find_nearest(GwyHMarkerBox *hmbox,
                             gint x,
                             gint y)
{
    GtkWidget *widget;
    gint ii, mdist, width, height, ipos;
    gdouble pos;
    guint i;

    widget = GTK_WIDGET(hmbox);
    width = widget->allocation.width;
    height = widget->allocation.height;

    ii = -1;
    mdist = G_MAXINT;
    for (i = 0; i < hmbox->markers->len; i++) {
        pos = g_array_index(hmbox->markers, gdouble, i);
        ipos = ROUND(pos*(width - 1));
        if (ipos == x)
            return i;

        if (ABS(ipos - x) < mdist) {
            mdist = ABS(ipos - x);
            ii = i;
        }
        else if (ipos > x)
            break;
    }
    if (ii < 0)
        return -1;

    pos = g_array_index(hmbox->markers, gdouble, ii);
    ipos = ROUND(pos*(width - 1));
    if (hmbox->flipped)
        y = height-1 - y;

    return ABS(ipos - x) <= y/GWY_SQRT3 ? ii : -1;
}

/**
 * gwy_hmarker_box_get_selected_marker:
 * @hmbox: A horizontal marker box.
 *
 * Gets the index of the currently selected marker in a horizontal marker
 * box.
 *
 * Returns: The index of currently selected marker, -1 when none is
 *          selected.
 **/
gint
gwy_hmarker_box_get_selected_marker(GwyHMarkerBox *hmbox)
{
    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), -1);

    return hmbox->selected;
}

/**
 * gwy_hmarker_box_set_selected_marker:
 * @hmbox: A horizontal marker box.
 * @i: The index of marker to select.  Pass -1 to unselect.
 *
 * Selects a marker in a horizontal marker box.
 **/
void
gwy_hmarker_box_set_selected_marker(GwyHMarkerBox *hmbox,
                                    gint i)
{
    g_return_if_fail(GWY_IS_HMARKER_BOX(hmbox));
    g_return_if_fail(i < (gint)hmbox->markers->len);

    /* Negative value means no selection */
    if (i < 0)
        i = -1;

    if (i == hmbox->selected)
        return;

    hmbox->selected = i;
    if (GTK_WIDGET_REALIZED(hmbox))
        gtk_widget_queue_draw(GTK_WIDGET(hmbox));
    g_signal_emit(hmbox, hmarker_box_signals[MARKER_SELECTED], 0, i);
}

/**
 * gwy_hmarker_box_get_marker_position:
 * @hmbox: A horizontal marker box.
 * @i: The index of marker to get position of.
 *
 * Gets the position of a marker in a horizontal marker box.
 *
 * Returns: The marker position, in the range [0.0, 1.0].
 **/
gdouble
gwy_hmarker_box_get_marker_position(GwyHMarkerBox *hmbox,
                                    gint i)
{
    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), 0.0);
    g_return_val_if_fail(i >= 0 && i < hmbox->markers->len, 0.0);

    return g_array_index(hmbox->markers, gdouble, i);
}

/**
 * gwy_hmarker_box_set_marker_position:
 * @hmbox: A horizontal marker box.
 * @i: Index of marker to move.
 * @pos: The new marker position, in the range [0.0, 1.0].
 *
 * Moves a marker in a horizontal marker box.
 *
 * Returns: %TRUE on success. If the move does not validate, %FALSE is returned
 *          and the marker position does not change.
 **/
gboolean
gwy_hmarker_box_set_marker_position(GwyHMarkerBox *hmbox,
                                    gint i,
                                    gdouble pos)
{
    gdouble currpos;
    gboolean needs_redraw;
    gint j, width;

    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), FALSE);
    g_return_val_if_fail(i >= 0 && i < hmbox->markers->len, FALSE);

    currpos = g_array_index(hmbox->markers, gdouble, i);
    j = i;
    if ((hmbox->validate
         && !hmbox->validate(hmbox, GWY_MARKER_OPERATION_MOVE, &j, &pos))
        || pos == currpos)
        return FALSE;

    width = GTK_WIDGET(hmbox)->allocation.width;
    needs_redraw = (ROUND(pos*(width - 1)) != ROUND(currpos*(width - 1)));

    g_array_index(hmbox->markers, gdouble, i) = pos;
    if (needs_redraw && GTK_WIDGET_REALIZED(hmbox))
        gtk_widget_queue_draw(GTK_WIDGET(hmbox));
    g_signal_emit(hmbox, hmarker_box_signals[MARKER_MOVED], 0, i);

    return TRUE;
}

/**
 * gwy_hmarker_box_add_marker:
 * @hmbox: A horizontal marker box.
 * @i: Index to insert marker at.
 * @pos: Position to insert marker to, in the range [0.0, 1.0].
 *
 * Adds a marker to a horizontal marker box.
 *
 * Returns: On success, the index the marker was added at.  If the insertion
 *          does not validate, -1 is returned and no marker is added.
 **/
gint
gwy_hmarker_box_add_marker(GwyHMarkerBox *hmbox,
                           gint i,
                           gdouble pos)
{
    gboolean selection_changed = FALSE;

    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), -1);
    g_return_val_if_fail(i >= 0 && i <= hmbox->markers->len, -1);

    if (pos < 0.0 || pos > 1.0)
        return -1;

    if (hmbox->validate
        && !hmbox->validate(hmbox, GWY_MARKER_OPERATION_ADD, &i, &pos))
        return -1;

    if (i == hmbox->markers->len)
        g_array_append_val(hmbox->markers, pos);
    else
        g_array_insert_val(hmbox->markers, i, pos);

    if (i <= hmbox->selected) {
        hmbox->selected++;
        selection_changed = TRUE;
    }

    if (GTK_WIDGET_REALIZED(hmbox))
        gtk_widget_queue_draw(GTK_WIDGET(hmbox));

    g_signal_emit(hmbox, hmarker_box_signals[MARKER_ADDED], 0, i);
    if (selection_changed)
        g_signal_emit(hmbox, hmarker_box_signals[MARKER_SELECTED], 0,
                      hmbox->selected);

    return i;
}

/**
 * gwy_hmarker_box_remove_marker:
 * @hmbox: A horizontal marker box.
 * @i: Index of marker to remove.
 *
 * Removes a marker from a horizontal marker box.
 *
 * Returns: %TRUE on success. If the removal does not validate, %FALSE is
 *          returned and the marker is kept.
 **/
gboolean
gwy_hmarker_box_remove_marker(GwyHMarkerBox *hmbox,
                              gint i)
{
    gboolean selection_changed = FALSE;
    gdouble pos;
    gint j;

    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), FALSE);
    g_return_val_if_fail(i >= 0 && i < hmbox->markers->len, FALSE);

    pos = g_array_index(hmbox->markers, gdouble, i);
    j = i;
    if (hmbox->validate
        && !hmbox->validate(hmbox, GWY_MARKER_OPERATION_REMOVE, &j, &pos))
        return FALSE;

    if (i == hmbox->selected)
        gwy_hmarker_box_set_selected_marker(hmbox, -1);

    g_array_remove_index(hmbox->markers, i);
    if (i < hmbox->selected) {
        hmbox->selected--;
        selection_changed = TRUE;
    }

    if (GTK_WIDGET_REALIZED(hmbox))
        gtk_widget_queue_draw(GTK_WIDGET(hmbox));

    g_signal_emit(hmbox, hmarker_box_signals[MARKER_REMOVED], 0, i);
    if (selection_changed)
        g_signal_emit(hmbox, hmarker_box_signals[MARKER_SELECTED], 0,
                      hmbox->selected);

    return TRUE;
}

/**
 * gwy_hmarker_box_get_nmarkers:
 * @hmbox: A horizontal marker box.
 *
 * Gets the number of markers in a horizontal marker box.
 *
 * Returns: The number of markers.
 **/
gint
gwy_hmarker_box_get_nmarkers(GwyHMarkerBox *hmbox)
{
    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), 0);

    return hmbox->markers->len;
}

/**
 * gwy_hmarker_box_get_markers:
 * @hmbox: A horizontal marker box.
 *
 * Gets all markers in a horizontal marker box.
 *
 * Returns: The markers as an array of positions, owned by @hmbox.  It must
 *          not be modified nor freed by caller and it's valid only until
 *          next marker change.
 **/
const gdouble*
gwy_hmarker_box_get_markers(GwyHMarkerBox *hmbox)
{
    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), NULL);

    return (gdouble*)hmbox->markers->data;
}

/**
 * gwy_hmarker_box_set_markers:
 * @hmbox: A horizontal marker box.
 * @n: The number of markers to set.  If it's zero, @markers can be %NULL.
 * @markers: Markers position.
 *
 * Sets positions of all markers.
 *
 * No validation is performed, even if validator is set.
 **/
void
gwy_hmarker_box_set_markers(GwyHMarkerBox *hmbox,
                            gint n,
                            const gdouble *markers)
{
    gint i;

    g_return_if_fail(GWY_IS_HMARKER_BOX(hmbox));
    g_return_if_fail(n >= 0);
    g_return_if_fail(!n || markers);

    if (n == hmbox->markers->len) {
        for (i = 0; i < n; i++) {
            if (g_array_index(hmbox->markers, gdouble, i) == markers[i])
                break;
        }
        if (i == n)
            return;
    }

    gwy_hmarker_box_set_selected_marker(hmbox, -1);
    g_array_set_size(hmbox->markers, 0);
    g_array_append_vals(hmbox->markers, markers, n);

    if (GTK_WIDGET_REALIZED(hmbox))
        gtk_widget_queue_draw(GTK_WIDGET(hmbox));
}

/**
 * gwy_hmarker_box_set_flipped:
 * @hmbox: A horizontal marker box.
 * @flipped: %TRUE to draw markers upside down.
 *
 * Sets whether a horizontal marker box is drawn upside down.
 **/
void
gwy_hmarker_box_set_flipped(GwyHMarkerBox *hmbox,
                            gboolean flipped)
{
    g_return_if_fail(GWY_IS_HMARKER_BOX(hmbox));

    flipped = !!flipped;
    if (flipped == hmbox->flipped)
        return;

    hmbox->flipped = flipped;
    if (GTK_WIDGET_REALIZED(hmbox))
        gtk_widget_queue_draw(GTK_WIDGET(hmbox));
    g_object_notify(G_OBJECT(hmbox), "flipped");
}

/**
 * gwy_hmarker_box_get_flipped:
 * @hmbox: A horizontal marker box.
 *
 * Returns whether a horizontal marker box is drawn upside down.
 *
 * Returns: %TRUE if markers are drawn upside down.
 **/
gboolean
gwy_hmarker_box_get_flipped(GwyHMarkerBox *hmbox)
{
    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), FALSE);

    return hmbox->flipped;
}

/**
 * gwy_hmarker_box_set_validator:
 * @hmbox: A horizontal marker box.
 * @validate: Marker validation function.  Pass %NULL to disable validation.
 *
 * Sets marker validation function.
 **/
void
gwy_hmarker_box_set_validator(GwyHMarkerBox *hmbox,
                              GwyMarkerValidateFunc validate)
{
    g_return_if_fail(GWY_IS_HMARKER_BOX(hmbox));

    if (validate == hmbox->validate)
        return;

    hmbox->validate = validate;
}

/**
 * gwy_hmarker_box_get_validator:
 * @hmbox: A horizontal marker box.
 *
 * Gets the marker validation function currently in use.
 *
 * Returns: The marker validation function.
 **/
GwyMarkerValidateFunc
gwy_hmarker_box_get_validator(GwyHMarkerBox *hmbox)
{
    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), NULL);

    return hmbox->validate;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyhmarkerbox
 * @title: GwyHMarkerBox
 * @short_description: A box with movable horizontal markers.
 *
 * #GwyHMarkerBox is a horizontal box with triangular markers that can be
 * moved, added, and/or deleted.  One marker is always selected, unless no
 * marker is selected.
 *
 * Marker coordinates are always from the range [0.0, 1.0].
 *
 * It is possible to fully control where and how user can move, add, and/or
 * delete markers with marker validation function set with
 * gwy_hmarker_box_set_validator().  By default, no validator is in effect
 * and user can change markers completely freely.
 *
 * There is no associated #GwyVMarkerBox widget and #GwyMarkerBox base class,
 * but it probably should be.
 **/

/**
 * GwyMarkerValidateFunc:
 * @hmbox: The #GwyHMarkerBox to validate markers for.
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
 * #GwyHMarkerBox methods.  However, it is NOT called upon
 * gwy_hmarker_box_set_markers() as it is unclear how the validation should
 * proceed.
 *
 * The function must not have any side-effects, that is it must not assume the
 * operation will be actually performed when it returns %TRUE.
 *
 * Marker validation that assures markers are sorted and there is always
 * a marker at 0.0 and another at 1.0 could look:
 * <informalexample><programlisting>
 * static gboolean
 * validate_marker(GwyHMarkerBox *hmbox,
 *                 GwyMarkerOperationType optype,
 *                 gint *i,
 *                 gdouble *pos)
 * {
 *     const gdouble *markers;
 *     gint j, n;
 *     <!-- Hello, gtk-doc! -->
 *     n = gwy_hmarker_box_get_nmarkers(hmbox);
 *     <!-- Hello, gtk-doc! -->
 *     /<!-- -->* Insertions are sorted *<!-- -->/
 *     if (optype == GWY_MARKER_OPERATION_ADD) {
 *         markers = gwy_hmarker_box_get_markers(hmbox);
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
 *         markers = gwy_hmarker_box_get_markers(hmbox);
 *         *pos = CLAMP(*pos, markers[*i - 1], markers[*i + 1]);
 *     }
 *     return TRUE;
 * }
 * </programlisting></informalexample>
 *
 * Returns: %TRUE to allow requested the operation, %FALSE to disallow it.
 **/

/**
 * GwyMarkerOperationType:
 * @GWY_MARKER_OPERATION_MOVE: Marker is moved.
 * @GWY_MARKER_OPERATION_ADD: Marker is added.
 * @GWY_MARKER_OPERATION_REMOVE: Marker is removed.
 *
 * Marker operation type (for validation).
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
