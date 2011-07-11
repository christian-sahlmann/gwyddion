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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* Only one interpreter is created. After initialization of '__main__'
 * and 'gwy' module the directory is copied every time the independent
 * pseudo-sub-interpreter is needed. So every plugin is called with
 * own copy of main dictionary created by create_environment() function
 * and destroyed by destroy_environment() which deallocate created copy.
 */
#include "config.h"


/* include this first, before NO_IMPORT_PYGOBJECT is defined */
#include <pygtk-2.0/pygobject.h>
#include <app/gwyapp.h>
#include "wrap_calls.h"
#include <glib/gstdio.h>
#include "pygwy.h"


//static GValue*    convert_pyobject_to_gvalue         (PyObject *o);
//static PyObject*  convert_gvalue_to_pyobject         (GValue *value);
static void       pygwy_create_py_list_of_containers (GwyContainer *data, gpointer list);
#include "pygwywrap.c"
#include "pygwy-console.h"
#line 43 "pygwy.c"

typedef struct {
    gchar *name;
    gchar *filename;
    PyObject *code;
    time_t m_time;
} PygwyPluginInfo;


typedef enum {
    PYGWY_PROCESS, PYGWY_FILE, PYGWY_GRAPH, PYGWY_LAYER, PYGWY_TOOL, PYGWY_UNDEFINED
} PygwyPluginType;

const gchar pygwy_plugin_dir_name[] = "pygwy";

static gboolean         module_register       (void);
static void             pygwy_proc_run        (GwyContainer *data,
                                               GwyRunType run,
                                               const gchar *name);
static void             pygwy_graph_run       (GwyGraph *graph,
                                               const gchar *name);
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
static void             pygwy_register_plugins(void);

typedef struct {
   GList *plugins;
   PyObject *dict;
   PyObject *main_module;
} PygwyModuleInfo;

static PygwyModuleInfo s_pygwy;

static GwyModuleInfo module_info = {
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
    pygwy_register_console();
    return TRUE;
}


void
pygwy_initialize(void)
{
    PyObject *m;

    if (!Py_IsInitialized()) {
        gwy_debug("Initializing Python interpreter" );
        // Do not register signal handlers
        Py_InitializeEx(0);
        gwy_debug("Add main module");
        s_pygwy.main_module = PyImport_AddModule("__main__");
        gwy_debug("Init pygobject");
        init_pygobject();

        gwy_debug("Init module gwy");
        m = Py_InitModule("gwy", (PyMethodDef*) pygwy_functions);
        gwy_debug("Get dict");
        s_pygwy.dict = PyModule_GetDict(m);

        gwy_debug("Register classes");
        pygwy_register_classes(s_pygwy.dict);
        gwy_debug("Register constaints");
        pygwy_add_constants(m, "GWY_");
     } else {
        gwy_debug("Python interpreter already initialized");
    }
}

PyObject*
pygwy_run_string(const char *cmd, int type, PyObject *g, PyObject *l) {
    PyObject *ret = PyRun_String(cmd, type, g, l);
    if (!ret) {
        PyErr_Print();
    }
    return ret;
}

static void
pygwy_show_stderr(gchar *str)
{
    GtkWidget *dlg, *scroll, *frame, *b_close, *text;

    dlg = gtk_dialog_new();
    gtk_window_set_default_size(GTK_WINDOW(dlg), 600, 350);
    gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_title(GTK_WINDOW(dlg), "Python interpreter result");

    frame = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), frame, TRUE, TRUE, 0);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(frame), scroll);

    text = gtk_text_view_new();
    gtk_container_add(GTK_CONTAINER(scroll), text);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)),
                             str,
                             -1);

    b_close = gtk_button_new_from_stock("gtk-close");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), b_close, 0);

    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void
pygwy_initialize_stderr_redirect(PyObject *d)
{
    // redirect stderr to temporary file
    pygwy_run_string("import sys, tempfile\n"
                     "_stderr_redir = tempfile.TemporaryFile()\n"
                     "sys.stderr = _stderr_redir\n",
                     //"sys.stdout = _stderr_redir",
                     Py_file_input,
                     d,
                     d);
}

