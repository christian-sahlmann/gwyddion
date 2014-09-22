/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
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
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <cairo.h>
/* FIXME: We also need pangocairo, i.e. Pango 1.8+, maybe newer. */

/* We want cairo_ps_surface_set_eps().  So if we don't get it we just pretend
 * cairo doesn't have the PS surface at all. */
#if (CAIRO_VERSION_MAJOR < 1 || (CAIRO_VERSION_MAJOR == 1 && CAIRO_VERSION_MINOR < 6))
#undef CAIRO_HAS_PS_SURFACE
#endif

#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif

#ifdef CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
#endif

#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif

#ifdef HAVE_PNG
#include <png.h>
#ifdef HAVE_ZLIB
#include <zlib.h>
#else
#define Z_BEST_COMPRESSION 9
#endif
#endif

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

#define pangoscale ((gdouble)PANGO_SCALE)
#define fixzero(x) (fabs(x) < 1e-14 ? 0.0 : (x))

enum {
    TICK_LENGTH     = 10,
    PREVIEW_SIZE    = 240,
};

/* What is present on the exported image
 * XXX: Makes no sense */
typedef enum {
    PIXMAP_LATERAL_NONE,
    PIXMAP_RULERS,
    PIXMAP_FMSCALE = PIXMAP_RULERS,
    PIXMAP_SCALEBAR
} ImageOutput;

typedef enum {
    INSET_POS_TOP_LEFT,
    INSET_POS_TOP_CENTER,
    INSET_POS_TOP_RIGHT,
    INSET_POS_BOTTOM_LEFT,
    INSET_POS_BOTTOM_CENTER,
    INSET_POS_BOTTOM_RIGHT,
    INSET_NPOS
} InsetPosType;

struct ImgExportFormat;

typedef struct {
    gdouble x, y;
    gdouble w, h;
} ImgExportRect;

typedef struct {
    gdouble from, to, step, base;
} RulerTicks;

typedef struct {
    /* Only the width and height are meaningful here */
    GwySIValueFormat *vf_hruler;
    GwySIValueFormat *vf_vruler;
    GwySIValueFormat *vf_fmruler;
    RulerTicks hruler_ticks;
    RulerTicks vruler_ticks;
    RulerTicks fmruler_ticks;
    gdouble hruler_label_height;
    gdouble vruler_label_width;
    gdouble fmruler_label_width;
    gdouble inset_length;
    GwyLayerBasicRangeType fm_rangetype;
    gboolean fm_inverted;

    /* Actual rectangles, including positions, of the image parts. */
    ImgExportRect image;
    ImgExportRect hruler;
    ImgExportRect vruler;
    ImgExportRect inset;
    ImgExportRect fmgrad;
    ImgExportRect fmruler;
    ImgExportRect title;

    /* Union of all above (plus maybe borders). */
    ImgExportRect canvas;
} ImgExportSizes;

typedef struct {
    gdouble zoom;
    ImageOutput xytype;
    ImageOutput ztype;
    GwyRGBA inset_color;
    InsetPosType inset_pos;
    gboolean draw_mask;
    gboolean draw_selection;
    gboolean text_antialias;
    gchar *font;
    gdouble font_size;
    gboolean scale_font;
    gboolean inset_draw_ticks;
    gboolean inset_draw_label;
    gdouble fmscale_gap;
    gdouble inset_gap;
    guint greyscale;
    gchar *inset_length;
    /* Interface only */
    const struct _ImgExportFormat *format;
    GwyDataView *data_view;
    GwyDataField *dfield;
    GwyContainer *data;
    gint id;
    /* These two are `1:1' sizes, i.e. they differ from data field sizes when
     * realsquare is TRUE. */
    gint xres;
    gint yres;
    gboolean realsquare;

    /* New args */
    gdouble lw;
} ImgExportArgs;

typedef struct {
    ImgExportArgs *args;
    GtkWidget *dialog;
} ImgExportControls;

typedef gboolean (*WritePixbufFunc)(GdkPixbuf *pixbuf,
                                    const gchar *name,
                                    const gchar *filename,
                                    GError **error);
typedef gboolean (*WriteImageFunc)(const ImgExportArgs *args,
                                   const gchar *name,
                                   const gchar *filename,
                                   GError **error);

typedef struct _ImgExportFormat {
    const gchar *name;
    const gchar *description;
    const gchar *extensions;
    WritePixbufFunc write_pixbuf;   /* If NULL, use generic GdkPixbuf func. */
    WriteImageFunc write_grey16;    /* 16bit grey */
    WriteImageFunc write_vector;    /* scalable */
} ImgExportFormat;

static gboolean module_register         (void);
static gint     img_export_detect       (const GwyFileDetectInfo *fileinfo,
                                         gboolean only_name,
                                         const gchar *name);
static gboolean img_export_export       (GwyContainer *data,
                                         const gchar *filename,
                                         GwyRunType mode,
                                         GError **error,
                                         const gchar *name);
static gint     gwy_pixmap_step_to_prec (gdouble d);
static void     img_export_free_args    (ImgExportArgs *args);
static void     img_export_load_args    (GwyContainer *container,
                                         ImgExportArgs *args);
static void     img_export_save_args    (GwyContainer *container,
                                         ImgExportArgs *args);
static void     img_export_sanitize_args(ImgExportArgs *args);
static gchar*   scalebar_auto_length    (GwyDataField *dfield,
                                         gdouble *p);
static gdouble  inset_length_ok         (GwyDataField *dfield,
                                         const gchar *inset_length);

#ifdef HAVE_PNG
static gboolean write_image_png16(const ImgExportArgs *args,
                                  const gchar *name,
                                  const gchar *filename,
                                  GError **error);
#else
#define write_image_png16 NULL
#endif

static gboolean write_image_tiff16  (const ImgExportArgs *args,
                                     const gchar *name,
                                     const gchar *filename,
                                     GError **error);
static gboolean write_image_pgm16   (const ImgExportArgs *args,
                                     const gchar *name,
                                     const gchar *filename,
                                     GError **error);
static gboolean write_vector_generic(const ImgExportArgs *args,
                                     const gchar *name,
                                     const gchar *filename,
                                     GError **error);
static gboolean write_pixbuf_generic(GdkPixbuf *pixbuf,
                                     const gchar *name,
                                     const gchar *filename,
                                     GError **error);
static gboolean write_pixbuf_tiff   (GdkPixbuf *pixbuf,
                                     const gchar *name,
                                     const gchar *filename,
                                     GError **error);
static gboolean write_pixbuf_ppm    (GdkPixbuf *pixbuf,
                                     const gchar *name,
                                     const gchar *filename,
                                     GError **error);
static gboolean write_pixbuf_bmp    (GdkPixbuf *pixbuf,
                                     const gchar *name,
                                     const gchar *filename,
                                     GError **error);
static gboolean write_pixbuf_targa  (GdkPixbuf *pixbuf,
                                     const gchar *name,
                                     const gchar *filename,
                                     GError **error);

static ImgExportFormat image_formats[] = {
    {
        "png",
        N_("Portable Network Graphics (.png)"),
        ".png",
        NULL, write_image_png16, NULL,
    },
    {
        "jpeg",
        N_("JPEG (.jpeg,.jpg)"),
        ".jpeg,.jpg,.jpe",
        NULL, NULL, NULL,
    },
    {
        "tiff",
        N_("TIFF (.tiff,.tif)"),
        ".tiff,.tif",
        write_pixbuf_tiff, write_image_tiff16, NULL,
    },
    {
        "pnm",
        N_("Portable Image (.ppm,.pnm)"),
        ".ppm,.pnm",
        write_pixbuf_ppm, write_image_pgm16, NULL,
    },
    {
        "bmp",
        N_("Windows or OS2 Bitmap (.bmp)"),
        ".bmp",
        write_pixbuf_bmp, NULL, NULL,
    },
    {
        "tga",
        N_("TARGA (.tga,.targa)"),
        ".tga,.targa",
        write_pixbuf_targa, NULL, NULL,
    },
#ifdef CAIRO_HAS_PDF_SURFACE
    {
        "pdf",
        N_("Portable document format (.pdf)"),
        ".pdf",
        NULL, NULL, write_vector_generic,
    },
#endif
#ifdef CAIRO_HAS_PS_SURFACE
    {
        "eps",
        N_("Encapsulated PostScript (.eps)"),
        ".eps",
        NULL, NULL, write_vector_generic,
    },
#endif
#ifdef CAIRO_HAS_SVG_SURFACE
    {
        "svg",
        N_("Scalable Vector Graphics (.svg)"),
        ".svg",
        NULL, NULL, write_vector_generic,
    },
#endif
};

