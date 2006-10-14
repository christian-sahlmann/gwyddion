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

/*< private_header >*/

#ifndef __GWY_APP_INTERNAL_H__
#define __GWY_APP_INTERNAL_H__

#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwy3dwindow.h>
#include <libgwydgets/gwygraphwindow.h>

#include <app/gwyappfilechooser.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
gint     _gwy_app_get_n_recent_files          (void);

G_GNUC_INTERNAL
void     _gwy_app_data_window_setup           (GwyDataWindow *data_window);
G_GNUC_INTERNAL
void     _gwy_app_3d_window_setup             (Gwy3DWindow *window3d);
G_GNUC_INTERNAL
void     _gwy_app_graph_window_setup          (GwyGraphWindow *graph_window);

G_GNUC_INTERNAL
void     _gwy_app_data_view_set_current       (GwyDataView *data_view);
G_GNUC_INTERNAL
void     _gwy_app_graph_set_current           (GwyGraph *graph);

void     gwy_app_main_window_set              (GtkWidget *window);

G_END_DECLS

#endif /* __GWY_APP_INTERNAL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

