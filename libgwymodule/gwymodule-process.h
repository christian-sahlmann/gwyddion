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

#ifndef __GWY_MODULE_PROCESS_H__
#define __GWY_MODULE_PROCESS_H__

#include <gtk/gtkobject.h>
#include <libgwyddion/gwycontainer.h>
/* XXX: GwyMenuSensFlags */
#include <app/menu.h>

G_BEGIN_DECLS

typedef enum {
    GWY_RUN_NONE           = 0,
    GWY_RUN_WITH_DEFAULTS  = 1 << 0,
    GWY_RUN_NONINTERACTIVE = 1 << 1,
    GWY_RUN_MODAL          = 1 << 2,
    GWY_RUN_INTERACTIVE    = 1 << 3,
    GWY_RUN_MASK           = 0x0f
} GwyRunType;

typedef struct _GwyProcessFuncInfo GwyProcessFuncInfo;

typedef gboolean       (*GwyProcessFunc)        (GwyContainer *data,
                                                 GwyRunType run,
                                                 const gchar *name);

struct _GwyProcessFuncInfo {
    const gchar *name;
    const gchar *menu_path;
    GwyProcessFunc process;
    GwyRunType run;
};

gboolean       gwy_process_func_register      (const gchar *modname,
                                               GwyProcessFuncInfo *func_info);
gboolean       gwy_process_func_run           (const guchar *name,
                                               GwyContainer *data,
                                               GwyRunType run);
GwyRunType     gwy_process_func_get_run_types (const gchar *name);
G_CONST_RETURN
gchar*         gwy_process_func_get_menu_path (const gchar *name);
GtkObject*     gwy_process_func_build_menu    (GtkObject *item_factory,
                                               const gchar *prefix,
                                               GCallback item_callback);
void           gwy_process_func_set_sensitivity_flags (const gchar *name,
                                                       GwyMenuSensFlags flags);

G_END_DECLS

#endif /* __GWY_MODULE_PROCESS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
