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

#ifndef __GWY_PROCESS_CORRECT_H__
#define __GWY_PROCESS_CORRECT_H__

#include <libprocess/datafield.h>

G_BEGIN_DECLS

typedef enum {
    GWY_SYMMETRY_AUTO = 0,
    GWY_SYMMETRY_PARALLEL,
    GWY_SYMMETRY_TRIANGULAR,
    GWY_SYMMETRY_SQUARE,
    GWY_SYMMETRY_RHOMBIC,
    GWY_SYMMETRY_HEXAGONAL,
    GWY_SYMMETRY_LAST
} GwyPlaneSymmetry;

void gwy_data_field_correct_laplace_iteration(GwyDataField *data_field,
                                              GwyDataField *mask_field,
                                              GwyDataField *buffer_field,
                                              gdouble *error,
                                              gdouble *corfactor);

void gwy_data_field_correct_average(GwyDataField *data_field,
                                    GwyDataField *mask_field);

void gwy_data_field_mask_outliers(GwyDataField *data_field,
                                  GwyDataField *mask_field,
                                  gdouble thresh);

/*
void gwy_data_field_mark_scars(GwyDataField *data_field,
                               GwyDataField *scar_field,
                               gdouble threshold_high,
                               gdouble threshold_low,
                               gdouble min_scar_len,
                               gdouble max_scar_width);
*/

GwyPlaneSymmetry gwy_data_field_unrotate_find_corrections(GwyDataLine *derdist,
                                                          gdouble *correction);

G_END_DECLS

#endif /*__GWY_PROCESS_CORRECT__*/

