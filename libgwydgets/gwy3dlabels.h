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

#ifndef __GWY_3D_LABELS__
#define __GWY_3D_LABELS__

#include <glib-object.h>
#include <gtk/gtkadjustment.h>

#include <libgwyddion/gwycontainer.h>
#include <libgwyddion/gwysiunit.h>

G_BEGIN_DECLS

#define GWY_TYPE_3D_LABELS              \
    (gwy_3d_labels_get_type())
#define GWY_3D_LABELS(obj)              \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_3D_LABELS, Gwy3DLabels))
#define GWY_3D_LABELS_CLASS(klass)      \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_3D_LABELS, Gwy3DLabelsClass))
#define GWY_IS_3D_LABELS(obj)           \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_3D_LABELS))
#define GWY_IS_3D_LABELS_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_3D_LABELS))
#define GWY_3D_LABELS_GET_CLASS(obj)    \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_3D_LABELS, Gwy3DLabelsClass))


typedef struct _Gwy3DLabelDescription      Gwy3DLabelDescription;
typedef struct _Gwy3DLabelDescriptionClass Gwy3DLabelDescriptionClass;
typedef struct _Gwy3DLabels                Gwy3DLabels;
typedef struct _Gwy3DLabelsClass           Gwy3DLabelsClass;


typedef enum {
    GWY_3D_VIEW_LABEL_X,
    GWY_3D_VIEW_LABEL_Y,
    GWY_3D_VIEW_LABEL_MIN,
    GWY_3D_VIEW_LABEL_MAX
} Gwy3DLabelName;

struct _Gwy3DLabelDescription {
    gchar          * base_key;
    gchar          * default_text;
    gchar          * text;
    gboolean         auto_scale;
    GtkAdjustment  * delta_x;
    GtkAdjustment  * delta_y;
    GtkAdjustment  * rotation;
    GtkAdjustment  * size;
    Gwy3DLabels    * owner;
} ;


struct _Gwy3DLabels {
    GObject parent;

    GwyContainer * container;

    Gwy3DLabelDescription ** labels;
    guint labels_count;

    gchar ** keys;
    gchar ** values;
    guint variables_count;
    gchar *  text;

    gpointer reserved1;
    gpointer reserved2;
} ;

struct _Gwy3DLabelsClass {
    GObjectClass parent_class;

    void (* label_changed) (Gwy3DLabels *labels);

    gpointer reserved1;
    gpointer reserved2;
};

GType                   gwy_3d_labels_get_type             (void) G_GNUC_CONST;

Gwy3DLabels            * gwy_3d_labels_new            (GwyContainer * container);
void                     gwy_3d_labels_update         (Gwy3DLabels * labels,
                                                       GwyContainer * container,
                                                       GwySIUnit * si_unit);
gchar                  * gwy_3d_labels_format_text    (Gwy3DLabels * labels,
                                                       Gwy3DLabelName label_name);

Gwy3DLabelDescription  * gwy_3d_labels_get_description(Gwy3DLabels * gwy3dlabels,
                                                      Gwy3DLabelName label_name);

void    gwy_3d_label_description_set_text(Gwy3DLabelDescription * label_description,
                                          const gchar* text);
void    gwy_3d_label_description_reset    (Gwy3DLabelDescription * label_description);
gboolean gwy_3d_label_description_get_autoscele(
                                          Gwy3DLabelDescription * label_description);
void     gwy_3d_label_description_set_autoscale(
                                          Gwy3DLabelDescription * label_description,
                                           const gboolean autoscale);

#define gwy_3d_labels_get_delta_x(labels, name) \
            (gwy_3d_labels_get_description(labels, name)->delta_x->value)
#define gwy_3d_labels_get_delta_y(labels, name) \
            (gwy_3d_labels_get_description(labels, name)->delta_y->value)
#define gwy_3d_labels_get_rotation(labels, name) \
            (gwy_3d_labels_get_description(labels, name)->rotation->value)
#define gwy_3d_labels_get_size(labels, name) \
            (gwy_3d_labels_get_description(labels, name)->size->value)
gdouble gwy_3d_labels_user_size(Gwy3DLabels *labels,
                                Gwy3DLabelName name,
                                gint user_size);

G_END_DECLS

#endif /* gwy3dlabels.h */
