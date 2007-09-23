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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libprocess/spectra.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwy3dwindow.h>
#include <libgwydgets/gwygraphwindow.h>
#include <libgwydgets/gwysensitivitygroup.h>

#include <app/gwyappfilechooser.h>

G_BEGIN_DECLS

/* Data types interesting keys can correspond to */
typedef enum {
    KEY_IS_NONE = 0,
    KEY_IS_DATA,
    KEY_IS_DATA_VISIBLE,
    KEY_IS_MASK,
    KEY_IS_SHOW,
    KEY_IS_GRAPH,
    KEY_IS_GRAPH_VISIBLE,
    KEY_IS_SPECTRA,
    KEY_IS_SPECTRA_VISIBLE,
    KEY_IS_META,
    KEY_IS_TITLE,
    KEY_IS_SELECT,
    KEY_IS_RANGE_TYPE,
    KEY_IS_RANGE,
    KEY_IS_PALETTE,
    KEY_IS_MASK_COLOR,
    KEY_IS_REAL_SQUARE,
    KEY_IS_3D_SETUP,
    KEY_IS_3D_PALETTE,
    KEY_IS_3D_MATERIAL,
    KEY_IS_3D_LABEL,
    KEY_IS_SPS_REF,
    KEY_IS_FILENAME,
    KEY_IS_GRAPH_LASTID
} GwyAppKeyType;

G_GNUC_INTERNAL
gint     _gwy_app_get_n_recent_files          (void);

G_GNUC_INTERNAL
void     _gwy_app_data_window_setup           (GwyDataWindow *data_window);
G_GNUC_INTERNAL
void     _gwy_app_3d_window_setup             (Gwy3DWindow *window3d);
G_GNUC_INTERNAL
gboolean _gwy_app_3d_view_init_setup          (GwyContainer *container,
                                               const gchar *setup_prefix);
G_GNUC_INTERNAL
void     _gwy_app_graph_window_setup          (GwyGraphWindow *graph_window);

G_GNUC_INTERNAL
void     _gwy_app_data_view_set_current       (GwyDataView *data_view);
G_GNUC_INTERNAL
void     _gwy_app_graph_set_current           (GwyGraph *graph);
G_GNUC_INTERNAL
void     _gwy_app_spectra_set_current         (GwySpectra *spectra);

G_GNUC_INTERNAL
GwySensitivityGroup* _gwy_app_sensitivity_get_group(void);

G_GNUC_INTERNAL
GdkPixbuf* _gwy_app_recent_file_try_thumbnail  (const gchar *filename_sys);
G_GNUC_INTERNAL
void       _gwy_app_recent_file_write_thumbnail(const gchar *filename_sys,
                                                GwyContainer *data,
                                                gint id,
                                                GdkPixbuf *pixbuf);

G_GNUC_INTERNAL
gint       _gwy_app_analyse_data_key           (const gchar *strkey,
                                                GwyAppKeyType *type,
                                                guint *len);
/* XXX */
void     gwy_app_main_window_set              (GtkWidget *window);

G_END_DECLS

#endif /* __GWY_APP_INTERNAL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

