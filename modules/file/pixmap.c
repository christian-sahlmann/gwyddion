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

/*
 * TODO:
 * - use GwySIUnit
 */

#define DEBUG

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gdk/gdk.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef G_OS_WIN32
#include <tiffio.h>
#endif

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define BITS_PER_SAMPLE 8

#define TICK_LENGTH 10

#define GWY_PNG_EXTENSIONS ".png"
#define GWY_JPEG_EXTENSIONS ".jpeg,.jpg"
#define GWY_TIFF_EXTENSIONS ".tiff,.tif"
#define GWY_PPM_EXTENSIONS ".ppm,.pnm"
#define GWY_BMP_EXTENSIONS ".bmp"
#define GWY_TARGA_EXTENSIONS ".tga"

#define ZOOM2LW(x) ((x) > 1 ? ((x) + 0.4) : 1)

typedef enum {
    PIXMAP_RAW_DATA,
    PIXMAP_RULERS,
    PIXMAP_EVERYTHING
} PixmapOutput;

typedef struct {
    gdouble zoom;
    PixmapOutput otype;
} PixmapArgs;

static gboolean      module_register           (const gchar *name);
static gint          pixmap_detect             (const gchar *filename,
                                                gboolean only_name,
                                                const gchar *name);
static gboolean      pixmap_save               (GwyContainer *data,
                                                const gchar *filename,
                                                const gchar *name);
static gboolean      pixmap_dialog             (PixmapArgs *args,
                                                const gchar *name);
static gboolean      pixmap_do_write_png       (const gchar *filename,
                                                GdkPixbuf *pixbuf);
static gboolean      pixmap_do_write_jpeg      (const gchar *filename,
                                                GdkPixbuf *pixbuf);
#ifndef G_OS_WIN32
static gboolean      pixmap_do_write_tiff      (const gchar *filename,
                                                GdkPixbuf *pixbuf);
#endif
static gboolean      pixmap_do_write_ppm       (const gchar *filename,
                                                GdkPixbuf *pixbuf);
static gboolean      pixmap_do_write_bmp       (const gchar *filename,
                                                GdkPixbuf *pixbuf);
static gboolean      pixmap_do_write_targa     (const gchar *filename,
                                                GdkPixbuf *pixbuf);
static GdkPixbuf*    hruler                    (gint size,
                                                gint extra,
                                                gdouble real,
                                                gdouble zoom,
                                                const gchar *units);
static GdkPixbuf*    vruler                    (gint size,
                                                gint extra,
                                                gdouble real,
                                                gdouble zoom,
                                                const gchar *units);
static GdkPixbuf*    fmscale                   (gint size,
                                                gdouble bot,
                                                gdouble top,
                                                gdouble zoom,
                                                const gchar *units);
static GdkDrawable*  prepare_drawable          (gint width,
                                                gint height,
                                                gint lw,
                                                GdkGC **gc);
static PangoLayout*  prepare_layout            (gdouble zoom);
static gsize         find_format               (const gchar *name);
static void          find_data_window_for_data (GwyDataWindow *window,
                                                gpointer *p);
static void          pixmap_load_args          (GwyContainer *container,
                                                PixmapArgs *args);
static void          pixmap_save_args          (GwyContainer *container,
                                                PixmapArgs *args);

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

static const GwyEnum output_formats[] = {
    { "Data alone",    PIXMAP_RAW_DATA },
    { "Data + rulers", PIXMAP_RULERS },
    { "Everything",    PIXMAP_EVERYTHING },
};

