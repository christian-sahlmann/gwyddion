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
 * and a few drawing functions.
 */

#include <string.h>
#include "gwyvruler.h"

#define RULER_WIDTH           18
#define MINIMUM_INCR          5

#define ROUND(x)((int)((x) + 0.5))


static void gwy_vruler_class_init    (GwyVRulerClass *klass);
static void gwy_vruler_init          (GwyVRuler      *vruler);
static gint gwy_vruler_motion_notify (GtkWidget      *widget,
                                      GdkEventMotion *event);
static void gwy_vruler_draw_ticks    (GwyRuler       *ruler);
static void gwy_vruler_draw_pos      (GwyRuler       *ruler);
static void label_callback           (GwyRuler *ruler,
                                      gint position,
                                      const gchar *label,
                                      PangoLayout *layout,
                                      gint digit_height,
                                      gint digit_offset);
static void tick_callback            (GwyRuler *ruler,
                                      gint position,
                                      gint depth);


GType
gwy_vruler_get_type(void)
{
    static GType vruler_type = 0;

    if (!vruler_type) {
        static const GTypeInfo vruler_info = {
            sizeof(GwyVRulerClass),
            NULL,           /* base_init */
            NULL,           /* base_finalize */
            (GClassInitFunc)gwy_vruler_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof(GwyVRuler),
            0,              /* n_preallocs */
            (GInstanceInitFunc)gwy_vruler_init,
            NULL,
        };

        vruler_type = g_type_register_static(GWY_TYPE_RULER, "GwyVRuler",
                                             &vruler_info, 0);
    }

    return vruler_type;
}

static void
gwy_vruler_class_init(GwyVRulerClass *klass)
{
    GtkWidgetClass *widget_class;
    GwyRulerClass *ruler_class;

    widget_class = (GtkWidgetClass*) klass;
    ruler_class = (GwyRulerClass*) klass;

    widget_class->motion_notify_event = gwy_vruler_motion_notify;

    ruler_class->draw_ticks = gwy_vruler_draw_ticks;
    ruler_class->draw_pos = gwy_vruler_draw_pos;
}

static void
gwy_vruler_init(GwyVRuler *vruler)
{
    GtkWidget *widget;

    widget = GTK_WIDGET(vruler);
    widget->requisition.width = widget->style->xthickness * 2 + RULER_WIDTH;
    widget->requisition.height = widget->style->ythickness * 2 + 1;
}

GtkWidget*
gwy_vruler_new(void)
{
    return g_object_new(GWY_TYPE_VRULER, NULL);
}


static gint
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
label_callback(GwyRuler *ruler,
               gint position,
               const gchar *label,
               PangoLayout *layout,
               gint digit_height,
               gint digit_offset)
{
    GtkWidget *widget = (GtkWidget*)ruler;
    PangoRectangle logical_rect;
    gint xthickness;
    const gchar *utf8p, *utf8next;
    gint j;

    xthickness = widget->style->xthickness;
    pango_layout_set_text(layout, label, -1);

    utf8p = label;
    utf8next = g_utf8_next_char(utf8p);
    j = 0;
    while (*utf8p) {
        pango_layout_set_text(layout, utf8p, utf8next - utf8p);
        pango_layout_get_extents(layout, NULL, &logical_rect);

        gtk_paint_layout(widget->style,
                         ruler->backing_store,
                         GTK_WIDGET_STATE(widget),
                         FALSE,
                         NULL,
                         widget,
                         "vruler",
                         xthickness + 1,
                         position + digit_height*j + 2
                            + PANGO_PIXELS(logical_rect.y - digit_offset),
                         layout);
        utf8p = utf8next;
        utf8next = g_utf8_next_char(utf8p);
        j++;
    }
}

static void
tick_callback(GwyRuler *ruler,
              gint position,
              gint depth)
{
    GtkWidget *widget = (GtkWidget*)ruler;
    gint xthickness;
    gint width, tick_length;
    GdkGC *gc;

    gc = widget->style->fg_gc[GTK_STATE_NORMAL];
    xthickness = widget->style->xthickness;
    width = widget->allocation.width - 2*xthickness;
    tick_length = width/(depth + 1) - 2;

    gdk_draw_line(ruler->backing_store, gc,
                  width + xthickness - tick_length, position,
                  width + xthickness, position);
}

static void
gwy_vruler_draw_ticks(GwyRuler *ruler)
{
    GtkWidget *widget;
    GdkGC *gc;
    gint width, height;
    gint xthickness;
    gint ythickness;

    if (!GTK_WIDGET_DRAWABLE(ruler))
        return;

    widget = GTK_WIDGET(ruler);

    gc = widget->style->fg_gc[GTK_STATE_NORMAL];

    xthickness = widget->style->xthickness;
    ythickness = widget->style->ythickness;

    height = widget->allocation.height;
    width = widget->allocation.width - 2*xthickness;

    gtk_paint_box(widget->style, ruler->backing_store,
                  GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                  NULL, widget, "vruler",
                  0, 0,
                  widget->allocation.width, widget->allocation.height);

    gdk_draw_line(ruler->backing_store, gc,
                  height + xthickness,
                  ythickness,
                  height + xthickness,
                  widget->allocation.height - ythickness);

    _gwy_ruler_real_draw_ticks(ruler, height,
                               xthickness + MINIMUM_INCR, MINIMUM_INCR,
                               label_callback, tick_callback);
}


static void
gwy_vruler_draw_pos(GwyRuler *ruler)
{
    GtkWidget *widget;
    GdkGC *gc;
    int i;
    gint x, y;
    gint width, height;
    gint bs_width, bs_height;
    gint xthickness;
    gint ythickness;
    gdouble increment;

    if (GTK_WIDGET_DRAWABLE(ruler)) {
        widget = GTK_WIDGET(ruler);

        gc = widget->style->fg_gc[GTK_STATE_NORMAL];
        xthickness = widget->style->xthickness;
        ythickness = widget->style->ythickness;
        width = widget->allocation.width - xthickness * 2;
        height = widget->allocation.height;

        bs_height = width / 2;
        bs_height |= 1;  /* make sure it's odd */
        bs_width = bs_height / 2 + 1;

        if ((bs_width > 0) && (bs_height > 0)) {
            /*  If a backing store exists, restore the ruler  */
            if (ruler->backing_store && ruler->non_gr_exp_gc)
                gdk_draw_drawable(ruler->widget.window,
                                  ruler->non_gr_exp_gc,
                                  ruler->backing_store,
                                  ruler->xsrc, ruler->ysrc,
                                  ruler->xsrc, ruler->ysrc,
                                  bs_width, bs_height);

            increment = (gdouble) height / (ruler->upper - ruler->lower);

            x = (width + bs_width) / 2 + xthickness;
            y = ROUND((ruler->position - ruler->lower) * increment)
                + (ythickness - bs_height) / 2 - 1;

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
