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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/dwt.h>
#include <libprocess/stats.h>

/* INTERPOLATION: New (not applicable). */

typedef struct {
    gint ncof;
    gint ioff;
    gint joff;
    gdouble *cc;
    gdouble *cr;
    GwyDataLine *wksp;  /* working space */
} GwyDWTFilter;

/*private functions prototypes*/
static void         gwy_data_line_dwt_real(GwyDataLine *dline,
                                           GwyDWTFilter *wt,
                                           GwyTransformDirection direction,
                                           gint minsize);
static GwyDataLine* pwt                   (GwyDWTFilter *wt,
                                           GwyDataLine *dline,
                                           gint n,
                                           GwyTransformDirection direction);
static GwyDWTFilter* wtset_new  (GwyDataLine *wt_coefs);
static void          wtset_free (GwyDWTFilter *wt);

static gint remove_by_universal_threshold(GwyDataField *dfield,
                                          gint ulcol,
                                          gint ulrow,
                                          gint brcol,
                                          gint brrow,
                                          gboolean hard,
                                          gdouble threshold);
static gint remove_by_adaptive_threshold(GwyDataField *dfield,
                                         gint ulcol,
                                         gint ulrow,
                                         gint brcol,
                                         gint brrow,
                                         gboolean hard,
                                         gdouble multiple_threshold,
                                         gdouble noise_variance);
static gint remove_by_threshold(GwyDataField *dfield,
                                gint ulcol,
                                gint ulrow,
                                gint brcol,
                                gint brrow,
                                gboolean hard,
                                gdouble multiple_threshold,
                                gdouble noise_variance);

static gint find_anisotropy(GwyDataField *dfield,
                            GwyDataField *mask,
                            gint ul,
                            gint br,
                            gdouble threshold,
                            gdouble setsize);

static const gdouble haar[] = { 1, 1 };
static const gdouble daub4[] = {
    0.68301270189222,
    1.18301270189222,
    0.31698729810778,
    -0.18301270189222,
};
static const gdouble daub6[] = {
    0.47046720778405,
    1.14111691583131,
    0.65036500052554,
    -0.19093441556797,
    -0.12083220831036,
    0.04981749973164,
};
static const gdouble daub8[] = {
    0.32580342805130,
    1.01094571509183,
    0.89220013824676,
    -0.03957502623564,
    -0.26450716736904,
    0.04361630047418,
    0.04650360107098,
    -0.01498698933036,
};
static const gdouble daub12[] = {
    0.111540743350*G_SQRT2,
    0.494623890398*G_SQRT2,
    0.751133908021*G_SQRT2,
    0.315250351709*G_SQRT2,
    -0.226264693965*G_SQRT2,
    -0.129766867567*G_SQRT2,
    0.097501605587*G_SQRT2,
    0.027522865530*G_SQRT2,
    -0.031582039318*G_SQRT2,
    0.000553842201*G_SQRT2,
    0.004777257511*G_SQRT2,
    -0.001077301085*G_SQRT2,
};
static const gdouble daub20[] = {
    0.026670057901*G_SQRT2,
    0.188176800078*G_SQRT2,
    0.527201188932*G_SQRT2,
    0.688459039454*G_SQRT2,
    0.281172343661*G_SQRT2,
    -0.249846424327*G_SQRT2,
    -0.195946274377*G_SQRT2,
    0.127369340336*G_SQRT2,
    0.093057364604*G_SQRT2,
    -0.071394147166*G_SQRT2,
    -0.029457536822*G_SQRT2,
    0.033212674059*G_SQRT2,
    0.003606553567*G_SQRT2,
    -0.010733175483*G_SQRT2,
    0.001395351747*G_SQRT2,
    0.001992405295*G_SQRT2,
    -0.000685856695*G_SQRT2,
    -0.000116466855*G_SQRT2,
    0.000093588670*G_SQRT2,
    -0.000013264203*G_SQRT2,
};

/* indexed by GwyDWTType */
static const struct {
    guint size;
    const gdouble *coeff;
}
coefficients[] = {
    { G_N_ELEMENTS(haar),   haar   },
    { G_N_ELEMENTS(daub4),  daub4  },
    { G_N_ELEMENTS(daub6),  daub6  },
    { G_N_ELEMENTS(daub8),  daub8  },
    { G_N_ELEMENTS(daub12), daub12 },
    { G_N_ELEMENTS(daub20), daub20 },
};