static const PixmapArgs pixmap_defaults = {
    1.0, PIXMAP_EVERYTHING
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
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    GwyPalette *palette;
    GdkPixbuf *pixbuf, *hrpixbuf, *vrpixbuf, *datapixbuf, *tmpixbuf;
    GdkPixbuf *scalepixbuf = NULL;
    GwyContainer *settings;
    PixmapArgs args;
    const guchar *samples;
    guchar *pixels;
    gpointer p[2];
    gint width, height, zwidth, zheight, hrh, vrw, scw, nsamp, y, lw;
    gint border = 20;
    gint gap = 20;
    gint fmw = 18;
    gboolean ok;
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
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    settings = gwy_app_settings_get();
    pixmap_load_args(settings, &args);
    if (!pixmap_dialog(&args, pixmap_formats[i].name))
        return FALSE;

    layer = gwy_data_view_get_base_layer(data_view);
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), FALSE);
    palette = gwy_layer_basic_get_palette(GWY_LAYER_BASIC(layer));
    samples = gwy_palette_get_samples(palette, &nsamp);
    datapixbuf = gdk_pixbuf_copy(gwy_pixmap_layer_paint(layer));
    width = gdk_pixbuf_get_width(datapixbuf);
    height = gdk_pixbuf_get_height(datapixbuf);

    layer = gwy_data_view_get_alpha_layer(data_view);
    if (layer) {
        tmpixbuf = gwy_pixmap_layer_paint(layer);
        gdk_pixbuf_composite(tmpixbuf, datapixbuf,
                             0, 0, width, height,
                             0, 0, 1.0, 1.0,
                             GDK_INTERP_NEAREST, 0xff);
    }

    zwidth = args.zoom*width;
    zheight = args.zoom*height;
    if (args.otype == PIXMAP_RAW_DATA) {
        pixbuf = gdk_pixbuf_scale_simple(datapixbuf, zwidth, zheight,
                                         GDK_INTERP_TILES);
        ok = pixmap_formats[i].do_write(filename, pixbuf);
        g_object_unref(pixbuf);

        return ok;
    }

    gap *= args.zoom;
    fmw *= args.zoom;
    lw = ZOOM2LW(args.zoom);

    hrpixbuf = hruler(zwidth + 2*lw, border, gwy_data_field_get_xreal(dfield),
                      args.zoom, "m");
    hrh = gdk_pixbuf_get_height(hrpixbuf);
    vrpixbuf = vruler(zheight + 2*lw, border, gwy_data_field_get_yreal(dfield),
                      args.zoom, "m");
    vrw = gdk_pixbuf_get_width(vrpixbuf);
    if (args.otype == PIXMAP_EVERYTHING) {
        scalepixbuf = fmscale(zheight + 2*lw,
                            gwy_data_field_get_min(dfield),
                            gwy_data_field_get_max(dfield),
                            args.zoom, "m");
        scw = gdk_pixbuf_get_width(scalepixbuf);
    }
    else {
        gap = 0;
        fmw = 0;
        scw = 0;
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            vrw + zwidth + 2*lw + 2*border
                            + gap + fmw + 2*lw + scw,
                            hrh + zheight + 2*lw + 2*border + border/3);
    gdk_pixbuf_fill(pixbuf, 0xffffffff);
    gdk_pixbuf_scale(datapixbuf, pixbuf,
                     vrw + lw + border, hrh + lw + border,
                     zwidth, zheight,
                     vrw + lw + border, hrh + lw + border,
                     args.zoom, args.zoom,
                     GDK_INTERP_TILES);
    g_object_unref(datapixbuf);
    gdk_pixbuf_copy_area(hrpixbuf,
                         0, 0,
                         zwidth + 2*lw + border, hrh,
                         pixbuf,
                         vrw + border, border);
    g_object_unref(hrpixbuf);
    gdk_pixbuf_copy_area(vrpixbuf,
                         0, 0,
                         vrw, zheight + 2*lw + border,
                         pixbuf,
                         border, hrh + border);
    g_object_unref(vrpixbuf);
    if (args.otype == PIXMAP_EVERYTHING) {
        gdk_pixbuf_copy_area(scalepixbuf,
                            0, 0,
                            scw, zheight + 2*lw,
                            pixbuf,
                            border + vrw + zwidth + 2*lw + gap + fmw + 2*lw,
                            hrh + border);
        g_object_unref(scalepixbuf);

        pixels = gdk_pixbuf_get_pixels(pixbuf);
        for (y = 0; y < zheight; y++) {
            gint j, k;
            guchar *row;

            row = pixels
                + gdk_pixbuf_get_rowstride(pixbuf)*(border + hrh + lw + y)
                + 3*(int)(border + vrw + zwidth + 2*lw + gap + lw);
            k = nsamp-1 - floor(nsamp*y/args.zoom/height);
            for (j = 0; j < fmw; j++) {
                row[3*j] = samples[4*k];
                row[3*j + 1] = samples[4*k + 1];
                row[3*j + 2] = samples[4*k + 2];
            }
        }
    }

    /* outline */
    tmpixbuf = gdk_pixbuf_new_subpixbuf(pixbuf,
                                        vrw + border, hrh + border,
                                        lw, zheight + 2*lw);
    gdk_pixbuf_fill(tmpixbuf, 0x000000);
    gdk_pixbuf_copy_area(tmpixbuf, 0, 0, lw, zheight + 2*lw,
                         pixbuf, vrw + border + zwidth + lw, hrh + border);
    if (args.otype == PIXMAP_EVERYTHING) {
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, lw, zheight + lw,
                            pixbuf,
                            vrw + border + zwidth + 2*lw + gap,
                            hrh + border);
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, lw, zheight + 2*lw,
                            pixbuf,
                            vrw + border + zwidth + 2*lw + gap + fmw + lw,
                            hrh + border);
    }
    g_object_unref(tmpixbuf);

    tmpixbuf = gdk_pixbuf_new_subpixbuf(pixbuf,
                                        vrw + border, hrh + border,
                                        zwidth + 2*lw, lw);
    gdk_pixbuf_fill(tmpixbuf, 0x000000);
    gdk_pixbuf_copy_area(tmpixbuf, 0, 0, zwidth + 2*lw, lw,
                         pixbuf, vrw + border, hrh + border + zheight + lw);
    if (args.otype == PIXMAP_EVERYTHING) {
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, fmw + 2*lw, lw,
                            pixbuf,
                            vrw + border + zwidth + 2*lw + gap,
                            hrh + border);
        gdk_pixbuf_copy_area(tmpixbuf, 0, 0, fmw + 2*lw, lw,
                            pixbuf,
                            vrw + border + zwidth + 2*lw + gap,
                            hrh + border + lw + zheight);
    }
    g_object_unref(tmpixbuf);

    ok = pixmap_formats[i].do_write(filename, pixbuf);
    if (ok)
        pixmap_save_args(settings, &args);

    g_object_unref(pixbuf);

    return ok;
}

