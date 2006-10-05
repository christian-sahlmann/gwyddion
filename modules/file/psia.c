/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  Loosely based on jpkscan.c:
 *  Loader for JPK Image Scans.
 *  Copyright (C) 2005  JPK Instruments AG.
 *  Written by Sven Neumann <neumann@jpk.com>.
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

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <tiffio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>

#include "err.h"

#define MAGIC      "II\x00\x2a"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

/* Custom TIFF tags */
#define PSIA_TIFFTAG_MagicNumber       50432
#define PSIA_TIFFTAG_Version           50433
#define PSIA_TIFFTAG_Data              50434
#define PSIA_TIFFTAG_Header            50435
#define PSIA_TIFFTAG_Comments          50436
#define PSIA_TIFFTAG_LineProfileHeader 50437
/* PSIA claims tag numbers 50432 to 50441, but nothing is known about the
 * remaining tags. */
#define PSIA_MAGIC_NUMBER              0x0E031301

static gboolean      module_register     (void);
static gint          psia_detect         (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* psia_load           (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static GwyContainer* psia_load_tiff      (const gchar *filename,
                                          GError **error);
static gboolean      tiff_check_version  (gint macro,
                                          gint micro,
                                          GError **error);
static gboolean      tiff_get_custom_uint(TIFF *tif,
                                          ttag_t tag,
                                          guint *value);
static void          tiff_ignore         (const gchar *module,
                                          const gchar *format,
                                          va_list args);
static void          tiff_error          (const gchar *module,
                                          const gchar *format,
                                          va_list args);
static void          meta_store_double   (GwyContainer *container,
                                          const gchar *name,
                                          gdouble value,
                                          const gchar *unit);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports PSIA data files."),
    "Sven Neumann <neumann@jpk.com>, Yeti <yeti@gwyddion.net>",
    "0.1",
    "JPK Instruments AG, David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    GError *err = NULL;

    /* Handling of custom tags was introduced with LibTIFF version 3.6.0 */
    /* FIXME: Can we do better?  Module registration should be able to return
     * GErrors too... */
    if (!tiff_check_version(3, 6, &err)) {
        g_warning("%s", err->message);
        g_clear_error(&err);
        return FALSE;
    }

    gwy_file_func_register("psia",
                           N_("PSIA data files (.tiff)"),
                           (GwyFileDetectFunc)&psia_detect,
                           (GwyFileLoadFunc)&psia_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
psia_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    TIFFErrorHandler old_error, old_warning;
    TIFF *tiff;
    gint score = 0;
    guint magic;

    if (only_name)
        return score;

    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    old_warning = TIFFSetWarningHandler(tiff_ignore);
    old_error = TIFFSetErrorHandler(tiff_ignore);

    if ((tiff = TIFFOpen(fileinfo->name, "r"))
        && tiff_get_custom_uint(tiff, PSIA_TIFFTAG_MagicNumber, &magic)
        && magic == PSIA_MAGIC_NUMBER)
        score = 100;

    if (tiff)
        TIFFClose(tiff);

    TIFFSetErrorHandler(old_error);
    TIFFSetErrorHandler(old_warning);

    return score;
}

static GwyContainer*
psia_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    TIFFErrorHandler old_error, old_warning;
    GwyContainer *container;

    gwy_debug("Loading <%s>", filename);

    old_warning = TIFFSetWarningHandler(tiff_ignore);
    old_error = TIFFSetErrorHandler(tiff_error);

    container = psia_load_tiff(filename, error);

    TIFFSetErrorHandler(old_error);
    TIFFSetErrorHandler(old_warning);

    return container;
}

static GwyContainer*
psia_load_tiff(const gchar *filename, GError **error)
{
    GwyContainer *container = NULL;
    GwyContainer *meta = NULL;
    TIFF *tiff;

    tiff = TIFFOpen(filename, "r");
    if (!tiff) {
        /* This can be I/O too, but it's hard to tell the difference. */
        err_FILE_TYPE(error, _("PSIA"));
        return NULL;
    }

#if 0
    /*  sanity check, grid dimensions must be present!  */
    if (!(tiff_get_custom_double(tiff, JPK_TIFFTAG_Grid_uLength, &ulen)
          && tiff_get_custom_double(tiff, JPK_TIFFTAG_Grid_vLength, &vlen))) {
        TIFFClose(tiff);
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File does not contain grid dimensions."));
        return NULL;
    }

    container = gwy_container_new();
    meta = gwy_container_new();
    /* FIXME: I'm unable to meaningfully sort out the metadata to channels,
     * so each one receives an identical copy of the global metadata now. */
    tiff_load_meta(tiff, meta);

    gwy_debug("ulen: %g vlen: %g", ulen, vlen);

    do {
        if (!TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &ilen)) {
            g_warning("Could not get image width, skipping");
            continue;
        }

        if (!TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &jlen)) {
            g_warning("Could not get image length, skipping");
            continue;
        }

        TIFFGetFieldDefaulted(tiff, TIFFTAG_BITSPERSAMPLE, &bps);

        if (!TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photo)) {
            g_warning("Could not get photometric tag, skipping");
            continue;
        }

        /*  we are only interested in 16bit grayscale  */
        switch (photo) {
            case PHOTOMETRIC_MINISBLACK:
            case PHOTOMETRIC_MINISWHITE:
                if (bps == 16)
                    break;
            default:
                continue;
        }

        if (TIFFGetField(tiff, TIFFTAG_PLANARCONFIG, &planar)
            && planar != PLANARCONFIG_CONTIG) {
            g_warning("Can only handle planar data, skipping");
            continue;
        }

        tiff_load_channel(tiff, container, meta, idx++, ilen, jlen, ulen, vlen);
    }
    while (TIFFReadDirectory(tiff));
