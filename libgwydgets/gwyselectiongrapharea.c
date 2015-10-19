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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwyselectiongrapharea.h>

enum {
    OBJECT_SIZE = 4
};


static void gwy_selection_graph_area_crop(GwySelection *selection,
                                          gdouble xmin,
                                          gdouble ymin,
                                          gdouble xmax,
                                          gdouble ymax);
static void gwy_selection_graph_area_move(GwySelection *selection,
                                          gdouble vx,
                                          gdouble vy);

G_DEFINE_TYPE(GwySelectionGraphArea, gwy_selection_graph_area,
              GWY_TYPE_SELECTION)

static void
gwy_selection_graph_area_class_init(GwySelectionGraphAreaClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = OBJECT_SIZE;
    sel_class->crop = gwy_selection_graph_area_crop;
    sel_class->move = gwy_selection_graph_area_move;
}

static void
gwy_selection_graph_area_init(GwySelectionGraphArea *selection)
{
    /* Set max. number of objects to one */
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
}

static gboolean
gwy_selection_graph_area_crop_object(GwySelection *selection,
                                     gint i,
                                     gpointer user_data)
{
    const gdouble *minmax = (const gdouble*)user_data;
    gdouble xy[OBJECT_SIZE];

    gwy_selection_get_object(selection, i, xy);
    return (MIN(xy[0], xy[2]) >= minmax[0]
            && MIN(xy[1], xy[3]) >= minmax[1]
            && MAX(xy[0], xy[2]) <= minmax[2]
            && MAX(xy[1], xy[3]) <= minmax[3]);
}

static void
gwy_selection_graph_area_crop(GwySelection *selection,
                              gdouble xmin,
                              gdouble ymin,
                              gdouble xmax,
                              gdouble ymax)
{
    gdouble minmax[4] = { xmin, ymin, xmax, ymax };

    gwy_selection_filter(selection, gwy_selection_graph_area_crop_object,
                         minmax);
}

static void
gwy_selection_graph_area_move(GwySelection *selection,
                              gdouble vx,
                              gdouble vy)
{
    gdouble *data = (gdouble*)selection->objects->data;
    guint i, n = selection->objects->len/OBJECT_SIZE;

    for (i = 0; i < n; i++) {
        data[OBJECT_SIZE*i + 0] += vx;
        data[OBJECT_SIZE*i + 1] += vy;
        data[OBJECT_SIZE*i + 2] += vx;
        data[OBJECT_SIZE*i + 3] += vy;
    }
}

/**
 * gwy_selection_graph_area_new:
 *
 * Creates a new area-wise graph selection.
 *
 * Returns: A new selection object.
 *
 * Since: 2.1
 **/
GwySelection*
gwy_selection_graph_area_new(void)
{
    return (GwySelection*)g_object_new(GWY_TYPE_SELECTION_GRAPH_AREA, NULL);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyselectiongrapharea
 * @title: GwySelectionGraphArea
 * @short_description: Area-wise graph selection
 *
 * #GwySelectionGraphArea is used to represent area-wise graph selections.
 * Selection data consists of coordinate quadruples (x0, y0, x1, y1).
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