static gboolean
pixmap_dialog(PixmapArgs *args,
              const gchar *name)
{
    GtkObject *zoom;
    GtkWidget *dialog, *table, *spin, *omenu;
    enum { RESPONSE_RESET = 1 };
    gint response;
    gchar *s, *title;

    s = g_ascii_strup(name, -1);
    title = g_strconcat(_("Export "), s, NULL);
    g_free(s);
    dialog = gtk_dialog_new_with_buttons(title, NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    g_free(title);

    table = gtk_table_new(2, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    zoom = gtk_adjustment_new(args->zoom, 0.06, 16.0, 0.1, 1.0, 0);
    spin = gwy_table_attach_spinbutton(table, 0, _("_Zoom:"), "", zoom);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);

    omenu = gwy_option_menu_create(output_formats, G_N_ELEMENTS(output_formats),
                                   "output-format", NULL, NULL, args->otype);
    gwy_table_attach_row(table, 1, _("Output:"), "", omenu);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = pixmap_defaults;
            gtk_adjustment_set_value(GTK_ADJUSTMENT(zoom), args->zoom);
            gwy_option_menu_set_history(omenu, "output-format", args->otype);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->zoom = gtk_adjustment_get_value(GTK_ADJUSTMENT(zoom));
    args->otype = gwy_option_menu_get_history(omenu, "output-format");
    gtk_widget_destroy(dialog);

    return TRUE;
}

/***************************************************************************
 *
 *  writers
 *
 ***************************************************************************/

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

    /* The ugly part: TARGA uses BGR instead of RGB and is written upside down,
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

/***************************************************************************
 *
 *  renderers
 *
 ***************************************************************************/

static GdkPixbuf*
hruler(gint size,
       gint extra,
       gdouble real,
       gdouble zoom,
       const gchar *units)
{
    const gsize bufsize = 64;
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    gdouble magnitude, base, step, x;
    gchar *s;
    gint precision, l, n, ix;
    gint tick, height, lw;

    s = g_new(gchar, bufsize);
    layout = prepare_layout(zoom);

    magnitude = gwy_math_humanize_numbers(real/12, real, &precision);

    g_snprintf(s, bufsize, "%.*f", precision, real/magnitude);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);

    g_snprintf(s, bufsize, "%.*f %s%s",
               precision, 0.0, gwy_math_SI_prefix(magnitude), units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical2);

    l = MAX(PANGO_PIXELS(logical1.width), PANGO_PIXELS(logical2.width));
    n = MIN(10, size/l);
    step = real/magnitude/n;
    base = exp(G_LN10*floor(log10(step)));
    step = step/base;
    if (step <= 2.0)
        step = 2.0;
    else if (step <= 5.0)
        step = 5.0;
    else {
        base *= 10;
        step = 1.0;
    }

    tick = zoom*TICK_LENGTH;
    lw = ZOOM2LW(zoom);
    l = MAX(PANGO_PIXELS(logical1.height), PANGO_PIXELS(logical2.height));
    height = l + 2*zoom + tick + 2;
    drawable = prepare_drawable(size + extra, height, lw, &gc);

    for (x = 0.0; x <= real/magnitude; x += base*step) {
        if (!x)
            g_snprintf(s, bufsize, "%.*f %s%s",
                       precision, x, gwy_math_SI_prefix(magnitude), units);
        else
            g_snprintf(s, bufsize, "%.*f", precision, x);
        pango_layout_set_markup(layout, s, -1);
        ix = x/(real/magnitude)*size + lw/2;
        pango_layout_get_extents(layout, NULL, &logical1);
        if (ix + PANGO_PIXELS(logical1.width) <= size + extra/4)
            gdk_draw_layout(drawable, gc, ix+1, 1, layout);
        gdk_draw_line(drawable, gc, ix, height-1, ix, height-1-tick);
    }

    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, size + extra, height);

    g_object_unref(gc);
    g_object_unref(drawable);
    g_object_unref(layout);
    g_free(s);

    return pixbuf;
}

