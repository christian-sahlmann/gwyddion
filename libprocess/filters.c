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

#include <string.h>
#include <stdlib.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "datafield.h"

static gint thin_data_field(GwyDataField *data_field);


/**
 * gwy_data_field_area_convolve:
 * @data_field: A data field to convolve.  It must be larger than @kernel_field
 *              (or at least of the same size).
 * @kernel_field: Kenrel field to convolve @data_field with.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Convolves a rectangular part of a data field with given kernel.
 *
 * Since: 1.3
 **/
void
gwy_data_field_area_convolve(GwyDataField *data_field,
                             GwyDataField *kernel_field,
                             gint col, gint row,
                             gint width, gint height)
{
    gint xres, yres, kxres, kyres, i, j, m, n;
    gdouble fieldval, avgval;
    GwyDataField *hlp_df;

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    xres = data_field->xres;
    yres = data_field->yres;
    kxres = kernel_field->xres;
    kyres = kernel_field->yres;
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= xres
                     && row + height <= yres);

    if (kxres > width || kyres > height) {
        g_warning("Kernel size larger than field area size.");
        return;
    }

    hlp_df =
        (GwyDataField *) gwy_data_field_new(xres, yres, data_field->xreal,
                                            data_field->yreal, TRUE);

    avgval = gwy_data_field_area_get_avg(data_field, col, row, width, height);

    for (i = row; i < row + height; i++) {   /*0-yres */
        for (j = col; j < col + width; j++) {       /*0-xres */
            /*target_field->data[j + data_field->xres*i]; */
            for (m = (-kyres/2); m < (kyres - kyres/2); m++) {
                for (n = (-kxres/2); n < (kxres - kxres/2); n++) {
                    if (((j + n) < xres) && ((i + m) < yres) && ((j + n) >= 0)
                        && ((i + m) >= 0))
                        fieldval = data_field->data[(j + n) + xres * (i + m)];
                    else
                        fieldval = avgval;

                    hlp_df->data[j + xres * i] +=
                        fieldval * kernel_field->data[(m + kyres/2)
                                                      + kxres * (n + kxres/2)];
                }
            }
        }
    }

    for (i = row; i < row + height; i++) {
        for (j = col; j < col + width; j++) {
            data_field->data[j + xres * i] = hlp_df->data[j + xres * i];
        }
    }
    g_object_unref(hlp_df);

    gwy_data_field_invalidate(data_field);
}

void
gwy_data_field_convolve(GwyDataField *data_field,
                        GwyDataField *kernel_field,
                        gint ulcol, gint ulrow,
                        gint brcol, gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    gwy_data_field_area_convolve(data_field, kernel_field,
                                 ulcol, ulrow,
                                 brcol-ulcol, brrow-ulrow);
}

/**
 * gwy_data_field_area_filter_mean:
 * @data_field: A data field to apply mean filter to.
 * @size: Averaged area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with mean filter of size @size.
 *
 * Since: 1.3
 **/
void
gwy_data_field_area_filter_mean(GwyDataField *data_field,
                                gint size,
                                gint col, gint row,
                                gint width, gint height)
{
    gint rowstride;
    gint i, j, k;
    gint from, to;
    gdouble *buffer, *data, *p;
    gdouble s;

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(size > 0);
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    buffer = g_new(gdouble, width*height);
    rowstride = data_field->xres;
    data = data_field->data + rowstride*row + col;
/*printf("%d %d, %dx%d\n", col, row, width, height);*/
    /* vertical pass */
    for (j = 0; j < width; j++) {
        for (i = 0; i < height; i++) {
            s = 0.0;
            p = data + j;
            from = MAX(0, i - (size-1)/2);
            to = MIN(height-1, i + size/2);
            for (k = from; k <= to; k++)
                s += p[k*rowstride];
            buffer[i*width + j] = s/(to - from + 1);
        }
    }

    /* horizontal pass */
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            s = 0.0;
            p = buffer + i*width;
            from = MAX(0, j - (size-1)/2);
            to = MIN(width-1, j + size/2);
            for (k = from; k <= to; k++)
                s += p[k];
            data[i*rowstride + j] = s/(to - from + 1);
        }
    }

    g_free(buffer);
    gwy_data_field_invalidate(data_field);
}

void
gwy_data_field_filter_mean(GwyDataField *data_field,
                           gint size,
                           gint ulcol, gint ulrow,
                           gint brcol, gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    gwy_data_field_area_filter_mean(data_field, size,
                                    ulcol, ulrow,
                                    brcol-ulcol, brrow-ulrow);
}


