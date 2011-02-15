/*
 *  @(#) $Id$
 *  Copyright (C) 2010,2011 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_CALDATA_H__
#define __GWY_CALDATA_H__

#include <libgwyddion/gwysiunit.h>
#include <libprocess/gwyprocessenums.h>

G_BEGIN_DECLS

#define GWY_TYPE_CALDATA            (gwy_caldata_get_type())
#define GWY_CALDATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CALDATA, GwyCalData))
#define GWY_CALDATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CALDATA, GwyCalDataClass))
#define GWY_IS_CALDATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CALDATA))
#define GWY_IS_CALDATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CALDATA))
#define GWY_CALDATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CALDATA, GwyCalDataClass))

typedef struct {
    gdouble *xerr;
    gdouble *yerr;
    gdouble *zerr;
    gdouble *xunc;
    gdouble *yunc;
    gdouble *zunc;
    gint n;
} GwyCurveCalibrationData;


typedef struct _GwyCalData      GwyCalData;
typedef struct _GwyCalDataClass GwyCalDataClass;


#define gwy_caldata_duplicate(caldata) \
        (GWY_CALDATA(gwy_serializable_duplicate(G_OBJECT(caldata))))

GType  gwy_caldata_get_type  (void) G_GNUC_CONST;

GwyCalData* gwy_caldata_new                (gint ndata);
void        gwy_caldata_resize             (GwyCalData *caldata, 
                                            gint ndata);
void        gwy_caldata_append             (GwyCalData *caldata, 
                                            GwyCalData *sec);
gint        gwy_caldata_get_ndata          (GwyCalData *caldata);
gdouble*    gwy_caldata_get_x              (GwyCalData *caldata);
gdouble*    gwy_caldata_get_y              (GwyCalData *caldata);
gdouble*    gwy_caldata_get_z              (GwyCalData *caldata);
gdouble*    gwy_caldata_get_xerr           (GwyCalData *caldata);
gdouble*    gwy_caldata_get_yerr           (GwyCalData *caldata);
gdouble*    gwy_caldata_get_zerr           (GwyCalData *caldata);
gdouble*    gwy_caldata_get_xunc           (GwyCalData *caldata);
gdouble*    gwy_caldata_get_yunc           (GwyCalData *caldata);
gdouble*    gwy_caldata_get_zunc           (GwyCalData *caldata);
void        gwy_caldata_get_range          (GwyCalData *caldata,
                                            gdouble *xfrom,
                                            gdouble *xto,
                                            gdouble *yfrom,
                                            gdouble *yto,
                                            gdouble *zfrom,
                                            gdouble *zto);
void        gwy_caldata_set_range          (GwyCalData *caldata,
                                            gdouble xfrom,
                                            gdouble xto,
                                            gdouble yfrom,
                                            gdouble yto,
                                            gdouble zfrom,
                                            gdouble zto);


GwySIUnit*  gwy_caldata_get_si_unit_x      (GwyCalData *caldata);
GwySIUnit*  gwy_caldata_get_si_unit_y      (GwyCalData *caldata);
GwySIUnit*  gwy_caldata_get_si_unit_z      (GwyCalData *caldata);
void        gwy_caldata_set_si_unit_x      (GwyCalData *caldata,
                                            GwySIUnit *si_unit);
void        gwy_caldata_set_si_unit_y      (GwyCalData *caldata,
                                            GwySIUnit *si_unit);
void        gwy_caldata_set_si_unit_z      (GwyCalData *caldata,
                                            GwySIUnit *si_unit);
void        gwy_caldata_setup_interpolation(GwyCalData *caldata);
void        gwy_caldata_interpolate        (GwyCalData *caldata,
                                            gdouble x,
                                            gdouble y,
                                            gdouble z,
                                            gdouble *xerr,
                                            gdouble *yerr,
                                            gdouble *zerr,
                                            gdouble *xunc,
                                            gdouble *yunc,
                                            gdouble *zunc);

void        gwy_caldata_save_data          (GwyCalData *caldata,
                                            gchar *filename);

gboolean    gwy_caldata_inside             (GwyCalData *caldata,
                                            gdouble x,
                                            gdouble y,
                                            gdouble z);
//void
//gwy_caldata_debug(GwyCalData *caldata, gchar *message);


G_END_DECLS

#endif /* __GWY_CALDATA_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
