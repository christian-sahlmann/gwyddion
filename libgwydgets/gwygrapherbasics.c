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

#include <math.h>
#include <gtk/gtkmain.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include "gwygrapher.h"


/**
 * gwy_grapher_draw_point:
 * @window: widget window 
 * @gc: Grapherical context
 * @i: x position on the screen
 * @j: y position on the screen
 * @type: type of point (square, circle, etc.)
 * @size: size of point
 * @color: color of point
 * @clear: clear window part under symbol
 *
 * Plots a point of requested parameters on the screen.
 **/
/*
void
gwy_grapher_draw_point(GdkWindow *window, GdkGC *gc, gint i, gint j, gint type,
                     gint size, G_GNUC_UNUSED GdkColor *color, gboolean clear)
{

    gint size_half = size/2;

    
    if (clear)
    {
        gdk_window_clear_area(window,
                              i - size_half - 1, j - size_half - 1,
                              size + 2, size + 2);
    }

    
    gdk_gc_set_line_attributes (gc, 1,
                  GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
    switch (type) {
        case GWY_GRAPHER_POINT_SQUARE:
        gdk_draw_line(window, gc,
                 i - size_half, j - size_half, i + size_half, j - size_half);
        gdk_draw_line(window, gc,
                 i + size_half, j - size_half, i + size_half, j + size_half);
        gdk_draw_line(window, gc,
                 i + size_half, j + size_half, i - size_half, j + size_half);
        gdk_draw_line(window, gc,
                 i - size_half, j + size_half, i - size_half, j - size_half);
        break;

        case GWY_GRAPHER_POINT_CROSS:
        gdk_draw_line(window, gc,
                 i - size_half, j, i + size_half, j);
        gdk_draw_line(window, gc,
                 i, j - size_half, i, j + size_half);
        break;

        case GWY_GRAPHER_POINT_CIRCLE:
        gdk_draw_arc(window, gc, 0, i - size_half, j - size_half,
                     size, size, 0, 23040);
        break;

        case GWY_GRAPHER_POINT_STAR:
        gdk_draw_line(window, gc,
                 i - size_half, j - size_half, i + size_half, j + size_half);
        gdk_draw_line(window, gc,
                 i + size_half, j - size_half, i - size_half, j + size_half);
        gdk_draw_line(window, gc,
                 i, j - size_half, i, j + size_half);
        gdk_draw_line(window, gc,
                 i - size_half, j, i + size_half, j);
        break;

        case GWY_GRAPHER_POINT_TIMES:
        gdk_draw_line(window, gc,
                 i - size_half, j - size_half, i + size_half, j + size_half);
        gdk_draw_line(window, gc,
                 i + size_half, j - size_half, i - size_half, j + size_half);
        break;

        case GWY_GRAPHER_POINT_TRIANGLE_UP:
        gdk_draw_line(window, gc,
                 i, j - size*0.57, i - size_half, j + size*0.33);
        gdk_draw_line(window, gc,
                 i - size_half, j + size*0.33, i + size_half, j + size*0.33);
        gdk_draw_line(window, gc,
                 i + size_half, j + size*0.33, i, j - size*0.33);
        break;

        case GWY_GRAPHER_POINT_TRIANGLE_DOWN:
        gdk_draw_line(window, gc,
                 i, j + size*0.57, i - size_half, j - size*0.33);
        gdk_draw_line(window, gc,
                 i - size_half, j - size*0.33, i + size_half, j - size*0.33);
        gdk_draw_line(window, gc,
                 i + size_half, j - size*0.33, i, j + size*0.33);
        break;

        case GWY_GRAPHER_POINT_DIAMOND:
        gdk_draw_line(window, gc,
                 i - size_half, j, i, j - size_half);
        gdk_draw_line(window, gc,
                 i, j - size_half, i + size_half, j);
        gdk_draw_line(window, gc,
                 i + size_half, j, i, j + size_half);
        gdk_draw_line(window, gc,
                 i, j + size_half, i - size_half, j);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}
*/
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
