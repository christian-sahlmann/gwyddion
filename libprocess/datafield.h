
#ifndef __GWY_DATAFIELD_H__
#define __GWY_DATAFIELD_H__
#include <glib.h>
#include "interpolation.h"
#include "dataline.h"
#include "gwywatchable.h"
#include "gwyserializable.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_DATAFIELD                  (gwy_data_field_get_type())
#define GWY_DATAFIELD(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATAFIELD, GwyDataField))
#define GWY_DATAFIELD_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATAFIELD, GwyDataField))
#define GWY_IS_DATAFIELD(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATAFIELD))
#define GWY_IS_DATAFIELD_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATAFIELD))
#define GWY_DATAFIELD_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATAFIELD, GwyDataField))


/*provisory struct for field (for function arguments simplification)*/
typedef struct{
    GObject parent_instance;

    gint xres;      /*X resolution*/
    gint yres;      /*Y resolution*/
    gdouble xreal;  /*X real field size (in nanometers)*/
    gdouble yreal;  /*Y real field size (in nanometers)*/
    gdouble *data;  /*data field*/
    gint N;	    /*xres*yres*/
} GwyDataField;

typedef struct{
        GObjectClass parent_class;
} GwyDataFieldClass;


GType gwy_data_field_get_type  (void) G_GNUC_CONST;

GObject* gwy_data_field_new(gint xres, gint yres, gdouble xreal, gdouble yreal, gboolean nullme);

/*allocation*/
gint gwy_data_field_alloc(GwyDataField *a, gint xres, gint yres);

/*allocate data field, set its size and null all values (or not)*/
gint gwy_data_field_initialize(GwyDataField *a, gint xres, gint yres, gdouble xreal, gdouble yreal, gboolean nullme);

/*free data field*/
void gwy_data_field_free(GwyDataField *a);

/*resample data field (change resolution)*/
gint gwy_data_field_resample(GwyDataField *a, gint xres, gint yres, gint interpolation);

/*resize data field according to UL (upper left) and BR (bottom right) points 
   (crop and change resolution if necessary)*/
gint gwy_data_field_resize(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj);

/*copy everything to field (allrady allocated)*/
gint gwy_data_field_copy(GwyDataField *a, GwyDataField *b);


/************************************************************/
/*Get and set values of the struct members*/

/*simple operations*/
gint gwy_data_field_get_xres(GwyDataField *a);
gint gwy_data_field_get_yres(GwyDataField *a);
gdouble gwy_data_field_get_xreal(GwyDataField *a);
gdouble gwy_data_field_get_yreal(GwyDataField *a);
void gwy_data_field_set_xreal(GwyDataField *a, gdouble xreal);
void gwy_data_field_set_yreal(GwyDataField *a, gdouble yreal);

/*pixel <-> real coords transform*/
gdouble gwy_data_field_itor(GwyDataField *a, gdouble pixval);
gdouble gwy_data_field_jtor(GwyDataField *a, gdouble pixval);
gdouble gwy_data_field_rtoi(GwyDataField *a, gdouble realval);
gdouble gwy_data_field_rtoj(GwyDataField *a, gdouble realval);


/*data value at given pixel*/
gboolean gwy_data_field_outside(GwyDataField *a, gint i, gint j);
gdouble gwy_data_field_get_val(GwyDataField *a, gint i, gint j);
void gwy_data_field_set_val(GwyDataField *a, gint i, gint j, gdouble value);

/*data value interpolated somewhere between given pixels*/
gdouble gwy_data_field_get_dval(GwyDataField *a, gdouble x, gdouble y, gint interpolation);

/*data value interpolated somewhere in the (xreal,yreal) coords*/
gdouble gwy_data_field_get_dval_real(GwyDataField *a, gdouble x, gdouble y, gint interpolation);

/*rotate field. Cut points that will be outside and null points not defined inside*/
gint gwy_data_field_rotate(GwyDataField *a, gdouble angle, gint interpolation);

/*invert field along x/y/z axis*/
gint gwy_data_field_invert(GwyDataField *a, gboolean x, gboolean y, gboolean z);

