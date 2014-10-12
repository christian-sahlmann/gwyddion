/*
 *  @(#) $Id$
 *  Copyright (C) 2004-2014 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/**
 * [FILE-MAGIC-USERGUIDE]
 * Pixmap images
 * .png .jpeg .tiff .tga .pnm .bmp
 * Read[1] Export[2]
 * [1] Import support relies on Gdk-Pixbuf and hence may vary among systems.
 * [2] Usually lossy, intended for presentational purposes.  16bit grayscale
 * export is possible to PNG, TIFF and PNM.
 **/

#include "config.h"
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyversion.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libdraw/gwypixfield.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils-file.h>
#include "err.h"
#include "gwytiff.h"
#include "image-keys.h"

enum {
    PREVIEW_SIZE = 240,
};

/* What value is used when importing from image */
typedef enum {
    PIXMAP_MAP_NONE = 0,
    PIXMAP_MAP_RED,
    PIXMAP_MAP_GREEN,
    PIXMAP_MAP_BLUE,
    PIXMAP_MAP_VALUE,
    PIXMAP_MAP_SUM,
    PIXMAP_MAP_ALPHA,
    PIXMAP_MAP_LUMA,
    PIXMAP_MAP_ALL,
    PIXMAP_MAP_LAST
} PixmapMapType;

typedef struct {
    gdouble xreal;
    gdouble yreal;
    gint32 xyexponent;
    gboolean xymeasureeq;
    gchar *xyunit;
    gdouble zreal;
    gint32 zexponent;
    gchar *zunit;
    PixmapMapType maptype;
    GdkPixbuf *pixbuf;
} PixmapLoadArgs;

typedef struct {
    GtkWidget *dialog;
    GdkPixbuf *small_pixbuf;
    GtkWidget *xreal;
    GtkWidget *yreal;
    GtkWidget *xyexponent;
    GtkWidget *xymeasureeq;
    GtkWidget *xyunits;
    GtkWidget *zreal;
    GtkWidget *zexponent;
    GtkWidget *zunits;
    GtkWidget *maptype;
    GtkWidget *view;
    gint xres;
    gint yres;
    PixmapLoadArgs *args;
} PixmapLoadControls;

/* there is a information duplication here,
 * however, we may invent an export format GdkPixbuf cannot load */
typedef struct {
    const gchar *name;
    const gchar *description;
    const gchar *extensions;
    const GdkPixbufFormat *pixbuf_format;
} PixmapFormatInfo;

static gboolean          module_register             (void);
static gint              pixmap_detect               (const GwyFileDetectInfo *fileinfo,
                                                      gboolean only_name,
                                                      const gchar *name);
static GwyContainer*     pixmap_load                 (const gchar *filename,
                                                      GwyRunType mode,
                                                      GError **error,
                                                      const gchar *name);
static void              pixmap_load_set_field       (GwyContainer *container,
                                                      gint id,
                                                      GwyDataField *dfield,
                                                      const PixmapLoadArgs *args,
                                                      const gchar *title);
static void              pixmap_load_pixbuf_to_dfield(GdkPixbuf *pixbuf,
                                                      GwyDataField *dfield,
                                                      PixmapMapType maptype);
static gboolean          pixmap_load_dialog          (PixmapLoadArgs *args,
                                                      const gchar *name,
                                                      gint xres,
                                                      gint yres,
                                                      gboolean mapknown,
                                                      gboolean grayscale,
                                                      gboolean has_alpha);
static void              pixmap_load_create_preview  (PixmapLoadArgs *args,
                                                      PixmapLoadControls *controls);
static void              pixmap_load_map_type_update (GtkWidget *combo,
                                                      PixmapLoadControls *controls);
static void              xyreal_changed_cb           (GtkAdjustment *adj,
                                                      PixmapLoadControls *controls);
static void              xymeasureeq_changed_cb      (PixmapLoadControls *controls);
static void              set_combo_from_unit         (GtkWidget *combo,
                                                      const gchar *str);
static void              units_change_cb             (GtkWidget *button,
                                                      PixmapLoadControls *controls);
static void              pixmap_add_import_log       (GwyContainer *data,
                                                      gint id,
                                                      const gchar *filetype,
                                                      const gchar *filename);
static void              pixmap_load_update_controls (PixmapLoadControls *controls,
                                                      PixmapLoadArgs *args);
static void              pixmap_load_update_values   (PixmapLoadControls *controls,
                                                      PixmapLoadArgs *args);
static PixmapFormatInfo* find_format                 (const gchar *name);
static void              pixmap_load_load_args       (GwyContainer *container,
                                                      PixmapLoadArgs *args);
static void              pixmap_load_save_args       (GwyContainer *container,
                                                      PixmapLoadArgs *args);
static void              pixmap_load_sanitize_args   (PixmapLoadArgs *args);

