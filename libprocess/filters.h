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

#ifndef __GWY_PROCESS_FILTERS_H__
#define __GWY_PROCESS_FILTERS_H__

#include <libprocess/datafield.h>

G_BEGIN_DECLS

typedef enum {
      GWY_FILTER_MEAN          = 0, /*mean value filter (averaging)*/
      GWY_FILTER_MEDIAN        = 1, /*median value filter*/
      GWY_FILTER_CONSERVATIVE  = 2, /*conservative denoising filter*/
      GWY_FILTER_LAPLACIAN     = 3, /*Laplacian 2nd derivative filter*/
      GWY_FILTER_SOBEL         = 4, /*Sobel gradient filter*/
      GWY_FILTER_PREWITT       = 5  /*Prewitt gradient filter*/
} GwyFilterType;

/* new-style functions */
void gwy_data_field_area_convolve              (GwyDataField *data_field,
                                                GwyDataField *kernel_field,
                                                gint col,
                                                gint row,
                                                gint width,
                                                gint height);
void gwy_data_field_area_filter_median         (GwyDataField *data_field,
                                                gint size,
                                                gint col,
                                                gint row,
                                                gint width,
                                                gint height);
void gwy_data_field_area_filter_mean           (GwyDataField *data_field,
                                                gint size,
                                                gint col,
                                                gint row,
                                                gint width,
                                                gint height);
void gwy_data_field_area_filter_conservative   (GwyDataField *data_field,
                                                gint size,
                                                gint col,
                                                gint row,
                                                gint width,
                                                gint height);
void gwy_data_field_area_filter_laplacian      (GwyDataField *data_field,
                                                gint col,
                                                gint row,
                                                gint width,
                                                gint height);
void gwy_data_field_area_filter_sobel          (GwyDataField *data_field,
                                                GtkOrientation orientation,
                                                gint col,
                                                gint row,
                                                gint width,
                                                gint height);
void gwy_data_field_area_filter_prewitt        (GwyDataField *data_field,
                                                GtkOrientation orientation,
                                                gint col,
                                                gint row,
                                                gint width,
                                                gint height);

/* old-style functions */
void gwy_data_field_convolve                   (GwyDataField *data_field,
                                                GwyDataField *kernel_field,
                                                gint ulcol,
                                                gint ulrow,
                                                gint brcol,
                                                gint brrow);
void gwy_data_field_filter_median              (GwyDataField *data_field,
                                                gint size,
                                                gint ulcol,
                                                gint ulrow,
                                                gint brcol,
                                                gint brrow);
void gwy_data_field_filter_mean                (GwyDataField *data_field,
                                                gint size,
                                                gint ulcol,
                                                gint ulrow,
                                                gint brcol,
                                                gint brrow);
void gwy_data_field_filter_conservative        (GwyDataField *data_field,
                                                gint size,
                                                gint ulcol,
                                                gint ulrow,
                                                gint brcol,
                                                gint brrow);
void gwy_data_field_filter_laplacian           (GwyDataField *data_field,
                                                gint ulcol,
                                                gint ulrow,
                                                gint brcol,
                                                gint brrow);
void gwy_data_field_filter_sobel               (GwyDataField *data_field,
                                                GtkOrientation orientation,
                                                gint ulcol,
                                                gint ulrow,
                                                gint brcol,
                                                gint brrow);
void gwy_data_field_filter_prewitt             (GwyDataField *data_field,
                                                GtkOrientation orientation,
                                                gint ulcol,
                                                gint ulrow,
                                                gint brcol,
                                                gint brrow);

G_END_DECLS

#endif /*__GWY_PROCESS_FILTERS__*/
