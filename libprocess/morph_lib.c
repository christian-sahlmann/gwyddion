/*
   Library of mathematical morphology and tip estimation routines
   written by John Villarrubia, National Institute of Standards and
   Technology, an agency of the U.S. Department of Commerce,
   Gaithersburg, MD 20899, USA.

   The algorithms presented below are intended to be used for research
   purposes only and bear no warranty, either express or implied.
   Please note that within the United States, copyright protection,
   under Section 105 of the United States Code, Title 17, is not
   available for any work of the United States Government and/or for
   any works conceived by United States Government employees under this
   software release. User acknowledges that Villarrubia's actual work
   is in the public domain and is not subject to copyright. However, if
   User utilizes the aforementioned government-created algorithms in a
   manner which substantially alters User's work, User agrees to
   acknowledge this by reference to Villarrubia's papers, which are
   listed below.
*/

/* Technical and formal modification by Petr Klapetek and David Necas (Yeti),
 * 2004 to fit better in Gwyddion */


#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "morph_lib.h"


/*static members forward declaration*/
static gint
itip_estimate_iter(gint **image, gint im_xsiz, gint im_ysiz, gint tip_xsiz,
                   gint tip_ysiz, gint xc, gint yc, gint **tip0, gint thresh,
                   gboolean use_edges, GwySetFractionFunc set_fraction,
                   GwySetMessageFunc set_message);

static gint
useit(gint x, gint y, gint **image, gint sx, gint sy, gint delta);

static gint **
iopen(gint **image, gint im_xsiz, gint im_ysiz, gint **tip, gint tip_xsiz,
      gint tip_ysiz);

static gint
itip_estimate_point(gint ixp, gint jxp, gint **image,
                    gint im_xsiz, gint im_ysiz, gint tip_xsiz, gint tip_ysiz,
                    gint xc, gint yc, gint **tip0, gint thresh,
                    gboolean use_edges);

/*end of forward declarations*/


/**
 * _gwy_morph_lib_iallocmatrix:
 * @ysiz: rows number
 * @xsiz: columns number
 *
 *  Allocates an integer matrix of dimension [ysiz][xsiz] using an array
 *  of pointers to rows. ysiz is the number of rows. xsiz is the number
 *  of columns.
 *
 * Returns: alocated matrix
 **/
gint**
_gwy_morph_lib_iallocmatrix(gint ysiz, gint xsiz)
{
    gint **mptr;                /* points to allocated matrix */
    gint i;                     /* counter */

    /* Allocate pointers to rows */
    mptr = g_new(gint*, ysiz);

    /* Allocate rows */
    for (i = 0; i < ysiz; i++)
        mptr[i] = g_new(gint, xsiz);

    /* Done. Return result. */
    return mptr;
}

/**
 * _gwy_morph_lib_ifreematrix:
 * @mptr: pointer to matrix
 * @ysiz: number of rows
 *
 * Frees memory allocated with allocmatrix.
 **/
void
_gwy_morph_lib_ifreematrix(gint **mptr, gint ysiz)
{
    gint i;

    for (i = 0; i < ysiz; i++)
        g_free(mptr[i]);
    g_free(mptr);
}


/**
 * _gwy_morph_lib_ireflect:
 * @surface: integer array to be reflected.
 * @surf_xsiz: number of columns
 * @surf_ysiz: number of rows
 *
 * Perform reflection of integer array.
 *
 * Returns: reflected array
 **/
gint **
_gwy_morph_lib_ireflect(gint **surface, gint surf_xsiz, gint surf_ysiz)
{
    gint **result;
    gint i, j;                  /* index */

    /* create output array of appropriate size */
    result = _gwy_morph_lib_iallocmatrix(surf_ysiz, surf_xsiz);

    for (j = 0; j < surf_ysiz; j++) { /* Loop over all points in output array */
        for (i = 0; i < surf_xsiz; i++) {
            result[j][i] = -surface[surf_ysiz - 1 - j][surf_xsiz - 1 - i];
        }
    }
    return result;
}