static const ImgExportArgs img_export_defaults = {
    1.0, PIXMAP_RULERS, PIXMAP_FMSCALE,
    { 1.0, 1.0, 1.0, 1.0 }, INSET_POS_BOTTOM_RIGHT,
    TRUE, TRUE, TRUE,
    "Helvetica", 12.0, TRUE, TRUE, TRUE,
    1.0, 1.0,
    0, "",
    /* Interface only */
    NULL, NULL, NULL, NULL, 0, 0, 0, FALSE,

    /* New args */
    1.0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Renders data into pixmap images and imports data from pixmap images. "
       "It supports the following image formats for export: "
       "PNG, JPEG, TIFF, PPM, BMP, TARGA. "
       "Import support relies on GDK and thus may be installation-dependent."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static ImgExportFormat*
find_format(const gchar *name, gboolean cairoext)
{
    guint i, len;

    for (i = 0; i < G_N_ELEMENTS(image_formats); i++) {
        ImgExportFormat *format = image_formats + i;

        if (cairoext) {
            len = strlen(format->name);
            if (strncmp(name, format->name, len) == 0
                && strcmp(name + len, "cairo") == 0)
                return format;
        }
        else {
            if (gwy_strequal(name, format->name))
                return format;
        }
    }

    return NULL;
}

static gboolean
module_register(void)
{
    GSList *l, *pixbuf_formats;
    guint i;

    /* Find out which image formats we can write using generic GdkPixbuf
     * functions. */
    pixbuf_formats = gdk_pixbuf_get_formats();
    for (l = pixbuf_formats; l; l = g_slist_next(l)) {
        GdkPixbufFormat *pixbuf_format = (GdkPixbufFormat*)l->data;
        const gchar *name;
        ImgExportFormat *format;

        name = gdk_pixbuf_format_get_name(pixbuf_format);
        if (!gdk_pixbuf_format_is_writable(pixbuf_format)) {
            gwy_debug("Ignoring pixbuf format %s, not writable", name);
            continue;
        }

        if (!(format = find_format(name, FALSE))) {
            gwy_debug("Skipping writable pixbuf format %s "
                      "because we don't know it.", name);
            continue;
        }

        if (format->write_pixbuf) {
            gwy_debug("Skipping pixbuf format %s, we have our own writer.",
                      name);
            continue;
        }

        gwy_debug("Adding generic pixbuf writer for %s.", name);
        format->write_pixbuf = write_pixbuf_generic;
    }
    g_slist_free(pixbuf_formats);

    /* Register file functions for the formats.  We want separate functions so
     * that users can see the formats listed in the file dialog.  We must use
     * names different from the pixmap module, so append "cairo". */
    for (i = 0; i < G_N_ELEMENTS(image_formats); i++) {
        ImgExportFormat *format = image_formats + i;
        gchar *caironame;

        if (!format->write_pixbuf
            && !format->write_grey16
            && !format->write_vector)
            continue;

        caironame = g_strconcat(format->name, "cairo", NULL);
        gwy_file_func_register(caironame,
                               format->description,
                               &img_export_detect,
                               NULL,
                               NULL,
                               &img_export_export);
    }

    return TRUE;
}

static gint
img_export_detect(const GwyFileDetectInfo *fileinfo,
                  G_GNUC_UNUSED gboolean only_name,
                  const gchar *name)
{
    ImgExportFormat *format;
    gint score;
    gchar **extensions;
    guint i;

    gwy_debug("Running detection for file type %s", name);

    format = find_format(name, TRUE);
    g_return_val_if_fail(format, 0);

    extensions = g_strsplit(format->extensions, ",", 0);
    g_assert(extensions);
    for (i = 0; extensions[i]; i++) {
        if (g_str_has_suffix(fileinfo->name_lowercase, extensions[i]))
            break;
    }
    /* TODO: Raise score once the module works. */
    score = extensions[i] ? 15 : 0;
    g_strfreev(extensions);

    return score;
}

static gchar*
scalebar_auto_length(GwyDataField *dfield,
                     gdouble *p)
{
    static const double sizes[] = {
        1.0, 2.0, 3.0, 4.0, 5.0,
        10.0, 20.0, 30.0, 40.0, 50.0,
        100.0, 200.0, 300.0, 400.0, 500.0,
    };
    GwySIValueFormat *format;
    GwySIUnit *siunit;
    gdouble base, x, vmax, real;
    gchar *s;
    gint power10;
    guint i;

    real = gwy_data_field_get_xreal(dfield);
    siunit = gwy_data_field_get_si_unit_xy(dfield);
    vmax = 0.42*real;
    power10 = 3*(gint)(floor(log10(vmax)/3.0));
    base = pow10(power10 + 1e-14);
    x = vmax/base;
    for (i = 1; i < G_N_ELEMENTS(sizes); i++) {
        if (x < sizes[i])
            break;
    }
    x = sizes[i-1] * base;

    format = gwy_si_unit_get_format_for_power10(siunit,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                power10, NULL);
    s = g_strdup_printf("%.*f %s",
                        format->precision, x/format->magnitude, format->units);
    gwy_si_unit_value_format_free(format);

    if (p)
        *p = x/real;

    return s;
}

static gdouble
inset_length_ok(GwyDataField *dfield,
                const gchar *inset_length)
{
    gdouble xreal, length;
    gint power10;
    GwySIUnit *siunit, *siunitxy;
    gchar *end, *plain_text_length = NULL;
    gboolean ok;

    if (!inset_length || !*inset_length)
        return 0.0;

    gwy_debug("checking inset <%s>", inset_length);
    if (!pango_parse_markup(inset_length, -1, 0,
                            NULL, &plain_text_length, NULL, NULL))
        return 0.0;

    gwy_debug("plain_text version <%s>", plain_text_length);
    length = g_strtod(plain_text_length, &end);
    gwy_debug("unit part <%s>", end);
    siunit = gwy_si_unit_new_parse(end, &power10);
    gwy_debug("power10 %d", power10);
    length *= pow10(power10);
    xreal = gwy_data_field_get_xreal(dfield);
    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    ok = (gwy_si_unit_equal(siunit, siunitxy)
          && length > 0.1*xreal
          && length < 0.85*xreal);
    g_free(plain_text_length);
    g_object_unref(siunit);
    gwy_debug("xreal %g, length %g, ok: %d", xreal, length, ok);

    return ok ? length : 0.0;
}

static PangoLayout*
create_layout(const gchar *fontname, gdouble fontsize, cairo_t *cr)
{
    PangoContext *context;
    PangoFontDescription *fontdesc;
    PangoLayout *layout;
    gchar *full_font;

    /* This creates a layout with private context so we can modify the
     * context at will. */
    layout = pango_cairo_create_layout(cr);

    full_font = g_strdup_printf("%s %.2f", fontname, fontsize);
    fontdesc = pango_font_description_from_string(full_font);
    g_free(full_font);
    pango_font_description_set_size(fontdesc, PANGO_SCALE*fontsize);
    context = pango_layout_get_context(layout);
    pango_context_set_font_description(context, fontdesc);
    pango_font_description_free(fontdesc);
    pango_layout_context_changed(layout);
    /* XXX: Must call pango_cairo_update_layout() if we change the
     * transformation afterwards. */

    return layout;
}

static void
format_layout(PangoLayout *layout,
              PangoRectangle *logical,
              GString *string,
              const gchar *format,
              ...)
{
    gchar *buffer;
    gint length;
    va_list args;

    g_string_truncate(string, 0);
    va_start(args, format);
    length = g_vasprintf(&buffer, format, args);
    va_end(args);
    g_string_append_len(string, buffer, length);
    g_free(buffer);

    /* Replace ASCII with proper minus */
    if (string->str[0] == '-') {
        g_string_erase(string, 0, 1);
        g_string_prepend_unichar(string, 0x2212);
    }

    pango_layout_set_markup(layout, string->str, string->len);
    pango_layout_get_extents(layout, NULL, logical);
}

static cairo_surface_t*
create_surface(const ImgExportArgs *args,
               const gchar *name,
               const gchar *filename,
               gdouble width, gdouble height)
{
    cairo_surface_t *surface = NULL;
    cairo_format_t imageformat = CAIRO_FORMAT_RGB24;
    gint iwidth, iheight;

    if (width <= 0.0)
        width = 100.0;
    if (height <= 0.0)
        height = 100.0;

    iwidth = (gint)ceil(width);
    iheight = (gint)ceil(height);

    /* XXX: PNG supports transparency.  But Cairo draws with premultiplied
     * alpha, which means we would have to decompose it again for PNG.   This
     * can turn ugly. */
    if (gwy_stramong(name,
                     "png", "jpeg2000", "jpeg", "tiff", "pnm",
                     "bmp", "tga", NULL))
        surface = cairo_image_surface_create(imageformat, iwidth, iheight);
#ifdef CAIRO_HAS_PDF_SURFACE
    else if (gwy_strequal(name, "pdf"))
        surface = cairo_pdf_surface_create(filename, width, height);
#endif
#ifdef CAIRO_HAS_PS_SURFACE
    else if (gwy_strequal(name, "eps")) {
        surface = cairo_ps_surface_create(filename, width, height);
        /* Requires cairo 1.6. */
        cairo_ps_surface_set_eps(surface, TRUE);
    }
#endif
#ifdef CAIRO_HAS_SVG_SURFACE
    else if (gwy_strequal(name, "svg")) {
        surface = cairo_svg_surface_create(filename, width, height);
    }
#endif
    else {
        g_assert_not_reached();
    }

    return surface;
}

static void
find_hruler_ticks(const ImgExportArgs *args, ImgExportSizes *sizes,
                  PangoLayout *layout, GString *s)
{
    GwySIUnit *xyunit = gwy_data_field_get_si_unit_xy(args->dfield);
    gdouble size = sizes->image.w;
    gdouble real = gwy_data_field_get_xreal(args->dfield);
    gdouble offset = gwy_data_field_get_xoffset(args->dfield);
    RulerTicks *ticks = &sizes->hruler_ticks;
    PangoRectangle logical1, logical2;
    GwySIValueFormat *vf;
    gdouble len, bs, height;
    guint n;

    vf = gwy_si_unit_get_format_with_resolution(xyunit,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                real, real/12,
                                                NULL);
    sizes->vf_hruler = vf;
    gwy_debug("unit '%s'", vf->units);
    offset /= vf->magnitude;
    real /= vf->magnitude;
    format_layout(layout, &logical1, s, "%.*f", vf->precision, -real);
    gwy_debug("right '%s'", s->str);
    format_layout(layout, &logical2, s, "%.*f %s",
                  vf->precision, offset, vf->units);
    gwy_debug("first '%s'", s->str);

    height = MAX(logical1.height/pangoscale, logical2.height/pangoscale);
    sizes->hruler_label_height = height;
    len = MAX(logical1.width/pangoscale, logical2.width/pangoscale);
    gwy_debug("label len %g, height %g", len, height);
    n = CLAMP(GWY_ROUND(size/len), 1, 10);
    gwy_debug("nticks %u", n);
    ticks->step = real/n;
    ticks->base = pow10(floor(log10(ticks->step)));
    ticks->step /= ticks->base;
    if (ticks->step <= 2.0)
        ticks->step = 2.0;
    else if (ticks->step <= 5.0)
        ticks->step = 5.0;
    else {
        ticks->base *= 10.0;
        ticks->step = 1.0;
        if (vf->precision)
            vf->precision--;
    }
    gwy_debug("base %g, step %g", ticks->base, ticks->step);

    bs = ticks->base * ticks->step;
    ticks->from = ceil(offset/bs - 1e-14)*bs;
    ticks->from = fixzero(ticks->from);
    ticks->to = floor((real + offset)/bs + 1e-14)*bs;
    ticks->to = fixzero(ticks->to);
    gwy_debug("from %g, to %g", ticks->from, ticks->to);
}

/* This must be called after find_hruler_ticks().  For unit consistency, we
 * choose the units in the horizontal ruler and force the same here. */
static void
find_vruler_ticks(const ImgExportArgs *args, ImgExportSizes *sizes,
                  PangoLayout *layout, GString *s)
{
    gdouble size = sizes->image.h;
    gdouble real = gwy_data_field_get_yreal(args->dfield);
    gdouble offset = gwy_data_field_get_yoffset(args->dfield);
    RulerTicks *ticks = &sizes->vruler_ticks;
    PangoRectangle logical1, logical2;
    GwySIValueFormat *vf;
    gdouble height, bs, width;

    *ticks = sizes->hruler_ticks;
    vf = sizes->vf_vruler = gwy_si_unit_value_format_copy(sizes->vf_hruler);
    offset /= vf->magnitude;
    real /= vf->magnitude;
    format_layout(layout, &logical1, s, "%.*f", vf->precision, offset);
    gwy_debug("top '%s'", s->str);
    format_layout(layout, &logical2, s, "%.*f", vf->precision, offset + real);
    gwy_debug("last '%s'", s->str);

    height = MAX(logical1.height/pangoscale, logical2.height/pangoscale);
    gwy_debug("label height %g", height);

    /* Fix too dense ticks */
    while (ticks->base*ticks->step/real*size < 1.1*height) {
        if (ticks->step == 1.0)
            ticks->step = 2.0;
        else if (ticks->step == 2.0)
            ticks->step = 5.0;
        else {
            ticks->step = 1.0;
            ticks->base *= 10.0;
            if (vf->precision)
                vf->precision--;
        }
    }
    /* XXX: We also want to fix too sparse ticks but we do not want to make
     * the verical ruler different from the horizontal unless it really looks
     * bad.  So some ‘looks really bad’ criterion is necessary. */
    gwy_debug("base %g, step %g", ticks->base, ticks->step);

    bs = ticks->base * ticks->step;
    ticks->from = ceil(offset/bs - 1e-14)*bs;
    ticks->from = fixzero(ticks->from);
    ticks->to = floor((real + offset)/bs + 1e-14)*bs;
    ticks->to = fixzero(ticks->to);
    gwy_debug("from %g, to %g", ticks->from, ticks->to);

    /* Update widths for the new ticks. */
    format_layout(layout, &logical1, s, "%.*f", vf->precision, ticks->from);
    gwy_debug("top2 '%s'", s->str);
    format_layout(layout, &logical2, s, "%.*f", vf->precision, ticks->to);
    gwy_debug("last2 '%s'", s->str);

    width = MAX(logical1.width/pangoscale, logical2.width/pangoscale);
    sizes->vruler_label_width = width;
}

static void
find_fmscale_ticks(const ImgExportArgs *args, ImgExportSizes *sizes,
                   PangoLayout *layout, GString *s)
{
    GwySIUnit *zunit = gwy_data_field_get_si_unit_z(args->dfield);
    GwyPixmapLayer *layer;
    gdouble size = sizes->image.h;
    gdouble min, max, real;
    RulerTicks *ticks = &sizes->fmruler_ticks;
    PangoRectangle logical1, logical2;
    GwySIValueFormat *vf;
    gdouble bs, height, width;
    guint n;

    layer = gwy_data_view_get_base_layer(args->data_view);
    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    gwy_layer_basic_get_range(GWY_LAYER_BASIC(layer), &min, &max);
    real = MAX(fabs(min), fabs(max));

    sizes->fm_inverted = (max < min);
    sizes->fm_rangetype = gwy_layer_basic_get_range_type(GWY_LAYER_BASIC(layer));

    /* TODO: Handle inverted range! */
    vf = gwy_si_unit_get_format_with_resolution(zunit,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                real, real/240,
                                                NULL);
    sizes->vf_fmruler = vf;
    gwy_debug("unit '%s'", vf->units);
    min /= vf->magnitude;
    max /= vf->magnitude;
    real /= vf->magnitude;
    /* TODO: Do this when we place units to the ticks.  We can also place them
     * to the axis label. */
    format_layout(layout, &logical1, s, "%.*f %s",
                  vf->precision, max, vf->units);
    gwy_debug("max '%s'", s->str);
    format_layout(layout, &logical2, s, "%.*f", vf->precision, min);
    gwy_debug("min '%s'", s->str);

    width = MAX(logical1.width/pangoscale, logical2.width/pangoscale);
    sizes->fmruler_label_width = width;
    height = MAX(logical1.height/pangoscale, logical2.height/pangoscale);
    gwy_debug("label width %g, height %g", width, height);
    n = CLAMP(GWY_ROUND(size/height), 1, 10);
    gwy_debug("nticks %u", n);
    ticks->step = real/n;
    ticks->base = pow10(floor(log10(ticks->step)));
    ticks->step /= ticks->base;
    if (ticks->step <= 2.0)
        ticks->step = 2.0;
    else if (ticks->step <= 5.0)
        ticks->step = 5.0;
    else {
        ticks->base *= 10.0;
        ticks->step = 1.0;
        if (vf->precision)
            vf->precision--;
    }
    gwy_debug("base %g, step %g", ticks->base, ticks->step);

    /* XXX: This is rudimentary.  Must create the end-axis ticks! */
    bs = ticks->base * ticks->step;
    ticks->from = ceil(min/bs - 1e-14)*bs;
    ticks->from = fixzero(ticks->from);
    ticks->to = floor(max/bs + 1e-14)*bs;
    ticks->to = fixzero(ticks->to);
    gwy_debug("from %g, to %g", ticks->from, ticks->to);
}

static void
measure_inset(const ImgExportArgs *args, ImgExportSizes *sizes,
              PangoLayout *layout, GString *s)
{
    ImgExportRect *rect = &sizes->inset;
    gdouble hsize = sizes->image.w, vsize = sizes->image.h;
    gdouble real = gwy_data_field_get_xreal(args->dfield);
    PangoRectangle logical;
    InsetPosType pos = args->inset_pos;
    gdouble lw = args->lw;

    sizes->inset_length = inset_length_ok(args->dfield, args->inset_length);
    if (!(sizes->inset_length > 0.0))
        return;

    rect->w = sizes->inset_length/real*(hsize - 2.0*lw);
    rect->h = lw;
    if (args->inset_draw_ticks)
        rect->h += TICK_LENGTH;

    if (args->inset_draw_label) {
        format_layout(layout, &logical, s, "%s", args->inset_length);
        rect->w = MAX(rect->w, logical.width/pangoscale);
        rect->h += logical.height/pangoscale + lw;
    }

    /* TODO: split horizontal and vertical gap */
    if (pos == INSET_POS_TOP_LEFT
        || pos == INSET_POS_TOP_CENTER
        || pos == INSET_POS_TOP_RIGHT)
        rect->y = lw + TICK_LENGTH*args->inset_gap;
    else
        rect->y = vsize - lw - rect->h - TICK_LENGTH*args->inset_gap;

    if (pos == INSET_POS_TOP_LEFT || pos == INSET_POS_BOTTOM_LEFT)
        rect->x = 2.0*lw + TICK_LENGTH*args->inset_gap;
    else if (pos == INSET_POS_TOP_RIGHT || pos == INSET_POS_BOTTOM_RIGHT)
        rect->x = hsize - 2.0*lw - rect->w - TICK_LENGTH*args->inset_gap;
    else
        rect->x = hsize/2 - 0.5*rect->w;
}

static ImgExportSizes*
calculate_sizes(const ImgExportArgs *args,
                const gchar *name)
{
    PangoLayout *layout;
    ImgExportSizes *sizes = g_new0(ImgExportSizes, 1);
    guint xres = gwy_data_field_get_xres(args->dfield);
    guint yres = gwy_data_field_get_yres(args->dfield);
    gdouble xreal = gwy_data_field_get_xreal(args->dfield);
    gdouble yreal = gwy_data_field_get_yreal(args->dfield);
    GString *s = g_string_new(NULL);
    gdouble lw = args->lw;
    cairo_surface_t *surface;
    cairo_t *cr;

    surface = create_surface(args, name, NULL, 0.0, 0.0);
    g_return_val_if_fail(surface, NULL);
    cr = cairo_create(surface);
    layout = create_layout(args->font, args->font_size, cr);

    /* Data */
    if (args->realsquare) {
        gdouble scale = MAX(xres/xreal, yres/yreal);
        /* This is how GwyDataView rounds it so we should get a pixmap of
         * this size. */
        sizes->image.w = GWY_ROUND(xreal*scale);
        sizes->image.h = GWY_ROUND(yreal*scale);
    }
    else {
        sizes->image.w = xres;
        sizes->image.h = yres;
    }
    sizes->image.w += 2.0*lw;
    sizes->image.h += 2.0*lw;

    /* Horizontal ruler */
    find_hruler_ticks(args, sizes, layout, s);
    sizes->hruler.w = sizes->image.w;
    sizes->hruler.h = sizes->hruler_label_height + TICK_LENGTH + lw;

    /* Vertical ruler */
    find_vruler_ticks(args, sizes, layout, s);
    sizes->vruler.w = sizes->vruler_label_width + TICK_LENGTH + lw;
    sizes->vruler.h = sizes->image.h;

    sizes->hruler.x += sizes->vruler.w;
    sizes->vruler.y += sizes->hruler.h;
    sizes->image.x += sizes->vruler.w;
    sizes->image.y += sizes->hruler.h;

    /* Inset scale bar */
    measure_inset(args, sizes, layout, s);
    sizes->inset.x += sizes->image.x;
    sizes->inset.y += sizes->image.y;

    /* False colour gradient */
    sizes->fmgrad = sizes->image;
    /* NB: We subtract lw here to make the fmscale visually just touch the
     * image in the case of zero gap. */
    sizes->fmgrad.x += sizes->image.w + TICK_LENGTH*args->fmscale_gap - lw;
    sizes->fmgrad.w = 1.5*args->font_size + 2.0*lw;

    /* False colour axis */
    find_fmscale_ticks(args, sizes, layout, s);
    sizes->fmruler.x = sizes->fmgrad.x + sizes->fmgrad.w;
    sizes->fmruler.y = sizes->fmgrad.y;
    sizes->fmruler.w = sizes->fmruler_label_width;
    sizes->fmruler.h = sizes->fmgrad.h;

    /* Canvas */
    sizes->canvas.w = sizes->fmruler.x + sizes->fmruler.w;
    sizes->canvas.h = sizes->image.y + sizes->image.h;

    g_string_free(s, TRUE);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return sizes;
}

static void
destroy_sizes(ImgExportSizes *sizes)
{
    if (sizes->vf_hruler)
        gwy_si_unit_value_format_free(sizes->vf_hruler);
    if (sizes->vf_vruler)
        gwy_si_unit_value_format_free(sizes->vf_vruler);
    if (sizes->vf_fmruler)
        gwy_si_unit_value_format_free(sizes->vf_fmruler);
    g_free(sizes);
}

static void
pixmap_draw_data(const ImgExportArgs *args,
                 const ImgExportSizes *sizes,
                 cairo_t *cr)
{
    const ImgExportRect *rect = &sizes->image;
    GdkPixbuf *pixbuf;
    gdouble lw = args->lw;
    gdouble w, h;

    pixbuf = gwy_data_view_export_pixbuf(args->data_view, 1.0,
                                         args->draw_mask, args->draw_selection);
    w = rect->w - 2.0*lw;
    h = rect->h - 2.0*lw;

    cairo_save(cr);
    cairo_rectangle(cr, rect->x + lw, rect->y + lw, w, h);
    cairo_clip(cr);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, rect->x + lw, rect->y + lw);
    /* Pixelated zoom, this is what we usually want for data.  But we can
     * make it configurable. */
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_restore(cr);
    g_object_unref(pixbuf);

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, lw);
    cairo_rectangle(cr, rect->x + 0.5*lw, rect->y + 0.5*lw, w + lw, h + lw);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void