#endif

    TIFFClose(tiff);
/*    g_object_unref(meta); */

    return container;
}

static gboolean
tiff_check_version(gint required_macro, gint required_micro, GError **error)
{
    gchar *version = g_strdup(TIFFGetVersion());
    gchar *ptr;
    gboolean result = TRUE;
    gint major;
    gint minor;
    gint micro;

    ptr = strchr(version, '\n');
    if (ptr)
        *ptr = '\0';

    ptr = version;
    while (*ptr && !g_ascii_isdigit(*ptr))
        ptr++;

    if (sscanf(ptr, "%d.%d.%d", &major, &minor, &micro) != 3) {
        g_warning("Cannot parse TIFF version, proceed with fingers crossed");
    }
    else if ((major < required_macro)
             || (major == required_macro && minor < required_micro)) {
        result = FALSE;

        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("LibTIFF too old!\n\n"
                      "You are using %s. Please update to "
                      "libtiff version %d.%d or newer."), version,
                    required_macro, required_micro);
    }

    g_free(version);

    return result;
}

/*  reads what the TIFF spec calls LONG  */
static gboolean
tiff_get_custom_uint(TIFF *tif, ttag_t tag, guint *value)
{
    guint32 *l;
    gint count;

    if (TIFFGetField(tif, tag, &count, &l)) {
        *value = *l;
        return TRUE;
    }
    else {
        *value = 0;
        return FALSE;
    }
}

static void
tiff_ignore(const gchar *module G_GNUC_UNUSED,
            const gchar *format G_GNUC_UNUSED,
            va_list args G_GNUC_UNUSED)
{
    /*  ignore  */
}

/* TODO: pass the error message upstream, somehow */
static void
tiff_error(const gchar *module G_GNUC_UNUSED, const gchar *format, va_list args)
{
    g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, format, args);
}

static void
meta_store_double(GwyContainer *container,
                  const gchar *name, gdouble value, const gchar *unit)
{
    GwySIUnit *siunit = gwy_si_unit_new(unit);
    GwySIValueFormat *format = gwy_si_unit_get_format(siunit,
                                                      GWY_SI_UNIT_FORMAT_MARKUP,
                                                      value, NULL);

    gwy_container_set_string_by_name(container, name,
                                     g_strdup_printf("%5.3f %s",
                                                     value/format->magnitude,
                                                     format->units));
    g_object_unref(siunit);
    gwy_si_unit_value_format_free(format);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
