/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_APP_INTERNAL_H__
#define __GWY_APP_INTERNAL_H__

#include <gtk/gtkwidget.h>
#include <libgwymodule/gwymodule-process.h>

G_BEGIN_DECLS

void            gwy_app_main_window_set             (GtkWidget *window);
void            gwy_app_tool_use_cb                 (const gchar *toolname,
                                                     GtkWidget *button);
void            gwy_app_zoom_set_cb                 (gpointer data);
void            gwy_app_mask_kill_cb                (void);
void            gwy_app_show_kill_cb                (void);
void            gwy_app_change_mask_color_cb        (gpointer unused,
                                                     gboolean defaultc);

guint           gwy_app_run_process_func_cb         (gchar *name);
void            gwy_app_run_process_func_in_mode    (gchar *name,
                                                     GwyRunType run);
void            gwy_app_run_graph_func_cb           (gchar *name);


G_END_DECLS

#endif /* __GWY_APP_INTERNAL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