/**
 * _gwy_morph_lib_idilation:
 * @surface: surface array
 * @surf_xsiz: number of columns
 * @surf_ysiz: number of rows
 * @tip: tip array
 * @tip_xsiz: number of columns
 * @tip_ysiz: number of rows
 * @xc: tip apex column coordinate
 * @yc: tip apex row coordinate
 * @set_fraction: function to output computation fraction (or NULL)
 * @set_message: function to output computation state message (or NULL)
 *
 * Performs dilation algorithm (for integer arrays).
 *
 * Returns: dilated data (newly allocated).
 **/
gint **
_gwy_morph_lib_idilation(gint **surface, gint surf_xsiz, gint surf_ysiz,
                         gint **tip, gint tip_xsiz, gint tip_ysiz,
                         gint xc, gint yc,
                         GwySetFractionFunc set_fraction,
                         GwySetMessageFunc set_message)
{
    gint **result;
    gint i, j, px, py;          /* index */
    gint max;
    gint pxmin, pxmax, pymin, pymax;    /* range of indices into tip */
    gint temp;

    /* create output array of appropriate size */
    result = _gwy_morph_lib_iallocmatrix(surf_ysiz, surf_xsiz);
    if (set_message)
        set_message(N_("Dilation"));
    for (j = 0; j < surf_ysiz; j++) { /* Loop over all points in output array */
        /* Compute allowed range of py. This may be different from
           the full range of the tip due to edge overlaps. */
        pymin = MAX(j - surf_ysiz + 1, -yc);
        pymax = MIN(tip_ysiz - yc - 1, j);
        for (i = 0; i < surf_xsiz; i++) {
            /* Compute allowed range of px. This may be different from
               the full range of the tip due to edge overlaps. */
            pxmin = MAX(i - surf_xsiz + 1, -xc);
            pxmax = MIN(tip_xsiz - xc - 1, i);
            max = surface[j - pymin][i - pxmin] + tip[pymin + yc][pxmin + xc];
            for (px = pxmin; px <= pxmax; px++) { /* Loop over points in tip */
                for (py = pymin; py <= pymax; py++) {
                    temp = surface[j - py][i - px] + tip[py + yc][px + xc];
                    max = MAX(temp, max);
                }
            }
            result[j][i] = max;
        }
        if (set_fraction && !set_fraction((gdouble)j/surf_ysiz))
            return result;
    }
    if (set_fraction != NULL)
        set_fraction(0.0);
    return result;
}

/**
 * _gwy_morph_lib_ierosion:
 * @surface: surface array
 * @surf_xsiz: number of columns
 * @surf_ysiz: number of rows
 * @tip: tip array
 * @tip_xsiz: number of columns
 * @tip_ysiz: number of rows
 * @xc: tip apex column coordinate
 * @yc: tip apex row coordinate
 * @set_fraction: function to output computation fraction (or NULL)
 * @set_message: function to output computation state message (or NULL)
 *
 * Performs erosion algorithm (for integer arrays).
 *
 * Returns: eroded data (newly allocated).
 **/
gint **
_gwy_morph_lib_ierosion(gint **image, gint im_xsiz, gint im_ysiz,
                        gint **tip, gint tip_xsiz, gint tip_ysiz,
                        gint xc, gint yc,
                        GwySetFractionFunc set_fraction,
                        GwySetMessageFunc set_message)
{
    gint **result;
    gint i, j, px, py;          /* index */
    gint min;
    gint pxmin, pxmax, pymin, pymax;    /* range of indices into tip */
    gint temp;

    /* create output array of appropriate size */
    result = _gwy_morph_lib_iallocmatrix(im_ysiz, im_xsiz);
    if (set_message)
        set_message(N_("Erosion"));
    for (j = 0; j < im_ysiz; j++) {   /* Loop over all points in output array */
        /* Compute allowed range of py. This may be different from
           the full range of the tip due to edge overlaps. */
        pymin = MAX(-j, -yc);
        pymax = MIN(tip_ysiz - yc, im_ysiz - j) - 1;
        for (i = 0; i < im_xsiz; i++) {
            /* Compute allowed range of px. This may be different from
               the full range of the tip due to edge overlaps. */
            pxmin = MAX(-xc, -i);
            pxmax = MIN(tip_xsiz - xc, im_xsiz - i) - 1;
            min = image[j + pymin][i + pxmin] - tip[pymin + yc][pxmin + xc];
            for (px = pxmin; px <= pxmax; px++) { /* Loop over points in tip */
                for (py = pymin; py <= pymax; py++) {
                    temp = image[j + py][i + px] - tip[py + yc][px + xc];
                    min = MIN(temp, min);
                }
            }
            result[j][i] = min;
        }
        if (set_fraction && !set_fraction((gdouble)j/im_ysiz))
            return result;
    }
    if (set_fraction)
        set_fraction(0.0);
    return result;
}

