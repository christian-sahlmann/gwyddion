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

#ifndef __GWY_PROCESS_FRACTALS_H__
#define __GWY_PROCESS_FRACTALS_H__

#include <libprocess/datafield.h>

G_BEGIN_DECLS

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

void gwy_data_field_fractal_correction(GwyDataField *data_field,
                                       GwyDataField *mask_field,
                                       GwyInterpolationType interpolation);

G_END_DECLS

#endif /*__GWY_PROCESS_FRACTALS__*/