/*public functions*/


/**
 * gwy_dwt_set_coefficients:
 * @dline: Data line to store wavelet coefficients to (or %NULL to allocate
 *         a new one).
 * @type: Wavelet type.
 *
 * Fills resampled or nely allocated data line with wavelet coefficients.
 *
 * Returns: resampled or newly allocated GwyDataLine with wavelet coefficients.
 **/
GwyDataLine*
gwy_dwt_set_coefficients(GwyDataLine *dline, GwyDWTType type)
{
    guint size;

    g_return_val_if_fail((gint)type >= 0 && type <= GWY_DWT_DAUB20, NULL);
    size = coefficients[type].size;
    if (!dline)
        dline = gwy_data_line_new(size, size, FALSE);
    else
        gwy_data_line_resample(dline, size, GWY_INTERPOLATION_NONE);

    memcpy(dline->data, coefficients[type].coeff, size*sizeof(gdouble));

    return dline;
}

/**
 * gwy_data_line_dwt:
 * @dline: Data line to be transformed, it must have at least four samples.
 * @wt_coefs: Data line where the wavelet transform coefficients are stored.
 * @direction: Transform direction.
 * @minsize: size of minimal transform result block
 *
 * Performs steps of the wavelet decomposition.
 *
 * The smallest low pass coefficients block is equal to @minsize. Run with
 * @minsize = @dline->res/2 to perform one step of decomposition
 * or @minsize = 4 to perform full decomposition (or anything between).
 *
 * Returns: Transformed data line, that is @dline itself.
 *          XXX Why? XXX
 **/
GwyDataLine*
gwy_data_line_dwt(GwyDataLine *dline,
                  GwyDataLine *wt_coefs,
                  GwyTransformDirection direction,
                  gint minsize)
{
    GwyDWTFilter *wt;

    g_return_val_if_fail(GWY_IS_DATA_LINE(dline), NULL);
    g_return_val_if_fail(dline->res >= 4, NULL);
    g_return_val_if_fail(GWY_IS_DATA_LINE(wt_coefs), NULL);

    wt = wtset_new(wt_coefs);
    wt->wksp = gwy_data_line_new(dline->res+1, dline->res+1, FALSE);
    gwy_data_line_dwt_real(dline, wt, direction, minsize);
    wtset_free(wt);

    return dline;
}

/**
 * gwy_data_field_xdwt:
 * @dfield: Data field to be transformed.
 * @wt_coefs: Data line where the wavelet transform coefficients are stored.
 * @direction: Transform direction.
 * @minsize: size of minimal transform result block
 *
 * Performs steps of the X-direction image wavelet decomposition.
 *
 * The smallest low pass coefficients block is equal to @minsize. Run with
 * @minsize = @dfield->xres/2 to perform one step of decomposition
 * or @minsize = 4 to perform full decomposition (or anything between).
 *
 * Returns: Transformed data field (that is @dfield itself).
 *          XXX Why? XXX
 **/
GwyDataField*
gwy_data_field_xdwt(GwyDataField *dfield,
                    GwyDataLine *wt_coefs,
                    GwyTransformDirection direction,
                    gint minsize)
{
    GwyDWTFilter *wt;
    GwyDataLine *rin;
    gint k;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), NULL);
    g_return_val_if_fail(GWY_IS_DATA_LINE(wt_coefs), NULL);

    rin = gwy_data_line_new(dfield->xres, dfield->xreal, FALSE);
    wt = wtset_new(wt_coefs);
    wt->wksp = gwy_data_line_new(rin->res+1, rin->res+1, FALSE);

    for (k = 0; k < dfield->yres; k++) {
        gwy_data_field_get_row(dfield, rin, k);
        gwy_data_line_dwt_real(rin, wt, direction, minsize);
        gwy_data_field_set_row(dfield, rin, k);
    }

    g_object_unref(rin);
    wtset_free(wt);

    return dfield;
}

