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

#include <libgwyddion/gwymacros.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <gdk/gdk.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#ifdef HAVE_TIFFIO_H
#  ifdef HAVE_LIBTIFF
#    include <tiffio.h>
#    define HAVE_TIFF
#  endif
#endif

/* FIXME: TIFF breaks badly on Win32, nooone knows why */
#ifdef G_OS_WIN32
#  undef HAVE_TIFF
#endif

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define BITS_PER_SAMPLE 8

#define TICK_LENGTH 10

#define GWY_PNG_EXTENSIONS ".png"
#define GWY_JPEG_EXTENSIONS ".jpeg,.jpg,.jpe"
#define GWY_TIFF_EXTENSIONS ".tiff,.tif"
#define GWY_PPM_EXTENSIONS ".ppm,.pnm"
#define GWY_BMP_EXTENSIONS ".bmp"
#define GWY_TARGA_EXTENSIONS ".tga,.targa"

#define ZOOM2LW(x) ((x) > 1 ? ((x) + 0.4) : 1)

enum { pixmap_ping_length = 4096 };

static guchar pixmap_ping_buf[pixmap_ping_length];

/* What is present on the exported image */
typedef enum {
    PIXMAP_RAW_DATA,
    PIXMAP_RULERS,
    PIXMAP_EVERYTHING,
    PIXMAP_LAST
} PixmapOutput;

/* What value is used when importing from image */
typedef enum {
    PIXMAP_MAP_NONE = 0,
    PIXMAP_MAP_RED,
    PIXMAP_MAP_GREEN,
    PIXMAP_MAP_BLUE,
    PIXMAP_MAP_VALUE,
    PIXMAP_MAP_SUM,
    PIXMAP_MAP_LAST
} PixmapMapType;

typedef struct {
    gdouble zoom;
    PixmapOutput otype;
} PixmapSaveArgs;

typedef struct {
    gdouble xreal;
    gdouble yreal;
    gint32 xyexponent;
    gboolean xymeasureeq;
    gdouble zreal;
    gint32 zexponent;
    PixmapMapType maptype;
    GdkPixbuf *pixbuf;
} PixmapLoadArgs;

typedef struct {
    GSList *group;
    GtkWidget *image;
    GwyContainer *data;
    PixmapSaveArgs *args;
} PixmapSaveControls;

typedef struct {
    GtkWidget *xreal;
    GtkWidget *yreal;
    GtkWidget *xyexponent;
    GtkWidget *xymeasureeq;
    GtkWidget *zreal;
    GtkWidget *zexponent;
    GtkWidget *maptype;
    GtkWidget *image;
    gint xres;
    gint yres;
    PixmapLoadArgs *args;
} PixmapLoadControls;

/* there is a information duplication here,
 * however, we may invent an export format GdkPixbuf cannot load */
typedef struct {
    const gchar *extensions;
    GwyFileFuncInfo *func_info;
    const GdkPixbufFormat *pixbuf_format;
} PixmapFormatInfo;

static gboolean          module_register           (const gchar *name);
static gint              pixmap_detect             (const gchar *filename,
                                                    gboolean only_name,
                                                    const gchar *name);
static GwyContainer*     pixmap_load               (const gchar *filename,
                                                    const gchar *name);
static gboolean          pixmap_load_dialog        (PixmapLoadArgs *args,
                                                    const gchar *name,
                                                    gint xres,
                                                    gint yres,
                                                    const gboolean mapknown);
static void              pixmap_load_create_preview(PixmapLoadArgs *args,
                                                    PixmapLoadControls *controls);
static void              pixmap_load_map_type_update(GtkWidget *button,
                                                     PixmapLoadControls *controls);
static void              xyreal_changed_cb         (GtkAdjustment *adj,
                                                    PixmapLoadControls *controls);
static void              xymeasureeq_changed_cb    (PixmapLoadControls *controls);
static void              pixmap_load_update_controls(PixmapLoadControls *controls,
                                                    PixmapLoadArgs *args);
static void              pixmap_load_update_values (PixmapLoadControls *controls,
                                                    PixmapLoadArgs *args);
static GtkWidget*        table_attach_heading      (GtkWidget *table,
                                                    const gchar *text,
                                                    gint row);
static GdkPixbuf*        pixmap_draw_pixbuf        (GwyContainer *data,
                                                    const gchar *format_name);
static GdkPixbuf*        pixmap_real_draw_pixbuf   (GwyContainer *data,
                                                    PixmapSaveArgs *args);
static gboolean          pixmap_save_dialog        (GwyContainer *data,
                                                    PixmapSaveArgs *args,
                                                    const gchar *name);
static gboolean          pixmap_save_png           (GwyContainer *data,
                                                    const gchar *filename);
static gboolean          pixmap_save_jpeg          (GwyContainer *data,
                                                    const gchar *filename);
#ifdef HAVE_TIFF
static gboolean          pixmap_save_tiff          (GwyContainer *data,
                                                    const gchar *filename);
#endif
static gboolean          pixmap_save_ppm           (GwyContainer *data,
                                                    const gchar *filename);
static gboolean          pixmap_save_bmp           (GwyContainer *data,
                                                    const gchar *filename);
static gboolean          pixmap_save_targa         (GwyContainer *data,
                                                    const gchar *filename);
static GdkPixbuf*        hruler                    (gint size,
                                                    gint extra,
                                                    gdouble real,
                                                    gdouble zoom,
                                                    GwySIUnit *siunit);
static GdkPixbuf*        vruler                    (gint size,
                                                    gint extra,
                                                    gdouble real,
                                                    gdouble zoom,
                                                    GwySIUnit *siunit);
static GdkPixbuf*        fmscale                   (gint size,
                                                    gdouble bot,
                                                    gdouble top,
                                                    gdouble zoom,
                                                    GwySIUnit *siunit);
static GdkDrawable*      prepare_drawable          (gint width,
                                                    gint height,
                                                    gint lw,
                                                    GdkGC **gc);
static PangoLayout*      prepare_layout            (gdouble zoom);
static PixmapFormatInfo* find_format               (const gchar *name);
static void              pixmap_save_load_args     (GwyContainer *container,
                                                    PixmapSaveArgs *args);
