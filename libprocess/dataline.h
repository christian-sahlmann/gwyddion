/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_DATALINE_H__
#define __GWY_DATALINE_H__
#include <glib-object.h>

#include <libprocess/simplefft.h>
#include <libprocess/interpolation.h>
#include <libgwyddion/gwywatchable.h>
#include <libgwyddion/gwyserializable.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_DATA_LINE                  (gwy_data_line_get_type())
#define GWY_DATA_LINE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_LINE, GwyDataLine))
#define GWY_DATA_LINE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_LINE, GwyDataLine))
#define GWY_IS_DATA_LINE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_LINE))
#define GWY_IS_DATA_LINE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_LINE))
#define GWY_DATA_LINE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_LINE, GwyDataLineClass))

typedef struct _GwyDataLine      GwyDataLine;
typedef struct _GwyDataLineClass GwyDataLineClass;

typedef enum {
  GWY_SF_OUTPUT_DH    = 0,  /*distribution of heights*/
  GWY_SF_OUTPUT_CDH   = 1,  /*cumulative distribution of heights*/
  GWY_SF_OUTPUT_DA    = 2,  /*distribution of angles*/
  GWY_SF_OUTPUT_CDA   = 3,  /*cumulative distribution of angles*/
  GWY_SF_OUTPUT_ACF   = 4,  /*autocorrelation fucntions*/
  GWY_SF_OUTPUT_HHCF  = 5,  /*height-height correlation function*/
  GWY_SF_OUTPUT_PSDF  = 6   /*power spectral density fucntion*/
} GwySFOutputType;


/*provisory struct for field (for function arguments simplification)*/
struct _GwyDataLine {
    GObject parent_instance;

    gint res;      /*X resolution*/
    gdouble real;  /*X real field size (in nanometers)*/
    gdouble *data; /*data field*/
};

struct _GwyDataLineClass {
    GObjectClass parent_class;
};


GType gwy_data_line_get_type  (void) G_GNUC_CONST;

GObject* gwy_data_line_new(gint res, gdouble real, gboolean nullme);

/*allocate data line*/
void gwy_data_line_alloc(GwyDataLine *a, gint res);

/*allocate data line, set its size and null all values (or not)*/
void gwy_data_line_initialize(GwyDataLine *a, gint res, gdouble real, gboolean nullme);

/*free data line*/
void gwy_data_line_free(GwyDataLine *a);

/*resample data line (change resolution)*/
void gwy_data_line_resample(GwyDataLine *a, gint res, gint interpolation);

/*resize data line according to from-to*/
gboolean gwy_data_line_resize(GwyDataLine *a, gint from, gint to);

/*copy everything to line (allrady allocated)*/
gboolean gwy_data_line_copy(GwyDataLine *a, GwyDataLine *b);


/************************************************************/
/*Get and set values of the struct members*/

/*simple operations*/
gint gwy_data_line_get_res(GwyDataLine *a);
gdouble gwy_data_line_get_real(GwyDataLine *a);
void gwy_data_line_set_real(GwyDataLine *a, gdouble real);

/*pixel <-> real coords transform*/
gdouble gwy_data_line_itor(GwyDataLine *a, gdouble pixval);
gdouble gwy_data_line_rtoi(GwyDataLine *a, gdouble realval);

/*data value at given pixel*/
gdouble gwy_data_line_get_val(GwyDataLine *a, gint i);
gint gwy_data_line_set_val(GwyDataLine *a, gint i, gdouble value);

/*data value interpolated somewhere between given pixels*/
gdouble gwy_data_line_get_dval(GwyDataLine *a, gdouble x, gint interpolation);

/*data value interpolated somewhere in the (xreal,yreal) coords*/
gdouble gwy_data_line_get_dval_real(GwyDataLine *a, gdouble x, gint interpolation);

/*************************************************************************************/
/*Processing*/

/*invert line along x/y/z axis*/
void gwy_data_line_invert(GwyDataLine *a, gboolean x, gboolean z);

/*fill, multiply or add something*/
void gwy_data_line_fill(GwyDataLine *a, gdouble value);
void gwy_data_line_multiply(GwyDataLine *a, gdouble value);
void gwy_data_line_add(GwyDataLine *a, gdouble value);
void gwy_data_line_part_fill(GwyDataLine *a, gint from, gint to, gdouble value);
void gwy_data_line_part_multiply(GwyDataLine *a, gint from, gint to, gdouble value);
void gwy_data_line_part_add(GwyDataLine *a, gint from, gint to, gdouble value);

