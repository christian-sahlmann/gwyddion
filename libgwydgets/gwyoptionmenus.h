/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#ifndef __GWY_OPTION_MENUS_H__
#define __GWY_OPTION_MENUS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtkwidget.h>
#include <libprocess/cwt.h>
#include <libprocess/simplefft.h>
#include <libgwydgets/gwydatawindow.h>

typedef struct {
    const gchar *name;
    gint value;
} GwyOptionMenuEntry;

GtkWidget* gwy_palette_menu               (GCallback callback,
                                           gpointer cbdata);
GtkWidget* gwy_palette_option_menu        (GCallback callback,
                                           gpointer cbdata,
                                           const gchar *current);
GtkWidget* gwy_interpolation_option_menu  (GCallback callback,
                                           gpointer cbdata,
                                           GwyInterpolationType current);
GtkWidget* gwy_windowing_option_menu      (GCallback callback,
                                           gpointer cbdata,
                                           GwyWindowingType current);
GtkWidget* gwy_zoom_mode_option_menu      (GCallback callback,
                                           gpointer cbdata,
                                           GwyZoomMode current);
GtkWidget* gwy_2dcwt_option_menu          (GCallback callback,
                                           gpointer cbdata,
                                           Gwy2DCWTWaveletType current);
GtkWidget* gwy_fft_output_menu            (GCallback callback,
                                           gpointer cbdata,
                                           GwyFFTOutputType current);
GtkWidget* gwy_option_menu_create         (const GwyOptionMenuEntry *entries,
                                           gint nentries,
                                           const gchar *key,
                                           GCallback callback,
                                           gpointer cbdata,
                                           gint current);
gboolean   gwy_option_menu_set_history    (GtkWidget *option_menu,
                                           const gchar *key,
                                           gint current);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_OPTION_MENUS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

