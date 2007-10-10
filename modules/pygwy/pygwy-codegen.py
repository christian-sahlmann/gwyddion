#!/usr/bin/python
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

# Add codegen path to our import path
i = 1
codegendir = None
while i < len(sys.argv):
    arg = sys.argv[i]
    if arg == '--codegendir':
        del sys.argv[i]
        codegendir = sys.argv[i]
        del sys.argv[i]
    elif arg.startswith('--codegendir='):
        codegendir = arg.split('=', 2)[1]
        del sys.argv[i]
    else:
        i += 1
if codegendir:
    sys.path.insert(0, codegendir)
del codegendir

# Load it
from codegen import *


# New argument types:
class GwyRGBAArg(argtypes.ArgType):
   def write_param(self, ptype, pname, pdflt, pnull, info):
      info.varlist.add('GwyRGBA', pname)
      info.arglist.append(pname)
      info.add_parselist('[dddd]', ['&' + pname + '.r', '&' + pname + '.g','&' + pname + '.b' , '&' + pname + '.a'], [pname])
   def write_return(self, ptype, ownsreturn, info):
      info.varlist.add('GwyRGBA', 'ret');
      info.varlist.add('PyObject', '*color_tuple');
      info.codeafter.append('    color_tuple = PyTuple_New(4);\n')      
      info.codeafter.append('    PyTuple_SetItem(color_tuple, 0, PyFloat_FromDouble(ret.r));\n')
      info.codeafter.append('    PyTuple_SetItem(color_tuple, 1, PyFloat_FromDouble(ret.g));\n')
      info.codeafter.append('    PyTuple_SetItem(color_tuple, 2, PyFloat_FromDouble(ret.b));\n')
      info.codeafter.append('    PyTuple_SetItem(color_tuple, 3, PyFloat_FromDouble(ret.a));\n')
      info.codeafter.append('    return color_tuple;')

class GwyRGBAArgPtr(argtypes.ArgType):
   def write_param(self, ptype, pname, pdflt, pnull, info):
      # info.varlist.add('GwyRGBA', '*p_' + pname + ' = ' + pname)
      info.kwlist = ['"red"', '"green"', '"blue"' ]
      info.varlist.add('GwyRGBA', 's_' + pname)
      info.varlist.add('GwyRGBA', '*' + pname + ' = &s_' + pname)
      info.arglist.append(pname)
      info.add_parselist('dddd', ['&' + pname + '->r', '&' + pname + '->g', '&' + pname + '->b' , '&' + pname + '->a'], [pname])
      info.codebefore.append('    gwy_debug("color: %f %f %f %f", '+pname+'->r, '+pname+'->g, '+pname+'->b, '+pname+'->a);')
   def write_return(self, ptype, ownsreturn, info):
      info.varlist.add('GwyRGBA', '*ret');
      info.varlist.add('PyObject', '*color_tuple');
      info.codeafter.append('    color_tuple = PyTuple_New(4);\n')      
      info.codeafter.append('    PyTuple_SetItem(color_tuple, 0, PyFloat_FromDouble(ret->r));\n')
      info.codeafter.append('    PyTuple_SetItem(color_tuple, 1, PyFloat_FromDouble(ret->g));\n')
      info.codeafter.append('    PyTuple_SetItem(color_tuple, 2, PyFloat_FromDouble(ret->b));\n')
      info.codeafter.append('    PyTuple_SetItem(color_tuple, 3, PyFloat_FromDouble(ret->a));\n')
      info.codeafter.append('    return color_tuple;')

class GDoubleValue(argtypes.ArgType):
   def write_param(self, ptype, pname, pdflt, pnull, info):
      info.varlist.add(ptype, pname)
      info.varlist.add('PyObject', '*'+pname+'_pyobj')
      info.arglist.append('&'+pname)
      info.add_parselist("O", ['&'+pname+'_pyobj'], [pname])
      info.codebefore.append('    if (!PyFloat_Check('+pname+'_pyobj)) {\n')
      info.codebefore.append('        PyErr_SetString(PyExc_TypeError, "Parameter \''+pname+'\' must be a float variable");\n')
      info.codebefore.append('        return NULL;\n')
      info.codebefore.append('    }\n')
      info.codebefore.append('    '+pname+' = PyFloat_AsDouble('+pname+'_pyobj);\n')      
      info.codeafter.append('    ((PyFloatObject *) '+pname+'_pyobj)->ob_fval = '+pname+';\n')

