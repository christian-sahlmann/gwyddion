/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_SENSITIVITY_GROUP_H__
#define __GWY_SENSITIVITY_GROUP_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GWY_TYPE_SENSITIVITY_GROUP            (gwy_sensitivity_group_get_type())
#define GWY_SENSITIVITY_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SENSITIVITY_GROUP, GwySensitivityGroup))
#define GWY_SENSITIVITY_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SENSITIVITY_GROUP, GwySensitivityGroupClass))
#define GWY_IS_SENSITIVITY_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SENSITIVITY_GROUP))
#define GWY_IS_SENSITIVITY_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SENSITIVITY_GROUP))
#define GWY_SENSITIVITY_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SENSITIVITY_GROUP, GwySensitivityGroupClass))

typedef struct _GwySensitivityGroup       GwySensitivityGroup;
typedef struct _GwySensitivityGroupClass  GwySensitivityGroupClass;

struct _GwySensitivityGroup {
    GObject parent_instance;

    guint state;
    guint old_state;
    GList *lists;
    gulong source_id;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwySensitivityGroupClass {
    GObjectClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};


GType  gwy_sensitivity_group_get_type         (void) G_GNUC_CONST;
GwySensitivityGroup* gwy_sensitivity_group_new(void);
void   gwy_sensitivity_group_add_widget       (GwySensitivityGroup *sensgroup,
                                               GtkWidget *widget,
                                               guint mask);
void   gwy_sensitivity_group_set_state        (GwySensitivityGroup *sensgroup,
                                               guint affected_mask,
                                               guint state);
guint  gwy_sensitivity_group_get_state        (GwySensitivityGroup *sensgroup);
void   gwy_sensitivity_group_release_widget   (GwySensitivityGroup *sensgroup,
                                               GtkWidget *widget);
guint  gwy_sensitivity_group_get_widget_mask  (GwySensitivityGroup *sensgroup,
                                               GtkWidget *widget);
void   gwy_sensitivity_group_set_widget_mask  (GwySensitivityGroup *sensgroup,
                                               GtkWidget *widget,
                                               guint mask);

G_END_DECLS

#endif /* __GWY_SENSITIVITY_GROUP_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

