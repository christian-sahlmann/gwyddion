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

#ifndef __GWY_APP_FUNCUSE_H__
#define __GWY_APP_FUNCUSE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GwyFunctionUse GwyFunctionUse;

GwyFunctionUse* gwy_func_use_new             (void);
void            gwy_func_use_free            (GwyFunctionUse *functions);
void            gwy_func_use_add             (GwyFunctionUse *functions,
                                              const gchar *name);
const gchar*    gwy_func_use_get             (GwyFunctionUse *functions,
                                              guint i);
GwyFunctionUse* gwy_func_use_load            (const gchar *filename);
void            gwy_func_use_save            (GwyFunctionUse *functions,
                                              const gchar *filename);
gchar*          gwy_func_use_get_filename    (const gchar *type);

GwyFunctionUse* gwy_app_process_func_get_use (void);
void            gwy_app_process_func_save_use(void);

G_END_DECLS

#endif /* __GWY_APP_FUNCUSE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
