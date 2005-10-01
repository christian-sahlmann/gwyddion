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
#include <libgwyddion/gwyddion.h>
#include "gwyruler.h"
#include "gwydgettypes.h"

#define MINIMUM_INCR 5

enum {
    PROP_0,
    PROP_LOWER,
    PROP_UPPER,
    PROP_POSITION,
    PROP_MAX_SIZE,
    PROP_UNITS_PLACEMENT,
};

typedef enum {
    GWY_SCALE_0,
    GWY_SCALE_1,
    GWY_SCALE_2,
    GWY_SCALE_2_5,
    GWY_SCALE_5,
    GWY_SCALE_LAST
} GwyScaleScale;

static void          gwy_ruler_draw_ticks         (GwyRuler *ruler);
static void          gwy_ruler_realize            (GtkWidget *widget);
static void          gwy_ruler_unrealize          (GtkWidget *widget);
static void          gwy_ruler_size_allocate      (GtkWidget *widget,
                                                   GtkAllocation *allocation);
static gint          gwy_ruler_expose             (GtkWidget *widget,
                                                   GdkEventExpose *event);
static void          gwy_ruler_make_pixmap        (GwyRuler *ruler);
static void          gwy_ruler_set_property       (GObject *object,
                                                   guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void          gwy_ruler_get_property       (GObject *object,
                                                   guint prop_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static void          gwy_ruler_update_value_format(GwyRuler *ruler);
static gdouble       compute_base                 (gdouble max,
                                                   gdouble basebase);
static GwyScaleScale next_scale                   (GwyScaleScale scale,
                                                   gdouble *base,
                                                   gdouble measure,
                                                   gint min_incr);

static const gdouble steps[GWY_SCALE_LAST] = {
    0.0, 1.0, 2.0, 2.5, 5.0,
};

G_DEFINE_ABSTRACT_TYPE(GwyRuler, gwy_ruler, GTK_TYPE_WIDGET)

static void
gwy_ruler_class_init(GwyRulerClass *class)
{
    GObjectClass   *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(class);
    widget_class = (GtkWidgetClass*)class;

    gobject_class->set_property = gwy_ruler_set_property;
    gobject_class->get_property = gwy_ruler_get_property;

    widget_class->realize = gwy_ruler_realize;
    widget_class->unrealize = gwy_ruler_unrealize;
    widget_class->size_allocate = gwy_ruler_size_allocate;
    widget_class->expose_event = gwy_ruler_expose;

    g_object_class_install_property
        (gobject_class,
         PROP_LOWER,
         g_param_spec_double("lower",
                             "Lower limit",
                             "Lower limit of ruler",
                             -G_MAXDOUBLE,
                             G_MAXDOUBLE,
                             0.0,
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_UPPER,
         g_param_spec_double("upper",
                             "Upper limit",
                             "Upper limit of ruler",
                             -G_MAXDOUBLE,
                             G_MAXDOUBLE,
                             0.0,
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_POSITION,
         g_param_spec_double("position",
                             "Position",
                             "Position of mark on the ruler",
                             -G_MAXDOUBLE,
                             G_MAXDOUBLE,
                             0.0,
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_MAX_SIZE,
         g_param_spec_double("max-size",
                             "Maximum size",
                             "Maximum size of the ruler",
                             -G_MAXDOUBLE,
                             G_MAXDOUBLE,
                             0.0,
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_UNITS_PLACEMENT,
         g_param_spec_enum("units-placement",
                           "Units placement",
                           "The placement of units on the ruler, if any",
                           GWY_TYPE_UNITS_PLACEMENT,
                           GWY_UNITS_PLACEMENT_NONE,
                           G_PARAM_READWRITE));
}

static void
gwy_ruler_init(GwyRuler *ruler)
{
    ruler->units = GWY_SI_UNIT(gwy_si_unit_new("m"));
    gwy_ruler_update_value_format(ruler);
}

static void
gwy_ruler_set_property(GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    GwyRuler *ruler = GWY_RULER(object);

    switch (prop_id) {
        case PROP_LOWER:
        gwy_ruler_set_range(ruler, g_value_get_double(value), ruler->upper,
                            ruler->position, ruler->max_size);
        break;

        case PROP_UPPER:
        gwy_ruler_set_range(ruler, ruler->lower, g_value_get_double(value),
                            ruler->position, ruler->max_size);
        break;

        case PROP_POSITION:
        gwy_ruler_set_range(ruler, ruler->lower, ruler->upper,
                            g_value_get_double(value), ruler->max_size);
        break;

        case PROP_MAX_SIZE:
        gwy_ruler_set_range(ruler, ruler->lower, ruler->upper,
                            ruler->position,  g_value_get_double(value));
        break;

        case PROP_UNITS_PLACEMENT:
        gwy_ruler_set_units_placement(ruler,
                                      (GwyUnitsPlacement)g_value_get_uint(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_ruler_get_property(GObject      *object,
                       guint         prop_id,
                       GValue       *value,
                       GParamSpec   *pspec)
{
    GwyRuler *ruler = GWY_RULER(object);

    switch (prop_id) {
        case PROP_LOWER:
        g_value_set_double(value, ruler->lower);
        break;

        case PROP_UPPER:
        g_value_set_double(value, ruler->upper);
        break;

        case PROP_POSITION:
        g_value_set_double(value, ruler->position);
        break;

        case PROP_MAX_SIZE:
        g_value_set_double(value, ruler->max_size);
        break;

        case PROP_UNITS_PLACEMENT:
        g_value_set_uint(value, (guint)ruler->units_placement);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_ruler_set_range:
 * @ruler: A #GwyRuler
 * @lower: Lower limit of the ruler.
 * @upper: Upper limit of the ruler.
 * @position: Current position of the mark on the ruler.
 * @max_size: Maximum value used for calculating size of text labels.
 *
 * Sets range and current value of a ruler.
 **/
void
gwy_ruler_set_range(GwyRuler *ruler,
                    gdouble   lower,
                    gdouble   upper,
                    gdouble   position,
                    gdouble   max_size)
{
    g_return_if_fail(GWY_IS_RULER(ruler));

    g_object_freeze_notify(G_OBJECT(ruler));
    if (ruler->lower != lower) {
        ruler->lower = lower;
        g_object_notify(G_OBJECT(ruler), "lower");
    }
    if (ruler->upper != upper) {
        ruler->upper = upper;
        g_object_notify(G_OBJECT(ruler), "upper");
    }
    if (ruler->position != position) {
        ruler->position = position;
        g_object_notify(G_OBJECT(ruler), "position");
    }
    if (ruler->max_size != max_size) {
        ruler->max_size = max_size;
        g_object_notify(G_OBJECT(ruler), "max_size");
    }
    g_object_thaw_notify(G_OBJECT(ruler));
    gwy_ruler_update_value_format(ruler);

    if (GTK_WIDGET_DRAWABLE(ruler))
        gtk_widget_queue_draw(GTK_WIDGET(ruler));
}

/**
 * gwy_ruler_get_range:
 * @ruler: A #GwyRuler
 * @lower: Location to store lower limit of the ruler, or %NULL
 * @upper: Location to store upper limit of the ruler, or %NULL
 * @position: Location to store the current position of the mark on the ruler,
 *            or %NULL
 * @max_size: Location to store the maximum size of the ruler used when
 *            calculating the space to leave for the text, or %NULL.
 *
 * Retrieves values indicating the range and current position of a #GwyRuler.
 * See gwy_ruler_set_range().
 **/
void
gwy_ruler_get_range(GwyRuler *ruler,
                    gdouble  *lower,
                    gdouble  *upper,
                    gdouble  *position,
                    gdouble  *max_size)
{
    g_return_if_fail(GWY_IS_RULER(ruler));

    if (lower)
        *lower = ruler->lower;
    if (upper)
        *upper = ruler->upper;
    if (position)
        *position = ruler->position;
    if (max_size)
        *max_size = ruler->max_size;
}

/**
 * gwy_ruler_set_units_placement:
 * @ruler: A #GwyRuler
 * @placement: Units placement specification.
 *
 * Sets whether and where units should be placed on the ruler.
 **/
void
gwy_ruler_set_units_placement(GwyRuler *ruler,
                              GwyUnitsPlacement placement)
{
    g_return_if_fail(GWY_IS_RULER(ruler));
    placement = MIN(placement, GWY_UNITS_PLACEMENT_AT_ZERO);
    if (ruler->units_placement == placement)
        return;

    ruler->units_placement = placement;
    g_object_notify(G_OBJECT(ruler), "units_placement");

    if (GTK_WIDGET_DRAWABLE(ruler))
        gtk_widget_queue_draw(GTK_WIDGET(ruler));
}

/**
 * gwy_ruler_get_units_placement:
 * @ruler: A #GwyRuler
 *
 * Gets current units placement of ruler @ruler.
 *
 * Returns: The units placement.
 **/
GwyUnitsPlacement
gwy_ruler_get_units_placement(GwyRuler *ruler)
{
    g_return_val_if_fail(GWY_IS_RULER(ruler), GWY_UNITS_PLACEMENT_NONE);
    return ruler->units_placement;
}

static void
gwy_ruler_realize(GtkWidget *widget)
{
    GwyRuler *ruler;
    GdkWindowAttr attributes;
    gint attributes_mask;

    ruler = GWY_RULER(widget);
    GTK_WIDGET_SET_FLAGS(ruler, GTK_REALIZED);

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);
    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.event_mask |= (GDK_EXPOSURE_MASK |
                              GDK_POINTER_MOTION_MASK |
                              GDK_POINTER_MOTION_HINT_MASK);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
                                    &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, ruler);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    ruler->layout = gtk_widget_create_pango_layout(widget, NULL);
    gwy_ruler_make_pixmap(ruler);
}

static void
gwy_ruler_unrealize(GtkWidget *widget)
{
    GwyRuler *ruler = GWY_RULER(widget);

    gwy_object_unref(ruler->layout);
    gwy_object_unref(ruler->backing_store);
    gwy_object_unref(ruler->non_gr_exp_gc);
    gwy_object_unref(ruler->units);
    gwy_si_unit_value_format_free(ruler->vformat);

    if (GTK_WIDGET_CLASS(gwy_ruler_parent_class)->unrealize)
        (GTK_WIDGET_CLASS(gwy_ruler_parent_class)->unrealize)(widget);
}

static void
gwy_ruler_size_allocate(GtkWidget     *widget,
                        GtkAllocation *allocation)
{
    GwyRuler *ruler = GWY_RULER(widget);

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(widget)) {
        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);

        gwy_ruler_update_value_format(ruler);
        gwy_ruler_make_pixmap(ruler);
    }
}

static gint
gwy_ruler_expose(GtkWidget *widget,
                 G_GNUC_UNUSED GdkEventExpose *event)
{
    GwyRuler *ruler;

    if (GTK_WIDGET_DRAWABLE(widget)) {
        ruler = GWY_RULER(widget);

        gtk_paint_box(widget->style, widget->window,
                      GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                      NULL, widget, "ruler",
                      0, 0,
                      widget->allocation.width, widget->allocation.height);

        gdk_draw_drawable(ruler->backing_store,
                          ruler->non_gr_exp_gc,
                          widget->window,
                          0, 0, 0, 0,
                          widget->allocation.width,
                          widget->allocation.height);

        gwy_ruler_draw_ticks(ruler);

        gdk_draw_drawable(widget->window,
                          ruler->non_gr_exp_gc,
                          ruler->backing_store,
                          0, 0, 0, 0,
                          widget->allocation.width,
                          widget->allocation.height);

        gwy_ruler_draw_pos(ruler);
    }

    return FALSE;
}

static void
gwy_ruler_make_pixmap(GwyRuler *ruler)
{
    GtkWidget *widget;
    gint width;
    gint height;

    widget = GTK_WIDGET(ruler);

    if (ruler->backing_store) {
        gdk_drawable_get_size(ruler->backing_store, &width, &height);
        if ((width == widget->allocation.width) &&
            (height == widget->allocation.height))
            return;

        g_object_unref(ruler->backing_store);
    }

    ruler->backing_store = gdk_pixmap_new(widget->window,
                                          widget->allocation.width,
                                          widget->allocation.height,
                                          -1);
    ruler->xsrc = 0;
    ruler->ysrc = 0;

    if (!ruler->non_gr_exp_gc) {
        ruler->non_gr_exp_gc = gdk_gc_new(widget->window);
        gdk_gc_set_exposures(ruler->non_gr_exp_gc, FALSE);
    }
}

/**
 * gwy_ruler_set_units:
 * @ruler: A #GwyRuler.
 * @units: The base units this ruler should display.
 *
 * Sets the base units a ruler displays.
 *
 * Setting units to %NULL effectively disables them.
 **/
void
gwy_ruler_set_units(GwyRuler *ruler,
                    GwySIUnit *units)
{
    g_return_if_fail(GWY_IS_RULER(ruler));
    gwy_object_unref(ruler->units);
    if (units)
        g_object_ref(units);
    ruler->units = units;
    gwy_ruler_update_value_format(ruler);
    gtk_widget_queue_draw(GTK_WIDGET(ruler));
}

/**
 * gwy_ruler_get_units:
 * @ruler: A #GwyRuler.
 *
 * Returns the base units a ruler uses.
 *
 * Returns: The units the rules uses.
 **/
GwySIUnit*
gwy_ruler_get_units(GwyRuler *ruler)
{
    g_return_val_if_fail(GWY_IS_RULER(ruler), NULL);
    return ruler->units;
}

static void
gwy_ruler_update_value_format(GwyRuler *ruler)
{
    gdouble max;

    max = ruler->max_size;
    if (!max)
        max = MAX(fabs(ruler->lower), fabs(ruler->upper));
    if (!max)
        max = 1.2;

    ruler->vformat
        = gwy_si_unit_get_format_with_resolution(ruler->units,
                                                 GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                 max, max/12,
                                                 ruler->vformat);
    if (ruler->vformat->precision > 1)
        ruler->vformat
            = gwy_si_unit_get_format_with_resolution(ruler->units,
                                                     GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                     max/12, max/24,
                                                     ruler->vformat);
}

/**
 * gwy_ruler_draw_pos:
 * @ruler: A #GwyRuler.
 *
 * Draws a position marker.
 *
 * This method is intended primarily for subclass implementation.
 **/
void
gwy_ruler_draw_pos(GwyRuler *ruler)
{
    void (*method)(GwyRuler*);

    if ((method = GWY_RULER_GET_CLASS(ruler)->draw_pos))
        method(ruler);
}

static void
gwy_ruler_draw_ticks(GwyRuler *ruler)
{
    gdouble lower, upper, max;
    gint text_size, labels, i, scale_depth;
    gint tick_length, min_label_spacing, min_tick_spacing;
    gdouble range, measure, base, step, first;
    GwyScaleScale scale;
    GwySIValueFormat *format;
    PangoRectangle rect;
    gchar *unit_str;
    gint unitstr_len;
    gboolean units_drawn;
    GtkWidget *widget;
    GdkGC *gc;
    gint ascent, descent, vpos;
    struct { GwyScaleScale scale; double base; } tick_info[4];

    widget = GTK_WIDGET(ruler);
    if (!GTK_WIDGET_DRAWABLE(ruler))
        return;

    GWY_RULER_GET_CLASS(ruler)->prepare_sizes(ruler);
    min_label_spacing = ruler->hthickness + MINIMUM_INCR;
    min_tick_spacing = MINIMUM_INCR;

    GWY_RULER_GET_CLASS(ruler)->draw_frame(ruler);

    format = ruler->vformat;
    upper = ruler->upper;
    lower = ruler->lower;
    if (upper <= lower || ruler->pixelsize < 2 || ruler->pixelsize > 10000)
        return;
    max = ruler->max_size;
    if (max == 0)
        max = MAX(fabs(lower), fabs(upper));

    range = upper - lower;
    measure = range/format->magnitude / ruler->pixelsize;
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

    pango_layout_set_markup(ruler->layout, unit_str, -1);
    pango_layout_get_extents(ruler->layout, NULL, &rect);
    ascent = PANGO_ASCENT(rect);
    descent = PANGO_DESCENT(rect);
    text_size = PANGO_PIXELS(rect.width) + 1;

    /* reallocate unit_str with some margin */
    unitstr_len = strlen(unit_str) + 16;
    unit_str = g_renew(gchar, unit_str, unitstr_len);

    /* fit as many labels as you can */
    labels = floor(ruler->pixelsize/(text_size + ruler->hthickness
                                     + min_label_spacing));
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
    units_drawn = FALSE;
    first = floor(lower/format->magnitude / (base*step))*base*step;
    for (i = 0; ; i++) {
        gint pos;
        gdouble val;

        val = i*step*base + first;
        pos = floor((val - lower/format->magnitude)/measure);
        if (pos >= ruler->pixelsize)
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

        pango_layout_set_markup(ruler->layout, unit_str, -1);
        /* this is the best approximation of same positioning I'm able to do,
         * but it's still wrong */
        pango_layout_get_extents(ruler->layout, NULL, &rect);
        vpos = ruler->vthickness + PANGO_PIXELS(ascent - PANGO_ASCENT(rect));
        GWY_RULER_GET_CLASS(ruler)->draw_layout(ruler, pos + 3, vpos);
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
        tick_length = ruler->height/(scale_depth + 1) - 2;
        scale = tick_info[scale_depth].scale;
        base = tick_info[scale_depth].base;
        step = steps[scale];
        first = floor(lower/format->magnitude / (base*step))*base*step;
        for (i = 0; ; i++) {
            gint pos;
            gdouble val;

            val = (i + 0.000001)*step*base + first;
            pos = floor((val - lower/format->magnitude)/measure);
            if (pos >= ruler->pixelsize)
                break;
            if (pos < 0)
                continue;

            GWY_RULER_GET_CLASS(ruler)->draw_tick(ruler, pos, tick_length);
        }
        scale_depth--;
    }

    g_free(unit_str);
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