/**
 * gwy_data_field_ydwt:
 * @dfield: Data field to be transformed.
 * @wt_coefs: Data line where the wavelet transform coefficients are stored.
 * @direction: Transform direction.
 * @minsize: size of minimal transform result block
 *
 * Performs steps of the Y-direction image wavelet decomposition.
 *
 * The smallest low pass coefficients block is equal to @minsize. Run with
 * @minsize = @dfield->yres/2 to perform one step of decomposition
 * or @minsize = 4 to perform full decomposition (or anything between).
 *
 * Returns: Transformed data field (that is @dfield itself).
 *          XXX Why? XXX
 **/
GwyDataField*
gwy_data_field_ydwt(GwyDataField *dfield,
                    GwyDataLine *wt_coefs,
                    GwyTransformDirection direction,
                    gint minsize)
{
    GwyDWTFilter *wt;
    GwyDataLine *rin;
    gint k;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), NULL);
    g_return_val_if_fail(GWY_IS_DATA_LINE(wt_coefs), NULL);

    rin = gwy_data_line_new(dfield->yres, dfield->yreal, FALSE);
    wt = wtset_new(wt_coefs);
    wt->wksp = gwy_data_line_new(rin->res+1, rin->res+1, FALSE);

    for (k = 0; k < dfield->xres; k++) {
        gwy_data_field_get_column(dfield, rin, k);
        gwy_data_line_dwt_real(rin, wt, direction, minsize);
        gwy_data_field_set_column(dfield, rin, k);
    }

    g_object_unref(rin);
    wtset_free(wt);

    return dfield;
}

/**
 * gwy_data_field_dwt:
 * @dfield: Data field to be transformed (must be square).
 * @wt_coefs: Data line where the wavelet transform coefficients are stored.
 * @direction: Transform direction.
 * @minsize: size of minimal transform result block
 *
 * Performs steps of the 2D image wavelet decomposition.
 *
 * The smallest low pass coefficients block is equal to @minsize. Run with
 * @minsize = @dfield->xres/2 to perform one step of decomposition
 * or @minsize = 4 to perform full decomposition (or anything between).
 *
 * Returns: Transformed data field (that is @dfield itself).
 *          XXX Why? XXX
 **/
GwyDataField*
gwy_data_field_dwt(GwyDataField *dfield,
                   GwyDataLine *wt_coefs,
                   GwyTransformDirection direction,
                   gint minsize)
{
    GwyDWTFilter *wt;
    GwyDataLine *rin;
    gint nn, k;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), NULL);
    g_return_val_if_fail(GWY_IS_DATA_LINE(wt_coefs), NULL);
    g_return_val_if_fail(dfield->xres == dfield->yres, NULL);

    rin = gwy_data_line_new(dfield->xres, dfield->xreal, FALSE);
    wt = wtset_new(wt_coefs);
    wt->wksp = gwy_data_line_new(rin->res+1, rin->res+1, FALSE);

    switch (direction) {
        case GWY_TRANSFORM_DIRECTION_FORWARD:
        for (nn = dfield->xres; nn >= 2*minsize; nn >>= 1) {
            for (k = 0; k < nn; k++) {
                gwy_data_field_get_row_part(dfield, rin, k, 0, nn);
                gwy_data_line_dwt_real(rin, wt, direction, nn/2);
                gwy_data_field_set_row_part(dfield, rin, k, 0, nn);
            }
            for (k = 0; k < nn; k++) {
                gwy_data_field_get_column_part(dfield, rin, k, 0, nn);
                gwy_data_line_dwt_real(rin, wt, direction, nn/2);
                gwy_data_field_set_column_part(dfield, rin, k, 0, nn);
            }
        }
        break;

        case GWY_TRANSFORM_DIRECTION_BACKWARD:
        for (nn = 2*minsize; nn <= dfield->xres; nn <<= 1) {
            for (k = 0; k < nn; k++) {
                gwy_data_field_get_row_part(dfield, rin, k, 0, nn);
                gwy_data_line_dwt_real(rin, wt, direction, nn/2);
                gwy_data_field_set_row_part(dfield, rin, k, 0, nn);
            }
            for (k = 0; k < nn; k++) {
                gwy_data_field_get_column_part(dfield, rin, k, 0, nn);
                gwy_data_line_dwt_real(rin, wt, direction, nn/2);
                gwy_data_field_set_column_part(dfield, rin, k, 0, nn);
            }
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }

    g_object_unref(rin);
    wtset_free(wt);

    return dfield;
}

