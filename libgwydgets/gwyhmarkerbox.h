/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_HMARKER_BOX_H__
#define __GWY_HMARKER_BOX_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

typedef enum {
    GWY_MARKER_OPERATION_MOVE,
    GWY_MARKER_OPERATION_ADD,
    GWY_MARKER_OPERATION_REMOVE
} GwyMarkerOperationType;

#define GWY_TYPE_HMARKER_BOX            (gwy_hmarker_box_get_type())
#define GWY_HMARKER_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_HMARKER_BOX, GwyHMarkerBox))
#define GWY_HMARKER_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_HMARKER_BOX, GwyHMarkerBoxClass))
#define GWY_IS_HMARKER_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_HMARKER_BOX))
#define GWY_IS_HMARKER_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_HMARKER_BOX))
#define GWY_HMARKER_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_HMARKER_BOX, GwyHMarkerBoxClass))

typedef struct _GwyHMarkerBox      GwyHMarkerBox;
typedef struct _GwyHMarkerBoxClass GwyHMarkerBoxClass;

typedef gboolean (*GwyMarkerValidateFunc)(GwyHMarkerBox *hmbox,
                                          GwyMarkerOperationType optype,
                                          gint i,
                                          gdouble *pos);

struct _GwyHMarkerBox {
    GtkWidget widget;

    /* properties */
    gboolean flipped;
    GwyMarkerValidateFunc validate;

    /* state */
    GArray *markers;
    gboolean moved;
    gint selected;
    gint button;
    gint offset;
};

struct _GwyHMarkerBoxClass {
    GtkWidgetClass parent_class;

    /* signals */
    void (*marker_selected)(GwyHMarkerBox *hmbox,
                            gint i);
    void (*marker_moved)(GwyHMarkerBox *hmbox,
                         gint i);
    void (*marker_added)(GwyHMarkerBox *hmbox,
                         gint i);
    void (*marker_removed)(GwyHMarkerBox *hmbox,
                           gint i);

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

GType      gwy_selection_marker_get_type      (void) G_GNUC_CONST;
GType      gwy_hmarker_box_get_type           (void) G_GNUC_CONST;
GtkWidget* gwy_hmarker_box_new                (void);
gint       gwy_hmarker_box_get_selected_marker(GwyHMarkerBox *hmbox);
void       gwy_hmarker_box_set_selected_marker(GwyHMarkerBox *hmbox,
                                               gint i);
gdouble    gwy_hmarker_box_get_marker_position(GwyHMarkerBox *hmbox,
                                               gint i);
gboolean   gwy_hmarker_box_set_marker_position(GwyHMarkerBox *hmbox,
                                               gint i,
                                               gdouble pos);
gint       gwy_hmarker_box_add_marker         (GwyHMarkerBox *hmbox,
                                               gdouble pos);
gboolean   gwy_hmarker_box_remove_marker      (GwyHMarkerBox *hmbox,
                                               gint i);
gint       gwy_hmarker_box_get_nmarkers       (GwyHMarkerBox *hmbox);
void       gwy_hmarker_box_set_markers        (GwyHMarkerBox *hmbox,
                                               gint n,
                                               const gdouble *markers);
void       gwy_hmarker_box_set_flipped        (GwyHMarkerBox *hmbox,
                                               gboolean flipped);
gboolean   gwy_hmarker_box_get_flipped        (GwyHMarkerBox *hmbox);
void       gwy_hmarker_box_set_validator      (GwyHMarkerBox *hmbox,
                                               GwyMarkerValidateFunc validate);
GwyMarkerValidateFunc gwy_hmarker_box_get_validator(GwyHMarkerBox *hmbox);

G_END_DECLS

#endif /* __GWY_HMARKER_BOX_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