/*get some basic properties:*/
gdouble gwy_data_line_get_max(GwyDataLine *a);
gdouble gwy_data_line_get_min(GwyDataLine *a);
gdouble gwy_data_line_get_avg(GwyDataLine *a);
gdouble gwy_data_line_get_rms(GwyDataLine *a);
gdouble gwy_data_line_get_sum(GwyDataLine *a);
gdouble gwy_data_line_part_get_max(GwyDataLine *a, gint from, gint to);
gdouble gwy_data_line_part_get_min(GwyDataLine *a, gint from, gint to);
gdouble gwy_data_line_part_get_avg(GwyDataLine *a, gint from, gint to);
gdouble gwy_data_line_part_get_rms(GwyDataLine *a, gint from, gint to);
gdouble gwy_data_line_part_get_sum(GwyDataLine *a, gint from, gint to);

/*threshold dividing at thresval and setting to top and bottom*/
gint gwy_data_line_threshold(GwyDataLine *a, gdouble threshval, gdouble bottom, gdouble top);
gint gwy_data_line_part_threshold(GwyDataLine *a, gint from, gint to, gdouble threshval, gdouble bottom, gdouble top);

/*get 1st order line leveling coefficients*/
void gwy_data_line_line_coeffs(GwyDataLine *a, gdouble *av, gdouble *bv);
void gwy_data_line_part_line_coeffs(GwyDataLine *a, gint from, gint to, gdouble *av, gdouble *bv);

/*do 1st order line leveling*/
void gwy_data_line_line_level(GwyDataLine *a, gdouble av, gdouble bv);

/*do "rotation" along the y-axis by specified angle to do better line leveling*/
void gwy_data_line_line_rotate(GwyDataLine *a, gdouble angle, gint interpolation);

/*get derivations (according to "real" sizes of field)*/
gdouble gwy_data_line_get_der(GwyDataLine *a, gint i);

/*1DFFT interface*/
void gwy_data_line_fft(GwyDataLine *ra, GwyDataLine *ia,
                       GwyDataLine *rb, GwyDataLine *ib,
                       void (*fft)(), GwyWindowingType windowing,
                       gint direction, GwyInterpolationType interpolation,
                       gboolean preserverms, gboolean level);

/*simple version of (*fft) function using fft_hum() from "simplefft.h"*/
void gwy_data_line_fft_hum(gint direction, GwyDataLine *ra, GwyDataLine *ia,
                           GwyDataLine *rb, GwyDataLine *ib,
                           gint interpolation);

/*distribution of heights*/
void gwy_data_line_dh(GwyDataLine *data_line,
                      GwyDataLine *target_line,
                      gdouble ymin,
                      gdouble ymax,
                      gint nsteps);

/*cumulative distribution of heights*/
void gwy_data_line_cdh(GwyDataLine *data_line,
                      GwyDataLine *target_line,
                      gdouble ymin,
                      gdouble ymax,
                      gint nsteps);

/*distribution of slopes*/
void gwy_data_line_da(GwyDataLine *data_line,
                      GwyDataLine *target_line,
                      gdouble ymin,
                      gdouble ymax,
                      gint nsteps);

/*cumulative distribution of slopes*/
void gwy_data_line_cda(GwyDataLine *data_line,
                      GwyDataLine *target_line,
                      gdouble ymin,
                      gdouble ymax,
                      gint nsteps);

/*autocorrelation function*/
void gwy_data_line_acf(GwyDataLine *data_line,
                      GwyDataLine *target_line);

/*height-height correlation function*/
void gwy_data_line_hhcf(GwyDataLine *data_line,
                      GwyDataLine *target_line);

/*power spectral density function*/
void gwy_data_line_psdf(GwyDataLine *data_line,
                      GwyDataLine *target_line,
		      gint windowing,
                      gint interpolation);

gdouble* gwy_data_line_part_fit_polynom (GwyDataLine *data_line,
                                         gint n,
                                         gdouble *coeffs,
                                         gint from,
                                         gint to);
gdouble* gwy_data_line_fit_polynom      (GwyDataLine *data_line,
                                         gint n,
                                         gdouble *coeffs);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_DATALINE_H__*/


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

