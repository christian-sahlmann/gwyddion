/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

/* FIXME: this module is completely braindamaged, since we don't want to
 * save the data as they are, but how gwyddion presents them, so we have to
 * resort to play dirty tricks with the data windows in the desperate hope
 * we won't fuck anything up... */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <png.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* broken under Win32 */
#ifndef G_OS_WIN32
#include <jpeglib.h>
#endif

#include <tiffio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwydataviewlayer.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <app/app.h>

#define BITS_PER_SAMPLE 8

#define GWY_PNG_EXTENSIONS ".png"
#define GWY_PNG_MAGIC "\211PNG\r\n\032\n"
#define GWY_PNG_MAGIC_SIZE (sizeof(GWY_PNG_MAGIC)-1)

/* this is broken for EXIF, but we never read JPEGs anyway, only export */
#define GWY_JPEG_EXTENSIONS ".jpeg,.jpg"
#define GWY_JPEG_MAGIC "\xff\xd8\xff\xe0\x00\x10JFIF"
#define GWY_JPEG_MAGIC_SIZE (sizeof(GWY_JPEG_MAGIC)-1)

/* this is broken, but we never read TIFFs anyway, only export */
#define GWY_TIFF_EXTENSIONS ".tif,.tiff"
#define GWY_TIFF_MAGIC "II\x2a\x00"
/*#define GWY_TIFF_MAGIC "MM\x00\x2a"*/
#define GWY_TIFF_MAGIC_SIZE (sizeof(GWY_TIFF_MAGIC)-1)

#define GWY_PPM_EXTENSIONS ".ppm,.pnm"
#define GWY_PPM_MAGIC "P6"
#define GWY_PPM_MAGIC_SIZE (sizeof(GWY_PPM_MAGIC)-1)

#define GWY_BMP_EXTENSIONS ".bmp"
#define GWY_BMP_MAGIC "BM"
#define GWY_BMP_MAGIC_SIZE (sizeof(GWY_BMP_MAGIC)-1)

static gboolean      module_register      (const gchar *name);
static gint          pixmap_detect        (const gchar *filename,
                                           gboolean only_name,
                                           const gchar *name);
static gboolean      pixmap_save          (GwyContainer *data,
                                           const gchar *filename,
                                           const gchar *name);
static gboolean      pixmap_do_write_png  (FILE *fh,
                                           const gchar *filename,
                                           GdkPixbuf *pixbuf);
#ifndef G_OS_WIN32
static gboolean      pixmap_do_write_jpeg (FILE *fh,
                                           const gchar *filename,
                                           GdkPixbuf *pixbuf);
#endif
static gboolean      pixmap_do_write_tiff (FILE *fh,
                                           const gchar *filename,
                                           GdkPixbuf *pixbuf);
static gboolean      pixmap_do_write_ppm  (FILE *fh,
                                           const gchar *filename,
                                           GdkPixbuf *pixbuf);
static gboolean      pixmap_do_write_bmp  (FILE *fh,
                                           const gchar *filename,
                                           GdkPixbuf *pixbuf);
static gsize         find_format          (const gchar *name);

