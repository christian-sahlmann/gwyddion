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
    gchar *name;
    gchar *file;
    gboolean loaded;
    GSList *funcs;
} _GwyModuleInfoInternal;

G_GNUC_INTERNAL
gboolean _gwy_module_add_registered_function(const gchar *prefix,
                                             const gchar *name);

G_GNUC_INTERNAL
gboolean _gwy_file_func_remove              (const gchar *name);

G_GNUC_INTERNAL
gboolean _gwy_process_func_remove           (const gchar *name);

G_GNUC_INTERNAL
gboolean _gwy_tool_func_remove              (const gchar *name);

G_GNUC_INTERNAL
gboolean _gwy_graph_func_remove             (const gchar *name);

G_GNUC_INTERNAL
gboolean _gwy_layer_func_remove             (const gchar *name);

/* XXX XXX XXX: pedantic check */
G_GNUC_INTERNAL
void
_gwy_module_watch_settings(const gchar *modname,
                           const gchar *funcname);

G_GNUC_INTERNAL
void
_gwy_module_unwatch_settings(void);

G_END_DECLS

#endif /* __GWY_MODULE_INTERNAL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

