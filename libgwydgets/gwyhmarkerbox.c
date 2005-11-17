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
#define DEBUG 1
#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwyhmarkerbox.h>

enum { HMARKER_BOX_HEIGHT = 8 };

enum {
    PROP_0,
    PROP_FLIPPED,
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

    hmarker_box_signals[MARKER_SELECTED]
        = g_signal_new("marker-selected",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyHMarkerBoxClass, marker_selected),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

    hmarker_box_signals[MARKER_MOVED]
        = g_signal_new("marker-moved",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyHMarkerBoxClass, marker_moved),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

    hmarker_box_signals[MARKER_ADDED]
        = g_signal_new("marker-added",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyHMarkerBoxClass, marker_added),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

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
        gwy_hmarker_box_draw_marker(widget, pos, FALSE, hmbox->flipped);
    }
    /* Draw selected last, `over' other markers */
    if (hmbox->selected >= 0) {
        pos = g_array_index(hmbox->markers, gdouble, hmbox->selected);
        gwy_hmarker_box_draw_marker(widget, pos, TRUE, hmbox->flipped);
    }
}

static void
gwy_hmarker_box_draw_marker(GtkWidget *widget,
                            gdouble pos,
                            gboolean selected,
                            gboolean flipped)
{
    GtkStateType state, gcstate;
    GdkPoint points[3];
    gint height, width;
    gint iw, ipos;

    state = GTK_WIDGET_STATE(widget);

    width = widget->allocation.width;
    height = widget->allocation.height;

    if (state == GTK_STATE_INSENSITIVE)
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
    if (selected)
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
    if ((event->state & GDK_SHIFT_MASK)) {
        pos = (x - hmbox->offset)/(widget->allocation.width - 1.0);
        i = gwy_hmarker_box_add_marker(hmbox, pos);
    }
    else
        i = gwy_hmarker_box_find_nearest(hmbox, x, y);

    if (i < 0)
        return FALSE;

    if ((event->state & GDK_CONTROL_MASK)) {
        gwy_hmarker_box_remove_marker(hmbox, i);
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
    gint x, y;

    /* React to left button only */
    hmbox = GWY_HMARKER_BOX(widget);
    if (event->button != 1 || !hmbox->button)
        return FALSE;

    x = (gint)event->x;
    y = (gint)event->y;

    hmbox->button = 0;
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
    gdouble pos;
    gint x, y;

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

gint
gwy_hmarker_box_get_selected_marker(GwyHMarkerBox *hmbox)
{
    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), -1);

    return hmbox->selected;
}

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
 * @pos: The new marker position.
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
    gint width;

    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), FALSE);
    g_return_val_if_fail(i >= 0 && i < hmbox->markers->len, FALSE);

    currpos = g_array_index(hmbox->markers, gdouble, i);
    if ((hmbox->validate
         && !hmbox->validate(hmbox, GWY_MARKER_OPERATION_MOVE, i, &pos))
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
 * @pos: Position to insert marker to.
 *
 * Adds a marker to a horizontal marker box.
 *
 * Returns: On success, the index the marker was added at.  If the insertion
 *          does not validate, -1 is returned and no marker is added.
 **/
gint
gwy_hmarker_box_add_marker(GwyHMarkerBox *hmbox,
                           gdouble pos)
{
    gboolean selection_changed = FALSE;
    gint i;

    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), -1);

    if (pos < 0.0 || pos > 1.0)
        return -1;

    for (i = 0; i < hmbox->markers->len; i++) {
        if (pos < g_array_index(hmbox->markers, gdouble, i))
            break;
    }
    gwy_debug("%f %d", pos, i);
    if (hmbox->validate
        && !hmbox->validate(hmbox, GWY_MARKER_OPERATION_ADD, i, &pos))
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

    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), FALSE);
    g_return_val_if_fail(i >= 0 && i < hmbox->markers->len, FALSE);

    pos = g_array_index(hmbox->markers, gdouble, i);
    if (hmbox->validate
        && !hmbox->validate(hmbox, GWY_MARKER_OPERATION_REMOVE, i, &pos))
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

gint
gwy_hmarker_box_get_nmarkers(GwyHMarkerBox *hmbox)
{
    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), 0);

    return hmbox->markers->len;
}

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

gboolean
gwy_hmarker_box_get_flipped(GwyHMarkerBox *hmbox)
{
    g_return_val_if_fail(GWY_IS_HMARKER_BOX(hmbox), FALSE);

    return hmbox->flipped;
}

void
gwy_hmarker_box_set_validator(GwyHMarkerBox *hmbox,
                              GwyMarkerValidateFunc validate)
{
    g_return_if_fail(GWY_IS_HMARKER_BOX(hmbox));

    if (validate == hmbox->validate)
        return;

    hmbox->validate = validate;
}

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
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
