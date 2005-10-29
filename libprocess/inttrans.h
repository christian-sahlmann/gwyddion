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

#ifndef __GWY_PROCESS_INTTRANS_H__
#define __GWY_PROCESS_INTTRANS_H__

#include <libprocess/datafield.h>
#include <libprocess/cwt.h>

G_BEGIN_DECLS

gint gwy_fft_find_nice_size       (gint size);
#ifndef GWY_DISABLE_DEPRECATED
gint gwy_data_field_get_fft_res   (gint data_res);
#endif
void gwy_data_line_fft            (GwyDataLine *rsrc,
                                   GwyDataLine *isrc,
                                   GwyDataLine *rdest,
                                   GwyDataLine *idest,
                                   GwyWindowingType windowing,
                                   GwyTransformDirection direction,
                                   GwyInterpolationType interpolation,
                                   gboolean preserverms,
                                   gboolean level);
void gwy_data_field_2dfft         (GwyDataField *ra,
                                   GwyDataField *ia,
                                   GwyDataField *rb,
                                   GwyDataField *ib,
                                   GwyWindowingType windowing,
                                   GwyTransformDirection direction,
                                   GwyInterpolationType interpolation,
                                   gboolean preserverms,
                                   gboolean level);
void gwy_data_field_2dfft_real    (GwyDataField *ra,
                                   GwyDataField *rb,
                                   GwyDataField *ib,
                                   GwyWindowingType windowing,
                                   GwyTransformDirection direction,
                                   GwyInterpolationType interpolation,
                                   gboolean preserverms,
                                   gboolean level);
void gwy_data_field_2dfft_humanize(GwyDataField *data_field);
void gwy_data_field_xfft          (GwyDataField *ra,
                                   GwyDataField *ia,
                                   GwyDataField *rb,
                                   GwyDataField *ib,
                                   GwyWindowingType windowing,
                                   GwyTransformDirection direction,
                                   GwyInterpolationType interpolation,
                                   gboolean preserverms,
                                   gboolean level);
void gwy_data_field_yfft          (GwyDataField *ra,
                                   GwyDataField *ia,
                                   GwyDataField *rb,
                                   GwyDataField *ib,
                                   GwyWindowingType windowing,
                                   GwyTransformDirection direction,
                                   GwyInterpolationType interpolation,
                                   gboolean preserverms,
                                   gboolean level);
void gwy_data_field_xfft_real     (GwyDataField *ra,
                                   GwyDataField *rb,
                                   GwyDataField *ib,
                                   GwyWindowingType windowing,
                                   GwyTransformDirection direction,
                                   GwyInterpolationType interpolation,
                                   gboolean preserverms,
                                   gboolean level);
void gwy_data_field_cwt           (GwyDataField *data_field,
                                   GwyInterpolationType interpolation,
                                   gdouble scale,
                                   Gwy2DCWTWaveletType wtype);
void gwy_data_field_fft_filter_1d (GwyDataField *data_field,
                                   GwyDataField *result_field,
                                   GwyDataLine *weights,
                                   GwyOrientation orientation,
                                   GwyInterpolationType interpolation);


G_END_DECLS

#endif /*__GWY_PROCESS_INTTRANS_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
