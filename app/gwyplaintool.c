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
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/app.h>
#include <app/gwyplaintool.h>

static void     gwy_plain_tool_finalize      (GObject *object);
static void     gwy_plain_tool_show          (GwyTool *tool);
static void     gwy_plain_tool_hide          (GwyTool *tool);
static void     gwy_plain_tool_data_switched (GwyTool *tool,
                                              GwyDataView *data_view);

G_DEFINE_ABSTRACT_TYPE(GwyPlainTool, gwy_plain_tool, GWY_TYPE_PLAIN_TOOL)

static void
gwy_plain_tool_class_init(GwyPlainToolClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);

    gobject_class->finalize = gwy_plain_tool_finalize;

    tool_class->hide = gwy_plain_tool_hide;
    tool_class->show = gwy_plain_tool_show;
    tool_class->data_switched = gwy_plain_tool_data_switched;
}

static void
gwy_plain_tool_finalize(G_GNUC_UNUSED GObject *object)
{
    G_OBJECT_CLASS(gwy_plain_tool_parent_class)->finalize(object);
}

static void
gwy_plain_tool_init(GwyPlainTool *tool)
{
}

static void
gwy_plain_tool_show(GwyTool *tool)
{
    GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->show(tool);
}

static void
gwy_plain_tool_hide(GwyTool *tool)
{
    GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->hide(tool);
}

void
gwy_plain_tool_data_switched(GwyTool *tool,
                             GwyDataView *data_view)
{
    GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->data_switched(tool, data_view);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyplaintool
 * @title: GwyPlainTool
 * @short_description: Base class for simple tools
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
