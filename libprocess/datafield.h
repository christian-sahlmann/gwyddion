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

#ifndef __GWY_DATAFIELD_H__
#define __GWY_DATAFIELD_H__
#include <glib.h>
#include <gtk/gtk.h>
#include <libprocess/interpolation.h>
#include <libprocess/dataline.h>
#include <libgwyddion/gwywatchable.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwysiunit.h>
#include <libprocess/cwt.h>

G_BEGIN_DECLS

#define GWY_TYPE_DATA_FIELD                  (gwy_data_field_get_type())
#define GWY_DATA_FIELD(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_FIELD, GwyDataField))
#define GWY_DATA_FIELD_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_FIELD, GwyDataFieldClass))
#define GWY_IS_DATA_FIELD(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_FIELD))
#define GWY_IS_DATA_FIELD_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_FIELD))
#define GWY_DATA_FIELD_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_FIELD, GwyDataFieldClass))

typedef struct _GwyDataField      GwyDataField;
typedef struct _GwyDataFieldClass GwyDataFieldClass;

typedef enum {
      GWY_FILTER_MEAN          = 0, /*mean value filter (averaging)*/
      GWY_FILTER_MEDIAN        = 1, /*median value filter*/
      GWY_FILTER_CONSERVATIVE  = 2, /*conservative denoising filter*/
      GWY_FILTER_LAPLACIAN     = 3, /*Laplacian 2nd derivative filter*/
      GWY_FILTER_SOBEL         = 4, /*Sobel gradient filter*/
      GWY_FILTER_PREWITT       = 5  /*Prewitt gradient filter*/
} GwyFilterType;

#ifndef GWY_DISABLE_DEPRECATED
/* XXX: never used in libprocess itself */
typedef enum {
    GWY_MERGE_UNION        = 0, /*union of all found grains*/
    GWY_MERGE_INTERSECTION = 1  /*intersection of grains found by different methods*/
} GwyMergeType;
#endif

typedef enum {
    GWY_WSHED_INIT         = 0, /*start initializations*/
    GWY_WSHED_LOCATE       = 1, /*locate steps*/
    GWY_WSHED_MIN          = 2, /*find minima*/
    GWY_WSHED_WSHED        = 3, /*watershed steps*/
    GWY_WSHED_MARK         = 4, /*mark grain boundaries*/
    GWY_WSHED_FINISHED     = 5
} GwyWatershedStateType;

typedef enum {
    GWY_COMP_INIT         = 0, /*start initializations*/
    GWY_COMP_ITERATE      = 1, /*locate steps*/
    GWY_COMP_FINISHED     = 2
} GwyComputationStateType;

#ifndef GWY_DISABLE_DEPRECATED
/* XXX: this is not *fractal* *type* at all */
/* XXX: never used in libprocess itself */
typedef enum {
    GWY_FRACTAL_PARTITIONING  = 0,
    GWY_FRACTAL_CUBECOUNTING  = 1,
    GWY_FRACTAL_TRIANGULATION = 2,
    GWY_FRACTAL_PSDF          = 3
} GwyFractalType;
#endif

typedef struct {
    GwyWatershedStateType state;
    gint internal_i;
    GwyDataField *min;
    GwyDataField *water;
    GwyDataField *mark_dfield;
    gint fraction;
    GString *description;
} GwyWatershedStatus;


struct _GwyDataField {
    GObject parent_instance;

    gint xres;      /*X resolution*/
    gint yres;      /*Y resolution*/
    gdouble xreal;  /*X real field size (in nanometers)*/
    gdouble yreal;  /*Y real field size (in nanometers)*/
    gdouble *data;  /*data field*/
    gint N;	    /*xres*yres*/
    GwySIUnit *si_unit_xy; /*SI unit corresponding to XY axis*/
    GwySIUnit *si_unit_z; /*SI unit corresponding to height (Z) axis*/
};

