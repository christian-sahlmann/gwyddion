/* @(#) $Id$ */

#ifndef __GWY_LAYER_POINTER_H__
#define __GWY_LAYER_POINTER_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifndef GWY_TYPE_DATA_VIEW_LAYER
#  include <libgwydgets/gwydataviewlayer.h>
#endif /* no GWY_TYPE_DATA_VIEW_LAYER */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_LAYER_POINTER            (gwy_layer_pointer_get_type())
#define GWY_LAYER_POINTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_POINTER, GwyLayerPointer))
#define GWY_LAYER_POINTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_POINTER, GwyLayerPointerClass))
#define GWY_IS_LAYER_POINTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_POINTER))
#define GWY_IS_LAYER_POINTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_POINTER))
#define GWY_LAYER_POINTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_POINTER, GwyLayerPointerClass))

typedef struct _GwyLayerPointer      GwyLayerPointer;
typedef struct _GwyLayerPointerClass GwyLayerPointerClass;

struct _GwyLayerPointer {
    GwyDataViewLayer parent_instance;

    guint button;
    gboolean selected;
    gdouble x;
    gdouble y;
};

struct _GwyLayerPointerClass {
    GwyDataViewLayerClass parent_class;

    GdkCursor *point_cursor;
};

GType            gwy_layer_pointer_get_type       (void) G_GNUC_CONST;

GtkObject*       gwy_layer_pointer_new            (void);
gboolean         gwy_layer_pointer_get_point      (GwyDataViewLayer *layer,
                                                   gdouble *x,
                                                   gdouble *y);
void             gwy_layer_pointer_unselect       (GwyDataViewLayer *layer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_LAYER_POINTER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

