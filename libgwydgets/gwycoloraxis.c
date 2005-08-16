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

#include "config.h"
#include <gtk/gtk.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <glib-object.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwymath.h>

#include "gwycoloraxis.h"

static void     gwy_color_axis_destroy            (GtkObject *object);
static void     gwy_color_axis_realize            (GtkWidget *widget);
static void     gwy_color_axis_unrealize          (GtkWidget *widget);
static void     gwy_color_axis_size_request       (GtkWidget *widget,
                                                   GtkRequisition *requisition);
static void     gwy_color_axis_size_allocate      (GtkWidget *widget,
                                                   GtkAllocation *allocation);
static gboolean gwy_color_axis_expose             (GtkWidget *widget,
                                                   GdkEventExpose *event);
static void     gwy_color_axis_adjust             (GwyColorAxis *axis,
                                                   gint width,
                                                   gint height);
static void     gwy_color_axis_draw_label         (GtkWidget *widget);
static void     gwy_color_axis_update             (GwyColorAxis *axis);

G_DEFINE_TYPE(GwyColorAxis, gwy_color_axis, GTK_TYPE_WIDGET)

static void
gwy_color_axis_class_init(GwyColorAxisClass *klass)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = GTK_OBJECT_CLASS(klass);
    widget_class = GTK_WIDGET_CLASS(klass);

    object_class->destroy = gwy_color_axis_destroy;

    widget_class->realize = gwy_color_axis_realize;
    widget_class->expose_event = gwy_color_axis_expose;
    widget_class->size_request = gwy_color_axis_size_request;
    widget_class->unrealize = gwy_color_axis_unrealize;
    widget_class->size_allocate = gwy_color_axis_size_allocate;
}

static void
gwy_color_axis_init(GwyColorAxis *axis)
{
    axis->orientation = GTK_ORIENTATION_VERTICAL;
    axis->tick_length = 6;
    axis->stripe_width = 10;
    axis->has_labels = TRUE;
}

/**
 * gwy_color_axis_new_with_range:
 * @orientation: The orientation of the axis.
 * @min: The minimum.
 * @max: The maximum.
 *
 * Creates a new color axis.
 *
 * Returns: The newly created color axis as a #GtkWidget.
 **/
GtkWidget*
gwy_color_axis_new_with_range(GtkOrientation orientation,
                              gdouble min,
                              gdouble max)
{
    GwyColorAxis *axis;

    axis = gtk_type_new(gwy_color_axis_get_type());
    axis->orientation = orientation;
    axis->min = MIN(min, max);
    axis->max = MAX(min, max);

    axis->gradient = gwy_gradients_get_gradient(NULL);
    axis->gradient_id
        = g_signal_connect_swapped(axis->gradient, "data-changed",
                                   G_CALLBACK(gwy_color_axis_update), axis);
    gwy_resource_use(GWY_RESOURCE(axis->gradient));

    axis->siunit = gwy_si_unit_new("");

    return GTK_WIDGET(axis);
}

/**
 * gwy_color_axis_new:
 * @orientation: The orientation of the axis.
 *
 * Creates a new color axis.
 *
 * Returns: The newly created color axis as a #GtkWidget.
 **/
GtkWidget*
gwy_color_axis_new(GtkOrientation orientation)
{
    return gwy_color_axis_new_with_range(orientation, 0.0, 1.0);
}

static void
gwy_color_axis_destroy(GtkObject *object)
{
    GwyColorAxis *axis;

    axis = (GwyColorAxis*)object;
    if (axis->gradient_id) {
        g_signal_handler_disconnect(axis->gradient, axis->gradient_id);
        axis->gradient_id = 0;
    }
    gwy_object_unref(axis->siunit);
    if (axis->gradient) {
        gwy_resource_release(GWY_RESOURCE(axis->gradient));
        axis->gradient = NULL;
    }
    gwy_object_unref(axis->stripe);

    GTK_OBJECT_CLASS(gwy_color_axis_parent_class)->destroy(object);
}

