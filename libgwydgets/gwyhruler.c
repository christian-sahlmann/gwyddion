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
 * and a few drawing functions.
 */
#define DEBUG 1
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwyhruler.h"

#define RULER_HEIGHT          20
#define MINIMUM_INCR          5

typedef enum {
    GWY_SCALE_0,
    GWY_SCALE_1,
    GWY_SCALE_2,
    GWY_SCALE_2_5,
    GWY_SCALE_5,
    GWY_SCALE_LAST
} GwyScaleScale;

static const gdouble steps[GWY_SCALE_LAST] = {
    0.0, 1.0, 2.0, 2.5, 5.0,
};

static void     gwy_hruler_class_init    (GwyHRulerClass *klass);
static void     gwy_hruler_init          (GwyHRuler      *hruler);
static gboolean gwy_hruler_motion_notify (GtkWidget      *widget,
                                          GdkEventMotion *event);
static void     gwy_hruler_draw_ticks    (GwyRuler       *ruler);
static void     gwy_hruler_draw_pos      (GwyRuler       *ruler);
static void     gwy_hruler_real_draw_ticks(GwyRuler *ruler,
                                           gint pixelsize,
                                           gint min_label_spacing,
                                           gint min_tick_spacing);
static gdouble       compute_base            (gdouble max,
                                              gdouble basebase);
static GwyScaleScale next_scale              (GwyScaleScale scale,
                                              gdouble *base,
                                              gdouble measure,
                                              gint min_incr);

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

    gdk_draw_line(ruler->backing_store, gc,
                  xthickness,
                  height + ythickness,
                  widget->allocation.width - xthickness,
                  height + ythickness);

    gwy_hruler_real_draw_ticks(ruler, width,
                               xthickness + MINIMUM_INCR, MINIMUM_INCR);
}

