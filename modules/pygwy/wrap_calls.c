#include "wrap_calls.h"
#include <stdio.h>

/* function-helper to short array creation */
static GArray*
create_array(gpointer *data, guint len, guint type_size, gboolean free_data)
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
GwyDataLine* gwy_data_field_get_profile_wrap    (GwyDataField *data_field,
                                            gint scol,
                                            gint srow,
                                            gint ecol,
                                            gint erow,
                                            gint res,
                                            gint thickness,
                                            GwyInterpolationType interpolation)
{
   return gwy_data_field_get_profile(data_field, NULL, scol, srow, ecol, erow, res, thickness, interpolation);
}

/**
 * gwy_selection_get_data_wrap:
 *
 * Return list of selected points.
 *
 * Returns: a list of selected data
**/
GArray*
gwy_selection_get_data_wrap(GwySelection *selection) {
   gdouble *data;
   gint len;

   len = gwy_selection_get_data(selection, NULL);
   len *= gwy_selection_get_object_size(selection);
   data = g_malloc(sizeof(gdouble) * len);
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
GArray*
gwy_data_field_fit_polynom_wrap(GwyDataField *data_field,
                                gint col_degree,
                                gint row_degree) 
{
   gdouble* coeffs;

   coeffs = gwy_data_field_fit_polynom(data_field, col_degree, row_degree, NULL);
   return create_array(coeffs, (col_degree+1)*(row_degree+1), sizeof(gdouble), TRUE);
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
GArray*
gwy_data_field_area_fit_polynom_wrap(GwyDataField *data_field, 
      gint col, 
      gint row, 
      gint width, 
      gint height, 
      gint col_degree,
      gint row_degree
      )
{
   gdouble* coeffs;

   coeffs = gwy_data_field_area_fit_polynom(data_field, col, row, width, height, col_degree, row_degree, NULL);
   return create_array(coeffs, (col_degree+1)*(row_degree+1), sizeof(gdouble), TRUE);

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
GArray* 
gwy_data_field_elliptic_area_extract_wrap(
      GwyDataField *data_field,
      gint col,
      gint row,
      gint width,
      gint height)
{
   gdouble *data;

   data = g_malloc(sizeof(gdouble)*gwy_data_field_get_elliptic_area_size(width, height));
   gwy_data_field_elliptic_area_extract(data_field, col, row, width, height, data);

   return create_array(data, gwy_data_field_get_elliptic_area_size(width, height), sizeof(gdouble), TRUE);
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
GArray*
gwy_data_field_circular_area_extract_wrap(
      GwyDataField *data_field,
      gint col,
      gint row,
      gdouble radius)
{
   gdouble *data;
   
   data = g_malloc(sizeof(gdouble)*gwy_data_field_get_circular_area_size(radius));
   gwy_data_field_circular_area_extract(data_field, col, row, radius, data);

   return create_array(data, gwy_data_field_get_circular_area_size(radius), sizeof(gdouble), TRUE);
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

