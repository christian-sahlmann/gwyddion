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

static void     gwy_hmarker_box_draw_box         (GwyMarkerBox *mbox);
static void     gwy_hmarker_box_draw_marker      (GwyMarkerBox *mbox,
                                                  gint i);
static gint     gwy_hmarker_box_find_nearest     (GwyMarkerBox *hmbox,
                                                  gint x,
                                                  gint y);
static gboolean gwy_hmarker_box_button_press     (GtkWidget *widget,
                                                  GdkEventButton *event);
static gboolean gwy_hmarker_box_button_release   (GtkWidget *widget,
                                                  GdkEventButton *event);
static gboolean gwy_hmarker_box_motion_notify    (GtkWidget *widget,
                                                  GdkEventMotion *event);

G_DEFINE_TYPE(GwyHMarkerBox, gwy_hmarker_box, GWY_TYPE_MARKER_BOX)

static void
gwy_hmarker_box_class_init(GwyHMarkerBoxClass *klass)
{
    GwyMarkerBoxClass *mbox_class;
    GtkWidgetClass *widget_class;

    mbox_class = (GwyMarkerBoxClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    widget_class->button_press_event = gwy_hmarker_box_button_press;
    widget_class->button_release_event = gwy_hmarker_box_button_release;
    widget_class->motion_notify_event = gwy_hmarker_box_motion_notify;

    mbox_class->draw_box = gwy_hmarker_box_draw_box;
    mbox_class->draw_marker = gwy_hmarker_box_draw_marker;
}

static void
gwy_hmarker_box_init(G_GNUC_UNUSED GwyHMarkerBox *hmbox)
{
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
gwy_hmarker_box_draw_box(GwyMarkerBox *mbox)
{
    GtkWidget *widget;
    GtkStateType state, gcstate;
    gint height, width;

    widget = GTK_WIDGET(mbox);
    state = GTK_WIDGET_STATE(widget);

    width = widget->allocation.width;
    height = widget->allocation.height;

    if (state == GTK_STATE_INSENSITIVE)
        gcstate = state;
    else
        gcstate = GTK_STATE_NORMAL;
    gdk_draw_rectangle(widget->window, widget->style->bg_gc[gcstate],
                       TRUE, 0, 0, width, height);
}

static void
gwy_hmarker_box_draw_marker(GwyMarkerBox *mbox,
                            gint i)
{
    GtkWidget *widget;
    GtkStateType state, gcstate;
    gboolean ghost, selected;
    GdkPoint points[3];
    gint height, width;
    gdouble pos;
    gint iw, ipos;

    widget = GTK_WIDGET(mbox);
    state = GTK_WIDGET_STATE(widget);
    width = widget->allocation.width;
    height = widget->allocation.height;

    selected = (i == mbox->selected);
    ghost = selected && mbox->ghost;
    selected = selected && mbox->highlight;
    pos = g_array_index(mbox->markers, gdouble, i);

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
    if (mbox->flipped) {
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
gwy_hmarker_box_button_press(GtkWidget *widget,
                             GdkEventButton *event)
{
    GwyMarkerBox *mbox;
    gint x, y, i, ipos, width;
    gdouble pos;

    /* React to left button only */
    mbox = GWY_MARKER_BOX(widget);
    if (event->button != 1)
        return FALSE;

    x = (gint)event->x;
    y = (gint)event->y;
    width = widget->allocation.width;

    i = gwy_hmarker_box_find_nearest(mbox, x, y);
    if (i < 0) {
        /* Control enforces selection, no markers are added */
        if ((event->state & GDK_CONTROL_MASK))
            return FALSE;
        pos = (x - mbox->offset)/(widget->allocation.width - 1.0);
        i = gwy_marker_box_add_marker(mbox, mbox->markers->len, pos);
        if (i < 0)
            return FALSE;
    }
    /* Control deselect a selected marker */
    if ((event->state & GDK_CONTROL_MASK) && i == mbox->selected) {
        gwy_marker_box_set_selected_marker(mbox, -1);
        return FALSE;
    }

    pos = g_array_index(mbox->markers, gdouble, i);
    ipos = ROUND(pos*(width - 1));
    mbox->button = event->button;
    mbox->offset = x - ipos;
    mbox->moved = FALSE;
    gwy_marker_box_set_selected_marker(mbox, i);

    return FALSE;
}

static gboolean
gwy_hmarker_box_button_release(GtkWidget *widget,
                               GdkEventButton *event)
{
    GwyMarkerBox *mbox;
    gdouble pos;
    gboolean ghost;
    gint x, y;

    /* React to left button only */
    mbox = GWY_MARKER_BOX(widget);
    if (event->button != 1 || !mbox->button)
        return FALSE;

    x = (gint)event->x;
    y = (gint)event->y;
    ghost = (y > 3*widget->allocation.height/2 + 2
             || y < -widget->allocation.height/2 - 2);

    mbox->ghost = FALSE;
    mbox->button = 0;

    if (ghost) {
        if (gwy_marker_box_remove_marker(mbox, mbox->selected))
            return FALSE;
    }
    if (!mbox->moved)
        return FALSE;

    pos = (x - mbox->offset)/(widget->allocation.width - 1.0);
    gwy_marker_box_set_marker_position(mbox, mbox->selected,
                                       CLAMP(pos, 0.0, 1.0));

    return FALSE;
}

static gboolean
gwy_hmarker_box_motion_notify(GtkWidget *widget,
                              GdkEventMotion *event)
{
    GwyMarkerBox *mbox;
    gboolean ghost;
    gdouble pos;
    gint x, y, j;

    gwy_debug("motion event: (%f, %f)", event->x, event->y);

    mbox = GWY_MARKER_BOX(widget);
    if (!mbox->button)
        return FALSE;

    if (event->is_hint)
        gdk_window_get_pointer(widget->window, &x, &y, NULL);
    else {
        x = (gint)event->x;
        y = (gint)event->y;
    }

    pos = (x - mbox->offset)/(widget->allocation.width - 1.0);
    if (gwy_marker_box_set_marker_position(mbox, mbox->selected,
                                           CLAMP(pos, 0.0, 1.0)))
        mbox->moved = TRUE;

    ghost = (y > 3*widget->allocation.height/2 + 2
             || y < -widget->allocation.height/2 - 2);
    if (ghost && mbox->validate) {
        j = mbox->selected;
        if (!mbox->validate(mbox, GWY_MARKER_OPERATION_REMOVE, &j, &pos))
            ghost = FALSE;
    }
    if (ghost != mbox->ghost
        && GTK_WIDGET_REALIZED(widget)) {
        mbox->ghost = ghost;
        gtk_widget_queue_draw(GTK_WIDGET(mbox));
    }

    return FALSE;
}

static gint
gwy_hmarker_box_find_nearest(GwyMarkerBox *mbox,
                             gint x,
                             gint y)
{
    GtkWidget *widget;
    gint ii, mdist, width, height, ipos;
    gdouble pos;
    guint i;

    widget = GTK_WIDGET(mbox);
    width = widget->allocation.width;
    height = widget->allocation.height;

    ii = -1;
    mdist = G_MAXINT;
    for (i = 0; i < mbox->markers->len; i++) {
        pos = g_array_index(mbox->markers, gdouble, i);
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

    pos = g_array_index(mbox->markers, gdouble, ii);
    ipos = ROUND(pos*(width - 1));
    if (mbox->flipped)
        y = height-1 - y;

    return ABS(ipos - x) <= y/GWY_SQRT3 ? ii : -1;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyhmarkerbox
 * @title: GwyHMarkerBox
 * @short_description: A box with movable horizontal markers.
 *
 * #GwyHMarkerBox is a horizontal marker box, use the #GwyMarkerBox interface
 * to control it.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
