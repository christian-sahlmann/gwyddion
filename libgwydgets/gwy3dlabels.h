/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *  Copyright (C) 2004 Martin Siler.
 *  E-mail: silerm@physics.muni.cz.
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

#ifndef __GWY_3D_LABEL_GROUP__
#define __GWY_3D_LABEL_GROUP__

#include <glib-object.h>
#include <gtk/gtkadjustment.h>

#include <libgwyddion/gwycontainer.h>
#include <libgwyddion/gwysiunit.h>
#include <libgwydgets/gwy3dlabel.h>

G_BEGIN_DECLS

#define GWY_TYPE_3D_LABEL_GROUP            (gwy_3d_label_group_get_type())
#define GWY_3D_LABEL_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_3D_LABEL_GROUP, Gwy3DLabelGroup))
#define GWY_3D_LABEL_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_3D_LABEL_GROUP, Gwy3DLabelGroupClass))
#define GWY_IS_3D_LABEL_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_3D_LABEL_GROUP))
#define GWY_IS_3D_LABEL_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_3D_LABEL_GROUP))
#define GWY_3D_LABEL_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_3D_LABEL_GROUP, Gwy3DLabelGroupClass))


typedef struct _Gwy3DLabelGroup                Gwy3DLabelGroup;
typedef struct _Gwy3DLabelGroupClass           Gwy3DLabelGroupClass;

struct _Gwy3DLabelGroup {
    GtkObject parent;

    GwyContainer *container;
    GHashTable *labels;
    GHashTable *variables;

    gpointer reserved1;
    gpointer reserved2;
};

struct _Gwy3DLabelGroupClass {
    GtkObjectClass parent_class;

    void (*label_changed) (Gwy3DLabelGroup *label_group,
                           const gchar *key);

    gpointer reserved1;
    gpointer reserved2;
};

GType            gwy_3d_label_group_get_type    (void) G_GNUC_CONST;
Gwy3DLabelGroup* gwy_3d_label_group_new         (GwyContainer *container);
void             gwy_3d_label_group_update      (Gwy3DLabelGroup *label_group,
                                                 GwySIUnit *si_unit);
gchar*           gwy_3d_label_group_expand_label(Gwy3DLabelGroup *label_group,
                                                 const gchar *key);

G_END_DECLS

#endif /* gwy3dlabel_group.h */