pixmap_draw_hruler(const ImgExportArgs *args,
                   const ImgExportSizes *sizes,
                   PangoLayout *layout,
                   GString *s,
                   cairo_t *cr)
{
    gdouble xreal = gwy_data_field_get_xreal(args->dfield);
    gdouble xoffset = gwy_data_field_get_xoffset(args->dfield);
    const ImgExportRect *rect = &sizes->hruler;
    const RulerTicks *ticks = &sizes->hruler_ticks;
    GwySIValueFormat *vf = sizes->vf_hruler;
    gdouble lw = args->lw;
    gdouble x, bs, scale, ximg;
    gboolean units_placed = FALSE;

    scale = (rect->w - 2.0*lw)/(xreal/vf->magnitude);
    bs = ticks->step*ticks->base;

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, lw);
    for (x = ticks->from; x <= ticks->to + 1e-14*bs; x += bs) {
        ximg = (x - xoffset)*scale + rect->x + lw;
        gwy_debug("x %g -> %g", x, ximg);
        cairo_move_to(cr, ximg, rect->h);
        cairo_line_to(cr, ximg, rect->h - TICK_LENGTH);
    };
    cairo_stroke(cr);
    cairo_restore(cr);

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    for (x = ticks->from; x <= ticks->to + 1e-14*bs; x += bs) {
        PangoRectangle logical;

        x = fixzero(x);
        ximg = (x - xoffset)*scale + rect->x + lw;
        if (!units_placed && (x >= 0.0 || ticks->to <= -1e-14)) {
            format_layout(layout, &logical, s, "%.*f %s",
                          vf->precision, x, vf->units);
            units_placed = TRUE;
        }
        else
            format_layout(layout, &logical, s, "%.*f", vf->precision, x);

        if (ximg + logical.width/pangoscale <= rect->w) {
            cairo_move_to(cr, ximg, rect->h - TICK_LENGTH - lw);
            cairo_rel_move_to(cr, 0.0, -logical.height/pangoscale);
            pango_cairo_show_layout(cr, layout);
        }
    };
    cairo_restore(cr);
}