struct _GwyDataFieldClass {
    GObjectClass parent_class;
};


GType gwy_data_field_get_type  (void) G_GNUC_CONST;

GObject* gwy_data_field_new(gint xres,
                            gint yres,
                            gdouble xreal,
                            gdouble yreal,
                            gboolean nullme);

#ifndef GWY_DISABLE_DEPRECATED
void gwy_data_field_alloc(GwyDataField *a,
                          gint xres,
                          gint yres);
void gwy_data_field_initialize(GwyDataField *a,
                               gint xres,
                               gint yres,
                               gdouble xreal,
                               gdouble yreal,
                               gboolean nullme);
#endif

/*resample data field (change resolution)*/
void gwy_data_field_resample(GwyDataField *a,
                             gint xres,
                             gint yres,
                             GwyInterpolationType interpolation);

/*resize data field according to UL (upper left) and BR (bottom right) points
  (crop and change resolution if necessary)*/
gboolean gwy_data_field_resize(GwyDataField *a,
                               gint ulcol,
                               gint ulrow,
                               gint brcol,
                               gint brrow);

/*copy everything to field (allrady allocated)*/
gboolean gwy_data_field_copy(GwyDataField *a,
                             GwyDataField *b);
gboolean gwy_data_field_area_copy (GwyDataField *src,
                                   GwyDataField *dest,
                                   gint ulcol,
                                   gint ulrow,
                                   gint brcol,
                                   gint brrow,
                                   gint destcol,
                                   gint destrow);

/************************************************************/
/*Get and set values of the struct members*/

/*simple operations*/
gdouble* gwy_data_field_get_data(GwyDataField *a);
gint gwy_data_field_get_xres(GwyDataField *a);
gint gwy_data_field_get_yres(GwyDataField *a);
gdouble gwy_data_field_get_xreal(GwyDataField *a);
gdouble gwy_data_field_get_yreal(GwyDataField *a);
void gwy_data_field_set_xreal(GwyDataField *a,
                              gdouble xreal);
void gwy_data_field_set_yreal(GwyDataField *a,
                              gdouble yreal);

GwySIUnit* gwy_data_field_get_si_unit_xy(GwyDataField *a);
GwySIUnit* gwy_data_field_get_si_unit_z(GwyDataField *a);
void gwy_data_field_set_si_unit_xy(GwyDataField *a,
                                   GwySIUnit *si_unit);
void gwy_data_field_set_si_unit_z(GwyDataField *a,
                                  GwySIUnit *si_unit);
GwySIValueFormat* gwy_data_field_get_value_format_xy(GwyDataField *data_field,
                                                     GwySIValueFormat *format);
GwySIValueFormat* gwy_data_field_get_value_format_z(GwyDataField *data_field,
                                                    GwySIValueFormat *format);

/*pixel <-> real coords transform*/
gdouble gwy_data_field_itor(GwyDataField *a,
                            gdouble pixval);
gdouble gwy_data_field_jtor(GwyDataField *a,
                            gdouble pixval);
gdouble gwy_data_field_rtoi(GwyDataField *a,
                            gdouble realval);
gdouble gwy_data_field_rtoj(GwyDataField *a,
                            gdouble realval);


/*data value at given pixel*/
gboolean gwy_data_field_outside(GwyDataField *a,
                                gint col,
                                gint row);
gdouble gwy_data_field_get_val(GwyDataField *a,
                               gint col,
                               gint row);
void gwy_data_field_set_val(GwyDataField *a,
                            gint col,
                            gint row,
                            gdouble value);

/*data value interpolated somewhere between given pixels*/
gdouble gwy_data_field_get_dval(GwyDataField *a,
                                gdouble x,
                                gdouble y,
                                GwyInterpolationType interpolation);

/*data value interpolated somewhere in the (xreal,yreal) coords*/
gdouble gwy_data_field_get_dval_real(GwyDataField *a,
                                     gdouble x,
                                     gdouble y,
                                     GwyInterpolationType interpolation);

