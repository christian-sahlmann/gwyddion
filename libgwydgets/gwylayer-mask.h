/* @(#) $Id$ */

#ifndef __GWY_LAYER_MASK_H__
#define __GWY_LAYER_MASK_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifndef GWY_TYPE_DATA_VIEW_LAYER
#  include <libgwydgets/gwydataviewlayer.h>
#endif /* no GWY_TYPE_DATA_VIEW_LAYER */

#ifndef GWY_TYPE_PALETTE_DEF
#  include <libdraw/gwypalettedef.h>
#endif /* no GWY_TYPE_PALETTE_DEF */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_LAYER_MASK            (gwy_layer_mask_get_type())
#define GWY_LAYER_MASK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_MASK, GwyLayerMask))
#define GWY_LAYER_MASK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_MASK, GwyLayerMaskClass))
#define GWY_IS_LAYER_MASK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_MASK))
#define GWY_IS_LAYER_MASK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_MASK))
#define GWY_LAYER_MASK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_MASK, GwyLayerMaskClass))

typedef struct _GwyLayerMask      GwyLayerMask;
typedef struct _GwyLayerMaskClass GwyLayerMaskClass;

struct _GwyLayerMask {
    GwyDataViewLayer parent_instance;

    GwyRGBA color;
    gboolean changed;
};

struct _GwyLayerMaskClass {
    GwyDataViewLayerClass parent_class;
};

GType            gwy_layer_mask_get_type        (void) G_GNUC_CONST;

GtkObject*       gwy_layer_mask_new             (void);
void             gwy_layer_mask_set_color       (GwyDataViewLayer *layer,
                                                 GwyRGBA *color);
GwyRGBA          gwy_layer_mask_get_color       (GwyDataViewLayer *layer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_LAYER_MASK_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

