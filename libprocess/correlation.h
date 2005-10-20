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

#ifndef __GWY_PROCESS_CORRELATION_H__
#define __GWY_PROCESS_CORRELATION_H__

#include <libprocess/datafield.h>

G_BEGIN_DECLS

gdouble gwy_data_field_get_correlation_score(GwyDataField *data_field,
                                             GwyDataField *kernel_field,
                                             gint ulcol,
                                             gint ulrow,
                                             gint kernel_ulcol,
                                             gint kernel_ulrow,
                                             gint kernel_brcol,
                                             gint kernel_brrow);

void gwy_data_field_crosscorrelate(GwyDataField *data_field1,
                                   GwyDataField *data_field2,
                                   GwyDataField *x_dist,
                                   GwyDataField *y_dist,
                                   GwyDataField *score,
                                   gint search_width,
                                   gint search_height,
                                   gint window_width,
                                   gint window_height);

void gwy_data_field_crosscorrelate_iteration(GwyDataField *data_field1,
                                             GwyDataField *data_field2,
                                             GwyDataField *x_dist,
                                             GwyDataField *y_dist,
                                             GwyDataField *score,
                                             gint search_width,
                                             gint search_height,
                                             gint window_width,
                                             gint window_height,
                                             GwyComputationStateType *state,
                                             gint *iteration);

void gwy_data_field_correlate(GwyDataField *data_field,
                              GwyDataField *kernel_field,
                              GwyDataField *score,
                              GwyCorrelationType method);

void gwy_data_field_correlate_iteration(GwyDataField *data_field,
                                        GwyDataField *kernel_field,
                                        GwyDataField *score,
                                        GwyComputationStateType *state,
                                        gint *iteration);

G_END_DECLS

#endif /*__GWY_PROCESS_CORRELATION__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