class GIntValue(argtypes.ArgType):
   def write_param(self, ptype, pname, pdflt, pnull, info):
      info.varlist.add(ptype, pname)
      info.varlist.add('PyObject', '*'+pname+'_pyobj')
      info.arglist.append('&'+pname)
      info.add_parselist("O", ['&'+pname+'_pyobj'], [pname])
      info.codebefore.append('    if (!PyInt_Check('+pname+'_pyobj)) {\n')
      info.codebefore.append('        PyErr_SetString(PyExc_TypeError, "Parameter \''+pname+'\' must be an integer variable");\n')
      info.codebefore.append('        return NULL;\n')
      info.codebefore.append('    }\n')
      info.codebefore.append('    '+pname+' = (int) PyInt_AsLong('+pname+'_pyobj);\n')      
      info.codeafter.append('    ((PyIntObject *) '+pname+'_pyobj)->ob_ival = '+pname+';\n')

class GConstDoubleArray(argtypes.ArgType):
   def write_param(self, ptype, pname, pdflt, pnull, info):
      ptype = ptype.replace('*', '')
      ptype = ptype.replace('const-', '')
      info.varlist.add(ptype, '*'+pname);
      info.varlist.add('int', 'i_'+pname);
      info.varlist.add('PyObject', '*'+pname+'_pyobj')
      info.add_parselist("O", ['&'+pname+'_pyobj'], [pname])
      info.arglist.append(pname)
      info.codebefore.append('    if (!PyList_Check('+pname+'_pyobj)) {\n')
      info.codebefore.append('        PyErr_SetString(PyExc_TypeError, "Parameter \''+pname+'\' must be a list of floats");\n')
      info.codebefore.append('        return NULL;\n')
      info.codebefore.append('    }\n')
      info.codebefore.append('    '+pname+' =  g_malloc(sizeof('+ptype+')*PyList_Size('+pname+'_pyobj));\n')
      info.codebefore.append('    if ('+pname+' == NULL) {\n')
      info.codebefore.append('        return PyErr_NoMemory();\n')
      info.codebefore.append('    }\n')
      info.codebefore.append('    for (i_'+pname+' = 0; i_'+pname+' < PyList_Size('+pname+'_pyobj); i_'+pname+'++) {\n')
      info.codebefore.append('        if (!PyFloat_Check(PyList_GetItem('+pname+'_pyobj, i_'+pname+'))) {\n')
      info.codebefore.append('            g_free('+pname+');\n')
      info.codebefore.append('            PyErr_SetString(PyExc_TypeError, "Parameter \''+pname+'\' must be a list of floats");\n')
      info.codebefore.append('            return NULL;\n')
      info.codebefore.append('        }\n')
      info.codebefore.append('        '+pname+'[i_'+pname+'] = PyFloat_AsDouble(PyList_GetItem('+pname+'_pyobj, i_'+pname+'));\n')
      info.codebefore.append('    }\n')
      info.codeafter.append('    g_free('+pname+');\n')
   def write_return(self, ptype, ownsreturn, info):
      info.codeafter.append('    PyErr_SetString(PyExc_NotImplementedError, "Return type \''+ptype+'\' not supported");\n')
      ptype = ptype.replace('*', '')
      ptype = ptype.replace('const-', '')
      info.varlist.add(ptype, '*ret')
      info.codeafter.append('    return NULL;')

class GDoubleArray(argtypes.ArgType):
   def write_param(self, ptype, pname, pdflt, pnull, info):
      ptype = ptype.replace('*', '')
      info.varlist.add(ptype, '*'+pname);
      info.varlist.add('int', 'i_'+pname);
      info.varlist.add('PyObject', '*'+pname+'_pyobj')
      info.add_parselist("O", ['&'+pname+'_pyobj'], [pname])
      info.arglist.append(pname)
      info.codebefore.append('    if (!PyList_Check('+pname+'_pyobj)) {\n')
      info.codebefore.append('        PyErr_SetString(PyExc_TypeError, "Parameter \''+pname+'\' must be a list of floats");\n')
      info.codebefore.append('        return NULL;\n')
      info.codebefore.append('    }\n')
      info.codebefore.append('    '+pname+' =  g_malloc(sizeof('+ptype+')*PyList_Size('+pname+'_pyobj));\n')
      info.codebefore.append('    if ('+pname+' == NULL) {\n')
      info.codebefore.append('        return PyErr_NoMemory();\n')
      info.codebefore.append('    }\n')
      info.codebefore.append('    for (i_'+pname+' = 0; i_'+pname+' < PyList_Size('+pname+'_pyobj); i_'+pname+'++) {\n')
      info.codebefore.append('        if (!PyFloat_Check(PyList_GetItem('+pname+'_pyobj, i_'+pname+'))) {\n')
      info.codebefore.append('            g_free('+pname+');\n')
      info.codebefore.append('            PyErr_SetString(PyExc_TypeError, "Parameter \''+pname+'\' must be a list of floats");\n')
      info.codebefore.append('            return NULL;\n')
      info.codebefore.append('        }\n')
      info.codebefore.append('        '+pname+'[i_'+pname+'] = PyFloat_AsDouble(PyList_GetItem('+pname+'_pyobj, i_'+pname+'));\n')
      info.codebefore.append('    }\n')
      info.codeafter.append('    for (i_'+pname+' = 0; i_'+pname+' < PyList_Size('+pname+'_pyobj); i_'+pname+'++) {\n')
      info.codeafter.append('        ((PyFloatObject *) PyList_GetItem('+pname+'_pyobj, i_'+pname+'))->ob_fval = '+pname+'[i_'+pname+'];\n')
      info.codeafter.append('    }\n')
      info.codeafter.append('    g_free('+pname+');\n')
   def write_return(self, ptype, ownsreturn, info):
      info.codeafter.append('    PyErr_SetString(PyExc_NotImplementedError, "Return type \''+ptype+'\' not supported");\n')
      ptype = ptype.replace('*', '')
      ptype = ptype.replace('const-', '')
      info.varlist.add(ptype, '*ret')
      info.codeafter.append('    return NULL;')