void
gwy_hruler_real_draw_ticks(GwyRuler *ruler,
                           gint pixelsize,
                           gint min_label_spacing,
                           gint min_tick_spacing)
{
    gdouble lower, upper, max;
    gint text_size, labels, i, scale_depth;
    gint height, tick_length, xthickness, ythickness;
    gdouble range, measure, base, step, first;
    GwyScaleScale scale;
    GwySIValueFormat *format;
    PangoLayout *layout;
    PangoRectangle rect;
    gchar *unit_str;
    gint unitstr_len;
    gboolean units_drawn;
    GtkWidget *widget;
    GdkGC *gc;
    gint ascent, descent, ypos;
    struct { GwyScaleScale scale; double base; } tick_info[4];

    widget = GTK_WIDGET(ruler);
    ythickness = widget->style->ythickness;
    xthickness = widget->style->xthickness;

    upper = ruler->upper;
    lower = ruler->lower;
    if (upper <= lower || pixelsize < 2 || pixelsize > 10000)
        return;
    max = ruler->max_size;
    if (max == 0)
        max = MAX(fabs(lower), fabs(upper));

    range = upper - lower;
    format = gwy_si_unit_get_format_with_resolution(ruler->units,
                                                    max, max/12, NULL);
    measure = range/format->magnitude / pixelsize;
    max /= format->magnitude;

    switch (ruler->units_placement && ruler->units) {
        case GWY_UNITS_PLACEMENT_AT_ZERO:
        unit_str
            = g_strdup_printf("%d %s",
                              (lower > 0) ? (gint)(lower/format->magnitude) : 0,
                              format->units);
        break;

        default:
        unit_str = g_strdup_printf("%d", (gint)max);
        break;
    }

    layout = gtk_widget_create_pango_layout(widget, NULL);
    pango_layout_set_markup(layout, unit_str, -1);
    pango_layout_get_extents(layout, NULL, &rect);
    ascent = PANGO_ASCENT(rect);
    descent = PANGO_DESCENT(rect);
    text_size = PANGO_PIXELS(rect.width) + 1;

    /* reallocate unit_str with some margin */
    unitstr_len = strlen(unit_str) + 16;
    unit_str = g_renew(gchar, unit_str, unitstr_len);

    /* fit as many labels as you can */
    labels = floor(pixelsize/(text_size + xthickness + min_label_spacing));
    labels = MAX(labels, 1);
    if (labels > 6)
        labels = 6 + (labels - 5)/2;

    step = range/format->magnitude / labels;
    base = compute_base(step, 10);
    step /= base;
    if (step >= 5.0 || base < 1.0) {
        scale = GWY_SCALE_1;
        base *= 10;
    }
    else if (step >= 2.5)
        scale = GWY_SCALE_5;
    else if (step >= 2.0)
        scale = GWY_SCALE_2_5;
    else
        scale = GWY_SCALE_2;
    step = steps[scale];

    /* draw labels */
    height = widget->allocation.height - 2*ythickness;
    units_drawn = FALSE;
    first = floor(lower/format->magnitude / (base*step))*base*step;
    for (i = 0; ; i++) {
        gint pos;
        gdouble val;

        val = i*step*base + first;
        pos = floor((val - lower/format->magnitude)/measure);
        if (pos >= pixelsize)
            break;
        if (pos < 0)
            continue;
        if (!units_drawn
            && (upper < 0 || val >= 0)
            && ruler->units_placement == GWY_UNITS_PLACEMENT_AT_ZERO
            && ruler->units) {
            g_snprintf(unit_str, unitstr_len, "%d %s",
                       ROUND(val), format->units);
            units_drawn = TRUE;
        }
        else
            g_snprintf(unit_str, unitstr_len, "%d", ROUND(val));

        pango_layout_set_markup(layout, unit_str, -1);
        /* this is the best approximation of same positioning I'm able to do,
         * but it's still wrong */
        pango_layout_get_extents(layout, NULL, &rect);
        ypos = ythickness + PANGO_PIXELS(ascent - PANGO_ASCENT(rect));
        gtk_paint_layout(widget->style,
                         ruler->backing_store,
                         GTK_WIDGET_STATE(widget),
                         FALSE,
                         NULL,
                         widget,
                         "hruler",
                         pos + 3, ypos,
                         layout);
    }

    /* draw tick marks, from smallest to largest */
    scale_depth = 0;
    while (scale && scale_depth < (gint)G_N_ELEMENTS(tick_info)) {
        tick_info[scale_depth].scale = scale;
        tick_info[scale_depth].base = base;
        scale = next_scale(scale, &base, measure, min_tick_spacing);
        scale_depth++;
    }
    scale_depth--;

    gc = widget->style->fg_gc[GTK_STATE_NORMAL];
    while (scale_depth > -1) {
        tick_length = height/(scale_depth + 1) - 2;
        scale = tick_info[scale_depth].scale;
        base = tick_info[scale_depth].base;
        step = steps[scale];
        first = floor(lower/format->magnitude / (base*step))*base*step;
        for (i = 0; ; i++) {
            gint pos;
            gdouble val;

            val = (i + 0.000001)*step*base + first;
            pos = floor((val - lower/format->magnitude)/measure);
            if (pos >= pixelsize)
                break;
            if (pos < 0)
                continue;

            gdk_draw_line(ruler->backing_store, gc,
                          pos, height + ythickness,
                          pos, height - tick_length + ythickness);
        }
        scale_depth--;
    }

    g_free(unit_str);
    gwy_si_unit_value_format_free(format);
    g_object_unref(layout);
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

static gdouble
compute_base(gdouble max, gdouble basebase)
{
    gint i;
    gdouble base;

    i = floor(log(max)/log(basebase));
    base = 1.0;
    if (i > 0)
        while (i--)
            base *= basebase;
    else
        while (i++)
            base /= basebase;
    return base;
}

static GwyScaleScale
next_scale(GwyScaleScale scale,
           gdouble *base,
           gdouble measure,
           gint min_incr)
{
    GwyScaleScale new_scale = GWY_SCALE_0;

    switch (scale) {
        case GWY_SCALE_1:
        *base /= 10.0;
        if ((gint)floor(*base*2.0/measure) > min_incr)
            new_scale = GWY_SCALE_5;
        else if ((gint)floor(*base*2.5/measure) > min_incr)
            new_scale = GWY_SCALE_2_5;
        else if ((gint)floor(*base*5.0/measure) > min_incr)
            new_scale = GWY_SCALE_5;
        break;

        case GWY_SCALE_2:
        if ((gint)floor(*base/measure) > min_incr)
            new_scale = GWY_SCALE_1;
        break;

        case GWY_SCALE_2_5:
        *base /= 10.0;
        if ((gint)floor(*base*5.0/measure) > min_incr)
            new_scale = GWY_SCALE_5;
        break;

        case GWY_SCALE_5:
        if ((gint)floor(*base/measure) > min_incr)
            new_scale = GWY_SCALE_1;
        else if ((gint)floor(*base*2.5/measure) > min_incr)
            new_scale = GWY_SCALE_2_5;
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return new_scale;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
