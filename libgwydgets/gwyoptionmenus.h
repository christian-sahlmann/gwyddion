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

#include <gtk/gtkwidget.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/cwt.h>
#include <libprocess/simplefft.h>
#include <libprocess/dataline.h>
#include <libgwydgets/gwydatawindow.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GtkWidget* gwy_menu_palette               (GCallback callback,
                                           gpointer cbdata);
GtkWidget* gwy_option_menu_palette        (GCallback callback,
                                           gpointer cbdata,
                                           const gchar *current);
GtkWidget* gwy_option_menu_interpolation  (GCallback callback,
                                           gpointer cbdata,
                                           GwyInterpolationType current);
GtkWidget* gwy_option_menu_windowing      (GCallback callback,
                                           gpointer cbdata,
                                           GwyWindowingType current);
GtkWidget* gwy_option_menu_zoom_mode      (GCallback callback,
                                           gpointer cbdata,
                                           GwyZoomMode current);
GtkWidget* gwy_option_menu_2dcwt          (GCallback callback,
                                           gpointer cbdata,
                                           Gwy2DCWTWaveletType current);
GtkWidget* gwy_option_menu_fft_output     (GCallback callback,
                                           gpointer cbdata,
                                           GwyFFTOutputType current);
GtkWidget* gwy_option_menu_sfunctions_output (GCallback callback,
                                           gpointer cbdata,
                                           GwySFOutputType current);
GtkWidget* gwy_option_menu_direction       (GCallback callback,
                                           gpointer cbdata,
                                           GtkOrientation current);
GtkWidget* gwy_option_menu_create         (const GwyEnum *entries,
                                           gint nentries,
                                           const gchar *key,
                                           GCallback callback,
                                           gpointer cbdata,
                                           gint current);
gboolean   gwy_option_menu_set_history    (GtkWidget *option_menu,
                                           const gchar *key,
                                           gint current);
gint       gwy_option_menu_get_history    (GtkWidget *option_menu,
                                           const gchar *key);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_OPTION_MENUS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

