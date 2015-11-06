/*
 *  $Id$
 *  Copyright (C) 2008 Jan Horak, 2015 David Necas (Yeti)
 *  E-mail: xhorak@gmail.com, yeti@gwyddio.net.
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
 *
 *  Description: This file contains custom fuctions for automatically
 *  generated wrapping by using pygwy-codegen.
 */

#include "wrap_calls.h"
#include <libgwymodule/gwymodule-file.h>
#include <app/settings.h>
#include <libprocess/tip.h>

/* What is present on the exported image */
typedef enum {
    PIXMAP_NONE,
    PIXMAP_RULERS,
    PIXMAP_FMSCALE = PIXMAP_RULERS,
    PIXMAP_SCALEBAR
} PixmapOutput;


/* function-helper to short array creation */
static GArray*
create_array(gpointer data, guint len, guint type_size, gboolean free_data)
{
   GArray *ret;

   ret = g_array_new(FALSE, FALSE, type_size);
   g_array_append_vals(ret, data, len);
   if (free_data)
      g_free(data);
   return ret;
}

/**
 * gwy_data_field_get_profile_wrap:
 * @data_field: A data field.
 * @scol: The column the line starts at (inclusive).
 * @srow: The row the line starts at (inclusive).
 * @ecol: The column the line ends at (inclusive).
 * @erow: The row the line ends at (inclusive).
 * @res: Requested resolution of data line (the number of samples to take).
 *       If nonpositive, data line resolution is chosen to match @data_field's.
 * @thickness: Thickness of line to be averaged.
 * @interpolation: Interpolation type to use.
 *
 * Extracts a possibly averaged profile from data field to a data line.
 *
 * Returns: @data_line itself if it was not %NULL, otherwise a newly created
 *          data line.
 **/
GwyDataLine*
gwy_data_field_get_profile_wrap(GwyDataField *data_field,
                                gint scol,
                                gint srow,
                                gint ecol,
                                gint erow,
                                gint res,
                                gint thickness,
                                GwyInterpolationType interpolation)
{
    return gwy_data_field_get_profile(data_field, NULL, scol, srow, ecol, erow,
                                      res, thickness, interpolation);
}

/**
 * gwy_selection_get_data_wrap:
 *
 * Return list of selected points.
 *
 * Returns: a list of selected data
**/
GArrayDouble*
gwy_selection_get_data_wrap(GwySelection *selection)
{
    gdouble *data;
    gint len;

    len = gwy_selection_get_data(selection, NULL);
    len *= gwy_selection_get_object_size(selection);
    data = g_new(gdouble, len);
    gwy_selection_get_data(selection, data);

    return create_array(data, len, sizeof(gdouble), TRUE);
}

/**
 * gwy_data_field_fit_polynom_wrap:
 * @data_field: A data field.
 * @col_degree: Degree of polynomial to fit column-wise (x-coordinate).
 * @row_degree: Degree of polynomial to fit row-wise (y-coordinate).
 *
 * Fits a two-dimensional polynomial to a data field.
 *
 * Returns: a newly allocated array with coefficients.
 **/
GArrayDouble*
gwy_data_field_fit_polynom_wrap(GwyDataField *data_field,
                                gint col_degree,
                                gint row_degree)
{
    gdouble* coeffs;

    coeffs = gwy_data_field_fit_polynom(data_field, col_degree, row_degree, NULL);
    return create_array(coeffs, (col_degree+1)*(row_degree+1), sizeof(gdouble),
                        TRUE);
}

/**
 * gwy_data_field_area_fit_polynom_wrap:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @col_degree: Degree of polynomial to fit column-wise (x-coordinate).
 * @row_degree: Degree of polynomial to fit row-wise (y-coordinate).
 *
 * Fits a two-dimensional polynomial to a rectangular part of a data field.
 *
 * The coefficients are stored by row into @coeffs, like data in a datafield.
 * Row index is y-degree, column index is x-degree.
 *
 * Note naive x^n y^m polynomial fitting is numerically unstable, therefore
 * this method works only up to @col_degree = @row_degree = 6.
 *
 * Returns: a newly allocated array with coefficients.
 *
 **/