static void
pixmap_draw_vruler(const ImgExportArgs *args,
                   const ImgExportSizes *sizes,
                   PangoLayout *layout,
                   GString *s,
                   cairo_t *cr)
{
    gdouble yreal = gwy_data_field_get_yreal(args->dfield);
    gdouble yoffset = gwy_data_field_get_yoffset(args->dfield);
    const ImgExportRect *rect = &sizes->vruler;
    const RulerTicks *ticks = &sizes->vruler_ticks;
    GwySIValueFormat *vf = sizes->vf_vruler;
    gdouble lw = args->lw;
    gdouble y, bs, scale, yimg;

    scale = (rect->h - 2.0*lw)/(yreal/vf->magnitude);
    bs = ticks->step*ticks->base;

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, lw);
    for (y = ticks->from; y <= ticks->to + 1e-14*bs; y += bs) {
        yimg = (y - yoffset)*scale + rect->y + lw;
        gwy_debug("y %g -> %g", y, yimg);
        cairo_move_to(cr, rect->w, yimg);
        cairo_line_to(cr, rect->w - TICK_LENGTH, yimg);
    };
    cairo_stroke(cr);
    cairo_restore(cr);

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    for (y = ticks->from; y <= ticks->to + 1e-14*bs; y += bs) {
        PangoRectangle logical;

        y = fixzero(y);
        yimg = (y - yoffset)*scale + rect->y + lw;
        format_layout(layout, &logical, s, "%.*f", vf->precision, y);
        if (yimg + logical.height/pangoscale <= rect->h) {
            cairo_move_to(cr, rect->w - TICK_LENGTH - lw, yimg);
            cairo_rel_move_to(cr, -logical.width/pangoscale, 0.0);
            pango_cairo_show_layout(cr, layout);
        }
    };
    cairo_restore(cr);
}

static void
pixmap_draw_inset(const ImgExportArgs *args,
                  const ImgExportSizes *sizes,
                  PangoLayout *layout,
                  GString *s,
                  cairo_t *cr)
{
    gdouble xreal = gwy_data_field_get_xreal(args->dfield);
    const ImgExportRect *rect = &sizes->inset, *imgrect = &sizes->image;
    const GwyRGBA *colour = &args->inset_color;
    PangoRectangle logical;
    gdouble lw = args->lw;
    gdouble xcentre, length, y, w, h;

    length = (sizes->image.w - 2.0*lw)/xreal*sizes->inset_length;
    xcentre = rect->x + 0.5*rect->w;
    y = 0.5*lw;

    w = imgrect->w - 2.0*lw;
    h = imgrect->h - 2.0*lw;

    cairo_save(cr);
    cairo_rectangle(cr, imgrect->x + lw, imgrect->y + lw, w, h);
    cairo_clip(cr);
    cairo_set_source_rgba(cr, colour->r, colour->g, colour->b, colour->a);
    cairo_set_line_width(cr, lw);
    if (args->inset_draw_ticks) {
        cairo_move_to(cr, xcentre - 0.5*length, rect->y);
        cairo_rel_line_to(cr, 0.0, TICK_LENGTH);
        cairo_move_to(cr, xcentre + 0.5*length, rect->y);
        cairo_rel_line_to(cr, 0.0, TICK_LENGTH);
        y = 0.5*TICK_LENGTH;
    }
    cairo_move_to(cr, xcentre - 0.5*length, rect->y + y);
    cairo_line_to(cr, xcentre + 0.5*length, rect->y + y);
    cairo_stroke(cr);
    cairo_restore(cr);

    if (args->inset_draw_ticks)
        y = TICK_LENGTH + lw;
    else
        y = 2.0*lw;

    if (!args->inset_draw_label)
        return;

    cairo_save(cr);
    cairo_rectangle(cr, imgrect->x + lw, imgrect->y + lw, w, h);
    cairo_clip(cr);
    cairo_set_source_rgba(cr, colour->r, colour->g, colour->b, colour->a);
    format_layout(layout, &logical, s, "%s", args->inset_length);
    cairo_move_to(cr, xcentre - 0.5*logical.width/pangoscale, rect->y + y);
    pango_cairo_show_layout(cr, layout);
    cairo_restore(cr);
}

