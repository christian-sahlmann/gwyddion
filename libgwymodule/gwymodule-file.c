/* @(#) $Id$ */

#include <string.h>
#include <libgwyddion/gwymacros.h>
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

gboolean
gwy_run_file_save_func(const gchar *name,
                       GwyContainer *data,
                       const gchar *filename)
{
    GwyFileFuncInfo *func_info;

    g_return_val_if_fail(filename, FALSE);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, FALSE);
    g_return_val_if_fail(func_info->save, FALSE);

    return func_info->save(data, filename);
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

    return ddata.winner;
}

GwyContainer*
gwy_file_load(const gchar *filename)
{
    const gchar *winner;

    winner = gwy_file_detect(filename);
    if (!winner)
        return NULL;

    return gwy_run_file_load_func(winner, filename);
}

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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