static void
pygwy_finalize_stderr_redirect(PyObject *d)
{
    PyObject *py_stderr;
    gchar *buf;
    // rewind redirected stderr file, read its content and display it in error window
    pygwy_run_string("_stderr_redir.seek(0)\n"
                     "_stderr_str = _stderr_redir.read()\n"
                     "_stderr_redir.close()",
                     Py_file_input,
                     d,
                     d);
    py_stderr = PyDict_GetItemString(d, "_stderr_str");
    if (py_stderr && PyString_Check(py_stderr)) {
        buf = PyString_AsString(py_stderr);
        gwy_debug("Pygwy plugin stderr output:\n%s", buf);
        if (buf[0] != '\0') // show stderr only when it is not empty string
            pygwy_show_stderr(buf);
    }
}

PyObject *
create_environment(const gchar *filename, gboolean show_errors) {
    PyObject *d, *plugin_filename;
    char *argv[1];
    argv[0] = NULL;

    d = PyDict_Copy(PyModule_GetDict(s_pygwy.main_module));
    // set __file__ variable for clearer error reporting
    plugin_filename = Py_BuildValue("s", filename);
    PyDict_SetItemString(d, "__file__", plugin_filename);
    PySys_SetArgv(0, argv);

    // redirect stderr and stdout of python script to temporary file
    if (show_errors)
      pygwy_initialize_stderr_redirect(d);
    return d;
}

void
destroy_environment(PyObject *d, gboolean show_errors) {
    // show content of temporary file which contains stderr and stdout of python
    // script and close it
    if (show_errors)
      pygwy_finalize_stderr_redirect(d);
    PyDict_Clear(d);
    Py_DECREF(d);
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
    gchar *plugin_file_content, *type_str, *suffix;
    PyObject *plugin_module = NULL, *d, *plugin_dict;
    GError *err = NULL;
    gboolean error = FALSE;

    *code = NULL; *name = NULL; *menu_path = NULL; *type = PYGWY_UNDEFINED;

    if (!g_file_get_contents(filename,
                            &plugin_file_content,
                            NULL,
                            &err)) {
        g_warning("Cannot read content of file '%s'", filename);
        return;
    }
    d = create_environment(filename, TRUE);
    // clear read values

    if (!d) {
        g_warning("Cannot create copy of Python dictionary.");
        PyErr_Print();
        error = TRUE;
        goto error;
    }
    // compile file of given filename to module
    *code = Py_CompileString((char *)plugin_file_content,
                                module,
                                Py_file_input); // new ref
    if (!(*code)) {
        g_warning("Cannot compile plugin file '%s'", filename);
        PyErr_Print();
        error = TRUE;
        goto error;
    }
    // Execute compiled module
    plugin_module = PyImport_ExecCodeModule("get_data", *code); // new ref
    if (!plugin_module) {
        g_warning("Cannot exec plugin code in file '%s'", filename);
        PyErr_Print();
        error = TRUE;
        goto error;
    }
    plugin_dict = PyModule_GetDict(plugin_module);
    // Get parameters from dict
    *name = g_path_get_basename(filename);
    suffix = g_strrstr(*name, ".");
    suffix[0] = '\0';
    gwy_debug("plugin name: %s", *name);
    *desc = pygwy_read_val_from_dict(plugin_dict, "plugin_desc", filename);
    *menu_path = pygwy_read_val_from_dict(plugin_dict, "plugin_menu", filename);
    type_str = pygwy_read_val_from_dict(plugin_dict, "plugin_type", filename);
    gwy_debug("read values: %s %s %s %p %p %p", *desc, *menu_path, type_str, *desc, *menu_path, type_str);
    if (!type_str) {
       g_warning("Undefined plugin type, cannot load.");
       *type = PYGWY_UNDEFINED;
       error = TRUE;
       goto error;
    }
    // FIXME: move string somewhere else
    if (g_ascii_strcasecmp ("PROCESS", type_str) == 0) {
        *type = PYGWY_PROCESS;
    } else if (g_ascii_strcasecmp ("FILE", type_str) == 0) {
        *type = PYGWY_FILE;
    } else if (g_ascii_strcasecmp ("GRAPH", type_str) == 0) {
        *type = PYGWY_GRAPH;
    } else if (g_ascii_strcasecmp ("LAYER", type_str) == 0) {
        *type = PYGWY_LAYER;
    } else if (g_ascii_strcasecmp ("TOOL", type_str) == 0) {
        *type = PYGWY_TOOL;
    } else {
        g_warning("Unknown type '%s' in '%s'", type_str, filename);
        *type = PYGWY_UNDEFINED;
    }

error:
    if (error) {
       Py_XDECREF(*code);
       *code = NULL;
    }
    Py_XDECREF(plugin_module);
    g_free(plugin_file_content);
    destroy_environment(d, TRUE);
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
                           gchar *desc)
{
    PygwyPluginInfo *info;
    gwy_debug("%s, %s, %s", filename, name, desc);
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
        s_pygwy.plugins = g_list_append(s_pygwy.plugins, info);
    } else {
        gwy_debug("Free: %s %s", info->name, info->filename);
        // FIXME: Terminated by glib free(): invalid pointer: blabla
        // when inserting duplicate module
        // g_free(info->name);
        // g_free(info->filename);
        // g_free(info);
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
        g_warning("Cannot register. Undefined 'menu_path' variable in '%s'",
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
        s_pygwy.plugins = g_list_append(s_pygwy.plugins, info);
    } else {
        // FIXME: Terminated by glib free(): invalid pointer: blabla
        // when inserting duplicate module
        // g_free(info->name);
        // g_free(info->filename);
        // g_free(info);
        g_warning("Cannot register plugin '%s'", filename);
        return FALSE;
    }
    return TRUE;

}