static void
pixmap_draw_fmgrad(GwyContainer *data,
                   const ImgExportArgs *args,
                   const ImgExportSizes *sizes,
                   cairo_t *cr)
{
    GwyPixmapLayer *layer;
    const ImgExportRect *rect = &sizes->fmgrad;
    GwyGradient *gradient;
    const GwyGradientPoint *points;
    cairo_pattern_t *pat;
    const guchar *name = NULL, *key;
    gint w, h, npoints, i;
    gdouble lw = args->lw;
    gboolean inverted = sizes->fm_inverted;

    layer = gwy_data_view_get_base_layer(args->data_view);
    key = gwy_layer_basic_get_gradient_key(GWY_LAYER_BASIC(layer));
    if (key)
        gwy_container_gis_string_by_name(data, key, &name);
    gradient = gwy_gradients_get_gradient(name);
    points = gwy_gradient_get_points(gradient, &npoints);

    w = rect->w - 2.0*lw;
    h = rect->h - 2.0*lw;

    if (inverted)
        pat = cairo_pattern_create_linear(rect->x, rect->y + lw,
                                          rect->x, rect->y + h);
    else
        pat = cairo_pattern_create_linear(rect->x, rect->y + h,
                                          rect->x, rect->y + lw);
    for (i = 0; i < npoints; i++) {
        const GwyGradientPoint *gpt = points + i;
        const GwyRGBA *color = &gpt->color;

        cairo_pattern_add_color_stop_rgb(pat, gpt->x,
                                         color->r, color->g, color->b);
    }
    cairo_pattern_set_filter(pat, CAIRO_FILTER_BILINEAR);

    cairo_save(cr);
    cairo_rectangle(cr, rect->x + lw, rect->y + lw, w, h);
    cairo_clip(cr);
    cairo_set_source(cr, pat);
    cairo_paint(cr);
    cairo_restore(cr);

    cairo_pattern_destroy(pat);

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, lw);
    cairo_rectangle(cr, rect->x + 0.5*lw, rect->y + 0.5*lw, w + lw, h + lw);
    cairo_stroke(cr);
    cairo_restore(cr);
}

/* We assume cr is already created for the layout with the correct scale(!). */
static void
image_draw_cairo(GwyContainer *data,
                 const ImgExportArgs *args,
                 const ImgExportSizes *sizes,
                 cairo_t *cr)
{
    PangoLayout *layout;
    GString *s = g_string_new(NULL);

    layout = create_layout(args->font, args->font_size, cr);

    pixmap_draw_data(args, sizes, cr);
    pixmap_draw_hruler(args, sizes, layout, s, cr);
    pixmap_draw_vruler(args, sizes, layout, s, cr);
    pixmap_draw_inset(args, sizes, layout, s, cr);
    pixmap_draw_fmgrad(data, args, sizes, cr);

    g_object_unref(layout);
    g_string_free(s, TRUE);
}

static GdkPixbuf*
render_pixbuf(const ImgExportArgs *args, const gchar *name)
{
    ImgExportSizes *sizes;
    cairo_surface_t *surface;
    GdkPixbuf *pixbuf;
    guchar *imgdata, *pixels;
    guint xres, yres, imgrowstride, pixrowstride, i, j;
    cairo_format_t imgformat;
    cairo_t *cr;

    sizes = calculate_sizes(args, name);
    g_return_val_if_fail(sizes, FALSE);
    surface = create_surface(args, name, NULL,
                             args->zoom*sizes->canvas.w,
                             args->zoom*sizes->canvas.h);
    cr = cairo_create(surface);
    cairo_scale(cr, args->zoom, args->zoom);
    image_draw_cairo(args->data, args, sizes, cr);
    cairo_surface_flush(surface);
    cairo_destroy(cr);

    imgdata = cairo_image_surface_get_data(surface);
    xres = cairo_image_surface_get_width(surface);
    yres = cairo_image_surface_get_height(surface);
    imgrowstride = cairo_image_surface_get_stride(surface);
    imgformat = cairo_image_surface_get_format(surface);
    g_return_val_if_fail(imgformat == CAIRO_FORMAT_RGB24, NULL);
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, xres, yres);
    pixrowstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    for (i = 0; i < yres; i++) {
        const guchar *p = imgdata + i*imgrowstride;
        guchar *q = pixels + i*pixrowstride;

        if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
            for (j = xres; j; j--, p += 4, q += 3) {
                *q = *(p + 2);
                *(q + 1) = *(p + 1);
                *(q + 2) = *p;
            }
        }
        else {
            for (j = xres; j; j--, p += 4, q += 3) {
                *q = *(p + 1);
                *(q + 1) = *(p + 2);
                *(q + 2) = *(p + 3);
            }
        }
    }

    cairo_surface_destroy(surface);
    destroy_sizes(sizes);

    return pixbuf;
}

/*
 * How to determine the size?
 * - Axes and scales dimensions depend on the text dimensions.
 * - We can measure text in Pango units (convertible with PANGO_PIXELS to
 *   device units: pixels or points) and only need a layout.
 * - For PangoCairo rendering the layout must be created with
 *   pango_cairo_create_layout(cr), i.e. we need a Cairo context.
 * - Cairo context can be only created for a Cairo surface.
 * - This is a cyclic dependence.
 *
 * So we create text-measuring Cairo surface of more or less random dimensions.
 * It is of the same type but not used for any rendering.  We measure labels
 * using this surface and calculate positions and sizes of all components of
 * the exported image.  Then we actually render them.
 *
 * To avoid doing everything twice, the measurement phase should provide
 * directly interpretable information about positions and sizes.
 **/
static gboolean
img_export_export(GwyContainer *data,
                  const gchar *filename,
                  GwyRunType mode,
                  GError **error,
                  const gchar *name)
{
    GwyContainer *settings;
    ImgExportArgs args;
    const ImgExportFormat *format;
    gboolean ok;

    settings = gwy_app_settings_get();
    img_export_load_args(settings, &args);
    format = args.format = find_format(name, TRUE);
    g_return_val_if_fail(args.format, FALSE);

    args.font_size = 12.0; // XXX XXX XXX
    args.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &args.data_view,
                                     GWY_APP_DATA_FIELD, &args.dfield,
                                     GWY_APP_DATA_FIELD_ID, &args.id,
                                     0);

    // TODO: Run some GUI here.
    ok = TRUE;

    if (ok) {
        if (format->write_vector)
            ok = format->write_vector(&args, format->name, filename, error);
        else if (format->write_grey16 && args.greyscale)
            ok = format->write_grey16(&args, format->name, filename, error);
        else if (format->write_pixbuf) {
            GdkPixbuf *pixbuf = render_pixbuf(&args, format->name);
            ok = format->write_pixbuf(pixbuf, format->name, filename, error);
            g_object_unref(pixbuf);
        }
        else {
            ok = FALSE;
            g_assert_not_reached();
        }
    }
    else {
        err_CANCELLED(error);
    }

    img_export_save_args(settings, &args);
    img_export_free_args(&args);

    return ok;
}

static guint16*
render_image_grey16(GwyDataField *dfield)
{
    guint xres = gwy_data_field_get_xres(dfield);
    guint yres = gwy_data_field_get_yres(dfield);
    gdouble min, max;
    guint16 *pixels;

    pixels = g_new(guint16, xres*yres);
    gwy_data_field_get_min_max(dfield, &min, &max);
    if (min == max)
        memset(pixels, 0, xres*yres*sizeof(guint16));
    else {
        const gdouble *d = gwy_data_field_get_data_const(dfield);
        gdouble q = 65535.999999/(max - min);
        guint i;

        for (i = 0; i < xres*yres; i++)
            pixels[i] = (guint16)(q*(d[i] - min));
    }

    return pixels;
}

#ifdef HAVE_PNG
static void
add_png_text_chunk_string(png_text *chunk,
                          const gchar *key,
                          const gchar *str,
                          gboolean take)
{
    chunk->compression = PNG_TEXT_COMPRESSION_NONE;
    chunk->key = (char*)key;
    chunk->text = take ? (char*)str : g_strdup(str);
    chunk->text_length = strlen(chunk->text);
}

static void
add_png_text_chunk_float(png_text *chunk,
                         const gchar *key,
                         gdouble value)
{
    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];

    chunk->compression = PNG_TEXT_COMPRESSION_NONE;
    chunk->key = (char*)key;
    g_ascii_dtostr(buffer, sizeof(buffer), value);
    chunk->text = g_strdup(buffer);
    chunk->text_length = strlen(chunk->text);
}