/**
 * _gwy_morph_lib_icmap:
 * @image: image array
 * @im_xsiz: number of columns
 * @im_ysiz: number of rows
 * @tip: tip array
 * @tip_xsiz: number of columns
 * @tip_ysiz: number of rows
 * @rsurf: eroded surface array
 * @xc: tip apex column coordinate
 * @yc: tip apex row coordinate
 * @set_fraction: function to output computation fraction (or NULL)
 * @set_message: function to output computation state message (or NULL)
 *
 * Performs the certainty map algorithm.
 *
 * Returns: certainty map (newly allocated).
 **/
gint **
_gwy_morph_lib_icmap(gint **image, gint im_xsiz, gint im_ysiz,
                     gint **tip, gint tip_xsiz, gint tip_ysiz, gint **rsurf,
                     gint xc, gint yc,
                     GwySetFractionFunc set_fraction,
                     GwySetMessageFunc set_message)
{
    gint **cmap;
    gint imx, imy, tpx, tpy;    /* index */
    gint tpxmin, tpxmax, tpymin, tpymax;
    gint count;
    gint rxc, ryc;              /* center coordinates of reflected tip */
    gint x = 0, y = 0;

    rxc = tip_xsiz - 1 - xc;
    ryc = tip_ysiz - 1 - yc;
    if (set_message)
        set_message(N_("Certainty map"));
    /* create output array of appropriate size */
    cmap = _gwy_morph_lib_iallocmatrix(im_ysiz, im_xsiz);
    for (imy = 0; imy < im_ysiz; imy++)
        for (imx = 0; imx < im_xsiz; imx++) {
            cmap[imy][imx] = 0;
        }

    /*
       Loop over all pixels in the interior of the image. We skip
       pixels near the edge. Since it is possible there are unseen
       touches over the edge, we must conservatively leave these cmap
       entries at 0.
     */
    for (imy = ryc; imy <= im_ysiz + ryc - tip_ysiz; imy++) {
        for (imx = rxc; imx <= im_xsiz + rxc - tip_xsiz; imx++) {
            tpxmin = MAX(0, rxc - imx);
            tpxmax = MIN(tip_xsiz - 1, im_xsiz - 1 + rxc - imx);
            tpymin = MAX(0, ryc - imy);
            tpymax = MIN(tip_ysiz - 1, im_ysiz - 1 + ryc - imy);
            count = 0;
            for (tpy = tpymin; tpy <= tpymax && count < 2; tpy++) {
                for (tpx = tpxmin; tpx <= tpxmax && count < 2; tpx++) {
                    if (image[imy][imx]-tip[tip_ysiz-1-tpy][tip_xsiz-1-tpx]
                        == rsurf[tpy+imy-ryc][tpx+imx-rxc])
                    {
                        count++;        /* increment count */
                        x = tpx + imx - rxc;    /* remember coordinates */
                        y = tpy + imy - ryc;
                    }
                }
            }
            if (count == 1)
                cmap[y][x] = 1; /* 1 contact = good recon */
        }
        if (set_fraction && !set_fraction((gdouble)imy/(im_ysiz+ryc-tip_ysiz)))
            return cmap;
    }
    if (set_fraction)
        set_fraction(0.0);
    return cmap;
}


/**
 * _gwy_morph_lib_dallocmatrix:
 * @ysiz: rows number
 * @xsiz: columns number
 *
 * Allocates a double matrix of dimension [ysiz][xsiz] using an array
 * of pointers to rows. ysiz is the number of rows. xsiz is the number
 * of columns.
 *
 * Returns: alocated matrix
 **/