/**
 * gwy_data_field_dwt_denoise:
 * @dfield: Data field to be denoised (must be square).
 * @wt_coefs: Data line where the wavelet transform coefficients are stored.
 * @hard: Set to %TRUE to apply hard thresholding.
 * @multiple_threshold: A positive value to multiply threshold with (to change
 *                      thresholding).
 * @type: Type of thresholding
 *
 * Performs wavelet denoising.
 *
 * It is based on threshold obtained from noise variance
 * (obtained from high scale wvelet coefficients). This threshold can
 * be multiplied by user defined value.
 **/
GwyDataField*
gwy_data_field_dwt_denoise(GwyDataField *dfield,
                           GwyDataLine *wt_coefs,
                           gboolean hard,
                           gdouble multiple_threshold,
                           GwyDWTDenoiseType type)
{
    gint br, ul, ulcol, ulrow, brcol, brrow, count;
    gdouble median, noise_variance, threshold;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), NULL);
    g_return_val_if_fail(GWY_IS_DATA_LINE(wt_coefs), NULL);
    g_return_val_if_fail(dfield->xres == dfield->yres, NULL);

    gwy_data_field_dwt(dfield, wt_coefs, GWY_TRANSFORM_DIRECTION_FORWARD, 4);

    ulcol = dfield->xres/2; ulrow = dfield->xres/2;
    brcol = dfield->xres; brrow = dfield->xres;

    median = gwy_data_field_area_get_median(dfield,
                                            ulcol, ulrow,
                                            brcol-ulcol, brrow-ulrow);
    noise_variance = median/0.6745;

    if (type == GWY_DWT_DENOISE_UNIVERSAL)
        threshold = noise_variance*sqrt(2*log(dfield->xres*dfield->yres/4));
    else
        threshold = 0.0;
    threshold *= multiple_threshold;

    for (br = dfield->xres; br > 4; br >>= 1) {
        ul = br/2;

        count = 0;

        ulcol = ul;
        ulrow = ul;
        brcol = br;
        brrow = br;
        switch (type) {
            case GWY_DWT_DENOISE_SCALE_ADAPTIVE:
            count = remove_by_threshold(dfield,
                                        ulcol, ulrow, brcol, brrow,
                                        hard, multiple_threshold,
                                        noise_variance);
            break;

            case GWY_DWT_DENOISE_UNIVERSAL:
            count = remove_by_universal_threshold(dfield,
                                                  ulcol, ulrow, brcol, brrow,
                                                  hard, threshold);
            break;

            case GWY_DWT_DENOISE_SPACE_ADAPTIVE:
            count = remove_by_adaptive_threshold(dfield,
                                                 ulcol, ulrow, brcol, brrow,
                                                 hard, multiple_threshold,
                                                 noise_variance);
            break;

            default:
            g_assert_not_reached();
            break;
        }

        ulcol = 0;
        ulrow = ul;
        brcol = ul;
        brrow = br;
        switch (type){
            case GWY_DWT_DENOISE_SCALE_ADAPTIVE:
            count = remove_by_threshold(dfield,
                                        ulcol, ulrow, brcol, brrow,
                                        hard, multiple_threshold,
                                        noise_variance);
            break;

            case GWY_DWT_DENOISE_UNIVERSAL:
            count = remove_by_universal_threshold(dfield,
                                                  ulcol, ulrow, brcol, brrow,
                                                  hard, threshold);
            break;

            case GWY_DWT_DENOISE_SPACE_ADAPTIVE:
            count = remove_by_adaptive_threshold(dfield,
                                                 ulcol, ulrow, brcol, brrow,
                                                 hard, multiple_threshold,
                                                 noise_variance);
            break;

            default:
            g_assert_not_reached();
            break;
        }

        ulcol = ul;
        ulrow = 0;
        brcol = br;
        brrow = ul;
        switch (type){
            case GWY_DWT_DENOISE_SCALE_ADAPTIVE:
            count = remove_by_threshold(dfield,
                                        ulcol, ulrow, brcol, brrow,
                                        hard, multiple_threshold,
                                        noise_variance);
            break;

            case GWY_DWT_DENOISE_UNIVERSAL:
            count = remove_by_universal_threshold(dfield,
                                                  ulcol, ulrow, brcol, brrow,
                                                  hard, threshold);
            break;

            case GWY_DWT_DENOISE_SPACE_ADAPTIVE:
            count = remove_by_adaptive_threshold(dfield,
                                                 ulcol, ulrow, brcol, brrow,
                                                 hard, multiple_threshold,
                                                 noise_variance);
            break;

            default:
            g_assert_not_reached();
            break;
        }

    }

    gwy_data_field_dwt(dfield, wt_coefs, GWY_TRANSFORM_DIRECTION_BACKWARD, 4);
    return dfield;
}


