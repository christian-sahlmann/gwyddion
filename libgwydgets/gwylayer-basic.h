#ifndef __GWY_LAYER_BASIC_H__
#define __GWY_LAYER_BASIC_H__

#include <gdk/gdk.h>
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

#define GWY_TYPE_LAYER_BASIC            (gwy_layer_basic_get_type())
#define GWY_LAYER_BASIC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_BASIC, GwyLayerBasic))
#define GWY_LAYER_BASIC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_BASIC, GwyLayerBasicClass))
#define GWY_IS_LAYER_BASIC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_BASIC))
#define GWY_IS_LAYER_BASIC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_BASIC))
#define GWY_LAYER_BASIC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_BASIC, GwyLayerBasicClass))

typedef struct _GwyLayerBasic      GwyLayerBasic;
typedef struct _GwyLayerBasicClass GwyLayerBasicClass;

struct _GwyLayerBasic {
    GwyDataViewLayer parent_instance;
};

struct _GwyLayerBasicClass {
    GwyDataViewLayerClass parent_class;
};

GType            gwy_layer_basic_get_type        (void) G_GNUC_CONST;

GtkObject*       gwy_layer_basic_new             (GwyContainer *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_LAYER_BASIC_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

