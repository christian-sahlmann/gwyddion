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

#ifndef __GWY_MARKER_BOX_H__
#define __GWY_MARKER_BOX_H__

#include <gtk/gtkwidget.h>
#include <libgwydgets/gwydgetenums.h>

G_BEGIN_DECLS

#define GWY_TYPE_MARKER_BOX            (gwy_marker_box_get_type())
#define GWY_MARKER_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_MARKER_BOX, GwyMarkerBox))
#define GWY_MARKER_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_MARKER_BOX, GwyMarkerBoxClass))
#define GWY_IS_MARKER_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_MARKER_BOX))
#define GWY_IS_MARKER_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_MARKER_BOX))
#define GWY_MARKER_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_MARKER_BOX, GwyMarkerBoxClass))

typedef struct _GwyMarkerBox      GwyMarkerBox;
typedef struct _GwyMarkerBoxClass GwyMarkerBoxClass;

typedef gboolean (*GwyMarkerValidateFunc)(GwyMarkerBox *mbox,
                                          GwyMarkerOperationType optype,
                                          gint *i,
                                          gdouble *pos);

struct _GwyMarkerBox {
    GtkWidget widget;

    /* properties */
    gboolean flipped;
    gboolean highlight;
    GwyMarkerValidateFunc validate;

    /* state */
    GArray *markers;
    gboolean moved;
    gboolean ghost;
    gint selected;
    gint button;
    gint offset;
};

struct _GwyMarkerBoxClass {
    GtkWidgetClass parent_class;

    /* signals */
    void (*marker_selected)(GwyMarkerBox *mbox,
                            gint i);
    void (*marker_moved)(GwyMarkerBox *mbox,
                         gint i);
    void (*marker_added)(GwyMarkerBox *mbox,
                         gint i);
    void (*marker_removed)(GwyMarkerBox *mbox,
                           gint i);
    void (*markers_set)(GwyMarkerBox *mbox);

    /* virtual methods */
    void (*draw_box)(GwyMarkerBox *mbox);
    void (*draw_marker)(GwyMarkerBox *mbox,
                        gint i);

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

GType      gwy_marker_box_get_type              (void) G_GNUC_CONST;
gint       gwy_marker_box_get_selected_marker   (GwyMarkerBox *mbox);
void       gwy_marker_box_set_selected_marker   (GwyMarkerBox *mbox,
                                                 gint i);
gdouble    gwy_marker_box_get_marker_position   (GwyMarkerBox *mbox,
                                                 gint i);
gboolean   gwy_marker_box_set_marker_position   (GwyMarkerBox *mbox,
                                                 gint i,
                                                 gdouble pos);
gint       gwy_marker_box_add_marker            (GwyMarkerBox *mbox,
                                                 gint i,
                                                 gdouble pos);
gboolean   gwy_marker_box_remove_marker         (GwyMarkerBox *mbox,
                                                 gint i);
gint       gwy_marker_box_get_nmarkers          (GwyMarkerBox *mbox);
const gdouble* gwy_marker_box_get_markers       (GwyMarkerBox *mbox);
void       gwy_marker_box_set_markers           (GwyMarkerBox *mbox,
                                                 gint n,
                                                 const gdouble *markers);
void       gwy_marker_box_set_flipped           (GwyMarkerBox *mbox,
                                                 gboolean flipped);
gboolean   gwy_marker_box_get_flipped           (GwyMarkerBox *mbox);
void       gwy_marker_box_set_highlight_selected(GwyMarkerBox *mbox,
                                                 gboolean highlight);
gboolean   gwy_marker_box_get_highlight_selected(GwyMarkerBox *mbox);
void       gwy_marker_box_set_validator        (GwyMarkerBox *mbox,
                                                GwyMarkerValidateFunc validate);
GwyMarkerValidateFunc gwy_marker_box_get_validator(GwyMarkerBox *mbox);

G_END_DECLS

#endif /* __GWY_MARKER_BOX_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
