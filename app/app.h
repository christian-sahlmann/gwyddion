/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_APP_APP_H__
#define __GWY_APP_APP_H__

#include <libgwyddion/gwycontainer.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwygraph.h>

G_BEGIN_DECLS

typedef enum {
    GWY_APP_WINDOW_TYPE_DATA  = 1 << 0,
    GWY_APP_WINDOW_TYPE_GRAPH = 1 << 1,
    GWY_APP_WINDOW_TYPE_3D    = 1 << 2,
    GWY_APP_WINDOW_TYPE_ANY   = 0x7
} GwyAppWindowType;

#ifndef GWY_DISABLE_DEPRECATED
GwyContainer*  gwy_app_get_current_data            (void);
GwyDataWindow* gwy_app_data_window_get_current     (void);
gboolean       gwy_app_data_window_set_current     (GwyDataWindow *window);
void           gwy_app_data_window_remove          (GwyDataWindow *window);
void           gwy_app_data_window_foreach         (GFunc func,
                                                    gpointer user_data);
GwyDataWindow* gwy_app_data_window_get_for_data    (GwyContainer *data);

void           gwy_app_graph_window_remove         (GtkWidget *window);
GtkWidget*     gwy_app_graph_window_get_current    (void);
gboolean       gwy_app_graph_window_set_current    (GtkWidget *window);

GtkWidget*     gwy_app_get_current_window          (GwyAppWindowType type);
#endif
void           gwy_app_switch_tool                 (const gchar *toolname);
void           gwy_app_add_main_accel_group        (GtkWindow *window);
void           gwy_app_save_window_position        (GtkWindow *window,
                                                    const gchar *prefix,
                                                    gboolean position,
                                                    gboolean size);
void           gwy_app_restore_window_position     (GtkWindow *window,
                                                    const gchar *prefix,
                                                    gboolean grow_only);
GtkWidget*     gwy_app_main_window_get             (void);
void           gwy_app_data_view_change_mask_color (GwyDataView *data_view);

gboolean       gwy_app_quit                        (void);

void            gwy_app_init_widget_styles         (void);
void            gwy_app_init_i18n                  (void);

gboolean        gwy_app_init_common                (GError **error,
                                                    ...);

G_END_DECLS

#endif /* __GWY_APP_APP_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