static void              pixmap_save_save_args     (GwyContainer *container,
                                                    PixmapSaveArgs *args);
static void              pixmap_save_sanitize_args (PixmapSaveArgs *args);
static void              pixmap_load_load_args     (GwyContainer *container,
                                                    PixmapLoadArgs *args);
static void              pixmap_load_save_args     (GwyContainer *container,
                                                    PixmapLoadArgs *args);
static void              pixmap_load_sanitize_args (PixmapLoadArgs *args);

static struct {
    const gchar *name;
    const gchar *description;
    const gchar *extensions;
    GwyFileSaveFunc save;
}
saveable_formats[] = {
    {
        "png",
        N_("Portable Network Graphics (.png)"),
        GWY_PNG_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_png,
    },
    {
        "jpeg",
        N_("JPEG (.jpeg,.jpg)"),
        GWY_JPEG_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_jpeg,
    },
#ifdef HAVE_TIFF
    {
        "tiff",
        N_("TIFF (.tiff,.tif)"),
        GWY_TIFF_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_tiff,
    },
#endif
    {
        "pnm",
        N_("Portable Pixmap (.ppm,.pnm)"),
        GWY_PPM_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_ppm,
    },
    {
        "bmp",
        N_("Windows or OS2 Bitmap (.bmp)"),
        GWY_BMP_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_bmp
    },
    {
        "tga",
        N_("TARGA (.tga,.targa)"),
        GWY_TARGA_EXTENSIONS,
        (GwyFileSaveFunc)&pixmap_save_targa
    },
};

/* List of PixmapFormatInfo for all formats.
 * FIXME: this is never freed */
static GSList *pixmap_formats = NULL;

static const PixmapSaveArgs pixmap_save_defaults = {
    1.0, PIXMAP_EVERYTHING
};

static const PixmapLoadArgs pixmap_load_defaults = {
    100.0, 100.0, -6, TRUE, 1.0, -6, PIXMAP_MAP_VALUE, NULL
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "pixmap",
    N_("Exports data as as pixmap images and imports data from pixmap images. "
       "Supports following image formats for export: "
       "PNG, "
       "JPEG, "
       "TIFF (if available), "
       "PPM, "
       "BMP, "
       "TARGA. "
       "Import support relies on GDK and thus may be installation-dependent."),
    "Yeti <yeti@gwyddion.net>",
    "4.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    GwyFileFuncInfo *func_info;
    PixmapFormatInfo *format_info;
    GSList *formats, *l;
    gboolean registered[G_N_ELEMENTS(saveable_formats)];
    guint i;

    memset(registered, 0, G_N_ELEMENTS(saveable_formats)*sizeof(gboolean));
    formats = gdk_pixbuf_get_formats();
    for (l = formats; l; l = g_slist_next(l)) {
        GdkPixbufFormat *pixbuf_format = (GdkPixbufFormat*)l->data;
        gchar *fmtname;

        fmtname = gdk_pixbuf_format_get_name(pixbuf_format);
        /* ignore some really silly formats */
        if (strcmp(fmtname, "ico") == 0
            || strcmp(fmtname, "ani") == 0
            || strcmp(fmtname, "wbmp") == 0
            /* libwmf loader seems to try to claim ownership of almost
             * arbitrary binary data, prints error messages, and it's silly
             * to load WMF to Gwyddion anyway */
            || strcmp(fmtname, "wmf") == 0
            || strcmp(fmtname, "svg") == 0) {
            g_free(fmtname);
            continue;
        }

        func_info = g_new0(GwyFileFuncInfo, 1);
        func_info->name = fmtname;
        func_info->load = &pixmap_load;
        func_info->detect = &pixmap_detect;
        format_info = g_new0(PixmapFormatInfo, 1);
        format_info->func_info = func_info;
        format_info->pixbuf_format = pixbuf_format;
        for (i = 0; i < G_N_ELEMENTS(saveable_formats); i++) {
            /* FIXME: hope we have the same format names */
            if (strcmp(fmtname, saveable_formats[i].name) == 0) {
                gwy_debug("Found GdkPixbuf loader for known type: %s", fmtname);
                func_info->file_desc = saveable_formats[i].description;
                func_info->save = saveable_formats[i].save;
                format_info->extensions = saveable_formats[i].extensions;
                registered[i] = TRUE;
                break;
            }
        }
        if (!func_info->save) {
            gchar *s, **ext;

            gwy_debug("Found GdkPixbuf loader for new type: %s", fmtname);
            func_info->file_desc
                = gdk_pixbuf_format_get_description(pixbuf_format);
            /* FIXME: fix the slashes in menu path better */
            for (s = strchr(func_info->file_desc, '/'); s; s = strchr(s, '/'))
                *s = '-';

            ext = gdk_pixbuf_format_get_extensions(pixbuf_format);
            s = g_strjoinv(",.", ext);
            format_info->extensions = g_strconcat(".", s, NULL);
            g_free(s);
            g_strfreev(ext);
        }
        gwy_file_func_register(name, func_info);
        pixmap_formats = g_slist_append(pixmap_formats, format_info);
    }

    for (i = 0; i < G_N_ELEMENTS(saveable_formats); i++) {
        if (registered[i])
            continue;
        gwy_debug("Saveable format %s not known to GdkPixbuf",
                  saveable_formats[i].name);
        func_info = g_new0(GwyFileFuncInfo, 1);
        func_info->name = saveable_formats[i].name;
        func_info->file_desc = saveable_formats[i].description;
        func_info->save = saveable_formats[i].save;
        func_info->detect = &pixmap_detect;
        format_info = g_new0(PixmapFormatInfo, 1);
        format_info->func_info = func_info;
        format_info->extensions = saveable_formats[i].extensions;

        gwy_file_func_register(name, func_info);
        pixmap_formats = g_slist_append(pixmap_formats, format_info);
    }

    g_slist_free(formats);

    return TRUE;
}

/***************************************************************************
 *
 *  detect
 *
 ***************************************************************************/

