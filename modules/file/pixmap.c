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
#define GWY_JPEG_EXTENSIONS ".jpeg,.jpg"
#define GWY_TIFF_EXTENSIONS ".tiff,.tif"
#define GWY_PPM_EXTENSIONS ".ppm,.pnm"
#define GWY_BMP_EXTENSIONS ".bmp"
#define GWY_TARGA_EXTENSIONS ".tga"

static gboolean      module_register           (const gchar *name);
static gint          pixmap_detect             (const gchar *filename,
                                                gboolean only_name,
                                                const gchar *name);
static gboolean      pixmap_save               (GwyContainer *data,
                                                const gchar *filename,
                                                const gchar *name);
static gboolean      pixmap_do_write_png       (const gchar *filename,
                                                GdkPixbuf *pixbuf);
#ifndef G_OS_WIN32
static gboolean      pixmap_do_write_jpeg      (const gchar *filename,
                                                GdkPixbuf *pixbuf);
#endif
static gboolean      pixmap_do_write_tiff      (const gchar *filename,
                                                GdkPixbuf *pixbuf);
static gboolean      pixmap_do_write_ppm       (const gchar *filename,
                                                GdkPixbuf *pixbuf);
static gboolean      pixmap_do_write_bmp       (const gchar *filename,
                                                GdkPixbuf *pixbuf);
static gboolean      pixmap_do_write_targa     (const gchar *filename,
                                                GdkPixbuf *pixbuf);
static gsize         find_format               (const gchar *name);
static void          find_data_window_for_data (GwyDataWindow *window,
                                                gpointer *p);

static struct {
    const gchar *name;
    const gchar *extensions;
    gboolean (*do_write)(const gchar*, GdkPixbuf*);
}
const pixmap_formats[] = {
    {
        "png",
        GWY_PNG_EXTENSIONS,
        &pixmap_do_write_png,
    },
    {
        "jpeg",
        GWY_JPEG_EXTENSIONS,
        &pixmap_do_write_jpeg,
    },
#ifndef G_OS_WIN32
    {
        "tiff",
        GWY_TIFF_EXTENSIONS,
        &pixmap_do_write_tiff,
    },
#endif
    {
        "ppm",
        GWY_PPM_EXTENSIONS,
        &pixmap_do_write_ppm,
    },
    {
        "bmp",
        GWY_BMP_EXTENSIONS,
        &pixmap_do_write_bmp
    },
    {
        "targa",
        GWY_TARGA_EXTENSIONS,
        &pixmap_do_write_targa
    },
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "pixmap",
    "Exports data as as pixmap images.  Supports following image formats: "
        "PNG (Portable Network Graphics), "
        "JPEG (Joint Photographic Experts Group), "
#ifndef G_OS_WIN32
        "TIFF (Tag Image File Format), "
#endif
        "PPM (Portable Pixmap), "
        "BMP (Windows or OS2 Bitmap), "
        "TARGA (Truevision Advanced Raster Graphics Adapter).",
    "Yeti <yeti@gwyddion.net>",
    "3.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
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
    static GwyFileFuncInfo gwyjpeg_func_info = {
        "jpeg",
        "JPEG (" GWY_JPEG_EXTENSIONS ")",
        (GwyFileDetectFunc)&pixmap_detect,
        NULL,
        (GwyFileSaveFunc)&pixmap_save,
    };
#ifndef G_OS_WIN32
    static GwyFileFuncInfo gwytiff_func_info = {
        "tiff",
        "TIFF (" GWY_TIFF_EXTENSIONS ")",
        (GwyFileDetectFunc)&pixmap_detect,
        NULL,
        (GwyFileSaveFunc)&pixmap_save,
    };
#endif
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
    static GwyFileFuncInfo gwytarga_func_info = {
        "targa",
        "TARGA (" GWY_TARGA_EXTENSIONS ")",
        (GwyFileDetectFunc)&pixmap_detect,
        NULL,
        (GwyFileSaveFunc)&pixmap_save,
    };

    gwy_file_func_register(name, &gwypng_func_info);
    gwy_file_func_register(name, &gwyjpeg_func_info);
#ifndef G_OS_WIN32
    gwy_file_func_register(name, &gwytiff_func_info);
#endif
    gwy_file_func_register(name, &gwyppm_func_info);
    gwy_file_func_register(name, &gwybmp_func_info);
    gwy_file_func_register(name, &gwytarga_func_info);

    return TRUE;
}