static struct {
    const gchar *name;
    const gchar *extensions;
    const gchar *magic;
    gsize magic_size;
    gboolean (*do_write)(FILE*, const gchar*, GdkPixbuf*);
}
const pixmap_formats[] = {
    {
        "png",
        GWY_PNG_EXTENSIONS,  GWY_PNG_MAGIC,  GWY_PNG_MAGIC_SIZE,
        &pixmap_do_write_png,
    },
#ifndef G_OS_WIN32
    {
        "jpeg",
        GWY_JPEG_EXTENSIONS, GWY_JPEG_MAGIC, GWY_JPEG_MAGIC_SIZE,
        &pixmap_do_write_jpeg,
    },
#endif
    {
        "tiff",
        GWY_TIFF_EXTENSIONS, GWY_TIFF_MAGIC, GWY_TIFF_MAGIC_SIZE,
        &pixmap_do_write_tiff,
    },
    {
        "ppm",
        GWY_PPM_EXTENSIONS,  GWY_PPM_MAGIC,  GWY_PPM_MAGIC_SIZE,
        &pixmap_do_write_ppm,
    },
    {
        "bmp",
        GWY_BMP_EXTENSIONS,  GWY_BMP_MAGIC,  GWY_BMP_MAGIC_SIZE,
        &pixmap_do_write_bmp
    },
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "pixmap",
    "Exports data as as pixmap images.  Supports following image formats: "
        "PNG (Portable Network Graphics), "
#ifndef G_OS_WIN32
        "JPEG (Joint Photographic Experts Group), "
#endif
        "TIFF (Tag Image File Format), "
        "PPM (Portable Pixmap), "
        "BMP (Windows or OS2 Bitmap).",
    "Yeti <yeti@physics.muni.cz>",
    "2.1",
    "David Nečas (Yeti) & Petr Klapetek",
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
        "Portable Network Graphics (" GWY_PNG_EXTENSIONS ")",
        (GwyFileDetectFunc)&pixmap_detect,
        NULL,
        (GwyFileSaveFunc)&pixmap_save,
    };
#ifndef G_OS_WIN32
    static GwyFileFuncInfo gwyjpeg_func_info = {
        "jpeg",
        "JPEG (" GWY_JPEG_EXTENSIONS ")",
        (GwyFileDetectFunc)&pixmap_detect,
        NULL,
        (GwyFileSaveFunc)&pixmap_save,
    };
#endif
    static GwyFileFuncInfo gwytiff_func_info = {
        "tiff",
        "TIFF (" GWY_TIFF_EXTENSIONS ")",
        (GwyFileDetectFunc)&pixmap_detect,
        NULL,
        (GwyFileSaveFunc)&pixmap_save,
    };
    static GwyFileFuncInfo gwyppm_func_info = {
        "ppm",
        "Portable Pixmap (" GWY_PPM_EXTENSIONS ")",
        (GwyFileDetectFunc)&pixmap_detect,
        NULL,
        (GwyFileSaveFunc)&pixmap_save,
    };
    static GwyFileFuncInfo gwybmp_func_info = {
        "bmp",
        "Windows or OS2 Bitmap (" GWY_BMP_EXTENSIONS ")",
        (GwyFileDetectFunc)&pixmap_detect,
        NULL,
        (GwyFileSaveFunc)&pixmap_save,
    };

    gwy_file_func_register(name, &gwypng_func_info);
#ifndef G_OS_WIN32
    gwy_file_func_register(name, &gwyjpeg_func_info);
#endif
    gwy_file_func_register(name, &gwytiff_func_info);
    gwy_file_func_register(name, &gwyppm_func_info);
    gwy_file_func_register(name, &gwybmp_func_info);

    return TRUE;
}