static const GwyEnum value_map_types[] = {
    { N_("All channels"), PIXMAP_MAP_ALL,   },
    { N_("Red"),          PIXMAP_MAP_RED,   },
    { N_("Green"),        PIXMAP_MAP_GREEN, },
    { N_("Blue"),         PIXMAP_MAP_BLUE,  },
    { N_("Value (max)"),  PIXMAP_MAP_VALUE, },
    { N_("RGB sum"),      PIXMAP_MAP_SUM,   },
    { N_("Luma"),         PIXMAP_MAP_LUMA,  },
    { N_("Alpha"),        PIXMAP_MAP_ALPHA, },
};

static struct {
    const gchar *name;
    const gchar *description;
}
known_formats[] = {
    {
        "png",
        N_("Portable Network Graphics (.png)"),
    },
    {
        "jpeg",
        N_("JPEG (.jpeg,.jpg)"),
    },
    {
        "tiff",
        N_("TIFF (.tiff,.tif)"),
    },
    {
        "pnm",
        N_("Portable Pixmap (.ppm,.pnm)"),
    },
    {
        "bmp",
        N_("Windows or OS2 Bitmap (.bmp)"),
    },
    {
        "tga",
        N_("TARGA (.tga,.targa)"),
    },
    {
        "gif",
        N_("Graphics Interchange Format GIF (.gif)"),
    },
    {
        "jpeg2000",
        N_("JPEG 2000 (.jpx)"),
    },
    {
        "pcx",
        N_("PCX (.pcx)"),
    },
    {
        "xpm",
        N_("X Pixmap (.xpm)"),
    },
    {
        "ras",
        N_("Sun raster image (.ras)"),
    },
    {
        "icns",
        N_("Apple icon (.icns)"),
    },
};

/* List of PixmapFormatInfo for all formats.  This is never freed */
static GSList *pixmap_formats = NULL;

static const PixmapLoadArgs pixmap_load_defaults = {
    100.0, 100.0, -6, TRUE, "m", 1.0, -6, "m", PIXMAP_MAP_VALUE, NULL
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports data from low-depth pixmap images (PNG, TIFF, JPEG, ...).  "
       "The set of available formats depends on available GDK pixbuf "
       "loaders."),
    "Yeti <yeti@gwyddion.net>",
    "8.0",
    "David Nečas (Yeti)",
    "2004-2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    PixmapFormatInfo *format_info;
    GSList *formats, *l;
    guint i;

    formats = gdk_pixbuf_get_formats();
    for (l = formats; l; l = g_slist_next(l)) {
        GdkPixbufFormat *pixbuf_format = (GdkPixbufFormat*)l->data;
        gchar *s, **ext;
        gchar *fmtname;

        /* Ignore all vector formats */
        fmtname = gdk_pixbuf_format_get_name(pixbuf_format);
        gwy_debug("Found format %s", fmtname);
        if (gdk_pixbuf_format_is_scalable(pixbuf_format)) {
            gwy_debug("Ignoring scalable GdkPixbuf format %s.", fmtname);
            continue;
        }

        /* Use a whitelist of safe formats, namely those with an explicit
         * detection in pixmap_detect().  GdkPixbuf loaders tend to accept
         * any rubbish as their format and then crash because it isn't. */
        if (!gwy_stramong(fmtname,
                          "bmp", "gif", "icns", "jpeg", "jpeg2000", "pcx",
                          "png", "pnm", "ras", "tga", "tiff", "xpm", NULL)) {
            gwy_debug("Ignoring GdkPixbuf format %s because it is not on "
                      "the whitelist.", fmtname);
            continue;
        }

        format_info = g_new0(PixmapFormatInfo, 1);
        format_info->name = fmtname;
        format_info->pixbuf_format = pixbuf_format;

        gwy_debug("Found GdkPixbuf loader for new type: %s", fmtname);
        format_info->description
            = gdk_pixbuf_format_get_description(pixbuf_format);
        ext = gdk_pixbuf_format_get_extensions(pixbuf_format);
        s = g_strjoinv(",.", ext);
        format_info->extensions = g_strconcat(".", s, NULL);
        g_free(s);
        g_strfreev(ext);

        /* Fix silly descriptions starting `The image format...' or
         * something like that that are unusable for a sorted list. */
        for (i = 0; i < G_N_ELEMENTS(known_formats); i++) {
            if (gwy_strequal(fmtname, known_formats[i].name)) {
                gwy_debug("Fixing the description of known type: %s",
                          fmtname);
                format_info->description = known_formats[i].description;
                break;
            }
        }
        gwy_file_func_register(format_info->name,
                               format_info->description,
                               &pixmap_detect,
                               &pixmap_load,
                               NULL,
                               NULL);
        pixmap_formats = g_slist_append(pixmap_formats, format_info);
    }

    g_slist_free(formats);

    return TRUE;
}