gdouble**
_gwy_morph_lib_dallocmatrix(gint ysiz, gint xsiz)
{
    gdouble **mptr;                /* points to allocated matrix */
    gint i;                     /* counter */

    /* Allocate pointers to rows */
    mptr = g_new(gdouble*, ysiz);

    /* Allocate rows */
    for (i = 0; i < ysiz; i++)
        mptr[i] = g_new(gdouble, xsiz);

    /* Done. Return result. */
    return mptr;
}

/**
 * _gwy_morph_lib_dfreematrix:
 * @mptr: pointer to matrix
 * @ysiz: number of rows
 *
 * Frees memory allocated with dallocmatrix.
 **/
void
_gwy_morph_lib_dfreematrix(gdouble **mptr, gint ysiz)
{
    gint i;

    for (i = 0; i < ysiz; i++)
        g_free(mptr[i]);
    g_free(mptr);
}

/**
 * _gwy_morph_lib_dreflect:
 * @surface: double array to be reflected.
 * @surf_xsiz: number of columns
 * @surf_ysiz: number of rows
 *
 * Perform reflection of double array.
 *
 * Returns: reflected array
 **/
gdouble **
_gwy_morph_lib_dreflect(gdouble **surface, gint surf_xsiz, gint surf_ysiz)
{
    gdouble **result;
    gint i, j;                  /* index */

    /* create output array of appropriate size */
    result = _gwy_morph_lib_dallocmatrix(surf_ysiz, surf_xsiz);

    for (j = 0; j < surf_ysiz; j++) { /* Loop over all points in output array */
        for (i = 0; i < surf_xsiz; i++) {
            result[j][i] = -surface[surf_ysiz - 1 - j][surf_xsiz - 1 - i];
        }
    }
    return result;
}


/**
 * _gwy_morph_lib_ddilation:
 * @surface: surface array
 * @surf_xsiz: number of columns
 * @surf_ysiz: number of rows
 * @tip: tip array
 * @tip_xsiz: number of columns
 * @tip_ysiz: number of rows
 * @xc: tip apex column coordinate
 * @yc: tip apex row coordinate
 * @set_fraction: function to output computation fraction (or NULL)
 * @set_message: function to output computation state message (or NULL)
 *
 * Performs dilation algorithm (for double arrays).
 *
 * Returns: dilated data (newly allocated).
 **/
gdouble **
_gwy_morph_lib_ddilation(gdouble **surface, gint surf_xsiz, gint surf_ysiz,
                         gdouble **tip, gint tip_xsiz, gint tip_ysiz,
                         gint xc, gint yc,
                         GwySetFractionFunc set_fraction,
                         GwySetMessageFunc set_message)
{
    gdouble **result;
    gint i, j, px, py;          /* index */
    gdouble max;
    gint pxmin, pxmax, pymin, pymax;    /* range of indices into tip */
    gdouble temp;

    /* create output array of appropriate size */
    result = _gwy_morph_lib_dallocmatrix(surf_ysiz, surf_xsiz);
    if (set_message)
        set_message(N_("Dilation"));
    for (j = 0; j < surf_ysiz; j++) { /* Loop over all points in output array */
        /* Compute allowed range of py. This may be different from
           the full range of the tip due to edge overlaps. */
        pymin = MAX(j - surf_ysiz + 1, -yc);
        pymax = MIN(tip_ysiz - yc - 1, j);
        for (i = 0; i < surf_xsiz; i++) {
            /* Compute allowed range of px. This may be different from
               the full range of the tip due to edge overlaps. */
            pxmin = MAX(i - surf_xsiz + 1, -xc);
            pxmax = MIN(tip_xsiz - xc - 1, i);
            max = surface[j - pymin][i - pxmin] + tip[pymin + yc][pxmin + xc];
            for (px = pxmin; px <= pxmax; px++) { /* Loop over points in tip */
                for (py = pymin; py <= pymax; py++) {
                    temp = surface[j - py][i - px] + tip[py + yc][px + xc];
                    max = MAX(temp, max);
                }
            }
            result[j][i] = max;
        }
        if (set_fraction && !set_fraction((gdouble)j/surf_ysiz))
            return result;
    }
    if (set_fraction)
        set_fraction(0.0);
    return result;
}

