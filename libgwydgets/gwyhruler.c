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
#include "gwyhruler.h"

#define _(x) x

#define RULER_HEIGHT          18
#define MINIMUM_INCR          5

#define ROUND(x)((int)((x) + 0.5))

static void gwy_hruler_class_init    (GwyHRulerClass *klass);
static void gwy_hruler_init          (GwyHRuler      *hruler);
static gint gwy_hruler_motion_notify (GtkWidget      *widget,
                                      GdkEventMotion *event);
static void gwy_hruler_draw_ticks    (GwyRuler       *ruler);
static void gwy_hruler_draw_pos      (GwyRuler       *ruler);
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
gwy_hruler_get_type(void)
{
    static GType hruler_type = 0;

    if (!hruler_type) {
        static const GTypeInfo hruler_info =
        {
            sizeof(GwyHRulerClass),
            NULL,           /* base_init */
            NULL,           /* base_finalize */
            (GClassInitFunc)gwy_hruler_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof(GwyHRuler),
            0,              /* n_preallocs */
            (GInstanceInitFunc)gwy_hruler_init,
            NULL,
        };

        hruler_type = g_type_register_static(GWY_TYPE_RULER, "GwyHRuler",
                                             &hruler_info, 0);
    }

    return hruler_type;
}

static void
gwy_hruler_class_init(GwyHRulerClass *klass)
{
    GtkWidgetClass *widget_class;
    GwyRulerClass *ruler_class;

    widget_class = (GtkWidgetClass*) klass;
    ruler_class = (GwyRulerClass*) klass;

    widget_class->motion_notify_event = gwy_hruler_motion_notify;

    ruler_class->draw_ticks = gwy_hruler_draw_ticks;
    ruler_class->draw_pos = gwy_hruler_draw_pos;
}

static void
gwy_hruler_init(GwyHRuler *hruler)
{
    GtkWidget *widget;

    widget = GTK_WIDGET(hruler);
    widget->requisition.width = widget->style->xthickness * 2 + 1;
    widget->requisition.height = widget->style->ythickness * 2 + RULER_HEIGHT;
}


GtkWidget*
gwy_hruler_new(void)
{
    return g_object_new(GWY_TYPE_HRULER, NULL);
}

static gint
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
label_callback(GwyRuler *ruler,
               gint position,
               const gchar *label,
               PangoLayout *layout,
               gint digit_height,
               gint digit_offset)
{
    GtkWidget *widget = (GtkWidget*)ruler;
    PangoRectangle ink_rect;
    gint ythickness;

    ythickness = widget->style->ythickness;
    pango_layout_set_text(layout, label, -1);
    /* XXX: this is different from vruler??? taken mindlessly from Gtk+ */
    pango_layout_get_extents(layout, &ink_rect, NULL);

    gtk_paint_layout(widget->style,
                     ruler->backing_store,
                     GTK_WIDGET_STATE(widget),
                     FALSE,
                     NULL,
                     widget,
                     "hruler",
                     position + 2,
                     ythickness + PANGO_PIXELS(ink_rect.y - digit_offset),
                     layout);
}

static void
tick_callback(GwyRuler *ruler,
              gint position,
              gint depth)
{
    GtkWidget *widget = (GtkWidget*)ruler;
    gint ythickness;
    gint height, tick_length;
    GdkGC *gc;

    gc = widget->style->fg_gc[GTK_STATE_NORMAL];
    ythickness = widget->style->ythickness;
    height = widget->allocation.height - 2*ythickness;
    tick_length = height/(depth + 1) - 2;

    gdk_draw_line(ruler->backing_store, gc,
                  position, height + ythickness,
                  position, height - tick_length + ythickness);
}

static void
gwy_hruler_draw_ticks(GwyRuler *ruler)
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

    width = widget->allocation.width;
    height = widget->allocation.height - 2*ythickness;

    gtk_paint_box(widget->style, ruler->backing_store,
                  GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                  NULL, widget, "hruler",
                  0, 0,
                  widget->allocation.width, widget->allocation.height);


    gdk_draw_line(ruler->backing_store, gc,
                  xthickness,
                  height + ythickness,
                  widget->allocation.width - xthickness,
                  height + ythickness);

    _gwy_ruler_real_draw_ticks(ruler, width,
                               xthickness + MINIMUM_INCR, MINIMUM_INCR,
                               label_callback, tick_callback);

}

static void
gwy_hruler_draw_pos(GwyRuler *ruler)
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
        width = widget->allocation.width;
        height = widget->allocation.height - ythickness * 2;

        bs_width = height / 2;
        bs_width |= 1;  /* make sure it's odd */
        bs_height = bs_width / 2 + 1;

        if ((bs_width > 0) && (bs_height > 0)) {
            /*  If a backing store exists, restore the ruler  */
            if (ruler->backing_store && ruler->non_gr_exp_gc)
                gdk_draw_drawable(ruler->widget.window,
                                  ruler->non_gr_exp_gc,
                                  ruler->backing_store,
                                  ruler->xsrc, ruler->ysrc,
                                  ruler->xsrc, ruler->ysrc,
                                  bs_width, bs_height);

            increment = (gdouble) width / (ruler->upper - ruler->lower);

            x = ROUND((ruler->position - ruler->lower) * increment)
                + (xthickness - bs_width) / 2 - 1;
            y = (height + bs_height) / 2 + ythickness;

            for (i = 0; i < bs_height; i++)
                gdk_draw_line(widget->window, gc,
                              x + i, y + i,
                              x + bs_width - 1 - i, y + i);


            ruler->xsrc = x;
            ruler->ysrc = y;
        }
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
