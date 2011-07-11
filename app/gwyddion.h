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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*< private_header >*/

#ifndef __GWYDDION_GWYDDION_H__
#define __GWYDDION_GWYDDION_H__

#include <gtk/gtkwidget.h>
#include <libgwyddion/gwycontainer.h>

G_BEGIN_DECLS

#define GWY_TOOLBOX_WM_ROLE "gwyddion-toolbox"

#define REMOTE_NONE   0
#define REMOTE_X11    1
#define REMOTE_WIN32  2
#define REMOTE_UNIQUE 3

typedef struct _GwyRemote GwyRemote;

typedef enum {
    GWY_APP_REMOTE_NONE = 0,
    GWY_APP_REMOTE_NEW,
    GWY_APP_REMOTE_EXISTING,
    GWY_APP_REMOTE_QUERY
} GwyAppRemoteType;

void       gwy_remote_setup             (GtkWidget *toolbox);
void       gwy_remote_finalize          (GtkWidget *toolbox);
void       gwy_remote_do                (GwyAppRemoteType type,
                                         int argc,
                                         char **argv);
GwyRemote* gwy_remote_get               (void);
void       gwy_remote_free              (GwyRemote *remote);
gboolean   gwy_remote_open_files        (GwyRemote *remote,
                                         int argc,
                                         char **argv);
void       gwy_remote_print             (GwyRemote *remote);

GtkWidget* gwy_app_toolbox_create           (void);
GtkWidget* gwy_app_show_data_browser        (void);
void       gwy_app_about                    (void);
void       gwy_app_tip_of_the_day           (void);
void       gwy_app_metadata_browser         (GwyContainer *data,
                                             gint id);
void       gwy_app_splash_start             (gboolean visible);
void       gwy_app_splash_finish            (void);
void       gwy_app_splash_set_message       (const gchar *message);
void       gwy_app_splash_set_message_prefix(const gchar *prefix);
gboolean   gwy_app_gl_disabled              (void);

G_END_DECLS

#endif /* __GWYDDION_GWYDDION_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
