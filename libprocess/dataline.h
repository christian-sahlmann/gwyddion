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

G_BEGIN_DECLS

#define GWY_TYPE_DATA_LINE                  (gwy_data_line_get_type())
#define GWY_DATA_LINE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_LINE, GwyDataLine))
#define GWY_DATA_LINE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_LINE, GwyDataLineClass))
#define GWY_IS_DATA_LINE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_LINE))
#define GWY_IS_DATA_LINE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_LINE))
#define GWY_DATA_LINE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_LINE, GwyDataLineClass))

typedef struct _GwyDataLine      GwyDataLine;
typedef struct _GwyDataLineClass GwyDataLineClass;

/* FIXME: is used in linestats */
typedef enum {
    GWY_SF_OUTPUT_DH    = 0,
    GWY_SF_OUTPUT_CDH   = 1,
    GWY_SF_OUTPUT_DA    = 2,
    GWY_SF_OUTPUT_CDA   = 3,
    GWY_SF_OUTPUT_ACF   = 4,
    GWY_SF_OUTPUT_HHCF  = 5,
    GWY_SF_OUTPUT_PSDF  = 6
} GwySFOutputType;

/* FIXME: simply use a number? */
typedef enum {
    GWY_FIT_POLY_0   = 0,
    GWY_FIT_POLY_1   = 1,
    GWY_FIT_POLY_2   = 2,
    GWY_FIT_POLY_3   = 3
} GwyFitLineType;

struct _GwyDataLine {
    GObject parent_instance;

    gint res;
    gdouble real;
    gdouble *data;
    /* XXX: Why the fscking GwyDataLine contain no units? */
};

struct _GwyDataLineClass {
    GObjectClass parent_class;
};

#define gwy_data_line_duplicate(data_line) ((GwyDataField*)gwy_serializable_duplicate(G_OBJECT(data_line)))

GType  gwy_data_line_get_type  (void) G_GNUC_CONST;

GObject*       gwy_data_line_new                   (gint res,
                                                    gdouble real,
                                                    gboolean nullme);
GObject*       gwy_data_line_new_alike             (GwyDataLine *model,
                                                    gboolean nullme);
void           gwy_data_line_resample      (GwyDataLine *data_line,
                                            gint res,
                                            GwyInterpolationType interpolation);
gboolean       gwy_data_line_resize                (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to);
gboolean       gwy_data_line_copy                  (GwyDataLine *data_line,
                                                    GwyDataLine *b);
gdouble*       gwy_data_line_get_data              (GwyDataLine *data_line);
const gdouble* gwy_data_line_get_data_const        (GwyDataLine *data_line);
gint           gwy_data_line_get_res               (GwyDataLine *data_line);
gdouble        gwy_data_line_get_real              (GwyDataLine *data_line);
void           gwy_data_line_set_real              (GwyDataLine *data_line,
                                                    gdouble real);
gdouble        gwy_data_line_itor                  (GwyDataLine *data_line,
                                                    gdouble pixval);
gdouble        gwy_data_line_rtoi                  (GwyDataLine *data_line,
                                                    gdouble realval);
gdouble        gwy_data_line_get_val               (GwyDataLine *data_line,
                                                    gint i);
gint           gwy_data_line_set_val               (GwyDataLine *data_line,
                                                    gint i,
                                                    gdouble value);
gdouble        gwy_data_line_get_dval              (GwyDataLine *data_line,
                                                    gdouble x,
                                                    gint interpolation);
gdouble        gwy_data_line_get_dval_real         (GwyDataLine *data_line,
                                                    gdouble x,
                                                    gint interpolation);
void           gwy_data_line_invert                (GwyDataLine *data_line,
                                                    gboolean x,
                                                    gboolean z);
void           gwy_data_line_fill                  (GwyDataLine *data_line,
                                                    gdouble value);
void           gwy_data_line_multiply              (GwyDataLine *data_line,
                                                    gdouble value);
void           gwy_data_line_add                   (GwyDataLine *data_line,
                                                    gdouble value);
void           gwy_data_line_part_fill             (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to,
                                                    gdouble value);
void           gwy_data_line_part_multiply         (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to,
                                                    gdouble value);
void           gwy_data_line_part_add              (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to,
                                                    gdouble value);
gdouble        gwy_data_line_get_max               (GwyDataLine *data_line);
gdouble        gwy_data_line_get_min               (GwyDataLine *data_line);
gdouble        gwy_data_line_get_avg               (GwyDataLine *data_line);
gdouble        gwy_data_line_get_rms               (GwyDataLine *data_line);
gdouble        gwy_data_line_get_sum               (GwyDataLine *data_line);
gdouble        gwy_data_line_part_get_max          (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to);
gdouble        gwy_data_line_part_get_min          (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to);
gdouble        gwy_data_line_part_get_avg          (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to);
gdouble        gwy_data_line_part_get_rms          (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to);
gdouble        gwy_data_line_part_get_sum          (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to);
gint           gwy_data_line_threshold             (GwyDataLine *data_line,
                                                    gdouble threshval,
                                                    gdouble bottom,
                                                    gdouble top);
gint           gwy_data_line_part_threshold        (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to,
                                                    gdouble threshval,
                                                    gdouble bottom,
                                                    gdouble top);
void           gwy_data_line_line_coeffs           (GwyDataLine *data_line,
                                                    gdouble *av,
                                                    gdouble *bv);
void           gwy_data_line_part_line_coeffs      (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to,
                                                    gdouble *av,
                                                    gdouble *bv);
void           gwy_data_line_line_level            (GwyDataLine *data_line,
                                                    gdouble av,
                                                    gdouble bv);
void           gwy_data_line_line_rotate           (GwyDataLine *data_line,
                                                    gdouble angle,
                                                    gint interpolation);
gdouble        gwy_data_line_get_der               (GwyDataLine *data_line,
                                                    gint i);
void           gwy_data_line_fft            (GwyDataLine *ra,
                                             GwyDataLine *ia,
                                             GwyDataLine *rb,
                                             GwyDataLine *ib,
                                             void (*fft)(),
                                             GwyWindowingType windowing,
                                             gint direction,
                                             GwyInterpolationType interpolation,
                                             gboolean preserverms,
                                             gboolean level);
void           gwy_data_line_fft_hum               (gint direction,
                                                    GwyDataLine *ra,
                                                    GwyDataLine *ia,
                                                    GwyDataLine *rb,
                                                    GwyDataLine *ib,
                                                    gint interpolation);
gdouble*       gwy_data_line_part_fit_polynom      (GwyDataLine *data_line,
                                                    gint n,
                                                    gdouble *coeffs,
                                                    gint from,
                                                    gint to);
gdouble*       gwy_data_line_fit_polynom           (GwyDataLine *data_line,
                                                    gint n,
                                                    gdouble *coeffs);
void           gwy_data_line_part_subtract_polynom (GwyDataLine *data_line,
                                                    gint n,
                                                    gdouble *coeffs,
                                                    gint from,
                                                    gint to);
void           gwy_data_line_subtract_polynom      (GwyDataLine *data_line,
                                                    gint n,
                                                    gdouble *coeffs);
gdouble        gwy_data_line_get_modus             (GwyDataLine *data_line,
                                                    gint histogram_steps);
gdouble        gwy_data_line_part_get_modus        (GwyDataLine *data_line,
                                                    gint from,
                                                    gint to,
                                                    gint histogram_steps);

G_END_DECLS

#endif /*__GWY_DATALINE_H__*/


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