static gboolean
write_image_png16(const ImgExportArgs *args,
                  const gchar *name,
                  const gchar *filename,
                  GError **error)
{
    enum { NCHUNKS = 11 };

    const guchar *title = "Data";

    GwyDataField *dfield = args->dfield;
    guint xres = gwy_data_field_get_xres(dfield);
    guint yres = gwy_data_field_get_yres(dfield);
    guint16 *pixels;
    png_structp writer;
    png_infop writer_info;
    png_byte **rows = NULL;
    png_text *text_chunks = NULL;
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    guint transform_flags = PNG_TRANSFORM_SWAP_ENDIAN;
#endif
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    guint transform_flags = PNG_TRANSFORM_IDENTITY;
#endif
    /* A bit of convoluted typing to get a png_charpp equivalent. */
    gchar param0[G_ASCII_DTOSTR_BUF_SIZE], param1[G_ASCII_DTOSTR_BUF_SIZE];
    gchar *s, *params[2];
    gdouble min, max;
    gboolean ok = FALSE;
    FILE *fh;
    guint i;

    g_return_val_if_fail(gwy_strequal(name, "pngcairo"), FALSE);

    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    writer = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!writer) {
        fclose(fh);
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("libpng initialization error (in %s)"),
                    "png_create_write_struct");
        return FALSE;
    }

    writer_info = png_create_info_struct(writer);
    if (!writer_info) {
        fclose(fh);
        png_destroy_read_struct(&writer, NULL, NULL);
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("libpng initialization error (in %s)"),
                    "png_create_info_struct");
        return FALSE;
    }

    gwy_data_field_get_min_max(dfield, &min, &max);
    s = g_strdup_printf("/%d/data/title", args->id);
    gwy_container_gis_string_by_name(args->data, s, &title);
    g_free(s);

    /* Create the chunks dynamically because the fields of png_text are
     * variable. */
    text_chunks = g_new0(png_text, NCHUNKS);
    i = 0;
    /* Standard PNG keys */
    add_png_text_chunk_string(text_chunks + i++, "Title", title, FALSE);
    add_png_text_chunk_string(text_chunks + i++, "Software", "Gwyddion", FALSE);
    /* Gwyddion GSF keys */
    gwy_data_field_get_min_max(dfield, &min, &max);
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_XREAL,
                             gwy_data_field_get_xreal(dfield));
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_YREAL,
                             gwy_data_field_get_yreal(dfield));
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_XOFFSET,
                             gwy_data_field_get_xoffset(dfield));
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_YOFFSET,
                             gwy_data_field_get_yoffset(dfield));
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_ZMIN, min);
    add_png_text_chunk_float(text_chunks + i++, GWY_IMGKEY_ZMAX, max);
    s = gwy_si_unit_get_string(gwy_data_field_get_si_unit_xy(dfield),
                               GWY_SI_UNIT_FORMAT_PLAIN);
    add_png_text_chunk_string(text_chunks + i++, GWY_IMGKEY_XYUNIT, s, TRUE);
    s = gwy_si_unit_get_string(gwy_data_field_get_si_unit_z(dfield),
                               GWY_SI_UNIT_FORMAT_PLAIN);
    add_png_text_chunk_string(text_chunks + i++, GWY_IMGKEY_ZUNIT, s, TRUE);
    add_png_text_chunk_string(text_chunks + i++, GWY_IMGKEY_TITLE, title, FALSE);
    g_assert(i == NCHUNKS);

    png_set_text(writer, writer_info, text_chunks, NCHUNKS);

    /* Present the scaling information also as calibration chunks.
     * Unfortunately, they cannot represent it fully – the rejected xCAL and
     * yCAL chunks would be necessary for that. */
    png_set_sCAL(writer, writer_info, PNG_SCALE_METER,  /* Usually... */
                 gwy_data_field_get_xreal(dfield),
                 gwy_data_field_get_yreal(dfield));
    s = gwy_si_unit_get_string(gwy_data_field_get_si_unit_z(dfield),
                               GWY_SI_UNIT_FORMAT_PLAIN);
    g_ascii_dtostr(param0, sizeof(param0), min);
    g_ascii_dtostr(param1, sizeof(param1), (max - min)/G_MAXUINT16);
    params[0] = param0;
    params[1] = param1;
    png_set_pCAL(writer, writer_info, "Z", 0, G_MAXUINT16, 0, 2, s, params);
    g_free(s);

    pixels = render_image_grey16(dfield);
    rows = g_new(png_bytep, yres);
    for (i = 0; i < yres; i++)
        rows[i] = (png_bytep)pixels + i*xres*sizeof(guint16);

    if (setjmp(png_jmpbuf(writer))) {
        /* FIXME: Not very helpful.  Thread-unsafe. */
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("libpng error occured"));
        goto end;
    }

    png_init_io(writer, fh);
    png_set_filter(writer, 0, PNG_ALL_FILTERS);
    png_set_compression_level(writer, Z_BEST_COMPRESSION);
    png_set_IHDR(writer, writer_info, xres, yres,
                 16, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    /* XXX */
    png_set_rows(writer, writer_info, rows);
    png_write_png(writer, writer_info, transform_flags, NULL);

    ok = TRUE;

end:
    fclose(fh);
    g_free(rows);
    g_free(pixels);
    png_destroy_write_struct(&writer, &writer_info);
    for (i = 0; i < NCHUNKS; i++)
        g_free(text_chunks[i].text);
    g_free(text_chunks);

    return ok;
}
#endif

/* Expand a word and double-word into LSB-ordered sequence of bytes */
#define W(x) (x)&0xff, (x)>>8
#define Q(x) (x)&0xff, ((x)>>8)&0xff, ((x)>>16)&0xff, (x)>>24

static gboolean
write_image_tiff16(const ImgExportArgs *args,
                   const gchar *name,
                   const gchar *filename,
                   GError **error)
{
    enum {
        N_ENTRIES = 11,
        ESTART = 4 + 4 + 2,
        HEAD_SIZE = ESTART + 12*N_ENTRIES + 4,  /* head + 0th directory */
        /* offsets of things we have to fill run-time */
        WIDTH_OFFSET = ESTART + 12*0 + 8,
        HEIGHT_OFFSET = ESTART + 12*1 + 8,
        BPS_OFFSET = ESTART + 12*2 + 8,
        ROWS_OFFSET = ESTART + 12*8 + 8,
        BYTES_OFFSET = ESTART + 12*9 + 8,
        BIT_DEPTH = 16,
    };

    static guchar tiff_head[] = {
        0x49, 0x49,   /* magic (LSB) */
        W(42),        /* more magic */
        Q(8),         /* 0th directory offset */
        W(N_ENTRIES), /* number of entries */
        W(GWY_TIFFTAG_IMAGE_WIDTH), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_IMAGE_LENGTH), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_BITS_PER_SAMPLE), W(GWY_TIFF_SHORT), Q(1), Q(BIT_DEPTH),
        W(GWY_TIFFTAG_COMPRESSION), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_COMPRESSION_NONE),
        W(GWY_TIFFTAG_PHOTOMETRIC), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_PHOTOMETRIC_MIN_IS_BLACK),
        W(GWY_TIFFTAG_STRIP_OFFSETS), W(GWY_TIFF_LONG), Q(1), Q(HEAD_SIZE),
        W(GWY_TIFFTAG_ORIENTATION), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_ORIENTATION_TOPLEFT),
        W(GWY_TIFFTAG_SAMPLES_PER_PIXEL), W(GWY_TIFF_SHORT), Q(1), Q(1),
        W(GWY_TIFFTAG_ROWS_PER_STRIP), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_STRIP_BYTE_COUNTS), W(GWY_TIFF_LONG), Q(1), Q(0),
        W(GWY_TIFFTAG_PLANAR_CONFIG), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_PLANAR_CONFIG_CONTIGNUOUS),
        Q(0),              /* next directory (0 = none) */
        /* Here start the image data */
    };

    GwyDataField *dfield = args->dfield;
    guint xres = gwy_data_field_get_xres(dfield);
    guint yres = gwy_data_field_get_yres(dfield);
    guint nbytes = BIT_DEPTH*xres*yres;
    guint16 *pixels;
    FILE *fh;

    g_return_val_if_fail(gwy_strequal(name, "tiffcairo"), FALSE);

    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    *(guint32*)(tiff_head + WIDTH_OFFSET) = GUINT32_TO_LE(xres);
    *(guint32*)(tiff_head + HEIGHT_OFFSET) = GUINT32_TO_LE(yres);
    *(guint32*)(tiff_head + ROWS_OFFSET) = GUINT32_TO_LE(yres);
    *(guint32*)(tiff_head + BYTES_OFFSET) = GUINT32_TO_LE(nbytes);

    if (fwrite(tiff_head, 1, sizeof(tiff_head), fh) != sizeof(tiff_head)) {
        err_WRITE(error);
        fclose(fh);
        return FALSE;
    }

    pixels = render_image_grey16(dfield);
    if (fwrite(pixels, sizeof(guint16), xres*yres, fh) != xres*yres) {
        err_WRITE(error);
        fclose(fh);
        g_free(pixels);
        return FALSE;
    }

    fclose(fh);
    g_free(pixels);

    return TRUE;
}

#undef Q
#undef W

static void
add_ppm_comment_string(GString *str,
                       const gchar *key,
                       const gchar *value,
                       gboolean take)
{
    g_string_append_printf(str, "# %s %s\n", key, value);
    if (take)
        g_free((gpointer)value);
}

static void
add_ppm_comment_float(GString *str,
                      const gchar *key,
                      gdouble value)
{
    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];

    g_ascii_dtostr(buffer, sizeof(buffer), value);
    g_string_append_printf(str, "# %s %s\n", key, buffer);
}

