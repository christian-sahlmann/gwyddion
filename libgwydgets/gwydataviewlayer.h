/* @(#) $Id$ */

#ifndef __GWY_DATAVIEWLAYER_H__
#define __GWY_DATAVIEWLAYER_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifndef GWY_TYPE_CONTAINER
#  include <libgwyddion/gwycontainer.h>
#endif /* no GWY_TYPE_CONTAINER */

#ifndef GWY_TYPE_PALETTE
#  include <libdraw/gwypalette.h>
#endif /* no GWY_TYPE_PALETTE */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_DATA_VIEW_LAYER            (gwy_data_view_layer_get_type())
#define GWY_DATA_VIEW_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_VIEW_LAYER, GwyDataViewLayer))
#define GWY_DATA_VIEW_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_VIEW_LAYER, GwyDataViewLayerClass))
#define GWY_IS_DATA_VIEW_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_VIEW_LAYER))
#define GWY_IS_DATA_VIEW_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_VIEW_LAYER))
#define GWY_DATA_VIEW_LAYER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_VIEW_LAYER, GwyDataViewLayerClass))

typedef struct _GwyDataViewLayer      GwyDataViewLayer;
typedef struct _GwyDataViewLayerClass GwyDataViewLayerClass;

struct _GwyDataViewLayer {
    GtkObject parent_instance;

    GtkWidget *parent;
    GwyContainer *data;
    GdkPixbuf *pixbuf;
    GwyPalette *palette;
    GdkGC *gc;
    PangoLayout *layout;
};

struct _GwyDataViewLayerClass {
    GtkObjectClass parent_class;

    /* renderers */
    GdkPixbuf* (*paint)(GwyDataViewLayer *layer);
    void (*draw)(GwyDataViewLayer *layer, GdkDrawable *drawable);
    gboolean (*wants_repaint)(GwyDataViewLayer *layer);
    /* events */
    gboolean (*button_press)(GwyDataViewLayer *layer, GdkEventButton *event);
    gboolean (*button_release)(GwyDataViewLayer *layer, GdkEventButton *event);
    gboolean (*motion_notify)(GwyDataViewLayer *layer, GdkEventMotion *event);
    gboolean (*key_press)(GwyDataViewLayer *layer, GdkEventKey *event);
    gboolean (*key_release)(GwyDataViewLayer *layer, GdkEventKey *event);
    /* signal functions */
    void (*plugged)(GwyDataViewLayer *layer);
    void (*unplugged)(GwyDataViewLayer *layer);
};

GType            gwy_data_view_layer_get_type        (void) G_GNUC_CONST;

gboolean         gwy_data_view_layer_is_vector       (GwyDataViewLayer *layer) G_GNUC_CONST;
gboolean         gwy_data_view_layer_wants_repaint   (GwyDataViewLayer *layer);
void             gwy_data_view_layer_draw            (GwyDataViewLayer *layer,
                                                      GdkDrawable *drawable);
GdkPixbuf*       gwy_data_view_layer_paint           (GwyDataViewLayer *layer);
gboolean         gwy_data_view_layer_button_press    (GwyDataViewLayer *layer,
                                                      GdkEventButton *event);
gboolean         gwy_data_view_layer_button_release  (GwyDataViewLayer *layer,
                                                      GdkEventButton *event);
gboolean         gwy_data_view_layer_motion_notify   (GwyDataViewLayer *layer,
                                                      GdkEventMotion *event);
gboolean         gwy_data_view_layer_key_press       (GwyDataViewLayer *layer,
                                                      GdkEventKey *event);
gboolean         gwy_data_view_layer_key_release     (GwyDataViewLayer *layer,
                                                      GdkEventKey *event);
void             gwy_data_view_layer_plugged         (GwyDataViewLayer *layer);
void             gwy_data_view_layer_unplugged       (GwyDataViewLayer *layer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_DATAVIEWLAYER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

