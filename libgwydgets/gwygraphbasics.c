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
#include <math.h>
#include <gtk/gtkmain.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include "gwygraph.h"
#include "gwygraphmodel.h"
#include "gwygraphcurvemodel.h"

static gint
x_data_to_pixel(GwyGraphActiveAreaSpecs *specs, gdouble data)
{
   if (!specs->log_x)
       return specs->xmin
            + (gint)((data - specs->real_xmin)
                     /(specs->real_width)*((gdouble)specs->width-1));

   return specs->xmin
       + (gint)((log10(data) - log10(specs->real_xmin))
                /((log10(specs->real_xmin + specs->real_width) - log10(specs->real_xmin)))*((gdouble)specs->width-1));
   
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
                /((log10(specs->real_ymin + specs->real_height) - log10(specs->real_ymin)))*((gdouble)specs->height-1));
}

void
gwy_graph_draw_curve(GdkDrawable *drawable,
                     GdkGC *gc,
                     GwyGraphActiveAreaSpecs *specs, GObject *curvemodel)
{
    gint i, x, y, pxn = 0, pyn = 0;
    GwyGraphCurveModel *cmodel;

    cmodel = GWY_GRAPH_CURVE_MODEL(curvemodel);
    for (i = 0; i < (cmodel->n); i++) {
        if (i == 0) {
            x = x_data_to_pixel(specs, cmodel->xdata[i]);
            y = y_data_to_pixel(specs, cmodel->ydata[i]);
        }
        else {
            x = pxn;
            y = pyn;
        }
        if (i < (cmodel->n - 1)) {
            pxn = x_data_to_pixel(specs, cmodel->xdata[i + 1]);
            pyn = y_data_to_pixel(specs, cmodel->ydata[i + 1]);
        }
        if (i < (cmodel->n - 1)
            && (cmodel->type == GWY_GRAPH_CURVE_LINE
                || cmodel->type == GWY_GRAPH_CURVE_LINE_POINTS))
            gwy_graph_draw_line(drawable, gc,
                                x, y,
                                pxn, pyn,
                                cmodel->line_style, cmodel->line_size,
                                &(cmodel->color));


        if ((cmodel->type == GWY_GRAPH_CURVE_POINTS
             || cmodel->type == GWY_GRAPH_CURVE_LINE_POINTS))
            gwy_graph_draw_point(drawable, gc,
                                 x, y,
                                 cmodel->point_type, cmodel->point_size,
                                 &(cmodel->color), FALSE);
    }
}


