#!/usr/bin/python
# Runtime for pygtk module codegen.py and overriden functions for Gwyddion data types.
# Public domain

import sys
import re

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

# override function_wrapper to deal with output arguments
class RetTupleHandler():
    def __init__(self,handler,returns):
        self.returns=returns
        self.handler=handler

    def write_return(self,ptype, caller_owns_return, info):
        info.varlist.add("PyObject", '*py_tuple_ret')
        info.varlist.add("int", 'py_tuple_index')
        info.codebefore.append('\n');
        info.codeafter.append('\n');
        if ptype==None or ptype=="none":
            info.codebefore.append('    py_tuple_ret = PyTuple_New(%d);\n' % (self.returns))
            info.codebefore.append('    py_tuple_index = 0;\n')
        else:
            self.handler.write_return(ptype,caller_owns_return,info)
            info.varlist.add("PyObject", '*py_original_ret')
            info.codebefore.append('    py_tuple_ret = PyTuple_New(%d);\n' % (self.returns+1))
            info.codebefore.append('    py_tuple_index = 1;\n')
            for i in range(len(info.codeafter)):
                newc = re.sub(r'return\s+(?!NULL)(.*)',
                              r'py_original_ret = \1',info.codeafter[i])
                info.codeafter[i] = newc+'\n'

            info.codeafter.append('    PyTuple_SetItem(py_tuple_ret,0,py_original_ret);\n')

        info.codeafter.append('    return py_tuple_ret;\n')

def write_function_wrapper(self, function_obj, template,
                           handle_return=0, is_method=0, kwargs_needed=0,
                           substdict=None):
    '''This function is the guts of all functions that generate
    wrappers for functions, methods and constructors.'''

    if not substdict: substdict = {}

    info = argtypes.WrapperInfo()
    returns = 0

    substdict.setdefault('errorreturn', 'NULL')

    # for methods, we want the leading comma
    if is_method:
        info.arglist.append('')

    if function_obj.varargs:
        raise argtypes.ArgTypeNotFoundError("varargs functions not supported")

    for param in function_obj.params:
        if param.pdflt != None and '|' not in info.parsestr:
            info.add_parselist('|', [], [])
        handler = argtypes.matcher.get(param.ptype)
        if param.ptype.endswith("Value"):
          returns=returns+1
        handler.write_param(param.ptype, param.pname, param.pdflt,
                            param.pnull, info)

    substdict['setreturn'] = ''
    if handle_return:
        if function_obj.ret not in ('none', None):
            substdict['setreturn'] = 'ret = '
        handler = argtypes.matcher.get(function_obj.ret)
        if returns>0:
            handler = RetTupleHandler(handler,returns)
        handler.write_return(function_obj.ret,
                             function_obj.caller_owns_return, info)

    if function_obj.deprecated != None:
        deprecated = self.deprecated_tmpl % {
            'deprecationmsg': function_obj.deprecated,
            'errorreturn': substdict['errorreturn'] }
    else:
        deprecated = ''

    # if name isn't set, set it to function_obj.name
    substdict.setdefault('name', function_obj.name)

    if function_obj.unblock_threads:
        substdict['begin_allow_threads'] = 'pyg_begin_allow_threads;'
        substdict['end_allow_threads'] = 'pyg_end_allow_threads;'
    else:
        substdict['begin_allow_threads'] = ''
        substdict['end_allow_threads'] = ''

    if self.objinfo:
        substdict['typename'] = self.objinfo.c_name
    substdict.setdefault('cname',  function_obj.c_name)
    substdict['varlist'] = info.get_varlist()
    substdict['typecodes'] = info.parsestr
    substdict['parselist'] = info.get_parselist()
    substdict['arglist'] = info.get_arglist()
    substdict['codebefore'] = deprecated + (
        string.replace(info.get_codebefore(),
        'return NULL', 'return ' + substdict['errorreturn'])
        )
    substdict['codeafter'] = (
        string.replace(info.get_codeafter(),
                       'return NULL',
                       'return ' + substdict['errorreturn']))

    if info.parsestr or kwargs_needed:
        substdict['parseargs'] = self.parse_tmpl % substdict
        substdict['extraparams'] = ', PyObject *args, PyObject *kwargs'
        flags = 'METH_VARARGS|METH_KEYWORDS'

        # prepend the keyword list to the variable list
        substdict['varlist'] = info.get_kwlist() + substdict['varlist']
    else:
        substdict['parseargs'] = ''
        substdict['extraparams'] = ''
        flags = 'METH_NOARGS'

    return template % substdict, flags

Wrapper.write_function_wrapper=write_function_wrapper

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
      info.arglist.append('&'+pname)
      info.codebefore.append('    '+pname+' = 0;\n')
      info.codeafter.append('    PyTuple_SetItem(py_tuple_ret,py_tuple_index++,PyFloat_FromDouble('+pname+'));\n')

class GIntValue(argtypes.ArgType):
   def write_param(self, ptype, pname, pdflt, pnull, info):
      info.varlist.add(ptype, pname)
      info.arglist.append('&'+pname)
      info.codebefore.append('    '+pname+' = 0;\n')
      info.codeafter.append('    PyTuple_SetItem(py_tuple_ret,py_tuple_index++,PyInt_FromLong('+pname+'));\n')

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

class StringPassArg(argtypes.ArgType):
    def write_param(self, ptype, pname, pdflt, pnull, info):
        if pdflt != None:
            if pdflt != 'NULL': pdflt = '"' + pdflt + '"'
            info.varlist.add('char', '*' + pname + ' = ' + pdflt)
        else:
            info.varlist.add('char', '*' + pname)
        info.arglist.append(pname+'_dup')
        if pnull:
            info.add_parselist('z', ['&' + pname], [pname])
        else:
            info.add_parselist('s', ['&' + pname], [pname])
        info.varlist.add('char', '*' + pname+"_dup")
        info.codebefore.append('    '+pname+'_dup = g_strdup('+pname+');\n')

    def write_return(self, ptype, ownsreturn, info):
        if ownsreturn:
            # have to free result ...
            info.varlist.add('gchar', '*ret')
            info.codeafter.append('    if (ret) {\n' +
                                  '        PyObject *py_ret = PyString_FromString(ret);\n' +
                                  '        g_free(ret);\n' +
                                  '        return py_ret;\n' +
                                  '    }\n' +
                                  '    Py_INCREF(Py_None);\n' +
                                  '    return Py_None;')
        else:
            info.varlist.add('const gchar', '*ret')
            info.codeafter.append('    if (ret)\n' +
                                  '        return PyString_FromString(ret);\n'+
                                  '    Py_INCREF(Py_None);\n' +
                                  '    return Py_None;')



      
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

arg = argtypes.StringArg()
argtypes.matcher.register('keep_gchar*', arg)
del arg

arg = StringPassArg()
argtypes.matcher.register('pass_owner_gchar*', arg)
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
