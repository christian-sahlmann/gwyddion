#line 1 "pygwy.c"
/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

/* Only one interpreter is created. After initialization of '__main__' 
 * and 'gwy' module the directory is copied every time the independent
 * pseudo-sub-interpreter is needed. So every plugin is called with 
 * own copy of main dictionary created by create_environment() function
 * and destroyed by destroy_environment() which deallocate created copy. 
 */

#define DEBUG 1

#include "config.h"

/* include this first, before NO_IMPORT_PYGOBJECT is defined */
#include <pygtk-2.0/pygobject.h>

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/datafield.h>
#include <libprocess/fractals.h>
#include <libgwymodule/gwymodule.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwyapp.h>
#include "pygwywrap.c"
#line 48 "pygwy.c"

typedef struct {
    gchar *name;
    gchar *filename;
    PyObject *code;
    time_t m_time;
} PygwyPluginInfo;

typedef enum {
    PYGWY_PROCESS, PYGWY_FILE, PYGWY_GRAPH, PYGWY_LAYER, PYGWY_UNDEFINED
} PygwyPluginType;

static gboolean         module_register       (void);
static void             pygwy_proc_run        (GwyContainer *data,
                                               GwyRunType run,
                                               const gchar *name);
static void             pygwy_register_plugins(void);
static PygwyPluginInfo* pygwy_find_plugin     (const gchar* name);
static gboolean         pygwy_file_save_run   (GwyContainer *data,
                                               const gchar *filename,
                                               GwyRunType mode,
                                               GError **error, 
                                               const gchar *name);
static GwyContainer*    pygwy_file_load_run   (const gchar *filename,
                                               G_GNUC_UNUSED GwyRunType mode,
                                               GError **error,
                                               const gchar *name);
static gint             pygwy_file_detect_run (const GwyFileDetectInfo
                                               *fileinfo, 
                                               gboolean only_name,
                                               gchar *name);

static GList *s_pygwy_plugins = NULL;
static PyObject *s_pygwy_dict;
static PyObject *s_main_module;

static const GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Pygwy, the Gwyddion Python wrapper."),
    "Jan Hořák <xhorak@gmail.com>",
    "0.1",
    "Jan Hořák",
    "2007"
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    pygwy_register_plugins();
    return TRUE;
}

static void
pygwy_initialize(void)
{
    PyObject *m;

    if (!Py_IsInitialized()) {
        gwy_debug("Initializing Python interpreter" );
        // Do not register signal handlers
        Py_InitializeEx(0);
        gwy_debug("Add main module");
        s_main_module = PyImport_AddModule("__main__");
        gwy_debug("Init pygobject");
        init_pygobject();

        gwy_debug("Init module gwy");
        m = Py_InitModule("gwy", (PyMethodDef*) pygwy_functions);
        gwy_debug("Get dict");
        s_pygwy_dict = PyModule_GetDict(m);

        gwy_debug("Register classes");
        pygwy_register_classes(s_pygwy_dict);
        gwy_debug("Register constaints");
        pygwy_add_constants(m, "GWY_");
     } else {
        gwy_debug("Python interpreter already initialized");
    }
}

static PyObject *
create_environment() {
    return PyDict_Copy(PyModule_GetDict(s_main_module));
}

static void
destroy_environment(PyObject *d) {
    PyDict_Clear(d);
    Py_DECREF(d);
}

static PyObject*
pygwy_run_string(char *cmd, int type, PyObject *g, PyObject *l) {
    PyObject *ret = PyRun_String(cmd, type, g, l);
    if (!ret) {
        PyErr_Print();
    }
    return ret;
}

static gchar*
pygwy_read_val_from_dict(PyObject *d, char *v, const gchar *f) 
{
    char *ret = NULL;
    PyObject *py_str;

    py_str = PyDict_GetItemString(d, v);
    if (py_str) {
        if (!PyArg_Parse(py_str, "s", &ret)) {
            ret = NULL;
        }
    }
    if (!ret) {
        g_warning("Value '%s' not found in '%s' dict.", v, f);
    } else {
        gwy_debug("Read value '%s' from '%s': %s", v, f, ret);
    }
    return ret;
}

