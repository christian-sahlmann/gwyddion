/* @(#) $Id$ */

#ifndef __GWY_LAYER_LINES_H__
#define __GWY_LAYER_LINES_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifndef GWY_TYPE_DATA_VIEW_LAYER
#  include <libgwydgets/gwydataviewlayer.h>
#endif /* no GWY_TYPE_DATA_VIEW_LAYER */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_LAYER_LINES            (gwy_layer_lines_get_type())
#define GWY_LAYER_LINES(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_LINES, GwyLayerLines))
#define GWY_LAYER_LINES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_LINES, GwyLayerLinesClass))
#define GWY_IS_LAYER_LINES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_LINES))
#define GWY_IS_LAYER_LINES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_LINES))
#define GWY_LAYER_LINES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_LINES, GwyLayerLinesClass))

typedef struct _GwyLayerLines      GwyLayerLines;
typedef struct _GwyLayerLinesClass GwyLayerLinesClass;

struct _GwyLayerLines {
    GwyDataViewLayer parent_instance;

    gint nlines;
    gint nselected;
    gint near;
    gboolean moving_line;
    gdouble lmove_x;
    gdouble lmove_y;
    guint button;
    gdouble *lines;
};

struct _GwyLayerLinesClass {
    GwyDataViewLayerClass parent_class;

    GdkCursor *near_cursor;
    GdkCursor *nearline_cursor;
    GdkCursor *move_cursor;
};

GType            gwy_layer_lines_get_type       (void) G_GNUC_CONST;

GtkObject*       gwy_layer_lines_new            (void);
void             gwy_layer_lines_set_max_lines  (GwyDataViewLayer *layer,
                                                 gint nlines);
gint             gwy_layer_lines_get_max_lines  (GwyDataViewLayer *layer);
gint             gwy_layer_lines_get_lines      (GwyDataViewLayer *layer,
                                                 gdouble *lines);
void             gwy_layer_lines_unselect       (GwyDataViewLayer *layer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_LAYER_LINES_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

