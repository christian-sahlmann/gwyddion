/*
 *  @(#) $Id: dataline.h 8202 2007-06-22 20:17:43Z yeti-dn $
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

#ifndef __GWY_CALDATA_H__
#define __GWY_CALDATA_H__

#include <libgwyddion/gwysiunit.h>
#include <libprocess/gwyprocessenums.h>
#include <libprocess/natural.h>

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

struct _GwyCalData {
    GObject parent_instance;

    gdouble *x;
    gdouble *y;
    gdouble *z;
    gdouble *xerr;
    gdouble *yerr;
    gdouble *zerr;
    gdouble *xunc;
    gdouble *yunc;
    gdouble *zunc;
    gdouble x_from;
    gdouble x_to;
    gdouble y_from;
    gdouble y_to;
    gdouble z_from;
    gdouble z_to;
    gint ndata; 

    GwySIUnit *si_unit_x;
    GwySIUnit *si_unit_y;
    GwySIUnit *si_unit_z;

    GwyDelaunayVertex *err_ps;      //as all triangulation works for vectors, there are two separate meshes now which is stupid.
    GwyDelaunayVertex *unc_ps;
    GwyDelaunayMesh *err_m;
    GwyDelaunayMesh *unc_m;

    gpointer reserved1;
    gint int1;
};

struct _GwyCalDataClass {
    GObjectClass parent_class;

    void (*reserved1)(void);
};

#define gwy_caldata_duplicate(caldata) \
        (GWY_CALDATA(gwy_serializable_duplicate(G_OBJECT(caldata))))

GType  gwy_caldata_get_type  (void) G_GNUC_CONST;

GwyCalData*   gwy_caldata_new                   (gint ndata);
gint           gwy_caldata_get_ndata            (GwyCalData *caldata);
GwySIUnit*     gwy_caldata_get_si_unit_x      (GwyCalData *caldata);
GwySIUnit*     gwy_caldata_get_si_unit_y       (GwyCalData *caldata);
GwySIUnit*     gwy_caldata_get_si_unit_z       (GwyCalData *caldata);
void           gwy_caldata_set_si_unit_x       (GwyCalData *caldata,
                                                    GwySIUnit *si_unit);
void           gwy_caldata_set_si_unit_y       (GwyCalData *caldata,
                                                    GwySIUnit *si_unit);
void           gwy_caldata_set_si_unit_z       (GwyCalData *caldata,
                                                    GwySIUnit *si_unit);
void           gwy_caldata_setup_interpolation (GwyCalData *caldata);
void           gwy_caldata_interpolate         (GwyCalData *caldata,
                                                gdouble x, gdouble y, gdouble z,
                                                gdouble *xerr, gdouble *yerr, gdouble *zerr,
                                                gdouble *xunc, gdouble *yunc, gdouble *zunc);

G_END_DECLS

#endif /* __GWY_CALDATA_H__ */


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

