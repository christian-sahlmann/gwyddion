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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_PROCESS_CORRECT_H__
#define __GWY_PROCESS_CORRECT_H__

#include <libprocess/datafield.h>

G_BEGIN_DECLS

typedef void (*GwyCoordTransform2DFunc)(gdouble x,
                                        gdouble y,
                                        gdouble *px,
                                        gdouble *py,
                                        gpointer user_data);

void gwy_data_field_correct_laplace_iteration (GwyDataField *data_field,
                                               GwyDataField *mask_field,
                                               GwyDataField *buffer_field,
                                               gdouble corrfactor,
                                               gdouble *error);
void gwy_data_field_correct_average           (GwyDataField *data_field,
                                               GwyDataField *mask_field);
void gwy_data_field_mask_outliers             (GwyDataField *data_field,
                                               GwyDataField *mask_field,
                                               gdouble thresh);
void gwy_data_field_mask_outliers2            (GwyDataField *data_field,
                                               GwyDataField *mask_field,
                                               gdouble thresh_low,
                                               gdouble thresh_high);
void gwy_data_field_distort                   (GwyDataField *source,
                                               GwyDataField *dest,
                                               GwyCoordTransform2DFunc invtrans,
                                               gpointer user_data,
                                               GwyInterpolationType interp,
                                               GwyExteriorType exterior,
                                               gdouble fill_value);

GwyPlaneSymmetry gwy_data_field_unrotate_find_corrections(GwyDataLine *derdist,
                                                          gdouble *correction);

G_END_DECLS

#endif /* __GWY_PROCESS_CORRECT__ */