/*rotate field. Cut points that will be outside and null points not defined inside*/
void gwy_data_field_rotate(GwyDataField *a,
                           gdouble angle,
                           GwyInterpolationType interpolation);

/*invert field along x/y/z axis*/
void gwy_data_field_invert(GwyDataField *a,
                           gboolean x,
                           gboolean y,
                           gboolean z);

/*fill,
  multiply or add something*/
void gwy_data_field_fill(GwyDataField *a,
                         gdouble value);
void gwy_data_field_multiply(GwyDataField *a,
                             gdouble value);
void gwy_data_field_add(GwyDataField *a,
                        gdouble value);
void gwy_data_field_area_fill(GwyDataField *a,
                              gint ulcol,
                              gint ulrow,
                              gint brcol,
                              gint brrow,
                              gdouble value);
void gwy_data_field_area_multiply(GwyDataField *a,
                                  gint ulcol,
                                  gint ulrow,
                                  gint brcol,
                                  gint brrow,
                                  gdouble value);
void gwy_data_field_area_add(GwyDataField *a,
                             gint ulcol,
                             gint ulrow,
                             gint brcol,
                             gint brrow,
                             gdouble value);

/*get some basic properties:*/
gdouble gwy_data_field_get_max(GwyDataField *a);
gdouble gwy_data_field_get_min(GwyDataField *a);
gdouble gwy_data_field_get_avg(GwyDataField *a);
gdouble gwy_data_field_get_rms(GwyDataField *a);
gdouble gwy_data_field_get_sum(GwyDataField *a);
gdouble gwy_data_field_get_surface_area(GwyDataField *a,
                                        GwyInterpolationType interpolation);

#ifndef GWY_DISABLE_DEPRECATED
gdouble gwy_data_field_get_area_max(GwyDataField *a,
                                    gint ulcol,
                                    gint ulrow,
                                    gint brcol,
                                    gint brrow);
gdouble gwy_data_field_get_area_min(GwyDataField *a,
                                    gint ulcol,
                                    gint ulrow,
                                    gint brcol,
                                    gint brrow);
gdouble gwy_data_field_get_area_avg(GwyDataField *a,
                                    gint ulcol,
                                    gint ulrow,
                                    gint brcol,
                                    gint brrow);
gdouble gwy_data_field_get_area_rms(GwyDataField *a,
                                    gint ulcol,
                                    gint ulrow,
                                    gint brcol,
                                    gint brrow);
gdouble gwy_data_field_get_area_sum(GwyDataField *a,
                                    gint ulcol,
                                    gint ulrow,
                                    gint brcol,
                                    gint brrow);
gdouble gwy_data_field_get_area_surface_area(GwyDataField *a,
                                        gint ulcol,
                                        gint ulrow,
                                        gint brcol,
                                        gint brrow,
                                        GwyInterpolationType interpolation);
#endif

gdouble gwy_data_field_area_get_max(GwyDataField *dfield,
                                    gint col,
                                    gint row,
                                    gint width,
                                    gint height);
gdouble gwy_data_field_area_get_min(GwyDataField *dfield,
                                    gint col,
                                    gint row,
                                    gint width,
                                    gint height);
gdouble gwy_data_field_area_get_avg(GwyDataField *dfield,
                                    gint col,
                                    gint row,
                                    gint width,
                                    gint height);
gdouble gwy_data_field_area_get_rms(GwyDataField *dfield,
                                    gint col,
                                    gint row,
                                    gint width,
                                    gint height);
gdouble gwy_data_field_area_get_sum(GwyDataField *dfield,
                                    gint col,
                                    gint row,
                                    gint width,
                                    gint height);
gdouble gwy_data_field_area_get_surface_area(GwyDataField *dfield,
                                        gint col,
                                        gint row,
                                        gint width,
                                        gint height,
                                        GwyInterpolationType interpolation);

