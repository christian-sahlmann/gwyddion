/*
 *  @(#) $Id$
 *  Copyright (C) 2012 David Necas (Yeti), Petr Klapetek, Jozef Vesely.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, vesely@gjh.sk.
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

/********** from pygwy.c **********/

#include <pygtk-2.0/pygobject.h>
#include <app/gwyapp.h>
#include "wrap_calls.h"
#include <glib/gstdio.h>
#include "pygwy.h"

/* function to fill list of containers, for gwy_app_data_browser_get_containers
 */
static void
pygwy_create_py_list_of_containers(GwyContainer *data, gpointer list)
{
   if (PyList_Append((PyObject *) list, pygobject_new((GObject *)data)) < 0) {
      g_warning("Could not append container to python list of containers.");
   }
}

#include "pygwywrap.c"
#include "pygwy-console.h"

/********** from gwybatch.c **********/

static void
load_modules(void)
{
    const gchar *const module_types[] = {
        "file", "layer", "process", "graph", "tools", NULL
    };
    GPtrArray *module_dirs;
    const gchar *upath;
    gchar *mpath;
    guint i;

    module_dirs = g_ptr_array_new();

    /* System modules */
    mpath = gwy_find_self_dir("modules");
    for (i = 0; module_types[i]; i++) {
        g_ptr_array_add(module_dirs,
                        g_build_filename(mpath, module_types[i], NULL));
    }
    g_free(mpath);

    /* User modules */
    upath = gwy_get_user_dir();
    for (i = 0; module_types[i]; i++) {
        g_ptr_array_add(module_dirs,
                        g_build_filename(upath, module_types[i], NULL));
    }

    /* Register all found there, in given order. */
    g_ptr_array_add(module_dirs, NULL);
    gwy_module_register_modules((const gchar**)module_dirs->pdata);

    for (i = 0; module_dirs->pdata[i]; i++)
        g_free(module_dirs->pdata[i]);
    g_ptr_array_free(module_dirs, TRUE);
}

/* FIXME: It would be better to just fix the flags using RTLD_NOLOAD because
 * the libraries are surely loaded. */
static gboolean
reload_libraries(void)
{
    static const gchar *const gwyddion_libs[] = {
        "libgwyddion2", "libgwyprocess2", "libgwydraw2", "libgwydgets2",
        "libgwymodule2", "libgwyapp2x",
    };
    guint i;

    for (i = 0; i < G_N_ELEMENTS(gwyddion_libs); i++) {
        gchar *filename = g_strconcat(gwyddion_libs[i], ".", G_MODULE_SUFFIX,
                                      NULL);
        GModule *modhandle = g_module_open(filename, G_MODULE_BIND_LAZY);
        if (!modhandle) {
            gchar *excstr = g_strdup_printf("Cannot dlopen() %s.", filename);
            PyErr_SetString(PyExc_ImportError, excstr);
            g_free(excstr);
            return FALSE;
        }
        g_module_make_resident(modhandle);
        g_free(filename);
    }

    return TRUE;
}

PyMODINIT_FUNC
initgwy(void)
{
    gchar *settings_file;
    PyObject *mod, *dict;

    if (!reload_libraries())
        return;

    /* gwybatch.c */
    /* This requires a display.  */
    gtk_init(NULL, NULL);
    //gtk_parse_args(NULL, NULL);
    gwy_widgets_type_init();
    gwy_undo_set_enabled(FALSE);
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GRADIENT));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GL_MATERIAL));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GRAIN_VALUE));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_CALIBRATION));
    settings_file = gwy_app_settings_get_settings_filename();
    gwy_app_settings_load(settings_file, NULL);
    g_free(settings_file);
    /* This requires a display.  */
    gwy_stock_register_stock_items();
    load_modules();

    /* pygwy.c */
    init_pygobject();
    mod = Py_InitModule("gwy", (PyMethodDef*)pygwy_functions);
    dict = PyModule_GetDict(mod);
    /* This does "import gtk" so display is required. */
    pygwy_register_classes(dict);
    pygwy_add_constants(mod, "GWY_");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
