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
 *  This program is distributed in the hope this it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <gtk/gtkmain.h>
#include <glib-object.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwygraph.h"
#include "gwygraphmodel.h"
#include "gwygraphcurvemodel.h"

static const GwyRGBA nice_colors[] = {
    { 0.000, 0.000, 0.000, 1.000 },    /* Black */
    { 1.000, 0.000, 0.000, 1.000 },    /* Light red */
    { 0.000, 0.784, 0.000, 1.000 },    /* Light green */
    { 0.000, 0.000, 1.000, 1.000 },    /* Medium blue */
    { 0.000, 0.650, 0.650, 1.000 },    /* Light azure */
    { 0.529, 0.216, 0.000, 1.000 },    /* Dark brown */
    { 1.000, 0.510, 0.000, 1.000 },    /* Orange */
    { 1.000, 0.784, 0.000, 1.000 },    /* Rich yellow */
    { 0.588, 0.000, 0.588, 1.000 },    /* Dark violet */
    { 1.000, 0.000, 1.000, 1.000 },    /* Pink */
/*  { 0.000, 0.784, 1.000, 1.000 }, */ /* Greenish blue */
    { 0.095, 0.351, 0.500, 1.000 },    /* Navy blue */
    { 0.232, 0.580, 0.340, 1.000 },    /* Greenish */
    { 0.510, 0.510, 0.510, 1.000 },    /* Grey */
    { 0.780, 0.000, 0.000, 1.000 },    /* Dark red */
    { 0.000, 0.510, 0.000, 1.000 },    /* Dark green */
    { 0.000, 0.000, 0.558, 1.000 },    /* Dark blue */
    { 0.000, 0.467, 0.467, 1.000 },    /* Azure */
    { 0.604, 0.367, 0.095, 1.000 },    /* Dark brown */
    { 0.810, 0.572, 0.000, 1.000 },    /* Dark orange */
    { 1.000, 0.000, 0.510, 1.000 },    /* Purpur */
    { 0.588, 0.588, 0.000, 1.000 },    /* Green-brown */
    { 0.000, 0.510, 1.000, 1.000 },    /* Light greenish blue */
    { 0.681, 0.000, 1.000, 1.000 },    /* Light violet */
};

static gint
x_data_to_pixel(GwyGraphActiveAreaSpecs *specs, gdouble data)
{
   if (!specs->log_x)
       return specs->xmin
            + (gint)((data - specs->real_xmin)
                     /(specs->real_width)*((gdouble)specs->width-1));

   return specs->xmin
       + (gint)((log10(data) - log10(specs->real_xmin))
                /((log10(specs->real_xmin + specs->real_width)
                   - log10(specs->real_xmin)))*((gdouble)specs->width-1));

}

static gint
y_data_to_pixel(GwyGraphActiveAreaSpecs *specs, gdouble data)
{
    if (!specs->log_y)
        return specs->ymin + specs->height
            - (gint)((data - specs->real_ymin)
                     /(specs->real_height)*((gdouble)specs->height-1));

    return specs->ymin + specs->height
        - (gint)((log10(data) - log10(specs->real_ymin))
                /((log10(specs->real_ymin + specs->real_height)
                   - log10(specs->real_ymin)))*((gdouble)specs->height-1));
}

/**
 * gwy_graph_draw_curve:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC graphics context
 * @specs: specifications (boundaries) of the active area of the graph
 * @curvemodel: the curve model object
 *
 * Draw a curve on the graph
 **/
