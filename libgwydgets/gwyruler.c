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

#include "gwyruler.h"

#define _(x) x

enum {
    PROP_0,
    PROP_LOWER,
    PROP_UPPER,
    PROP_POSITION,
    PROP_MAX_SIZE,
    PROP_UNITS_PLACEMENT,
};

static void gwy_ruler_class_init    (GwyRulerClass  *klass);
static void gwy_ruler_init          (GwyRuler       *ruler);
static void gwy_ruler_realize       (GtkWidget      *widget);
static void gwy_ruler_unrealize     (GtkWidget      *widget);
static void gwy_ruler_size_allocate (GtkWidget      *widget,
                                     GtkAllocation  *allocation);
static gint gwy_ruler_expose        (GtkWidget      *widget,
                                     GdkEventExpose *event);
static void gwy_ruler_make_pixmap   (GwyRuler       *ruler);
static void gwy_ruler_set_property  (GObject        *object,
                                     guint           prop_id,
                                     const GValue   *value,
                                     GParamSpec     *pspec);
static void gwy_ruler_get_property  (GObject        *object,
                                     guint           prop_id,
                                     GValue         *value,
                                     GParamSpec     *pspec);

static GtkWidgetClass *parent_class;

static const GwyRulerMetric ruler_metrics[] = {
    {"Pixels", "µm", 1.0, { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000 }, { 1, 5, 10, 50, 100 }},
    {"Inches", "in", 72.0, { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 }, { 1, 2, 4, 8, 16 }},
    {"Centimeters", "cm", 28.35, { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000 }, { 1, 5, 10, 50, 100 }},
};


GType
gwy_ruler_get_type(void)
{
    static GType ruler_type = 0;

    if (!ruler_type) {
        static const GTypeInfo ruler_info = {
            sizeof(GwyRulerClass),
            NULL,           /* base_init */
            NULL,           /* base_finalize */
            (GClassInitFunc)gwy_ruler_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof(GwyRuler),
            0,              /* n_preallocs */
            (GInstanceInitFunc)gwy_ruler_init,
            NULL,
        };

        ruler_type = g_type_register_static(GTK_TYPE_WIDGET, "GwyRuler",
                                            &ruler_info, 0);
    }

    return ruler_type;
}