static void
gwy_color_axis_unrealize(GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS(gwy_color_axis_parent_class)->unrealize)
        GTK_WIDGET_CLASS(gwy_color_axis_parent_class)->unrealize(widget);
}

static void
gwy_color_axis_realize(GtkWidget *widget)
{
    GwyColorAxis *axis;
    GdkWindowAttr attributes;
    gint attributes_mask;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_COLOR_AXIS(widget));

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    axis = GWY_COLOR_AXIS(widget);

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

    /*compute axis*/
    gwy_color_axis_update(axis);
}

static void
gwy_color_axis_size_request(GtkWidget *widget,
                            GtkRequisition *requisition)
{
    GwyColorAxis *axis;

    axis = GWY_COLOR_AXIS(widget);

    /* XXX */
    if (axis->orientation == GTK_ORIENTATION_VERTICAL) {
        requisition->width = 80;
        requisition->height = 100;
    }
    else {
        requisition->width = 100;
        requisition->height = 80;
    }
}

static void
gwy_color_axis_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GwyColorAxis *axis;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_COLOR_AXIS(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;

    axis = GWY_COLOR_AXIS(widget);
    if (GTK_WIDGET_REALIZED(widget)) {
        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
    }
    gwy_color_axis_adjust(axis, allocation->width, allocation->height);
}

static void
gwy_color_axis_adjust(GwyColorAxis *axis, gint width, gint height)
{
    gint i, j, rowstride, palsize, dval;
    guchar *pixels, *line;
    const guchar *samples, *s;
    gdouble cor;

    g_return_if_fail(axis->orientation == GTK_ORIENTATION_VERTICAL
                     || axis->orientation == GTK_ORIENTATION_HORIZONTAL);
    gwy_object_unref(axis->stripe);

    if (axis->orientation == GTK_ORIENTATION_VERTICAL)
        axis->stripe = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                      axis->stripe_width, height);
    else
        axis->stripe = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                      width, axis->stripe_width);
    gwy_debug_objects_creation(G_OBJECT(axis->stripe));

    /*render stripe according to orientation*/
    pixels = gdk_pixbuf_get_pixels(axis->stripe);
    rowstride = gdk_pixbuf_get_rowstride(axis->stripe);
    samples = gwy_gradient_get_samples(axis->gradient, &palsize);

    if (axis->orientation == GTK_ORIENTATION_VERTICAL) {
        cor = (palsize - 1.0)/height;
        for (i = 0; i < height; i++) {
            line = pixels + i*rowstride;
            dval = (gint)((height-i-1)*cor + 0.5);
            for (j = 0; j < axis->stripe_width*height; j += height) {
                s = samples + 4*dval;
                *(line++) = *(s++);
                *(line++) = *(s++);
                *(line++) = *s;
            }
        }
    }
    else {
        cor = (palsize - 1.0)/width;
        for (i = 0; i < height; i++) {
            line = pixels + i*rowstride;
            for (j = 0; j < axis->stripe_width*height; j += height) {
                dval = (gint)((j/height)*cor + 0.5);
                s = samples + 4*dval;
                *(line++) = *(s++);
                *(line++) = *(s++);
                *(line++) = *s;
            }
        }
    }
}

static gboolean
gwy_color_axis_expose(GtkWidget *widget,
                      GdkEventExpose *event)
{
    GwyColorAxis *axis;
    GdkGC *gc;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    axis = GWY_COLOR_AXIS(widget);
    gc = widget->style->fg_gc[GTK_WIDGET_STATE(widget)];

    if (axis->orientation == GTK_ORIENTATION_HORIZONTAL)
        gdk_draw_pixbuf(widget->window, gc, axis->stripe,
                        0, 0,
                        0, widget->allocation.height - axis->stripe_width - 1,
                        gdk_pixbuf_get_width(axis->stripe),
                        gdk_pixbuf_get_height(axis->stripe),
                        GDK_RGB_DITHER_NONE, 0, 0);
    else
        gdk_draw_pixbuf(widget->window, gc, axis->stripe,
                        0, 0,
                        0, 0,
                        gdk_pixbuf_get_width(axis->stripe),
                        gdk_pixbuf_get_height(axis->stripe),
                        GDK_RGB_DITHER_NONE, 0, 0);

    gwy_color_axis_draw_label(widget);

    return FALSE;
}