/**
 * _gwy_morph_lib_derosion:
 * @surface: surface array
 * @surf_xsiz: number of columns
 * @surf_ysiz: number of rows
 * @tip: tip array
 * @tip_xsiz: number of columns
 * @tip_ysiz: number of rows
 * @xc: tip apex column coordinate
 * @yc: tip apex row coordinate
 * @set_fraction: function to output computation fraction (or NULL)
 * @set_message: function to output computation state message (or NULL)
 *
 * Performs erosion algorithm (for double arrays).
 *
 * Returns: eroded data (newly allocated).
 **/
gdouble **
_gwy_morph_lib_derosion(gdouble **image, gint im_xsiz, gint im_ysiz,
                        gdouble **tip, gint tip_xsiz, gint tip_ysiz,
                        gint xc, gint yc,
                        GwySetFractionFunc set_fraction,
                        GwySetMessageFunc set_message)
{
    gdouble **result;
    gint i, j, px, py;          /* index */
    gdouble min;
    gint pxmin, pxmax, pymin, pymax;    /* range of indices into tip */
    gdouble temp;

    /* create output array of appropriate size */
    result = _gwy_morph_lib_dallocmatrix(im_ysiz, im_xsiz);
    if (set_message)
        set_message(N_("Erosion"));
    for (j = 0; j < im_ysiz; j++) {   /* Loop over all points in output array */
        /* Compute allowed range of py. This may be different from
           the full range of the tip due to edge overlaps. */
        pymin = MAX(-j, -yc);
        pymax = MIN(tip_ysiz - yc, im_ysiz - j) - 1;
        for (i = 0; i < im_xsiz; i++) {
            /* Compute allowed range of px. This may be different from
               the full range of the tip due to edge overlaps. */
            pxmin = MAX(-xc, -i);
            pxmax = MIN(tip_xsiz - xc, im_xsiz - i) - 1;
            min = image[j + pymin][i + pxmin] - tip[pymin + yc][pxmin + xc];
            for (px = pxmin; px <= pxmax; px++) { /* Loop over points in tip */
                for (py = pymin; py <= pymax; py++) {
                    temp = image[j + py][i + px] - tip[py + yc][px + xc];
                    min = MIN(temp, min);
                }
            }
            result[j][i] = min;
        }
        if (set_fraction && !set_fraction((gdouble)j/im_ysiz))
            return result;
    }
    if (set_fraction)
        set_fraction(0.0);
    return result;
}


static gint **
iopen(gint **image, gint im_xsiz, gint im_ysiz, gint **tip,
      gint tip_xsiz, gint tip_ysiz)
{
    gint **result, **eros;

    eros = _gwy_morph_lib_ierosion(image, im_xsiz, im_ysiz, tip,
                                   tip_xsiz, tip_ysiz,
                                   tip_xsiz/2, tip_ysiz/2, NULL, NULL);
    result = _gwy_morph_lib_idilation(eros, im_xsiz, im_ysiz, tip,
                                      tip_xsiz, tip_ysiz,
                                      tip_xsiz/2, tip_ysiz/2, NULL, NULL);
    _gwy_morph_lib_ifreematrix(eros, im_ysiz);  /* free intermediate result */

    return result;
}

/**
 * _gwy_morph_lib_itip_estimate:
 * @image: surface data
 * @im_xsiz: number of columns
 * @im_ysiz: number of rows
 * @tip_xsiz: tip number of columns
 * @tip_ysiz: tip numbe rof rows
 * @xc: tip apex column coordinate
 * @yc: tip apex row coordinate
 * @tip0: tip data to be refined
 * @thresh: threshold
 * @use_edges: whether to use also image edges
 * @set_fraction: function to output computation fraction (or NULL)
 * @set_message: functon to output computation state message (or NULL)
 *
 * Performs tip estimation algorithm.
 **/
