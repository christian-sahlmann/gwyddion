/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwyselectiongraphzoom.h>

enum {
    OBJECT_SIZE = 4
};

G_DEFINE_TYPE(GwySelectionGraphZoom, gwy_selection_graph_zoom,
              GWY_TYPE_SELECTION)

static void
gwy_selection_graph_zoom_class_init(GwySelectionGraphZoomClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = OBJECT_SIZE;
}

static void
gwy_selection_graph_zoom_init(GwySelectionGraphZoom *selection)
{
    /* Set max. number of objects to one */
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
}

/**
 * gwy_selection_graph_zoom_new:
 *
 * Creates a new zoom-wise graph selection.
 *
 * Returns: A new selection object.
 **/
GwySelection*
gwy_selection_graph_zoom_new(void)
{
    return (GwySelection*)g_object_new(GWY_TYPE_SELECTION_GRAPH_ZOOM, NULL);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyselectiongraphzoom
 * @title: GwySelectionGraphZoom
 * @short_description: Graph zoom selection
 *
 * #GwySelectionGraphZoom is used to represent zoom graph selections.
 * Selection data consists of coordinate quadruples (x0, y0, x1, y1).
 *
 * This selection type is completely identical to #GwySelectionGraphArea and
 * should probably not exist as a separate class.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
