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

#ifndef __GWY_STATUSBAR_H__
#define __GWY_STATUSBAR_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_STATUSBAR            (gwy_statusbar_get_type())
#define GWY_STATUSBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_STATUSBAR, GwyStatusbar))
#define GWY_STATUSBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_STATUSBAR, GwyStatusbarClass))
#define GWY_IS_STATUSBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_STATUSBAR))
#define GWY_IS_STATUSBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_STATUSBAR))
#define GWY_STATUSBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_STATUSBAR, GwyStatusbarClass))

typedef struct _GwyStatusbar      GwyStatusbar;
typedef struct _GwyStatusbarClass GwyStatusbarClass;

struct _GwyStatusbar {
    GtkStatusbar parent_instance;
};

struct _GwyStatusbarClass {
    GtkStatusbarClass parent_class;
};

GtkWidget*       gwy_statusbar_new              (void);
GType            gwy_statusbar_get_type         (void) G_GNUC_CONST;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_STATUSBAR_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