void
_gwy_morph_lib_itip_estimate(gint **image, gint im_xsiz, gint im_ysiz,
                             gint tip_xsiz, gint tip_ysiz, gint xc,
                             gint yc, gint **tip0,
                             gint thresh, gboolean use_edges,
                             GwySetFractionFunc set_fraction,
                             GwySetMessageFunc set_message)
{
    gint iter = 0;
    gint count = 1;
    gchar buffer[100];

    while (count) {
        iter++;
        g_snprintf(buffer, sizeof(buffer),
                   N_("Iterating estimate (iteration %d)"), iter);
        if (set_message)
            set_message(buffer);
        count = itip_estimate_iter(image, im_xsiz, im_ysiz,
                                   tip_xsiz, tip_ysiz, xc, yc, tip0, thresh,
                                   use_edges, set_fraction, set_message);
        g_snprintf(buffer, sizeof(buffer),
                   N_("%d image locations produced refinement"), count);
        if (set_message)
            set_message(buffer);
    }
}


static gint
itip_estimate_iter(gint **image, gint im_xsiz, gint im_ysiz, gint tip_xsiz,
                   gint tip_ysiz, gint xc, gint yc, gint **tip0, gint thresh,
                   gboolean use_edges, GwySetFractionFunc set_fraction,
                   GwySetMessageFunc set_message)
{
    gint ixp, jxp;           /* index into the image (x') */
    gint **open;
    gdouble fraction;

    gint count = 0;          /* counts places where tip estimate is improved */

    open = iopen(image, im_xsiz, im_ysiz, tip0, tip_xsiz, tip_ysiz);

    for (jxp = tip_ysiz - 1 - yc; jxp <= im_ysiz - 1 - yc; jxp++) {
         for (ixp = tip_xsiz - 1 - xc; ixp <= im_xsiz - 1 - xc; ixp++) {
            if (image[jxp][ixp] - open[jxp][ixp] > thresh)
                if (itip_estimate_point(ixp, jxp, image,
                                        im_xsiz, im_ysiz, tip_xsiz, tip_ysiz,
                                        xc, yc, tip0, thresh, use_edges))
                    count++;
                if (set_fraction) {
                    fraction = (gdouble)(jxp-(tip_ysiz - 1 - yc))
                               /(gdouble)((im_ysiz - 1 - yc)
                                          - (tip_ysiz - 1 - yc));
                    fraction = MAX(fraction, 0);
                    if (!set_fraction(fraction))
                        break;
                }
        }
    }

    _gwy_morph_lib_ifreematrix(open, im_ysiz);
    return count;
}

/**
 * _gwy_morph_lib_itip_estimate0:
 * @image: surface data
 * @im_xsiz: number of columns
 * @im_ysiz: number of rows
 * @tip_xsiz: tip number of columns
 * @tip_ysiz: tip numbe rof rows
 * @xc: tip apex column coordinate
 * @yc: tip apex row coordinate
 * @tip0: tip data to be refined
 * @thresh: threshold
 * @use_edges: whether to use also image edges
 * @set_fraction: function to output computation fraction (or NULL)
 * @set_message: functon to output computation state message (or NULL)
 *
 * Performs partial tip estimation algorithm.
 **/
