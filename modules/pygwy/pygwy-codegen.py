#!/usr/bin/python
import sys

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

class GArrayDouble(argtypes.ArgType):
   def write_return(self, ptype, ownsreturn, info):
      ptype = ptype.replace('*', '')
      info.varlist.add(ptype, '*ret')
      info.varlist.add("PyObject", '*py_ret')
      info.varlist.add("gint", 'i_ret')
      info.codeafter.append('    py_ret = PyList_New(ret->len);\n')
      info.codeafter.append('    for (i_ret = 0; i_ret < ret->len; i_ret++) {\n')
      info.codeafter.append('        PyList_SetItem(py_ret, i_ret, PyFloat_FromDouble(g_array_index(ret, gdouble, i_ret)));\n')
      info.codeafter.append('    }\n')
      info.codeafter.append('    g_array_free(ret, TRUE);\n')
      info.codeafter.append('    return py_ret;\n')

class GArrayInt(argtypes.ArgType):
   def write_return(self, ptype, ownsreturn, info):
      ptype = ptype.replace('*', '')
      info.varlist.add(ptype, '*ret')
      info.varlist.add("PyObject", '*py_ret')
      info.varlist.add("gint", 'i_ret')
      info.codeafter.append('    py_ret = PyList_New(ret->len);\n')
      info.codeafter.append('    for (i_ret = 0; i_ret < ret->len; i_ret++) {\n')
      info.codeafter.append('        PyList_SetItem(py_ret, i_ret, PyInt_FromLong(g_array_index(ret, gint, i_ret)));\n')
      info.codeafter.append('    }\n')
      info.codeafter.append('    g_array_free(ret, TRUE);\n')
      info.codeafter.append('    return py_ret;\n')


      
# Extend argtypes
arg = GArrayDouble()
argtypes.matcher.register('GArray*', arg)
del arg

arg = GArrayInt()
argtypes.matcher.register('GArrayInt*', arg)
del arg

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


#definitions.MethodDefBase.__init__ = method_def_base_constructor

# Run codegen
sys.exit(main(sys.argv))