static void
gwy_color_axis_draw_label(GtkWidget *widget)
{
    GwyColorAxis *axis;
    PangoLayout *layout;
    GwySIValueFormat *format = NULL;
    GString *strmin, *strmax;
    GdkGC *gc;
    PangoRectangle rect;
    gint xthickness, ythickness, width, height, swidth, tlength, off;
    gdouble max;

    axis = GWY_COLOR_AXIS(widget);
    xthickness = widget->style->xthickness;
    ythickness = widget->style->ythickness;
    width = widget->allocation.width;
    height = widget->allocation.height;
    swidth = axis->stripe_width;
    tlength = axis->tick_length;
    off = swidth + 1
          + ((axis->orientation == GTK_ORIENTATION_VERTICAL)
             ? xthickness : ythickness);

    /* Compute minimum and maximum numbers */
    strmin = g_string_new("");
    strmax = g_string_new("");
    max = MAX(fabs(axis->min), fabs(axis->max));
    if (max == 0) {
        g_string_assign(strmin, "0.0");
        g_string_assign(strmax, "0.0");
    }
    else {
        format = gwy_si_unit_get_format(axis->siunit,
                                        GWY_SI_UNIT_FORMAT_VFMARKUP, max, NULL);
        g_string_printf(strmin, "%3.1f %s",
                        axis->min/format->magnitude, format->units);
        g_string_printf(strmax, "%3.1f %s",
                        axis->max/format->magnitude, format->units);
    }

    layout = gtk_widget_create_pango_layout(widget, "");

    /* Draw frame around false color scale */
    gc = widget->style->fg_gc[GTK_WIDGET_STATE(widget)];
    if (axis->orientation == GTK_ORIENTATION_VERTICAL) {
        gdk_draw_rectangle(widget->window, gc, FALSE, 0, 0, swidth, height - 1);
        gdk_draw_line(widget->window, gc,
                      swidth, 0, swidth + tlength, 0);
        gdk_draw_line(widget->window, gc,
                      swidth, height/2, swidth + tlength, height/2);
        gdk_draw_line(widget->window, gc,
                      swidth, height - 1, swidth + tlength, height - 1);
    }
    else {
        gdk_draw_rectangle(widget->window, gc, FALSE, 0, 0, width - 1, swidth);
        gdk_draw_line(widget->window, gc,
                      0, swidth, 0, swidth + tlength);
        gdk_draw_line(widget->window, gc,
                      width/2, swidth, width/2, swidth + tlength);
        gdk_draw_line(widget->window, gc,
                      width - 1, swidth, width - 1, swidth + tlength);
    }

    /* Draw text */
    gc = widget->style->text_gc[GTK_WIDGET_STATE(widget)];

    pango_layout_set_markup(layout,  strmax->str, strmax->len);
    pango_layout_get_pixel_extents(layout, NULL, &rect);
    if (axis->orientation == GTK_ORIENTATION_VERTICAL)
        gdk_draw_layout(widget->window, gc, off, ythickness, layout);
    else
        gdk_draw_layout(widget->window, gc, xthickness, off, layout);

    pango_layout_set_markup(layout,  strmin->str, strmin->len);
    pango_layout_get_pixel_extents(layout, NULL, &rect);
    if (axis->orientation == GTK_ORIENTATION_VERTICAL)
        gdk_draw_layout(widget->window, gc,
                        off, height - rect.height - ythickness,
                        layout);
    else
        gdk_draw_layout(widget->window, gc,
                        width - rect.width - xthickness, off,
                        layout);

    if (format)
        gwy_si_unit_value_format_free(format);
    g_object_unref(layout);
    g_string_free(strmin, TRUE);
    g_string_free(strmax, TRUE);
}

