/* @(#) $Id$ */

#include <stdio.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#define EXTENSION ".gwy"
#define MAGIC "GWYO"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

static gboolean      module_register     (const gchar *name);
static gint          gwyfile_detect      (const gchar *filename,
                                          gboolean only_name);
static GwyContainer* gwyfile_load        (const gchar *filename);
static gboolean      gwyfile_save        (GwyContainer *data,
                                          const gchar *filename);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "gwyfile",
    "Load and save Gwyddion native serialized objects.",
    "Yeti",
    "0.1",
    "Yeti",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo gwyfile_func_info = {
        "test_func",
        "Gwyddion native serialized objects (" EXTENSION ")",
        &gwyfile_detect,
        &gwyfile_load,
        &gwyfile_save,
    };

    gwy_register_file_func(name, &gwyfile_func_info);

    return TRUE;
}

static gint
gwyfile_detect(const gchar *filename,
               gboolean only_name)
{
    FILE *fh;
    gchar magic[4];
    gint score;

    if (only_name)
        return g_str_has_suffix(filename, EXTENSION) ? 100 : 0;

    if (!(fh = fopen(filename, "rb")))
        return 0;
    score = 0;
    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && memcmp(magic, MAGIC, MAGIC_SIZE) == 0)
        score = 100;
    fclose(fh);

    return score;
}

static GwyContainer*
gwyfile_load(const gchar *filename)
{
    GObject *object;
    GError *err = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    gsize pos = 0;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < 4
        || memcmp(buffer, MAGIC, MAGIC_SIZE)) {
        g_warning("File %s doesn't seem to be a .gwy file", filename);
        g_free(buffer);
        return NULL;
    }

    object = gwy_serializable_deserialize(buffer + 4, size - 4, &pos);
    g_free(buffer);
    if (!object) {
        g_warning("File %s deserialization failed", filename);
        return NULL;
    }
    if (!GWY_IS_CONTAINER(object)) {
        g_warning("File %s contains some strange object", filename);
        g_object_unref(object);
        return NULL;
    }

    return (GwyContainer*)object;
}

static gboolean
gwyfile_save(GwyContainer *data,
             const gchar *filename)
{
    guchar *buffer = NULL;
    gsize size = 0;
    FILE *fh;

    if (!(fh = fopen(filename, "wb")))
        return FALSE;
    buffer = gwy_serializable_serialize(G_OBJECT(data), buffer, &size);
    if (fwrite(MAGIC, 1, MAGIC_SIZE, fh) != MAGIC_SIZE
        fwrite(buffer, 1, size, fh) != size) {
        fclose(fh);
        g_free(buffer);
        return FALSE;
    }
    fclose(fh);
    g_free(buffer);
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
