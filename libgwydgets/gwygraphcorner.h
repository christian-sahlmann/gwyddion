
#ifndef __GWY_GRAPH_CORNER_H__
#define __GWY_GRAPH_CORNER_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GWY_TYPE_GRAPH_CORNER            (gwy_graph_corner_get_type())
#define GWY_GRAPH_CORNER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_CORNER, GwyGraphCorner))
#define GWY_GRAPH_CORNER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_CORNER, GwyGraphCorner))
#define GWY_IS_GRAPH_CORNER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_CORNER))
#define GWY_IS_GRAPH_CORNER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_CORNER))
#define GWY_GRAPH_CORNER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_CORNER, GwyGraphCorner))
    

typedef struct {
    GtkWidget widget;

} GwyGraphCorner;

typedef struct {
     GtkWidgetClass parent_class;
} GwyGraphCornerClass;


GtkWidget* gwy_graph_corner_new();

GType gwy_graph_corner_get_type(void) G_GNUC_CONST;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_GRAPH_CORNER_H__*/