void
gwy_graph_draw_line(GdkDrawable *drawable, GdkGC *gc,
                    gint x_from, gint y_from,
                    gint x_to, gint y_to,
                    GdkLineStyle line_style,
                    gint size,
                    const GwyRGBA *color)
{
    GwyRGBA rgba;

    /* XXX and who will free this? XXX */
    if (gc == NULL)
        gc = gdk_gc_new(drawable);

    gwy_rgba_set_gdk_gc_fg(color, gc);
    rgba.r = MIN(color->g + 0.2, 1.0);
    rgba.g = MIN(color->b + 0.2, 1.0);
    rgba.b = MIN(color->r + 0.2, 1.0);
    gwy_rgba_set_gdk_gc_bg(color, gc);

    gdk_gc_set_line_attributes(gc, size,
                               line_style, GDK_CAP_BUTT, GDK_JOIN_MITER);

    gdk_draw_line(drawable, gc, x_from, y_from, x_to, y_to);
}


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

    /* XXX and who will free this? XXX */
    if (gc == NULL)
        gc = gdk_gc_new(drawable);

    point_thickness = MAX(size/10, 1);

    gwy_rgba_set_gdk_gc_fg(color, gc);
    gdk_gc_set_line_attributes(gc, point_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    i = x;
    j = y;
    switch (type) {
        case GWY_GRAPH_POINT_SQUARE:
        gdk_draw_line(drawable, gc,
                        i - size_half, j - size_half,
                        i + size_half, j - size_half);
        gdk_draw_line(drawable, gc,
                        i + size_half, j - size_half,
                        i + size_half, j + size_half);
        gdk_draw_line(drawable, gc,
                        i + size_half, j + size_half,
                        i - size_half, j + size_half);
        gdk_draw_line(drawable, gc,
                        i - size_half, j + size_half,
                        i - size_half, j - size_half);
        break;

        case GWY_GRAPH_POINT_CROSS:
        gdk_draw_line(drawable, gc, i - size_half, j, i + size_half, j);
        gdk_draw_line(drawable, gc, i, j - size_half, i, j + size_half);
        break;

        case GWY_GRAPH_POINT_CIRCLE:
        gdk_draw_arc(drawable, gc, FALSE,
                        i - size_half, j - size_half,
                        size, size,
                        0, 23040);
        break;

        case GWY_GRAPH_POINT_STAR:
        gdk_draw_line(drawable, gc,
                        i - size_half, j - size_half,
                        i + size_half, j + size_half);
        gdk_draw_line(drawable, gc,
                        i + size_half, j - size_half,
                        i - size_half, j + size_half);
        gdk_draw_line(drawable, gc, i, j - size_half, i, j + size_half);
        gdk_draw_line(drawable, gc, i - size_half, j, i + size_half, j);
        break;

        case GWY_GRAPH_POINT_TIMES:
        gdk_draw_line(drawable, gc,
                        i - size_half, j - size_half,
                        i + size_half, j + size_half);
        gdk_draw_line(drawable, gc,
                        i + size_half, j - size_half,
                        i - size_half, j + size_half);
        break;

        case GWY_GRAPH_POINT_TRIANGLE_UP:
        gdk_draw_line(drawable, gc,
                        i, j - size * 0.57,
                        i - size_half, j + size * 0.33);
        gdk_draw_line(drawable, gc,
                        i - size_half, j + size * 0.33,
                        i + size_half, j + size * 0.33);
        gdk_draw_line(drawable, gc,
                        i + size_half, j + size * 0.33,
                        i, j - size * 0.33);
        break;

        case GWY_GRAPH_POINT_TRIANGLE_DOWN:
        gdk_draw_line(drawable, gc,
                        i, j + size * 0.57,
                        i - size_half, j - size * 0.33);
        gdk_draw_line(drawable, gc,
                        i - size_half, j - size * 0.33,
                        i + size_half, j - size * 0.33);
        gdk_draw_line(drawable, gc,
                        i + size_half, j - size * 0.33, i,
                        j + size * 0.33);
        break;

        case GWY_GRAPH_POINT_DIAMOND:
        gdk_draw_line(drawable, gc, i - size_half, j, i, j - size_half);
        gdk_draw_line(drawable, gc, i, j - size_half, i + size_half, j);
        gdk_draw_line(drawable, gc, i + size_half, j, i, j + size_half);
        gdk_draw_line(drawable, gc, i, j + size_half, i - size_half, j);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

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

    /* XXX and who will free this? XXX */
    if (gc == NULL)
        gc = gdk_gc_new(drawable);

    for (i = 0; i < GWY_SELECTION(selection)->n; i++) {
        gwy_selection_get_object(GWY_SELECTION(selection), i, selection_data);
        gwy_graph_draw_point(drawable, gc,
                             x_data_to_pixel(specs, selection_data[0]),
                             y_data_to_pixel(specs, selection_data[1]),
                             GWY_GRAPH_POINT_CROSS, size, &color, FALSE);
    }
}

void
gwy_graph_draw_selection_areas(GdkDrawable *drawable, GdkGC *gc,
                               GwyGraphActiveAreaSpecs *specs,
                               GwyGraphDataArea *data_areas,
                               gint n_of_areas)
{
    /* FIXME: use Gtk+ theme */
    static const GwyRGBA color = { 0.8, 0.3, 0.6, 1.0 };
    gint i;
    gint xmin, xmax, ymin, ymax;

    if (n_of_areas == 0 || data_areas == NULL)
        return;

    /* XXX and who will free this? XXX */
    if (gc == NULL)
        gc = gdk_gc_new(drawable);

    gwy_rgba_set_gdk_gc_fg(&color, gc);

    for (i = 0; i < n_of_areas; i++) {
        xmin = x_data_to_pixel(specs, data_areas[i].xmin);
        xmax = x_data_to_pixel(specs, data_areas[i].xmax);
        ymin = y_data_to_pixel(specs, data_areas[i].ymin);
        ymax = y_data_to_pixel(specs, data_areas[i].ymax);

        gdk_draw_rectangle(drawable, gc, TRUE,
                           MIN(xmin, xmax),
                           MIN(ymin, ymax),
                           fabs(xmax - xmin), fabs(ymax - ymin));
    }
}

void
gwy_graph_draw_selection_lines(GdkDrawable *drawable, GdkGC *gc,
                               GwyGraphActiveAreaSpecs *specs,
                               double *data_lines,
                               gint n_of_lines, GtkOrientation orientation)
{
    /* FIXME: use Gtk+ theme */
    static const GwyRGBA color = { 0.8, 0.3, 0.6, 1.0 };
    gint i;

    if (n_of_lines == 0 || data_lines == NULL)
        return;

    /* XXX and who will free this? XXX */
    if (gc == NULL)
        gc = gdk_gc_new(drawable);

    gwy_rgba_set_gdk_gc_fg(&color, gc);

    for (i = 0; i < n_of_lines; i++) {
        if (orientation == GTK_ORIENTATION_HORIZONTAL)
            gwy_graph_draw_line(drawable, gc,
                                specs->xmin,
                                y_data_to_pixel(specs, data_lines[i]),
                                specs->xmin + specs->width,
                                y_data_to_pixel(specs, data_lines[i]),
                                GDK_LINE_SOLID, 1, &color);
        else
            gwy_graph_draw_line(drawable, gc,
                                x_data_to_pixel(specs, data_lines[i]),
                                specs->ymin,
                                x_data_to_pixel(specs, data_lines[i]),
                                specs->ymin + specs->height,
                                GDK_LINE_SOLID, 1, &color);
    }
}

void 
gwy_graph_draw_grid(GdkDrawable *drawable, 
                    GdkGC *gc,
                    GwyGraphActiveAreaSpecs *specs,
                    GArray *x_grid_data,
                    GArray *y_grid_data)
{
    gint i;
    gdouble pos, *pvalue;
    static const GwyRGBA color = { 0.75, 0.75, 0.75, 1.0 };
    
    gwy_rgba_set_gdk_gc_fg(&color, gc);
    
    for (i = 0; i < x_grid_data->len; i++) {
        pvalue = &g_array_index(x_grid_data, gdouble, i);
        pos = y_data_to_pixel(specs, *pvalue);
        gdk_draw_line(drawable, gc, specs->xmin - 1, specs->height - pos, specs->xmin + specs->width + 1, specs->height - pos);
    }

    for (i = 0; i < y_grid_data->len; i++) {
        pvalue = &g_array_index(y_grid_data, gdouble, i);
        pos = x_data_to_pixel(specs, *pvalue);
        gdk_draw_line(drawable, gc, pos, specs->ymin - 1, pos, specs->ymin + specs->height + 1);
    }
    
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
