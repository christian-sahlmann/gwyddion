/* @(#) $Id$ */

#ifndef __GWY_LAYER_SELECT_H__
#define __GWY_LAYER_SELECT_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifndef GWY_TYPE_DATA_VIEW_LAYER
#  include <libgwydgets/gwydataviewlayer.h>
#endif /* no GWY_TYPE_DATA_VIEW_LAYER */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_LAYER_SELECT            (gwy_layer_select_get_type())
#define GWY_LAYER_SELECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_SELECT, GwyLayerSelect))
#define GWY_LAYER_SELECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_SELECT, GwyLayerSelectClass))
#define GWY_IS_LAYER_SELECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_SELECT))
#define GWY_IS_LAYER_SELECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_SELECT))
#define GWY_LAYER_SELECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_SELECT, GwyLayerSelectClass))

typedef struct _GwyLayerSelect      GwyLayerSelect;
typedef struct _GwyLayerSelectClass GwyLayerSelectClass;

struct _GwyLayerSelect {
    GwyDataViewLayer parent_instance;

    gboolean selected;
    gint near;
    guint button;
    gdouble x0;
    gdouble y0;
    gdouble x1;
    gdouble y1;
};

struct _GwyLayerSelectClass {
    GwyDataViewLayerClass parent_class;

    GdkCursor *corner_cursor[4];
    GdkCursor *resize_cursor;
};

GType            gwy_layer_select_get_type        (void) G_GNUC_CONST;

GtkObject*       gwy_layer_select_new             (void);
gboolean         gwy_layer_select_get_selection   (GwyDataViewLayer *layer,
                                                   gdouble *xmin,
                                                   gdouble *ymin,
                                                   gdouble *xmax,
                                                   gdouble *ymax);
void             gwy_layer_select_unselect        (GwyDataViewLayer *layer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_LAYER_SELECT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