static gint
pixmap_detect(const gchar *filename,
              G_GNUC_UNUSED gboolean only_name,
              const gchar *name)
{
    gsize i, ext;
    gint score;
    gchar **extensions;

    i = find_format(name);
    g_return_val_if_fail(i < G_N_ELEMENTS(pixmap_formats), 0);


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

static gboolean
pixmap_save(GwyContainer *data,
            const gchar *filename,
            const gchar *name)
{
    gpointer p[2];
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyPixmapLayer *layer;
    GdkPixbuf *pixbuf = NULL;
    gsize i;

    i = find_format(name);
    g_return_val_if_fail(i < G_N_ELEMENTS(pixmap_formats), 0);

    p[0] = data;
    p[1] = NULL;
    gwy_app_data_window_foreach((GFunc)find_data_window_for_data, p);
    g_return_val_if_fail(p[1], FALSE);
    data_window = (GwyDataWindow*)p[1];
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

    return pixmap_formats[i].do_write(filename, pixbuf);
}

static gboolean
pixmap_do_write_png(const gchar *filename,
                    GdkPixbuf *pixbuf)
{
    GError *err = NULL;
    gboolean ok;

    ok = gdk_pixbuf_save(pixbuf, filename, "png", &err, NULL);
    if (!ok) {
        g_warning("PNG `%s' write failed: %s", filename, err->message);
        g_clear_error(&err);
    }

    return ok;
}

static gboolean
pixmap_do_write_jpeg(const gchar *filename,
                     GdkPixbuf *pixbuf)
{
    GError *err = NULL;
    gboolean ok;

    ok = gdk_pixbuf_save(pixbuf, filename, "jpeg", &err, "quality", "98", NULL);
    if (!ok) {
        g_warning("JPEG `%s' write failed: %s", filename, err->message);
        g_clear_error(&err);
    }

    return ok;
}

#ifndef G_OS_WIN32
/* FIXME: this breaks badly on Win32, nooone knows why */
static gboolean
pixmap_do_write_tiff(const gchar *filename,
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

    out = TIFFOpen(filename, "w");

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
#endif

static gboolean
pixmap_do_write_ppm(const gchar *filename,
                    GdkPixbuf *pixbuf)
{
    static const gchar *ppm_header = "P6\n%u\n%u\n255\n";
    guchar *pixels = NULL;
    gsize rowstride, i, width, height;
    gboolean ok = FALSE;
    gchar *ppmh = NULL;
    FILE *fh;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    fh = fopen(filename, "wb");
    if (!fh) {
        g_warning("PPM `%s' write failed!", filename);
        return FALSE;
    }

    ppmh = g_strdup_printf(ppm_header, width, height);
    if (fwrite(ppmh, 1, strlen(ppmh), fh) != strlen(ppmh))
        goto end;

    for (i = 0; i < height; i++) {
        if (fwrite(pixels + i*rowstride, 1, 3*width, fh) != 3*width)
            goto end;
    }

    ok = TRUE;
end:
    g_free(ppmh);
    fclose(fh);

    return ok;
}

static gboolean
pixmap_do_write_bmp(const gchar *filename,
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
    gboolean ok = FALSE;
    FILE *fh;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    bmprowstride = 3*((width + 3) >> 2 << 2);
    bmplen = height*bmprowstride + sizeof(bmp_head);

    fh = fopen(filename, "wb");
    if (!fh) {
        g_warning("PPM `%s' write failed!", filename);
        return FALSE;
    }

    *(guint32*)(bmp_head + 2) = GUINT32_TO_LE(bmplen);
    *(guint32*)(bmp_head + 18) = GUINT32_TO_LE(width);
    *(guint32*)(bmp_head + 22) = GUINT32_TO_LE(height);
    *(guint32*)(bmp_head + 34) = GUINT32_TO_LE(height*bmprowstride);
    if (fwrite(bmp_head, 1, sizeof(bmp_head), fh) != sizeof(bmp_head))
        goto end;

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
        if (fwrite(buffer, 1, bmprowstride, fh) != bmprowstride)
            goto end;
    }

    ok = TRUE;
end:
    g_free(buffer);
    fclose(fh);

    return ok;
}

static gboolean
pixmap_do_write_targa(const gchar *filename,
                      GdkPixbuf *pixbuf)
{
   static guchar targa_head[] = {
     0,           /* idlength */
     0,           /* colourmaptype */
     2,           /* datatypecode: uncompressed RGB */
     0, 0, 0, 0,  /* colourmaporigin, colourmaplength */
     0,           /* colourmapdepth */
     0, 0, 0, 0,  /* x-origin, y-origin */
     0, 0,        /* width */
     0, 0,        /* height */
     24,          /* bits per pixel */
     0,           /* image descriptor */
    };
    guchar *pixels, *buffer = NULL;
    gsize rowstride, i, j, width, height;
    gboolean ok = FALSE;
    FILE *fh;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    if (height > 65535 || width > 65535) {
        g_warning("Image too large to be stored as TARGA");
        return FALSE;
    }
    targa_head[12] = (width) & 0xff;
    targa_head[13] = (width >> 8) & 0xff;
    targa_head[14] = (height) & 0xff;
    targa_head[15] = (height >> 8) & 0xff;

    fh = fopen(filename, "wb");
    if (!fh) {
        g_warning("TARGA `%s' write failed!", filename);
        return FALSE;
    }

    if (fwrite(targa_head, 1, sizeof(targa_head), fh) != sizeof(targa_head))
        goto end;

    /* The ugly part: TARGS uses BGR instead of RGB and is written upside down,
     * it's really strange it wasn't invented by MS... */
    buffer = g_new(guchar, rowstride);
    for (i = 0; i < height; i++) {
        guchar *p = pixels + (height - 1 - i)*rowstride;
        guchar *q = buffer;

        for (j = width; j; j--, p += 3, q += 3) {
            *q = *(p + 2);
            *(q + 1) = *(p + 1);
            *(q + 2) = *p;
        }
        if (fwrite(buffer, 1, rowstride, fh) != rowstride)
            goto end;
    }

    ok = TRUE;
end:
    g_free(buffer);
    fclose(fh);

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

static void
find_data_window_for_data(GwyDataWindow *window,
                          gpointer *p)
{
    if (gwy_data_window_get_data(window) == p[0])
        p[1] = window;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
