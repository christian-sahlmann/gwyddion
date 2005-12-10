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
#include <glib-object.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include <libdraw/gwyselection.h>
#include <libgwydgets/gwygraphselections.h>

#include <stdio.h>

enum {
        POINT_OBJECT_SIZE = 2,
        AREA_OBJECT_SIZE = 4,
        LINE_OBJECT_SIZE = 1
};


G_DEFINE_TYPE(GwySelectionGraphPoint, gwy_selection_graph_point, GWY_TYPE_SELECTION)


static void
gwy_selection_graph_point_class_init(GwySelectionGraphPointClass *klass)
{
        GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

        sel_class->object_size = POINT_OBJECT_SIZE;
}


static void
gwy_selection_graph_point_init(GwySelectionGraphPoint *selection)
{
        g_array_set_size(GWY_SELECTION(selection)->objects, POINT_OBJECT_SIZE);
}


G_DEFINE_TYPE(GwySelectionGraphArea, gwy_selection_graph_area, GWY_TYPE_SELECTION)


static void
gwy_selection_graph_area_class_init(GwySelectionGraphAreaClass *klass)
{
        GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

        sel_class->object_size = AREA_OBJECT_SIZE;
}


static void
gwy_selection_graph_area_init(GwySelectionGraphArea *selection)
{
        g_array_set_size(GWY_SELECTION(selection)->objects, AREA_OBJECT_SIZE);
}


G_DEFINE_TYPE(GwySelectionGraphLine, gwy_selection_graph_line, GWY_TYPE_SELECTION)


static void
gwy_selection_graph_line_class_init(GwySelectionGraphLineClass *klass)
{
        GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

        sel_class->object_size = LINE_OBJECT_SIZE;
}


static void
gwy_selection_graph_line_init(GwySelectionGraphLine *selection)
{
        g_array_set_size(GWY_SELECTION(selection)->objects, LINE_OBJECT_SIZE);
}



/************************** Documentation ****************************/

/**
 * SECTION:gwygraphselections
 * @title: GwyGraphSelections
 * @short_description: Graph selection types
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
