/* @(#) $Id$ */

#ifndef __GWY_GRAPH_LABEL_H__
#define __GWY_GRAPH_LABEL_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>

#define GWY_GRAPH_LABEL_NORTHEAST 0
#define GWY_GRAPH_LABEL_NORTHWEST 1
#define GWY_GRAPH_LABEL_SOUTHEAST 2
#define GWY_GRAPH_LABEL_SOUTHWEST 3

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_GRAPH_LABEL            (gwy_graph_label_get_type())
#define GWY_GRAPH_LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_LABEL, GwyGraphLabel))
#define GWY_GRAPH_LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_LABEL, GwyGraphLabel))
#define GWY_IS_GRAPH_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_LABEL))
#define GWY_IS_GRAPH_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_LABEL))
#define GWY_GRAPH_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_LABEL, GwyGraphLabel))

/*single curve properties*/
typedef struct {
    gint is_line;
    gint is_point;

    gint point_size;
    gint point_type;

    GdkLineStyle line_style;

    GString *description;
    GdkColor color;
} GwyGraphAreaCurveParams;
    

typedef struct {
    gboolean is_frame;
    gint frame_thickness;
    
    gint position;
    gint sample_length;
    
    PangoFontDescription *font;
} GwyGraphLabelParams;

typedef struct {
    GtkWidget widget;

    GwyGraphLabelParams par; 
    gboolean is_visible;

    GPtrArray *curve_params;
} GwyGraphLabel;

typedef struct {
     GtkWidgetClass parent_class;
} GwyGraphLabelClass;


GtkWidget* gwy_graph_label_new();

GType gwy_graph_label_get_type(void) G_GNUC_CONST;

void gwy_graph_label_set_visible(GwyGraphLabel *label, gboolean is_visible);

void gwy_graph_label_set_style(GwyGraphLabel *label, GwyGraphLabelParams style);

void gwy_graph_label_add_curve(GwyGraphLabel *label, GwyGraphAreaCurveParams *params);

void gwy_graph_label_clear(GwyGraphLabel *label);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_AXIS_H__*/
