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

#ifndef __GWY_DATAVIEW_H__
#define __GWY_DATAVIEW_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>

#include <libgwyddion/gwycontainer.h>
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwydgets/gwypixmaplayer.h>

G_BEGIN_DECLS

#define GWY_TYPE_DATA_VIEW            (gwy_data_view_get_type())
#define GWY_DATA_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_VIEW, GwyDataView))
#define GWY_DATA_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_VIEW, GwyDataViewClass))
#define GWY_IS_DATA_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_VIEW))
#define GWY_IS_DATA_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_VIEW))
#define GWY_DATA_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_VIEW, GwyDataViewClass))

typedef struct _GwyDataView      GwyDataView;
typedef struct _GwyDataViewClass GwyDataViewClass;

struct _GwyDataView {
    GtkWidget widget;

    GwyContainer *data;

    GwyPixmapLayer *base_layer;
    GwyPixmapLayer *alpha_layer;
    GwyVectorLayer *top_layer;

    gulong base_hid;
    gulong alpha_hid;

    gboolean layers_changed : 1;
    gboolean size_requested : 1;

    gdouble zoom;    /* real zoom (larger number means larger pixmaps) */
    gdouble newzoom;    /* requested (ideal) zoom value */
    gdouble xmeasure;    /* physical units per pixel */
    gdouble ymeasure;    /* physical units per pixel */
    gint xoff;    /* x offset of the pixbuf from widget->allocation.x */
    gint yoff;    /* y offset of the pixbuf from widget->allocation.y */

    GdkPixbuf *pixbuf;      /* everything, this is drawn on the screen */
    GdkPixbuf *base_pixbuf; /* unscaled base (lower layers) */

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyDataViewClass {
    GtkWidgetClass parent_class;

    void (*redrawn)(GwyDataView *data_view);

    gpointer reserved2;
};

GtkWidget*        gwy_data_view_new               (GwyContainer *data);
GType             gwy_data_view_get_type          (void) G_GNUC_CONST;

GwyPixmapLayer*   gwy_data_view_get_base_layer    (GwyDataView *data_view);
GwyPixmapLayer*   gwy_data_view_get_alpha_layer   (GwyDataView *data_view);
GwyVectorLayer*   gwy_data_view_get_top_layer     (GwyDataView *data_view);
void              gwy_data_view_set_base_layer    (GwyDataView *data_view,
                                                   GwyPixmapLayer *layer);
void              gwy_data_view_set_alpha_layer   (GwyDataView *data_view,
                                                   GwyPixmapLayer *layer);
void              gwy_data_view_set_top_layer     (GwyDataView *data_view,
                                                   GwyVectorLayer *layer);
gdouble           gwy_data_view_get_hexcess       (GwyDataView* data_view);
gdouble           gwy_data_view_get_vexcess       (GwyDataView* data_view);
void              gwy_data_view_set_zoom          (GwyDataView *data_view,
                                                   gdouble zoom);
gdouble           gwy_data_view_get_zoom          (GwyDataView *data_view);
gdouble           gwy_data_view_get_real_zoom     (GwyDataView *data_view);
GwyContainer*     gwy_data_view_get_data          (GwyDataView *data_view);
void              gwy_data_view_coords_xy_clamp   (GwyDataView *data_view,
                                                   gint *xscr,
                                                   gint *yscr);
void              gwy_data_view_coords_xy_to_real (GwyDataView *data_view,
                                                   gint xscr,
                                                   gint yscr,
                                                   gdouble *xreal,
                                                   gdouble *yreal);
void              gwy_data_view_coords_real_to_xy (GwyDataView *data_view,
                                                   gdouble xreal,
                                                   gdouble yreal,
                                                   gint *xscr,
                                                   gint *yscr);
gdouble           gwy_data_view_get_xmeasure      (GwyDataView *data_view);
gdouble           gwy_data_view_get_ymeasure      (GwyDataView *data_view);
GdkPixbuf*        gwy_data_view_get_thumbnail     (GwyDataView *data_view,
                                                   gint size);
GdkPixbuf*        gwy_data_view_get_pixbuf        (GwyDataView *data_view,
                                                   gint max_width,
                                                   gint max_height);

G_END_DECLS

#endif /* __GWY_DATAVIEW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