static void 
pygwy_get_plugin_metadata(const gchar *filename, 
                          const gchar *module, 
                          PyObject **code,
                          gchar **name, 
                          gchar **desc,
                          gchar **menu_path,
                          PygwyPluginType *type)
{
    gchar *plugin_file_content, *type_str;
    PyObject *code_obj, *plugin_module, *d, *plugin_dict;
    GError *err = NULL;

    *code = NULL; *name = NULL; *menu_path = NULL; *type = PYGWY_UNDEFINED;

    if (!g_file_get_contents(filename, 
                            &plugin_file_content, 
                            NULL, 
                            &err)) {
        g_warning("Cannot read content of file '%s'", filename);
        return;
    }
    d = create_environment();
    if (!d) {
        g_warning("Cannot create copy of Python dictionary.");
        PyErr_Print();
        *code = NULL;
        return;
    }
    // compile file of given filename to module
    code_obj = Py_CompileString((char *)plugin_file_content, 
                                module, 
                                Py_file_input); // new ref
    if (!code_obj) {
        g_warning("Cannot compile plugin file '%s'", filename);
        PyErr_Print();
        goto error;
    }
    *code = code_obj;
    // Execute compiled module
    plugin_module = PyImport_ExecCodeModule("get_data", code_obj); // new ref
    if (!plugin_module) {
        g_warning("Cannot exec plugin code in file '%s'", filename);
        PyErr_Print();
        *code = NULL;
        goto error;
    }
    plugin_dict = PyModule_GetDict(plugin_module);
    // Get parameters from dict
    *name = pygwy_read_val_from_dict(plugin_dict, "plugin_name", filename);
    *desc = pygwy_read_val_from_dict(plugin_dict, "plugin_desc", filename);
    *menu_path = pygwy_read_val_from_dict(plugin_dict, "plugin_menu", filename);
    type_str = pygwy_read_val_from_dict(plugin_dict, "plugin_type", filename);
    // FIXME: move string somewhere else
    if (g_ascii_strcasecmp ("PROCESS", type_str) == 0) { 
        *type = PYGWY_PROCESS;
    } else if (g_ascii_strcasecmp ("FILE", type_str) == 0) {
        *type = PYGWY_FILE;
    } else if (g_ascii_strcasecmp ("GRAPH", type_str) == 0) {
        *type = PYGWY_GRAPH;
    } else {
        g_warning("Unknown type '%s' in '%s'", type_str, filename);
        *type = PYGWY_UNDEFINED;
    }

error:
    if (!(*code)) { 
        Py_XDECREF(code_obj);
    }
    Py_XDECREF(plugin_module);
    g_free(plugin_file_content);
    destroy_environment(d);
}

static PygwyPluginInfo*
pygwy_create_plugin_info(gchar *filename, gchar *name, PyObject *code)
{
    PygwyPluginInfo *info;
    struct stat file_stat;

    info = g_new(PygwyPluginInfo, 1);
    info->name = name;
    info->filename = filename;
    info->code = code;
    g_stat(filename, &file_stat);
    info->m_time = file_stat.st_mtime;
    return info;
}

static gboolean
pygwy_register_file_plugin(gchar *filename, 
                           PyObject *code, 
                           gchar *name, 
                           gchar*desc)
{
    PygwyPluginInfo *info;

    if (!code) {
        g_warning("Cannot create code object for file '%s'", filename);
        return FALSE;
    }
    if (!name) {
        g_warning("Cannot register. Undefined 'plugin_name' variable in '%s'", 
                  filename);
        return FALSE;
    }
    if (!desc) { 
        g_warning("Cannot register. Undefined 'plugin_desc' variable in '%s'", 
                  filename);
        return FALSE;
    }
    info = pygwy_create_plugin_info(filename, name, code);

    gwy_debug("Registering file func.");
    if (gwy_file_func_register(info->name,
                               desc,
                               (GwyFileDetectFunc)&pygwy_file_detect_run,
                               (GwyFileLoadFunc)&pygwy_file_load_run,
                               NULL,
                               (GwyFileSaveFunc)&pygwy_file_save_run)) {
        s_pygwy_plugins = g_list_append(s_pygwy_plugins, info);
    } else {
        g_free(info->name);
        g_free(info->filename);
        g_free(info);
        g_warning("Cannot register plugin '%s'", filename);
        return FALSE;
    }
    return TRUE;
}

