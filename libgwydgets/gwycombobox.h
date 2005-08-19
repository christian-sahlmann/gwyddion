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

#ifndef __GWY_COMBO_BOX_H__
#define __GWY_COMBO_BOX_H__

#include <gtk/gtkwidget.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwyddion/gwyenum.h>
#include <libgwyddion/gwysiunit.h>

G_BEGIN_DECLS

/*
#define GWY_TYPE_COMBO_BOX            (gwy_combo_box_get_type())
#define GWY_COMBO_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_COMBO_BOX, GwyComboBox))
#define GWY_COMBO_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_COMBO_BOX, GwyComboBoxClass))
#define GWY_IS_COMBO_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_COMBO_BOX))
#define GWY_IS_COMBO_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_COMBO_BOX))
#define GWY_COMBO_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_COMBO_BOX, GwyComboBoxClass))

typedef struct _GwyComboBox      GwyComboBox;
typedef struct _GwyComboBoxClass GwyComboBoxClass;

struct _GwyComboBox {
    GtkWidget widget;

};

struct _GwyComboBoxClass {
    GtkWidgetClass parent_class;

};
*/

GtkWidget* gwy_enum_combo_box_new        (const GwyEnum *entries,
                                          gint nentries,
                                          GCallback callback,
                                          gpointer cbdata,
                                          gint active,
                                          gboolean translate);
GtkWidget* gwy_combo_box_metric_unit_new (GCallback callback,
                                          gpointer cbdata,
                                          gint from,
                                          gint to,
                                          GwySIUnit *unit,
                                          gint active);
void       gwy_enum_combo_box_set_active (GtkComboBox *combo,
                                          gint active);
gint       gwy_enum_combo_box_get_active (GtkComboBox *combo);
void       gwy_enum_combo_box_update_int (GtkComboBox *combo,
                                          gint *integer);

G_END_DECLS

#endif /* __GWY_COMBO_BOX_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
