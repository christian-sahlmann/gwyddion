
#ifndef __GWY_DATALINE_H__
#define __GWY_DATALINE_H__
#include <glib.h>

#include "gwywatchable.h"
#include "gwyserializable.h"
#include "simplefft.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    
#define GWY_TYPE_DATALINE                  (gwy_dataline_get_type())
#define GWY_DATALINE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATALINE, GwyDataLine))
#define GWY_DATALINE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATALINE, GwyDataLine))
#define GWY_IS_DATALINE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATALINE))
#define GWY_IS_DATALINE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATALINE))
#define GWY_DATALINE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATALINE, GwyDataLine))
    
#ifndef __GWY_INTERPOLATION__
#define __GWY_INTERPOLATION__
#define GWY_INTERPOLATION_NONE      0
#define GWY_INTERPOLATION_ROUND     1
#define GWY_INTERPOLATION_BILINEAR  2
#define GWY_INTERPOLATION_KEY       3
#define GWY_INTERPOLATION_BSPLINE   4
#define GWY_INTERPOLATION_OMOMS     5
#define GWY_INTERPOLATION_NNA       6
#endif /*__GWY_INTERPOLATION__*/

/*provisory struct for field (for function arguments simplification)*/
typedef struct{
    gint res;      /*X resolution*/
    gdouble real;  /*X real field size (in nanometers)*/
    gdouble *data; /*data field*/
} GwyDataLine;

typedef struct{
    GObjectClass parent_class; 
} GwyDataLineClass;


GType gwy_dataline_get_type  (void) G_GNUC_CONST;

GObject* gwy_dataline_new(gint res, gdouble real, gboolean nullme);

/*allocate data line*/
gint gwy_dataline_alloc(GwyDataLine *a, gint res);

/*allocate data line, set its size and null all values (or not)*/
gint gwy_dataline_initialize(GwyDataLine *a, gint res, gdouble real, gboolean nullme);

/*free data line*/
void gwy_dataline_free(GwyDataLine *a);

/*resample data line (change resolution)*/
gint gwy_dataline_resample(GwyDataLine *a, gint res, gint interpolation);

/*resize data line according to from-to*/
gint gwy_dataline_resize(GwyDataLine *a, gint from, gint to);

/*copy everything to line (allrady allocated)*/
gint gwy_dataline_copy(GwyDataLine *a, GwyDataLine *b);


/************************************************************/
/*Get and set values of the struct members*/

/*simple operations*/
gint gwy_dataline_get_res(GwyDataLine *a);
gdouble gwy_dataline_get_real(GwyDataLine *a);
void gwy_dataline_set_real(GwyDataLine *a, gdouble real);

/*pixel <-> real coords transform*/
gdouble gwy_dataline_itor(GwyDataLine *a, gdouble pixval);
gdouble gwy_dataline_rtoi(GwyDataLine *a, gdouble realval);

/*data value at given pixel*/
gdouble gwy_dataline_get_val(GwyDataLine *a, gint i);
gint gwy_dataline_set_val(GwyDataLine *a, gint i, gdouble value);

/*data value interpolated somewhere between given pixels*/
gdouble gwy_dataline_get_dval(GwyDataLine *a, gdouble x, gint interpolation);

/*data value interpolated somewhere in the (xreal,yreal) coords*/
gdouble gwy_dataline_get_dval_real(GwyDataLine *a, gdouble x, gint interpolation);

/*FIXME move this somewhere, this has nothing to do with datafield:*/
gdouble gwy_dataline_get_dval_of_ddata(gdouble x, gdouble x1, gdouble y1, gdouble x2, gdouble y2, gint interpolation);

/*************************************************************************************/
/*Processing*/

/*invert line along x/y/z axis*/
gint gwy_dataline_invert(GwyDataLine *a, gboolean x, gboolean z);

/*fill, multiply or add something*/
void gwy_dataline_fill(GwyDataLine *a, gdouble value);
void gwy_dataline_multiply(GwyDataLine *a, gdouble value);
void gwy_dataline_add(GwyDataLine *a, gdouble value);
gint gwy_dataline_part_fill(GwyDataLine *a, gint from, gint to, gdouble value);
gint gwy_dataline_part_multiply(GwyDataLine *a, gint from, gint to, gdouble value);
gint gwy_dataline_part_add(GwyDataLine *a, gint from, gint to, gdouble value);

/*get some basic properties:*/
gdouble gwy_dataline_get_max(GwyDataLine *a);
gdouble gwy_dataline_get_min(GwyDataLine *a);
gdouble gwy_dataline_get_avg(GwyDataLine *a);
gdouble gwy_dataline_get_rms(GwyDataLine *a);
gdouble gwy_dataline_get_sum(GwyDataLine *a);
gdouble gwy_dataline_part_get_max(GwyDataLine *a, gint from, gint to);
gdouble gwy_dataline_part_get_min(GwyDataLine *a, gint from, gint to);
gdouble gwy_dataline_part_get_avg(GwyDataLine *a, gint from, gint to);
gdouble gwy_dataline_part_get_rms(GwyDataLine *a, gint from, gint to);
gdouble gwy_dataline_part_get_sum(GwyDataLine *a, gint from, gint to);

/*threshold dividing at thresval and setting to top and bottom*/
gint gwy_dataline_threshold(GwyDataLine *a, gdouble threshval, gdouble bottom, gdouble top);
gint gwy_dataline_part_threshold(GwyDataLine *a, gint from, gint to, gdouble threshval, gdouble bottom, gdouble top);

/*get 1st order line leveling coefficients*/
void gwy_dataline_line_coefs(GwyDataLine *a, gdouble *av, gdouble *bv);
void gwy_dataline_part_line_coefs(GwyDataLine *a, gint from, gint to, gdouble *av, gdouble *bv);

/*do 1st order line leveling*/
void gwy_dataline_line_level(GwyDataLine *a, gdouble av, gdouble bv);

/*do "rotation" along the y-axis by specified angle to do better line leveling*/
gint gwy_dataline_line_rotate(GwyDataLine *a, gdouble angle, gint interpolation);

/*get derivations (according to "real" sizes of field)*/
gdouble gwy_dataline_get_der(GwyDataLine *a, gint i);

/*1DFFT interface*/
gint gwy_dataline_fft(GwyDataLine *ra, GwyDataLine *ia, GwyDataLine *rb, GwyDataLine *ib, gint (*fft)(), gint windowing, gint direction,
		   gint interpolation, gboolean preserverms, gboolean level);

/*simple version of (*fft) function using fft_hum() from "simplefft.h"*/
gint gwy_dataline_fft_hum(gint direction, GwyDataLine *ra, GwyDataLine *ia, GwyDataLine *rb, GwyDataLine *ib, gint interpolation);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_DATALINE_H__*/