static gboolean
pygwy_register_proc_plugin(gchar *filename, 
                           PyObject *code, 
                           gchar *name, 
                           gchar *menu_path)
{
    PygwyPluginInfo *info;

    if (!code) {
        g_warning("Cannot create code object for file '%s'", filename);
        return FALSE;
    }
    if (!name) {
        g_warning("Cannot register. Undefined 'plugin_name' variable in '%s'", 
                  filename);
        return FALSE;
    }
    if (!menu_path) { 
        g_warning("Cannot register. Undefined 'plugin_desc' variable in '%s'", 
                  filename);
        return FALSE;
    }
    info = pygwy_create_plugin_info(filename, name, code);

    gwy_debug("Registering proc func.");
    if (gwy_process_func_register(info->name,
                                  pygwy_proc_run,
                                  menu_path,
                                  NULL,
                                  GWY_RUN_IMMEDIATE,
                                  GWY_MENU_FLAG_DATA, // TODO: determine correct flag
                                  N_("Function written in Python")) ) { // not very descriptive
        // append plugin to list of plugins
        s_pygwy_plugins = g_list_append(s_pygwy_plugins, info);
    } else {
        g_free(info->name);
        g_free(info->filename);
        g_free(info);
        g_warning("Cannot register plugin '%s'", filename);
        return FALSE;
    }
    return TRUE;

}

static void
pygwy_register_plugins(void)
{
    // FIXME: maybe place somewhere else
    static const gchar pygwy_plugin_dir_name[] = "pygwy";
    GDir *plugin_dir;
    const gchar *plugin_filename;
    gchar *plugin_menu_path, *plugin_fullpath_filename;
    gchar *plugin_dir_name, *plugin_name, *plugin_desc;
    PygwyPluginType plugin_type = PYGWY_UNDEFINED;
    GError *err = NULL;
    PyObject *plugin_code;

    plugin_dir_name = g_build_filename(gwy_get_user_dir(), 
                                       pygwy_plugin_dir_name, 
                                       NULL);
    gwy_debug("Plugin path: %s", plugin_dir_name);

    plugin_dir = g_dir_open(plugin_dir_name, 0, &err);
    if (plugin_dir == NULL && err) {
        if (err->code == G_FILE_ERROR_NOENT) { 
            // directory not found/does not exist
            if (g_mkdir(plugin_dir_name, 0700)) {
                g_warning("Cannot create pygwy plugin directory %s", 
                          plugin_dir_name);
            } else {
                gwy_debug("Pygwy directory created: %s", plugin_dir_name);
            }
        } else {
            g_warning("Cannot open pygwy directory: %s, reason: %s", 
                      plugin_dir_name, 
                      err->message);
        }
        g_free(plugin_dir_name);
        /* Whenever the directory has been created or not, there is no reason
           to continue by reading scripts as long as no script is available */
        return;
    }
    // initialize python iterpret and init gwy module
    pygwy_initialize();
    // try to register each file with python extension in pygwy plugin directory
    while ((plugin_filename = g_dir_read_name(plugin_dir))) {
        if (g_str_has_suffix(plugin_filename, ".py")
           || g_str_has_suffix(plugin_filename, ".PY")
           || g_str_has_suffix(plugin_filename, ".Py") ) {
            // Read content of plugin file
            plugin_fullpath_filename = g_build_filename(plugin_dir_name, 
                                                        plugin_filename, 
                                                        NULL);
            // get plugin's metadata
            pygwy_get_plugin_metadata(plugin_fullpath_filename, 
                                      plugin_filename, 
                                      &plugin_code,
                                      &plugin_name,
                                      &plugin_desc,
                                      &plugin_menu_path,
                                      &plugin_type);
            printf("plugin_type: %d\n", plugin_type);
            switch(plugin_type) 
            {
                case PYGWY_PROCESS:
                    pygwy_register_proc_plugin(plugin_fullpath_filename,
                                               plugin_code,
                                               plugin_name,
                                               plugin_menu_path);
                    break;
                case PYGWY_FILE:
                    pygwy_register_file_plugin(plugin_fullpath_filename, 
                                               plugin_code, 
                                               plugin_name, 
                                               plugin_desc);
                    break;
                case PYGWY_UNDEFINED:
                    g_warning("Cannot register plugin without defined 'plugin_type' variable  ('%s')", 
                              plugin_fullpath_filename);
                    break;
            }
        } else { // if (check suffix)
            gwy_debug("wrong extension for file: %s", plugin_filename);
        }
    }
    g_dir_close(plugin_dir);
    g_free(plugin_dir_name);
}

