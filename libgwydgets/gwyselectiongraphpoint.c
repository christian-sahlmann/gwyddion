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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwyselectiongraphpoint.h>

enum {
    OBJECT_SIZE = 2,
};

G_DEFINE_TYPE(GwySelectionGraphPoint, gwy_selection_graph_point,
              GWY_TYPE_SELECTION)

static void
gwy_selection_graph_point_class_init(GwySelectionGraphPointClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = OBJECT_SIZE;
}

static void
gwy_selection_graph_point_init(GwySelectionGraphPoint *selection)
{
    /* Set max. number of objects to one */
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
}

/**
 * gwy_selection_graph_point_new:
 *
 * Creates a new point-wise graph selection.
 *
 * Returns: A new selection object.
 *
 * Since: 2.1
 **/
GwySelection*
gwy_selection_graph_point_new(void)
{
    return (GwySelection*)g_object_new(GWY_TYPE_SELECTION_GRAPH_POINT, NULL);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyselectiongraphpoint
 * @title: GwySelectionGraphPoint
 * @short_description: Point-wise graph selection
 *
 * #GwySelectionGraphPoint is used to represent point-wise graph selections.
 * Selection data consists of coordinate pairs (x, y).
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
