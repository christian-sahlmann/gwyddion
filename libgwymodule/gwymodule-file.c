/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>

#include "gwymoduleinternal.h"
#include "gwymodule-file.h"

typedef struct {
    const gchar *filename;
    const gchar *winner;
    gint score;
    gboolean only_name;
    GwyFileOperation mode;
} GwyFileDetectData;

static void gwy_file_func_info_free    (gpointer data);
static void file_detect_max_score_cb   (const gchar *key,
                                        GwyFileFuncInfo *func_info,
                                        GwyFileDetectData *ddata);
static gint file_menu_entry_compare    (GwyFileFuncInfo *a,
                                        GwyFileFuncInfo *b);

static GHashTable *file_funcs = NULL;
static void (*func_register_callback)(const gchar *fullname) = NULL;

enum { bufsize = 1024 };

/**
 * gwy_file_func_register:
 * @modname: Module identifier (name).
 * @func_info: File type function info.
 *
 * Registeres a file type function.
 *
 * To keep compatibility with old versions @func_info should not be an
 * automatic variable.  However, since 1.6 it keeps a copy of @func_info.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_file_func_register(const gchar *modname,
                       GwyFileFuncInfo *func_info)
{
    _GwyModuleInfoInternal *iinfo;
    GwyFileFuncInfo *ftinfo;
    gchar *canon_name;

    gwy_debug("");
    gwy_debug("name = %s, file_desc = %s, detect = %p, load = %p, save = %p",
              func_info->name, func_info->file_desc,
              func_info->detect, func_info->load, func_info->save);

    if (!file_funcs) {
        gwy_debug("Initializing...");
        file_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           NULL, &gwy_file_func_info_free);
    }

    iinfo = gwy_module_get_module_info(modname);
    g_return_val_if_fail(iinfo, FALSE);
    g_return_val_if_fail(func_info->load || func_info->save, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    if (g_hash_table_lookup(file_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }

    ftinfo = g_memdup(func_info, sizeof(GwyFileFuncInfo));
    ftinfo->name = g_strdup(func_info->name);
    /* FIXME: This is not very clean. But we need the translated string often,
     * namely in menu building code. */
    ftinfo->file_desc = g_strdup(_(func_info->file_desc));

    g_hash_table_insert(file_funcs, (gpointer)ftinfo->name, ftinfo);
    canon_name = g_strconcat(GWY_MODULE_PREFIX_FILE, ftinfo->name, NULL);
    iinfo->funcs = g_slist_append(iinfo->funcs, canon_name);
    if (func_register_callback)
        func_register_callback(canon_name);

    return TRUE;
}

void
_gwy_file_func_set_register_callback(void (*callback)(const gchar *fullname))
{
    func_register_callback = callback;
}

static void
gwy_file_func_info_free(gpointer data)
{
    GwyFileFuncInfo *ftinfo = (GwyFileFuncInfo*)data;

    g_free((gpointer)ftinfo->name);
    g_free((gpointer)ftinfo->file_desc);
    g_free(ftinfo);
}

/**
 * gwy_file_func_run_detect:
 * @name: A file type function name.
 * @filename: A file name to detect.
 * @only_name: Whether to use only file name for a guess, or try to actually
 *             access the file.
 *
 * Runs a file type detection function identified by @name.
 *
 * Value of @only_name should be %TRUE if the file doesn't exist (is to be
 * written) so its contents can't be used for file type detection.
 *
 * This is a low-level function, consider using gwy_file_detect() if you
 * simply want to detect a file type.
 *
 * Returns: An integer score expressing the likehood of the file being
 *          loadable as this type. A basic scale is 20 for a good extension,
 *          100 for good magic header, more for more thorough tests.
 **/
gint
gwy_file_func_run_detect(const gchar *name,
                         const gchar *filename,
                         gboolean only_name)
{
    GwyFileFuncInfo *func_info;

    g_return_val_if_fail(filename, 0);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, 0);
    if (!func_info->detect)
        return 0;
    return func_info->detect(filename, only_name, name);
}

/**
 * gwy_file_func_run_load:
 * @name: A file load function name.
 * @filename: A file name to load data from.
 *
 * Runs a file load function identified by @name.
 *
 * This is a low-level function, consider using gwy_file_load() if you
 * simply want to load a file.
 *
 * Returns: A new #GwyContainer with data from @filename, or %NULL.
 **/
GwyContainer*
gwy_file_func_run_load(const gchar *name,
                       const gchar *filename)
{
    GwyFileFuncInfo *func_info;

    g_return_val_if_fail(filename, NULL);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, NULL);
    g_return_val_if_fail(func_info->load, NULL);

    return func_info->load(filename, name);
}

/**
 * gwy_file_func_run_save:
 * @name: A file save function name.
 * @data: A #GwyContainer to save.
 * @filename: A file name to save @data as.
 *
 * Runs a file save function identified by @name.
 *
 * It guarantees the container lifetime spans through the actual file saving,
 * so the module function doesn't have to care about it.
 *
 * This is a low-level function, consider using gwy_file_save() if you
 * simply want to save a file.
 *
 * Returns: %TRUE if file save succeeded, %FALSE otherwise.
 **/