static void
pygwy_reload_code(PygwyPluginInfo **info)
{
    struct stat file_stat;
    gchar *plugin_file_content;
    PyObject *code_obj;
    GError *err;

    gwy_debug("Reloading code from '%s'", (*info)->filename);
    if (!g_stat((*info)->filename, &file_stat)) {
        if (file_stat.st_mtime != (*info)->m_time) {
            gwy_debug("File '%s' has been changed. Re-reading file.", 
                      (*info)->filename);
            if (!g_file_get_contents((*info)->filename, 
                                    &plugin_file_content, 
                                    NULL, 
                                    &err)) {
                g_warning("Cannot read content of file '%s'", 
                          (*info)->filename);
            }
            code_obj = Py_CompileString((char *)plugin_file_content, 
                                        (*info)->name, 
                                        Py_file_input); // new ref
            if (!code_obj) {
                g_warning("Cannot create code object for file '%s'", 
                          (*info)->filename);
                PyErr_Print();
                return;
            }
            (*info)->code = code_obj; // XXX: override info->code without Py_DECREF is ok?
            (*info)->m_time = file_stat.st_mtime;
        } else {
            g_debug("No changes in '%s' since last run.", (*info)->filename);
        }
    } else {
        g_warning("Cannot get last modification time for file '%s'", 
                  (*info)->filename);
    }

}

static gboolean
pygwy_check_func(PyObject *m, gchar *name, gchar *filename) 
{
    gboolean ret;
    PyObject *func;

    if (!m) {
        g_warning("Undefined pygwy module == NULL ('%s')", filename);
        return FALSE;
    }
    func = PyDict_GetItemString(PyModule_GetDict(m), name);

    if (!func) {
        g_warning("Function '%s' not found in '%s'", name, filename);
        return FALSE;
    }

    if (!PyCallable_Check(func)) {
        g_warning("Function '%s' in '%s' is not defined.", name, filename);
        ret = FALSE;
    } else {
        ret = TRUE;
    }
    return ret;
}


static void
pygwy_proc_run(GwyContainer *data, GwyRunType run, const gchar *name)
{
    PygwyPluginInfo *info;
    PyObject *py_container, *module, *d;
    gchar *cmd;

    // find plugin
    if (!(info = pygwy_find_plugin(name))) {
        g_warning("Cannot find plugin '%s'.", name);
        return;
    }
    gwy_debug("Running plugin '%s', filename '%s'", 
              info->name, 
              info->filename);

    // create new environment   
    d = create_environment();
    if (!d) {
        g_warning("Cannot create copy of Python dictionary.");
        return;
    }

    // check last and current file modification time and override 
    // the code if required
    pygwy_reload_code(&info);
    gwy_debug("Import module and check for 'run' func");
    // import, execute the module and check for 'run' func
    module = PyImport_ExecCodeModule(info->name, info->code);
    if (!pygwy_check_func(module, "run", info->filename)) {
        destroy_environment(d);
        return;
    }

    gwy_debug("Running plugin '%s', filename '%s'", info->name, info->filename);
    // create container named 'data' to allow access the container from python
    py_container = pygobject_new((GObject*)data);
    if (!py_container) {
        g_warning("Variable 'gwy.data' was not inicialized.");
    }
    PyDict_SetItemString(s_pygwy_dict, "data", py_container);
    // import module using precompiled code and run its 'run()' function
    cmd = g_strdup_printf("import %s\n%s.run()", info->name, info->name);
    pygwy_run_string(cmd, Py_file_input, d, d);
    g_free(cmd);
    Py_DECREF(module);
    Py_DECREF(py_container); //FIXME
    destroy_environment(d);
}