GArrayDouble*
gwy_data_field_area_fit_polynom_wrap(GwyDataField *data_field,
                                     gint col,
                                     gint row,
                                     gint width,
                                     gint height,
                                     gint col_degree, gint row_degree)
{
    gdouble *coeffs;

    coeffs = gwy_data_field_area_fit_polynom(data_field,
                                             col, row, width, height,
                                        col_degree, row_degree, NULL);
    return create_array(coeffs, (col_degree + 1) * (row_degree + 1),
                        sizeof(gdouble), TRUE);
}

/**
 * gwy_data_field_elliptic_area_extract_wrap:
 * @data_field: A data field.
 * @col: Upper-left bounding box column coordinate.
 * @row: Upper-left bounding box row coordinate.
 * @width: Bounding box width (number of columns).
 * @height: Bounding box height (number of rows).
 *
 * Extracts values from an elliptic region of a data field.
 *
 * The elliptic region is defined by its bounding box which must be completely
 * contained in the data field.
 *
 * Returns: The number of extracted values.
 **/
GArrayDouble*
gwy_data_field_elliptic_area_extract_wrap(GwyDataField *data_field,
                                          gint col,
                                          gint row,
                                          gint width,
                                          gint height)
{
   gdouble *data;
   guint n = gwy_data_field_get_elliptic_area_size(width, height);

   data = g_new(gdouble, n);
   gwy_data_field_elliptic_area_extract(data_field,
                                        col, row, width, height, data);

   return create_array(data, n, sizeof(gdouble), TRUE);
}

/**
 * gwy_data_field_circular_area_extract_wrap:
 * @data_field: A data field.
 * @col: Row index of circular area centre.
 * @row: Column index of circular area centre.
 * @radius: Circular area radius (in pixels).  See
 *          gwy_data_field_circular_area_extract_with_pos() for caveats.
 *
 * Extracts values from a circular region of a data field.
 *
 * Returns: Array of values.
 **/
GArrayDouble*
gwy_data_field_circular_area_extract_wrap(GwyDataField *data_field,
                                          gint col,
                                          gint row,
                                          gdouble radius)
{
    gdouble *data;

    data = g_new(gdouble, gwy_data_field_get_circular_area_size(radius));
    gwy_data_field_circular_area_extract(data_field, col, row, radius, data);

    return create_array(data, gwy_data_field_get_circular_area_size(radius),
                        sizeof(gdouble), TRUE);
}

/**
 * gwy_app_data_browser_get_data_ids_wrap:
 * @data: A data container.
 *
 * Gets the list of all channels in a data container.
 *
 * The container must be known to the data browser.
 *
 * Returns: A newly allocated array with channel ids.
 **/
GArrayInt*
gwy_app_data_browser_get_data_ids_wrap(GwyContainer *data)
{
    gint *ids, *id_p, c = 0;

    ids = gwy_app_data_browser_get_data_ids(data);
    id_p = ids;
    while (*(id_p++) != -1)
        c++;

    return create_array(ids, c, sizeof(gint), TRUE);
}

/**
 * gwy_get_key_from_name_wrap:
 * @name: string representation of key.
 *
 * Convert string representation of key to numerical.
 *
 * Returns: key value.
 **/
gint
gwy_get_key_from_name(const gchar *name)
{
    return g_quark_from_string(name);
}

GwyDataField*
gwy_tip_dilation_wrap(GwyDataField *tip, GwyDataField *surface)
{
    GwyDataField *r = gwy_data_field_new_alike(surface, FALSE);

    if (!gwy_tip_dilation(tip, surface, r, NULL, NULL)) {
        g_object_unref(r);
        return NULL;
    }
    return r;
}

GwyDataField*
gwy_tip_erosion_wrap(GwyDataField *tip, GwyDataField *surface)
{
    GwyDataField *r = gwy_data_field_new_alike(surface, FALSE);

    if (!gwy_tip_erosion(tip, surface, r, NULL, NULL)) {
        g_object_unref(r);
        return NULL;
    }
    return r;
}