gboolean
gwy_file_func_run_save(const gchar *name,
                       GwyContainer *data,
                       const gchar *filename)
{
    GwyFileFuncInfo *func_info;
    GwyDataField *dfield;
    gboolean status;

    g_return_val_if_fail(filename, FALSE);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, FALSE);
    g_return_val_if_fail(func_info->save, FALSE);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    /* TODO: Container */
    dfield = (GwyDataField*)gwy_container_get_object_by_name(data, "/0/data");
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);
    g_object_ref(data);
    g_object_ref(dfield);
    status = func_info->save(data, filename, name);
    g_object_unref(dfield);
    g_object_unref(data);

    return status;
}

static void
file_detect_max_score_cb(const gchar *key,
                         GwyFileFuncInfo *func_info,
                         GwyFileDetectData *ddata)
{
    gint score;

    g_assert(strcmp(key, func_info->name) == 0);

    if (!func_info->detect)
        return;
    if ((ddata->mode & GWY_FILE_LOAD) && !func_info->load)
        return;
    if ((ddata->mode & GWY_FILE_SAVE) && !func_info->save)
        return;

    score = func_info->detect(ddata->filename, ddata->only_name,
                              func_info->name);
    if (score > ddata->score) {
        ddata->winner = func_info->name;
        ddata->score = score;
    }
}

/**
 * gwy_file_detect:
 * @filename: A file name to detect type of.
 * @only_name: Whether to use only file name for a guess, or try to actually
 *             access the file.
 * @operations: The file operations (all of them) the file type should support.
 *
 * Detects file type of file @filename.
 *
 * Returns: The type name (i.e., the same name as passed to
 *          e.g. gwy_run_file_load_func()) of most probable type of @filename,
 *          or %NULL if there's no probable one.
 **/
G_CONST_RETURN gchar*
gwy_file_detect(const gchar *filename,
                gboolean only_name,
                GwyFileOperation operations)
{
    GwyFileDetectData ddata;

    g_return_val_if_fail(file_funcs, NULL);

    ddata.filename = filename;
    ddata.winner = NULL;
    ddata.score = 0;
    ddata.only_name = only_name;
    ddata.mode = operations;
    g_hash_table_foreach(file_funcs, (GHFunc)file_detect_max_score_cb, &ddata);

    if (!ddata.score)
        return NULL;
    return ddata.winner;
}

/**
 * gwy_file_load:
 * @filename: A file name to load data from.
 *
 * Loads a data file, autodetecting its type.
 *
 * Returns: A new #GwyContainer with data from @filename, or %NULL.
 **/
GwyContainer*
gwy_file_load(const gchar *filename)
{
    const gchar *winner;

    g_return_val_if_fail(file_funcs, NULL);

    winner = gwy_file_detect(filename, FALSE, GWY_FILE_LOAD);
    if (!winner)
        return NULL;

    return gwy_file_func_run_load(winner, filename);
}

/**
 * gwy_file_save:
 * @data: A #GwyContainer to save.
 * @filename: A file name to save the data as.
 *
 * Saves a data file, deciding to save as what type from the file name.
 *
 * Returns: %TRUE if file save succeeded, %FALSE otherwise.
 **/
gboolean
gwy_file_save(GwyContainer *data,
              const gchar *filename)
{
    GwyFileDetectData ddata;

    g_return_val_if_fail(file_funcs, FALSE);

    ddata.filename = filename;
    ddata.winner = NULL;
    ddata.score = 0;
    ddata.only_name = TRUE;
    ddata.mode = GWY_FILE_SAVE;
    g_hash_table_foreach(file_funcs, (GHFunc)file_detect_max_score_cb, &ddata);

    if (!ddata.winner)
        return FALSE;

    return gwy_file_func_run_save(ddata.winner, data, filename);
}

/**
 * gwy_file_func_build_menu:
 * @item_factory: A #GtkItemFactory to add items to.
 * @prefix: Where to add the menu items to the factory.
 * @item_callback: A #GtkItemFactoryCallback1 called when an item from the
 *                 menu is selected.
 * @type: Only function providing this file operation are included in the
 *        menu.
 *
 * Creates #GtkItemFactory for a file type menu with all registered file type
 * functions.
 *
 * Returns: The menu item factory as a #GtkObject.
 **/
