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
/* GtkOrientation */
#include <gtk/gtk.h>
#include <libprocess/interpolation.h>
#include <libprocess/dataline.h>
#include <libgwyddion/gwysiunit.h>

G_BEGIN_DECLS

#define GWY_TYPE_DATA_FIELD                  (gwy_data_field_get_type())
#define GWY_DATA_FIELD(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_FIELD, GwyDataField))
#define GWY_DATA_FIELD_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_FIELD, GwyDataFieldClass))
#define GWY_IS_DATA_FIELD(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_FIELD))
#define GWY_IS_DATA_FIELD_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_FIELD))
#define GWY_DATA_FIELD_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_FIELD, GwyDataFieldClass))

typedef struct _GwyDataField      GwyDataField;
typedef struct _GwyDataFieldClass GwyDataFieldClass;

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

typedef enum {
    GWY_COMP_INIT         = 0, /*start initializations*/
    GWY_COMP_ITERATE      = 1, /*locate steps*/
    GWY_COMP_FINISHED     = 2
} GwyComputationStateType;

G_END_DECLS

/* XXX: This is here to allow people #include just datafield.h and get all
 * datafield-related functions as before, should be removed someday */
#include <libprocess/correct.h>
#include <libprocess/correlation.h>
#include <libprocess/cwt.h>
#include <libprocess/fractals.h>
#include <libprocess/filters.h>
#include <libprocess/grains.h>
#include <libprocess/inttrans.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>

G_BEGIN_DECLS

GType gwy_data_field_get_type  (void) G_GNUC_CONST;

GObject* gwy_data_field_new(gint xres,
                            gint yres,
                            gdouble xreal,
                            gdouble yreal,
                            gboolean nullme);

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

void gwy_data_field_shade(GwyDataField *data_field,
                          GwyDataField *target_field,
                          gdouble theta,
                          gdouble phi);

void gwy_data_field_fit_lines(GwyDataField *data_field,
                              gint ulcol,
                              gint ulrow,
                              gint brcol,
                              gint brrow,
                              GwyFitLineType fit_type,
                              gboolean exclude,
                              GtkOrientation orientation);


G_END_DECLS

#endif /*__GWY_DATAFIELD_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