void
gwy_graph_draw_curve(GdkDrawable *drawable,
                     GdkGC *gc,
                     GwyGraphActiveAreaSpecs *specs, GObject *curvemodel)
{
    gint i, x, y, pxn = 0, pyn = 0;
    GwyGraphCurveModel *cmodel;

    cmodel = GWY_GRAPH_CURVE_MODEL(curvemodel);
    for (i = 0; i < cmodel->n; i++) {
        if (i == 0) {
            x = x_data_to_pixel(specs, cmodel->xdata[i]);
            y = y_data_to_pixel(specs, cmodel->ydata[i]);
        }
        else {
            x = pxn;
            y = pyn;
        }
        if (i < cmodel->n - 1) {
            pxn = x_data_to_pixel(specs, cmodel->xdata[i + 1]);
            pyn = y_data_to_pixel(specs, cmodel->ydata[i + 1]);
        }
        if (x < (-specs->width) || x > (2*specs->width) || y < (-specs->height) || y > (2*specs->height) ||
            pxn < (-specs->width) || pxn > (2*specs->width) || pyn < (-specs->height) || pyn > (2*specs->height))
            continue;
        if (i < cmodel->n - 1
            && (cmodel->mode == GWY_GRAPH_CURVE_LINE
                || cmodel->mode == GWY_GRAPH_CURVE_LINE_POINTS))
            gwy_graph_draw_line(drawable, gc,
                                x, y,
                                pxn, pyn,
                                cmodel->line_style, cmodel->line_size,
                                &(cmodel->color));


        if ((cmodel->mode == GWY_GRAPH_CURVE_POINTS
             || cmodel->mode == GWY_GRAPH_CURVE_LINE_POINTS))
            gwy_graph_draw_point(drawable, gc,
                                 x, y,
                                 cmodel->point_type, cmodel->point_size,
                                 &(cmodel->color), FALSE);
    }
}

/**
 * gwy_graph_draw_line:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC graphics context
 * @x_from: x coordinate of the start point of the line
 * @y_from: y coordinate of the start point of the line
 * @x_to: x coordinate of the end point of the line
 * @y_to: y coordinate of the end point of the line
 * @line_style: graph line style
 * @size: point size
 * @color: point color
 *
 * Draw a line on the graph.
 **/
void
gwy_graph_draw_line(GdkDrawable *drawable, GdkGC *gc,
                    gint x_from, gint y_from,
                    gint x_to, gint y_to,
                    GdkLineStyle line_style,
                    gint size,
                    const GwyRGBA *color)
{
    GwyRGBA rgba;

    gwy_rgba_set_gdk_gc_fg(color, gc);
    rgba.r = MIN(color->g + 0.2, 1.0);
    rgba.g = MIN(color->b + 0.2, 1.0);
    rgba.b = MIN(color->r + 0.2, 1.0);
    gwy_rgba_set_gdk_gc_bg(color, gc);

    gdk_gc_set_line_attributes(gc, size,
                               line_style, GDK_CAP_BUTT, GDK_JOIN_MITER);

    gdk_draw_line(drawable, gc, x_from, y_from, x_to, y_to);
}

/**
 * gwy_graph_draw_point:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC graphics context
 * @x: x coordinate of the point
 * @y: y coordinate of the point
 * @type: graph point type
 * @size: point size
 * @color: point color
 * @clear:
 *
 * Draw a point on the graph.
 **/
