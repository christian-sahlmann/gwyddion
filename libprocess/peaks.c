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

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "peaks.h"

typedef struct {
    gdouble prominence;
    gdouble x;
    gdouble height;
    gdouble area;
    gdouble width;
    gint i;
} Peak;

struct _GwyPeaks {
    GArray *peaks;
    GwyPeakBackgroundType background;
    GwyPeakOrderType order;
};

static gint compare_prominence_descending(gconstpointer a,
                                          gconstpointer b);
static gint compare_abscissa_ascending   (gconstpointer a,
                                          gconstpointer b);
/**
 * gwy_peaks_new:
 *
 * Creates a new empty peak analyser.
 *
 * Returns: A new peak analyser.
 *
 * Since: 2.46
 **/
GwyPeaks*
gwy_peaks_new(void)
{
    GwyPeaks *peaks;

    peaks = g_slice_new(GwyPeaks);
    peaks->peaks = g_array_new(FALSE, FALSE, sizeof(Peak));
    peaks->background = GWY_PEAK_BACKGROUND_MMSTEP;
    peaks->order = GWY_PEAK_ORDER_ABSCISSA;

    return peaks;
}

/**
 * gwy_peaks_free:
 * @peaks: A peak analyser.
 *
 * Frees a peak analyser and all associated data.
 *
 * Since: 2.46
 **/
void
gwy_peaks_free(GwyPeaks *peaks)
{
    g_return_if_fail(peaks);
    g_array_free(peaks->peaks, TRUE);
    g_slice_free(GwyPeaks, peaks);
}

/**
 * gwy_peaks_set_background:
 * @peaks: A peak analyser.
 * @background: Background type to use in future analyses.
 *
 * Sets the background type a peak analyser will use.
 *
 * The default background is %GWY_PEAK_BACKGROUND_MMSTEP.
 *
 * Since: 2.46
 **/
void
gwy_peaks_set_background(GwyPeaks *peaks,
                         GwyPeakBackgroundType background)
{
    g_return_if_fail(peaks);
    peaks->background = background;
}

/**
 * gwy_peaks_set_order:
 * @peaks: A peak analyser.
 * @order: Order type to use in future analyses.
 *
 * Sets the order type a peak analyser will use.
 *
 * The default order is %GWY_PEAK_ORDER_ABSCISSA.
 *
 * Since: 2.46
 **/
void
gwy_peaks_set_order(GwyPeaks *peaks,
                    GwyPeakOrderType order)
{
    g_return_if_fail(peaks);
    peaks->order = order;
}

/**
 * gwy_peaks_analyze_xy:
 * @peaks: A peak analyser.
 * @xydata: Curve points (array with @n items) that must be ordered by @x
 *          values in ascending order.
 * @n: Number of data points in the curve.
 * @maxpeaks: Maximum number of the most prominent peaks to locate.
 *
 * Finds peaks a graph curve given as GwyXY data.
 *
 * The peaks are remembered by the analyser and their properties can be
 * subsequently requested using gwy_peaks_get_quantity().
 *
 * Returns: The number of peaks found.
 *
 * Since: 2.46
 **/
guint
gwy_peaks_analyze_xy(GwyPeaks *peaks,
                     const GwyXY *xydata,
                     guint n,
                     guint maxpeaks)
{
    gdouble *data;
    guint i, retval;

    g_return_val_if_fail(xydata, 0);

    data = g_new(gdouble, 2*n);
    for (i = 0; i < n; i++) {
        data[i] = xydata[i].x;
        data[n+i] = xydata[i].y;
    }
    retval = gwy_peaks_analyze(peaks, data, data+n, n, maxpeaks);
    g_free(data);

    return retval;
}

/**
 * gwy_peaks_analyze:
 * @peaks: A peak analyser.
 * @xdata: Abscissa values (array with @n items), must be ordered in ascending
 *         order.
 * @ydata: Ordinate values corresponding to @xdata.
 * @n: Number of data points in the curve.
 * @maxpeaks: Maximum number of the most prominent peaks to locate.
 *
 * Finds peaks a graph curve given as separated @x and @y data.
 *
 * The peaks are remembered by the analyser and their properties can be
 * subsequently requested using gwy_peaks_get_quantity().
 *
 * Returns: The number of peaks found.
 *
 * Since: 2.46
 **/