/*threshold dividing at thresval and setting to top and bottom*/
gint gwy_data_field_threshold(GwyDataField *a,
                              gdouble threshval,
                              gdouble bottom,
                              gdouble top);
gint gwy_data_field_area_threshold(GwyDataField *a,
                                   gint ulcol,
                                   gint ulrow,
                                   gint brcol,
                                   gint brrow,
                                   gdouble threshval,
                                   gdouble bottom,
                                   gdouble top);
gint gwy_data_field_clamp(GwyDataField *a,
                          gdouble bottom,
                          gdouble top);
gint gwy_data_field_area_clamp(GwyDataField *a,
                               gint ulcol,
                               gint ulrow,
                               gint brcol,
                               gint brrow,
                               gdouble bottom,
                               gdouble top);

/*data_line extraction*/
gboolean gwy_data_field_get_data_line(GwyDataField *a,
                                      GwyDataLine* b,
                                      gint ulcol,
                                      gint ulrow,
                                      gint brcol,
                                      gint brrow,
                                      gint res,
                                      GwyInterpolationType interpolation);

gboolean gwy_data_field_get_data_line_averaged(GwyDataField *a,
                                      GwyDataLine* b,
                                      gint ulcol,
                                      gint ulrow,
                                      gint brcol,
                                      gint brrow,
                                      gint res,
                                      gint thickness,
                                      GwyInterpolationType interpolation);

void gwy_data_field_get_row(GwyDataField *a,
                            GwyDataLine* b,
                            gint row);
void gwy_data_field_get_column(GwyDataField *a,
                               GwyDataLine* b,
                               gint col);
void gwy_data_field_set_row(GwyDataField *a,
                            GwyDataLine* b,
                            gint row);
void gwy_data_field_set_column(GwyDataField *a,
                               GwyDataLine* b,
                               gint col);

void gwy_data_field_get_row_part(GwyDataField *a,
                                 GwyDataLine* b,
                                 gint row,
                                 gint from,
                                 gint to);
void gwy_data_field_get_column_part(GwyDataField *a,
                                 GwyDataLine* b,
                                 gint col,
                                 gint from,
                                 gint to);



void gwy_data_field_area_fit_plane(GwyDataField *dfield,
                                   gint col,
                                   gint row,
                                   gint width,
                                   gint height,
                                   gdouble *pa,
                                   gdouble *pbx,
                                   gdouble *pby);

/*get 1st order plane leveling coefficients*/
void gwy_data_field_plane_coeffs(GwyDataField *a,
                                 gdouble *ap,
                                 gdouble *bp,
                                 gdouble *cp);

/*do 1st order plane leveling*/
void gwy_data_field_plane_level(GwyDataField *a,
                                gdouble ap,
                                gdouble bp,
                                gdouble cp);

/*do "rotation" along the x/y-axis by specified angle to do better plane leveling*/
void gwy_data_field_plane_rotate(GwyDataField *a,
                                 gdouble xangle,
                                 gdouble yangle,
                                 GwyInterpolationType interpolation);

/*get derivations (according to "real" sizes of field)*/
gdouble gwy_data_field_get_xder(GwyDataField *a,
                                gint col,
                                gint row);
gdouble gwy_data_field_get_yder(GwyDataField *a,
                                gint col,
                                gint row);
gdouble gwy_data_field_get_angder(GwyDataField *a,
                                  gint col,
                                  gint row,
                                  gdouble theta);

/*2DFFT using algorithm fft*/
gint gwy_data_field_get_fft_res(gint data_res);

void gwy_data_field_2dfft(GwyDataField *ra,
                          GwyDataField *ia,
                          GwyDataField *rb,
                          GwyDataField *ib,
                          void (*fft)(),
                          GwyWindowingType windowing,
                          gint direction,
                          GwyInterpolationType interpolation,
                          gboolean preserverms,
                          gboolean level);