/**
 * gwy_data_field_area_filter_canny:
 * @data_field: A data field to apply mean filter to.
 * @threshold: Slope detection threshold (range 0..1).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with canny edge detector filter.
 *
 * In version 1.6 this function filters whole data field.
 *
 * Since: 1.6
 **/
void
gwy_data_field_area_filter_canny(GwyDataField *data_field,
                                 gdouble threshold,
                                 gint col, gint row,
                                 gint width, gint height)
{
    GwyDataField *sobel_horizontal;
    GwyDataField *sobel_vertical;
    gint i, j, k;
    gdouble angle;
    gboolean pass;
    gdouble *data;

    sobel_horizontal = GWY_DATA_FIELD(gwy_data_field_new(data_field->xres,
                                                         data_field->yres,
                                                         data_field->xreal,
                                                         data_field->yreal,
                                                         FALSE));
    sobel_vertical = GWY_DATA_FIELD(gwy_data_field_new(data_field->xres,
                                                       data_field->yres,
                                                       data_field->xreal,
                                                       data_field->yreal,
                                                       FALSE));
    gwy_data_field_area_copy(data_field, sobel_horizontal,
                             0, 0,
                             data_field->xres,
                             data_field->yres,
                             0, 0);
    gwy_data_field_area_copy(data_field, sobel_vertical,
                             0, 0,
                             data_field->xres,
                             data_field->yres,
                             0, 0);

    gwy_data_field_area_filter_sobel(sobel_horizontal,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     0, 0,
                                     data_field->xres,
                                     data_field->yres);

    gwy_data_field_area_filter_sobel(sobel_vertical,
                                     GTK_ORIENTATION_VERTICAL,
                                     0, 0,
                                     data_field->xres,
                                     data_field->yres);

    data = data_field->data;
    for (k = 0; k < (data_field->xres*data_field->yres); k++)
        data[k] = fabs(sobel_horizontal->data[k])
                  + fabs(sobel_vertical->data[k]);


    threshold = gwy_data_field_get_min(data_field) +
               (gwy_data_field_get_max(data_field)
                - gwy_data_field_get_min(data_field))*threshold;

    for (i = 0; i < data_field->yres; i++) {
        for (j = 0; j < data_field->xres; j++) {
            pass = FALSE;
            if (data[j + data_field->xres*i] > threshold
                && i > 0 && j > 0
                && i < (data_field->yres - 1)
                && j < (data_field->xres - 1))
            {
                angle = atan2(sobel_vertical->data[j + data_field->xres*i],
                              sobel_horizontal->data[j + data_field->xres*i]);

                if (angle < 0.3925 || angle > 5.8875
                    || (angle > 2.7475 && angle < 3.5325)) {
                    if (data[j + 1 + data_field->xres*i] > threshold)
                        pass = TRUE;
                }
                else if ((angle > 1.178 && angle < 1.9632)
                         || (angle > 4.318 && angle < 5.1049)) {
                    if (data[j + 1 + data_field->xres*(i + 1)] > threshold)
                        pass = TRUE;
                }
                else {
                    if (data[j + data_field->xres*(i + 1)] > threshold)
                        pass = TRUE;
                }
            }
            /*we do not need sobel array more,
             * so use sobel_horizontal to store data results*/
            if (pass)
                sobel_horizontal->data[j + data_field->xres*i] = 1;
            else
                sobel_horizontal->data[j + data_field->xres*i] = 0;
        }
    }
    /*result is now in sobel_horizontal field*/
    gwy_data_field_area_copy(sobel_horizontal, data_field,
                             0, 0, data_field->xres, data_field->yres, 0, 0);

    /*thin the lines*/
    thin_data_field(data_field);
    gwy_data_field_invalidate(data_field);
}


 /**
 * gwy_data_field_area_filter_laplacian:
 * @data_field: A data field to apply mean filter to.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with Laplacian filter.
 *
 * Since: 1.3
 **/