static gint
pixmap_detect(const gchar *filename,
              gboolean only_name,
              const gchar *name)
{
    GdkPixbufLoader *loader;
    GError *err = NULL;
    PixmapFormatInfo *format_info;
    gint score;
    FILE *fh;
    guint n;
    gchar **extensions;
    guint ext;

    format_info = find_format(name);
    g_return_val_if_fail(format_info, 0);

    extensions = g_strsplit(format_info->extensions, ",", 0);
    g_assert(extensions);
    for (ext = 0; extensions[ext]; ext++) {
        if (g_str_has_suffix(filename, extensions[ext]))
            break;
    }
    score = extensions[ext] ? 20 : 0;
    g_strfreev(extensions);
    if (only_name) /* || !score)*/
        return score;

    if (!(fh = fopen(filename, "rb")))
        return 0;
    n = fread(pixmap_ping_buf, 1, pixmap_ping_length, fh);
    fclose(fh);

    /* FIXME: this is incorrect, but no one is going to import data from such
     * a small valid image anyway */
    if (n < 64)
        return 0;

    /* FIXME: GdkPixbuf doesn't good a good job regarding detection
     * we do some sanity check ourselves */
    if (strcmp(name, "png") == 0
        && memcmp(pixmap_ping_buf, "\x89PNG\r\n\x1a\n", 8) != 0)
        return 0;
    if (strcmp(name, "bmp") == 0
        && strncmp(pixmap_ping_buf, "BM", 2) != 0)
        return 0;
    if (strcmp(name, "pnm") == 0
        && (pixmap_ping_buf[0] != 'P' || !isdigit(pixmap_ping_buf[1])))
        return 0;
    if (strcmp(name, "xpm") == 0
        && strncmp(pixmap_ping_buf, "/* XPM */", 9) != 0)
        return 0;
    if (strcmp(name, "tiff") == 0
        && memcmp(pixmap_ping_buf, "MM\x00\x2a", 4) != 0
        && memcmp(pixmap_ping_buf, "II\x2a\x00", 4) != 0)
        return 0;
    if (strcmp(name, "jpeg") == 0
        && memcmp(pixmap_ping_buf, "\xff\xd8", 2) != 0)
        return 0;
    if (strcmp(name, "pcx") == 0
        && (pixmap_ping_buf[0] != '\x0a' || pixmap_ping_buf[1] > 0x05))
        return 0;
    if (strcmp(name, "gif") == 0
        && strncmp(pixmap_ping_buf, "GIF8", 4) != 0)
        return 0;
    if (strcmp(name, "svg") == 0
        && strncmp(pixmap_ping_buf, "<?xml", 5) != 0)
        return 0;
    if (strcmp(name, "ras") == 0
        && memcmp(pixmap_ping_buf, "\x59\xa6\x6a\x95", 4) != 0)
        return 0;
    /* FIXME: cannot detect targa, must try loader */

    loader = gdk_pixbuf_loader_new_with_type(name, NULL);
    if (!loader)
        return 0;

    if (gdk_pixbuf_loader_write(loader, pixmap_ping_buf, n, &err))
        score = 100;
    else {
        gwy_debug("%s", err->message);
        g_clear_error(&err);
        score = 0;
    }
    gdk_pixbuf_loader_close(loader, NULL);
    g_object_unref(loader);

    return score;
}

/***************************************************************************
 *
 *  load
 *
 ***************************************************************************/