static gint
pixmap_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name,
              const gchar *name)
{
    GdkPixbufLoader *loader;
    GError *err = NULL;
    PixmapFormatInfo *format_info;
    gint score;
    gchar **extensions;
    guint ext;

    gwy_debug("Running detection for file type %s", name);

    format_info = find_format(name);
    g_return_val_if_fail(format_info, 0);

    extensions = g_strsplit(format_info->extensions, ",", 0);
    g_assert(extensions);
    for (ext = 0; extensions[ext]; ext++) {
        if (g_str_has_suffix(fileinfo->name_lowercase, extensions[ext]))
            break;
    }
    score = extensions[ext] ? 19 : 0;
    g_strfreev(extensions);
    if (only_name) /* || !score)*/
        return score;

    /* FIXME: this is incorrect, but no one is going to import data from such
     * a small valid image anyway */
    if (fileinfo->buffer_len < 64)
        return 0;

    /* GdkPixbuf does a terrible job regarding detection so we do some sanity
     * check ourselves */
    score = 70;
    if (gwy_strequal(name, "png")) {
        if (memcmp(fileinfo->head, "\x89PNG\r\n\x1a\n", 8) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "bmp")) {
        if (strncmp(fileinfo->head, "BM", 2) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "pnm")) {
        if (fileinfo->head[0] != 'P' || !g_ascii_isdigit(fileinfo->head[1]))
            return 0;
    }
    else if (gwy_strequal(name, "xpm")) {
        if (strncmp(fileinfo->head, "/* XPM */", 9) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "tiff")) {
        /* The pixbuf loader is unlikely to load BigTIFFs any time soon. */
        GwyTIFFVersion version = GWY_TIFF_CLASSIC;
#ifdef __WIN64
        /* The TIFF loader (supposedly GDI-based) crashes on Win64.  Unclear
         * why.  TIFF is madness.  Note there is a fallback GwyTIFF loader in
         * hdrimage which will take over when we do this.  */
        return 0;
#else
        gwy_debug("Checking TIFF header");
        if (!gwy_tiff_detect(fileinfo->head, fileinfo->buffer_len,
                             &version, NULL))
            return 0;
        gwy_debug("TIFF header OK (type %.2s)", fileinfo->head);
#endif
    }
    else if (gwy_strequal(name, "jpeg")) {
        if (memcmp(fileinfo->head, "\xff\xd8", 2) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "pcx")) {
        if (fileinfo->head[0] != '\x0a' || fileinfo->head[1] > 0x05)
            return 0;
    }
    else if (gwy_strequal(name, "gif")) {
        if (strncmp(fileinfo->head, "GIF8", 4) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "ras")) {
        if (memcmp(fileinfo->head, "\x59\xa6\x6a\x95", 4) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "icns")) {
        if (memcmp(fileinfo->head, "icns", 4) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "jpeg2000")) {
        if (memcmp(fileinfo->head, "\x00\x00\x00\x0C\x6A\x50\x20\x20\x0D\x0A\x87\x0A\x00\x00\x00\x14\x66\x74\x79\x70\x6A\x70\x32", 23) != 0)
            return 0;
    }
    else if (gwy_strequal(name, "tga")) {
        guint8 cmtype = fileinfo->head[1];
        guint8 dtype = fileinfo->head[2];

        if (dtype == 1 || dtype == 9 || dtype == 32 || dtype == 33) {
            if (cmtype != 1)
                return 0;
        }
        else if (dtype == 2 || dtype == 3 || dtype == 10 || dtype == 11) {
            if (cmtype != 0)
                return 0;
        }
        else
            return 0;
    }
    else {
        /* Assign lower score to loaders we found by trying if they accept
         * the header because they often have no clue. */
        score = 55;
    }

    gwy_debug("Creating a loader for type %s", name);
    loader = gdk_pixbuf_loader_new_with_type(name, NULL);
    gwy_debug("Loader for type %s: %p", name, loader);
    if (!loader)
        return 0;

    /* The TIFF loaders (both libTIFF and GDI-based) seem to crash on broken
     * TIFFs a way too often.  Do not try to feed anything to it just accept
     * the file is a TIFF and hope some other loader of a TIFF-based format
     * will claim it with a higher score. */
    if (gwy_strequal(name, "tiff")) {
        gwy_debug("Avoiding feeding data to TIFF loader, calling "
                  "gdk_pixbuf_loader_close().");
        gdk_pixbuf_loader_close(loader, NULL);
        gwy_debug("Unreferencing the TIFF loader");
        g_object_unref(loader);
        gwy_debug("Returning score %d for TIFF", score - 10);
        return score - 10;
    }

    /* For sane readers, try to feed the start of the file and see if it fails.
     * Success rarely means anything though. */
    if (!gdk_pixbuf_loader_write(loader,
                                 fileinfo->head, fileinfo->buffer_len, &err)) {
        gwy_debug("%s", err->message);
        g_clear_error(&err);
        score = 0;
    }
    gdk_pixbuf_loader_close(loader, NULL);
    g_object_unref(loader);

    return score;
}

