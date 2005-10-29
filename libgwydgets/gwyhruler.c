/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Modified by Yeti 2003.  In fact, rewritten, except the skeleton
 * and a few drawing functions.  Rewritten again in 2005 to minimize
 * code duplication.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwyhruler.h"

#define RULER_HEIGHT 20

static gboolean gwy_hruler_motion_notify(GtkWidget *widget,
                                         GdkEventMotion *event);
static void     gwy_hruler_prepare_sizes(GwyRuler *ruler);
static void     gwy_hruler_draw_frame   (GwyRuler *ruler);
static void     gwy_hruler_draw_layout  (GwyRuler *ruler,
                                         gint hpos,
                                         gint vpos);
static void     gwy_hruler_draw_tick    (GwyRuler *ruler,
                                         gint pos,
                                         gint length);
static void     gwy_hruler_draw_pos     (GwyRuler *ruler);

G_DEFINE_TYPE(GwyHRuler, gwy_hruler, GWY_TYPE_RULER)

static void
gwy_hruler_class_init(GwyHRulerClass *klass)
{
    GtkWidgetClass *widget_class;
    GwyRulerClass *ruler_class;

    widget_class = (GtkWidgetClass*) klass;
    ruler_class = (GwyRulerClass*) klass;

    widget_class->motion_notify_event = gwy_hruler_motion_notify;

    ruler_class->prepare_sizes = gwy_hruler_prepare_sizes;
    ruler_class->draw_frame = gwy_hruler_draw_frame;
    ruler_class->draw_layout = gwy_hruler_draw_layout;
    ruler_class->draw_tick = gwy_hruler_draw_tick;
    ruler_class->draw_pos = gwy_hruler_draw_pos;
}

static void
gwy_hruler_init(GwyHRuler *hruler)
{
    GtkWidget *widget;

    widget = GTK_WIDGET(hruler);
    widget->requisition.width = widget->style->xthickness*2 + 1;
    widget->requisition.height = widget->style->ythickness*2 + RULER_HEIGHT;
}

/**
 * gwy_hruler_new:
 *
 * Creates a new #GwyHRuler.
 *
 * Returns: The new ruler as a #GtkWidget.
 **/
GtkWidget*
gwy_hruler_new(void)
{
    return g_object_new(GWY_TYPE_HRULER, NULL);
}

static gboolean
gwy_hruler_motion_notify(GtkWidget      *widget,
                         GdkEventMotion *event)
{
    GwyRuler *ruler;
    gint x;

    ruler = GWY_RULER(widget);

    if (event->is_hint)
        gdk_window_get_pointer(widget->window, &x, NULL, NULL);
    else
        x = event->x;

    ruler->position = ruler->lower + ((ruler->upper - ruler->lower) * x)
                                     / widget->allocation.width;
    g_object_notify(G_OBJECT(ruler), "position");

    /*  Make sure the ruler has been allocated already  */
    if (ruler->backing_store != NULL)
        gwy_ruler_draw_pos(ruler);

    return FALSE;
}

static void
gwy_hruler_prepare_sizes(GwyRuler *ruler)
{
    GtkWidget *widget;

    widget = GTK_WIDGET(ruler);
    ruler->hthickness = widget->style->xthickness;
    ruler->vthickness = widget->style->ythickness;
    ruler->height = widget->allocation.height - 2*ruler->vthickness;
    ruler->pixelsize = widget->allocation.width;
}

static void
gwy_hruler_draw_frame(GwyRuler *ruler)
{
    GdkGC *gc;
    GtkWidget *widget;

    widget = GTK_WIDGET(ruler);
    gc = widget->style->fg_gc[GTK_STATE_NORMAL];
    gdk_draw_line(ruler->backing_store, gc,
                  ruler->hthickness,
                  ruler->pixelsize + ruler->vthickness,
                  ruler->pixelsize - ruler->hthickness,
                  ruler->pixelsize + ruler->vthickness);
}

static void
gwy_hruler_draw_layout(GwyRuler *ruler,
                       gint hpos,
                       gint vpos)
{
    GtkWidget *widget;

    widget = GTK_WIDGET(ruler);
    gtk_paint_layout(widget->style,
                     ruler->backing_store,
                     GTK_WIDGET_STATE(widget),
                     FALSE,
                     NULL,
                     widget,
                     "hruler",
                     hpos + 3, vpos,
                     ruler->layout);
}

static void
gwy_hruler_draw_tick(GwyRuler *ruler,
                     gint pos,
                     gint length)
{
    GdkGC *gc;
    GtkWidget *widget;

    widget = GTK_WIDGET(ruler);
    gc = widget->style->fg_gc[GTK_STATE_NORMAL];
    gdk_draw_line(ruler->backing_store, gc,
                  pos, ruler->height + ruler->vthickness,
                  pos, ruler->height - length + ruler->vthickness);
}


static void
gwy_hruler_draw_pos(GwyRuler *ruler)
{
    GtkWidget *widget;
    GdkGC *gc;
    int i;
    gint x, y;
    gint bs_width, bs_height;
    gdouble increment;

    if (GTK_WIDGET_DRAWABLE(ruler)) {
        widget = GTK_WIDGET(ruler);
        gc = widget->style->fg_gc[GTK_STATE_NORMAL];

        bs_width = ruler->height/2;
        bs_width |= 1;  /* make sure it's odd */
        bs_height = bs_width/2 + 1;

        if ((bs_width > 0) && (bs_height > 0)) {
            /*  If a backing store exists, restore the ruler  */
            if (ruler->backing_store && ruler->non_gr_exp_gc)
                gdk_draw_drawable(ruler->widget.window,
                                  ruler->non_gr_exp_gc,
                                  ruler->backing_store,
                                  ruler->xsrc, ruler->ysrc,
                                  ruler->xsrc, ruler->ysrc,
                                  bs_width, bs_height);

            increment = (gdouble)ruler->pixelsize/(ruler->upper - ruler->lower);

            x = ROUND((ruler->position - ruler->lower) * increment)
                + (ruler->hthickness - bs_width)/2 - 1;
            y = (ruler->height + bs_height)/2 + ruler->vthickness;

            for (i = 0; i < bs_height; i++)
                gdk_draw_line(widget->window, gc,
                              x + i, y + i,
                              x + bs_width - 1 - i, y + i);

            ruler->xsrc = x;
            ruler->ysrc = y;
        }
    }
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyhruler
 * @title: GwyHRuler
 * @short_description: Horizontal ruler, similar to #GtkRuler
 * @see_also: #GwyVRuler -- vertical ruler
 *
 * Please see #GwyRuler for differences from #GtkHRuler.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
