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
 * Modified by Yeti 2003-2004.  In fact, rewritten, now includes a single
 * tick'n'label design function, and Gwy[HV]Rulers only do the actual
 * drawing at computed positions.  GtkMetric was removed, replaced with
 * a scale-free solution and later support for GwySIUnit was added.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwyddion.h>
#include "gwyruler.h"
#include "gwydgettypes.h"

enum {
    PROP_0,
    PROP_LOWER,
    PROP_UPPER,
    PROP_POSITION,
    PROP_MAX_SIZE,
    PROP_UNITS_PLACEMENT,
};

static void    gwy_ruler_realize             (GtkWidget      *widget);
static void    gwy_ruler_unrealize           (GtkWidget      *widget);
static void    gwy_ruler_size_allocate       (GtkWidget      *widget,
                                              GtkAllocation  *allocation);
static gint    gwy_ruler_expose              (GtkWidget      *widget,
                                              GdkEventExpose *event);
static void    gwy_ruler_make_pixmap         (GwyRuler       *ruler);
static void    gwy_ruler_set_property        (GObject        *object,
                                              guint           prop_id,
                                              const GValue   *value,
                                              GParamSpec     *pspec);
static void    gwy_ruler_get_property        (GObject        *object,
                                              guint           prop_id,
                                              GValue         *value,
                                              GParamSpec     *pspec);
static void    gwy_ruler_update_value_format (GwyRuler       *ruler);


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

    class->draw_ticks = NULL;
    class->draw_pos = NULL;

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

    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
                                    &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, ruler);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    gwy_ruler_make_pixmap(ruler);
}

static void
gwy_ruler_unrealize(GtkWidget *widget)
{
    GwyRuler *ruler = GWY_RULER(widget);

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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