void
_gwy_morph_lib_itip_estimate0(gint **image, gint im_xsiz, gint im_ysiz,
                              gint tip_xsiz, gint tip_ysiz,
                              gint xc, gint yc,
                              gint **tip0, gint thresh,
                              gboolean use_edges,
                              GwySetFractionFunc set_fraction,
                              GwySetMessageFunc set_message)
{
    gint i, j, n;
    gint arraysize;  /* size of array allocated to store list of image maxima */
    gint *x, *y;     /* point to coordinates of image maxima */
    gint iter = 0;
    gint count;
    gint delta;      /* defines what is meant by near neighborhood for purposes
                        of point selection. */
    gint maxcount = 20;
    gchar buffer[100];

    arraysize = 300;
    x = g_new(gint, arraysize);
    y = g_new(gint, arraysize);

    delta = MAX(MAX(tip_xsiz, tip_ysiz)/10, 1);

    if (set_message)
        set_message(N_("Searching for local maxima"));
    /* Create a list of coordinates to use */
    n = 0;                      /* Number of image maxima found so far */
    for (j = tip_ysiz - 1 - yc; j <= im_ysiz - 1 - yc; j++) {
        for (i = tip_xsiz - 1 - xc; i <= im_xsiz - 1 - xc; i++) {
            if (useit(i, j, image, im_xsiz, im_ysiz, delta)) {
                if (n == arraysize) {   /* need more room in temporary arrays */
                    arraysize *= 2;     /* increase array size by factor of 2 */
                    x = g_renew(gint, x, arraysize);
                    y = g_renew(gint, y, arraysize);
                }
                x[n] = i;       /* We found another one */
                y[n] = j;
                n++;
            }
        }
    }
    g_snprintf(buffer, sizeof(buffer),
               N_("Found %d internal local maxima"), n);
    if (set_message)
        set_message(buffer);

    /* Now refine tip at these coordinates recursively until no more change */
    do {
        count = 0;
        iter++;
        g_snprintf(buffer, sizeof(buffer),
                   N_("Iterating estimate (iteration %d)"), iter);
        if (set_message)
            set_message(buffer);

        for (i = 0; i < n; i++) {
            if (itip_estimate_point(x[i], y[i], image, im_xsiz, im_ysiz,
                                    tip_xsiz, tip_ysiz, xc, yc, tip0, thresh,
                                    use_edges))
                count++;
                if (set_fraction && !set_fraction((gdouble)i/(gdouble)n)) {
                    break;
                }
        }
        g_snprintf(buffer, sizeof(buffer),
                   N_("%d image locations produced refinement"), count);
        if (set_message)
            set_message(buffer);
    } while (count && count > maxcount);

    if (set_fraction)
        set_fraction(0.0);

    /* free temporary space */
    g_free(x);
    g_free(y);
}

/*
   The following is a routine that determines whether a selected point
   at coordintes x,y within an image is deemed to be suitable for
   image refinement. In this implementation, the algorithm simply
   decides to use the point if it is a local maximum of the image.
   It defines a local maximum as a point with height greater than any
   of its near neighbors, provided there are not too many near neighbors
   with values equal to the maximum (which indicates a flat).
*/
static gint
useit(gint x, gint y, gint **image, gint sx, gint sy, gint delta)
{
    gint xmin, xmax, ymin, ymax;        /* actual interval to search */
    gint i, j;
    gint max = image[y][x];    /* value of maximum height in the neighborhood */
    gint count = 0;             /* counts # spots where pixel = max value */

    xmin = MAX(x - delta, 0);
    xmax = MIN(x + delta, sx - 1);
    ymin = MAX(y - delta, 0);
    ymax = MIN(y + delta, sy - 1);

    for (j = ymin; j <= ymax; j++) {
        for (i = xmin; i <= xmax; i++) {
            max = image[j][i] >= max ? (count++, image[j][i]) : max;
        }
    }

    /* If the point equals the maximum value in the neighborhood we use it,
       unless there are too many points in the neighborhood with the same
       property--i.e. the neighborhood is flat */
    if (max == image[y][x] && count <= ((2 * delta + 1) ^ 2)/5)
        return 1;
    return 0;
}