GwyDataField*
gwy_tip_cmap_wrap(GwyDataField *tip, GwyDataField *surface)
{
    GwyDataField *r = gwy_data_field_new_alike(surface, FALSE);

    if (!gwy_tip_cmap(tip, surface, r, NULL, NULL)) {
        g_object_unref(r);
        return NULL;
    }
    return r;
}

GwyDataField*
gwy_tip_estimate_partial_wrap(GwyDataField *tip, GwyDataField *surface,
                              gdouble threshold, gboolean use_edges)
{
    gint v;

    return gwy_tip_estimate_partial(tip, surface, threshold, use_edges, &v,
                                    NULL, NULL);
}

GwyDataField*
gwy_tip_estimate_full_wrap(GwyDataField *tip, GwyDataField *surface,
                           gdouble threshold, gboolean use_edges)
{
    gint v;

    return gwy_tip_estimate_full(tip, surface, threshold, use_edges, &v,
                                 NULL, NULL);
}

GwyDataField*
gwy_data_field_create_full_mask(GwyDataField *d)
{
    GwyDataField *m;
    m = gwy_data_field_new_alike(d, TRUE);
    gwy_data_field_add(m, 1.0);
    return m;
}

gboolean
gwy_get_grain_quantity_needs_same_units_wrap(GwyGrainQuantity quantity)
{
    return gwy_grain_quantity_needs_same_units(quantity);
}

GwySIUnit*
gwy_construct_grain_quantity_units(GwyGrainQuantity quantity,
                                   GwySIUnit *siunitxy,
                                   GwySIUnit *siunitz)
{
    return gwy_grain_quantity_get_units(quantity, siunitxy, siunitz, NULL);
}

/**
 * gwy_data_field_number_grains_wrap:
 * @mask_field: A data field representing a mask.
 *
 * Constructs an array with grain numbers from a mask data field.
 *
 * Returns: An array of integers, containing 0 outside grains and the grain
 *          number inside a grain.
 **/
GArrayInt*
gwy_data_field_number_grains_wrap(GwyDataField *mask_field)
{
    gint xres = gwy_data_field_get_xres(mask_field);
    gint yres = gwy_data_field_get_yres(mask_field);
    gint *grains = g_new0(gint, xres*yres);
    gwy_data_field_number_grains(mask_field, grains);
    return create_array(grains, xres*yres, sizeof(gint), TRUE);
}

/**
 * gwy_data_field_number_grains_periodic_wrap:
 * @mask_field: A data field representing a mask.
 *
 * Constructs an array with grain numbers from a mask data field treated as
 * periodic.
 *
 * Returns: An array of integers, containing 0 outside grains and the grain
 *          number inside a grain.
 **/
GArrayInt*
gwy_data_field_number_grains_periodic_wrap(GwyDataField *mask_field)
{
    gint xres = gwy_data_field_get_xres(mask_field);
    gint yres = gwy_data_field_get_yres(mask_field);
    gint *grains = g_new0(gint, xres*yres);
    gwy_data_field_number_grains_periodic(mask_field, grains);
    return create_array(grains, xres*yres, sizeof(gint), TRUE);
}

static gint
find_ngrains(const GArrayInt *grains)
{
    gint ngrains = 0;
    const gint *g = (const gint*)grains->data;
    guint i, len = grains->len;

    for (i = 0; i < len; i++) {
        if (g[i] > ngrains)
            ngrains = g[i];
    }
    return ngrains;
}

/**
 * gwy_data_field_get_grain_bounding_boxes_wrap:
 * @data_field: A data field representing a mask.
 * @grains: Array of grain numbers.
 *
 * Finds bounding boxes of all grains in a mask data field.
 *
 * The array @grains must have the same number of elements as @mask_field.
 * Normally it is obtained from a function such as
 * gwy_data_field_number_grains().
 *
 * Returns: An array of quadruples of integers, each representing the bounding
 *          box of the corresponding grain (the zeroth quadrupe does not
 *          correspond to any as grain numbers start from 1).
 **/