static GdkPixbuf*
vruler(gint size,
       gint extra,
       gdouble real,
       gdouble zoom,
       const gchar *units)
{
    const gsize bufsize = 64;
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    gdouble magnitude, base, step, x;
    gchar *s;
    gint precision, l, n, ix;
    gint tick, width, lw;

    s = g_new(gchar, bufsize);
    layout = prepare_layout(zoom);

    magnitude = gwy_math_humanize_numbers(real/12, real, &precision);

    /* note the algorithm is the same to force consistency between axes,
     * even though the vertical one could be filled with tick more densely */
    g_snprintf(s, bufsize, "%.*f", precision, real/magnitude);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);

    g_snprintf(s, bufsize, "%.*f %s%s",
               precision, 0.0, gwy_math_SI_prefix(magnitude), units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical2);

    l = MAX(PANGO_PIXELS(logical1.width), PANGO_PIXELS(logical2.width));
    n = MIN(10, size/l);
    step = real/magnitude/n;
    base = exp(G_LN10*floor(log10(step)));
    step = step/base;
    if (step <= 2.0)
        step = 2.0;
    else if (step <= 5.0)
        step = 5.0;
    else {
        base *= 10;
        step = 1.0;
    }

    g_snprintf(s, bufsize, "%.*f", precision, real/magnitude);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);
    l = PANGO_PIXELS(logical1.width);

    tick = zoom*TICK_LENGTH;
    lw = ZOOM2LW(zoom);
    width = l + 2*zoom + tick + 2;
    drawable = prepare_drawable(width, size + extra, lw, &gc);

    for (x = 0.0; x <= real/magnitude; x += base*step) {
        g_snprintf(s, bufsize, "%.*f", precision, x);
        pango_layout_set_markup(layout, s, -1);
        ix = x/(real/magnitude)*size + lw/2;
        pango_layout_get_extents(layout, NULL, &logical1);
        if (ix + PANGO_PIXELS(logical1.height) <= size + extra/4)
            gdk_draw_layout(drawable, gc, 1, ix+1, layout);
        gdk_draw_line(drawable, gc, width-1, ix, width-1-tick, ix);
    }

    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, width, size + extra);

    g_object_unref(gc);
    g_object_unref(drawable);
    g_object_unref(layout);
    g_free(s);

    return pixbuf;
}


