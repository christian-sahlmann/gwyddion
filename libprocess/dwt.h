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

#ifndef __GWY_PROCESS_DWT_H__
#define __GWY_PROCESS_DWT_H__
#include <glib.h>
#include "dataline.h"
#include "datafield.h"


typedef enum {
	GWY_DWT_HAAR = 0,
	GWY_DWT_DAUB4 = 1,
	GWY_DWT_DAUB6 = 2,
	GWY_DWT_DAUB8 = 3,
	GWY_DWT_DAUB12 = 4,
	GWY_DWT_DAUB20 = 5
} GwyDWTType;

typedef enum {
    GWY_DWT_DENOISE_UNIVERSAL = 0,
    GWY_DWT_DENOISE_SCALE_ADAPTIVE = 1,
    GWY_DWT_DENOISE_SPACE_ADAPTIVE = 2
} GwyDWTDenoiseType;

G_BEGIN_DECLS

/*load wavelet coeficients*/
GwyDataLine *gwy_dwt_set_coefficients(GwyDataLine *dline, GwyDWTType type);

/*plot wavelet*/
GwyDataLine *gwy_dwt_plot_wavelet(GwyDataLine *dline, GwyDataLine *wt_coefs);

/*plot scaling function*/
GwyDataLine *gwy_dwt_plot_scaling_function(GwyDataLine *dline, GwyDataLine *wt_coefs);

/*compute dwt pyramidal algorithm for dataline*/
GwyDataLine * gwy_data_line_dwt(GwyDataLine *dline, GwyDataLine *wt_coefs, gint isign, gint minsize);

/*compute dwt pyramidal algorithms for datafields*/
GwyDataField * gwy_data_field_xdwt(GwyDataField *dfield, GwyDataLine *wt_coefs, gint isign, gint minsize);
GwyDataField * gwy_data_field_ydwt(GwyDataField *dfield, GwyDataLine *wt_coefs, gint isign, gint minsize);
GwyDataField * gwy_data_field_dwt(GwyDataField *dfield, GwyDataLine *wt_coefs, gint isign, gint minsize);

/*wavelet denoising*/
GwyDataField *gwy_data_field_dwt_denoise(GwyDataField *dfield, GwyDataLine *wt_coefs, gboolean hard,
					 gdouble multiple_threshold, GwyDWTDenoiseType type);

/*wavelet anisotropy marking (scars, etc.)*/
GwyDataField *gwy_data_field_dwt_mark_anisotropy(GwyDataField *dfield, GwyDataField *mask,
				GwyDataLine *wt_coefs, gdouble ratio, gint lowlimit);

/*wavelet interpolation (correction of lost data in mask)*/
GwyDataField *gwy_data_field_dwt_correction(GwyDataField *dfield, GwyDataField *mask,
				GwyDataLine *wt_coefs);

G_END_DECLS

#endif /*__GWY_PROCESS_DWT__*/
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

