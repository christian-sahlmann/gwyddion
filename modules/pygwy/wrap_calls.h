/*
 *  $Id$
 *  Copyright (C) 2008 Jan Horak, 2015 David Necas (Yeti)
 *  E-mail: xhorak@gmail.com, yeti@gwyddio.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 *  Description: This file contains custom fuctions for automatically
 *  generated wrapping by using pygwy-codegen.
 */

#ifndef _WRAP_CALLS_H
#define _WRAP_CALLS_H

#include <libprocess/gwyprocess.h>
#include <libdraw/gwyselection.h>
#include <libgwyddion/gwyddion.h>
#include <app/data-browser.h>

typedef GArray GArrayInt;
typedef GArray GArrayDouble;

GwyDataLine*  gwy_data_field_get_profile_wrap             (GwyDataField *data_field,
                                                           gint scol,
                                                           gint srow,
                                                           gint ecol,
                                                           gint erow,
                                                           gint res,
                                                           gint thickness,
                                                           GwyInterpolationType interpolation);
GArrayDouble* gwy_selection_get_data_wrap                 (GwySelection *selection);
GArrayDouble* gwy_data_field_fit_polynom_wrap             (GwyDataField *data_field,
                                                           gint col_degree,
                                                           gint row_degree);
GArrayDouble* gwy_data_field_area_fit_polynom_wrap        (GwyDataField *data_field,
                                                           gint col,
                                                           gint row,
                                                           gint width,
                                                           gint height,
                                                           gint col_degree,
                                                           gint row_degree);
GArrayDouble* gwy_data_field_elliptic_area_extract_wrap   (GwyDataField *data_field,
                                                           gint col,
                                                           gint row,
                                                           gint width,
                                                           gint height);
GArrayDouble* gwy_data_field_circular_area_extract_wrap   (GwyDataField *data_field,
                                                           gint col,
                                                           gint row,
                                                           gdouble radius);
GArrayInt*    gwy_app_data_browser_get_data_ids_wrap      (GwyContainer *data);
gint          gwy_get_key_from_name                       (const gchar *name);
GwyDataField* gwy_tip_dilation_wrap                       (GwyDataField *tip,
                                                           GwyDataField *surface);
GwyDataField* gwy_tip_erosion_wrap                        (GwyDataField *tip,
                                                           GwyDataField *surface);
GwyDataField* gwy_tip_cmap_wrap                           (GwyDataField *tip,
                                                           GwyDataField *surface);
GwyDataField* gwy_tip_estimate_partial_wrap               (GwyDataField *tip,
                                                           GwyDataField *surface,
                                                           gdouble threshold,
                                                           gboolean use_edges);
GwyDataField* gwy_tip_estimate_full_wrap                  (GwyDataField *tip,
                                                           GwyDataField *surface,
                                                           gdouble threshold,
                                                           gboolean use_edges);
GwyDataField* gwy_data_field_create_full_mask             (GwyDataField *d);
GArrayInt*    gwy_data_field_number_grains_wrap           (GwyDataField *mask_field);
GArrayInt*    gwy_data_field_number_grains_periodic_wrap  (GwyDataField *mask_field);
GArrayInt*    gwy_data_field_get_grain_bounding_boxes_wrap(GwyDataField *data_field,
                                                           const GArrayInt *grains);
GArrayDouble* gwy_data_field_grains_get_values_wrap       (GwyDataField *data_field,
                                                           const GArrayInt *grains,
                                                           GwyGrainQuantity quantity);

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
