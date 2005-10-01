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
#include "gwyvruler.h"

#define RULER_WIDTH 20

static void     gwy_vruler_realize      (GtkWidget *widget);
static gboolean gwy_vruler_motion_notify(GtkWidget *widget,
                                         GdkEventMotion *event);
static void     gwy_vruler_prepare_sizes(GwyRuler *ruler);
static void     gwy_vruler_draw_frame   (GwyRuler *ruler);
static void     gwy_vruler_draw_layout  (GwyRuler *ruler,
                                         gint hpos,
                                         gint vpos);
static void     gwy_vruler_draw_tick    (GwyRuler *ruler,
                                         gint pos,
                                         gint length);
static void     gwy_vruler_draw_pos     (GwyRuler *ruler);


G_DEFINE_TYPE(GwyVRuler, gwy_vruler, GWY_TYPE_RULER)

static void
gwy_vruler_class_init(GwyVRulerClass *klass)
{
    GtkWidgetClass *widget_class;
    GwyRulerClass *ruler_class;

    widget_class = (GtkWidgetClass*) klass;
    ruler_class = (GwyRulerClass*) klass;

    widget_class->motion_notify_event = gwy_vruler_motion_notify;
    widget_class->realize = gwy_vruler_realize;

    ruler_class->prepare_sizes = gwy_vruler_prepare_sizes;
    ruler_class->draw_frame = gwy_vruler_draw_frame;
    ruler_class->draw_layout = gwy_vruler_draw_layout;
    ruler_class->draw_tick = gwy_vruler_draw_tick;
    ruler_class->draw_pos = gwy_vruler_draw_pos;
}

static void
gwy_vruler_init(GwyVRuler *vruler)
{
    GtkWidget *widget;

    widget = GTK_WIDGET(vruler);
    widget->requisition.width = widget->style->xthickness*2 + RULER_WIDTH;
    widget->requisition.height = widget->style->ythickness*2 + 1;
}

/**
 * gwy_vruler_new:
 *
 * Creates a new #GwyVRuler.
 *
 * Returns: The new ruler as a #GtkWidget.
 **/
GtkWidget*
gwy_vruler_new(void)
{
    return g_object_new(GWY_TYPE_VRULER, NULL);
}

static void
gwy_vruler_realize(GtkWidget *widget)
{
    const PangoMatrix *cmatrix;
    PangoMatrix matrix = PANGO_MATRIX_INIT;
    PangoContext *context;
    GwyRuler *ruler;

    if (GTK_WIDGET_CLASS(gwy_vruler_parent_class)->realize)
        (GTK_WIDGET_CLASS(gwy_vruler_parent_class)->realize)(widget);

    ruler = GWY_RULER(widget);

    context = pango_layout_get_context(ruler->layout);
    if ((cmatrix = pango_context_get_matrix(context)))
        matrix = *cmatrix;
    pango_matrix_rotate(&matrix, -90.0);
    pango_context_set_matrix(context, &matrix);
    pango_layout_context_changed(ruler->layout);
}

static gboolean
gwy_vruler_motion_notify(GtkWidget      *widget,
                         GdkEventMotion *event)
{
    GwyRuler *ruler;
    gint y;

    ruler = GWY_RULER(widget);

    if (event->is_hint)
        gdk_window_get_pointer(widget->window, NULL, &y, NULL);
    else
        y = event->y;

    ruler->position = ruler->lower + ((ruler->upper - ruler->lower) * y)
                                     / widget->allocation.height;
    g_object_notify(G_OBJECT(ruler), "position");

    /*  Make sure the ruler has been allocated already  */
    if (ruler->backing_store != NULL)
        gwy_ruler_draw_pos(ruler);

    return FALSE;
}

static void
gwy_vruler_prepare_sizes(GwyRuler *ruler)
{
    GtkWidget *widget;

    widget = GTK_WIDGET(ruler);
    ruler->hthickness = widget->style->ythickness;
    ruler->vthickness = widget->style->xthickness;
    ruler->height = widget->allocation.width - 2*ruler->vthickness;
    ruler->pixelsize = widget->allocation.height;
}

static void
gwy_vruler_draw_frame(GwyRuler *ruler)
{
    GdkGC *gc;
    GtkWidget *widget;

    widget = GTK_WIDGET(ruler);
    gc = widget->style->fg_gc[GTK_STATE_NORMAL];
    gdk_draw_line(ruler->backing_store, gc,
                  ruler->pixelsize + ruler->vthickness,
                  ruler->hthickness,
                  ruler->pixelsize + ruler->vthickness,
                  ruler->pixelsize - ruler->hthickness);
}

static void
gwy_vruler_draw_layout(GwyRuler *ruler,
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
                     "vruler",
                     vpos, hpos + 3,
                     ruler->layout);
}

static void
gwy_vruler_draw_tick(GwyRuler *ruler,
                     gint pos,
                     gint length)
{
    GdkGC *gc;
    GtkWidget *widget;

    widget = GTK_WIDGET(ruler);
    gc = widget->style->fg_gc[GTK_STATE_NORMAL];
    gdk_draw_line(ruler->backing_store, gc,
                  ruler->height + ruler->vthickness, pos,
                  ruler->height - length + ruler->vthickness, pos);
}

static void
gwy_vruler_draw_pos(GwyRuler *ruler)
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

        bs_height = ruler->height/2;
        bs_height |= 1;  /* make sure it's odd */
        bs_width = bs_height/2 + 1;

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

            x = (ruler->height + bs_width)/2 + ruler->vthickness;
            y = ROUND((ruler->position - ruler->lower) * increment)
                + (ruler->hthickness - bs_height)/2 - 1;

            for (i = 0; i < bs_width; i++)
                gdk_draw_line(widget->window, gc,
                              x + i, y + i,
                              x + i, y + bs_height - 1 - i);

            ruler->xsrc = x;
            ruler->ysrc = y;
        }
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
