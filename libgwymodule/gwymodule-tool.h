/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_MODULE_TOOL_H__
#define __GWY_MODULE_TOOL_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtktooltips.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwymodule/gwymoduleenums.h>

G_BEGIN_DECLS

typedef struct _GwyToolFuncInfo GwyToolFuncInfo;

typedef gboolean (*GwyToolUseFunc)(GwyDataWindow *data_window,
                                   GwyToolSwitchEvent event);

struct _GwyToolFuncInfo {
    const gchar *name;
    const gchar *stock_id;
    const gchar *tooltip;
    GwyToolUseFunc use;
};

gboolean     gwy_tool_func_register      (const gchar *modname,
                                          GwyToolFuncInfo *func_info);
gboolean     gwy_tool_func_use           (const guchar *name,
                                          GwyDataWindow *data_window,
                                          GwyToolSwitchEvent event);
const gchar* gwy_tool_func_get_tooltip   (const gchar *name);
const gchar* gwy_tool_func_get_stock_id  (const gchar *name);
gboolean     gwy_tool_func_exists        (const gchar *name);

G_END_DECLS

#endif /* __GWY_MODULE_TOOL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
