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
    return specs->xmin + (gint)((data - specs->real_xmin)
         /(specs->real_width)*((gdouble)specs->width-1));
}

static gint 
y_data_to_pixel(GwyGraphActiveAreaSpecs *specs, gdouble data)
{
    return specs->ymin + specs->height - (gint)((data - specs->real_ymin)
         /(specs->real_height)*((gdouble)specs->height-1));
}

void
gwy_graph_draw_curve (GdkDrawable *drawable,
                        GdkGC *gc,
                        GwyGraphActiveAreaSpecs *specs,
                        GObject *curvemodel)
{
    gint i, x, y, pxn=0, pyn=0;
    GwyGraphCurveModel *cmodel;
    cmodel = GWY_GRAPH_CURVE_MODEL(curvemodel);
    for (i=0; i<(cmodel->n); i++)
    {
        if (i == 0)
        {
            x = x_data_to_pixel(specs, cmodel->xdata[i]);
            y = y_data_to_pixel(specs, cmodel->ydata[i]);
        }
        else
        {
            x = pxn;
            y = pyn;
        }
        if (i<(cmodel->n-1))
        {
            pxn = x_data_to_pixel(specs, cmodel->xdata[i+1]);
            pyn = y_data_to_pixel(specs, cmodel->ydata[i+1]);
        }
        if (i<(cmodel->n-1) 
            && (cmodel->type == GWY_GRAPH_CURVE_LINE 
                || cmodel->type == GWY_GRAPH_CURVE_LINE_POINTS))
                 gwy_graph_draw_line(drawable, gc,
                                  x,
                                  y,
                                  pxn,
                                  pyn,
                                  cmodel->line_style, cmodel->line_size,
                                  &(cmodel->color));
             
         
        if ((cmodel->type == GWY_GRAPH_CURVE_POINTS 
             || cmodel->type == GWY_GRAPH_CURVE_LINE_POINTS))
                 gwy_graph_draw_point(drawable, gc,
                                  x,
                                  y,
                                  cmodel->point_type, cmodel->point_size,
                                  &(cmodel->color), FALSE);
    }
}


void
gwy_graph_draw_line (GdkDrawable *drawable, GdkGC *gc,
                        gint x_from, gint y_from, gint x_to, gint y_to,
                        GdkLineStyle line_style,
                        gint size, GwyRGBA *color)
{
    GdkColor bcl, fcl;
    GdkColormap *colormap;
    GwyRGBA rgba;
    
    if (gc==NULL) gc = gdk_gc_new(drawable);

    colormap = gdk_colormap_get_system();
    gwy_rgba_to_gdk_color(color, &fcl);
    gdk_colormap_alloc_color(colormap, &fcl, TRUE, TRUE);
    gdk_gc_set_foreground(gc, &fcl);

    rgba.r = MIN(color->g+0.2, 1);
    rgba.g = MIN(color->b+0.2, 1);
    rgba.b = MIN(color->r+0.2, 1);
    gwy_rgba_to_gdk_color(&rgba, &bcl);
    gdk_colormap_alloc_color(colormap, &bcl, TRUE, TRUE);
    gdk_gc_set_background(gc, &bcl);
    
    gdk_gc_set_line_attributes (gc, size,
                  line_style, GDK_CAP_BUTT, GDK_JOIN_MITER);

    gdk_draw_line(drawable, gc, x_from, y_from, x_to, y_to);
   
}