void
gwy_data_field_area_filter_laplacian(GwyDataField *data_field,
                                     gint col, gint row,
                                     gint width, gint height)
{
    GwyDataField *kernel_df;

    kernel_df = GWY_DATA_FIELD(gwy_data_field_new(3, 3, 3, 3, FALSE));
    kernel_df->data[0] = 0;               /* 0 1 2*/
    kernel_df->data[1] = 1;               /* 3 4 5*/
    kernel_df->data[2] = 0;               /* 6 7 8*/
    kernel_df->data[3] = 1;
    kernel_df->data[4] = -4;
    kernel_df->data[5] = 1;
    kernel_df->data[6] = 0;
    kernel_df->data[7] = 1;
    kernel_df->data[8] = 0;

    gwy_data_field_convolve(data_field, kernel_df, col, row, width, height);

    g_object_unref(kernel_df);
    gwy_data_field_invalidate(data_field);
}

void
gwy_data_field_filter_laplacian(GwyDataField *data_field,
                                gint ulcol, gint ulrow,
                                gint brcol, gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    gwy_data_field_area_filter_laplacian(data_field,
                                         ulcol, ulrow,
                                         brcol-ulcol, brrow-ulrow);
}

/**
 * gwy_data_field_area_filter_sobel:
 * @data_field: A data field to apply mean filter to.
 * @orientation: Filter orientation.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with Laplacian filter.
 *
 * Since: 1.3
 **/
void
gwy_data_field_area_filter_sobel(GwyDataField *data_field,
                                 GtkOrientation orientation,
                                 gint col, gint row,
                                 gint width, gint height)
{
    GwyDataField *kernel_df;

    gwy_debug("");
    kernel_df = GWY_DATA_FIELD(gwy_data_field_new(3, 3, 3, 3, FALSE));

    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        kernel_df->data[0] = 0.25;
        kernel_df->data[1] = 0;
        kernel_df->data[2] = -0.25;
        kernel_df->data[3] = 0.5;
        kernel_df->data[4] = 0;
        kernel_df->data[5] = -0.5;
        kernel_df->data[6] = 0.25;
        kernel_df->data[7] = 0;
        kernel_df->data[8] = -0.25;
    }
    else {
        kernel_df->data[0] = 0.25;
        kernel_df->data[1] = 0.5;
        kernel_df->data[2] = 0.25;
        kernel_df->data[3] = 0;
        kernel_df->data[4] = 0;
        kernel_df->data[5] = 0;
        kernel_df->data[6] = -0.25;
        kernel_df->data[7] = -0.5;
        kernel_df->data[8] = -0.25;
    }
    gwy_data_field_area_convolve(data_field, kernel_df,
                                 col, row, width, height);
    g_object_unref(kernel_df);
    gwy_data_field_invalidate(data_field);
}

void
gwy_data_field_filter_sobel(GwyDataField *data_field,
                            GtkOrientation orientation,
                            gint ulcol, gint ulrow,
                            gint brcol, gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    gwy_data_field_area_filter_sobel(data_field, orientation,
                                     ulcol, ulrow,
                                     brcol-ulcol, brrow-ulrow);
}

/**
 * gwy_data_field_area_filter_prewitt:
 * @data_field: A data field to apply mean filter to.
 * @orientation: Filter orientation.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with Prewitt filter.
 *
 * Since: 1.3
 **/
void
gwy_data_field_area_filter_prewitt(GwyDataField *data_field,
                                   GtkOrientation orientation,
                                   gint col, gint row,
                                   gint width, gint height)
{
    GwyDataField *kernel_df;

    gwy_debug("");
    kernel_df = GWY_DATA_FIELD(gwy_data_field_new(3, 3, 3, 3, FALSE));

    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        kernel_df->data[0] = 1.0/3.0;
        kernel_df->data[1] = 0;
        kernel_df->data[2] = -1.0/3.0;
        kernel_df->data[3] = 1.0/3.0;
        kernel_df->data[4] = 0;
        kernel_df->data[5] = -1.0/3.0;
        kernel_df->data[6] = 1.0/3.0;
        kernel_df->data[7] = 0;
        kernel_df->data[8] = -1.0/3.0;
    }
    else {
        kernel_df->data[0] = 1.0/3.0;
        kernel_df->data[1] = 1.0/3.0;
        kernel_df->data[2] = 1.0/3.0;
        kernel_df->data[3] = 0;
        kernel_df->data[4] = 0;
        kernel_df->data[5] = 0;
        kernel_df->data[6] = -1.0/3.0;
        kernel_df->data[7] = -1.0/3.0;
        kernel_df->data[8] = -1.0/3.0;
    }
    gwy_data_field_convolve(data_field, kernel_df, col, row, width, height);
    g_object_unref(kernel_df);
    gwy_data_field_invalidate(data_field);
}