static gboolean
pygwy_register_graph_plugin(gchar *filename,
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
    if (gwy_graph_func_register(info->name,
                                pygwy_graph_run,
                                menu_path,
                                GWY_STOCK_GRAPH_FUNCTION,
                                GWY_MENU_FLAG_GRAPH, // TODO: determine correct flag
                                N_("Graph function written in Python")) ) { // not very descriptive
        // append plugin to list of plugins
        s_pygwy.plugins = g_list_append(s_pygwy.plugins, info);
    } else {
        // FIXME: Terminated by glib free(): invalid pointer: blabla
        // when inserting duplicate module
        // g_free(info->name);
        // g_free(info->filename);
        // g_free(info);
        g_warning("Cannot register plugin '%s'", filename);
        return FALSE;
    }
    return TRUE;

}

static void
pygwy_register_plugins(void)
{
    // FIXME: maybe place somewhere else
    GDir *plugin_dir;
    const gchar *plugin_filename;
    gchar *plugin_menu_path, *plugin_fullpath_filename;
    gchar *plugin_dir_name, *plugin_name, *plugin_desc = NULL;
    PygwyPluginType plugin_type = PYGWY_UNDEFINED;
    GError *err = NULL;
    PyObject *plugin_code, *dict;

    // reset static list 
    s_pygwy.plugins = NULL;

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
            gwy_debug("plugin_type: %d", plugin_type);
            switch(plugin_type)
            {
                case PYGWY_PROCESS:
                    if (plugin_code != NULL
                        && plugin_name != NULL
                        && plugin_menu_path != NULL)
                    {
                        pygwy_register_proc_plugin(plugin_fullpath_filename,
                                                   plugin_code,
                                                   plugin_name,
                                                   plugin_menu_path);
                    } else {
                        g_warning("Could not register process module: "
                                  "variables plugin_menu, "
                                  "plugin_type and plugin_name not defined.");
                    }
                    break;
                case PYGWY_FILE:
                    if (plugin_code != NULL
                        && plugin_name != NULL
                        && plugin_desc != NULL)
                    {
                    pygwy_register_file_plugin(plugin_fullpath_filename,
                                               plugin_code,
                                               plugin_name,
                                               plugin_desc);
                    } else {
                        g_warning("Could not register file module:"
                                  " variables plugin_desc, plugin_type"
                                  " and plugin_name not defined.");
                    }
                    break;
                case PYGWY_GRAPH:
                    printf("%s %s\n", plugin_name, plugin_desc);
                    if (plugin_code && plugin_name && plugin_desc) {
                    pygwy_register_graph_plugin(plugin_fullpath_filename,
                                                plugin_code,
                                                plugin_name,
                                                plugin_desc);

                    } else {
                        g_warning("Could not register graph module:"
                                  " variables plugin_desc, plugin_type"
                                  " and plugin_name not defined.");
                    }

                    break;
                case PYGWY_UNDEFINED:
                    g_warning("Cannot register plugin without defined "
                              "'plugin_type' variable  ('%s')",
                              plugin_fullpath_filename);
                    break;
                default:
                    g_warning("Rest of plugin types not yet implemented"); //TODO: PYGWY_LAYER, PYGWY_TOOL
            } // switch
            // reset plugin_code, plugin_name, plugin_desc, plugin_menu_path, plugin_type
            plugin_name = NULL;
            plugin_desc = NULL;
            plugin_menu_path = NULL;
            plugin_type = PYGWY_UNDEFINED;
            // remove variables from dictionary
            dict = PyModule_GetDict(s_pygwy.main_module);
            if (dict && PyDict_GetItemString(dict, "plugin_desc")) {
               gwy_debug("remove plugin_desc");
               PyDict_DelItemString(dict, "plugin_desc");
            }
            if (dict && PyDict_GetItemString(dict, "plugin_menu")) {
               gwy_debug("remove plugin_menu");
               PyDict_DelItemString(dict, "plugin_menu");
            }
            if (dict && PyDict_GetItemString(dict, "plugin_type")) {
               gwy_debug("remove plugin_type");
               PyDict_DelItemString(dict, "plugin_type");
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
            gwy_debug("No changes in '%s' since last run.", (*info)->filename);
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
    d = create_environment(info->filename, TRUE);
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
        destroy_environment(d, TRUE);
        return;
    }

    gwy_debug("Running plugin '%s', filename '%s'", info->name, info->filename);
    // create container named 'data' to allow access the container from python
    py_container = pygobject_new((GObject*)data);
    if (!py_container) {
        g_warning("Variable 'gwy.data' was not inicialized.");
    }
    PyDict_SetItemString(s_pygwy.dict, "data", py_container);

    // import module using precompiled code and run its 'run()' function
    cmd = g_strdup_printf("import %s\n"
                          "%s.run()",
                          info->name,
                          info->name);
    pygwy_run_string(cmd, Py_file_input, d, d);
    g_free(cmd);

    Py_DECREF(module);
    Py_DECREF(py_container); //FIXME
    destroy_environment(d, TRUE);
}

