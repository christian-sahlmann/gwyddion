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

    GdkPixbuf *pixbuf;      /* everything, this is drawn on the screen */
    GdkPixbuf *base_pixbuf; /* unscaled base (lower layers) */
};

struct _GwyDataViewClass {
    GtkWidgetClass parent_class;
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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_DATAVIEW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