static gint
pixmap_detect(const gchar *filename,
              gboolean only_name,
              const gchar *name)
{
    gsize i, ext;
    FILE *fh;
    gchar magic[4];
    gint score;

    i = find_format(name);
    g_return_val_if_fail(i < G_N_ELEMENTS(pixmap_formats), 0);

    if (only_name) {
        gchar **extensions;

        extensions = g_strsplit(pixmap_formats[i].extensions, ",", 0);
        g_assert(extensions);
        for (ext = 0; extensions[ext]; ext++) {
            if (g_str_has_suffix(filename, extensions[ext]))
                break;
        }
        score = extensions[ext] ? 20 : 0;
        g_strfreev(extensions);

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;
    score = 0;
    if (fread(magic, 1, pixmap_formats[i].magic_size, fh)
            == pixmap_formats[i].magic_size
        && memcmp(magic, pixmap_formats[i].magic, pixmap_formats[i].magic_size)
            == 0)
        score = 100;
    fclose(fh);

    return score;
}

static gboolean
pixmap_save(GwyContainer *data,
            const gchar *filename,
            const gchar *name)
{
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyPixmapLayer *layer;
    GdkPixbuf *pixbuf = NULL;
    gsize i;
    FILE *fh = NULL;
    gboolean ok;

    i = find_format(name);
    g_return_val_if_fail(i < G_N_ELEMENTS(pixmap_formats), 0);

    /* Note this is probably a race condition, no one guarantees the data
     * window can't change... */
    data_window = gwy_app_data_window_get_current();
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), FALSE);
    data_view = GWY_DATA_VIEW(gwy_data_window_get_data_view(data_window));
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), FALSE);
    g_return_val_if_fail(gwy_data_view_get_data(data_view) == data, FALSE);

    layer = gwy_data_view_get_base_layer(data_view);
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), FALSE);
    pixbuf = gwy_pixmap_layer_paint(layer);

    layer = gwy_data_view_get_alpha_layer(data_view);
    if (layer)
        g_warning("Cannot handle mask layers (yet)");

    if (!(fh = fopen(filename, "wb")))
        return FALSE;
    ok = pixmap_formats[i].do_write(fh, filename, pixbuf);
    if (!ok)
        unlink(filename);
    /* XXX: @#$%! libtiff can't let us close files normally... */
    if (strcmp(name, "tiff") != 0)
        fclose(fh);

    return ok;
}

static gboolean
pixmap_do_write_png(FILE *fh,
                    const gchar *filename,
                    GdkPixbuf *pixbuf)
{
    png_bytepp rowptr_png = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    guchar *pixels = NULL;
    gsize rowstride, i, width, height;
    gboolean ok = TRUE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    rowptr_png = g_new(png_bytep, height);
    for (i = 0; i < height; i++)
        rowptr_png[i] = pixels + i*rowstride;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                      NULL, NULL, NULL);
    g_assert(png_ptr);
    info_ptr = png_create_info_struct(png_ptr);
    g_assert(info_ptr);

    /* error handling */
    if (setjmp(png_jmpbuf(png_ptr))) {
        g_warning("PNG `%s' write failed!", filename);
        ok = FALSE;
        goto end;
    }

    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_rows(png_ptr, info_ptr, rowptr_png);
    png_init_io(png_ptr, fh);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

end:
    png_destroy_write_struct(&png_ptr, &info_ptr);
    g_free(rowptr_png);

    return ok;
}

#ifndef G_OS_WIN32
static gboolean
pixmap_do_write_jpeg(FILE *fh,
                     G_GNUC_UNUSED const gchar *filename,
                     GdkPixbuf *pixbuf)
{
    JSAMPROW *rowptr_jpeg = NULL;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    guchar *pixels = NULL;
    gsize rowstride, i, width, height;
    /* TODO: error handling */
    gboolean ok = TRUE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3; /* rgb */
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 98, TRUE);

    rowptr_jpeg = (JSAMPROW*)g_new(gchar*, height);
    for (i = 0; i < height; i++)
        rowptr_jpeg[i] = pixels + i*rowstride;

    jpeg_stdio_dest(&cinfo, fh);
    jpeg_start_compress(&cinfo, TRUE);
    jpeg_write_scanlines(&cinfo, rowptr_jpeg, height);
    jpeg_finish_compress(&cinfo);

    jpeg_destroy_compress(&cinfo);
    g_free(rowptr_jpeg);

    return ok;
}
#endif

static gboolean
pixmap_do_write_tiff(FILE *fh,
                     const gchar *filename,
                     GdkPixbuf *pixbuf)
{
    TIFF *out;
    guchar *pixels = NULL;
    gsize rowstride, i, width, height;
    /* TODO: error handling */
    gboolean ok = TRUE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    out = TIFFFdOpen(fileno(fh), filename, "w");

    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

    g_assert(TIFFScanlineSize(out) <= (glong)rowstride);
    TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, 3*width));
    for (i = 0; i < height; i++) {
        if (TIFFWriteScanline(out, pixels + i*rowstride, i, 0) < 0) {
            g_warning("TIFF `%s' write failed!", filename);
            ok = FALSE;
            break;
        }
    }
    TIFFClose(out);

    return ok;
}

