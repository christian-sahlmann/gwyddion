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

#ifndef __GWY_MODULE_INTERNAL_H__
#define __GWY_MODULE_INTERNAL_H__

#include "gwymoduleloader.h"

G_BEGIN_DECLS

/* prefixes for canonical function names */
#define GWY_MODULE_PREFIX_PROC     "proc::"
#define GWY_MODULE_PREFIX_FILE     "file::"
#define GWY_MODULE_PREFIX_TOOL     "tool::"
#define GWY_MODULE_PREFIX_GRAPH    "graph::"
#define GWY_MODULE_PREFIX_LAYER    "layer::"

/* internal module info */
typedef struct {
    GwyModuleInfo *mod_info;
    gchar *file;
    gboolean loaded;
    GSList *funcs;
} _GwyModuleInfoInternal;

_GwyModuleInfoInternal* gwy_module_get_module_info  (const gchar *name);
gboolean                gwy_file_func_remove        (const gchar *name);
gboolean                gwy_process_func_remove     (const gchar *name);
gboolean                gwy_tool_func_remove        (const gchar *name);
gboolean                gwy_graph_func_remove       (const gchar *name);
gboolean                gwy_layer_func_remove       (const gchar *name);

void  _gwy_file_func_set_register_callback(void (*callback)(const gchar *fullname));
void  _gwy_process_func_set_register_callback(void (*callback)(const gchar *fullname));
void  _gwy_tool_func_set_register_callback(void (*callback)(const gchar *fullname));
void  _gwy_graph_func_set_register_callback(void (*callback)(const gchar *fullname));
void  _gwy_layer_func_set_register_callback(void (*callback)(const gchar *fullname));

G_END_DECLS

#endif /* __GWY_MODULE_INTERNAL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

