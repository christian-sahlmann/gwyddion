/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 */

#ifndef __GWY_PEAKS_H__
#define __GWY_PEAKS_H__

#include <glib.h>
#include <libgwyddion/gwymath.h>

G_BEGIN_DECLS

typedef enum {
    GWY_PEAK_BACKGROUND_ZERO   = 0,
    GWY_PEAK_BACKGROUND_MMSTEP = 1,
} GwyPeakBackgroundType;

typedef enum {
    GWY_PEAK_ORDER_ABSCISSA   = 0,
    GWY_PEAK_ORDER_PROMINENCE = 1,
} GwyPeakOrderType;

typedef enum {
    GWY_PEAK_PROMINENCE = 0,
    GWY_PEAK_ABSCISSA   = 1,
    GWY_PEAK_HEIGHT     = 2,
    GWY_PEAK_AREA       = 3,
    GWY_PEAK_WIDTH      = 4,
} GwyPeakQuantity;

typedef struct _GwyPeaks GwyPeaks;

GwyPeaks* gwy_peaks_new           (void);
void      gwy_peaks_free          (GwyPeaks *peaks);
void      gwy_peaks_set_background(GwyPeaks *peaks,
                                   GwyPeakBackgroundType background);
void      gwy_peaks_set_order     (GwyPeaks *peaks,
                                   GwyPeakOrderType order);
guint     gwy_peaks_analyze_xy    (GwyPeaks *peaks,
                                   const GwyXY *xydata,
                                   guint n,
                                   guint maxpeaks);
guint     gwy_peaks_analyze       (GwyPeaks *peaks,
                                   const gdouble *xdata,
                                   const gdouble *ydata,
                                   guint n,
                                   guint maxpeaks);
guint     gwy_peaks_n_peaks       (GwyPeaks *peaks);
void      gwy_peaks_get_quantity  (GwyPeaks *peaks,
                                   GwyPeakQuantity quantity,
                                   gdouble *data);

G_END_DECLS

#endif /* __GWY_PEAKS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