void gwy_data_field_2dfft_real(GwyDataField *ra,
                               GwyDataField *rb,
                               GwyDataField *ib,
                               void (*fft)(),
                               GwyWindowingType windowing,
                               gint direction,
                               GwyInterpolationType interpolation,
                               gboolean preserverms,
                               gboolean level);


/*humanize 2DFFT output*/
void gwy_data_field_2dffthumanize(GwyDataField *a);

/*1DFFT along all profiles in specified direction*/
void gwy_data_field_xfft(GwyDataField *ra,
                         GwyDataField *ia,
                         GwyDataField *rb,
                         GwyDataField *ib,
                         void (*fft)(),
                         GwyWindowingType windowing,
                         gint direction,
                         GwyInterpolationType interpolation,
                         gboolean preserverms,
                         gboolean level);
void gwy_data_field_yfft(GwyDataField *ra,
                         GwyDataField *ia,
                         GwyDataField *rb,
                         GwyDataField *ib,
                         void (*fft)(),
                         GwyWindowingType windowing,
                         gint direction,
                         GwyInterpolationType interpolation,
                         gboolean preserverms,
                         gboolean level);
void gwy_data_field_xfft_real(GwyDataField *ra,
                              GwyDataField *rb,
                              GwyDataField *ib,
                              void (*fft)(),
                              GwyWindowingType windowing,
                              gint direction,
                              GwyInterpolationType interpolation,
                              gboolean preserverms,
                              gboolean level);
void gwy_data_field_yfft_real(GwyDataField *ra,
                              GwyDataField *rb,
                              GwyDataField *ib,
                              void (*fft)(),
                              GwyWindowingType windowing,
                              gint direction,
                              GwyInterpolationType interpolation,
                              gboolean preserverms,
                              gboolean level);


void gwy_data_field_cwt(GwyDataField *data_field,
                        GwyInterpolationType interpolation,
                        gdouble scale,
                        Gwy2DCWTWaveletType wtype);


void gwy_data_field_shade(GwyDataField *data_field,
                          GwyDataField *target_field,
                          gdouble theta,
                          gdouble phi);

void gwy_data_field_get_stats(GwyDataField *data_field,
                              gdouble *avg,
                              gdouble *ra,
                              gdouble *rms,
                              gdouble *skew,
                              gdouble *kurtosis);

#ifndef GWY_DISABLE_DEPRECATED
void gwy_data_field_get_area_stats(GwyDataField *data_field,
                                   gint ulcol,
                                   gint ulrow,
                                   gint brcol,
                                   gint brrow,
                                   gdouble *avg,
                                   gdouble *ra,
                                   gdouble *rms,
                                   gdouble *skew,
                                   gdouble *kurtosis);
#endif
void gwy_data_field_area_get_stats(GwyDataField *dfield,
                                    gint col,
                                    gint row,
                                    gint width,
                                    gint height,
                                    gdouble *avg,
                                    gdouble *ra,
                                    gdouble *rms,
                                    gdouble *skew,
                                    gdouble *kurtosis);

gint gwy_data_field_get_line_stat_function(GwyDataField *data_field,
                                           GwyDataLine *target_line,
                                           gint ulcol,
                                           gint ulrow,
                                           gint brcol,
                                           gint brrow,
                                           GwySFOutputType type,
                                           GtkOrientation orientation,
                                           GwyInterpolationType interpolation,
                                           GwyWindowingType windowing,
                                           gint nstats);

void gwy_data_field_filter_median(GwyDataField *data_field,
                                  gint size,
                                  gint ulcol,
                                  gint ulrow,
                                  gint brcol,
                                  gint brrow
                                  );

void gwy_data_field_filter_mean(GwyDataField *data_field,
                                gint size,
                                gint ulcol,
                                gint ulrow,
                                gint brcol,
                                gint brrow
                                );