void
gwy_graph_draw_point(GdkDrawable *drawable, GdkGC *gc,
                     gint x, gint y,
                     GwyGraphPointType type,
                     gint size,
                     const GwyRGBA *color,
                     G_GNUC_UNUSED gboolean clear)
{
    gint point_thickness;
    gint i, j;
    gint size_half = size/2;
    gboolean filled = FALSE;

    point_thickness = MAX(size/10, 1);

    gwy_rgba_set_gdk_gc_fg(color, gc);
    gdk_gc_set_line_attributes(gc, point_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    i = x;
    j = y;
    switch (type) {
        case GWY_GRAPH_POINT_SQUARE:
        gdk_draw_rectangle(drawable, gc, FALSE,
                           i - size_half, j - size_half,
                           2*size_half, 2*size_half);
        break;

        case GWY_GRAPH_POINT_FILLED_SQUARE:
        gdk_draw_rectangle(drawable, gc, TRUE,
                           i - size_half, j - size_half,
                           2*size_half + 1, 2*size_half + 1);
        break;

        case GWY_GRAPH_POINT_CROSS:
        gdk_draw_line(drawable, gc, i - size_half, j, i + size_half, j);
        gdk_draw_line(drawable, gc, i, j - size_half, i, j + size_half);
        break;

        case GWY_GRAPH_POINT_DISC:
        filled = TRUE;
        case GWY_GRAPH_POINT_CIRCLE:
        gdk_draw_arc(drawable, gc, filled,
                     i - size_half, j - size_half,
                     2*size_half + 1, 2*size_half + 1,
                     0, 23040);
        break;

        case GWY_GRAPH_POINT_STAR:
        gdk_draw_line(drawable, gc, i - size_half, j, i + size_half, j);
        gdk_draw_line(drawable, gc, i, j - size_half, i, j + size_half);
        case GWY_GRAPH_POINT_TIMES:
        gdk_draw_line(drawable, gc,
                      i - 3*size/8, j - 3*size/8, i + 3*size/8, j + 3*size/8);
        gdk_draw_line(drawable, gc,
                      i - 3*size/8, j + 3*size/8, i + 3*size/8, j - 3*size/8);
        break;

        case GWY_GRAPH_POINT_FILLED_TRIANGLE_UP:
        filled = TRUE;
        case GWY_GRAPH_POINT_TRIANGLE_UP:
        {
            GdkPoint vertices[] = {
                { i, j - size/GWY_SQRT3 },
                { i - size_half, j + size/GWY_SQRT3/2.0 },
                { i + size_half, j + size/GWY_SQRT3/2.0 },
            };
            gdk_draw_polygon(drawable, gc, filled,
                             vertices, G_N_ELEMENTS(vertices));
        }
        break;

        case GWY_GRAPH_POINT_FILLED_TRIANGLE_DOWN:
        filled = TRUE;
        case GWY_GRAPH_POINT_TRIANGLE_DOWN:
        {
            GdkPoint vertices[] = {
                { i, j + size/GWY_SQRT3 },
                { i - size_half, j - size/GWY_SQRT3/2.0 },
                { i + size_half, j - size/GWY_SQRT3/2.0 },
            };
            gdk_draw_polygon(drawable, gc, filled,
                             vertices, G_N_ELEMENTS(vertices));
        }
        break;

        case GWY_GRAPH_POINT_FILLED_DIAMOND:
        filled = TRUE;
        case GWY_GRAPH_POINT_DIAMOND:
        {
            GdkPoint vertices[] = {
                { i - size_half, j }, { i, j - size_half },
                { i + size_half, j }, { i, j + size_half },
            };
            gdk_draw_polygon(drawable, gc, filled,
                             vertices, G_N_ELEMENTS(vertices));
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

/**
 * gwy_graph_draw_selection_points:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC graphics context
 * @specs: specifications (boundaries) of the active area of the graph
 * @selection: a #GwySelectionGraphPoint structure
 *
 * Draw selection points on the graph
 **/
void
gwy_graph_draw_selection_points(GdkDrawable *drawable, GdkGC *gc,
                                GwyGraphActiveAreaSpecs *specs,
                                GwySelectionGraphPoint *selection)
{
    /* FIXME: use Gtk+ theme */
    static const GwyRGBA color = { 0.4, 0.4, 0.4, 1.0 };
    gint i, size;
    gdouble selection_data[2];

    size = 6;

    for (i = 0; i < gwy_selection_get_data(GWY_SELECTION(selection), NULL); i++) {
        gwy_selection_get_object(GWY_SELECTION(selection), i, selection_data);
        gwy_graph_draw_point(drawable, gc,
                             x_data_to_pixel(specs, selection_data[0]),
                             y_data_to_pixel(specs, selection_data[1]),
                             GWY_GRAPH_POINT_CROSS, size, &color, FALSE);
    }
}

/**
 * gwy_graph_draw_selection_areas:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC graphics context
 * @specs: specifications (boundaries) of the active area of the graph
 * @selection: a #GwySelectionGraphArea structure
 *
 * Draw selected area on the graph
 **/
void
gwy_graph_draw_selection_areas(GdkDrawable *drawable, GdkGC *gc,
                               GwyGraphActiveAreaSpecs *specs,
                               GwySelectionGraphArea *selection)
{
    /* FIXME: use Gtk+ theme */
    static const GwyRGBA color = { 0.8, 0.3, 0.6, 1.0 };
    gint i, n_of_areas;
    gint xmin, xmax, ymin, ymax;
    gdouble selection_areadata[4];

    n_of_areas = gwy_selection_get_data(GWY_SELECTION(selection), NULL);
    if (n_of_areas == 0)
        return;

    gwy_rgba_set_gdk_gc_fg(&color, gc);

    for (i = 0; i < n_of_areas; i++) {
        gwy_selection_get_object(GWY_SELECTION(selection), i, selection_areadata);
        xmin = x_data_to_pixel(specs, selection_areadata[0]);
        xmax = x_data_to_pixel(specs, selection_areadata[2]);
        ymin = y_data_to_pixel(specs, selection_areadata[1]);
        ymax = y_data_to_pixel(specs, selection_areadata[3]);

        gdk_draw_rectangle(drawable, gc, TRUE,
                           MIN(xmin, xmax),
                           MIN(ymin, ymax),
                           fabs(xmax - xmin), fabs(ymax - ymin));
    }
}
/**
 * gwy_graph_draw_selection_xareas:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC graphics context
 * @specs: specifications (boundaries) of the active area of the graph
 * @selection: a #GwySelectionGraph1DArea structure
 *
 * Draw selected x-area on the graph
 **/
void
gwy_graph_draw_selection_xareas(GdkDrawable *drawable, GdkGC *gc,
                               GwyGraphActiveAreaSpecs *specs,
                               GwySelectionGraph1DArea *selection)
{
    /* FIXME: use Gtk+ theme */
    static const GwyRGBA color = { 0.8, 0.3, 0.6, 1.0 };
    gint i, n_of_areas;
    gint xmin, xmax, ymin, ymax;
    gdouble selection_areadata[4];

    n_of_areas = gwy_selection_get_data(GWY_SELECTION(selection), NULL);
    if (n_of_areas == 0)
        return;

    gwy_rgba_set_gdk_gc_fg(&color, gc);

    for (i = 0; i < n_of_areas; i++) {
        gwy_selection_get_object(GWY_SELECTION(selection), i, selection_areadata);
        xmin = x_data_to_pixel(specs, selection_areadata[0]);
        xmax = x_data_to_pixel(specs, selection_areadata[1]);
        ymin = 0;
        ymax = specs->height;

        gdk_draw_rectangle(drawable, gc, TRUE,
                           MIN(xmin, xmax),
                           MIN(ymin, ymax),
                           fabs(xmax - xmin), fabs(ymax - ymin));
    }
}

/**
 * gwy_graph_draw_selection_yareas:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC graphics context
 * @specs: specifications (boundaries) of the active area of the graph
 * @selection: a #GwySelectionGraph1DArea structure
 *
 * Draw selected y-area on the graph
 **/
void
gwy_graph_draw_selection_yareas(GdkDrawable *drawable, GdkGC *gc,
                               GwyGraphActiveAreaSpecs *specs,
                               GwySelectionGraph1DArea *selection)
{
    /* FIXME: use Gtk+ theme */
    static const GwyRGBA color = { 0.8, 0.3, 0.6, 1.0 };
    gint i, n_of_areas;
    gint xmin, xmax, ymin, ymax;
    gdouble selection_areadata[4];

    n_of_areas = gwy_selection_get_data(GWY_SELECTION(selection), NULL);
    if (n_of_areas == 0)
        return;

    gwy_rgba_set_gdk_gc_fg(&color, gc);

    for (i = 0; i < n_of_areas; i++) {
        gwy_selection_get_object(GWY_SELECTION(selection), i, selection_areadata);
        xmin = 0;
        xmax = specs->width;
        ymin = y_data_to_pixel(specs, selection_areadata[0]);
        ymax = y_data_to_pixel(specs, selection_areadata[1]);

        gdk_draw_rectangle(drawable, gc, TRUE,
                           MIN(xmin, xmax),
                           MIN(ymin, ymax),
                           fabs(xmax - xmin), fabs(ymax - ymin));
    }
}


/**
 * gwy_graph_draw_selection_lines:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC graphics context
 * @specs: specifications (boundaries) of the active area of the graph
 * @selection: a #GwySelectionGraphLine structure
 * @orientation: horizontal or vertical orientation
 *
 * Draw selected lines on the graph
 **/
void
gwy_graph_draw_selection_lines(GdkDrawable *drawable, GdkGC *gc,
                               GwyGraphActiveAreaSpecs *specs,
                               GwySelectionGraphLine *selection,
                               GtkOrientation orientation)
{
    /* FIXME: use Gtk+ theme */
    static const GwyRGBA color = { 0.8, 0.3, 0.6, 1.0 };
    gint i, n_of_lines;
    gdouble selection_linedata;

    n_of_lines = gwy_selection_get_data(GWY_SELECTION(selection), NULL);
    if (n_of_lines == 0)
        return;

    gwy_rgba_set_gdk_gc_fg(&color, gc);

    for (i = 0; i < n_of_lines; i++) {
        gwy_selection_get_object(GWY_SELECTION(selection),
                                 i, &selection_linedata);
        if (orientation == GTK_ORIENTATION_HORIZONTAL)
            gwy_graph_draw_line(drawable, gc,
                                specs->xmin,
                                y_data_to_pixel(specs, selection_linedata),
                                specs->xmin + specs->width,
                                y_data_to_pixel(specs, selection_linedata),
                                GDK_LINE_SOLID, 1, &color);
        else
            gwy_graph_draw_line(drawable, gc,
                                x_data_to_pixel(specs, selection_linedata),
                                specs->ymin,
                                x_data_to_pixel(specs, selection_linedata),
                                specs->ymin + specs->height,
                                GDK_LINE_SOLID, 1, &color);
    }
}

/**
 * gwy_graph_draw_grid:
 * @drawable: a #GdkDrawable
 * @gc: a #GdkGC graphics context
 * @specs: specifications (boundaries) of the active area of the graph
 * @x_grid_data: array of grid data for the x-axis
 * @y_grid_data: array of grid data for the y-axis
 *
 * Draw array of grid lines on the graph
 **/
void
gwy_graph_draw_grid(GdkDrawable *drawable,
                    GdkGC *gc,
                    GwyGraphActiveAreaSpecs *specs,
                    GArray *x_grid_data,
                    GArray *y_grid_data)
{
    gint i;
    gdouble pos, *pvalue;
    static const GwyRGBA color = { 0.90, 0.90, 0.90, 1.0 };

    gwy_rgba_set_gdk_gc_fg(&color, gc);

    for (i = 0; i < x_grid_data->len; i++) {
        pvalue = &g_array_index(x_grid_data, gdouble, i);
        pos = y_data_to_pixel(specs, *pvalue);
        gdk_draw_line(drawable, gc,
                      specs->xmin - 1, specs->height - pos,
                      specs->xmin + specs->width + 1, specs->height - pos);
    }

    for (i = 0; i < y_grid_data->len; i++) {
        pvalue = &g_array_index(y_grid_data, gdouble, i);
        pos = x_data_to_pixel(specs, *pvalue);
        gdk_draw_line(drawable, gc,
                      pos, specs->ymin - 1, pos,
                      specs->ymin + specs->height + 1);
    }

}

/**
 * gwy_graph_get_preset_color:
 * @i: Color number, starting from 0 which is always black.  It can be any
 *     number but colors start to repeat after
 *     gwy_graph_get_n_preset_colors() colors.
 *
 * Gets a preset graph color.
 *
 * Preset colors are a set of selected colors one can use to distingush graph
 * curves when there is no reason to prefer a particular color.  Note they
 * can change between version, even their number can change.
 *
 * Returns: A constant color that must not be neither modified nor freed.
 **/
const GwyRGBA*
gwy_graph_get_preset_color(guint i)
{
    return nice_colors + (i % G_N_ELEMENTS(nice_colors));
}

/**
 * gwy_graph_get_n_preset_colors:
 *
 * Gets the number of distinct colors gwy_graph_get_preset_color() can return.
 *
 * Returns: The number of distinct colors.
 **/
guint
gwy_graph_get_n_preset_colors(void)
{
    return G_N_ELEMENTS(nice_colors);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygraphbasics
 * @title: GwyGraphBasics
 * @short_description: Common graph functions and utilities
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