static GwyContainer*
pixmap_load(const gchar *filename,
            GwyRunType mode,
            GError **error,
            const gchar *name)
{
    enum { buffer_length = 4096, nmaptypes = G_N_ELEMENTS(value_map_types) };
    guchar pixmap_buf[buffer_length];
    PixmapFormatInfo *format_info;
    GdkPixbufLoader *loader;
    GwyDataField *dfield;
    GwyContainer *data, *settings;
    GdkPixbuf *pixbuf;
    GError *err = NULL;
    FILE *fh;
    guint n, bpp;
    guchar *pixels, *p;
    gint i, j, width, height, rowstride;
    gboolean has_alpha, maptype_known, ok;
    gint not_grayscale, any_red, any_green, any_blue, alpha_important;
    PixmapLoadArgs args;

    gwy_debug("Loading <%s> as %s", filename, name);

    format_info = find_format(name);
    if (!format_info) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_UNIMPLEMENTED,
                    _("Pixmap has not registered file type `%s'."), name);
        return NULL;
    }

    if (!(fh = g_fopen(filename, "rb"))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Cannot open file for reading: %s."), g_strerror(errno));
        return NULL;
    }

    gwy_debug("Creating a loader for type %s", name);
    loader = gdk_pixbuf_loader_new_with_type(name, &err);
    if (!loader) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Cannot get pixbuf loader: %s."), err->message);
        g_clear_error(&err);
        fclose(fh);
        return NULL;
    }

    gwy_debug("Reading file content.");
    do {
        n = fread(pixmap_buf, 1, buffer_length, fh);
        gwy_debug("loaded %u bytes", n);
        if (!gdk_pixbuf_loader_write(loader, pixmap_buf, n, &err)) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Pixbuf loader refused data: %s."), err->message);
            g_clear_error(&err);
            g_object_unref(loader);
            fclose(fh);
            return NULL;
        }
    } while (n == buffer_length);
    fclose(fh);

    gwy_debug("Closing the loader.");
    if (!gdk_pixbuf_loader_close(loader, &err)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_DATA,
                    _("Pixbuf loader refused data: %s."), err->message);
        g_clear_error(&err);
        g_object_unref(loader);
        return NULL;
    }

    gwy_debug("Trying to get the pixbuf.");
    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    gwy_debug("Pixbuf is: %p.", pixbuf);
    g_assert(pixbuf);
    g_object_ref(pixbuf);
    gwy_debug("Finalizing loader.");
    g_object_unref(loader);

    settings = gwy_app_settings_get();
    pixmap_load_load_args(settings, &args);
    args.pixbuf = pixbuf;

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    bpp = has_alpha ? 4 : 3;
    /* check which value mapping methods seem feasible */
    not_grayscale = any_red = any_green = any_blue = alpha_important = 0;
    for (i = 0; i < height; i++) {
        p = pixels + i*rowstride;
        for (j = 0; j < width; j++) {
            guchar red = p[bpp*j], green = p[bpp*j+1], blue = p[bpp*j+2];

            not_grayscale |= (green ^ red) | (red ^ blue);
            any_green |= green;
            any_blue |= blue;
            any_red |= red;
            if (has_alpha)
                alpha_important |= 0xff ^ p[bpp*j+3];
        }
    }
    if (!has_alpha && args.maptype == PIXMAP_MAP_ALPHA)
        args.maptype = pixmap_load_defaults.maptype;

    maptype_known = FALSE;
    if (alpha_important) {
        args.maptype = PIXMAP_MAP_ALPHA;
    }
    else if (!not_grayscale) {
        args.maptype = PIXMAP_MAP_VALUE;
        maptype_known = TRUE;
    }
    else if (!any_green && !any_blue) {
        args.maptype = PIXMAP_MAP_RED;
        maptype_known = TRUE;
    }
    else if (!any_red && !any_blue) {
        args.maptype = PIXMAP_MAP_GREEN;
        maptype_known = TRUE;
    }
    else if (!any_red && !any_green) {
        args.maptype = PIXMAP_MAP_BLUE;
        maptype_known = TRUE;
    }

    /* ask user what she thinks */
    if (mode == GWY_RUN_INTERACTIVE) {
        gwy_debug("Manual import is necessary.");

        ok = pixmap_load_dialog(&args, name, width, height,
                                maptype_known, !not_grayscale, alpha_important);
        pixmap_load_save_args(settings, &args);
        if (!ok) {
            err_CANCELLED(error);
            g_object_unref(pixbuf);
            g_free(args.xyunit);
            g_free(args.zunit);
            return NULL;
        }
    }
    else {
        gwy_debug("Running non-interactively, reusing the last parameters.");
        /* Nothing to do here; the args have been already loaded. */
    }

    data = gwy_container_new();

    if (args.maptype == PIXMAP_MAP_ALL) {
        for (i = 0; i < bpp; i++) {
            PixmapMapType maptype = (i < 3) ? i + 1 : 6;
            dfield = gwy_data_field_new(width, height, args.xreal, args.yreal,
                                        FALSE);
            pixmap_load_pixbuf_to_dfield(pixbuf, dfield, maptype);
            pixmap_load_set_field(data, i, dfield, &args,
                                  gwy_enum_to_string(maptype,
                                                     value_map_types,
                                                     nmaptypes));
            g_object_unref(dfield);
            pixmap_add_import_log(data, i, name, filename);
        }
    }
    else {
        dfield = gwy_data_field_new(width, height, args.xreal, args.yreal,
                                    FALSE);
        pixmap_load_pixbuf_to_dfield(pixbuf, dfield, args.maptype);
        pixmap_load_set_field(data, 0, dfield, &args,
                              gwy_enum_to_string(args.maptype,
                                                 value_map_types, nmaptypes));
        g_object_unref(dfield);
        pixmap_add_import_log(data, 0, name, filename);
    }

    g_object_unref(pixbuf);
    g_free(args.xyunit);
    g_free(args.zunit);

    return data;
}