void gwy_data_field_filter_conservative(GwyDataField *data_field,
                                        gint size,
                                        gint ulcol,
                                        gint ulrow,
                                        gint brcol,
                                        gint brrow
                                        );

void gwy_data_field_filter_laplacian(GwyDataField *data_field,
                                     gint ulcol,
                                     gint ulrow,
                                     gint brcol,
                                     gint brrow
                                      );

void gwy_data_field_filter_sobel(GwyDataField *data_field,
                                 GtkOrientation orientation,
                                 gint ulcol,
                                 gint ulrow,
                                 gint brcol,
                                 gint brrow
                                 );

void gwy_data_field_filter_prewitt(GwyDataField *data_field,
                                   GtkOrientation orientation,
                                   gint ulcol,
                                   gint ulrow,
                                   gint brcol,
                                   gint brrow
                                   );

void gwy_data_field_convolve(GwyDataField *data_field,
                             GwyDataField *kernel_field,
                             gint ulcol,
                             gint ulrow,
                             gint brcol,
                             gint brrow
                              );



void gwy_data_field_grains_mark_local_maxima(GwyDataField *data_field,
                                             GwyDataField *grain_field);

void gwy_data_field_grains_mark_height(GwyDataField *data_field,
                                       GwyDataField *grain_field,
                                       gdouble threshval,
                                       gint dir);

void gwy_data_field_grains_mark_slope(GwyDataField *data_field,
                                      GwyDataField *grain_field,
                                      gdouble threshval,
                                      gint dir);

void gwy_data_field_grains_mark_curvature(GwyDataField *data_field,
                                          GwyDataField *grain_field,
                                          gdouble threshval,
                                          gint dir);

void gwy_data_field_grains_mark_watershed(GwyDataField *data_field,
                                          GwyDataField *grain_field,
                                          gint locate_steps,
                                          gint locate_thresh,
                                          gdouble locate_dropsize,
                                          gint wshed_steps,
                                          gdouble wshed_dropsize,
                                          gboolean prefilter,
                                          gint dir);

void gwy_data_field_grains_remove_manually(
                                           GwyDataField *grain_field,
                                           gint i);

void gwy_data_field_grains_remove_by_size(
                                          GwyDataField *grain_field,
                                          gint size);

void gwy_data_field_grains_remove_by_height(GwyDataField *data_field,
                                            GwyDataField *grain_field,
                                            gdouble threshval,
                                            gint direction);

void gwy_data_field_grains_watershed_iteration(GwyDataField *data_field,
                                               GwyDataField *grain_field,
                                               GwyWatershedStatus *status,
                                               gint locate_steps,
                                               gint locate_thresh,
                                               gdouble locate_dropsize,
                                               gint wshed_steps,
                                               gdouble wshed_dropsize,
                                               gboolean prefilter,
                                               gint dir);


gdouble gwy_data_field_grains_get_average(GwyDataField *grain_field);

void gwy_data_field_grains_get_distribution(GwyDataField *grain_field,
                                            GwyDataLine *distribution);

void gwy_data_field_grains_add(GwyDataField *grain_field,
                              GwyDataField *add_field);

void gwy_data_field_grains_intersect(GwyDataField *grain_field,
                                     GwyDataField *intersect_field);

void gwy_data_field_fit_lines(GwyDataField *data_field,
                              gint ulcol,
                              gint ulrow,
                              gint brcol,
                              gint brrow,
                              GwyFitLineType fit_type,
                              gboolean exclude,
                              GtkOrientation orientation
                              );

gdouble gwy_data_field_get_correlation_score(GwyDataField *data_field,
                                      GwyDataField *kernel_field,
                                      gint ulcol,
                                      gint ulrow,
                                      gint kernel_ulcol,
                                      gint kernel_ulrow,
                                      gint kernel_brcol,
                                      gint kernel_brrow
                                     );

