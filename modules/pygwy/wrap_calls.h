#include <libprocess/gwyprocess.h>
#include <libdraw/gwyselection.h>
#include <libgwyddion/gwyddion.h>
#include <app/data-browser.h>


typedef GArray GArrayInt;

GwyDataLine* gwy_data_field_get_profile_wrap    (GwyDataField *data_field,
                                            gint scol,
                                            gint srow,
                                            gint ecol,
                                            gint erow,
                                            gint res,
                                            gint thickness,
                                            GwyInterpolationType interpolation);

GArray* gwy_selection_get_data_wrap(GwySelection *selection);
GArray* gwy_data_field_fit_polynom_wrap(GwyDataField *data_field, gint col_degree, gint row_degree);
GArray* gwy_data_field_area_fit_polynom_wrap(GwyDataField *data_field, gint col, gint row, gint width, gint height, 
      gint col_degree, gint row_degree);

GArray* gwy_data_field_elliptic_area_extract_wrap(
      GwyDataField *data_field,
      gint col,
      gint row,
      gint width,
      gint height);

GArray* gwy_data_field_circular_area_extract_wrap(
      GwyDataField *data_field,
      gint col,
      gint row,
      gdouble radius);

GArrayInt* gwy_app_data_browser_get_data_ids_wrap(GwyContainer *data);