static void
pixmap_load_set_field(GwyContainer *container,
                      gint id,
                      GwyDataField *dfield,
                      const PixmapLoadArgs *args,
                      const gchar *title)
{
    GwySIUnit *siunit;
    GQuark quark;
    gchar *key;

    gwy_data_field_set_xreal(dfield, args->xreal*pow10(args->xyexponent));
    gwy_data_field_set_yreal(dfield, args->yreal*pow10(args->xyexponent));
    gwy_data_field_multiply(dfield, args->zreal*pow10(args->zexponent));
    siunit = gwy_si_unit_new(args->xyunit);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);
    siunit = gwy_si_unit_new(args->zunit);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    quark = gwy_app_get_data_key_for_id(id);
    gwy_container_set_object(container, quark, dfield);

    key = g_strdup_printf("%s/title", g_quark_to_string(quark));
    gwy_container_set_string_by_name(container, key, g_strdup(title));
    g_free(key);
}

static void
pixmap_load_pixbuf_to_dfield(GdkPixbuf *pixbuf,
                             GwyDataField *dfield,
                             PixmapMapType maptype)
{
    gint width, height, rowstride, i, j, bpp;
    guchar *pixels, *p;
    gdouble *val, *r;

    gwy_debug("%d", maptype);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    bpp = gdk_pixbuf_get_has_alpha(pixbuf) ? 4 : 3;
    gwy_data_field_resample(dfield, width, height, GWY_INTERPOLATION_NONE);
    val = gwy_data_field_get_data(dfield);

    for (i = 0; i < height; i++) {
        p = pixels + i*rowstride;
        r = val + i*width;

        switch (maptype) {
            case PIXMAP_MAP_ALPHA:
            p++;
            case PIXMAP_MAP_BLUE:
            p++;
            case PIXMAP_MAP_GREEN:
            p++;
            case PIXMAP_MAP_RED:
            for (j = 0; j < width; j++)
                r[j] = p[bpp*j]/255.0;
            break;

            case PIXMAP_MAP_VALUE:
            for (j = 0; j < width; j++) {
                guchar red = p[bpp*j], green = p[bpp*j+1], blue = p[bpp*j+2];
                guchar v = MAX(red, green);

                r[j] = MAX(v, blue)/255.0;
            }
            break;

            case PIXMAP_MAP_SUM:
            for (j = 0; j < width; j++) {
                guchar red = p[bpp*j], green = p[bpp*j+1], blue = p[bpp*j+2];

                r[j] = (red + green + blue)/(3*255.0);
            }
            break;

            case PIXMAP_MAP_LUMA:
            for (j = 0; j < width; j++) {
                guchar red = p[bpp*j], green = p[bpp*j+1], blue = p[bpp*j+2];

                r[j] = (0.2126*red + 0.7152*green + 0.0722*blue)/255.0;
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }
}

static const gchar*
describe_channels(gboolean grayscale, gboolean has_alpha)
{
    if (grayscale)
        return has_alpha ? "GA" : "G";
    else
        return has_alpha ? "RGBA" : "RGB";
}

static gboolean
pixmap_load_dialog(PixmapLoadArgs *args,
                   const gchar *name,
                   gint xres,
                   gint yres,
                   gboolean mapknown,
                   gboolean grayscale,
                   gboolean has_alpha)
{
    enum { RESPONSE_RESET = 1 };

    PixmapLoadControls controls;
    GwyContainer *data;
    GwyPixmapLayer *layer;
    GtkObject *adj;
    GtkAdjustment *adj2;
    GtkWidget *dialog, *table, *label, *align, *button, *hbox, *hbox2;
    GtkSizeGroup *sizegroup;
    GwySIUnit *unit;
    gint response;
    gchar *s, *title;
    gdouble zoom;
    gchar buf[16];
    gint row, n;

    controls.args = args;
    controls.xres = xres;
    controls.yres = yres;

    sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    s = g_ascii_strup(name, -1);
    /* TRANSLATORS: Dialog title; %s is PNG, TIFF, ... */
    title = g_strdup_printf(_("Import %s"), s);
    g_free(s);
    dialog = gtk_dialog_new_with_buttons(title, NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_file_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    g_free(title);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    table = gtk_table_new(4, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_add(GTK_CONTAINER(align), table);
    row = 0;

    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Image Information")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    g_snprintf(buf, sizeof(buf), "%u", xres);
    label = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gwy_table_attach_row(table, row++, _("Horizontal size:"), _("px"),
                         label);

    g_snprintf(buf, sizeof(buf), "%u", yres);
    label = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gwy_table_attach_row(table, row++, _("Vertical size:"), _("px"),
                         label);

    label = gtk_label_new(describe_channels(grayscale, has_alpha));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gwy_table_attach_row(table, row++, _("Channels:"), NULL,
                         label);

    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    zoom = PREVIEW_SIZE/(gdouble)MAX(xres, yres);
    controls.small_pixbuf = gdk_pixbuf_scale_simple(args->pixbuf,
                                                    MAX(zoom*xres, 1),
                                                    MAX(zoom*yres, 1),
                                                    GDK_INTERP_TILES);
    gwy_debug_objects_creation(G_OBJECT(controls.small_pixbuf));
    data = gwy_container_new();
    controls.view = gwy_data_view_new(data);
    g_object_unref(data);
    pixmap_load_create_preview(args, &controls);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gtk_container_add(GTK_CONTAINER(align), controls.view);

    table = gtk_table_new(6, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 0);
    row = 0;

    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Physical Dimensions")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    adj = gtk_adjustment_new(args->xreal, 0.01, 10000, 1, 100, 0);
    controls.xreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.xreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.xreal,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("_Width:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.xreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    align = gtk_alignment_new(0.0, 0.5, 1.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), align, 2, 3, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_container_add(GTK_CONTAINER(align), hbox2);

    unit = gwy_si_unit_new(args->xyunit);
    controls.xyexponent = gwy_combo_box_metric_unit_new(NULL, NULL,
                                                        args->xyexponent - 6,
                                                        args->xyexponent + 6,
                                                        unit,
                                                        args->xyexponent);
    gtk_size_group_add_widget(sizegroup, controls.xyexponent);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.xyexponent, FALSE, FALSE, 0);

    controls.xyunits = gtk_button_new_with_label(gwy_sgettext("verb|Change"));
    g_object_set_data(G_OBJECT(controls.xyunits), "id", (gpointer)"xy");
    g_signal_connect(controls.xyunits, "clicked",
                     G_CALLBACK(units_change_cb), &controls);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.xyunits, FALSE, FALSE, 0);
    row++;

    adj = gtk_adjustment_new(args->yreal, 0.01, 10000, 1, 100, 0);
    controls.yreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.yreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.yreal,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("H_eight:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.yreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Identical _measures"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls.xymeasureeq = button;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    adj = gtk_adjustment_new(args->zreal, 0.01, 10000, 1, 100, 0);
    controls.zreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.zreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.zreal,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("_Z-scale (per sample unit):"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.zreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    align = gtk_alignment_new(0.0, 0.5, 1.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), align, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_container_add(GTK_CONTAINER(align), hbox2);

    gwy_si_unit_set_from_string(unit, args->zunit);
    controls.zexponent = gwy_combo_box_metric_unit_new(NULL, NULL,
                                                       args->zexponent - 6,
                                                       args->zexponent + 6,
                                                       unit,
                                                       args->zexponent);
    gtk_size_group_add_widget(sizegroup, controls.zexponent);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.zexponent, FALSE, FALSE, 0);
    g_object_unref(unit);

    controls.zunits = gtk_button_new_with_label(gwy_sgettext("verb|Change"));
    g_object_set_data(G_OBJECT(controls.zunits), "id", (gpointer)"z");
    g_signal_connect(controls.zunits, "clicked",
                     G_CALLBACK(units_change_cb), &controls);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.zunits, FALSE, FALSE, 0);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    if (!mapknown) {
        label = gtk_label_new(_("Warning: Colorful images cannot be reliably "
                                "mapped to meaningful values."));
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                         GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
        gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
        row++;

        n = G_N_ELEMENTS(value_map_types);
        if (!gdk_pixbuf_get_has_alpha(args->pixbuf))
            n--;

        controls.maptype
            = gwy_enum_combo_box_new(value_map_types, n,
                                     G_CALLBACK(pixmap_load_map_type_update),
                                     &controls,
                                     args->maptype, TRUE);
        gwy_table_attach_row(table, row++,
                             gwy_sgettext("verb|Use"), _("as data"),
                             controls.maptype);
    }
    else
        controls.maptype = NULL;

    g_signal_connect_swapped(controls.xymeasureeq, "toggled",
                             G_CALLBACK(xymeasureeq_changed_cb), &controls);
    adj2 = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls.xreal));
    g_signal_connect(adj2, "value-changed",
                     G_CALLBACK(xyreal_changed_cb), &controls);
    adj2 = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls.yreal));
    g_signal_connect(adj2, "value-changed",
                     G_CALLBACK(xyreal_changed_cb), &controls);
    pixmap_load_update_controls(&controls, args);

    g_object_unref(sizegroup);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            pixmap_load_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            g_object_unref(controls.small_pixbuf);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            args->xreal = pixmap_load_defaults.xreal;
            args->yreal = pixmap_load_defaults.yreal;
            args->xyexponent = pixmap_load_defaults.xyexponent;
            args->xymeasureeq = pixmap_load_defaults.xymeasureeq;
            g_free(args->xyunit);
            args->xyunit = g_strdup(pixmap_load_defaults.xyunit);
            args->zreal = pixmap_load_defaults.zreal;
            args->zexponent = pixmap_load_defaults.zexponent;
            g_free(args->zunit);
            args->zunit = g_strdup(pixmap_load_defaults.zunit);
            args->maptype = pixmap_load_defaults.maptype;
            pixmap_load_update_controls(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    pixmap_load_update_values(&controls, args);
    gtk_widget_destroy(dialog);
    g_object_unref(controls.small_pixbuf);

    return TRUE;
}