/**
 * gwy_data_field_mark_anisotropy:
 * @dfield: Data field to mark anisotropy of (must be square).
 * @wt_coefs: Data line to store wavelet transform coefficients to.
 * @minsize: size of minimal transform result block
 *
 * Performs steps of the 2D image wavelet decomposition.
 *
 * The smallest low pass coefficients block is equal to @minsize. Run with
 * @minsize = @dfield->xres/2 to perform one step of decomposition
 * or @minsize = 4 to perform full decomposition (or anything between).
 *
 * Returns: Transformed data field (that is @dfield itself).
 *          XXX Why? XXX
 **/
GwyDataField*
gwy_data_field_dwt_mark_anisotropy(GwyDataField *dfield,
                                   GwyDataField *mask,
                                   GwyDataLine *wt_coefs,
                                   gdouble ratio,
                                   gint lowlimit)
{
    GwyDataField *buffer;
    gint br, ul, count;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), NULL);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(mask), NULL);
    g_return_val_if_fail(GWY_IS_DATA_LINE(wt_coefs), NULL);
    g_return_val_if_fail(dfield->xres == dfield->yres, NULL);

    buffer = gwy_data_field_duplicate(dfield);
    gwy_data_field_clear(mask);

    gwy_data_field_dwt(buffer, wt_coefs, 1, lowlimit);

    for (br = dfield->xres; br > lowlimit; br >>= 1) {
        ul = br/2;
        count = find_anisotropy(buffer, mask, ul, br, ratio, 3.5);
    }

    g_object_unref(buffer);
    return mask;
}

/*private functions*/

