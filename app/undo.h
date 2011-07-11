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

#ifndef __GWY_APP_UNDO_H__
#define __GWY_APP_UNDO_H__

#include <libgwyddion/gwycontainer.h>

G_BEGIN_DECLS

gulong   gwy_app_undo_checkpoint          (GwyContainer *data,
                                           ...);
gulong   gwy_app_undo_qcheckpoint         (GwyContainer *data,
                                           ...);
gulong   gwy_app_undo_checkpointv         (GwyContainer *data,
                                           guint n,
                                           const gchar **keys);
gulong   gwy_app_undo_qcheckpointv        (GwyContainer *data,
                                           guint n,
                                           const GQuark *keys);
void     gwy_app_undo_undo_container      (GwyContainer *data);
void     gwy_app_undo_redo_container      (GwyContainer *data);
void     gwy_app_undo_container_remove    (GwyContainer *data,
                                           const gchar *prefix);

gulong   gwy_undo_checkpoint              (GwyContainer *data,
                                           ...);
gulong   gwy_undo_qcheckpoint             (GwyContainer *data,
                                           ...);
gulong   gwy_undo_checkpointv             (GwyContainer *data,
                                           guint n,
                                           const gchar **keys);
gulong   gwy_undo_qcheckpointv            (GwyContainer *data,
                                           guint n,
                                           const GQuark *keys);
void     gwy_undo_undo_container          (GwyContainer *data);
void     gwy_undo_redo_container          (GwyContainer *data);
gboolean gwy_undo_container_has_undo      (GwyContainer *data);
gboolean gwy_undo_container_has_redo      (GwyContainer *data);
gint     gwy_undo_container_get_modified  (GwyContainer *data);
void     gwy_undo_container_set_unmodified(GwyContainer *data);
void     gwy_undo_container_remove        (GwyContainer *data,
                                           const gchar *prefix);
gboolean gwy_undo_get_enabled             (void);
void     gwy_undo_set_enabled             (gboolean setting);

G_END_DECLS

#endif /* __GWY_APP_UNDO_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