static void
pixmap_load_create_preview(PixmapLoadArgs *args,
                           PixmapLoadControls *controls)
{
    PixmapMapType maptype = args->maptype;
    GwyContainer *data;
    GwyDataField *dfield;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    if (!gwy_container_gis_object_by_name(data, "/0/data", &dfield)) {
        dfield = gwy_data_field_new(1, 1, 1.0, 1.0, FALSE);
        gwy_container_set_object_by_name(data, "/0/data", dfield);
        g_object_unref(dfield);
    }
    if (maptype == PIXMAP_MAP_ALL)
        maptype = PIXMAP_MAP_RED;
    pixmap_load_pixbuf_to_dfield(controls->small_pixbuf, dfield, maptype);
    gwy_data_field_data_changed(dfield);
}

static void
pixmap_load_map_type_update(GtkWidget *combo,
                            PixmapLoadControls *controls)
{

    controls->args->maptype
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    pixmap_load_create_preview(controls->args, controls);
}

static void
pixmap_load_update_controls(PixmapLoadControls *controls,
                            PixmapLoadArgs *args)
{
    GtkAdjustment *adj;

    /* TODO: Units */
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    gtk_adjustment_set_value(adj, args->xreal);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    gtk_adjustment_set_value(adj, args->yreal);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->xymeasureeq),
                                 args->xymeasureeq);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->xyexponent),
                                   args->xyexponent);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->zreal));
    gtk_adjustment_set_value(adj, args->zreal);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->zexponent),
                                  args->zexponent);
    if (controls->maptype)
        gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->maptype),
                                      args->maptype);
}

