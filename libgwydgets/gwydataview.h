/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#ifndef GWY_TYPE_CONTAINER
#  include <libgwyddion/gwycontainer.h>
#endif /* no GWY_TYPE_CONTAINER */

#ifndef GWY_TYPE_DATA_VIEW_LAYER
#  include <libgwydgets/gwydataviewlayer.h>
#endif /* no GWY_TYPE_DATA_VIEW_LAYER */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

    GwyDataViewLayer *top_layer;
    GwyDataViewLayer *alpha_layer;
    GwyDataViewLayer *base_layer;

    gdouble zoom;    /* zoom (larger number means larger pixmaps) */
    gdouble newzoom;    /* new zoom value (when zoom is set, but widget not
                           yet resized) */
    gdouble xmeasure;    /* physical units per pixel */
    gdouble ymeasure;    /* physical units per pixel */
    gint xoff;    /* x offset of the pixbuf from widget->allocation.x */
    gint yoff;    /* y offset of the pixbuf from widget->allocation.y */

    GdkPixbuf *pixbuf;      /* everything, this is drawn on the screen */
    GdkPixbuf *base_pixbuf; /* unscaled base (lower layers) */

    gboolean force_update;
};

struct _GwyDataViewClass {
    GtkWidgetClass parent_class;

    void (*updated)(GwyDataView *data_view);
};

GtkWidget*        gwy_data_view_new               (GwyContainer *data);
GType             gwy_data_view_get_type          (void) G_GNUC_CONST;

GwyDataViewLayer* gwy_data_view_get_base_layer    (GwyDataView *data_view);
GwyDataViewLayer* gwy_data_view_get_alpha_layer   (GwyDataView *data_view);
GwyDataViewLayer* gwy_data_view_get_top_layer     (GwyDataView *data_view);
void              gwy_data_view_set_base_layer    (GwyDataView *data_view,
                                                   GwyDataViewLayer *layer);
void              gwy_data_view_set_alpha_layer   (GwyDataView *data_view,
                                                   GwyDataViewLayer *layer);
void              gwy_data_view_set_top_layer     (GwyDataView *data_view,
                                                   GwyDataViewLayer *layer);
gdouble           gwy_data_view_get_hexcess       (GwyDataView* data_view);
gdouble           gwy_data_view_get_vexcess       (GwyDataView* data_view);
void              gwy_data_view_set_zoom          (GwyDataView *data_view,
                                                   gdouble zoom);
gdouble           gwy_data_view_get_zoom          (GwyDataView *data_view);
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
void              gwy_data_view_update            (GwyDataView *data_view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_DATAVIEW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