/**
 * gwy_color_axis_get_range:
 * @axis: A color axis.
 * @min: Where the range maximum should be stored (or %NULL).
 * @max: Where the range minimum should be stored (or %NULL).
 *
 * Gets the range of color axis @axis.
 **/
void
gwy_color_axis_get_range(GwyColorAxis *axis,
                         gdouble *min,
                         gdouble *max)
{
    g_return_if_fail(GWY_IS_COLOR_AXIS(axis));
    if (min)
        *min = axis->min;
    if (max)
        *max = axis->max;
}

/**
 * gwy_color_axis_set_range:
 * @axis: A color axis.
 * @min: The minimum.
 * @max: The maximum.
 *
 * Sets the range for color axis @axis to [@min, @max].
 **/
void
gwy_color_axis_set_range(GwyColorAxis *axis,
                         gdouble min,
                         gdouble max)
{
    g_return_if_fail(GWY_IS_COLOR_AXIS(axis));

    if (axis->min == MIN(min, max) && axis->max == MAX(min, max))
        return;

    axis->min = MIN(min, max);
    axis->max = MAX(min, max);
    gtk_widget_queue_draw(GTK_WIDGET(axis));
}

/**
 * gwy_color_axis_set_gradient:
 * @axis: A color axis.
 * @gradient: Name of gradient @axis should use.  It should exist.
 *
 * Sets the color gradient a color axis should use.
 **/
void
gwy_color_axis_set_gradient(GwyColorAxis *axis,
                            const gchar *gradient)
{
    GwyGradient *grad;

    g_return_if_fail(GWY_IS_COLOR_AXIS(axis));

    grad = gwy_gradients_get_gradient(gradient);
    if (grad == axis->gradient)
        return;

    g_signal_handler_disconnect(axis->gradient, axis->gradient_id);
    gwy_resource_release(GWY_RESOURCE(axis->gradient));
    axis->gradient = grad;
    gwy_resource_use(GWY_RESOURCE(axis->gradient));
    axis->gradient_id
        = g_signal_connect_swapped(axis->gradient, "data-changed",
                                   G_CALLBACK(gwy_color_axis_update), axis);

    gwy_color_axis_update(axis);
}

/**
 * gwy_color_axis_get_gradient:
 * @axis: A color axis.
 *
 * Returns the color gradient a color axis uses.
 *
 * Returns: The color gradient.
 **/
const gchar*
gwy_color_axis_get_gradient(GwyColorAxis *axis)
{
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(axis), NULL);

    return gwy_resource_get_name(GWY_RESOURCE(axis->gradient));
}

static void
gwy_color_axis_update(GwyColorAxis *axis)
{
    gwy_color_axis_adjust(axis,
                          GTK_WIDGET(axis)->allocation.width,
                          GTK_WIDGET(axis)->allocation.height);
    gtk_widget_queue_draw(GTK_WIDGET(axis));
}

/**
 * gwy_color_axis_get_si_unit:
 * @axis: A color axis.
 *
 * Returns the SI unit a color axis displays.
 *
 * Returns: The SI unit.
 **/
GwySIUnit*
gwy_color_axis_get_si_unit(GwyColorAxis *axis)
{
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(axis), NULL);

    return axis->siunit;
}

/**
 * gwy_color_axis_set_si_unit:
 * @axis: A color axis.
 * @unit: A SI unit to display next to minimum and maximum value.
 *
 * Sets the SI unit a color axis displays.
 **/
void
gwy_color_axis_set_si_unit(GwyColorAxis *axis,
                           GwySIUnit *unit)
{
    GwySIUnit *old;

    g_return_if_fail(GWY_IS_COLOR_AXIS(axis));
    g_return_if_fail(GWY_IS_SI_UNIT(unit));

    if (axis->siunit == unit)
        return;

    old = axis->siunit;
    g_object_ref(unit);
    axis->siunit = unit;
    gwy_object_unref(old);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