GArrayInt*
gwy_data_field_get_grain_bounding_boxes_wrap(GwyDataField *data_field,
                                             const GArrayInt *grains)
{
    gint xres = gwy_data_field_get_xres(data_field);
    gint yres = gwy_data_field_get_yres(data_field);
    const gint *g = (const gint*)grains->data;
    GArrayInt *bboxes;
    gint ngrains, *bbdata;

    g_return_val_if_fail(grains->len == xres*yres, NULL);
    ngrains = find_ngrains(grains);
    bboxes = g_array_sized_new(FALSE, FALSE, sizeof(gint), 4*(ngrains + 1));
    g_array_set_size(bboxes, 4*(ngrains+1));
    bbdata = (gint*)bboxes->data;
    gwy_data_field_get_grain_bounding_boxes(data_field, ngrains, g, bbdata);
    return bboxes;
}

/**
 * gwy_data_field_grains_get_values_wrap:
 * @data_field: A data field representing a surface.
 * @grains: Array of grain numbers.
 * @quantity: The quantity to calculate, identified by GwyGrainQuantity.
 *
 * Finds a speficied quantity for all grains in a data field.
 *
 * The array @grains must have the same number of elements as @data_field.
 * Normally it is obtained from a function such as
 * gwy_data_field_number_grains() for the corresponding mask.
 *
 * Returns: An array of floating point values, each representing the requested
 *          quantity for corresponding grain (the zeroth item does not
 *          correspond to any as grain numbers start from 1).
 **/
GArrayDouble*
gwy_data_field_grains_get_values_wrap(GwyDataField *data_field,
                                      const GArrayInt *grains,
                                      GwyGrainQuantity quantity)
{
    gint xres = gwy_data_field_get_xres(data_field);
    gint yres = gwy_data_field_get_yres(data_field);
    const gint *g = (const gint*)grains->data;
    GArrayDouble *values;
    gint ngrains;
    gdouble *vdata;

    g_return_val_if_fail(grains->len == xres*yres, NULL);
    ngrains = find_ngrains(grains);
    values = g_array_sized_new(FALSE, FALSE, sizeof(gdouble), ngrains + 1);
    g_array_set_size(values, ngrains+1);
    vdata = (gdouble*)values->data;
    gwy_data_field_grains_get_values(data_field, vdata, ngrains, g, quantity);
    return values;
}

/**
 * gwy_data_field_grains_get_distribution_wrap:
 * @data_field: A data field representing a surface.
 * @grain_field: A data field representing the mask.  It must have the same
 *               dimensions as the data field.
 * @grains: Array of grain numbers.
 * @quantity: The quantity to calculate, identified by GwyGrainQuantity.
 * @nstats: The number of bins in the histogram.  Pass a non-positive value to
 *          determine the number of bins automatically.
 *
 * Calculates the distribution of a speficied grain quantity.
 *
 * The array @grains must have the same number of elements as @data_field.
 * Normally it is obtained from a function such as
 * gwy_data_field_number_grains() for the corresponding mask.
 *
 * Returns: The distribution as a data line.
 **/
GwyDataLine*
gwy_data_field_grains_get_distribution_wrap(GwyDataField *data_field,
                                            GwyDataField *grain_field,
                                            const GArrayInt *grains,
                                            GwyGrainQuantity quantity,
                                            gint nstats)
{
    gint xres = gwy_data_field_get_xres(data_field);
    gint yres = gwy_data_field_get_yres(data_field);
    const gint *g = (const gint*)grains->data;
    gint ngrains;

    g_return_val_if_fail(grains->len == xres*yres, NULL);
    g_return_val_if_fail(grain_field->xres == xres, NULL);
    g_return_val_if_fail(grain_field->yres == yres, NULL);
    ngrains = find_ngrains(grains);
    return gwy_data_field_grains_get_distribution(data_field, grain_field,
                                                  NULL, ngrains, g,
                                                  quantity, nstats);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