static void
pixmap_load_update_values(PixmapLoadControls *controls,
                          PixmapLoadArgs *args)
{
    GtkAdjustment *adj;

    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    args->xreal = gtk_adjustment_get_value(adj);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    args->yreal = gtk_adjustment_get_value(adj);
    args->xyexponent
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->xyexponent));
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->zreal));
    args->zreal = gtk_adjustment_get_value(adj);
    args->zexponent
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->zexponent));
    if (controls->maptype)
        args->maptype
            = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->maptype));
}

static void
xyreal_changed_cb(GtkAdjustment *adj,
                  PixmapLoadControls *controls)
{
    static gboolean in_update = FALSE;
    GtkAdjustment *xadj, *yadj;
    gdouble value;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->xymeasureeq))
        || in_update)
        return;

    value = gtk_adjustment_get_value(adj);
    xadj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    yadj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    in_update = TRUE;
    if (xadj == adj)
        gtk_adjustment_set_value(yadj, value*controls->yres/controls->xres);
    else
        gtk_adjustment_set_value(xadj, value*controls->xres/controls->yres);
    in_update = FALSE;
}

static void
xymeasureeq_changed_cb(PixmapLoadControls *controls)
{
    GtkAdjustment *xadj, *yadj;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->xymeasureeq)))
        return;

    xadj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    yadj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    gtk_adjustment_set_value(yadj,
                             gtk_adjustment_get_value(xadj)
                             *controls->yres/controls->xres);
}

static void
set_combo_from_unit(GtkWidget *combo,
                    const gchar *str)
{
    GwySIUnit *unit;
    gint power10;

    unit = gwy_si_unit_new_parse(str, &power10);
    gwy_combo_box_metric_unit_set_unit(GTK_COMBO_BOX(combo),
                                       power10 - 6, power10 + 6, unit);
    g_object_unref(unit);
}