static void
gwy_ruler_class_init(GwyRulerClass *class)
{
    GObjectClass   *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(class);
    widget_class = (GtkWidgetClass*)class;

    parent_class = g_type_class_peek_parent(class);

    gobject_class->set_property = gwy_ruler_set_property;
    gobject_class->get_property = gwy_ruler_get_property;

    widget_class->realize = gwy_ruler_realize;
    widget_class->unrealize = gwy_ruler_unrealize;
    widget_class->size_allocate = gwy_ruler_size_allocate;
    widget_class->expose_event = gwy_ruler_expose;

    class->draw_ticks = NULL;
    class->draw_pos = NULL;

    g_object_class_install_property(gobject_class,
                                    PROP_LOWER,
                                    g_param_spec_double("lower",
                                                        _("Lower"),
                                                        _("Lower limit of ruler"),
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_UPPER,
                                    g_param_spec_double("upper",
                                                        _("Upper"),
                                                        _("Upper limit of ruler"),
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_POSITION,
                                    g_param_spec_double("position",
                                                        _("Position"),
                                                        _("Position of mark on the ruler"),
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_MAX_SIZE,
                                    g_param_spec_double("max_size",
                                                        _("Max Size"),
                                                        _("Maximum size of the ruler"),
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE));

    /* FIXME: in fact it's an enum... */
    g_object_class_install_property(gobject_class,
                                    PROP_UNITS_PLACEMENT,
                                    g_param_spec_uint("units_placement",
                                                      _("Units Placement"),
                                                      _("The placement of units on the ruler, if any"),
                                                      0,
                                                      GWY_UNITS_PLACEMENT_AT_ZERO,
                                                      GWY_UNITS_PLACEMENT_NONE,
                                                      G_PARAM_READWRITE));
}

static void
gwy_ruler_init(GwyRuler *ruler)
{
    ruler->backing_store = NULL;
    ruler->non_gr_exp_gc = NULL;
    ruler->xsrc = 0;
    ruler->ysrc = 0;
    ruler->slider_size = 0;
    ruler->lower = 0;
    ruler->upper = 0;
    ruler->position = 0;
    ruler->max_size = 0;
    ruler->units_placement = GWY_UNITS_PLACEMENT_NONE;

    gwy_ruler_set_metric(ruler, GTK_PIXELS);
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

void
gwy_ruler_set_metric(GwyRuler      *ruler,
                     GtkMetricType  metric)
{
    g_return_if_fail(GWY_IS_RULER(ruler));

    ruler->metric = (GwyRulerMetric*)&ruler_metrics[metric];

    if (GTK_WIDGET_DRAWABLE(ruler))
        gtk_widget_queue_draw(GTK_WIDGET(ruler));
}

/**
 * gwy_ruler_get_metric:
 * @ruler: a #GwyRuler
 *
 * Gets the units used for a #GwyRuler. See gwy_ruler_set_metric().
 *
 * Return value: the units currently used for @ruler
 **/
GtkMetricType
gwy_ruler_get_metric(GwyRuler *ruler)
{
    gint i;

    g_return_val_if_fail(GWY_IS_RULER(ruler), 0);

    for (i = 0; i < G_N_ELEMENTS(ruler_metrics); i++)
        if (ruler->metric == &ruler_metrics[i])
            return i;

    g_assert_not_reached();

    return 0;
}

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

    if (GTK_WIDGET_DRAWABLE(ruler))
        gtk_widget_queue_draw(GTK_WIDGET(ruler));
}

/**
 * gwy_ruler_get_range:
 * @ruler: a #GwyRuler
 * @lower: location to store lower limit of the ruler, or %NULL
 * @upper: location to store upper limit of the ruler, or %NULL
 * @position: location to store the current position of the mark on the ruler, or %NULL
 * @max_size: location to store the maximum size of the ruler used when calculating
 *            the space to leave for the text, or %NULL.
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

void
gwy_ruler_set_units_placement(GwyRuler *ruler,
                              GwyUnitsPlacement placement)
{
    g_return_if_fail(GWY_IS_RULER(ruler));
    placement = CLAMP(placement,
                      GWY_UNITS_PLACEMENT_NONE,
                      GWY_UNITS_PLACEMENT_AT_ZERO);
    if (ruler->units_placement == placement)
        return;

    g_object_freeze_notify(G_OBJECT(ruler));
    ruler->units_placement = placement;
    g_object_notify(G_OBJECT(ruler), "units_placement");
    g_object_thaw_notify(G_OBJECT(ruler));

    if (GTK_WIDGET_DRAWABLE(ruler))
        gtk_widget_queue_draw(GTK_WIDGET(ruler));
}

GwyUnitsPlacement
gwy_ruler_get_units_placement(GwyRuler *ruler)
{
    g_return_val_if_fail(GWY_IS_RULER(ruler), GWY_UNITS_PLACEMENT_NONE);
    return ruler->units_placement;
}

void
gwy_ruler_draw_ticks(GwyRuler *ruler)
{
    g_return_if_fail(GWY_IS_RULER(ruler));

    if (GWY_RULER_GET_CLASS(ruler)->draw_ticks)
        GWY_RULER_GET_CLASS(ruler)->draw_ticks(ruler);
}

void
gwy_ruler_draw_pos(GwyRuler *ruler)
{
    g_return_if_fail(GWY_IS_RULER(ruler));

    if (GWY_RULER_GET_CLASS(ruler)->draw_pos)
        GWY_RULER_GET_CLASS(ruler)->draw_pos(ruler);
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

    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, ruler);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_ACTIVE);

    gwy_ruler_make_pixmap(ruler);
}

static void
gwy_ruler_unrealize(GtkWidget *widget)
{
    GwyRuler *ruler = GWY_RULER(widget);

    if (ruler->backing_store)
        g_object_unref(ruler->backing_store);
    if (ruler->non_gr_exp_gc)
        g_object_unref(ruler->non_gr_exp_gc);

    ruler->backing_store = NULL;
    ruler->non_gr_exp_gc = NULL;

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        (GTK_WIDGET_CLASS(parent_class)->unrealize)(widget);
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

        gwy_ruler_make_pixmap(ruler);
    }
}

static gint
gwy_ruler_expose(GtkWidget      *widget,
                 GdkEventExpose *event)
{
    GwyRuler *ruler;

    if (GTK_WIDGET_DRAWABLE(widget)) {
        ruler = GWY_RULER(widget);

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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