static GdkPixbuf*
fmscale(gint size,
        gdouble bot,
        gdouble top,
        gdouble zoom,
        const gchar *units)
{
    const gsize bufsize = 64;
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    gdouble magnitude, x;
    gchar *s;
    gint precision, l;
    gint tick, width, lw;

    s = g_new(gchar, bufsize);
    layout = prepare_layout(zoom);

    x = MAX(fabs(bot), fabs(top));
    magnitude = gwy_math_humanize_numbers(x/120, x, &precision);

    g_snprintf(s, bufsize, "%.*f %s%s",
               precision, top/magnitude, gwy_math_SI_prefix(magnitude), units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);

    g_snprintf(s, bufsize, "%.*f %s%s",
               precision, bot/magnitude, gwy_math_SI_prefix(magnitude), units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical2);

    l = MAX(PANGO_PIXELS(logical1.width), PANGO_PIXELS(logical2.width));
    tick = zoom*TICK_LENGTH;
    lw = ZOOM2LW(zoom);
    width = l + 2*zoom + tick + 2;
    drawable = prepare_drawable(width, size, lw, &gc);

    g_snprintf(s, bufsize, "%.*f %s%s",
               precision, bot/magnitude, gwy_math_SI_prefix(magnitude), units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);
    gdk_draw_layout(drawable, gc,
                    width - PANGO_PIXELS(logical1.width) - 2,
                    size - 1 - PANGO_PIXELS(logical1.height),
                    layout);
    gdk_draw_line(drawable, gc, 0, size - (lw + 1)/2, tick, size - (lw + 1)/2);

    g_snprintf(s, bufsize, "%.*f %s%s",
               precision, top/magnitude, gwy_math_SI_prefix(magnitude), units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);
    gdk_draw_layout(drawable, gc,
                    width - PANGO_PIXELS(logical1.width) - 2, 1,
                    layout);
    gdk_draw_line(drawable, gc, 0, lw/2, tick, lw/2);

    gdk_draw_line(drawable, gc, 0, size/2, tick/2, size/2);

    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, width, size);

    g_object_unref(gc);
    g_object_unref(drawable);
    g_object_unref(layout);
    g_free(s);

    return pixbuf;
}

static GdkDrawable*
prepare_drawable(gint width,
                 gint height,
                 gint lw,
                 GdkGC **gc)
{
    GdkWindow *window;
    GdkDrawable *drawable;
    GdkColormap *cmap;
    GdkColor fg;

    /* FIXME: this creates a drawable with *SCREEN* bit depth */
    window = gwy_app_main_window_get()->window;
    drawable = GDK_DRAWABLE(gdk_pixmap_new(GDK_DRAWABLE(window),
                                           width, height, -1));
    cmap = gdk_drawable_get_colormap(drawable);
    *gc = gdk_gc_new(drawable);

    fg.red = 0xffff;
    fg.green = 0xffff;
    fg.blue = 0xffff;
    gdk_colormap_alloc_color(cmap, &fg, FALSE, TRUE);
    gdk_gc_set_foreground(*gc, &fg);
    gdk_draw_rectangle(drawable, *gc, TRUE, 0, 0, width, height);

    fg.red = 0x0000;
    fg.green = 0x0000;
    fg.blue = 0x0000;
    gdk_colormap_alloc_color(cmap, &fg, FALSE, TRUE);
    gdk_gc_set_foreground(*gc, &fg);
    gdk_gc_set_line_attributes(*gc, lw, GDK_LINE_SOLID,
                               GDK_CAP_PROJECTING, GDK_JOIN_BEVEL);

    return drawable;
}

static PangoLayout*
prepare_layout(gdouble zoom)
{
    PangoContext *context;
    PangoFontDescription *fontdesc;
    PangoLayout *layout;

    context = gdk_pango_context_get();
    fontdesc = pango_font_description_from_string("Helvetica 11");
    pango_font_description_set_size(fontdesc, 11*PANGO_SCALE*zoom);
    pango_context_set_font_description(context, fontdesc);
    layout = pango_layout_new(context);
    g_object_unref(context);
    /* FIXME: who frees fontdesc? */

    return layout;
}

/***************************************************************************
 *
 *  sub
 *
 ***************************************************************************/

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

static const gchar *zoom_key = "/module/pixmap/zoom";
static const gchar *otype_key = "/module/pixmap/otype";

static void
pixmap_load_args(GwyContainer *container,
                 PixmapArgs *args)
{
    *args = pixmap_defaults;

    gwy_container_gis_double_by_name(container, zoom_key, &args->zoom);
    gwy_container_gis_int32_by_name(container, otype_key, &args->otype);
}

static void
pixmap_save_args(GwyContainer *container,
                 PixmapArgs *args)
{
    gwy_container_set_double_by_name(container, zoom_key, args->zoom);
    gwy_container_set_int32_by_name(container, otype_key, args->otype);
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