class GIntArray(argtypes.ArgType):
   def write_param(self, ptype, pname, pdflt, pnull, info):
      ptype = ptype.replace('*', '')
      info.varlist.add(ptype, '*'+pname);
      info.varlist.add('int', 'i_'+pname);
      info.varlist.add('PyObject', '*'+pname+'_pyobj')
      info.add_parselist("O", ['&'+pname+'_pyobj'], [pname])
      info.arglist.append(pname)
      info.codebefore.append('    if (!PyList_Check('+pname+'_pyobj)) {\n')
      info.codebefore.append('        PyErr_SetString(PyExc_TypeError, "Parameter \''+pname+'\' must be a list of integers");\n')
      info.codebefore.append('        return NULL;\n')
      info.codebefore.append('    }\n')
      info.codebefore.append('    '+pname+' =  g_malloc(sizeof('+ptype+')*PyList_Size('+pname+'_pyobj));\n')
      info.codebefore.append('    if ('+pname+' == NULL) {\n')
      info.codebefore.append('        return PyErr_NoMemory();\n')
      info.codebefore.append('    }\n')
      info.codebefore.append('    for (i_'+pname+' = 0; i_'+pname+' < PyList_Size('+pname+'_pyobj); i_'+pname+'++) {\n')
      info.codebefore.append('        if (!PyInt_Check(PyList_GetItem('+pname+'_pyobj, i_'+pname+'))) {\n')
      info.codebefore.append('            g_free('+pname+');\n')
      info.codebefore.append('            PyErr_SetString(PyExc_TypeError, "Parameter \''+pname+'\' must be a list of integers");\n')
      info.codebefore.append('            return NULL;\n')
      info.codebefore.append('        }\n')
      info.codebefore.append('        '+pname+'[i_'+pname+'] = (int) PyInt_AsLong(PyList_GetItem('+pname+'_pyobj, i_'+pname+'));\n')
      info.codebefore.append('    }\n')
      info.codeafter.append('    for (i_'+pname+' = 0; i_'+pname+' < PyList_Size('+pname+'_pyobj); i_'+pname+'++) {\n')
      info.codeafter.append('        ((PyIntObject *) PyList_GetItem('+pname+'_pyobj, i_'+pname+'))->ob_ival = '+pname+'[i_'+pname+'];\n')
      info.codeafter.append('    }\n')
      info.codeafter.append('    g_free('+pname+');\n')
   def write_return(self, ptype, ownsreturn, info):
      info.codeafter.append('    PyErr_SetString(PyExc_NotImplementedError, "Return type \''+ptype+'\' not supported");\n')
      ptype = ptype.replace('*', '')
      ptype = ptype.replace('const-', '')
      info.varlist.add(ptype, '*ret')
      info.codeafter.append('    return NULL;')

# Extend argtypes
arg = argtypes.IntArg()
argtypes.matcher.register('GQuark', arg)
del arg

arg = GwyRGBAArg()
argtypes.matcher.register('GwyRGBA', arg)
del arg

arg = GwyRGBAArgPtr()
argtypes.matcher.register('GwyRGBA*', arg)
argtypes.matcher.register('const-GwyRGBA*', arg)
del arg

arg = GDoubleValue()
argtypes.matcher.register('GDoubleValue', arg)
del arg

arg = GIntValue()
argtypes.matcher.register('GIntValue', arg)
del arg

arg = GConstDoubleArray()
argtypes.matcher.register('const-gdouble*', arg)
del arg

arg = GIntArray()
argtypes.matcher.register('gint*', arg)
argtypes.matcher.register('guint*', arg)
del arg