static void
gwy_data_line_dwt_real(GwyDataLine *dline,
                       GwyDWTFilter *wt,
                       GwyTransformDirection direction,
                       gint minsize)
{
    gint nn;
    gint n;

    n = dline->res;
    dline->data -= 1;    /* XXX: hack, pwt() uses 1-based indexing */

    switch (direction) {
        case GWY_TRANSFORM_DIRECTION_FORWARD:
        for (nn = n; nn >= 2*minsize; nn >>= 1)
            pwt(wt, dline, nn, direction);
        break;

        case GWY_TRANSFORM_DIRECTION_BACKWARD:
        for (nn = 2*minsize; nn <= n; nn <<= 1)
            pwt(wt, dline, nn, direction);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    dline->data += 1;    /* XXX: hack, pwt() uses 1-based indexing */
}

static GwyDataLine*
pwt(GwyDWTFilter *wt,
    GwyDataLine *dline,
    gint n,
    GwyTransformDirection direction)
{
    double ai, ai1;
    long i, ii, jf, jr, k, n1, ni, nj, nh, nmod;
    gdouble *data, *wdata;

    g_return_val_if_fail(n >= 4, NULL);
    data = dline->data;
    wdata = wt->wksp->data;
    memset(wdata + 1, 0, n*sizeof(gdouble));

    nmod = wt->ncof*n;
    n1 = n-1;
    nh = n >> 1;
    switch (direction) {
        case GWY_TRANSFORM_DIRECTION_FORWARD:
        for (ii = 1, i = 1; i <= n ; i += 2, ii++) {
            ni = i + nmod + wt->ioff;
            nj = i + nmod + wt->joff;
            for (k = 1; k <= wt->ncof; k++) {
                jf = n1 & (ni+k);
                jr = n1 & (nj+k);
                wdata[ii] += wt->cc[k] * data[jf+1];
                wdata[ii+nh] += wt->cr[k] * data[jr+1];
            }
        }
        break;

        case GWY_TRANSFORM_DIRECTION_BACKWARD:
        for (ii = 1, i = 1; i <= n; i += 2, ii++) {
            ai = data[ii];
            ai1 = data[ii+nh];
            ni = i + nmod + wt->ioff;
            nj = i + nmod + wt->joff;
            for (k = 1; k <= wt->ncof; k++) {
                jf = (n1 & (ni+k)) + 1;
                jr = (n1 & (nj+k)) + 1;
                wdata[jf] += wt->cc[k]*ai;
                wdata[jr] += wt->cr[k]*ai1;
            }
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }

    memcpy(data + 1, wdata + 1, n*sizeof(gdouble));

    return dline;
}


static GwyDWTFilter*
wtset_new(GwyDataLine *wt_coefs)
{
    int i, k;
    gdouble sig = -1.0;
    GwyDWTFilter *wt;

    wt = g_new0(GwyDWTFilter, 1);
    wt->cc = g_new(gdouble, wt_coefs->res + 1);
    wt->cr = g_new(gdouble, wt_coefs->res + 1);
    wt->ncof = wt_coefs->res;

    for (i = 0; i < wt_coefs->res; i++)
        wt->cc[i+1] = wt_coefs->data[i]/G_SQRT2;

    for (k = 1; k <= wt_coefs->res; k++) {
        wt->cr[wt_coefs->res + 1 - k] = sig*wt->cc[k];
        sig = -sig;
    }

    /*FIXME none of the shifts centers wavelet well*/
    /*wt->ioff = wt->joff = -(wt_coefs->res >> 1);*/
    wt->ioff = 0;
    wt->joff = -wt->ncof;

    return wt;
}

static void
wtset_free(GwyDWTFilter *wt)
{
    gwy_object_unref(wt->wksp);
    g_free(wt->cc);
    g_free(wt->cr);
    g_free(wt);
}

/*universal thresholding with supplied threshold value*/
static gint
remove_by_universal_threshold(GwyDataField *dfield,
                              gint ulcol, gint ulrow, gint brcol, gint brrow,
                              gboolean hard, gdouble threshold)
{
    gdouble *datapos;
    gint i, j, count;

    count = 0;
    datapos = dfield->data + ulrow*dfield->xres + ulcol;
    for (i = 0; i < (brrow - ulrow); i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < (brcol - ulcol); j++) {
            if (fabs(*drow) < threshold) {
                if (hard)
                    *drow = 0;
                else {
                    if (*drow < 0)
                        *drow += threshold;
                    else
                        *drow -= threshold;
                }
                count++;
            }
            drow++;
        }
    }
    return count;
}

/*area adaptive thresholding*/
static gint
remove_by_adaptive_threshold(GwyDataField *dfield,
                             gint ulcol, gint ulrow, gint brcol, gint brrow,
                             gboolean hard,
                             gdouble multiple_threshold,
                             gdouble noise_variance)
{
    gdouble threshold, rms, min, max;
    gdouble *datapos;
    gint i, j, count;
    gint pbrcol, pbrrow, pulcol, pulrow;
    gint size = 12;

    count = 0;
    datapos = dfield->data + ulrow*dfield->xres + ulcol;
    for (i = 0; i < (brrow - ulrow); i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < (brcol - ulcol); j++) {

            pulcol = MAX(ulcol + j - size/2, ulcol);
            pulrow = MAX(ulrow + i - size/2, ulrow);
            pbrcol = MIN(ulcol + j + size/2, brcol);
            pbrrow = MIN(ulrow + i + size/2, brrow);

            rms = gwy_data_field_area_get_rms(dfield,
                                              pulcol, pulrow,
                                              pbrcol-pulcol, pbrrow-pulrow);
            if ((rms*rms - noise_variance*noise_variance) > 0) {
                rms = sqrt(rms*rms - noise_variance*noise_variance);
                threshold = noise_variance*noise_variance/rms;
                threshold *= multiple_threshold;
            }
            else {
                max = gwy_data_field_area_get_max(dfield,
                                                  pulcol, pulrow,
                                                  pbrcol-pulcol, pbrrow-pulrow);
                min = gwy_data_field_area_get_min(dfield,
                                                  pulcol, pulrow,
                                                  pbrcol-pulcol, pbrrow-pulrow);
                threshold = MAX(max, -min);
            }

            if (fabs(*drow) < threshold) {
                if (hard)
                    *drow = 0;
                else {
                    if (*drow < 0)
                        *drow += threshold;
                    else
                        *drow -= threshold;
                }
                count++;
            }
            drow++;
        }
    }
    return count;
}