guint
gwy_peaks_analyze(GwyPeaks *peaks,
                  const gdouble *xdata,
                  const gdouble *ydata,
                  guint n,
                  guint maxpeaks)
{
    GArray *p;
    gint i, k, flatsize;
    gdouble *ydata_filtered;

    g_return_val_if_fail(peaks, 0);
    g_return_val_if_fail(xdata, 0);
    g_return_val_if_fail(ydata, 0);

    p = peaks->peaks;
    g_array_set_size(p, 0);
    if (!n || !maxpeaks)
        return 0;

    /* Perform simple closing. */
    ydata_filtered = g_new(gdouble, n);
    ydata_filtered[0] = ydata[0];
    for (i = 1; i+1 < n; i++) {
        gdouble y = ydata[i];
        gdouble yl = 0.5*(ydata[i+1] + ydata[i-1]);
        ydata_filtered[i] = MAX(y, yl);
    }
    ydata_filtered[n-1] = ydata[n-1];

    /* Find local maxima. */
    flatsize = 0;
    for (i = 1; i+1 < n; i++) {
        gdouble y = ydata_filtered[i];
        gdouble yp = ydata_filtered[i-1];
        gdouble yn = ydata_filtered[i+1];

        /* The normal cases. */
        if (y < yp || y < yn)
            continue;
        if (y > yp && y > yn) {
            Peak peak;
            peak.i = i;
            g_array_append_val(p, peak);
            continue;
        }

        /* Flat tops. */
        if (y == yn && y > yp)
            flatsize = 0;
        else if (y == yn && y == yp)
            flatsize++;
        else if (y == yp && y > yn) {
            Peak peak;
            peak.i = i - flatsize/2;
            g_array_append_val(p, peak);
        }
    }

    g_free(ydata_filtered);

    /* Analyse prominence. */
    for (k = 0; k < p->len; k++) {
        Peak *peak = &g_array_index(p, Peak, k);
        gint ileft, iright;
        gdouble yleft, yright, arealeft, arearight, disp2left, disp2right;

        /* Find the peak extents. */
        for (ileft = peak->i - 1;
             ileft && ydata[ileft] == ydata[ileft+1];
             ileft--)
            ;
        while (ileft && ydata[ileft] > ydata[ileft-1])
            ileft--;
        yleft = ydata[ileft];

        for (iright = peak->i + 1;
             iright+1 < n && ydata[iright] == ydata[iright-1];
             iright++)
            ;
        while (iright+1 < n && ydata[iright] > ydata[iright+1])
            iright++;
        yright = ydata[iright];

        if (peaks->background == GWY_PEAK_BACKGROUND_ZERO) {
            yleft = yright = 0.0;
        }

        /* Calculate height, area, etc. */
        arealeft = arearight = 0.0;
        disp2left = disp2right = 0.0;
        peak->x = xdata[peak->i];
        for (i = ileft; i < peak->i; i++) {
            gdouble xl = xdata[i] - peak->x, xr = xdata[i+1] - peak->x,
                    yl = ydata[i] - yleft, yr = ydata[i+1] - yleft;
            arealeft += (xr - xl)*(yl + yr)/2.0;
            disp2left += (xr*xr*xr*(3.0*yr + yl)
                          - xl*xl*xl*(3.0*yl + yr)
                          - xl*xr*(xl + xr)*(yr - yl))/12.0;
        }
        for (i = iright; i > peak->i; i--) {
            gdouble xl = xdata[i-1] - peak->x, xr = xdata[i] - peak->x,
                    yl = ydata[i-1] - yright, yr = ydata[i] - yright;
            arearight += (xr - xl)*(yl + yr)/2.0;
            disp2right += (xr*xr*xr*(3.0*yr + yl)
                           - xl*xl*xl*(3.0*yl + yr)
                           - xl*xr*(xl + xr)*(yr - yl))/12.0;
        }

        peak->area = arealeft + arearight;
        peak->width = sqrt(0.5*(disp2left/arealeft + disp2right/arearight));
        i = peak->i;
        peak->height = ydata[i] - 0.5*(yleft + yright);
        if (ydata[i] > ydata[i-1] || ydata[i] > ydata[i+1]) {
            gdouble epsp = ydata[i] - ydata[i+1];
            gdouble epsm = ydata[i] - ydata[i-1];
            gdouble dp = xdata[i+1] - xdata[i];
            gdouble dm = xdata[i] - xdata[i-1];
            peak->x += 0.5*(epsm*dp*dp - epsp*dm*dm)/(epsm*dp + epsp*dm);
        }
    }

    for (k = 0; k < p->len; ) {
        Peak *peak = &g_array_index(p, Peak, k);
        gdouble xleft = (k > 0 ? g_array_index(p, Peak, k-1).x : xdata[0]);
        gdouble xright = (k+1 < p->len
                          ? g_array_index(p, Peak, k+1).x
                          : xdata[n-1]);

        if (peak->height <= 0.0
            || peak->area <= 0.0
            || peak->x >= xright
            || peak->x <= xleft) {
            g_array_remove_index(p, k);
        }
        else {
            peak->prominence = log(peak->height * peak->area
                                   * (xright - peak->x) * (peak->x - xleft));
            k++;
        }
    }

    g_array_sort(p, compare_prominence_descending);
    if (p->len > maxpeaks)
        g_array_set_size(p, maxpeaks);

    if (peaks->order == GWY_PEAK_ORDER_ABSCISSA)
        g_array_sort(p, compare_abscissa_ascending);

    return p->len;
}