static gboolean
write_image_pgm16(const ImgExportArgs *args,
                  const gchar *name,
                  const gchar *filename,
                  GError **error)
{
    static const gchar pgm_header[] = "P5\n%s%u\n%u\n65535\n";
    const guchar *title = "Data";

    GwyDataField *dfield;
    guint xres, yres, i;
    gdouble min, max;
    gboolean ok = FALSE;
    gchar *s, *ppmh = NULL;
    GString *str;
    guint16 *pixels;
    FILE *fh;

    g_return_val_if_fail(gwy_strequal(name, "pnmcairo"), FALSE);

    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    dfield = args->dfield;
    pixels = render_image_grey16(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_data_field_get_min_max(dfield, &min, &max);

    s = g_strdup_printf("/%d/data/title", args->id);
    gwy_container_gis_string_by_name(args->data, s, &title);
    g_free(s);

    /* Gwyddion GSF keys */
    str = g_string_new(NULL);
    add_ppm_comment_float(str, GWY_IMGKEY_XREAL,
                          gwy_data_field_get_xreal(dfield));
    add_ppm_comment_float(str, GWY_IMGKEY_YREAL,
                          gwy_data_field_get_yreal(dfield));
    add_ppm_comment_float(str, GWY_IMGKEY_XOFFSET,
                          gwy_data_field_get_xoffset(dfield));
    add_ppm_comment_float(str, GWY_IMGKEY_YOFFSET,
                          gwy_data_field_get_yoffset(dfield));
    add_ppm_comment_float(str, GWY_IMGKEY_ZMIN, min);
    add_ppm_comment_float(str, GWY_IMGKEY_ZMAX, max);
    s = gwy_si_unit_get_string(gwy_data_field_get_si_unit_xy(dfield),
                               GWY_SI_UNIT_FORMAT_PLAIN);
    add_ppm_comment_string(str, GWY_IMGKEY_XYUNIT, s, TRUE);
    s = gwy_si_unit_get_string(gwy_data_field_get_si_unit_z(dfield),
                               GWY_SI_UNIT_FORMAT_PLAIN);
    add_ppm_comment_string(str, GWY_IMGKEY_ZUNIT, s, TRUE);
    add_ppm_comment_string(str, GWY_IMGKEY_TITLE, title, FALSE);

    ppmh = g_strdup_printf(pgm_header, str->str, xres, yres);
    g_string_free(str, TRUE);

    if (fwrite(ppmh, 1, strlen(ppmh), fh) != strlen(ppmh)) {
        err_WRITE(error);
        goto end;
    }

    if (G_BYTE_ORDER != G_BIG_ENDIAN) {
        for (i = 0; i < xres*yres; i++)
            pixels[i] = GUINT16_TO_BE(pixels[i]);
    }

    if (fwrite(pixels, sizeof(guint16), xres*yres, fh) != xres*yres) {
        err_WRITE(error);
        goto end;
    }
    ok = TRUE;

end:
    g_free(pixels);
    g_free(ppmh);
    fclose(fh);

    return ok;
}

static gboolean
write_vector_generic(const ImgExportArgs *args,
                     const gchar *name,
                     const gchar *filename,
                     GError **error)
{
    gboolean ok = TRUE;
    ImgExportSizes *sizes;
    cairo_surface_t *surface;
    cairo_status_t status;
    cairo_t *cr;

    /* FIXME: We need some size determination method for vector drawings.
     * Note this requires changing the canvas and the cairo scale transform. */
    sizes = calculate_sizes(args, name);
    g_return_val_if_fail(sizes, FALSE);
    surface = create_surface(args, name, filename,
                             sizes->canvas.w, sizes->canvas.h);
    g_return_val_if_fail(surface, FALSE);
    cr = cairo_create(surface);
    image_draw_cairo(args->data, args, sizes, cr);
    cairo_surface_flush(surface);
    if ((status = cairo_status(cr))
        || (status = cairo_surface_status(surface))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Cairo error occurred: %s"),
                    cairo_status_to_string(status));
        ok = FALSE;
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    destroy_sizes(sizes);

    return ok;
}

static gboolean
write_pixbuf_generic(GdkPixbuf *pixbuf,
                     const gchar *name,
                     const gchar *filename,
                     GError **error)
{
    GError *err = NULL;

    if (gdk_pixbuf_save(pixbuf, filename, name, &err, NULL))
        return TRUE;

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                _("Pixbuf save failed: %s."), err->message);
    g_clear_error(&err);
    return FALSE;
}

/* Expand a word and double-word into LSB-ordered sequence of bytes */
#define W(x) (x)&0xff, (x)>>8
#define Q(x) (x)&0xff, ((x)>>8)&0xff, ((x)>>16)&0xff, (x)>>24

static gboolean
write_pixbuf_tiff(GdkPixbuf *pixbuf,
                  const gchar *name,
                  const gchar *filename,
                  GError **error)
{
    enum {
        N_ENTRIES = 14,
        ESTART = 4 + 4 + 2,
        HEAD_SIZE = ESTART + 12*N_ENTRIES + 4,  /* head + 0th directory */
        /* offsets of things we have to fill run-time */
        WIDTH_OFFSET = ESTART + 12*0 + 8,
        HEIGHT_OFFSET = ESTART + 12*1 + 8,
        ROWS_OFFSET = ESTART + 12*8 + 8,
        BYTES_OFFSET = ESTART + 12*9 + 8,
        BIT_DEPTH = 8,
        NCHANNELS = 3,
    };

    static guchar tiff_head[] = {
        0x49, 0x49,   /* magic (LSB) */
        W(42),        /* more magic */
        Q(8),         /* 0th directory offset */
        W(N_ENTRIES), /* number of entries */
        W(GWY_TIFFTAG_IMAGE_WIDTH), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_IMAGE_LENGTH), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_BITS_PER_SAMPLE), W(GWY_TIFF_SHORT), Q(3), Q(HEAD_SIZE),
        W(GWY_TIFFTAG_COMPRESSION), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_COMPRESSION_NONE),
        W(GWY_TIFFTAG_PHOTOMETRIC), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_PHOTOMETRIC_RGB),
        W(GWY_TIFFTAG_STRIP_OFFSETS), W(GWY_TIFF_LONG), Q(1),
            Q(HEAD_SIZE + 22),
        W(GWY_TIFFTAG_ORIENTATION), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_ORIENTATION_TOPLEFT),
        W(GWY_TIFFTAG_SAMPLES_PER_PIXEL), W(GWY_TIFF_SHORT), Q(1), Q(NCHANNELS),
        W(GWY_TIFFTAG_ROWS_PER_STRIP), W(GWY_TIFF_SHORT), Q(1), Q(0),
        W(GWY_TIFFTAG_STRIP_BYTE_COUNTS), W(GWY_TIFF_LONG), Q(1), Q(0),
        W(GWY_TIFFTAG_X_RESOLUTION), W(GWY_TIFF_RATIONAL), Q(1),
            Q(HEAD_SIZE + 6),
        W(GWY_TIFFTAG_Y_RESOLUTION), W(GWY_TIFF_RATIONAL), Q(1),
            Q(HEAD_SIZE + 14),
        W(GWY_TIFFTAG_PLANAR_CONFIG), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_PLANAR_CONFIG_CONTIGNUOUS),
        W(GWY_TIFFTAG_RESOLUTION_UNIT), W(GWY_TIFF_SHORT), Q(1),
            Q(GWY_TIFF_RESOLUTION_UNIT_INCH),
        Q(0),              /* next directory (0 = none) */
        /* header data */
        W(BIT_DEPTH), W(BIT_DEPTH), W(BIT_DEPTH),
        Q(72), Q(1),       /* x-resolution */
        Q(72), Q(1),       /* y-resolution */
        /* here starts the image data */
    };

    guint xres, yres, rowstride, i, nbytes, nchannels;
    guchar *pixels;
    FILE *fh;

    g_return_val_if_fail(gwy_strequal(name, "tiffcairo"), FALSE);

    nchannels = gdk_pixbuf_get_n_channels(pixbuf);
    g_return_val_if_fail(nchannels == 3, FALSE);

    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    xres = gdk_pixbuf_get_width(pixbuf);
    yres = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    nbytes = xres*yres*NCHANNELS;

    *(guint32*)(tiff_head + WIDTH_OFFSET) = GUINT32_TO_LE(xres);
    *(guint32*)(tiff_head + HEIGHT_OFFSET) = GUINT32_TO_LE(yres);
    *(guint32*)(tiff_head + ROWS_OFFSET) = GUINT32_TO_LE(yres);
    *(guint32*)(tiff_head + BYTES_OFFSET) = GUINT32_TO_LE(nbytes);

    if (fwrite(tiff_head, 1, sizeof(tiff_head), fh) != sizeof(tiff_head)) {
        err_WRITE(error);
        fclose(fh);
        return FALSE;
    }

    for (i = 0; i < yres; i++) {
        if (fwrite(pixels + i*rowstride, NCHANNELS, xres, fh) != xres) {
            err_WRITE(error);
            fclose(fh);
            return FALSE;
        }
    }

    fclose(fh);
    return TRUE;
}

#undef Q
#undef W

static gboolean
write_pixbuf_ppm(GdkPixbuf *pixbuf,
                 const gchar *name,
                 const gchar *filename,
                 GError **error)
{
    static const gchar ppm_header[] = "P6\n%u\n%u\n255\n";

    guint xres, yres, rowstride, nchannels, i;
    guchar *pixels;
    gboolean ok = FALSE;
    gchar *ppmh = NULL;
    FILE *fh;

    g_return_val_if_fail(gwy_strequal(name, "pnmcairo"), FALSE);

    nchannels = gdk_pixbuf_get_n_channels(pixbuf);
    g_return_val_if_fail(nchannels == 3, FALSE);

    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    xres = gdk_pixbuf_get_width(pixbuf);
    yres = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);

    ppmh = g_strdup_printf(ppm_header, xres, yres);
    if (fwrite(ppmh, 1, strlen(ppmh), fh) != strlen(ppmh)) {
        err_WRITE(error);
        goto end;
    }

    for (i = 0; i < yres; i++) {
        if (fwrite(pixels + i*rowstride, nchannels, xres, fh) != xres) {
            err_WRITE(error);
            goto end;
        }
    }

    ok = TRUE;

end:
    fclose(fh);
    g_object_unref(pixbuf);
    g_free(ppmh);
    return ok;
}

