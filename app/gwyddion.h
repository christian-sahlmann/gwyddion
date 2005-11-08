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

#ifndef __GWYDDION_GWYDDION_H__
#define __GWYDDION_GWYDDION_H__

#include <gtk/gtkwidget.h>
#include <libgwydgets/gwydatawindow.h>

G_BEGIN_DECLS

GtkWidget* gwy_app_toolbox_create            (void);
void       gwy_app_about                     (void);
void       gwy_app_metadata_browser          (GwyDataWindow *data_window);
void       gwy_app_init                      (int *argc,
                                              char ***argv);

void       gwy_app_splash_create             (void);
void       gwy_app_splash_close              (void);
void       gwy_app_splash_set_message        (const gchar *message);
void       gwy_app_splash_set_message_prefix (const gchar *prefix);
void       gwy_app_splash_enable             (gboolean enable);

G_END_DECLS

#endif /* __GWYDDION_GWYDDION_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