/**
 * gwy_peaks_n_peaks:
 * @peaks: A peak analyser.
 *
 * Gets the current number of peaks of a peak analyser.
 *
 * Returns: The currently remembered number of peaks.
 *
 * Since: 2.46
 **/
guint
gwy_peaks_n_peaks(GwyPeaks *peaks)
{
    g_return_val_if_fail(peaks, 0);
    return peaks->peaks->len;
}

/**
 * gwy_peaks_get_quantity:
 * @peaks: A peak analyser.
 * @quantity: Peak property to return.
 * @data: Array of sufficient length to hold values for all peaks (their
 *        number is returned by gwy_peaks_n_peaks()).
 *
 * Obtaines values of a given quantity for all found peaks.
 *
 * Since: 2.46
 **/
void
gwy_peaks_get_quantity(GwyPeaks *peaks,
                       GwyPeakQuantity quantity,
                       gdouble *data)
{
    GArray *p;
    guint i;

    g_return_if_fail(peaks);
    g_return_if_fail(data);
    g_return_if_fail(quantity <= GWY_PEAK_WIDTH);

    p = peaks->peaks;
    for (i = 0; i < p->len; i++) {
        const Peak *peak = &g_array_index(p, Peak, i);
        if (quantity == GWY_PEAK_PROMINENCE)
            data[i] = peak->prominence;
        else if (quantity == GWY_PEAK_ABSCISSA)
            data[i] = peak->x;
        else if (quantity == GWY_PEAK_HEIGHT)
            data[i] = peak->height;
        else if (quantity == GWY_PEAK_AREA)
            data[i] = peak->area;
        else if (quantity == GWY_PEAK_WIDTH)
            data[i] = peak->width;
    }
}

static gint
compare_prominence_descending(gconstpointer a, gconstpointer b)
{
    const gdouble pa = ((const Peak*)a)->prominence;
    const gdouble pb = ((const Peak*)b)->prominence;

    if (pa > pb)
        return -1;
    if (pa < pb)
        return 1;
    return 0;
}

static gint
compare_abscissa_ascending(gconstpointer a, gconstpointer b)
{
    const gdouble xa = ((const Peak*)a)->x;
    const gdouble xb = ((const Peak*)b)->x;

    if (xb > xa)
        return -1;
    if (xb < xa)
        return 1;
    return 0;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwypeaks
 * @title: GwyPeaks
 * @short_description: Graph peak analyser
 **/

/**
 * GwyPeaks:
 *
 * #GwyPeaks is an opaque data structure and should be only manipulated
 * with the functions below.
 *
 * Since: 2.46
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