static void
units_change_cb(GtkWidget *button,
                PixmapLoadControls *controls)
{
    GtkWidget *dialog, *hbox, *label, *entry;
    const gchar *id, *unit;
    gint response;

    pixmap_load_update_values(controls, controls->args);
    id = g_object_get_data(G_OBJECT(button), "id");
    dialog = gtk_dialog_new_with_buttons(_("Change Units"),
                                         GTK_WINDOW(controls->dialog),
                                         GTK_DIALOG_MODAL
                                         | GTK_DIALOG_NO_SEPARATOR,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("New _units:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    entry = gtk_entry_new();
    if (gwy_strequal(id, "xy"))
        gtk_entry_set_text(GTK_ENTRY(entry), controls->args->xyunit);
    else if (gwy_strequal(id, "z"))
        gtk_entry_set_text(GTK_ENTRY(entry), controls->args->zunit);
    else
        g_return_if_reached();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    unit = gtk_entry_get_text(GTK_ENTRY(entry));
    if (gwy_strequal(id, "xy")) {
        set_combo_from_unit(controls->xyexponent, unit);
        g_free(controls->args->xyunit);
        controls->args->xyunit = g_strdup(unit);
    }
    else if (gwy_strequal(id, "z")) {
        set_combo_from_unit(controls->zexponent, unit);
        g_free(controls->args->zunit);
        controls->args->zunit = g_strdup(unit);
    }

    gtk_widget_destroy(dialog);
}

static void
pixmap_add_import_log(GwyContainer *data,
                      gint id,
                      const gchar *filetype,
                      const gchar *filename)
{
    GwyContainer *settings;
    GQuark quark;
    gchar *myfilename = NULL, *fskey, *qualname;

    g_return_if_fail(filename);
    g_return_if_fail(filetype);
    g_return_if_fail(data);

    if (g_utf8_validate(filename, -1, NULL))
        myfilename = g_strdup(filename);
    if (!myfilename)
        myfilename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
    if (!myfilename)
        myfilename = g_strescape(filename, NULL);

    fskey = g_strdup_printf("/module/%s/filename", filetype);
    quark = g_quark_from_string(fskey);
    g_free(fskey);

    /* Eats myfilename. */
    settings = gwy_app_settings_get();
    gwy_container_set_string(settings, quark, myfilename);

    qualname = g_strconcat("file::", filetype, NULL);
    gwy_app_channel_log_add(data, -1, id, qualname, NULL);
    g_free(qualname);

    /* We know pixmap functions have no such setting as "filename". */
    gwy_container_remove(settings, quark);
}

static PixmapFormatInfo*
find_format(const gchar *name)
{
    GSList *l;

    for (l = pixmap_formats; l; l = g_slist_next(l)) {
        PixmapFormatInfo *format_info = (PixmapFormatInfo*)l->data;
        if (gwy_strequal(format_info->name, name))
            return format_info;
    }

    return NULL;
}

static const gchar xreal_key[]       = "/module/pixmap/xreal";
static const gchar yreal_key[]       = "/module/pixmap/yreal";
static const gchar xyexponent_key[]  = "/module/pixmap/xyexponent";
static const gchar xymeasureeq_key[] = "/module/pixmap/xymeasureeq";
static const gchar xyunit_key[]      = "/module/pixmap/xyunit";
static const gchar zreal_key[]       = "/module/pixmap/zreal";
static const gchar zexponent_key[]   = "/module/pixmap/zexponent";
static const gchar zunit_key[]       = "/module/pixmap/zunit";
static const gchar maptype_key[]     = "/module/pixmap/maptype";

static void
pixmap_load_sanitize_args(PixmapLoadArgs *args)
{
    args->maptype = MIN(args->maptype, PIXMAP_MAP_LAST-1);
    args->xreal = CLAMP(args->xreal, 0.01, 10000.0);
    args->yreal = CLAMP(args->yreal, 0.01, 10000.0);
    args->zreal = CLAMP(args->zreal, 0.01, 10000.0);
    args->xyexponent = CLAMP(args->xyexponent, -12, 3);
    args->zexponent = CLAMP(args->zexponent, -12, 3);
    args->xymeasureeq = !!args->xymeasureeq;
}

static void
pixmap_load_load_args(GwyContainer *container,
                      PixmapLoadArgs *args)
{
    *args = pixmap_load_defaults;

    gwy_container_gis_double_by_name(container, xreal_key, &args->xreal);
    gwy_container_gis_double_by_name(container, yreal_key, &args->yreal);
    gwy_container_gis_int32_by_name(container, xyexponent_key,
                                    &args->xyexponent);
    gwy_container_gis_double_by_name(container, zreal_key, &args->zreal);
    gwy_container_gis_int32_by_name(container, zexponent_key,
                                    &args->zexponent);
    gwy_container_gis_enum_by_name(container, maptype_key, &args->maptype);
    gwy_container_gis_boolean_by_name(container, xymeasureeq_key,
                                      &args->xymeasureeq);
    gwy_container_gis_string_by_name(container, xyunit_key,
                                     (const guchar**)&args->xyunit);
    gwy_container_gis_string_by_name(container, zunit_key,
                                     (const guchar**)&args->zunit);

    args->xyunit = g_strdup(args->xyunit);
    args->zunit = g_strdup(args->zunit);

    pixmap_load_sanitize_args(args);
}

static void
pixmap_load_save_args(GwyContainer *container,
                      PixmapLoadArgs *args)
{
    gwy_container_set_double_by_name(container, xreal_key, args->xreal);
    gwy_container_set_double_by_name(container, yreal_key, args->yreal);
    gwy_container_set_int32_by_name(container, xyexponent_key,
                                    args->xyexponent);
    gwy_container_set_double_by_name(container, zreal_key, args->zreal);
    gwy_container_set_int32_by_name(container, zexponent_key,
                                    args->zexponent);
    gwy_container_set_enum_by_name(container, maptype_key, args->maptype);
    gwy_container_set_boolean_by_name(container, xymeasureeq_key,
                                      args->xymeasureeq);
    gwy_container_set_string_by_name(container, xyunit_key,
                                     g_strdup(args->xyunit));
    gwy_container_set_string_by_name(container, zunit_key,
                                     g_strdup(args->zunit));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
