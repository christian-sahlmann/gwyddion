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

#ifndef __GWY_APP_MENU_H__
#define __GWY_APP_MENU_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtktooltips.h>
#include <libgwydgets/gwysensitivitygroup.h>

G_BEGIN_DECLS

typedef enum {
    GWY_MENU_FLAG_DATA       = 1 << 0,
    GWY_MENU_FLAG_UNDO       = 1 << 1,
    GWY_MENU_FLAG_REDO       = 1 << 2,
    GWY_MENU_FLAG_GRAPH      = 1 << 3,
    GWY_MENU_FLAG_LAST_PROC  = 1 << 4,
    GWY_MENU_FLAG_LAST_GRAPH = 1 << 5,
    GWY_MENU_FLAG_DATA_MASK  = 1 << 6,
    GWY_MENU_FLAG_DATA_SHOW  = 1 << 7,
    GWY_MENU_FLAG_3D         = 1 << 8,
    GWY_MENU_FLAG_MASK       = 0xff
} GwyMenuSensFlags;

void         gwy_app_menu_recent_files_update(GList *recent_files);
GtkTooltips* gwy_app_get_tooltips            (void);
GwySensitivityGroup* gwy_app_sensitivity_get_group   (void);
void         gwy_app_sensitivity_add_widget  (GtkWidget *widget,
                                              GwyMenuSensFlags mask);
void         gwy_app_sensitivity_set_state   (GwyMenuSensFlags affected_mask,
                                              GwyMenuSensFlags state);

G_END_DECLS

#endif /* __GWY_APP_MENU_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