static gboolean
pygwy_file_save_run(GwyContainer *data, 
                    const gchar *filename, 
                    GwyRunType mode, 
                    GError **error, 
                    const gchar *name)
{
    PyObject *py_container, *py_filename, *module, *py_res, *d;
    PygwyPluginInfo *info;
    gchar *cmd;
    gboolean res;

    // find plugin
    if (!(info = pygwy_find_plugin(name))) {
        g_warning("Cannot find plugin '%s'.", name);
        return FALSE;
    }
    gwy_debug("Running plugin '%s', filename '%s'", 
              info->name, 
              info->filename);
    // create new environment   
    d = create_environment();
    if (!d) {
        g_warning("Cannot create copy of Python dictionary.");
        return FALSE;
    }
    // check last and current file modification time and override 
    // the code if required
    pygwy_reload_code(&info);
    // import module using precompiled code and check for 'save()'
    module = PyImport_ExecCodeModule(info->name, info->code);
    // check if load function is defined
    if (!pygwy_check_func(module, "load", info->filename)) {
        destroy_environment(d);
        return FALSE;
    }
    // create input container and put it into __main__ module dictionary
    py_container = pygobject_new((GObject*)data);
    PyDict_SetItemString(d, "data", py_container);

    // create filename variable and put it into __main__ module dictionary
    py_filename = Py_BuildValue("s", filename);
    PyDict_SetItemString(d, "filename", py_filename);

    // import and execute the 'save' method
    cmd = g_strdup_printf("import %s\nresult = %s.save(data, filename)", 
                          info->name, 
                          info->name);
    pygwy_run_string(cmd, Py_file_input, d, d);
    g_free(cmd);
    // get result
    py_res = PyDict_GetItemString(d, "result");
    if (py_res && PyInt_Check(py_res) && PyInt_AsLong(py_res)) {
        res = TRUE;
    } else {
        // FIXME: show python traceback
        g_set_error(error, 
                    GWY_MODULE_FILE_ERROR, 
                    GWY_MODULE_FILE_ERROR_IO,
                    _("Pygwy plugin: %s (%s)\nExport failed."),
                     info->name, info->filename);
        res = FALSE;
    }
    Py_XDECREF(module);
    Py_XDECREF(py_container); //FIXME
    Py_XDECREF(py_filename);    
    destroy_environment(d);
    return res;
}

static GwyContainer*
pygwy_file_load_run(const gchar *filename,
                    G_GNUC_UNUSED GwyRunType mode,
                    GError **error, 
                    const gchar *name)
{
    GwyContainer *res = NULL;
    PyObject *o, *module, *type, *py_res, *d, *class_name;
    PyGObject *pyg_res;
    PygwyPluginInfo *info;
    gchar *cmd, *class_str;
    
    // find plugin
    if (!(info = pygwy_find_plugin(name))) {
        g_warning("Cannot find plugin '%s'.", name);
        return NULL;
    }
    gwy_debug("Running plugin '%s', filename '%s'", 
              info->name, 
              info->filename);

    // create new environment   
    d = create_environment();
    if (!d) {
        g_warning("Cannot create copy of Python dictionary.");
        goto error;
    }
    // check last and current file modification time and override 
    // the code if required
    pygwy_reload_code(&info);

    // import module using precompiled code and check for 'load()'
    module = PyImport_ExecCodeModule(info->name, info->code);
    // check if load function is defined
    if (!pygwy_check_func(module, "load", info->filename)) {
        goto error;
    }
    
    // create filename variable and put it into __main__ module dictionary
    o = Py_BuildValue("s", filename);
    if (!o)
        goto error;
    PyDict_SetItemString(d, "filename", o);
    cmd = g_strdup_printf("import %s\nresult = %s.load(\"test\")\nprint result",
                          info->name, 
                          info->name);
    pygwy_run_string(cmd, Py_file_input, d, d);
    g_free(cmd);
    py_res = PyDict_GetItemString(d, "result");
    if (!py_res)
        goto error;
    // check result's class
    type = PyObject_GetAttrString(py_res, "__class__");
    if (!type)
        goto error;
    class_name = PyObject_GetAttrString(type, "__name__");
    if (!class_name)
        goto error;
    class_str = PyString_AsString(class_name);
    if (!strcmp(class_str, "Container")) {
        pyg_res = (PyGObject *)PyDict_GetItemString(d, "result"); //XXX cast
        res = gwy_container_duplicate(GWY_CONTAINER(pyg_res->obj));
        //g_object_ref(res);
    }
error:
    Py_XDECREF(class_name);
    Py_XDECREF(type);
    Py_XDECREF(module);
    
    destroy_environment(d);
    gwy_debug("Return value %p", res);
    return res;
}

