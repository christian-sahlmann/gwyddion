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

#include <stdio.h>
#include <gtk/gtk.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <glib-object.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwymath.h>

#include "gwycoloraxis.h"

#define GWY_COLOR_AXIS_TYPE_NAME "GwyColorAxis"


/* Forward declarations - widget related*/
static void     gwy_color_axis_class_init         (GwyColorAxisClass *klass);
static void     gwy_color_axis_init               (GwyColorAxis *axis);
static void     gwy_color_axis_destroy            (GtkObject *object);
static void     gwy_color_axis_finalize           (GObject *object);

static void     gwy_color_axis_realize            (GtkWidget *widget);
static void     gwy_color_axis_unrealize          (GtkWidget *widget);
static void     gwy_color_axis_size_request       (GtkWidget *widget,
                                                   GtkRequisition *requisition);
static void     gwy_color_axis_size_allocate      (GtkWidget *widget,
                                                   GtkAllocation *allocation);
static gboolean gwy_color_axis_expose             (GtkWidget *widget,
                                                   GdkEventExpose *event);
static gboolean gwy_color_axis_button_press       (GtkWidget *widget,
                                                   GdkEventButton *event);
static gboolean gwy_color_axis_button_release     (GtkWidget *widget,
                                                   GdkEventButton *event);
static void     gwy_color_axis_adjust             (GwyColorAxis *axis,
                                                   gint width,
                                                   gint height);
static void     gwy_color_axis_draw_label         (GtkWidget *widget);
static void     gwy_color_axis_update             (GwyColorAxis *axis);

/* Local data */
static GtkWidgetClass *parent_class = NULL;