static GwyContainer*
pixmap_load(const gchar *filename,
            const gchar *name)
{
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
    gint not_grayscale, any_red, any_green, any_blue;
    gdouble *val, *r;
    PixmapLoadArgs args;

    gwy_debug("Loading <%s> as %s", filename, name);

    format_info = find_format(name);
    g_return_val_if_fail(format_info, 0);

    if (!(fh = fopen(filename, "rb")))
        return NULL;

    loader = gdk_pixbuf_loader_new_with_type(name, NULL);
    if (!loader) {
        fclose(fh);
        return NULL;
    }

    do {
        n = fread(pixmap_ping_buf, 1, pixmap_ping_length, fh);
        gwy_debug("loaded %u bytes", n);
        if (!gdk_pixbuf_loader_write(loader, pixmap_ping_buf, n, &err)) {
            g_warning("%s", err->message);
            g_clear_error(&err);
            g_object_unref(loader);
            return NULL;
        }
    } while (n == pixmap_ping_length);

    if (!gdk_pixbuf_loader_close(loader, &err)) {
        g_warning("%s", err->message);
        g_clear_error(&err);
        g_object_unref(loader);
        return NULL;
    }

    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    g_assert(pixbuf);
    g_object_ref(pixbuf);
    g_object_unref(loader);

    settings = gwy_app_settings_get();
    pixmap_load_load_args(settings, &args);
    args.pixbuf = pixbuf;

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    bpp = has_alpha ? 4 : 3;
    /* check which value mapping methods seem feasible */
    not_grayscale = any_red = any_green = any_blue = 0;
    for (i = 0; i < height; i++) {
        p = pixels + i*rowstride;
        for (j = 0; j < width; j++) {
            guchar red = p[bpp*j], green = p[bpp*j+1], blue = p[bpp*j+2];

            not_grayscale |= (green ^ red) | (red ^ blue);
            any_green |= green;
            any_blue |= blue;
            any_red |= red;
        }
    }
    maptype_known = FALSE;
    if (!not_grayscale) {
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
    ok = pixmap_load_dialog(&args, name, width, height, maptype_known);
    pixmap_load_save_args(settings, &args);
    if (!ok) {
        g_object_unref(pixbuf);
        return NULL;
    }

    dfield = GWY_DATA_FIELD(gwy_data_field_new(width, height,
                                               args.xreal, args.yreal, FALSE));
    val = gwy_data_field_get_data(dfield);
    for (i = 0; i < height; i++) {
        p = pixels + i*rowstride;
        r = val + i*width;

        switch (args.maptype) {
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

            default:
            g_assert_not_reached();
            break;
        }
    }
    g_object_unref(pixbuf);

    gwy_data_field_set_xreal(dfield,
                             args.xreal*exp(G_LN10*args.xyexponent));
    gwy_data_field_set_yreal(dfield,
                             args.yreal*exp(G_LN10*args.xyexponent));
    gwy_data_field_multiply(dfield,
                            args.zreal*exp(G_LN10*args.zexponent));
    data = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_object_by_name(data, "/0/data", G_OBJECT(dfield));
    g_object_unref(dfield);

    return data;
}

static gboolean
pixmap_load_dialog(PixmapLoadArgs *args,
                   const gchar *name,
                   gint xres,
                   gint yres,
                   const gboolean mapknown)
{
    static const GwyEnum value_map_types[] = {
        { "Red",         PIXMAP_MAP_RED },
        { "Green",       PIXMAP_MAP_GREEN },
        { "Blue",        PIXMAP_MAP_BLUE },
        { "Value (max)", PIXMAP_MAP_VALUE },
        { "RGB sum",     PIXMAP_MAP_SUM },
    };

    PixmapLoadControls controls;
    GtkObject *adj;
    GtkAdjustment *adj2;
    GtkWidget *dialog, *table, *label, *align, *button, *hbox;
    enum { RESPONSE_RESET = 1 };
    gint response;
    gchar *s, *title;
    gchar buf[16];
    gint row;

    controls.args = args;
    controls.xres = xres;
    controls.yres = yres;

    s = g_ascii_strup(name, -1);
    title = g_strconcat(_("Import "), s, NULL);
    g_free(s);
    dialog = gtk_dialog_new_with_buttons(title, NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    g_free(title);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    table = gtk_table_new(3, 3, FALSE);
    gtk_container_add(GTK_CONTAINER(align), table);
    row = 0;
    table_attach_heading(table, _("<b>Resolution</b>"), row++);

    g_snprintf(buf, sizeof(buf), "%u", xres);
    label = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gwy_table_attach_row(table, row++, _("_Horizontal size:"), _("px"),
                         label);

    g_snprintf(buf, sizeof(buf), "%u", yres);
    label = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    gwy_table_attach_row(table, row++, _("_Vertical size:"), _("px"),
                         label);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    controls.image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(align), controls.image);
    pixmap_load_create_preview(args, &controls);

    table = gtk_table_new(6, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    row = 0;

    table_attach_heading(table, _("<b>Physical dimensions</b>"), row++);

    adj = gtk_adjustment_new(args->xreal, 0.01, 10000, 1, 100, 100);
    controls.xreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.xreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.xreal,
                     1, 2, row, row+1, GTK_FILL, 0, 2, 2);

    label = gtk_label_new_with_mnemonic(_("_Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.xreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);

    align = gtk_alignment_new(0.0, 0.5, 0.2, 0.0);
    controls.xyexponent = gwy_option_menu_metric_unit(NULL, NULL,
                                                      -12, 3, "m",
                                                      args->xyexponent);
    gtk_container_add(GTK_CONTAINER(align), controls.xyexponent);
    gtk_table_attach(GTK_TABLE(table), align, 2, 3, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 2, 2);
    row++;

    adj = gtk_adjustment_new(args->yreal, 0.01, 10000, 1, 100, 100);
    controls.yreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.yreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.yreal,
                     1, 2, row, row+1, GTK_FILL, 0, 2, 2);

    label = gtk_label_new_with_mnemonic(_("H_eight"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.yreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Identical _measures"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls.xymeasureeq = button;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    adj = gtk_adjustment_new(args->zreal, 0.01, 10000, 1, 100, 100);
    controls.zreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls.zreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.zreal,
                     1, 2, row, row+1, GTK_FILL, 0, 2, 2);

    label = gtk_label_new_with_mnemonic(_("_Z-scale"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.zreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);

    align = gtk_alignment_new(0.0, 0.5, 0.2, 0.0);
    controls.zexponent = gwy_option_menu_metric_unit(NULL, NULL,
                                                     -12, 3, _("m/sample unit"),
                                                     args->zexponent);
    gtk_container_add(GTK_CONTAINER(align), controls.zexponent);
    gtk_table_attach(GTK_TABLE(table), align, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 2, 2);
    row++;

    if (!mapknown) {
        label = gtk_label_new(_("Warning: Colorful images cannot be reliably "
                                "mapped to meaningful values."));
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                         GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 2, 2);
        row++;

        controls.maptype
            = gwy_option_menu_create(value_map_types,
                                     G_N_ELEMENTS(value_map_types),
                                     "map-type",
                                     G_CALLBACK(pixmap_load_map_type_update),
                                     &controls,
                                     args->maptype);
        gwy_table_attach_row(table, row++, _("Use"), _("as data"),
                             controls.maptype);
    }
    else
        controls.maptype = NULL;

    g_signal_connect_swapped(controls.xymeasureeq, "toggled",
                             G_CALLBACK(xymeasureeq_changed_cb), &controls);
    adj2 = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls.xreal));
    g_signal_connect(adj2, "value_changed",
                     G_CALLBACK(xyreal_changed_cb), &controls);
    adj2 = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls.yreal));
    g_signal_connect(adj2, "value_changed",
                     G_CALLBACK(xyreal_changed_cb), &controls);
    pixmap_load_update_controls(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            pixmap_load_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = pixmap_load_defaults;
            pixmap_load_update_controls(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    pixmap_load_update_values(&controls, args);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
pixmap_load_create_preview(PixmapLoadArgs *args,
                           PixmapLoadControls *controls)
{
    GdkPixbuf *pixbuf;
    gint width, height, rowstride, i, j;
    guchar *pixels, *p;
    gdouble zoom;

    width = gdk_pixbuf_get_width(args->pixbuf);
    height = gdk_pixbuf_get_height(args->pixbuf);
    zoom = 120.0/MAX(width, height);
    pixbuf = gdk_pixbuf_scale_simple(args->pixbuf,
                                     zoom*width, zoom*height,
                                     GDK_INTERP_TILES);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            p = pixels + i*rowstride + 3*j;
            switch (args->maptype) {
                case PIXMAP_MAP_BLUE:
                p[0] = p[1] = p[2];
                break;

                case PIXMAP_MAP_GREEN:
                p[0] = p[2] = p[1];
                break;

                case PIXMAP_MAP_RED:
                p[1] = p[2] = p[0];
                break;

                case PIXMAP_MAP_VALUE:
                p[0] = MAX(p[0], p[1]);
                p[0] = p[1] = p[2] = MAX(p[0], p[2]);
                break;

                case PIXMAP_MAP_SUM:
                p[0] = p[1] = p[2] = (p[0] + p[1] + p[2])/3;
                break;

                default:
                g_assert_not_reached();
                break;
            }
        }
    }

    gtk_image_set_from_pixbuf(GTK_IMAGE(controls->image), pixbuf);
    g_object_unref(pixbuf);
}