arg = GDoubleArray()
argtypes.matcher.register('gdouble*', arg)
del arg

arg = argtypes.EnumArg("GtkOrientation", 'GTK_TYPE_ORIENTATION')
argtypes.matcher.register("GtkOrientation", arg)
del arg
arg = argtypes.EnumArg("GtkUpdateType", 'GTK_TYPE_UPDATE_TYPE')
argtypes.matcher.register("GtkUpdateType", arg)
del arg
arg = argtypes.EnumArg("GtkPositionType", 'GTK_TYPE_POSITION_TYPE')
argtypes.matcher.register("GtkPositionType", arg)
del arg

argtypes.matcher.register_object('GtkWidget', None, 'GTK_TYPE_WIDGET')
argtypes.matcher.register_object('GtkObject', None, 'GTK_TYPE_OBJECT')
argtypes.matcher.register_object('GtkListStore', None, 'GTK_TYPE_LIST_STORE')
argtypes.matcher.register_object('GtkWindow', None, 'GTK_TYPE_WINDOW')
argtypes.matcher.register_object('GtkTooltips', None, 'GTK_TYPE_TOOLTIPS')
argtypes.matcher.register_object('GtkComboBox', None, 'GTK_TYPE_COMBO_BOX')

argtypes.matcher.register_object('GdkGC', None, 'GDK_TYPE_GC')
argtypes.matcher.register_object('GdkGLConfig', None, 'GDK_TYPE_GL_CONFIG')
argtypes.matcher.register_object('GdkPixbuf', None, 'GDK_TYPE_PIXBUF')
argtypes.matcher.register_object('GdkDrawable', None, 'GDK_TYPE_DRAWABLE')
argtypes.matcher.register_object('GdkLineStyle', None, 'GDK_TYPE_LINE_STYLE')

# Modify parameter type when function or method is in extra_method_type
def create_parameter(c_name, ptype, pname, pdflt, pnull, pdir):
   if extra_method_type.has_key(c_name) and pname.split('|')[0] in extra_method_type[c_name][0]:
      ptype = extra_method_type[c_name][1][extra_method_type[c_name][0].index(pname)]
   return definitions.Parameter(ptype, pname, pdflt, pnull, pdir)

# overriden original method
def method_def_base_constructor(self, name, *args):
     dump = 0
     self.name = name
     self.ret = None
     self.caller_owns_return = None
     self.unblock_threads = None
     self.c_name = None
     self.typecode = None
     self.of_object = None
     self.params = [] # of form (type, name, default, nullok)
     self.varargs = 0
     self.deprecated = None
     for arg in definitions.get_valid_scheme_definitions(args):
         if arg[0] == 'of-object':
             self.of_object = arg[1]
         elif arg[0] == 'docstring':
             self.docstring = make_docstring(arg[1:])
         elif arg[0] == 'c-name':
             self.c_name = arg[1]
         elif arg[0] == 'gtype-id':
             self.typecode = arg[1]
         elif arg[0] == 'return-type':
             self.ret = arg[1]
             if extra_method_type.has_key(self.c_name) and 'return' in extra_method_type[self.c_name][0]:
                self.ret = extra_method_type[self.c_name][1][extra_method_type[self.c_name][0].index('return')]
         elif arg[0] == 'caller-owns-return':
             self.caller_owns_return = arg[1] in ('t', '#t')
         elif arg[0] == 'unblock-threads':
             self.unblock_threads = arg[1] in ('t', '#t')
         elif arg[0] == 'parameters':
             for parg in arg[1:]:
                 ptype = parg[0]
                 pname = parg[1]
                 pdflt = None
                 pnull = 0
                 pdir = None
                 for farg in parg[2:]:
                     assert isinstance(farg, tuple)
                     if farg[0] == 'default':
                         pdflt = farg[1]
                     elif farg[0] == 'null-ok':
                         pnull = 1
                     elif farg[0] == 'direction':
                         pdir = farg[1]
                 # add modified parameter according to create_parameter
                 self.params.append(create_parameter(self.c_name, ptype, pname, pdflt, pnull, pdir))
         elif arg[0] == 'varargs':
             self.varargs = arg[1] in ('t', '#t')
         elif arg[0] == 'deprecated':
             self.deprecated = arg[1]
         else:
             sys.stderr.write("Warning: %s argument unsupported.\n"
                              % (arg[0]))
             dump = 1
     if dump:
         self.write_defs(sys.stderr)

     if self.caller_owns_return is None and self.ret is not None:
         self.guess_return_value_ownership()

definitions.MethodDefBase.__init__ = method_def_base_constructor

# Run codegen
sys.exit(main(sys.argv))
