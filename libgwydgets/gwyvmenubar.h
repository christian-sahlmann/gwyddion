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


#ifndef __GWY_VMENU_BAR_H__
#define __GWY_VMENU_BAR_H__

#include <gtk/gtkmenubar.h>

G_BEGIN_DECLS

#ifndef GWY_DISABLE_DEPRECATED

#define GWY_TYPE_VMENU_BAR            (gwy_vmenu_bar_get_type())
#define GWY_VMENU_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_VMENU_BAR, GwyVMenuBar))
#define GWY_VMENU_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_VMENU_BAR, GwyVMenuBarClass))
#define GWY_IS_VMENU_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_VMENU_BAR))
#define GWY_IS_VMENU_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_VMENU_BAR))
#define GWY_VMENU_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_VMENU_BAR, GwyVMenuBarClass))

typedef struct _GwyVMenuBar       GwyVMenuBar;
typedef struct _GwyVMenuBarClass  GwyVMenuBarClass;

struct _GwyVMenuBar {
    GtkMenuBar parent_instance;
};

struct _GwyVMenuBarClass {
    GtkMenuBarClass parent_class;

    /* Padding for future expansion */
    void (*reserved1)(void);
    void (*reserved2)(void);
};

GType      gwy_vmenu_bar_get_type        (void) G_GNUC_CONST;
GtkWidget* gwy_vmenu_bar_new             (void);

#endif

G_END_DECLS

#endif /* __GWY_VMENU_BAR_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
