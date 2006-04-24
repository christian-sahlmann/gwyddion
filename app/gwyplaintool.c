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
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/app.h>
#include <app/gwyplaintool.h>

static void     gwy_plain_tool_finalize      (GObject *object);
static void     gwy_plain_tool_show          (GwyTool *tool);
static void     gwy_plain_tool_hide          (GwyTool *tool);
static void     gwy_plain_tool_data_switched (GwyTool *tool,
                                              GwyDataView *data_view);
static void     gwy_plain_tool_update_units  (GwyPlainTool *plain_tool);

G_DEFINE_ABSTRACT_TYPE(GwyPlainTool, gwy_plain_tool, GWY_TYPE_TOOL)

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
gwy_plain_tool_finalize(GObject *object)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(object);
    if (plain_tool->coord_format)
        gwy_si_unit_value_format_free(plain_tool->coord_format);
    if (plain_tool->value_format)
        gwy_si_unit_value_format_free(plain_tool->value_format);

    G_OBJECT_CLASS(gwy_plain_tool_parent_class)->finalize(object);
}

static void
gwy_plain_tool_init(G_GNUC_UNUSED GwyPlainTool *tool)
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
    GwyPlainTool *plain_tool;

    if (GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->data_switched)
        GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->data_switched(tool,
                                                                   data_view);

    plain_tool = GWY_PLAIN_TOOL(tool);
    /* XXX XXX XXX */
    plain_tool->data_view = data_view;
    if (plain_tool->unit_style)
        gwy_plain_tool_update_units(plain_tool);
}

static void
gwy_plain_tool_update_units(GwyPlainTool *plain_tool)
{
    GwyContainer *container;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    const gchar *key;

    g_return_if_fail(GWY_IS_DATA_VIEW(plain_tool->data_view));
    layer = gwy_data_view_get_base_layer(plain_tool->data_view);
    key = gwy_pixmap_layer_get_data_key(layer);
    container = gwy_data_view_get_data(plain_tool->data_view);
    dfield = gwy_container_get_object_by_name(container, key);
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));

    plain_tool->coord_format
        = gwy_data_field_get_value_format_xy(dfield,
                                             plain_tool->unit_style,
                                             plain_tool->coord_format);
    plain_tool->value_format
        = gwy_data_field_get_value_format_z(dfield,
                                            plain_tool->unit_style,
                                            plain_tool->value_format);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyplaintool
 * @title: GwyPlainTool
 * @short_description: Base class for simple tools
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