GtkObject*
gwy_file_func_build_menu(GtkObject *item_factory,
                         const gchar *prefix,
                         GCallback item_callback,
                         GwyFileOperation type)
{
    GtkItemFactoryEntry branch = { NULL, NULL, NULL, 0, "<Branch>", NULL };
    GtkItemFactoryEntry tearoff = { NULL, NULL, NULL, 0, "<Tearoff>", NULL };
    GtkItemFactoryEntry item = { NULL, NULL, item_callback, 0, "<Item>", NULL };
    GtkItemFactory *factory;
    gchar *path;
    GSList *l, *entries = NULL;
    gint dp_len;

    g_return_val_if_fail(GTK_IS_ITEM_FACTORY(item_factory), NULL);
    factory = GTK_ITEM_FACTORY(item_factory);

    if (!file_funcs) {
        g_warning("No file function present to build menu of");
        entries = NULL;
    }
    else
        g_hash_table_foreach(file_funcs, gwy_hash_table_to_slist_cb,
                             &entries);
    entries = g_slist_sort(entries, (GCompareFunc)file_menu_entry_compare);

    dp_len = strlen(prefix);

    /* the root branch */
    path = strncpy(g_new(gchar, bufsize), prefix, bufsize);
    branch.path = path;
    gtk_item_factory_create_item(factory, &branch, NULL, 1);

    /* the root tearoff */
    g_strlcpy(path + dp_len, "/---", bufsize - dp_len);
    tearoff.path = path;
    gtk_item_factory_create_item(factory, &tearoff, NULL, 1);

    item.path = path;
    for (l = entries; l; l = g_slist_next(l)) {
        GwyFileFuncInfo *func_info = (GwyFileFuncInfo*)l->data;
        GwyFileOperation capable = 0;

        capable |= func_info->load ? GWY_FILE_LOAD : 0;
        capable |= func_info->save ? GWY_FILE_SAVE : 0;
        capable |= func_info->detect ? GWY_FILE_DETECT : 0;
        if (!(capable & type))
            continue;

        g_strlcpy(path + dp_len+1, func_info->file_desc, bufsize - dp_len-1);
        gtk_item_factory_create_item(factory, &item,
                                     (gpointer)func_info->name, 1);
    }

    g_free(path);
    g_slist_free(entries);

    return item_factory;
}

static gint
file_menu_entry_compare(GwyFileFuncInfo *a,
                        GwyFileFuncInfo *b)
{
    g_assert(a->file_desc && b->file_desc);
    return strcmp(a->file_desc, b->file_desc);
}

/**
 * gwy_file_func_get_operations:
 * @name: File type function name.
 *
 * Returns possible operations for a file type function identified by
 * @name.
 *
 * This function is the prefered one for testing whether a file
 * function exists, as function with no operations cannot be registered.
 *
 * Returns: The file operation bit mask, zero if @name does not exist.
 **/
GwyFileOperation
gwy_file_func_get_operations(const gchar *name)
{
    GwyFileFuncInfo *func_info;
    GwyFileOperation capable = 0;

    func_info = g_hash_table_lookup(file_funcs, name);
    if (!func_info)
        return 0;

    capable |= func_info->load ? GWY_FILE_LOAD : 0;
    capable |= func_info->save ? GWY_FILE_SAVE : 0;
    capable |= func_info->detect ? GWY_FILE_DETECT : 0;

    return capable;
}

gboolean
gwy_file_func_remove(const gchar *name)
{
    gwy_debug("%s", name);
    if (!g_hash_table_remove(file_funcs, name)) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * GwyFileFuncInfo:
 * @name: File type function name (used for all detect/save/load functions).
 * @file_desc: Brief file type description.  This will appear in the menu
 *             so it should not contain slashes, and the preferred form is
 *             "Foobar data (.foo)".
 * @detect: The file type detecting function.
 * @load: The file loading function.
 * @save: The file saving function.
 *
 * Information about set of functions for one file type.
 **/

/**
 * GwyFileDetectFunc:
 * @filename: A file name to detect the filetype of.
 * @only_name: Whether the type should be guessed only from file name.
 * @name: Function name from #GwyFileFuncInfo (most modules can safely ignore
 *        this argument)
 *
 * The type of file type detection function.
 *
 * When called with %TRUE @only_name it should not try to access the file.
 *
 * Returns: An integer likehood score (see gwy_file_func_run_detect() for
 *          description).
 **/

/**
 * GwyFileLoadFunc:
 * @filename: A file name to load data from.
 * @name: Function name from #GwyFileFuncInfo (most modules can safely ignore
 *        this argument)
 *
 * The type of file loading function.
 *
 * Returns: A newly created data container or %NULL.
 **/

/**
 * GwyFileSaveFunc:
 * @data: A #GwyContainer to save.
 * @filename: A file name to save @data as.
 * @name: Function name from #GwyFileFuncInfo (most modules can safely ignore
 *        this argument)
 *
 * The type of file saving function.
 *
 * Returns: %TRUE if file save succeeded, %FALSE otherwise.
 **/

/**
 * GwyFileOperation:
 * @GWY_FILE_NONE: None.
 * @GWY_FILE_LOAD: Posibility to load files of this type.
 * @GWY_FILE_SAVE: Posibility to save files of this type.
 * @GWY_FILE_DETECT: Posibility to detect files are of this file type,
 * @GWY_FILE_MASK: The mask for all the flags.
 *
 * File type function file operations (capabilities).
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
