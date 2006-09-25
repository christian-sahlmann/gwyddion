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

#ifndef __GWY_GRAPH_BASIC_H__
#define __GWY_GRAPH_BASIC_H__

#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <libdraw/gwyrgba.h>
#include <libgwydgets/gwydgetenums.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <libgwydgets/gwygraphselections.h>

G_BEGIN_DECLS

typedef struct {
    gint xmin;
    gint ymin;
    gint height;
    gint width;
    gdouble real_xmin;
    gdouble real_ymin;
    gdouble real_height;
    gdouble real_width;
    gboolean log_x;
    gboolean log_y;
} GwyGraphActiveAreaSpecs;

void gwy_graph_draw_point           (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     gint x,
                                     gint y,
                                     GwyGraphPointType type,
                                     gint size,
                                     const GwyRGBA *color);
void gwy_graph_draw_line            (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     gint x_from,
                                     gint y_from,
                                     gint x_to,
                                     gint y_to,
                                     GdkLineStyle line_style,
                                     gint size,
                                     const GwyRGBA *color);
void gwy_graph_draw_curve           (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwyGraphCurveModel *gcmodel);
void gwy_graph_draw_selection_points(GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwySelectionGraphPoint *selection);
void gwy_graph_draw_selection_areas (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwySelectionGraphArea *selection);
void gwy_graph_draw_selection_lines (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwySelectionGraphLine *selection,
                                     GtkOrientation orientation);
void gwy_graph_draw_selection_xareas(GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwySelectionGraph1DArea *selection);
void gwy_graph_draw_selection_yareas (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwySelectionGraph1DArea *selection);
void gwy_graph_draw_grid            (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     guint nxdata,
                                     const gdouble *x_grid_data,
                                     guint nydata,
                                     const gdouble *y_grid_data);

const GwyRGBA* gwy_graph_get_preset_color     (guint i) G_GNUC_CONST;
guint          gwy_graph_get_n_preset_colors  (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GWY_GRAPH_BASIC_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