/*
   The following routine does the same thing as itip_estimate_iter, except
   that instead of looping through all i,j contained within the image, it
   computes the tip shape as deduced from a single i,j coordinate. For what
   is this useful? The order of evaluation of the points can affect the
   execution speed. That is because the image at some locations puts great
   constraints on the tip shape. If the tip shape is refined by considering
   these points first, time is saved later. The following routine, since it
   performs the calculation at a single point, allows the user to select
   the order in which image coordinates are considered. In using the
   routine in this mode, the fact that the refined tip replaces tip0 means
   results of one step automatically become the starting point for the next
   step. The routine returns an integer which is the number of pixels
   within the starting tip estimate which were updated.

   To compile codes which does not use parts of the image within a tipsize
   of the edge, set USE_EDGES to 0 on the next line.
*/
static gint
itip_estimate_point(gint ixp, gint jxp, gint **image,
                    gint im_xsiz, gint im_ysiz, gint tip_xsiz, gint tip_ysiz,
                    gint xc, gint yc, gint **tip0, gint thresh,
                    gboolean use_edges)
{
    gint ix, jx,              /* index into the output tip array (x) */
      id, jd;                 /* index into p' (d) */
    gint temp, imagep, dil;   /* various intermediate results */
    gint count = 0;           /* counts places where tip estimate is improved */
    gint interior;            /* =1 if no edge effects to worry about */
    gint apexstate, xstate, inside = 1,  /* Point is inside image */
        outside = 0;          /* point is outside image */


    interior = jxp >= tip_ysiz - 1
               && jxp <= im_ysiz - tip_ysiz
               && ixp >= tip_xsiz - 1
               && ixp <= im_xsiz - tip_xsiz;

    if (interior) {
        for (jx = 0; jx < tip_ysiz; jx++) {
            for (ix = 0; ix < tip_xsiz; ix++) {
                /* First handle the large middle area where we don't have to
                   be concerned with edge problems. Because edges are far
                   away, we can leave out the overhead of checking for them
                   in this section. */
                imagep = image[jxp][ixp];
                dil = -G_MAXINT;        /* initialize maximum to -infinity */
                for (jd = 0; jd < tip_ysiz; jd++) {
                    for (id = 0; id < tip_xsiz; id++) {
                        if (imagep - image[jxp + yc - jd][ixp + xc - id]
                            > tip0[jd][id])
                            continue;
                        temp = image[jx + jxp - jd][ix + ixp - id]
                               + tip0[jd][id] - imagep;
                        dil = MAX(dil, temp);
                    }           /* end for id */
                }               /* end for jd */
                if (dil == -G_MAXINT)
                    continue;
                if (dil < tip0[jx][ix] - thresh) {
                    count++,
                    tip0[jx][ix] = dil+thresh;
                }
            }                   /* end for ix */
        }                       /* end for jx */
        return count;
    }                           /* endif */

    if (use_edges) {
        /* Now handle the edges */
        for (jx = 0; jx < tip_ysiz; jx++) {
            for (ix = 0; ix < tip_xsiz; ix++) {
                imagep = image[jxp][ixp];
                dil = -G_MAXINT;    /* initialize maximum to -infinity */
                for (jd = 0; jd <= tip_ysiz - 1 && dil < G_MAXINT; jd++) {
                    for (id = 0; id <= tip_xsiz - 1; id++) {
                        /* Determine whether the tip apex at (xc,yc) lies
                         * within the domain of the translated image, and
                         * if so, if it is inside (i.e. below or on the
                         * surface of) the image. */
                        apexstate = outside;        /* initialize */
                        if (jxp + yc - jd < 0
                            || jxp + yc - jd >= im_ysiz
                            || ixp + xc - id < 0
                            || ixp + xc - id >= im_xsiz)
                            apexstate = inside;
                        else if (imagep - image[jxp + yc - jd][ixp + xc - id]
                                 <= tip0[jd][id])
                            apexstate = inside;
                        /* Determine whether the point (ix,jx) under
                         * consideration lies within the domain of the
                         * translated image */
                        if (jxp + jx - jd < 0
                            || jxp + jx - jd >= im_ysiz
                            || ixp + ix - id < 0
                            || ixp + ix - id >= im_xsiz)
                            xstate = outside;
                        else
                            xstate = inside;

                        /* There are 3 actions we might take, depending upon
                           which of 4 states (2 apexstate possibilities times 2
                           xstate ones) we are in. */

                        /* If apex is outside and x is either in or out no
                         * change is made for this (id,jd) */
                        if (apexstate == outside)
                            continue;

                        /* If apex is inside and x is outside
                           worst case is translated image value -> G_MAXDOUBLE.
                           This would result in no change for ANY (id,jd).
                           We therefore abort the loop and go to next (ix,jx)
                           value */
                        if (xstate == outside)
                            goto nextx;

                        /* The only remaining possibility is x and apex both
                         * inside. This is the same case we treated in the
                         * interior. */
                        temp = image[jx + jxp - jd][ix + ixp - id]
                               + tip0[jd][id] - imagep;
                        dil = MAX(dil, temp);
                    }               /* end for id */
                }                   /* end for jd */
                if (dil == -G_MAXINT)
                    continue;

                tip0[jx][ix] = (dil < tip0[jx][ix] - thresh)
                                ? (count++, dil + thresh)
                                : tip0[jx][ix];
              nextx:;
            }                       /* end for ix */
        }                           /* end for jx */
    }

    return count;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

