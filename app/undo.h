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

#ifndef __GWY_APP_UNDO_H__
#define __GWY_APP_UNDO_H__

#include <libgwyddion/gwycontainer.h>

G_BEGIN_DECLS

gulong          gwy_app_undo_checkpoint           (GwyContainer *data,
                                                   ...);
gulong          gwy_app_undo_checkpointv          (GwyContainer *data,
                                                   guint n,
                                                   const gchar **keys);
void            gwy_app_undo_undo                 (void);
void            gwy_app_undo_undo_window          (GwyDataWindow *data_window);
void            gwy_app_undo_redo                 (void);
void            gwy_app_undo_redo_window          (GwyDataWindow *data_window);
gboolean        gwy_app_data_window_has_undo      (GwyDataWindow *data_window);
gboolean        gwy_app_data_window_has_redo      (GwyDataWindow *data_window);
void            gwy_app_undo_clear                (GwyDataWindow *data_window);

G_END_DECLS

#endif /* __GWY_APP_UNDO_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