static void
pixmap_load_map_type_update(G_GNUC_UNUSED GtkWidget *item,
                            PixmapLoadControls *controls)
{

    controls->args->maptype = gwy_option_menu_get_history(controls->maptype,
                                                          "map-type");
    pixmap_load_create_preview(controls->args, controls);
}

static void
pixmap_load_update_controls(PixmapLoadControls *controls,
                            PixmapLoadArgs *args)
{
    GtkAdjustment *adj;

    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    gtk_adjustment_set_value(adj, args->xreal);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    gtk_adjustment_set_value(adj, args->yreal);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->xymeasureeq),
                                 args->xymeasureeq);
    gwy_option_menu_set_history(controls->xyexponent, "metric-unit",
                                args->xyexponent);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->zreal));
    gtk_adjustment_set_value(adj, args->zreal);
    gwy_option_menu_set_history(controls->zexponent, "metric-unit",
                                args->zexponent);
    if (controls->maptype)
        gwy_option_menu_set_history(controls->maptype, "map-type",
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
    args->xyexponent = gwy_option_menu_get_history(controls->xyexponent,
                                                   "metric-unit");
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->zreal));
    args->zreal = gtk_adjustment_get_value(adj);
    args->zexponent = gwy_option_menu_get_history(controls->zexponent,
                                                  "metric-unit");
    if (controls->maptype)
        args->maptype = gwy_option_menu_get_history(controls->maptype,
                                                    "map-type");
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

static GtkWidget*
table_attach_heading(GtkWidget *table,
                     const gchar *text,
                     gint row)
{
    GtkWidget *label;
    gchar *s;

    s = g_strconcat("<b>", text, "</b>", NULL);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), s);
    g_free(s);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, row, row+1);

    return label;
}

/***************************************************************************
 *
 *  writers
 *
 ***************************************************************************/

static gboolean
pixmap_save_png(GwyContainer *data,
                const gchar *filename)
{
    GdkPixbuf *pixbuf;
    GError *err = NULL;
    gboolean ok;

    pixbuf = pixmap_draw_pixbuf(data, "PNG");
    if (!pixbuf)
        return FALSE;

    ok = gdk_pixbuf_save(pixbuf, filename, "png", &err, NULL);
    if (!ok) {
        g_warning("PNG `%s' write failed: %s", filename, err->message);
        g_clear_error(&err);
    }
    g_object_unref(pixbuf);

    return ok;
}

static gboolean
pixmap_save_jpeg(GwyContainer *data,
                 const gchar *filename)
{
    GdkPixbuf *pixbuf;
    GError *err = NULL;
    gboolean ok;

    pixbuf = pixmap_draw_pixbuf(data, "JPEG");
    if (!pixbuf)
        return FALSE;

    ok = gdk_pixbuf_save(pixbuf, filename, "jpeg", &err, "quality", "98", NULL);
    if (!ok) {
        g_warning("JPEG `%s' write failed: %s", filename, err->message);
        g_clear_error(&err);
    }
    g_object_unref(pixbuf);

    return ok;
}

#ifdef HAVE_TIFF
static gboolean
pixmap_save_tiff(GwyContainer *data,
                 const gchar *filename)
{
    GdkPixbuf *pixbuf;
    TIFF *out;
    guchar *pixels = NULL;
    guint rowstride, i, width, height;
    /* TODO: error handling */
    gboolean ok = TRUE;

    pixbuf = pixmap_draw_pixbuf(data, "TIFF");
    if (!pixbuf)
        return FALSE;

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
    g_object_unref(pixbuf);

    return ok;
}
#endif

static gboolean
pixmap_save_ppm(GwyContainer *data,
                const gchar *filename)
{
    static const gchar *ppm_header = "P6\n%u\n%u\n255\n";
    GdkPixbuf *pixbuf;
    guchar *pixels = NULL;
    guint rowstride, i, width, height;
    gboolean ok = FALSE;
    gchar *ppmh = NULL;
    FILE *fh;

    pixbuf = pixmap_draw_pixbuf(data, "PPM");
    if (!pixbuf)
        return FALSE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    fh = fopen(filename, "wb");
    if (!fh) {
        g_warning("PPM `%s' write failed!", filename);
        g_object_unref(pixbuf);
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
    g_object_unref(pixbuf);
    g_free(ppmh);
    fclose(fh);

    return ok;
}

static gboolean
pixmap_save_bmp(GwyContainer *data,
                const gchar *filename)
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
    GdkPixbuf *pixbuf;
    guchar *pixels = NULL, *buffer = NULL;
    guint rowstride, i, j, width, height;
    guint bmplen, bmprowstride;
    gboolean ok = FALSE;
    FILE *fh;

    pixbuf = pixmap_draw_pixbuf(data, "BMP");
    if (!pixbuf)
        return FALSE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    bmprowstride = 12*((width + 3)/4);
    bmplen = height*bmprowstride + sizeof(bmp_head);

    fh = fopen(filename, "wb");
    if (!fh) {
        g_warning("PPM `%s' write failed!", filename);
        g_object_unref(pixbuf);
        return FALSE;
    }

    *(guint32*)(bmp_head + 2) = GUINT32_TO_LE(bmplen);
    *(guint32*)(bmp_head + 18) = GUINT32_TO_LE(bmprowstride/3);
    *(guint32*)(bmp_head + 22) = GUINT32_TO_LE(height);
    *(guint32*)(bmp_head + 34) = GUINT32_TO_LE(height*bmprowstride);
    if (fwrite(bmp_head, 1, sizeof(bmp_head), fh) != sizeof(bmp_head))
        goto end;

    /* The ugly part: BMP uses BGR instead of RGB and is written upside down,
     * this silliness may originate nowhere else than in MS... */
    buffer = g_new(guchar, bmprowstride);
    memset(buffer, 0xff, sizeof(bmprowstride));
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
    g_object_unref(pixbuf);
    g_free(buffer);
    fclose(fh);

    return ok;
}

