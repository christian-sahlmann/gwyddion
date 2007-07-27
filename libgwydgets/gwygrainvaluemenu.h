/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

#ifndef __GWY_GRAIN_VALUE_MENU_H__
#define __GWY_GRAIN_VALUE_MENU_H__

#include <libprocess/gwygrainvalue.h>
#include <gtk/gtktreeview.h>

G_BEGIN_DECLS

GtkWidget* gwy_grain_value_tree_view_new                (gboolean show_id,
                                                         const gchar *first_column,
                                                         ...);
void       gwy_grain_value_tree_view_set_expanded_groups(GtkTreeView *treeview,
                                                         guint expanded_bits);
guint      gwy_grain_value_tree_view_get_expanded_groups(GtkTreeView *treeview);
void       gwy_grain_value_tree_view_select             (GtkTreeView *treeview,
                                                         GwyGrainValue *gvalue);

G_END_DECLS

#endif /* __GWY_GRAIN_VALUE_MENU_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
