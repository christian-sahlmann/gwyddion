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

#ifndef __GWY_APP_SETTINGS_H__
#define __GWY_APP_SETTINGS_H__

#include <libgwyddion/gwycontainer.h>
#include <glib.h>

G_BEGIN_DECLS

#define GWY_APP_SETTINGS_ERROR gwy_app_settings_error_quark()

typedef enum {
    GWY_APP_SETTINGS_ERROR_FILE,
    GWY_APP_SETTINGS_ERROR_CORRUPT,
    GWY_APP_SETTINGS_ERROR_CFGDIR
} GwyAppSettingsError;

GQuark        gwy_app_settings_error_quark              (void);

GwyContainer* gwy_app_settings_get                      (void);
void          gwy_app_settings_free                     (void);
gboolean      gwy_app_settings_save                     (const gchar *filename,
                                                         GError **error);
gboolean      gwy_app_settings_load                     (const gchar *filename,
                                                         GError **error);

gboolean      gwy_app_settings_create_config_dir        (GError **error);
gchar**       gwy_app_settings_get_module_dirs          (void);
gchar*        gwy_app_settings_get_settings_filename    (void);
gchar*        gwy_app_settings_get_log_filename         (void);
gchar*        gwy_app_settings_get_recent_file_list_filename (void);

gboolean      gwy_app_gl_init                           (int *argc,
                                                         char ***argv);
gboolean      gwy_app_gl_is_ok                          (void);

G_END_DECLS

#endif /* __GWY_APP_SETTINGS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

