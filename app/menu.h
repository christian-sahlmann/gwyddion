/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#ifndef __GWY_APP_MENU_H__
#define __GWY_APP_MENU_H__

#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


enum {
    GWY_MENU_FLAG_DATA  = 1 << 0,
    GWY_MENU_FLAG_UNDO  = 1 << 1,
    GWY_MENU_FLAG_REDO  = 1 << 2,
    GWY_MENU_FLAG_GRAPH = 1 << 3,
    GWY_MENU_FLAG_MASK  = 0xf
};

typedef struct _GwyMenuSensitiveData GwyMenuSensitiveData;

struct _GwyMenuSensitiveData {
    guint flags;
    guint set_to;
};

GtkWidget* gwy_menu_create_xtns_menu         (GtkAccelGroup *accel_group);
GtkWidget* gwy_menu_create_proc_menu         (GtkAccelGroup *accel_group);
GtkWidget* gwy_menu_create_graph_menu        (GtkAccelGroup *accel_group);
GtkWidget* gwy_menu_create_file_menu         (GtkAccelGroup *accel_group);
GtkWidget* gwy_menu_create_edit_menu         (GtkAccelGroup *accel_group);

void       gwy_menu_set_sensitive_recursive  (GtkWidget *widget,
                                              GwyMenuSensitiveData *data);
void       gwy_menu_recent_files_update      (GList *recent_files);

/* FIXME: where this could really belong? */
void       gwy_app_run_process_func_cb       (gchar *name);
void       gwy_app_run_graph_func_cb         (gchar *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_APP_MENU_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