void
gwy_data_field_filter_prewitt(GwyDataField *data_field,
                              GtkOrientation orientation,
                              gint ulcol, gint ulrow,
                              gint brcol, gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    gwy_data_field_area_filter_prewitt(data_field, orientation,
                                       ulcol, ulrow,
                                       brcol-ulcol, brrow-ulrow);
}

/**
 * gwy_data_field_area_filter_median:
 * @data_field: A data field to apply mean filter to.
 * @size: Averaged area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with median filter.
 *
 * Since: 1.3
 **/
void
gwy_data_field_area_filter_median(GwyDataField *data_field,
                                  gint size,
                                  gint col, gint row,
                                  gint width, gint height)
{

    gint rowstride;
    gint i, j, k, len;
    gint xfrom, xto, yfrom, yto;
    gdouble *buffer, *data, *kernel;

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(size > 0);
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    buffer = g_new(gdouble, width*height);
    kernel = g_new(gdouble, size*size);
    rowstride = data_field->xres;
    data = data_field->data + rowstride*row + col;

    for (i = 0; i < height; i++) {
        yfrom = MAX(0, i - (size-1)/2);
        yto = MIN(height-1, i + size/2);
        for (j = 0; j < width; j++) {
            xfrom = MAX(0, j - (size-1)/2);
            xto = MIN(width-1, j + size/2);
            len = xto - xfrom + 1;
            for (k = yfrom; k <= yto; k++)
                memcpy(kernel + len*(k - yfrom),
                       data + k*rowstride + xfrom,
                       len*sizeof(gdouble));
            buffer[i*width + j] = gwy_math_median(len*(yto - yfrom + 1),
                                                  kernel);
        }
    }

    g_free(kernel);
    for (i = 0; i < height; i++)
        memcpy(data + i*rowstride, buffer + i*width, width*sizeof(gdouble));
    g_free(buffer);
    gwy_data_field_invalidate(data_field);
}

void
gwy_data_field_filter_median(GwyDataField *data_field,
                             gint size,
                             gint ulcol, gint ulrow,
                             gint brcol, gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    gwy_data_field_area_filter_median(data_field, size,
                                      ulcol, ulrow,
                                      brcol-ulcol, brrow-ulrow);
}

void
gwy_data_field_area_filter_conservative(GwyDataField *data_field,
                                        gint size,
                                        gint col, gint row,
                                        gint width, gint height)
{
    gint xres, yres, kxres, kyres, i, j, m, n;
    gint nb;
    gdouble maxval, minval;
    GwyDataField *hlp_df;

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(size > 0);
    xres = data_field->xres;
    yres = data_field->yres;
    kxres = size;
    kyres = size;
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= xres
                     && row + height <= yres);
    if (kxres > width || kyres > height) {
        g_warning("Kernel size larger than field area size.");
        return;
    }

    hlp_df = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, data_field->xreal,
                                               data_field->yreal, TRUE));


    for (i = row; i < row + height; i++) {
        for (j = col; j < col + width; j++) {
            nb = 0;
            maxval = -G_MAXDOUBLE;
            minval = G_MAXDOUBLE;
            for (m = (-kyres/2); m < (kyres - kyres/2); m++) {
                for (n = (-kxres/2); n < (kxres - kxres/2); n++) {
                    if (((j + n) < xres) && ((i + m) < yres) && ((j + n) >= 0)
                        && ((i + m) >= 0) && !(m==0 && n==0)) {

                        if (minval > data_field->data[(j + n) + xres * (i + m)])
                            minval = data_field->data[(j + n) + xres * (i + m)];
                        if (maxval < data_field->data[(j + n) + xres * (i + m)])
                            maxval = data_field->data[(j + n) + xres * (i + m)];
                    }
                }
            }

            if (data_field->data[j + xres * i] > maxval)
                hlp_df->data[j + xres * i] = maxval;
            else if (data_field->data[j + xres * i] < minval)
                hlp_df->data[j + xres * i] = minval;
            else
                hlp_df->data[j + xres * i] = data_field->data[j + xres * i];

        }
    }

    for (i = row; i < row + height; i++) {
        for (j = col; j < col + width; j++) {
            data_field->data[j + xres * i] = hlp_df->data[j + xres * i];
        }
    }

    g_object_unref(hlp_df);
    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_area_filter_conservative:
 * @data_field: A data field to apply mean filter to.
 * @size: Filtered area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with conservative denoise filter.
 *
 * Since: 1.3
 **/