GType
gwy_color_axis_get_type(void)
{
    static GType gwy_color_axis_type = 0;

    if (!gwy_color_axis_type) {
        static const GTypeInfo gwy_color_axis_info = {
            sizeof(GwyColorAxisClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_color_axis_class_init,
            NULL,
            NULL,
            sizeof(GwyColorAxis),
            0,
            (GInstanceInitFunc)gwy_color_axis_init,
            NULL,
        };
        gwy_debug("");
        gwy_color_axis_type = g_type_register_static(GTK_TYPE_WIDGET,
                                                      GWY_COLOR_AXIS_TYPE_NAME,
                                                      &gwy_color_axis_info,
                                                      0);
    }

    return gwy_color_axis_type;
}

static void
gwy_color_axis_class_init(GwyColorAxisClass *klass)
{


    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug("");
    object_class = GTK_OBJECT_CLASS(klass);
    widget_class = GTK_WIDGET_CLASS(klass);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_color_axis_finalize;

    object_class->destroy = gwy_color_axis_destroy;

    widget_class->realize = gwy_color_axis_realize;
    widget_class->expose_event = gwy_color_axis_expose;
    widget_class->size_request = gwy_color_axis_size_request;
    widget_class->unrealize = gwy_color_axis_unrealize;
    widget_class->size_allocate = gwy_color_axis_size_allocate;
    widget_class->button_press_event = gwy_color_axis_button_press;
    widget_class->button_release_event = gwy_color_axis_button_release;

}

static void
gwy_color_axis_init(GwyColorAxis *axis)
{
    gwy_debug("");

    axis->orientation = GTK_ORIENTATION_VERTICAL;
    axis->tick_length = 5;
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

    gwy_debug("");

    axis = gtk_type_new(gwy_color_axis_get_type());
    axis->orientation = orientation;
    /* TODO: use some font properties, at least */
    if (orientation == GTK_ORIENTATION_VERTICAL)
        axis->textarea = 70;
    else
        axis->textarea = 20;
    axis->min = min;
    axis->max = max;

    /* XXX */
    axis->font = pango_font_description_new();
    pango_font_description_set_family(axis->font, "Helvetica");
    pango_font_description_set_style(axis->font, PANGO_STYLE_NORMAL);
    pango_font_description_set_variant(axis->font, PANGO_VARIANT_NORMAL);
    pango_font_description_set_weight(axis->font, PANGO_WEIGHT_NORMAL);
    pango_font_description_set_size(axis->font, 10*PANGO_SCALE);

    axis->gradient = gwy_gradients_get_gradient(GWY_GRADIENT_DEFAULT);
    g_object_ref(axis->gradient);
    axis->siunit = GWY_SI_UNIT(gwy_si_unit_new("m"));

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
    GwyColorAxis *axis;

    gwy_debug(" ");

    axis = (GwyColorAxis*)g_object_new(GWY_TYPE_COLOR_AXIS, NULL);
    axis->orientation = orientation;
    /* TODO: use some font properties, at least */
    if (orientation == GTK_ORIENTATION_VERTICAL)
        axis->textarea = 70;
    else
        axis->textarea = 20;

    axis->min = 0.0;
    axis->max = 1.0;

    axis->font = pango_font_description_from_string("Helvetica 10");
    axis->gradient = gwy_gradients_get_gradient(GWY_GRADIENT_DEFAULT);
    g_object_ref(axis->gradient);
    axis->siunit = GWY_SI_UNIT(gwy_si_unit_new("m"));

    return GTK_WIDGET(axis);
}

static void
gwy_color_axis_finalize(GObject *object)
{
    GwyColorAxis *axis;

    gwy_debug("");

    axis = (GwyColorAxis*)object;
    gwy_object_unref(axis->pixbuf);
    g_object_unref(axis->siunit);
    g_object_unref(axis->gradient);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_color_axis_destroy(GtkObject *object)
{
    GwyColorAxis *axis;

    gwy_debug("");

    axis = (GwyColorAxis*)object;
    g_signal_handlers_disconnect_matched(axis->gradient, G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, axis);

    GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

static void
gwy_color_axis_unrealize(GtkWidget *widget)
{
    gwy_debug("");

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static void
gwy_color_axis_realize(GtkWidget *widget)
{
    GwyColorAxis *axis;
    GdkWindowAttr attributes;
    gint attributes_mask;
    GtkStyle *s;

    gwy_debug("");

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

    /*set backgroun for white forever*/
    s = gtk_style_copy(widget->style);
    s->bg_gc[0] =
    s->bg_gc[1] =
    s->bg_gc[2] =
    s->bg_gc[3] =
    s->bg_gc[4] = widget->style->white_gc;
    s->bg[0] =
    s->bg[1] =
    s->bg[2] =
    s->bg[3] =
    s->bg[4] = widget->style->white;

    gtk_style_set_background(s, widget->window, GTK_STATE_NORMAL);

    /*compute axis*/
    gwy_color_axis_update(axis);
}

static void
gwy_color_axis_size_request(GtkWidget *widget,
                             GtkRequisition *requisition)
{
    GwyColorAxis *axis;
    axis = GWY_COLOR_AXIS(widget);

    gwy_debug("");

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

    gwy_debug("");

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


    gwy_debug("");

    gwy_object_unref(axis->pixbuf);
    axis->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                  width, height);
    gwy_debug_objects_creation(G_OBJECT(axis->pixbuf));

    /*render pixbuf according to orientation*/
    pixels = gdk_pixbuf_get_pixels(axis->pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(axis->pixbuf);
    samples = gwy_gradient_get_samples(axis->gradient, &palsize);

    if (axis->orientation == GTK_ORIENTATION_VERTICAL) {
        cor = (palsize-1.0)/height;
        for (i = 0; i < height; i++) {
            line = pixels + i*rowstride;
            dval = (gint)((height-i-1)*cor + 0.5);
            for (j = 0; j < width*height; j += height) {
                s = samples + 4*dval;
                *(line++) = *(s++);
                *(line++) = *(s++);
                *(line++) = *s;
            }
        }
    }
    if (axis->orientation == GTK_ORIENTATION_HORIZONTAL) {
        cor = (palsize-1.0)/width;
        for (i = 0; i < height; i++) {
            line = pixels + i*rowstride;
            for (j = 0; j < width*height; j += height) {
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
    GdkGC *mygc;

    gwy_debug("");

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    axis = GWY_COLOR_AXIS(widget);

    gdk_window_clear_area(widget->window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);

    mygc = gdk_gc_new(widget->window);

    if (axis->orientation == GTK_ORIENTATION_HORIZONTAL)
        gdk_pixbuf_render_to_drawable(axis->pixbuf, widget->window, mygc, 0,
                                  0,
                                  0,
                                  axis->textarea,
                                  widget->allocation.width,
                                  widget->allocation.height
                                    - axis->textarea,
                                  GDK_RGB_DITHER_NONE,
                                  0,
                                  0);
    else
        gdk_pixbuf_render_to_drawable(axis->pixbuf, widget->window, mygc, 0,
                                  0,
                                  0,
                                  0,
                                  widget->allocation.width - axis->textarea,
                                  widget->allocation.height,
                                  GDK_RGB_DITHER_NONE,
                                  0,
                                  0);

    g_object_unref((GObject *)mygc);

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
    GdkGC *mygc;
    PangoRectangle rect;

    gwy_debug("");
    mygc = gdk_gc_new(widget->window);

    axis = GWY_COLOR_AXIS(widget);


    /*compute minimum and maximum numbers*/
    strmax = g_string_new(" ");
    if (axis->max == 0) {
        if (axis->min == 0)
            g_string_printf(strmax, "0.0");
        else {
            format = gwy_si_unit_get_format(axis->siunit, axis->max, NULL);
            g_string_printf(strmax, "0.0 ");
            g_string_append(strmax, format->units);
        }
    }
    else {
        format = gwy_si_unit_get_format(axis->siunit, axis->max, NULL);
        g_string_printf(strmax, "%3.1f ", axis->max/format->magnitude);
        g_string_append(strmax, format->units);
    }


    strmin = g_string_new(" ");
    if (axis->min == 0) {
        if (axis->max == 0)
            g_string_printf(strmin, "0.0");
        else {
            format = gwy_si_unit_get_format(axis->siunit, axis->max, format);
            g_string_printf(strmin, "0.0 ");
            g_string_append(strmin, format->units);
        }
    }
    else {
        /*yes, realy axis->max*/
        format = gwy_si_unit_get_format(axis->siunit, axis->max, format);
        g_string_printf(strmin, "%3.1f ", axis->min/format->magnitude);
        g_string_append(strmin, format->units);
    }



    if (axis->orientation == GTK_ORIENTATION_VERTICAL) {
        /*draw frame around axis*/
        gdk_draw_rectangle(widget->window, mygc, 0,
                           0,
                           0,
                           widget->allocation.width - axis->textarea,
                           widget->allocation.height - 1);

        gdk_draw_line(widget->window, mygc,
                      widget->allocation.width - axis->textarea,
                      0,
                      widget->allocation.width - axis->textarea
                          + axis->tick_length,
                      0);

        gdk_draw_line(widget->window, mygc,
                      widget->allocation.width - axis->textarea,
                      widget->allocation.height/2,
                      widget->allocation.width - axis->textarea
                          + axis->tick_length,
                      widget->allocation.height/2);

        gdk_draw_line(widget->window, mygc,
                      widget->allocation.width - axis->textarea,
                      widget->allocation.height - 1,
                      widget->allocation.width - axis->textarea
                          + axis->tick_length,
                      widget->allocation.height - 1);


        /*draw text*/
        layout = gtk_widget_create_pango_layout(widget, "");
        pango_layout_set_font_description(layout, axis->font);

        pango_layout_set_markup(layout,  strmax->str, strmax->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);
        gdk_draw_layout(widget->window, mygc,
                        widget->allocation.width - axis->textarea + 2,
                        2, layout);

        pango_layout_set_markup(layout,  strmin->str, strmin->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);
        gdk_draw_layout(widget->window, mygc,
                        widget->allocation.width - axis->textarea + 2,
                        widget->allocation.height - rect.height - 2, layout);
    }
    else {
        /*draw frame around axis*/
        gdk_draw_rectangle(widget->window, mygc, FALSE,
                           0,
                           axis->textarea,
                           widget->allocation.width - 1,
                           widget->allocation.height - 1);

        gdk_draw_line(widget->window, mygc,
                      0,
                      axis->textarea - axis->tick_length,
                      0,
                      axis->textarea);

        gdk_draw_line(widget->window, mygc,
                      widget->allocation.width/2,
                      axis->textarea - axis->tick_length,
                      widget->allocation.width/2,
                      axis->textarea);

        gdk_draw_line(widget->window, mygc,
                      widget->allocation.width - 1,
                      axis->textarea - axis->tick_length,
                      widget->allocation.width - 1,
                      axis->textarea);


        /*draw text*/
        layout = gtk_widget_create_pango_layout(widget, "");
        pango_layout_set_font_description(layout, axis->font);

        pango_layout_set_markup(layout,  strmin->str, strmin->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);
        gdk_draw_layout(widget->window, mygc, 2,
                        axis->textarea - rect.height - 2, layout);

        pango_layout_set_markup(layout,  strmax->str, strmax->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);
        gdk_draw_layout(widget->window, mygc,
                        widget->allocation.width - rect.width - 2,
                        axis->textarea - rect.height - 2, layout);

    }
    g_object_unref(mygc);
}



static gboolean
gwy_color_axis_button_press(GtkWidget *widget,
                             GdkEventButton *event)
{
    GwyColorAxis *axis;

    gwy_debug("");

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    axis = GWY_COLOR_AXIS(widget);


    return FALSE;
}

static gboolean
gwy_color_axis_button_release(GtkWidget *widget,
                               GdkEventButton *event)
{
    GwyColorAxis *axis;

    gwy_debug("");

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    axis = GWY_COLOR_AXIS(widget);


    return FALSE;
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
    axis->min = min;
    axis->max = max;
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
    GwyGradient *grad, *old;

    g_return_if_fail(GWY_IS_COLOR_AXIS(axis));

    grad = gwy_gradients_get_gradient(gradient);
    if (!grad || grad == axis->gradient)
        return;

    old = axis->gradient;
    g_signal_handlers_disconnect_matched(axis->gradient, G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, axis);
    g_object_ref(grad);
    axis->gradient = grad;
    g_signal_connect_swapped(axis->gradient, "data_changed",
                             G_CALLBACK(gwy_color_axis_update), axis);
    g_object_unref(old);

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

    return gwy_gradient_get_name(axis->gradient);
}

static void
gwy_color_axis_update(GwyColorAxis *axis)
{
    g_return_if_fail(GWY_IS_COLOR_AXIS(axis));
    gwy_color_axis_adjust(axis,
                          GTK_WIDGET(axis)->allocation.width,
                          GTK_WIDGET(axis)->allocation.height);
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
 * gwy_color_axis_set_unit:
 * @axis: A color axis.
 * @unit: An SI unit.
 *
 * Sets the SI unit a color axis should display next to values.
 **/
void
gwy_color_axis_set_unit(GwyColorAxis *axis,
                        GwySIUnit *unit)
{
    gwy_color_axis_set_si_unit(axis, unit);
}

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
