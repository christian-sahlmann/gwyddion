/*
 *  @(#) $Id: stats.c 10263 2009-10-19 12:39:41Z yeti-dn $
 *  Copyright (C) 2003-2009 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <string.h>

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <libprocess/grains.h>
#include <libprocess/inttrans.h>
#include <libprocess/simplefft.h>
#include "gwyprocessinternal.h"


gdouble    gwy_data_field_get_max_uncertainty       (GwyDataField *data_field, 
                                                     GwyDataField *uncz_field);
gdouble    gwy_data_field_area_get_max_uncertainty  (GwyDataField *dfield,
		                                             GwyDataField *uncz_field,
		                                             GwyDataField *mask,
                                                     gint col, gint row,
                                                     gint width, gint height);
gdouble    gwy_data_field_get_min_uncertainty       (GwyDataField *data_field, 
                                                     GwyDataField *uncz_field);

gdouble    gwy_data_field_area_get_min_uncertainty  (GwyDataField *dfield,
		                                             GwyDataField *uncz_field,
                                                     GwyDataField *mask,
                                                     gint col, gint row,
                                                     gint width, gint height);

void       gwy_data_field_get_min_max_uncertainty   (GwyDataField *data_field,
		                                     GwyDataField *uncz_field,
                                                     gdouble *min_unc,
                                                     gdouble *max_unc);

void       gwy_data_field_area_get_min_max_uncertainty(GwyDataField *data_field,
		                                               GwyDataField *uncz_field,
                                                       GwyDataField *mask,
                                                       gint col, gint row,
                                                       gint width, gint height,
                                                       gdouble *min_unc,
                                                       gdouble *max_unc);

void       gwy_data_field_area_get_min_max_uncertainty_mask(GwyDataField *data_field,
                                                            GwyDataField *uncz_field,
                                                            GwyDataField *mask,
                                                            GwyMaskingType mode,
                                                            gint col, gint row,
                                                            gint width, gint height,
                                                            gdouble *min_unc,
                                                            gdouble *max_unc);

gdouble    gwy_data_field_get_avg_uncertainty         (GwyDataField *data_field, 
                                                       GwyDataField *uncz_field);

gdouble    gwy_data_field_area_get_avg_uncertainty    (GwyDataField *dfield,
                                                       GwyDataField *uncz_field,
                                                       GwyDataField *mask,
                                                       gint col, gint row,
                                                       gint width, gint height);

gdouble    gwy_data_field_area_get_avg_uncertainty_mask(GwyDataField *dfield,
                                                        GwyDataField *uncz_field,
                                                        GwyDataField *mask,
                                                        GwyMaskingType mode,
                                                        gint col, gint row,
                                                        gint width, gint height);

gdouble    gwy_data_field_get_rms_uncertainty         (GwyDataField *data_field, 
                                                       GwyDataField *uncz_field);

gdouble    gwy_data_field_area_get_rms_uncertainty    (GwyDataField *dfield,
                                                       GwyDataField *uncz_field,
                                                       GwyDataField *mask,
                                                       gint col, gint row,
                                                       gint width, gint height);

gdouble    gwy_data_field_area_get_rms_uncertainty_mask(GwyDataField *dfield,
                                                        GwyDataField *uncz_field,
                                                        GwyDataField *mask,
                                                        GwyMaskingType mode,
                                                        gint col, gint row,
                                                        gint width, gint height);

void       gwy_data_field_get_stats_uncertainties      (GwyDataField *data_field,
                                                        GwyDataField *uncz_field,
                                                        gdouble *avg_unc,
                                                        gdouble *ra_unc,
                                                        gdouble *rms_unc,
                                                        gdouble *skew_unc,
                                                        gdouble *kurtosis_unc);

void       gwy_data_field_area_get_stats_uncertainties (GwyDataField *dfield,
                                                        GwyDataField *uncz_field,
                                                        GwyDataField *mask,
                                                        gint col, gint row,
                                                        gint width, gint height,
                                                        gdouble *avg_unc,
                                                        gdouble *ra_unc,
                                                        gdouble *rms_unc,
                                                        gdouble *skew_unc,
                                                        gdouble *kurtosis_unc);

void       gwy_data_field_area_get_stats_uncertainties_mask(GwyDataField *dfield,
                                                            GwyDataField *uncz_field,
                                                            GwyDataField *mask,
                                                            GwyMaskingType mode,
                                                            gint col, gint row,
                                                            gint width, gint height,
                                                            gdouble *avg_unc,
                                                            gdouble *ra_unc,
                                                            gdouble *rms_unc,
                                                            gdouble *skew_unc,
                                                            gdouble *kurtosis_unc);


void       gwy_data_field_area_acf_uncertainty     (GwyDataField *data_field, 
                                                    GwyDataField *uncz_field,
                                                    GwyDataLine *target_line, 
                                                    gint col, gint row,
                                                    gint width, gint height,
                                                    GwyOrientation orientation);

void       gwy_data_field_acf_uncertainty          (GwyDataField *data_field,
                                                    GwyDataField *uncz_field,
                                                    GwyDataLine *target_line,
                                                    GwyOrientation orientation);

void       gwy_data_field_area_hhcf_uncertainty    (GwyDataField *data_field,
                                                    GwyDataField *uncz_field,
                                                    GwyDataLine *target_line,
                                                    gint col, gint row,
                                                    gint width, gint height,
                                                    GwyOrientation orientation);

void       gwy_data_field_hhcf_uncertainty         (GwyDataField *data_field,
                                                    GwyDataField *uncz_field,
                                                    GwyDataLine *target_line,
                                                    GwyOrientation orientation);

gdouble    gwy_data_field_get_surface_area_uncertainty (GwyDataField *data_field, 
                                                        GwyDataField *uncz_field, 
                                                        GwyDataField *uncx_field, 
                                                        GwyDataField *uncy_field);

gdouble    gwy_data_field_area_get_surface_area_uncertainty(GwyDataField *data_field, 
                                                            GwyDataField *uncz_field,
                                                            GwyDataField *uncx_field, 
                                                            GwyDataField *uncy_field,
                                                            GwyDataField *mask,
                                                            gint col, gint row,
                                                            gint width, gint height);

gdouble    gwy_data_field_area_get_surface_area_mask_uncertainty(GwyDataField *data_field,
                                                                 GwyDataField *uncz_field, 
                                                                 GwyDataField *uncx_field, 
                                                                 GwyDataField *uncy_field,
                                                                 GwyDataField *mask,
                                                                 GwyMaskingType mode,
                                                                 gint col, gint row,
                                                                 gint width, gint height);

gdouble    gwy_data_field_area_get_median_uncertainty           (GwyDataField *dfield,
                                                                 GwyDataField *uncz_field,
                                                                 GwyDataField *mask,
                                                                 gint col, gint row,
                                                                 gint width, gint height);

gdouble    gwy_data_field_area_get_median_uncertainty_mask      (GwyDataField *dfield,
                                                                 GwyDataField *uncz_field,
                                                                 GwyDataField *mask,
                                                                 GwyMaskingType mode,
                                                                 gint col, gint row,
                                                                 gint width, gint height);

gdouble    gwy_data_field_get_median_uncertainty                (GwyDataField *data_field, 
                                                                 GwyDataField *uncz_field);
void    gwy_data_field_area_dh_uncertainty                      (GwyDataField *data_field,
                                                                 GwyDataField *uncz_field,
                                                                 GwyDataField *mask,
                                                                 GwyDataLine *target_line,
                                                                 gint col,
                                                                 gint row,
                                                                 gint width,
                                                                 gint height,
                                                                 gint nstats);
void    gwy_data_field_dh_uncertainty                           (GwyDataField *data_field,
                                                                 GwyDataField *uncz_field,
                                                                 GwyDataLine *target_line,
                                                                 gint nstats);



void gwy_data_field_area_get_normal_coeffs_uncertainty(GwyDataField *data_field,
		GwyDataField *uncz_field, 
		GwyDataField *uncx_field, 
		GwyDataField *uncy_field, 
		gint col, gint row,
		gint width, gint height,
		gdouble *x, gdouble *y, gdouble *z,
		gdouble *ux, gdouble *uy, gdouble *uz);

void gwy_data_field_get_normal_coeffs_uncertainty(GwyDataField *data_field,
		GwyDataField *uncz_field, 
		GwyDataField *uncx_field, 
		GwyDataField *uncy_field, 
		gdouble *x, gdouble *y, gdouble *z,
		gdouble *ux, gdouble *uy, gdouble *uz);

void gwy_data_field_area_get_inclination_uncertainty(GwyDataField *data_field,
		GwyDataField *uncz_field, 
		GwyDataField *uncx_field, 
		GwyDataField *uncy_field, 
		gint col, gint row,
		gint width, gint height,
		gdouble *utheta,
		gdouble *uphi);

void gwy_data_field_get_inclination_uncertainty(GwyDataField *data_field,
		GwyDataField *uncz_field, 
		GwyDataField *uncx_field, 
		GwyDataField *uncy_field, 
		gdouble *utheta,
		gdouble *uphi);

gdouble gwy_data_field_area_get_projected_area_uncertainty(
		gint nn, GwyDataField *uncx_field, GwyDataField *uncy_field);
void gwy_data_field_area_cdh_uncertainty(GwyDataField *data_field,
		GwyDataField *uncz_field, GwyDataField *mask,
		GwyDataLine *target_line, gint col, gint row,
		gint width, gint height, gint nstats);

void gwy_data_field_cdh_uncertainty(GwyDataField *data_field,
		GwyDataField *uncz_field, GwyDataLine *target_line,
		gint nstats);

gdouble gwy_data_field_get_xder_uncertainty(GwyDataField *data_field, GwyDataField *uncz_field, 
		GwyDataField * uncx_field,GwyDataField *uncy_field, gint col,gint row);

gdouble gwy_data_field_get_yder_uncertainty(GwyDataField *data_field, GwyDataField *uncz_field, 
		GwyDataField * uncx_field,GwyDataField *uncy_field, gint col,gint row);
	
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
