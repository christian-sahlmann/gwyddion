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

#ifndef __GWY_OPTION_MENUS_H__
#define __GWY_OPTION_MENUS_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwyenum.h>
#include <libgwyddion/gwynlfit.h>
#include <libgwyddion/gwysiunit.h>
#include <libprocess/gwyprocessenums.h>

G_BEGIN_DECLS

GtkWidget* gwy_menu_gradient                 (GCallback callback,
                                              gpointer cbdata);
GtkWidget* gwy_menu_gl_material              (GCallback callback,
                                              gpointer cbdata);
GtkWidget* gwy_option_menu_gradient          (GCallback callback,
                                              gpointer cbdata,
                                              const gchar *current);
GtkWidget* gwy_option_menu_gl_material       (GCallback callback,
                                              gpointer cbdata,
                                              const gchar *current);
GtkWidget* gwy_option_menu_metric_unit       (GCallback callback,
                                              gpointer cbdata,
                                              gint from,
                                              gint to,
                                              GwySIUnit *unit,
                                              gint current);
gboolean   gwy_option_menu_set_history       (GtkWidget *option_menu,
                                              const gchar *key,
                                              gint current);
gint       gwy_option_menu_get_history       (GtkWidget *option_menu,
                                              const gchar *key);

G_END_DECLS

#endif /* __GWY_OPTION_MENUS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

