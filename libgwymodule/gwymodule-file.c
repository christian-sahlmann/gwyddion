/* @(#) $Id$ */

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>

#include "gwymodule-file.h"

static GHashTable *file_funcs = NULL;

typedef struct {
    const gchar *filename;
    const gchar *winner;
    gint score;
    gboolean only_name;
    gboolean must_have_load;
    gboolean must_have_save;
} GwyFileDetectData;

/**
 * gwy_register_file_func:
 * @modname: Module identifier (name).
 * @func_info: File type function info.
 *
 * Registeres a data processing function.
 *
 * The passed @func_info must not be an automatic variable.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_register_file_func(const gchar *modname,
                       GwyFileFuncInfo *func_info)
{
    GwyModuleInfoInternal *iinfo;

    gwy_debug("%s", __FUNCTION__);
    gwy_debug("name = %s, file_desc = %s, detect = %p, load = %p, save = %p",
              func_info->name, func_info->file_desc,
              func_info->detect, func_info->load, func_info->save);

    if (!file_funcs) {
        gwy_debug("%s: Initializing...", __FUNCTION__);
        file_funcs = g_hash_table_new(g_str_hash, g_str_equal);
    }

    iinfo = gwy_module_get_module_info(modname);
    g_return_val_if_fail(iinfo, FALSE);
    g_return_val_if_fail(func_info->load || func_info->save, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    if (g_hash_table_lookup(file_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }
    g_hash_table_insert(file_funcs, (gpointer)func_info->name, func_info);
    return TRUE;
}

/**
 * gwy_run_file_detect_func:
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
 * Returns: An integer score expressing the likehood of the file being
 *          loadable as this type. A basic scale is 20 for a good extension,
 *          100 for good magic header, more for more thorough tests.
 **/
gint
gwy_run_file_detect_func(const gchar *name,
                         const gchar *filename,
                         gboolean only_name)
{
    GwyFileFuncInfo *func_info;

    g_return_val_if_fail(filename, 0);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, 0);
    if (!func_info->detect)
        return 0;
    return func_info->detect(filename, only_name);
}

/**
 * gwy_run_file_load_func:
 * @name: A file load function name.
 * @filename: A file name to load data from.
 *
 * Runs a file load function identified by @name.
 *
 * Returns: A new #GwyContainer with data from @filename, or %NULL.
 **/
GwyContainer*
gwy_run_file_load_func(const gchar *name,
                       const gchar *filename)
{
    GwyFileFuncInfo *func_info;

    g_return_val_if_fail(filename, NULL);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, NULL);
    g_return_val_if_fail(func_info->load, NULL);

    return func_info->load(filename);
}

/**
 * gwy_run_file_save_func:
 * @name: A file save function name.
 * @data: A #GwyContainer to save.
 * @filename: A file name to save @data as.
 *
 * Runs a file save function identified by @name.
 *
 * It guarantees the container lifetime spans through the actual file saving,
 * so the module function doesn't have to care about it.
 *
 * Returns: %TRUE if file save succeeded, %FALSE otherwise.
 **/
gboolean
gwy_run_file_save_func(const gchar *name,
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
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), NULL);
    g_object_ref(data);
    g_object_ref(dfield);
    status = func_info->save(data, filename);
    g_object_unref(dfield);
    g_object_unref(data);

    return status;
}

static void
file_detect_max_score(const gchar *key,
                      GwyFileFuncInfo *func_info,
                      GwyFileDetectData *ddata)
{
    gint score;

    g_assert(strcmp(key, func_info->name) == 0);

    if (!func_info->detect)
        return;
    if (ddata->must_have_load && !func_info->load)
        return;
    if (ddata->must_have_save && !func_info->save)
        return;

    score = func_info->detect(ddata->filename, ddata->only_name);
    if (score > ddata->score) {
        ddata->winner = func_info->name;
        ddata->score = score;
    }
}

/**
 * gwy_file_detect:
 * @filename: A file name to detect type of.
 *
 * Detects file type of file @filename.
 *
 * Returns: The type name (i.e., the same name as passed to
 *          e.g. gwy_run_file_load_func()) of most probable type of @filename,
 *          or %NULL if there's no probable one.
 **/
G_CONST_RETURN gchar*
gwy_file_detect(const gchar *filename)
{
    GwyFileDetectData ddata;

    ddata.filename = filename;
    ddata.winner = NULL;
    ddata.score = 0;
    ddata.only_name = FALSE;
    ddata.must_have_load = TRUE;
    ddata.must_have_save = FALSE;
    g_hash_table_foreach(file_funcs, (GHFunc)file_detect_max_score, &ddata);

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

    winner = gwy_file_detect(filename);
    if (!winner)
        return NULL;

    return gwy_run_file_load_func(winner, filename);
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

    ddata.filename = filename;
    ddata.winner = NULL;
    ddata.score = 0;
    ddata.only_name = TRUE;
    ddata.must_have_load = FALSE;
    ddata.must_have_save = TRUE;
    g_hash_table_foreach(file_funcs, (GHFunc)file_detect_max_score, &ddata);

    if (!ddata.winner)
        return FALSE;

    return gwy_run_file_save_func(ddata.winner, data, filename);
}

/************************** Documentation ****************************/

/**
 * GwyFileFuncInfo:
 * @name: File type function name (used for all detect/save/load functions).
 * @file_desc: Brief file type description.
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
 *
 * The type of file type detection function.
 *
 * When called with %TRUE @only_name it should not try to access the file.
 *
 * Returns: An integer likehood score (see gwy_run_file_detect_func() for
 *          description).
 **/

/**
 * GwyFileLoadFunc:
 * @filename: A file name to load data from.
 *
 * The type of file loading function.
 *
 * Returns: A newly created data container or %NULL.
 **/

/**
 * GwyFileSaveFunc:
 * @data: A #GwyContainer to save.
 * @filename: A file name to save @data as.
 *
 * The type of file saving function.
 *
 * Returns: %TRUE if file save succeeded, %FALSE otherwise.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