static gboolean
pixmap_save_targa(GwyContainer *data,
                  const gchar *filename)
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
     0x20,        /* image descriptor flags: origin upper */
    };
    GdkPixbuf *pixbuf;
    guchar *pixels, *buffer = NULL;
    guint targarowstride, rowstride, i, j, width, height;
    gboolean ok = FALSE;
    FILE *fh;

    pixbuf = pixmap_draw_pixbuf(data, "TARGA");
    if (!pixbuf)
        return FALSE;

    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    targarowstride = 12*((width + 3)/4);

    if (height > 65535 || width > 65535) {
        g_warning("Image too large to be stored as TARGA");
        return FALSE;
    }
    targa_head[12] = (targarowstride/3) & 0xff;
    targa_head[13] = (targarowstride/3 >> 8) & 0xff;
    targa_head[14] = (height) & 0xff;
    targa_head[15] = (height >> 8) & 0xff;

    fh = fopen(filename, "wb");
    if (!fh) {
        g_warning("TARGA `%s' write failed!", filename);
        g_object_unref(pixbuf);
        return FALSE;
    }

    if (fwrite(targa_head, 1, sizeof(targa_head), fh) != sizeof(targa_head))
        goto end;

    /* The ugly part: TARGA uses BGR instead of RGB */
    buffer = g_new(guchar, targarowstride);
    memset(buffer, 0xff, sizeof(targarowstride));
    for (i = 0; i < height; i++) {
        guchar *p = pixels + i*rowstride;
        guchar *q = buffer;

        for (j = width; j; j--, p += 3, q += 3) {
            *q = *(p + 2);
            *(q + 1) = *(p + 1);
            *(q + 2) = *p;
        }
        if (fwrite(buffer, 1, targarowstride, fh) != targarowstride)
            goto end;
    }

    ok = TRUE;
end:
    g_object_unref(pixbuf);
    g_free(buffer);
    fclose(fh);

    return ok;
}

/***************************************************************************
 *
 *  save - common
 *
 ***************************************************************************/

static GdkPixbuf*
pixmap_draw_pixbuf(GwyContainer *data,
                   const gchar *format_name)
{
    GdkPixbuf *pixbuf;
    GwyContainer *settings;
    PixmapSaveArgs args;

    settings = gwy_app_settings_get();
    pixmap_save_load_args(settings, &args);
    if (!pixmap_save_dialog(data, &args, format_name)) {
        pixmap_save_save_args(settings, &args);
        return NULL;
    }
    pixbuf = pixmap_real_draw_pixbuf(data, &args);
    pixmap_save_save_args(settings, &args);

    return pixbuf;
}

