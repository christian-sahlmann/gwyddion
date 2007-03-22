#line 1 "pygwy_orig.c"
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
#include <app/gwyapp.h>
#include "pygwywrap.c"
#line 39 "pygwy_orig.c"

typedef struct {
    gchar *name;
    gchar *filename;
} PygwyPluginInfo;

static gboolean         module_register       (void);
static void             pygwy_plugin_run      (GwyContainer *data,
                                               GwyRunType run,
                                               const gchar *name);
static void             pygwy_register_plugins(void);
static PygwyPluginInfo* pygwy_find_plugin     (const gchar* name);

static GList *s_pygwy_plugins = NULL;

DL_EXPORT(void)
initpygwy(GwyContainer *container)
{
    PyObject *m, *d;
    PyObject *py_container;
    init_pygobject();

    m = Py_InitModule("gwy", (PyMethodDef*) pygwy_functions);
    d = PyModule_GetDict(m);

    pygwy_register_classes(d);
    pygwy_add_constants(m, "GWY_");

    /* Create accessible object GwyContainer (gwy.data) */
    py_container = pygobject_new((GObject*)container);
    PyDict_SetItemString(d, "data", py_container);
}

static const GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Pygwy, then Gwyddion Python wrapper."),
    "Jan Hořák <xhorak@gmail.com>",
    "0.1",
    "Jan Hořák",
    "2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    pygwy_register_plugins();
    return TRUE;
}

static void
pygwy_register_plugins(void)
{
    // FIXME: maybe place somewhere else
    static const gchar pygwy_plugin_dir_name[] = "pygwy";

    GDir *pygwy_plugin_dir;
    const gchar *pygwy_filename;
    gchar *menu_path;
    gchar *pygwy_plugname;
    PygwyPluginInfo *info;
    gchar *dir;
    GError *err = NULL;

    dir = g_build_filename(gwy_get_user_dir(), pygwy_plugin_dir_name, NULL);

    gwy_debug("Plugin path: %s", dir);

    pygwy_plugin_dir = g_dir_open(dir, 0, &err);

    if (pygwy_plugin_dir == NULL && err) {
        if (err->code == G_FILE_ERROR_NOENT) { // directory not found/does not exist
            if (g_mkdir(dir, 0700)) {
                g_warning("Cannot create pygwy plugin directory %s", dir);
            } else {
                gwy_debug("Pygwy directory created: %s", dir);
            }
        } else {
            g_warning("Cannot open pygwy directory: %s, reason: %s", dir, err->message);
        }
        g_free(dir);
        /* Whenever the directory has been created or not, there is no reason
           to continue by reading scripts as long as no script */
        return;
    }

    while ((pygwy_filename = g_dir_read_name(pygwy_plugin_dir))) {
        if (g_str_has_suffix(pygwy_filename, ".py")
           || g_str_has_suffix(pygwy_filename, ".PY")
           || g_str_has_suffix(pygwy_filename, ".Py") ) {
            // for menu item name use filename without extension
            pygwy_plugname = g_strndup(pygwy_filename, strlen(pygwy_filename)-3);
        } else {
            gwy_debug("wrong extension for file: %s", pygwy_filename);
            continue;
        }
        info = g_new(PygwyPluginInfo, 1);
        info->name = pygwy_plugname;
        info->filename = g_build_filename(dir, pygwy_filename, NULL);
        menu_path = g_strconcat(_("/_Plug-Ins/"), pygwy_plugname, NULL);

        gwy_debug("appending: %s, %s", info->name, info->filename);
        if (gwy_process_func_register(info->name,
                                  pygwy_plugin_run,
                                  menu_path,
                                  NULL,
                                  GWY_RUN_IMMEDIATE,
                                  GWY_MENU_FLAG_DATA, // TODO: determine correct flag
                                  N_("Function written in Python")) ) { // not very descriptive
            s_pygwy_plugins = g_list_append(s_pygwy_plugins, info);
        } else {
            g_free(info->name);
            g_free(info->filename);
            g_free(info);
            g_warning("Cannot register plugin '%s'", pygwy_filename);
        }
    }
    g_dir_close(pygwy_plugin_dir);
    g_free(dir);
}

static void
pygwy_plugin_run(GwyContainer *data, GwyRunType run, const gchar *name)
{
    PygwyPluginInfo *info;
    PyThreadState* py_thread_state = NULL;
    FILE *pyfile;

    if (!(info = pygwy_find_plugin(name))) {
        g_warning("cannot find plugin.");
        return;
    }
   /* open script file */
    pyfile = fopen(info->filename, "r");

    if (!pyfile) {
        g_warning("Cannot find pygwy script file '%s'", info->filename);
        return;
    }
    /* Initialize the Python interpreter.  Required. */
    if (!Py_IsInitialized()) {
        gwy_debug("Initializing Python interpreter" );
        // Do not register signal handlers
        Py_InitializeEx(0);
        gwy_debug("Initializing Pygwy classes.");
        initpygwy(data);
     } else {
        gwy_debug("Python interpreter already initialized");
    }
    /* initialize module class */


    gwy_debug("Running plugin '%s', filename '%s'", info->name, info->filename);
    py_thread_state = Py_NewInterpreter();
    initpygwy(data);
    /* Run pyfile */
    PyRun_AnyFile(pyfile, info->filename);

    gwy_debug("Clear interpreter");
    /* Cleaning interpreter */
    Py_EndInterpreter(py_thread_state);

    // Py_Finalize();
    fclose(pyfile);

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