/*scale adaptive thresholding*/
static gint
remove_by_threshold(GwyDataField *dfield,
                    gint ulcol, gint ulrow, gint brcol, gint brrow,
                    gboolean hard, gdouble multiple_threshold,
                    gdouble noise_variance)
{
    gdouble rms, threshold, min, max;
    gdouble *datapos;
    gint i, j, n, count;

    n = (brrow-ulrow)*(brcol-ulcol);

    rms = gwy_data_field_area_get_rms(dfield,
                                      ulcol, ulrow, brcol-ulcol, brrow-ulrow);
    if ((rms*rms - noise_variance*noise_variance) > 0)
    {
        rms = sqrt(rms*rms - noise_variance*noise_variance);
        threshold = noise_variance*noise_variance/rms;
        threshold *= multiple_threshold;
    }
    else
    {
        max = gwy_data_field_area_get_max(dfield,
                                          ulcol, ulrow,
                                          brcol-ulcol, brrow-ulrow);
        min = gwy_data_field_area_get_min(dfield,
                                          ulcol, ulrow,
                                          brcol-ulcol, brrow-ulrow);
        threshold = MAX(max, -min);
    }

    count = 0;
    datapos = dfield->data + ulrow*dfield->xres + ulcol;
    for (i = 0; i < (brrow - ulrow); i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < (brcol - ulcol); j++) {
            if (fabs(*drow) < threshold)
            {
                if (hard)
                    *drow = 0;
                else {
                    if (*drow < 0)
                        *drow += threshold;
                    else
                        *drow -=threshold;
                }
                count++;
            }
            drow++;
        }
    }
    return count;
}

static gint
find_anisotropy(GwyDataField *dfield,
                GwyDataField *mask,
                gint ul, gint br,
                gdouble threshold, gdouble setsize)
{
    gdouble *brpos, *trpos, *blpos, *brdrow, *trdrow, *bldrow;
    gdouble cor, mcor, scor, rms;
    gint i, j, count, mincol, minrow, maxcol, maxrow;

    count = 0;
    brpos = dfield->data + ul*dfield->xres + ul;
    trpos = dfield->data + ul;
    blpos = dfield->data + ul*dfield->xres;

    /*ratio between all field and its fraction*/

    cor = dfield->xres/(gdouble)(br-ul);
    mcor = MIN(cor, 30);
    scor = MAX(cor, mcor/3.5);

    rms = gwy_data_field_area_get_rms(dfield, ul, ul, br-ul, br-ul);

    for (i = 0; i < (br - ul); i++) {
        brdrow = brpos + i*dfield->xres;
        bldrow = blpos + i*dfield->xres;
        trdrow = trpos + i*dfield->xres;

        for (j = 0; j < (br - ul); j++) {
            if ((fabs(*bldrow) - fabs(*trdrow))>(rms/threshold))
            {
                /* note that we shift a little result neighbourhood.
                 * This is probably due to bad centering,
                   of scaling function, but it should be studied yet */
                mincol = MAX(j*cor - setsize/2.0, 0);
                maxcol = MIN(j*cor + mcor*setsize, mask->xres);
                minrow = MAX(i*cor - scor*setsize/2.0, 0);
                maxrow = MIN(i*cor + scor*setsize/2.0, mask->yres);
                count++;
                gwy_data_field_area_fill(mask, mincol, minrow, maxcol, maxrow,
                                         1.0);
            }
            else if ((fabs(*trdrow) - fabs(*bldrow)) > (rms/threshold))
            {
                /* note that we shift a little result neighbourhood.
                 * This is probably due to bad centering,
                   of scaling function, but it should be studied yet */
                mincol = MAX(j*cor - scor*setsize/2.0, 0);
                maxcol = MIN(j*cor + scor*setsize/2.0, mask->xres);
                minrow = MAX(i*cor - setsize/2.0, 0);
                maxrow = MIN(i*cor + mcor*setsize, mask->yres);
                count++;
                gwy_data_field_area_fill(mask, mincol, minrow, maxcol, maxrow,
                                         1.0);
            }

            *brdrow++;
            *bldrow++;
            *trdrow++;
        }
    }
    return count;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