static GdkPixbuf*
pixmap_real_draw_pixbuf(GwyContainer *data,
                        PixmapSaveArgs *args)
{
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    GwyPalette *palette;
    GdkPixbuf *pixbuf, *hrpixbuf, *vrpixbuf, *datapixbuf, *tmpixbuf;
    GdkPixbuf *scalepixbuf = NULL;
    GwySIUnit *siunit_xy, *siunit_z;
    const guchar *samples;
    guchar *pixels;
    gint width, height, zwidth, zheight, hrh, vrw, scw, nsamp, y, lw;
    gint border = 20;
    gint gap = 20;
    gint fmw = 18;

    data_window = gwy_app_data_window_get_for_data(data);
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);
    data_view = GWY_DATA_VIEW(gwy_data_window_get_data_view(data_window));
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    g_return_val_if_fail(gwy_data_view_get_data(data_view) == data, NULL);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    siunit_xy = gwy_data_field_get_si_unit_xy(dfield);
    siunit_z = gwy_data_field_get_si_unit_z(dfield);

    layer = gwy_data_view_get_base_layer(data_view);
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);
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

    zwidth = args->zoom*width;
    zheight = args->zoom*height;
    if (args->otype == PIXMAP_RAW_DATA) {
        pixbuf = gdk_pixbuf_scale_simple(datapixbuf, zwidth, zheight,
                                         GDK_INTERP_TILES);
        gwy_debug_objects_creation(G_OBJECT(pixbuf));
        return pixbuf;
    }

    gap *= args->zoom;
    fmw *= args->zoom;
    lw = ZOOM2LW(args->zoom);

    hrpixbuf = hruler(zwidth + 2*lw, border, gwy_data_field_get_xreal(dfield),
                      args->zoom, siunit_xy);
    hrh = gdk_pixbuf_get_height(hrpixbuf);
    vrpixbuf = vruler(zheight + 2*lw, border, gwy_data_field_get_yreal(dfield),
                      args->zoom, siunit_xy);
    vrw = gdk_pixbuf_get_width(vrpixbuf);
    if (args->otype == PIXMAP_EVERYTHING) {
        scalepixbuf = fmscale(zheight + 2*lw,
                            gwy_data_field_get_min(dfield),
                            gwy_data_field_get_max(dfield),
                            args->zoom, siunit_z);
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
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    gdk_pixbuf_fill(pixbuf, 0xffffffff);
    gdk_pixbuf_scale(datapixbuf, pixbuf,
                     vrw + lw + border, hrh + lw + border,
                     zwidth, zheight,
                     vrw + lw + border, hrh + lw + border,
                     args->zoom, args->zoom,
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
    if (args->otype == PIXMAP_EVERYTHING) {
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
            k = nsamp-1 - floor(nsamp*y/args->zoom/height);
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
    gwy_debug_objects_creation(G_OBJECT(tmpixbuf));
    gdk_pixbuf_fill(tmpixbuf, 0x000000);
    gdk_pixbuf_copy_area(tmpixbuf, 0, 0, lw, zheight + 2*lw,
                         pixbuf, vrw + border + zwidth + lw, hrh + border);
    if (args->otype == PIXMAP_EVERYTHING) {
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
    gwy_debug_objects_creation(G_OBJECT(tmpixbuf));
    gdk_pixbuf_fill(tmpixbuf, 0x000000);
    gdk_pixbuf_copy_area(tmpixbuf, 0, 0, zwidth + 2*lw, lw,
                         pixbuf, vrw + border, hrh + border + zheight + lw);
    if (args->otype == PIXMAP_EVERYTHING) {
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

    return pixbuf;
}

static void
save_type_changed(GtkWidget *button,
                  PixmapSaveControls *controls)
{
    GdkPixbuf *pixbuf;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    controls->args->otype = gwy_radio_buttons_get_current(controls->group,
                                                          "output-format");
    pixbuf = pixmap_real_draw_pixbuf(controls->data, controls->args);
    gtk_image_set_from_pixbuf(GTK_IMAGE(controls->image), pixbuf);
    g_object_unref(pixbuf);
}

static gboolean
pixmap_save_dialog(GwyContainer *data,
                   PixmapSaveArgs *args,
                   const gchar *name)
{
    enum { RESPONSE_RESET = 1 };
    static const GwyEnum output_formats[] = {
        { N_("_Data alone"),    PIXMAP_RAW_DATA },
        { N_("Data + _rulers"), PIXMAP_RULERS },
        { N_("_Everything"),    PIXMAP_EVERYTHING },
    };

    GtkObject *zoom;
    PixmapSaveControls controls;
    GtkWidget *dialog, *table, *spin, *label, *hbox, *align;
    GwyDataField *dfield;
    GdkPixbuf *pixbuf;
    GSList *l;
    gint response;
    gchar *s, *title;
    gint row;

    controls.data = data;
    controls.args = args;

    s = g_ascii_strup(name, -1);
    title = g_strconcat(_("Export "), s, NULL);
    g_free(s);
    dialog = gtk_dialog_new_with_buttons(title, NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    g_free(title);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    table = gtk_table_new(2 + G_N_ELEMENTS(output_formats), 3, FALSE);
    gtk_container_add(GTK_CONTAINER(align), table);
    row = 0;

    zoom = gtk_adjustment_new(args->zoom, 0.06, 16.0, 0.1, 1.0, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Zoom:"), "", zoom);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    row++;

    label = gtk_label_new(_("Output:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.group = gwy_radio_buttons_create(output_formats,
                                              G_N_ELEMENTS(output_formats),
                                              "output-format",
                                              G_CALLBACK(save_type_changed),
                                              &controls,
                                              args->otype);
    for (l = controls.group; l; l = g_slist_next(l)) {
        gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(l->data),
                         0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
        row++;

    }

    /* preview */
    align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    controls.image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(align), controls.image);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    args->zoom = 120.0/MAX(gwy_data_field_get_xres(dfield),
                           gwy_data_field_get_yres(dfield));
    pixbuf = pixmap_real_draw_pixbuf(data, args);
    gtk_image_set_from_pixbuf(GTK_IMAGE(controls.image), pixbuf);
    g_object_unref(pixbuf);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->zoom = gtk_adjustment_get_value(GTK_ADJUSTMENT(zoom));
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = pixmap_save_defaults;
            gtk_adjustment_set_value(GTK_ADJUSTMENT(zoom), args->zoom);
            gwy_radio_buttons_set_current(controls.group,
                                          "output-format", args->otype);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->zoom = gtk_adjustment_get_value(GTK_ADJUSTMENT(zoom));
    gtk_widget_destroy(dialog);

    return TRUE;
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
       GwySIUnit *siunit)
{
    const guint bufsize = 64;
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    GwySIValueFormat *format;
    gdouble base, step, x;
    gchar *s;
    gint l, n, ix;
    gint tick, height, lw;

    s = g_new(gchar, bufsize);
    layout = prepare_layout(zoom);

    format = gwy_si_unit_get_format_with_resolution(siunit, real, real/12,
                                                    NULL);

    g_snprintf(s, bufsize, "%.*f", format->precision, real/format->magnitude);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);

    g_snprintf(s, bufsize, "%.*f %s", format->precision, 0.0, format->units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical2);

    l = MAX(PANGO_PIXELS(logical1.width), PANGO_PIXELS(logical2.width));
    n = MIN(10, size/l);
    step = real/format->magnitude/n;
    base = exp(G_LN10*floor(log10(step)));
    step = step/base;
    if (step <= 2.0)
        step = 2.0;
    else if (step <= 5.0)
        step = 5.0;
    else {
        base *= 10;
        step = 1.0;
        format->precision = MAX(format->precision - 1, 0);
    }

    tick = zoom*TICK_LENGTH;
    lw = ZOOM2LW(zoom);
    l = MAX(PANGO_PIXELS(logical1.height), PANGO_PIXELS(logical2.height));
    height = l + 2*zoom + tick + 2;
    drawable = prepare_drawable(size + extra, height, lw, &gc);

    for (x = 0.0; x <= real/format->magnitude; x += base*step) {
        if (!x)
            g_snprintf(s, bufsize, "%.*f %s",
                       format->precision, x, format->units);
        else
            g_snprintf(s, bufsize, "%.*f", format->precision, x);
        pango_layout_set_markup(layout, s, -1);
        ix = x/(real/format->magnitude)*size + lw/2;
        pango_layout_get_extents(layout, NULL, &logical1);
        if (ix + PANGO_PIXELS(logical1.width) <= size + extra/4)
            gdk_draw_layout(drawable, gc, ix+1, 1, layout);
        gdk_draw_line(drawable, gc, ix, height-1, ix, height-1-tick);
    }

    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, size + extra, height);

    gwy_si_unit_value_format_free(format);
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
       GwySIUnit *siunit)
{
    enum { bufsize = 64 };
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    GwySIValueFormat *format;
    gdouble base, step, x;
    gchar *s;
    gint l, n, ix;
    gint tick, width, lw;

    s = g_new(gchar, bufsize);
    layout = prepare_layout(zoom);

    format = gwy_si_unit_get_format_with_resolution(siunit, real, real/12,
                                                    NULL);

    /* note the algorithm is the same to force consistency between axes,
     * even though the vertical one could be filled with tick more densely */
    g_snprintf(s, bufsize, "%.*f", format->precision, real/format->magnitude);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);

    g_snprintf(s, bufsize, "%.*f %s", format->precision, 0.0, format->units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical2);

    l = MAX(PANGO_PIXELS(logical1.width), PANGO_PIXELS(logical2.width));
    n = MIN(10, size/l);
    step = real/format->magnitude/n;
    base = exp(G_LN10*floor(log10(step)));
    step = step/base;
    if (step <= 2.0)
        step = 2.0;
    else if (step <= 5.0)
        step = 5.0;
    else {
        base *= 10;
        step = 1.0;
        format->precision = MAX(format->precision - 1, 0);
    }

    g_snprintf(s, bufsize, "%.*f", format->precision, real/format->magnitude);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);
    l = PANGO_PIXELS(logical1.width);

    tick = zoom*TICK_LENGTH;
    lw = ZOOM2LW(zoom);
    width = l + 2*zoom + tick + 2;
    drawable = prepare_drawable(width, size + extra, lw, &gc);

    for (x = 0.0; x <= real/format->magnitude; x += base*step) {
        g_snprintf(s, bufsize, "%.*f", format->precision, x);
        pango_layout_set_markup(layout, s, -1);
        ix = x/(real/format->magnitude)*size + lw/2;
        pango_layout_get_extents(layout, NULL, &logical1);
        if (ix + PANGO_PIXELS(logical1.height) <= size + extra/4)
            gdk_draw_layout(drawable, gc,
                            l - PANGO_PIXELS(logical1.width) + 1, ix+1, layout);
        gdk_draw_line(drawable, gc, width-1, ix, width-1-tick, ix);
    }

    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, width, size + extra);

    gwy_si_unit_value_format_free(format);
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
        GwySIUnit *siunit)
{
    enum { bufsize = 64 };
    PangoRectangle logical1, logical2;
    PangoLayout *layout;
    GdkDrawable *drawable;
    GdkPixbuf *pixbuf;
    GdkGC *gc;
    gdouble x;
    GwySIValueFormat *format;
    gchar *s;
    gint l, tick, width, lw;

    s = g_new(gchar, bufsize);
    layout = prepare_layout(zoom);

    x = MAX(fabs(bot), fabs(top));
    format = gwy_si_unit_get_format_with_resolution(siunit, x, x/120, NULL);

    g_snprintf(s, bufsize, "%.*f %s",
               format->precision, top/format->magnitude, format->units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);

    g_snprintf(s, bufsize, "%.*f %s",
               format->precision, bot/format->magnitude, format->units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical2);

    l = MAX(PANGO_PIXELS(logical1.width), PANGO_PIXELS(logical2.width));
    tick = zoom*TICK_LENGTH;
    lw = ZOOM2LW(zoom);
    width = l + 2*zoom + tick + 2;
    drawable = prepare_drawable(width, size, lw, &gc);

    g_snprintf(s, bufsize, "%.*f %s",
               format->precision, bot/format->magnitude, format->units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);
    gdk_draw_layout(drawable, gc,
                    width - PANGO_PIXELS(logical1.width) - 2,
                    size - 1 - PANGO_PIXELS(logical1.height),
                    layout);
    gdk_draw_line(drawable, gc, 0, size - (lw + 1)/2, tick, size - (lw + 1)/2);

    g_snprintf(s, bufsize, "%.*f %s",
               format->precision, top/format->magnitude, format->units);
    pango_layout_set_markup(layout, s, -1);
    pango_layout_get_extents(layout, NULL, &logical1);
    gdk_draw_layout(drawable, gc,
                    width - PANGO_PIXELS(logical1.width) - 2, 1,
                    layout);
    gdk_draw_line(drawable, gc, 0, lw/2, tick, lw/2);

    gdk_draw_line(drawable, gc, 0, size/2, tick/2, size/2);

    pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                          0, 0, 0, 0, width, size);

    gwy_si_unit_value_format_free(format);
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

    /* FIXME: this creates a drawable with *SCREEN* bit depth
     * We should render a pixmap with Pango FT2 and use that */
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
    fontdesc = pango_font_description_from_string("Helvetica 12");
    pango_font_description_set_size(fontdesc, 12*PANGO_SCALE*zoom);
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

static PixmapFormatInfo*
find_format(const gchar *name)
{
    GSList *l;

    for (l = pixmap_formats; l; l = g_slist_next(l)) {
        PixmapFormatInfo *format_info = (PixmapFormatInfo*)l->data;
        if (strcmp(format_info->func_info->name, name) == 0)
            return format_info;
    }

    return NULL;
}

static const gchar *zoom_key = "/module/pixmap/zoom";
static const gchar *otype_key = "/module/pixmap/otype";

static void
pixmap_save_sanitize_args(PixmapSaveArgs *args)
{
    args->otype = MIN(args->otype, PIXMAP_LAST-1);
    args->zoom = CLAMP(args->zoom, 0.06, 16.0);
}

static void
pixmap_save_load_args(GwyContainer *container,
                      PixmapSaveArgs *args)
{
    *args = pixmap_save_defaults;

    gwy_container_gis_double_by_name(container, zoom_key, &args->zoom);
    gwy_container_gis_enum_by_name(container, otype_key, &args->otype);
    pixmap_save_sanitize_args(args);
}

static void
pixmap_save_save_args(GwyContainer *container,
                      PixmapSaveArgs *args)
{
    gwy_container_set_double_by_name(container, zoom_key, args->zoom);
    gwy_container_set_enum_by_name(container, otype_key, args->otype);
}

static const gchar *xreal_key = "/module/pixmap/xreal";
static const gchar *yreal_key = "/module/pixmap/yreal";
static const gchar *xyexponent_key = "/module/pixmap/xyexponent";
static const gchar *xymeasureeq_key = "/module/pixmap/xymeasureeq";
static const gchar *zreal_key = "/module/pixmap/zreal";
static const gchar *zexponent_key = "/module/pixmap/zexponent";
static const gchar *maptype_key = "/module/pixmap/maptype";

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
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