static gint
pygwy_file_detect_run(const GwyFileDetectInfo *fileinfo, 
                      gboolean only_name, 
                      gchar *name)
{
    PyObject *module, *py_res, *d, *o;
    PygwyPluginInfo *info;
    gchar *cmd;
    gboolean res;

    // find plugin
    if (!(info = pygwy_find_plugin(name))) {
        g_warning("Cannot find plugin '%s'.", name);
        return FALSE;
    }
    gwy_debug("Running plugin '%s', filename '%s'", 
              info->name, 
              info->filename);
    // create new environment   
    d = create_environment();
    if (!d) {
        g_warning("Cannot create copy of Python dictionary.");
        return FALSE;
    }
    // check last and current file modification time and override 
    // the code if required
    pygwy_reload_code(&info);
    // import module using precompiled code and check for 'detect_by_name()'
    // and 'detect_by_content()' functions
    module = PyImport_ExecCodeModule(info->name, info->code);
    // check if load function is defined
    if (!pygwy_check_func(module, "detect_by_name", info->filename)
       || !pygwy_check_func(module, "detect_by_content", info->filename)) {
        destroy_environment(d);
        return FALSE;
    }
    
    // create filename variable and put it into __main__ module dictionary
    if (only_name) {
        o = Py_BuildValue("s", fileinfo->name);
        PyDict_SetItemString(d, "filename", o);
    } else {
        o = Py_BuildValue("s", fileinfo->head);
        PyDict_SetItemString(d, "head", o);
        o = Py_BuildValue("s", fileinfo->tail);
        PyDict_SetItemString(d, "tail", o);
        o = Py_BuildValue("s", fileinfo->name);
        PyDict_SetItemString(d, "filename", o);
        o = Py_BuildValue("i", fileinfo->file_size);
        PyDict_SetItemString(d, "filesize", o);
        o = Py_BuildValue("i", fileinfo->buffer_len);
        PyDict_SetItemString(d, "buffer_len", o);
    }

    // import and execute the 'save' method
    if (only_name) {
        cmd = g_strdup_printf("import %s\nresult = %s.detect_by_name(filename)",
                              info->name, 
                              info->name);
    } else {
        cmd = g_strdup_printf("import %s\nresult = %s.detect_by_content(filename, head, tail, filesize)", 
                              info->name, 
                              info->name); 
    }
    pygwy_run_string(cmd, Py_file_input, d, d);
    g_free(cmd);
    // get result
    py_res = PyDict_GetItemString(d, "result");
    if (py_res && PyInt_Check(py_res) && PyInt_AsLong(py_res)) {
        res = PyInt_AsLong(py_res);
    } else {
        res = 0;
    }
    gwy_debug("Score for %s is %d (fileplugin %s)", 
              fileinfo->name, 
              res, 
              info->name);
    Py_DECREF(module);
    destroy_environment(d);
    return res;

}
 
static PygwyPluginInfo*
pygwy_find_plugin(const gchar* name)
{
    GList *l = s_pygwy_plugins;
    PygwyPluginInfo *info;

    while (l) {
        info = (PygwyPluginInfo*)(l->data);

        if (gwy_strequal(((PygwyPluginInfo*)(l->data))->name, name)) {
            break;
        }
        l = g_list_next(l);
    }
    if (!l) {
        g_warning("Cannot find record for Python plugin '%s'", name);
        return NULL;
    }
    return (PygwyPluginInfo*)l->data;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