void gwy_data_field_crosscorrelate(GwyDataField *data_field1,
                                   GwyDataField *data_field2,
                                   GwyDataField *x_dist,
                                   GwyDataField *y_dist,
                                   GwyDataField *score,
                                   gint search_width,
                                   gint search_height,
                                   gint window_width,
                                   gint window_height);
void gwy_data_field_crosscorrelate_iteration(GwyDataField *data_field1,
                                             GwyDataField *data_field2,
                                             GwyDataField *x_dist,
                                             GwyDataField *y_dist,
                                             GwyDataField *score,
                                             gint search_width,
                                             gint search_height,
                                             gint window_width,
                                             gint window_height,
                                             GwyComputationStateType *state,
                                             gint *iteration);
#ifndef GWY_DISABLE_DEPRECATED
void gwy_data_field_croscorrelate(GwyDataField *data_field1,
                                  GwyDataField *data_field2,
                                  GwyDataField *x_dist,
                                  GwyDataField *y_dist,
                                  GwyDataField *score,
                                  gint search_width,
                                  gint search_height,
                                  gint window_width,
                                  gint window_height);
void gwy_data_field_croscorrelate_iteration(GwyDataField *data_field1,
                                            GwyDataField *data_field2,
                                            GwyDataField *x_dist,
                                            GwyDataField *y_dist,
                                            GwyDataField *score,
                                            gint search_width,
                                            gint search_height,
                                            gint window_width,
                                            gint window_height,
                                            GwyComputationStateType *state,
                                            gint *iteration);
#endif

void gwy_data_field_correlate(GwyDataField *data_field,
                                  GwyDataField *kernel_field,
                                  GwyDataField *score);


void gwy_data_field_correlate_iteration(GwyDataField *data_field,
                                        GwyDataField *kernel_field,
                                        GwyDataField *score,
                                        GwyComputationStateType *state,
                                        gint *iteration);


void gwy_data_field_fractal_partitioning(GwyDataField *data_field,
                                         GwyDataLine *xresult,
                                         GwyDataLine *yresult,
                                         GwyInterpolationType interpolation);

void gwy_data_field_fractal_cubecounting(GwyDataField *data_field,
                                         GwyDataLine *xresult,
                                         GwyDataLine *yresult,
                                         GwyInterpolationType interpolation);

void gwy_data_field_fractal_triangulation(GwyDataField *data_field,
                                         GwyDataLine *xresult,
                                         GwyDataLine *yresult,
                                         GwyInterpolationType interpolation);

void gwy_data_field_fractal_psdf(GwyDataField *data_field,
                                         GwyDataLine *xresult,
                                         GwyDataLine *yresult,
                                         GwyInterpolationType interpolation);


gdouble gwy_data_field_fractal_cubecounting_dim(GwyDataLine *xresult,
                                                GwyDataLine *yresult,
                                                gdouble *a,
                                                gdouble *b);

gdouble gwy_data_field_fractal_triangulation_dim(GwyDataLine *xresult,
                                                 GwyDataLine *yresult,
                                                 gdouble *a,
                                                 gdouble *b);

gdouble gwy_data_field_fractal_partitioning_dim(GwyDataLine *xresult,
                                                GwyDataLine *yresult,
                                                gdouble *a,
                                                gdouble *b);

gdouble gwy_data_field_fractal_psdf_dim(GwyDataLine *xresult,
                                        GwyDataLine *yresult,
                                        gdouble *a,
                                        gdouble *b);



void gwy_data_field_correct_laplace_iteration(GwyDataField *data_field,
                                    GwyDataField *mask_field,
                                    GwyDataField *buffer_field,
                                    gdouble *error,
                                    gdouble *corfactor);

void gwy_data_field_correct_average(GwyDataField *data_field,
                                    GwyDataField *mask_field);

void
gwy_data_field_mask_outliers(GwyDataField *data_field,
                             GwyDataField *mask_field,
                             gdouble thresh);

G_END_DECLS


#endif /*__GWY_DATAFIELD_H__*/
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