static gboolean
pixmap_do_write_ppm(FILE *fh,
                    G_GNUC_UNUSED const gchar *filename,
                    GdkPixbuf *pixbuf)
{
    static const gchar *ppm_header = "P6\n%u\n%u\n255\n";
    guchar *pixels = NULL;
    gsize rowstride, i, width, height;
    gboolean ok = TRUE;
    gchar *ppmh = NULL;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    ppmh = g_strdup_printf(ppm_header, width, height);
    if (fwrite(ppmh, 1, strlen(ppmh), fh) != strlen(ppmh)) {
        ok = FALSE;
        goto end;
    }

    for (i = 0; i < height; i++) {
        if (fwrite(pixels + i*rowstride, 1, 3*width, fh) != 3*width) {
            ok = FALSE;
            goto end;
        }
    }
end:
    g_free(ppmh);

    return ok;
}

static gboolean
pixmap_do_write_bmp(FILE *fh,
                    G_GNUC_UNUSED const gchar *filename,
                    GdkPixbuf *pixbuf)
{
    static guchar bmp_head[] = {
        'B', 'M',    /* magic */
        0, 0, 0, 0,  /* file size */
        0, 0, 0, 0,  /* reserved */
        54, 0, 0, 0, /* offset */
        40, 0, 0, 0, /* header size */
        0, 0, 0, 0,  /* width */
        0, 0, 0, 0,  /* height */
        1, 0,        /* bit planes */
        24, 0,       /* bpp */
        0, 0, 0, 0,  /* compression type */
        0, 0, 0, 0,  /* (compressed) image size */
        0, 0, 0, 0,  /* x resolution */
        0, 0, 0, 0,  /* y resolution */
        0, 0, 0, 0,  /* ncl */
        0, 0, 0, 0,  /* nic */
    };
    guchar *pixels = NULL, *buffer = NULL;
    gsize rowstride, i, j, width, height;
    gsize bmplen, bmprowstride;
    gboolean ok = TRUE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    bmprowstride = 3*((width + 3) >> 2 << 2);
    bmplen = height*bmprowstride + sizeof(bmp_head);

    *(guint32*)(bmp_head + 2) = GUINT32_TO_LE(bmplen);
    *(guint32*)(bmp_head + 18) = GUINT32_TO_LE(width);
    *(guint32*)(bmp_head + 22) = GUINT32_TO_LE(height);
    *(guint32*)(bmp_head + 34) = GUINT32_TO_LE(height*bmprowstride);
    if (fwrite(bmp_head, 1, sizeof(bmp_head), fh) != sizeof(bmp_head))
        return FALSE;

    /* The ugly part: BMP uses BGR instead of RGB and is written upside down,
     * this silliness may originate nowhere else than in MS... */
    buffer = g_new(guchar, bmprowstride);
    for (i = 0; i < height; i++) {
        guchar *p = pixels + (height - 1 - i)*rowstride;
        guchar *q = buffer;

        for (j = width; j; j--, p += 3, q += 3) {
            *q = *(p + 2);
            *(q + 1) = *(p + 1);
            *(q + 2) = *p;
        }
        if (fwrite(buffer, 1, bmprowstride, fh) != bmprowstride) {
            ok = FALSE;
            goto end;
        }
    }

end:
    g_free(buffer);
    return ok;
}

static gsize
find_format(const gchar *name)
{
    gsize i;

    for (i = 0; i < G_N_ELEMENTS(pixmap_formats); i++) {
        if (strcmp(pixmap_formats[i].name, name) == 0)
            return i;
    }

    return (gsize)-1;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
