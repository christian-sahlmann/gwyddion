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

#ifndef __GWY_MODULE_LOADER_H__
#define __GWY_MODULE_LOADER_H__

#include <gmodule.h>

G_BEGIN_DECLS

#define GWY_MODULE_ABI_VERSION 2

#define _GWY_MODULE_QUERY _gwy_module_query
#define GWY_MODULE_QUERY(mod_info) \
    G_MODULE_EXPORT GwyModuleInfo* \
    _GWY_MODULE_QUERY(void) { return &mod_info; }

typedef struct _GwyModuleInfo GwyModuleInfo;

typedef gboolean       (*GwyModuleRegisterFunc) (const gchar *name);
typedef GwyModuleInfo* (*GwyModuleQueryFunc)    (void);

struct _GwyModuleInfo {
    guint32 abi_version;
    GwyModuleRegisterFunc register_func;
    const gchar *blurb;
    const gchar *author;
    const gchar *version;
    const gchar *copyright;
    const gchar *date;
};

void                    gwy_module_register_modules (const gchar **paths);
const GwyModuleInfo*    gwy_module_lookup           (const gchar *name);
const gchar*            gwy_module_get_filename     (const gchar *name);
GSList*                 gwy_module_get_functions    (const gchar *name);
void                    gwy_module_foreach          (GHFunc function,
                                                     gpointer data);
const GwyModuleInfo*    gwy_module_register_module  (const gchar *name);

G_END_DECLS

#endif /* __GWY_MODULE_LOADER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