static void
pygwy_graph_run(GwyGraph *graph, const gchar *name)
{
    PygwyPluginInfo *info;
    PyObject *py_graph, *module, *d;
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
    d = create_environment(info->filename, TRUE);
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
        destroy_environment(d, TRUE);
        return;
    }

    gwy_debug("Running plugin '%s', filename '%s'", info->name, info->filename);
    // create graph named 'graph' to allow access the graph object from python
    py_graph = pygobject_new((GObject*)graph);
    if (!py_graph) {
        g_warning("Variable 'gwy.data' was not inicialized.");
    }
    PyDict_SetItemString(s_pygwy.dict, "graph", py_graph);

    // import module using precompiled code and run its 'run()' function
    cmd = g_strdup_printf("import %s\n"
                          "%s.run()",
                          info->name,
                          info->name);
    pygwy_run_string(cmd, Py_file_input, d, d);
    g_free(cmd);

    Py_DECREF(module);
    Py_DECREF(py_graph); //FIXME
    destroy_environment(d, TRUE);
}


static gboolean
pygwy_file_save_run(GwyContainer *data,
                    const gchar *filename,
                    GwyRunType mode,
                    GError **error,
                    const gchar *name)
{
    PyObject *py_filename, *module, *py_res, *d;
    PyObject *py_container;
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
    d = create_environment(info->filename, TRUE);
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
    if (!pygwy_check_func(module, "save", info->filename)) {
        destroy_environment(d, TRUE);
        return FALSE;
    }
    // create input container and put it into __main__ module dictionary
    py_container = pygobject_new((GObject*)data);
    //py_container->obj = GOBJECT(data);
    PyDict_SetItemString(d, "data", py_container);

    // create filename variable and put it into __main__ module dictionary
    py_filename = Py_BuildValue("s", filename);
    PyDict_SetItemString(d, "filename", py_filename);

    // import and execute the 'save' method
    cmd = g_strdup_printf("import %s\n"
                          "result = %s.save(data, filename)",
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
    destroy_environment(d, TRUE);
    return res;
}

static GwyContainer*
pygwy_file_load_run(const gchar *filename,
                    G_GNUC_UNUSED GwyRunType mode,
                    GError **error,
                    const gchar *name)
{
    GwyContainer *res = NULL;
    PyObject *o, *module = NULL, *type = NULL, *py_res, *d, *class_name = NULL;
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
    d = create_environment(info->filename, TRUE);
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
    cmd = g_strdup_printf("import %s\n"
                          "result = %s.load(filename)\n"
                          "print result",
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

    destroy_environment(d, TRUE);
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
    d = create_environment(info->filename, TRUE);
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
        destroy_environment(d, TRUE);
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
        cmd = g_strdup_printf("import %s\n"
                              "result = %s.detect_by_name(filename)",
                              info->name,
                              info->name);
    } else {
        cmd = g_strdup_printf("import %s\n"
                              "result = %s.detect_by_content(filename, head, tail, filesize)",
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
    destroy_environment(d, TRUE);
    return res;

}

static PygwyPluginInfo*
pygwy_find_plugin(const gchar* name)
{
    GList *l = s_pygwy.plugins;
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
/*
static PyObject*
convert_gvalue_to_pyobject(GValue *value)
{
    GType type;
    PyObject *o = NULL;

    if (!value) {
        g_warning("Value undefined.");
        return NULL;
    }
    type = G_TYPE_FUNDAMENTAL(G_VALUE_TYPE(value));
    gwy_debug("gvalue fundamental type: %s, original type: %s", g_type_name(type), g_type_name(G_VALUE_TYPE(value)));
    // g_value_type returns concete object type like GwyDataField (GWY_TYPE_DATA_FIELD)
    switch (type) {
        case G_TYPE_CHAR:
            o = PyInt_FromLong(g_value_get_char(value));
            break;
        case G_TYPE_UCHAR:
            o = PyInt_FromLong(g_value_get_uchar(value));
            break;
        case G_TYPE_BOOLEAN:
            o = PyBool_FromLong(g_value_get_boolean(value));
            break;
        case G_TYPE_INT:
            o = PyInt_FromLong(g_value_get_int(value));
            break;
        case G_TYPE_UINT:

            o = PyInt_FromLong(g_value_get_uint(value));
            break;
        case G_TYPE_LONG:
            o = PyLong_FromLong(g_value_get_long(value));
            break;
        case G_TYPE_ULONG:
            o = PyLong_FromLong(g_value_get_ulong(value));
            break;
        case G_TYPE_INT64:
            o = PyLong_FromLong(g_value_get_int64(value));
            break;
        case G_TYPE_UINT64:
            o = PyLong_FromLong(g_value_get_uint64(value));
            break;
        case G_TYPE_FLOAT:
            o = PyFloat_FromDouble(g_value_get_float(value));
            break;
        case G_TYPE_DOUBLE:
            o = PyFloat_FromDouble(g_value_get_double(value));
            break;
        case G_TYPE_STRING:
            o = PyString_FromString(g_value_get_string(value));
            break;
        case G_TYPE_OBJECT:
            o = pygobject_new((GObject *) g_value_get_object(value));

//        case G_TYPE_POINTER:
//        case G_TYPE_ENUM:
//        case G_TYPE_FLAGS:
//        case G_TYPE_INVALID:
//        case G_TYPE_NONE:
//        case G_TYPE_INTERFACE:
//       case G_TYPE_BOXED:
//        case G_TYPE_PARAM:
//        case G_TYPE_GTYPE:

    }
    return o;

}

static GValue*
convert_pyobject_to_gvalue(PyObject *o)
{
    GValue *g_value;

    if (!o) {
        g_warning("PyObject undefined.");
        return NULL;
    }

    g_value = g_malloc0(sizeof(GValue));
    //PyObject_Type()??
    if (PyBool_Check(o)) {
        g_value_init(g_value, G_TYPE_BOOLEAN);
        g_value_set_boolean(g_value, PyInt_AsLong(o));
    }
    else if (PyFloat_CheckExact(o)) {
        g_value_init(g_value, G_TYPE_DOUBLE);
        g_value_set_double(g_value, PyFloat_AsDouble(o));
    }
    else if (PyInt_CheckExact(o)) {
        g_value_init(g_value, G_TYPE_INT);
        g_value_set_int(g_value, (int) PyInt_AsLong(o));
    }
    else if (PyLong_CheckExact(o)) {
        g_value_init(g_value, G_TYPE_LONG);
        g_value_set_long(g_value, PyLong_AsLong(o));
    }
    else if (PyString_CheckExact(o)) {
        g_value_init(g_value, G_TYPE_STRING);
        g_value_set_string(g_value, PyString_AsString(o));
    }
    else if (PyTuple_CheckExact(o)) {
        // FIXME: what to do with tuples?
        g_free(g_value);
        g_value = NULL;
    }
    else if (o->ob_type == &PyGwyContainer_Type
             || o->ob_type == &PyGwyDataField_Type
             || o->ob_type == &PyGwyDataLine_Type
             || o->ob_type == &PyGwyResource_Type
             || o->ob_type == &PyGwySelection_Type
             || o->ob_type == &PyGwyAxis_Type
             || o->ob_type == &PyGwyColorButton_Type
             || o->ob_type == &PyGwyContainer_Type
             || o->ob_type == &PyGwyCurve_Type
             || o->ob_type == &PyGwyDataView_Type
             || o->ob_type == &PyGwyDataViewLayer_Type
             || o->ob_type == &PyGwyDataWindow_Type
             || o->ob_type == &PyGwyGraph_Type
             || o->ob_type == &PyGwyGraphArea_Type
             || o->ob_type == &PyGwyGraphCorner_Type
             || o->ob_type == &PyGwyGraphCurveModel_Type
             || o->ob_type == &PyGwyGraphCurves_Type
             || o->ob_type == &PyGwyGraphData_Type
             || o->ob_type == &PyGwyGraphLabel_Type
             || o->ob_type == &PyGwyGraphModel_Type
             || o->ob_type == &PyGwyGraphWindow_Type
             || o->ob_type == &PyGwyInventory_Type
             || o->ob_type == &PyGwyMarkerBox_Type
             || o->ob_type == &PyGwyNullStore_Type
             || o->ob_type == &PyGwyPixmapLayer_Type
             || o->ob_type == &PyGwyLayerBasic_Type
             || o->ob_type == &PyGwyLayerMask_Type
             || o->ob_type == &PyGwyHMarkerBox_Type
             || o->ob_type == &PyGwyResource_Type
             || o->ob_type == &PyGwyNLFitPreset_Type
             || o->ob_type == &PyGwyFDCurvePreset_Type
             || o->ob_type == &PyGwyGLMaterial_Type
             || o->ob_type == &PyGwyCDLine_Type
             || o->ob_type == &PyGwyRuler_Type
             || o->ob_type == &PyGwyHRuler_Type
             || o->ob_type == &PyGwySIUnit_Type
             || o->ob_type == &PyGwySciText_Type
             || o->ob_type == &PyGwySpectra_Type
             || o->ob_type == &PyGwySelectionGraph1DArea_Type
             || o->ob_type == &PyGwySelectionGraphArea_Type
             || o->ob_type == &PyGwySelectionGraphLine_Type
             || o->ob_type == &PyGwySelectionGraphPoint_Type
             || o->ob_type == &PyGwySelectionGraphZoom_Type
             || o->ob_type == &PyGwySensitivityGroup_Type
             || o->ob_type == &PyGwyShader_Type
             || o->ob_type == &PyGwyStatusbar_Type
             || o->ob_type == &PyGwyStringList_Type
             || o->ob_type == &PyGwyVRuler_Type
             || o->ob_type == &PyGwyVectorLayer_Type
             || o->ob_type == &PyGwySpectra_Type) {
        GObject *d;

        d = G_OBJECT(((PyGObject *) (o))->obj);
        g_value_init(g_value, G_TYPE_FROM_INSTANCE(d));
        g_value_set_object(g_value, d);
    } else {
        g_free(g_value);
        g_value = NULL;
    }

    return g_value;
}
*/

// function to fill list of containers, for  gwy_app_data_browser_get_containers
static void
pygwy_create_py_list_of_containers(GwyContainer *data, gpointer list)
{

   if (PyList_Append((PyObject *) list, pygobject_new((GObject *)data)) < 0) {
      g_warning("Could not append container to python list of containers.");
   }
   
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
