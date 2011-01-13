/*
 *  @(#) $Id: gwycalibration.h 6698 2006-09-28 21:50:13Z yeti-dn $
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

#ifndef __GWY_CALIBRATION_H__
#define __GWY_CALIBRATION_H__

#include <glib-object.h>
#include <libgwyddion/gwyresource.h>
#include <libdraw/gwyrgba.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define GWY_CALIBRATION_DEFAULT "Gray"

#define GWY_TYPE_CALIBRATION                  (gwy_calibration_get_type())
#define GWY_CALIBRATION(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CALIBRATION, GwyCalibration))
#define GWY_CALIBRATION_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CALIBRATION, GwyCalibrationClass))
#define GWY_IS_CALIBRATION(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CALIBRATION))
#define GWY_IS_CALIBRATION_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CALIBRATION))
#define GWY_CALIBRATION_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CALIBRATION, GwyCalibrationClass))

typedef struct _GwyCalibration      GwyCalibration;
typedef struct _GwyCalibrationClass GwyCalibrationClass;


struct _GwyCalibration {
    GwyResource parent_instance;

    gint ndata;
    gchar* filename;

    gpointer reserved1;
    gpointer reserved2;
    gint int1;
};

struct _GwyCalibrationClass {
    GwyResourceClass parent_class;

    void (*reserved1)(void);
    void (*reserved2)(void);
};

GwyCalibration* gwy_calibration_new             (const gchar *name,
                                                        gint ndata,
                                                        const char* filename);
GType             gwy_calibration_get_type              (void) G_GNUC_CONST;
gint              gwy_calibration_get_data           (GwyCalibration *calibration);
const gchar*      gwy_calibration_get_filename          (GwyCalibration *calibration);
GwyInventory*     gwy_calibrations                      (void);
GwyCalibration*   gwy_calibrations_get_calibration      (const gchar *name);
void              _gwy_calibration_class_setup_presets(void);


G_END_DECLS

#endif /*__GWY_CALIBRATION_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