void
gwy_graph_draw_point (GdkDrawable *drawable, GdkGC *gc,
                        gint x, gint y, GwyGraphPointType type,
                        gint size, GwyRGBA *color, G_GNUC_UNUSED gboolean clear)
{
    GdkColor gcl;
    GdkColormap *colormap;
    gint point_thickness;
    gint i, j;
    gint size_half = size/2;
    
    if (gc==NULL) gc = gdk_gc_new(drawable);
    
    point_thickness = MAX(size/10, 1);
   
    colormap = gdk_colormap_get_system();
    gwy_rgba_to_gdk_color(color, &gcl);
    gdk_colormap_alloc_color(colormap, &gcl, TRUE, TRUE);
    
    gdk_gc_set_foreground(gc, &gcl);
    gdk_gc_set_line_attributes (gc, point_thickness,
                  GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
   
    i = x;
    j = y;
    switch (type) {
        case GWY_GRAPH_POINT_SQUARE:
        gdk_draw_line(drawable, gc,
                 i - size_half, j - size_half, i + size_half, j - size_half);
        gdk_draw_line(drawable, gc,
                 i + size_half, j - size_half, i + size_half, j + size_half);
        gdk_draw_line(drawable, gc,
                 i + size_half, j + size_half, i - size_half, j + size_half);
        gdk_draw_line(drawable, gc,
                 i - size_half, j + size_half, i - size_half, j - size_half);
        break;

        case GWY_GRAPH_POINT_CROSS:
        gdk_draw_line(drawable, gc,
                 i - size_half, j, i + size_half, j);
        gdk_draw_line(drawable, gc,
                 i, j - size_half, i, j + size_half);
        break;

        case GWY_GRAPH_POINT_CIRCLE:
        gdk_draw_arc(drawable, gc, 0, i - size_half, j - size_half,
                     size, size, 0, 23040);
        break;

        case GWY_GRAPH_POINT_STAR:
        gdk_draw_line(drawable, gc,
                 i - size_half, j - size_half, i + size_half, j + size_half);
        gdk_draw_line(drawable, gc,
                 i + size_half, j - size_half, i - size_half, j + size_half);
        gdk_draw_line(drawable, gc,
                 i, j - size_half, i, j + size_half);
        gdk_draw_line(drawable, gc,
                 i - size_half, j, i + size_half, j);
        break;

        case GWY_GRAPH_POINT_TIMES:
        gdk_draw_line(drawable, gc,
                 i - size_half, j - size_half, i + size_half, j + size_half);
        gdk_draw_line(drawable, gc,
                 i + size_half, j - size_half, i - size_half, j + size_half);
        break;

        case GWY_GRAPH_POINT_TRIANGLE_UP:
        gdk_draw_line(drawable, gc,
                 i, j - size*0.57, i - size_half, j + size*0.33);
        gdk_draw_line(drawable, gc,
                 i - size_half, j + size*0.33, i + size_half, j + size*0.33);
        gdk_draw_line(drawable, gc,
                 i + size_half, j + size*0.33, i, j - size*0.33);
        break;

        case GWY_GRAPH_POINT_TRIANGLE_DOWN:
        gdk_draw_line(drawable, gc,
                 i, j + size*0.57, i - size_half, j - size*0.33);
        gdk_draw_line(drawable, gc,
                 i - size_half, j - size*0.33, i + size_half, j - size*0.33);
        gdk_draw_line(drawable, gc,
                 i + size_half, j - size*0.33, i, j + size*0.33);
        break;

        case GWY_GRAPH_POINT_DIAMOND:
        gdk_draw_line(drawable, gc,
                 i - size_half, j, i, j - size_half);
        gdk_draw_line(drawable, gc,
                 i, j - size_half, i + size_half, j);
        gdk_draw_line(drawable, gc,
                 i + size_half, j, i, j + size_half);
        gdk_draw_line(drawable, gc,
                 i, j + size_half, i - size_half, j);
        break;

        default:
        g_assert_not_reached();
        break;
    } 
}

void gwy_graph_draw_selection_points(GdkDrawable *drawable,
                                       GdkGC *gc, GwyGraphActiveAreaSpecs *specs,
                                       GwyGraphDataPoint *data_points, gint n_of_points)
{
    gint i, size;
    GwyRGBA color;
    
    color.r = 0.4;
    color.g = 0.4;
    color.b = 0.4;
    color.a = 1;
    size = 6;
    
    if (gc==NULL) gc = gdk_gc_new(drawable);
    
    for (i = 0; i<n_of_points; i++)
    {
       gwy_graph_draw_point (drawable, gc,
                        x_data_to_pixel(specs, data_points[i].x), 
                        y_data_to_pixel(specs, data_points[i].y), 
                        GWY_GRAPH_POINT_CROSS,
                        size, &color, FALSE);
    }
}

void gwy_graph_draw_selection_areas(GdkDrawable *drawable,
                                      GdkGC *gc, GwyGraphActiveAreaSpecs *specs,
                                      GwyGraphDataArea *data_areas, gint n_of_areas)
{
    gint i;
    gint xmin, xmax, ymin, ymax;
    GwyRGBA color;
    GdkColor gcl;
    GdkColormap *colormap;

    if (n_of_areas == 0 || data_areas==NULL) return;
    color.r = 0.8;
    color.g = 0.3;
    color.b = 0.6;
    color.a = 1;
   
    if (gc==NULL) gc = gdk_gc_new(drawable);

    colormap = gdk_colormap_get_system();
    gwy_rgba_to_gdk_color(&color, &gcl);
    gdk_colormap_alloc_color(colormap, &gcl, TRUE, TRUE);
    
    gdk_gc_set_foreground(gc, &gcl);

    for (i = 0; i<n_of_areas; i++)
    {
        xmin = x_data_to_pixel(specs, data_areas[i].xmin);
        xmax = x_data_to_pixel(specs, data_areas[i].xmax);
        ymin = y_data_to_pixel(specs, data_areas[i].ymin);
        ymax = y_data_to_pixel(specs, data_areas[i].ymax);
        
        gdk_draw_rectangle(drawable, gc, TRUE,
                       MIN(xmin, xmax),
                       MIN(ymin, ymax),
                       fabs(xmax - xmin),
                       fabs(ymax - ymin));
    }
}

void gwy_graph_draw_selection_lines(GdkDrawable *drawable,
                                    GdkGC *gc,
                                    GwyGraphActiveAreaSpecs *specs,
                                    double *data_lines,
                                    gint n_of_lines,
                                    GtkOrientation orientation)
{
    gint i;
    GwyRGBA color;
    GdkColor gcl;
    GdkColormap *colormap;

    if (n_of_lines == 0 || data_lines==NULL) return;
    color.r = 0.8;
    color.g = 0.3;
    color.b = 0.6;
    color.a = 1;
   
    if (gc==NULL) gc = gdk_gc_new(drawable);

    colormap = gdk_colormap_get_system();
    gwy_rgba_to_gdk_color(&color, &gcl);
    gdk_colormap_alloc_color(colormap, &gcl, TRUE, TRUE);
    
    gdk_gc_set_foreground(gc, &gcl);

    for (i = 0; i<n_of_lines; i++)
    {
        if (orientation == GTK_ORIENTATION_HORIZONTAL)
        gwy_graph_draw_line(drawable, gc,
                                  specs->xmin, 
                                  y_data_to_pixel(specs, data_lines[i]), 
                                  specs->xmin + specs->width, 
                                  y_data_to_pixel(specs, data_lines[i]),
                                  GDK_LINE_SOLID,
                                  1, &color);    
        else
        gwy_graph_draw_line(drawable, gc,
                                  x_data_to_pixel(specs, data_lines[i]), 
                                  specs->ymin, 
                                  x_data_to_pixel(specs, data_lines[i]),
                                  specs->ymin + specs->height,
                                  GDK_LINE_SOLID,
                                  1, &color);
     } 
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