static gboolean
write_pixbuf_bmp(GdkPixbuf *pixbuf,
                 const gchar *name,
                 const gchar *filename,
                 GError **error)
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

    guchar *pixels, *buffer = NULL;
    guint i, j, xres, yres, nchannels, rowstride, bmplen, bmprowstride;
    FILE *fh;

    g_return_val_if_fail(gwy_strequal(name, "bmpcairo"), FALSE);

    nchannels = gdk_pixbuf_get_n_channels(pixbuf);
    g_return_val_if_fail(nchannels == 3, FALSE);

    xres = gdk_pixbuf_get_width(pixbuf);
    yres = gdk_pixbuf_get_height(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);

    bmprowstride = ((nchannels*xres + 3)/4)*4;
    bmplen = yres*bmprowstride + sizeof(bmp_head);

    *(guint32*)(bmp_head + 2) = GUINT32_TO_LE(bmplen);
    *(guint32*)(bmp_head + 18) = GUINT32_TO_LE(xres);
    *(guint32*)(bmp_head + 22) = GUINT32_TO_LE(yres);
    *(guint32*)(bmp_head + 34) = GUINT32_TO_LE(yres*bmprowstride);

    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    if (fwrite(bmp_head, 1, sizeof(bmp_head), fh) != sizeof(bmp_head)) {
        err_WRITE(error);
        fclose(fh);
        return FALSE;
    }

    /* The ugly part: BMP uses BGR instead of RGB and is written upside down,
     * this silliness may originate nowhere else than in MS... */
    buffer = g_new(guchar, bmprowstride);
    memset(buffer, 0xff, sizeof(bmprowstride));
    for (i = 0; i < yres; i++) {
        const guchar *p = pixels + (yres-1 - i)*rowstride;
        guchar *q = buffer;

        for (j = xres; j; j--, p += 3, q += 3) {
            *q = *(p + 2);
            *(q + 1) = *(p + 1);
            *(q + 2) = *p;
        }
        if (!fwrite(buffer, 1, bmprowstride, fh) != bmprowstride) {
            err_WRITE(error);
            fclose(fh);
            g_free(buffer);
            return FALSE;
        }
    }
    g_free(buffer);
    fclose(fh);

    return TRUE;
}

static gboolean
write_pixbuf_targa(GdkPixbuf *pixbuf,
                   const gchar *name,
                   const gchar *filename,
                   GError **error)
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

    guchar *pixels, *buffer = NULL;
    guint nchannels, xres, yres, rowstride, i, j;
    FILE *fh;

    g_return_val_if_fail(gwy_strequal(name, "tgacairo"), FALSE);

    nchannels = gdk_pixbuf_get_n_channels(pixbuf);
    g_return_val_if_fail(nchannels == 3, FALSE);

    xres = gdk_pixbuf_get_width(pixbuf);
    yres = gdk_pixbuf_get_height(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);

    if (xres >= 65535 || yres >= 65535) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Image is too large to be stored as TARGA."));
        return FALSE;
    }

    *(guint16*)(targa_head + 12) = GUINT16_TO_LE((guint16)xres);
    *(guint16*)(targa_head + 14) = GUINT16_TO_LE((guint16)yres);

    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    if (fwrite(targa_head, 1, sizeof(targa_head), fh) != sizeof(targa_head)) {
        err_WRITE(error);
        fclose(fh);
        return FALSE;
    }

    /* The ugly part: TARGA uses BGR instead of RGB */
    buffer = g_new(guchar, nchannels*xres);
    memset(buffer, 0xff, nchannels*xres);
    for (i = 0; i < yres; i++) {
        const guchar *p = pixels + i*rowstride;
        guchar *q = buffer;

        for (j = xres; j; j--, p += 3, q += 3) {
            *q = *(p + 2);
            *(q + 1) = *(p + 1);
            *(q + 2) = *p;
        }
        if (fwrite(buffer, nchannels, xres, fh) != xres) {
            err_WRITE(error);
            fclose(fh);
            g_free(buffer);
            return FALSE;
        }
    }
    fclose(fh);
    g_free(buffer);

    return TRUE;
}

/* Use the pixmap prefix for compatibility */
static const gchar draw_mask_key[]        = "/module/pixmap/draw_mask";
static const gchar draw_selection_key[]   = "/module/pixmap/draw_selection";
static const gchar text_antialias_key[]   = "/module/pixmap/text_antialias";
static const gchar font_key[]             = "/module/pixmap/font";
static const gchar font_size_key[]        = "/module/pixmap/font_size";
static const gchar greyscale_key[]        = "/module/pixmap/grayscale";
static const gchar inset_color_key[]      = "/module/pixmap/inset_color";
static const gchar inset_pos_key[]        = "/module/pixmap/inset_pos";
static const gchar inset_draw_ticks_key[] = "/module/pixmap/inset_draw_ticks";
static const gchar inset_draw_label_key[] = "/module/pixmap/inset_draw_label";
static const gchar inset_length_key[]     = "/module/pixmap/inset_length";
static const gchar scale_font_key[]       = "/module/pixmap/scale_font";
static const gchar fmscale_gap_key[]      = "/module/pixmap/fmscale_gap";
static const gchar inset_gap_key[]        = "/module/pixmap/inset_gap";
static const gchar xytype_key[]           = "/module/pixmap/xytype";
static const gchar zoom_key[]             = "/module/pixmap/zoom";
static const gchar ztype_key[]            = "/module/pixmap/ztype";

static void
img_export_sanitize_args(ImgExportArgs *args)
{
    args->xytype = MIN(args->xytype, PIXMAP_SCALEBAR);
    args->ztype = MIN(args->ztype, PIXMAP_FMSCALE);
    args->inset_color.a = 1.0;
    args->inset_pos = MIN(args->inset_pos, INSET_NPOS - 1);
    /* handle inset_length later, its usability depends on the data field. */
    args->zoom = CLAMP(args->zoom, 0.06, 16.0);
    args->draw_mask = !!args->draw_mask;
    args->draw_selection = !!args->draw_selection;
    args->text_antialias = !!args->text_antialias;
    args->scale_font = !!args->scale_font;
    args->inset_draw_ticks = !!args->inset_draw_ticks;
    args->inset_draw_label = !!args->inset_draw_label;
    args->font_size = CLAMP(args->font_size, 1.2, 120.0);
    args->fmscale_gap = CLAMP(args->fmscale_gap, 0.0, 2.0);
    args->inset_gap = CLAMP(args->inset_gap, 0.0, 2.0);
    args->greyscale = (args->greyscale == 16) ? 16 : 0;
}

static void
img_export_free_args(ImgExportArgs *args)
{
    g_free(args->font);
    g_free(args->inset_length);
}

static void
img_export_load_args(GwyContainer *container,
                     ImgExportArgs *args)
{
    *args = img_export_defaults;

    gwy_container_gis_double_by_name(container, zoom_key, &args->zoom);
    gwy_container_gis_enum_by_name(container, xytype_key, &args->xytype);
    gwy_container_gis_enum_by_name(container, ztype_key, &args->ztype);
    gwy_rgba_get_from_container(&args->inset_color, container, inset_color_key);
    gwy_container_gis_enum_by_name(container, inset_pos_key, &args->inset_pos);
    gwy_container_gis_string_by_name(container, inset_length_key,
                                     (const guchar**)&args->inset_length);
    gwy_container_gis_boolean_by_name(container, draw_mask_key,
                                      &args->draw_mask);
    gwy_container_gis_boolean_by_name(container, draw_selection_key,
                                      &args->draw_selection);
    gwy_container_gis_boolean_by_name(container, text_antialias_key,
                                      &args->text_antialias);
    gwy_container_gis_string_by_name(container, font_key,
                                     (const guchar**)&args->font);
    gwy_container_gis_boolean_by_name(container, scale_font_key,
                                      &args->scale_font);
    gwy_container_gis_double_by_name(container, font_size_key,
                                     &args->font_size);
    gwy_container_gis_double_by_name(container, fmscale_gap_key,
                                     &args->fmscale_gap);
    gwy_container_gis_double_by_name(container, inset_gap_key,
                                     &args->inset_gap);
    gwy_container_gis_int32_by_name(container, greyscale_key, &args->greyscale);
    gwy_container_gis_boolean_by_name(container, inset_draw_ticks_key,
                                      &args->inset_draw_ticks);
    gwy_container_gis_boolean_by_name(container, inset_draw_label_key,
                                      &args->inset_draw_label);

    args->font = g_strdup(args->font);
    args->inset_length = g_strdup(args->inset_length);

    img_export_sanitize_args(args);
}

static void
img_export_save_args(GwyContainer *container,
                     ImgExportArgs *args)
{
    gwy_container_set_double_by_name(container, zoom_key, args->zoom);
    gwy_container_set_enum_by_name(container, xytype_key, args->xytype);
    gwy_container_set_enum_by_name(container, ztype_key, args->ztype);
    gwy_rgba_store_to_container(&args->inset_color, container, inset_color_key);
    gwy_container_set_enum_by_name(container, inset_pos_key, args->inset_pos);
    gwy_container_set_string_by_name(container, inset_length_key,
                                     g_strdup(args->inset_length));
    gwy_container_set_boolean_by_name(container, draw_mask_key,
                                      args->draw_mask);
    gwy_container_set_boolean_by_name(container, draw_selection_key,
                                      args->draw_selection);
    gwy_container_set_boolean_by_name(container, text_antialias_key,
                                      args->text_antialias);
    gwy_container_set_string_by_name(container, font_key,
                                     g_strdup(args->font));
    gwy_container_set_boolean_by_name(container, scale_font_key,
                                      args->scale_font);
    gwy_container_set_double_by_name(container, font_size_key,
                                     args->font_size);
    gwy_container_set_double_by_name(container, fmscale_gap_key,
                                     args->fmscale_gap);
    gwy_container_set_double_by_name(container, inset_gap_key,
                                     args->inset_gap);
    gwy_container_set_int32_by_name(container, greyscale_key, args->greyscale);
    gwy_container_set_boolean_by_name(container, inset_draw_ticks_key,
                                      args->inset_draw_ticks);
    gwy_container_set_boolean_by_name(container, inset_draw_label_key,
                                      args->inset_draw_label);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
