/* @(#) $Id$ */

/* FIXME: this module is completely braindamaged, since we don't want to
 * save the data as they are, but how gwyddion presents them, we have to
 * resort to play dirty tricks with the data windows hoping we won't
 * fuck it up... */

#include <stdio.h>
#include <string.h>
#include <png.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwydataviewlayer.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <app/app.h>

#define BITS_PER_SAMPLE 8

#define EXTENSION ".png"
/* XXX: the magic header is never used anyway, since we only *write*
 * png files */
/* decimally: 137 80 78 71 13 10 26 10 */
#define MAGIC "\211PNG\r\n\032\n"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

static gboolean      module_register     (const gchar *name);
static gint          gwypng_detect       (const gchar *filename,
                                          gboolean only_name);
static gboolean      gwypng_save         (GwyContainer *data,
                                          const gchar *filename);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "png",
    "Export data as PNG (Portable Network Graphics) images.",
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
    static GwyFileFuncInfo gwypng_func_info = {
        "png",
        "Portable Network Graphics (" EXTENSION ")",
        (GwyFileDetectFunc)&gwypng_detect,
        NULL,
        (GwyFileSaveFunc)&gwypng_save,
    };

    gwy_file_func_register(name, &gwypng_func_info);

    return TRUE;
}

static gint
gwypng_detect(const gchar *filename,
              gboolean only_name)
{
    FILE *fh;
    gchar magic[4];
    gint score;

    if (only_name)
        return g_str_has_suffix(filename, EXTENSION) ? 20 : 0;

    if (!(fh = fopen(filename, "rb")))
        return 0;
    score = 0;
    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && memcmp(magic, MAGIC, MAGIC_SIZE) == 0)
        score = 100;
    fclose(fh);

    return score;
}

static gboolean
gwypng_save(GwyContainer *data,
            const gchar *filename)
{
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyDataViewLayer *layer;
    png_bytepp rowptr_png = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    guchar *pixels = NULL;
    GdkPixbuf *pixbuf = NULL;
    gsize rowstride, i, width, height;
    FILE *fh = NULL;

    /* Note this is probably a race condition, no one guarantees the data
     * window can't change... */
    data_window = gwy_app_data_window_get_current();
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), FALSE);
    data_view = GWY_DATA_VIEW(gwy_data_window_get_data_view(data_window));
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), FALSE);
    g_return_val_if_fail(gwy_data_view_get_data(data_view) == data, FALSE);

    layer = gwy_data_view_get_base_layer(data_view);
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), FALSE);
    pixbuf = gwy_data_view_layer_paint(layer);

    layer = gwy_data_view_get_alpha_layer(data_view);
    if (layer) {
        g_warning("Cannot handle mask layers (yet)");
    }

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowptr_png = g_new(png_bytep, height);
    for (i = 0; i < height; i++)
        rowptr_png[i] = pixels + i*rowstride;

    if (!(fh = fopen(filename, "wb")))
        return FALSE;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                      NULL, NULL, NULL);
    g_assert(png_ptr);
    info_ptr = png_create_info_struct(png_ptr);
    g_assert(info_ptr);

    /* error handling */
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        g_warning("PNG write failed!");
        if (fh)
            fclose(fh);
        /* TODO: free stuff */
        return FALSE;
    }

    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_rows(png_ptr, info_ptr, rowptr_png);
    png_init_io(png_ptr, fh);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    fclose(fh);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    /* TODO: free stuff */

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
