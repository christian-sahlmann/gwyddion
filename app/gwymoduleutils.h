/*
 *  @(#) $Id$
 *  Copyright (C) 2007-2015 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef __GWY_MODULE_UTILS_H__
#define __GWY_MODULE_UTILS_H__

#include <gtk/gtkwindow.h>
#include <libgwydgets/gwydataview.h>
#include <app/datachooser.h>

G_BEGIN_DECLS

/* XXX: Makes partially-updated modules compilable. To be removed in 2.41. */
typedef struct {
    GwyContainer *data;
    gint id;
} GwyAppDataIdTmp;

typedef gchar* (*GwySaveAuxiliaryCreate)(gpointer user_data,
                                         gssize *data_len);
typedef void (*GwySaveAuxiliaryDestroy)(gchar *data,
                                        gpointer user_data);

gboolean
gwy_save_auxiliary_data(const gchar *title,
                        GtkWindow *parent,
                        gssize data_len,
                        const gchar *data);

gboolean
gwy_save_auxiliary_with_callback(const gchar *title,
                                 GtkWindow *parent,
                                 GwySaveAuxiliaryCreate create,
                                 GwySaveAuxiliaryDestroy destroy,
                                 gpointer user_data);

void
gwy_set_data_preview_size(GwyDataView *data_view,
                          gint max_size);


gboolean gwy_app_data_id_verify_channel(GwyAppDataId *id);
gboolean gwy_app_data_id_verify_graph  (GwyAppDataId *id);
gboolean gwy_app_data_id_verify_volume (GwyAppDataId *id);
gboolean gwy_app_data_id_verify_spectra(GwyAppDataId *id);

G_END_DECLS

#endif /* __GWY_MODULE_UTILS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