void
gwy_data_field_filter_conservative(GwyDataField *data_field,
                                   gint size,
                                   gint ulcol, gint ulrow, gint brcol,
                                   gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    gwy_data_field_area_filter_conservative(data_field, size,
                                            ulcol, ulrow,
                                            brcol-ulcol, brrow-ulrow);
}



static inline gint
pixel_status(GwyDataField *data_field, gint i, gint j)
{
    if (data_field->data[j + data_field->xres * i] == 0)
        return 0;
    else
        return 1;
}


static gint
znzt_val(GwyDataField *data_field, gint i, gint j)
{
    gint ch = 0;
    gint pi[9];
    gint pj[9], k;

    pi[0] = i + 1;
    pi[1] = i;
    pi[2] = i - 1;
    pi[3] = i - 1;
    pi[4] = i - 1;
    pi[5] = i;
    pi[6] = i + 1;
    pi[7] = i + 1;
    pi[8] = i + 1;

    pj[0] = j + 1;
    pj[1] = j + 1;
    pj[2] = j + 1;
    pj[3] = j;
    pj[4] = j - 1;
    pj[5] = j - 1;
    pj[6] = j - 1;
    pj[7] = j;
    pj[8] = j + 1;

    for (k = 0; k < 8; k++)
        if (pixel_status(data_field, pi[k], pj[k]) == 0
            && pixel_status(data_field, pi[k + 1], pj[k + 1]) == 1)
            ch++;

    return ch;
}

static gint
nzn_val(GwyDataField *data_field, gint i, gint j)
{
    gint ch, ip, jp;

    ch = 0;
    for (ip = -1; ip <= 1; ip++) {
        for (jp = -1; jp <= 1; jp++) {
            if (!(ip == 0 && jp == 0))
                ch += pixel_status(data_field, i + ip, j + jp);
        }
    }

    return ch;
}

static gint
pixel_thinnable(GwyDataField *data_field, gint i, gint j)
{
    gint xres, yres;
    gint c1, c2, c3, c4;
    gdouble val;

    xres = data_field->xres;
    yres = data_field->yres;

    if (i <= 1 || j <= 1 || i >= (xres - 2) || (j >= yres - 2))
        return -1;

    c1 = c2 = c3 = c4 = 0;

    if (znzt_val(data_field, i, j) == 1)
        c1 = 1;
    val = nzn_val(data_field, i, j);

    if (val >= 2 && val <= 6)
        c2 = 1;

    if ((znzt_val(data_field, i + 1, j) != 1)
        || ((pixel_status(data_field, i, j + 1)
             * pixel_status(data_field, i, j - 1)
             * pixel_status(data_field, i + 1, j)) == 0))
        c3 = 1;

    if ((znzt_val(data_field, i, j + 1) != 1)
        || ((pixel_status(data_field, i, j + 1)
             * pixel_status(data_field, i - 1, j)
             * pixel_status(data_field, i + 1, j) == 0)))
        c4 = 1;

    if (c1 == 1 && c2 == 1 && c3 == 1 && c4 == 1)
        return 1;
    else
        return 0;

}

static gint
thinstep(GwyDataField *data_field)
{
    GwyDataField *hlp;
    gint i, j, ch;

    hlp = GWY_DATA_FIELD(gwy_data_field_new(data_field->xres,
                                            data_field->yres,
                                            data_field->xreal,
                                            data_field->yreal, TRUE));

    ch = 0;
    for (i = 2; i < (data_field->yres - 1); i++) {
        for (j = 2; j < (data_field->xres - 1); j++) {
            if (pixel_status(data_field, i, j) == 1
                && pixel_thinnable(data_field, i, j) == 1) {
                ch++;
                hlp->data[j + data_field->xres * i] = 1;
            }
        }
    }
    for (i = 2; i < (data_field->yres - 1); i++) {
        for (j = 2; j < (data_field->xres - 1); j++) {
            if (hlp->data[j + data_field->xres * i] == 1)
                data_field->data[j + data_field->xres * i] = 0;
        }
    }

    gwy_data_field_invalidate(data_field);

    return ch;
}

static gint
thin_data_field(GwyDataField *data_field)
{
    gint k, n;

    for (k = 0; k < 2000; k++) {
        n = thinstep(data_field);
        if (n == 0)
            break;
    }
    return k;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
