/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_APP_WAIT_H__
#define __GWY_APP_WAIT_H__

#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void     gwy_app_wait_start              (GtkWidget *window,
                                          const gchar *message);
void     gwy_app_wait_finish             (void);
gboolean gwy_app_wait_switch_widget      (GtkWidget *window,
                                          const gchar *message);
gboolean gwy_app_wait_set_fraction       (gdouble fraction);
gboolean gwy_app_wait_set_message        (const gchar *message);
gboolean gwy_app_wait_set_message_prefix (const gchar *prefix);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_APP_WAIT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