/*fill, multiply or add something*/
void gwy_data_field_fill(GwyDataField *a, gdouble value);
void gwy_data_field_multiply(GwyDataField *a, gdouble value);
void gwy_data_field_add(GwyDataField *a, gdouble value);
gint gwy_data_field_area_fill(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj, gdouble value);
gint gwy_data_field_area_multiply(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj, gdouble value);
gint gwy_data_field_area_add(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj, gdouble value);

/*get some basic properties:*/
gdouble gwy_data_field_get_max(GwyDataField *a);
gdouble gwy_data_field_get_min(GwyDataField *a);
gdouble gwy_data_field_get_avg(GwyDataField *a);
gdouble gwy_data_field_get_rms(GwyDataField *a);
gdouble gwy_data_field_get_sum(GwyDataField *a);
gdouble gwy_data_field_get_area_max(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj);
gdouble gwy_data_field_get_area_min(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj);
gdouble gwy_data_field_get_area_avg(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj);
gdouble gwy_data_field_get_area_rms(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj);
gdouble gwy_data_field_get_area_sum(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj);

/*threshold dividing at thresval and setting to top and bottom*/
gint gwy_data_field_threshold(GwyDataField *a, gdouble threshval, gdouble bottom, gdouble top);
gint gwy_data_field_area_threshold(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj, gdouble threshval, gdouble bottom, gdouble top);

/*data_line extraction*/
gint gwy_data_field_get_data_line(GwyDataField *a, GwyDataLine* b, gint uli, gint ulj, gint bri, gint brj, gint res, gint interpolation);
gint gwy_data_field_get_row(GwyDataField *a, GwyDataLine* b, gint i);
gint gwy_data_field_get_column(GwyDataField *a, GwyDataLine* b, gint i);
gint gwy_data_field_set_row(GwyDataField *a, GwyDataLine* b, gint i);
gint gwy_data_field_set_column(GwyDataField *a, GwyDataLine* b, gint i);

/*get 1st order plane leveling coefficients*/
gint gwy_data_field_plane_coefs(GwyDataField *a, gdouble *ap, gdouble *bp, gdouble *cp);

/*do 1st order plane leveling*/
gint gwy_data_field_plane_level(GwyDataField *a, gdouble ap, gdouble bp, gdouble cp);

/*do "rotation" along the x/y-axis by specified angle to do better plane leveling*/
gint gwy_data_field_plane_rotate(GwyDataField *a, gdouble xangle, gdouble yangle, gint interpolation);

/*get derivations (according to "real" sizes of field)*/
gdouble gwy_data_field_get_xder(GwyDataField *a, gint i, gint j);
gdouble gwy_data_field_get_yder(GwyDataField *a, gint i, gint j);
gdouble gwy_data_field_get_angder(GwyDataField *a, gint i, gint j, gdouble theta);

/*2DFFT using algorithm fft*/
gint gwy_data_field_2dfft(GwyDataField *ra, GwyDataField *ia, GwyDataField *rb, GwyDataField *ib, gint (*fft)(), 
		      gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level);
gint gwy_data_field_2dfft_real(GwyDataField *ra, GwyDataField *rb, GwyDataField *ib, gint (*fft)(), 
		      gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level);


/*humanize 2DFFT output*/
gint gwy_data_field_2dffthumanize(GwyDataField *a);

/*1DFFT along all profiles in specified direction*/
gint gwy_data_field_xfft(GwyDataField *ra, GwyDataField *ia, GwyDataField *rb, GwyDataField *ib, gint (*fft)(), 
		     gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level);
gint gwy_data_field_yfft(GwyDataField *ra, GwyDataField *ia, GwyDataField *rb, GwyDataField *ib, gint (*fft)(), 
		     gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level);
gint gwy_data_field_xfft_real(GwyDataField *ra, GwyDataField *rb, GwyDataField *ib, gint (*fft)(), 
		     gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level);
gint gwy_data_field_yfft_real(GwyDataField *ra, GwyDataField *rb, GwyDataField *ib, gint (*fft)(), 
		     gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*__GWY_DATAFIELD__*/

