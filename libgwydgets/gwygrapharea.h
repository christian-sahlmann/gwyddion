
#ifndef __GWY_GRAPH_AREA_H__
#define __GWY_GRAPH_AREA_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtklayout.h>

#include "gwygraphlabel.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_GRAPH_AREA            (gwy_graph_area_get_type())
#define GWY_GRAPH_AREA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_AREA, GwyGraphArea))
#define GWY_GRAPH_AREA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_AREA, GwyGraphArea))
#define GWY_IS_GRAPH_AREA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_AREA))
#define GWY_IS_GRAPH_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_AREA))
#define GWY_GRAPH_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_AREA, GwyGraphArea))


/*single curve data*/
typedef struct {
    gdouble *xvals;     /*screen coordinates of points*/
    gdouble *yvals;     /*screen coordinates of points*/
    gint N;              /*number of points*/
} GwyGraphAreaCurvePoints;

/*NOTE: GwyGraphAreaCurveParams is defined in gwygraphlabel.h*/

/*single curve*/
typedef struct {
    GwyGraphAreaCurvePoints data;       /*original data including its size*/
    GwyGraphAreaCurveParams params;     /*parameters of plot*/
    GdkPoint *points;			/*points to be directly plotted*/
} GwyGraphAreaCurve;

/*overall properties of area*/
typedef struct {
    
} GwyGraphAreaParams;

/*graph area structure*/
typedef struct {
    GtkLayout parent_instance;

    GdkGC *gc;
    				/*label*/
    GwyGraphLabel *lab;
    GwyGraphAreaParams par; 

    GPtrArray *curves;
    gdouble x_shift;
    gdouble y_shift;
    gdouble x_multiply;
    gdouble y_multiply;
    gdouble x_max;
    gdouble x_min;
    gdouble y_max;
    gdouble y_min;
    
} GwyGraphArea;

/*graph area class*/
typedef struct {
     GtkLayoutClass parent_class;
} GwyGraphAreaClass;


GtkWidget* gwy_graph_area_new(GtkAdjustment *hadjustment, GtkAdjustment *vadjustment);

GType gwy_graph_area_get_type(void) G_GNUC_CONST;

void gwy_graph_area_set_style(GwyGraphArea *area, GwyGraphAreaParams style);

void gwy_graph_area_set_boundaries(GwyGraphArea *area, gdouble x_min, gdouble x_max, gdouble y_min, gdouble y_max);

void gwy_graph_area_add_curve(GwyGraphArea *area, GwyGraphAreaCurve *curve);

void gwy_graph_area_clear(GwyGraphArea *area);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_AXIS_H__*/
