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

#ifndef __GWY_STOCK_H__
#define __GWY_STOCK_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_STOCK_BOLD            "gwy_bold"
#define GWY_STOCK_CROP            "gwy_crop"
#define GWY_STOCK_CWT             "gwy_cwt"
#define GWY_STOCK_FACET_LEVEL     "gwy_facet_level"
#define GWY_STOCK_FILTER          "gwy_filter"
#define GWY_STOCK_FIT_PLANE       "gwy_fit_plane"
#define GWY_STOCK_FIT_TRIANGLE    "gwy_fit_triangle"
#define GWY_STOCK_FIX_ZERO        "gwy_fix_zero"
#define GWY_STOCK_FFT             "gwy_fft"
#define GWY_STOCK_GRAINS          "gwy_grains"
#define GWY_STOCK_GRAPH           "gwy_graph"
#define GWY_STOCK_GRAPH_GAUSS     "gwy_graph_gauss"
#define GWY_STOCK_GRAPH_HALFGAUSS "gwy_graph_halfgauss"
#define GWY_STOCK_GRAPH_MEASURE   "gwy_graph_measure"
#define GWY_STOCK_GRAPH_PALETTE   "gwy_graph_palette"
#define GWY_STOCK_GRAPH_POINTER   "gwy_graph_pointer"
#define GWY_STOCK_GRAPH_ZOOM_FIT  "gwy_graph_zoom_fit"
#define GWY_STOCK_GRAPH_ZOOM_IN   "gwy_graph_zoom_in"
#define GWY_STOCK_GRAPH_ZOOM_OUT  "gwy_graph_zoom_out"
#define GWY_STOCK_GWYDDION        "gwy_gwyddion"
#define GWY_STOCK_ITALIC          "gwy_italic"
#define GWY_STOCK_NONE            "gwy_none"
#define GWY_STOCK_POINTER         "gwy_pointer"
#define GWY_STOCK_POINTER_MEASURE "gwy_pointer_measure"
#define GWY_STOCK_POLYNOM_REMOVE  "gwy_polynom_remove"
#define GWY_STOCK_PROFILE         "gwy_profile"
#define GWY_STOCK_ROTATE          "gwy_rotate"
#define GWY_STOCK_SCALE           "gwy_scale"
#define GWY_STOCK_SHADER          "gwy_shader"
#define GWY_STOCK_STAT_QUANTITIES "gwy_stat_quantities"
#define GWY_STOCK_SUBSCRIPT       "gwy_subscript"
#define GWY_STOCK_SUPERSCRIPT     "gwy_superscript"
#define GWY_STOCK_ZOOM_1_1        "gwy_zoom_1_1"
#define GWY_STOCK_ZOOM_FIT        "gwy_zoom_fit"
#define GWY_STOCK_ZOOM_IN         "gwy_zoom_in"
#define GWY_STOCK_ZOOM_OUT        "gwy_zoom_out"

#define GWY_ICON_SIZE_ABOUT "gwy-about"

void gwy_stock_register_stock_items(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_STOCK_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

