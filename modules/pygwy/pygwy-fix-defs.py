# alter defs, fix parameters and return types
import sys

extra_method_type = {}
extra_method_type['gwy_data_field_get_normal_coeffs'] = [['nx', 'ny', 'nz'], ['GDoubleValue', 'GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_correct_laplace_iteration'] = [['error'], ['GDoubleValue']] #TEST ...V
extra_method_type['gwy_data_line_get_normal_coeffs'] = [['av', 'bv'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_fractal_cubecounting_dim'] = [['a', 'b'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_fractal_triangulation_dim'] = [['a', 'b'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_fractal_partitioning_dim'] = [['a', 'b'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_fractal_psdf_dim'] = [['a', 'b'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_area_fit_plane'] = [['pa', 'pbx', 'pby'], ['GDoubleValue', 'GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_fit_plane'] = [['pa', 'pbx', 'pby'], ['GDoubleValue', 'GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_spectra_itoxy'] = [['x', 'y'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_get_min_max'] = [['min', 'max'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_spectra_itoxy'] = [['min', 'max'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_get_stats'] = [['avg', 'ra', 'rms', 'skew', 'kurtosis'], ['GDoubleValue', 'GDoubleValue', 'GDoubleValue', 'GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_area_get_stats'] = [['avg', 'ra', 'rms', 'skew', 'kurtosis'], ['GDoubleValue', 'GDoubleValue', 'GDoubleValue', 'GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_area_get_inclination'] = [['theta', 'phi'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_container_gis_double'] = [['value'], ['GDoubleValue']]
extra_method_type['gwy_3d_view_get_scale_range'] = [['min_scale', 'max_scale'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_axis_get_range'] = [['min', 'max'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_axis_get_requested_range'] = [['min', 'max'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_color_axis_get_range'] = [['min', 'max'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_view_coords_xy_to_real'] = [['xreal', 'yreal'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_view_get_real_data_sizes'] = [['xreal', 'yreal'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_graph_area_get_cursor'] = [['x_cursor', 'y_cursor'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_graph_curve_model_get_x_range'] = [['x_min', 'x_max'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_graph_curve_model_get_y_range'] = [['y_min', 'y_max'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_graph_curve_model_get_ranges'] = [['x_min', 'y_min', 'x_max', 'y_max'], ['GDoubleValue', 'GDoubleValue', 'GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_graph_model_get_x_range'] = [['x_min', 'x_max'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_graph_model_get_y_range'] = [['y_min', 'y_max'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_graph_model_get_ranges'] = [['x_min', 'y_min', 'x_max', 'y_max'], ['GDoubleValue', 'GDoubleValue', 'GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_layer_basic_get_range'] = [['min', 'max'], ['GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_ruler_get_range'] = [['lower', 'upper', 'position', 'max_size'], ['GDoubleValue', 'GDoubleValue', 'GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_ruler_get_range'] = [['lower', 'upper', 'position', 'max_size'], ['GDoubleValue', 'GDoubleValue', 'GDoubleValue', 'GDoubleValue']]
extra_method_type['gwy_data_field_circular_area_extract_with_pos'] = [ ['xpos', 'ypos'], ['GIntValue', 'GIntValue'] ]
extra_method_type['gwy_data_field_hough_polar_line_to_datafield'] = [ ['px1', 'px2', 'py1', 'py2'], ['GIntValue', 'GIntValue', 'GIntValue', 'GIntValue' ] ]
extra_method_type['gwy_data_field_area_count_in_range'] = [ ['nbelow', 'nabove'], ['GIntValue', 'GIntValue'] ]
extra_method_type['gwy_tip_estimate_partial'] = [ ['count'], ['GIntValue'] ]
extra_method_type['gwy_tip_estimate_full'] = [ ['count'], ['GIntValue'] ]
extra_method_type['gwy_container_gis_int32'] = [ ['value'], ['GIntValue'] ]
extra_method_type['gwy_container_gis_int64'] = [ ['value'], ['GIntValue'] ]
extra_method_type['gwy_math_humanize_numbers'] = [ ['value'], ['GIntValue'] ]
extra_method_type['gwy_si_unit_new_parse'] = [ ['power10'], ['GIntValue'] ]
extra_method_type['gwy_si_unit_set_from_string_parse'] = [ ['power10'], ['GIntValue'] ]
extra_method_type['gwy_data_chooser_get_active'] = [ ['id'], ['GIntValue'] ]
extra_method_type['gwy_gradient_get_samples'] = [ ['nsamples'], ['GIntValue'] ]
extra_method_type['gwy_gradient_get_points'] = [ ['npoints'], ['GIntValue'] ]
extra_method_type['gwy_file_detect_with_score'] = [ ['score'], ['GIntValue'] ]
extra_method_type['gwy_enum_combo_box_update_int'] = [ ['integer'], ['GIntValue'] ]
extra_method_type['gwy_data_view_coords_xy_clamp'] = [ ['xsrc', 'ysrc'], ['GIntValue', 'GIntValue'] ]
extra_method_type['gwy_data_view_coords_real_to_xy'] = [ ['xsrc', 'ysrc'], ['GIntValue', 'GIntValue'] ]
extra_method_type['gwy_data_view_get_pixel_data_sizes'] = [ ['xres', 'yres'], ['GIntValue', 'GIntValue'] ]
extra_method_type['gwy_container_gis_enum'] = [ ['value' ], ['GIntValue'] ]
extra_method_type['gwy_axis_get_major_ticks'] = [ ['nticks'], ['GIntValue'] ]
extra_method_type['gwy_graph_area_get_x_grid_data'] = [ ['ndata'], ['GIntValue'] ]
extra_method_type['gwy_graph_area_get_y_grid_data'] = [ ['ndata'], ['GIntValue'] ]
#extra_method_type['gwy_selection_get_data'] = [['data'],['GDoubleArrayToFill']]
extra_method_type['gwy_selection_get_data_wrap'] = [['len'],['GIntValue']]

extended_parameters = {}
#extended_parameters['gwy_interpolation_resolve_coeffs_1d'] = [['data'],['data|n']]
#extended_parameters['gwy_selection_get_data'] = [['data'],['data|ret']]


sys.path.insert(0, "/usr/share/pygtk/2.0/codegen")

import defsparser, definitions
parser = defsparser.DefsParser("pygwy.defs.tmp")
parser.startParsing()

for f in parser.functions:
   if f.name.endswith("_wrap"):
      f.name = f.name.replace("_wrap", "")
   #function name cannot start by number in Python
   if f.name[0] in ['0', '1','2', '3', '4', '5', '6', '7', '8', '9']:
      f.name = "a_"+f.name
   if extra_method_type.has_key(f.c_name):
      for p in f.params:
         for i in range(len(extra_method_type[f.c_name][0])):
            if p.pname == extra_method_type[f.c_name][0][i]:
               p.ptype = extra_method_type[f.c_name][1][i]
            if p.pname == "from":
               p.pname = "_from"
   if extended_parameters.has_key(f.c_name):
      for p in f.params:
         for i in range(len(extended_parameters[f.c_name][0])):
            if p.pname == extended_parameters[f.c_name][0][i]:
               p.pname = extended_parameters[f.c_name][1][i]
parser.write_defs()

