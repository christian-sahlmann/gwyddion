/*
 *  @(#) $Id$
 *  Loader for JPK Image Scans.
 *  Copyright (C) 2005  JPK Instruments AG.
 *  Written by Sven Neumann <neumann@jpk.com>.
 *
 *  Rewritten to use GwyTIFF and spectra added by Yeti <yeti@gwyddion.net>.
 *  Copyright (C) 2009-2015 David Necas (Yeti).
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
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-jpk-image-scan">
 *   <comment>JPK image scan</comment>
 *   <magic priority="10">
 *     <match type="string" offset="0" value="MM\x00\x2a"/>
 *   </magic>
 *   <glob pattern="*.jpk"/>
 *   <glob pattern="*.JPK"/>
 *   <glob pattern="*.jpk-qi-image"/>
 *   <glob pattern="*.JPK-QI-IMAGE"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * JPK Instruments
 * .jpk, .jpk-qi-image
 * Read
 **/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "jpk.h"
#include "gwytiff.h"

#if defined(HAVE_MINIZIP) || defined(HAVE_LIBZIP)
#define HAVE_GWYZIP 1
#include "gwyzip.h"
#else
#undef HAVE_GWYZIP
#endif

#define MAGIC "PK\x03\x04"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define MAGIC_FORCE1 "segments/0"
#define MAGIC_FORCE1_SIZE (sizeof(MAGIC_FORCE1)-1)
#define MAGIC_FORCE2 "header.properties"
#define MAGIC_FORCE2_SIZE (sizeof(MAGIC_FORCE2)-1)

typedef struct {
    GHashTable *header_properties;
    /* The backend storage for all the hash tables. */
    GSList *buffers;
} JPKForceFile;

static gboolean      module_register     (void);
static gint          jpkscan_detect      (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* jpkscan_load        (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static void          jpk_load_channel    (const GwyTIFF *tiff,
                                          const GwyTIFFImageReader *reader,
                                          const gchar *filename,
                                          GwyContainer *container,
                                          GwyContainer *meta,
                                          guint idx,
                                          gdouble ulen,
                                          gdouble vlen);
static void          jpk_load_meta       (GwyTIFF *tiff,
                                          GwyContainer *container);
static void          jpk_load_meta_string(GwyTIFF *tiff,
                                          GwyContainer *container,
                                          guint tag,
                                          const gchar *name);
static void          jpk_load_meta_double(GwyTIFF *tiff,
                                          GwyContainer *container,
                                          guint tag,
                                          const gchar *unit,
                                          const gchar *name);
static void          meta_store_double   (GwyContainer *container,
                                          const gchar *name,
                                          gdouble value,
                                          const gchar *unit);

#ifdef HAVE_GWYZIP
static gint          jpkforce_detect            (const GwyFileDetectInfo *fileinfo,
                                                 gboolean only_name);
static GwyContainer* jpkforce_load              (const gchar *filename,
                                                 GwyRunType mode,
                                                 GError **error);
static GHashTable*   jpk_parse_header_properties(GwyZipFile zipfile,
                                                 JPKForceFile *jpkfile,
                                                 const gchar *path,
                                                 GError **error);
#endif

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports JPK image scans."),
    "Sven Neumann <neumann@jpk.com>, Yeti <yeti@gwyddion.net>",
    "0.10",
    "JPK Instruments AG, David Nečas (Yeti)",
    "2005-2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("jpkscan",
                           N_("JPK image scans (.jpk, .jpk-qi-image)"),
                           (GwyFileDetectFunc)&jpkscan_detect,
                           (GwyFileLoadFunc)&jpkscan_load,
                           NULL,
                           NULL);
#ifdef HAVE_GWYZIP
    gwy_file_func_register("jpkforce",
                           N_("JPK force curves "
                              "(.jpk-force, .jpk-force-map, .jpk-qi-data)"),
                           (GwyFileDetectFunc)&jpkforce_detect,
                           (GwyFileLoadFunc)&jpkforce_load,
                           NULL,
                           NULL);
#endif

    return TRUE;
}

static gint
jpkscan_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    gdouble ulen, vlen;
    gchar *name = NULL;
    gint score = 0;
    guint byteorder = G_BIG_ENDIAN;
    GwyTIFFVersion version = GWY_TIFF_CLASSIC;

    if (only_name)
        return score;

    if (!gwy_tiff_detect(fileinfo->head, fileinfo->buffer_len,
                         &version, &byteorder))
        return 0;

    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))
        && gwy_tiff_get_float0(tiff, JPK_TIFFTAG_Grid_uLength, &ulen)
        && gwy_tiff_get_float0(tiff, JPK_TIFFTAG_Grid_vLength, &vlen)
        && ulen > 0.0
        && vlen > 0.0
        && (gwy_tiff_get_string0(tiff, JPK_TIFFTAG_ChannelFancyName, &name)
            || gwy_tiff_get_string0(tiff, JPK_TIFFTAG_Channel, &name)))
        score = 100;

    g_free(name);
    if (tiff)
        gwy_tiff_free(tiff);

    return score;
}

static GwyContainer*
jpkscan_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *container = NULL;
    GwyContainer *meta = NULL;
    GwyTIFF *tiff;
    GwyTIFFImageReader *reader = NULL;
    GError *err = NULL;
    guint idx, photo;
    gdouble ulen, vlen;

    tiff = gwy_tiff_load(filename, error);
    if (!tiff)
        return NULL;

    /*  sanity check, grid dimensions must be present!  */
    if (!(gwy_tiff_get_float0(tiff, JPK_TIFFTAG_Grid_uLength, &ulen)
          && gwy_tiff_get_float0(tiff, JPK_TIFFTAG_Grid_vLength, &vlen))) {
        gwy_tiff_free(tiff);
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File does not contain grid dimensions."));
        return NULL;
    }

    /* Use negated positive conditions to catch NaNs */
    if (!((ulen = fabs(ulen)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        ulen = 1.0;
    }
    if (!((vlen = fabs(vlen)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        vlen = 1.0;
    }

    container = gwy_container_new();
    meta = gwy_container_new();
    /* FIXME: I'm unable to meaningfully sort out the metadata to channels,
     * so each one receives an identical copy of the global metadata now. */
    jpk_load_meta(tiff, meta);

    gwy_debug("ulen: %g vlen: %g", ulen, vlen);

    for (idx = 0; idx < gwy_tiff_get_n_dirs(tiff); idx++) {
        reader = gwy_tiff_image_reader_free(reader);
        /* Request a reader, this ensures dimensions and stuff are defined. */
        reader = gwy_tiff_get_image_reader(tiff, idx, 1, &err);
        if (!reader) {
            /* 0th directory is usually thumbnail, do not complain about it. */
            if (idx > 0)
                g_warning("Ignoring directory %u: %s.", idx, err->message);
            g_clear_error(&err);
            continue;
        }

        if (!gwy_tiff_get_uint(tiff, idx, GWY_TIFFTAG_PHOTOMETRIC, &photo)) {
            g_warning("Could not get photometric tag, ignoring directory %u",
                      idx);
            continue;
        }

        /*  we are only interested in 16bit grayscale  */
        switch (photo) {
            case GWY_TIFF_PHOTOMETRIC_MIN_IS_BLACK:
            case GWY_TIFF_PHOTOMETRIC_MIN_IS_WHITE:
            if (reader->bits_per_sample == 16 || reader->bits_per_sample == 32)
                break;

            default:
            continue;
        }

        jpk_load_channel(tiff, reader, filename,
                         container, meta, idx, ulen, vlen);
    }

    gwy_tiff_free(tiff);
    g_object_unref(meta);

    return container;
}

/* FIXME: this function could use some sort of failure indication, if the
 * file is damaged and no data field can be loaded, suspicionless caller can
 * return empty Container */
static void
jpk_load_channel(const GwyTIFF *tiff,
                 const GwyTIFFImageReader *reader,
                 const gchar *filename,
                 GwyContainer *container,
                 GwyContainer *meta,
                 guint idx, gdouble ulen, gdouble vlen)
{
    GwyDataField *dfield;
    GwySIUnit *siunit;
    GString *key;
    gdouble *data;
    gchar *channel;
    gchar *name = NULL;
    gchar *slot = NULL;
    gchar *unit = NULL;
    gboolean retrace = FALSE;
    gboolean reflect = FALSE;
    gdouble mult = 0.0;
    gdouble offset = 0.0;
    gint num_slots = 0;
    gint i, j;

    gwy_tiff_get_string(tiff, idx, JPK_TIFFTAG_ChannelFancyName, &name);
    if (!name)
        gwy_tiff_get_string(tiff, idx, JPK_TIFFTAG_Channel, &name);
    g_return_if_fail(name != NULL);

    gwy_tiff_get_bool(tiff, idx, JPK_TIFFTAG_Channel_retrace, &retrace);
    channel = g_strdup_printf("%s%s", name, retrace ? " (retrace)" : "");
    g_free(name);
    gwy_debug("channel: %s", channel);

    gwy_tiff_get_sint(tiff, idx, JPK_TIFFTAG_NrOfSlots, &num_slots);
    g_return_if_fail(num_slots > 0);
    gwy_debug("num_slots: %d", num_slots);

    /* Locate the default slot */

    gwy_tiff_get_string(tiff, idx, JPK_TIFFTAG_DefaultSlot, &slot);
    g_return_if_fail(slot != NULL);
    gwy_debug("num_slots: %d, default slot: %s", num_slots, slot);

    for (i = 0; i < num_slots; i++) {
        gchar *string = NULL;

        if (gwy_tiff_get_string(tiff, idx, JPK_TIFFTAG_Slot_Name(i), &string)
            && string
            && gwy_strequal(string, slot)) {
            g_free(string);
            gwy_tiff_get_string(tiff, idx, JPK_TIFFTAG_Scaling_Type(i),
                                &string);
            g_return_if_fail(gwy_strequal(string, "LinearScaling"));

            gwy_tiff_get_float(tiff, idx, JPK_TIFFTAG_Scaling_Multiply(i),
                               &mult);
            gwy_tiff_get_float(tiff, idx, JPK_TIFFTAG_Scaling_Offset(i),
                               &offset);

            gwy_debug("multipler: %g offset: %g", mult, offset);

            g_free(string);
            gwy_tiff_get_string(tiff, idx, JPK_TIFFTAG_Encoder_Unit(i), &unit);

            break;
        }
        g_free(string);
    }
    g_free(slot);

    /* Create a new data field */
    dfield = gwy_data_field_new(reader->width, reader->height, ulen, vlen,
                                FALSE);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    if (unit) {
        siunit = gwy_si_unit_new(unit);
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);
        g_free(unit);
    }

    /* Read the scan data */
    gwy_tiff_get_bool(tiff, idx, JPK_TIFFTAG_Grid_Reflect, &reflect);
    data = gwy_data_field_get_data(dfield);

    for (j = 0; j < reader->height; j++)
        gwy_tiff_read_image_row(tiff, reader, 0,
                                reflect ? j : reader->height-1 - j,
                                mult, offset,
                                data + j*reader->width);

    /* Add the GwyDataField to the container */

    key = g_string_new(NULL);
    g_string_printf(key, "/%d/data", idx);
    gwy_container_set_object_by_name(container, key->str, dfield);
    g_object_unref(dfield);

    g_string_append(key, "/title");
    gwy_container_set_string_by_name(container, key->str, channel);

    if (gwy_container_get_n_items(meta)) {
        GwyContainer *tmp;

        tmp = gwy_container_duplicate(meta);
        g_string_printf(key, "/%d/meta", idx);
        gwy_container_set_object_by_name(container, key->str, tmp);
        g_object_unref(tmp);
    }
    gwy_file_channel_import_log_add(container, idx, NULL, filename);

    g_string_free(key, TRUE);
}

static void
jpk_load_meta(GwyTIFF *tiff, GwyContainer *container)
{
    gchar *string;
    gdouble frequency;
    gdouble value;

    jpk_load_meta_string(tiff, container, JPK_TIFFTAG_Name, "Name");
    jpk_load_meta_string(tiff, container, JPK_TIFFTAG_Comment, "Comment");
    jpk_load_meta_string(tiff, container, JPK_TIFFTAG_Sample, "Probe");
    jpk_load_meta_string(tiff, container, JPK_TIFFTAG_AccountName, "Account");

    jpk_load_meta_string(tiff, container, JPK_TIFFTAG_StartDate, "Time Start");
    jpk_load_meta_string(tiff, container, JPK_TIFFTAG_EndDate, "Time End");

    jpk_load_meta_double(tiff, container,
                         JPK_TIFFTAG_Grid_x0, "m", "Origin X");
    jpk_load_meta_double(tiff, container,
                         JPK_TIFFTAG_Grid_y0, "m", "Origin Y");
    jpk_load_meta_double(tiff, container,
                         JPK_TIFFTAG_Grid_uLength, "m", "Size X");
    jpk_load_meta_double(tiff, container,
                         JPK_TIFFTAG_Grid_vLength, "m", "Size Y");

    jpk_load_meta_double(tiff, container,
                         JPK_TIFFTAG_Scanrate_Dutycycle, NULL, "Duty Cycle");

    jpk_load_meta_string(tiff, container,
                         JPK_TIFFTAG_Feedback_Mode, "Feedback Mode");
    jpk_load_meta_double(tiff, container,
                         JPK_TIFFTAG_Feedback_iGain, "Hz", "Feedback IGain");
    jpk_load_meta_double(tiff, container,
                         JPK_TIFFTAG_Feedback_pGain, NULL, "Feedback PGain");
    jpk_load_meta_double(tiff, container,
                         JPK_TIFFTAG_Feedback_Setpoint, "V",
                         "Feedback Setpoint");

    /*  some values need special treatment  */

    if (gwy_tiff_get_float0(tiff,
                            JPK_TIFFTAG_Scanrate_Frequency, &frequency)
        && gwy_tiff_get_float0(tiff, JPK_TIFFTAG_Scanrate_Dutycycle, &value)
        && value > 0.0) {
        meta_store_double(container, "Scan Rate", frequency/value, "Hz");
    }

    if (gwy_tiff_get_float0(tiff, JPK_TIFFTAG_Feedback_iGain, &value))
        meta_store_double(container, "Feedback IGain", fabs(value), "Hz");

    if (gwy_tiff_get_float0(tiff, JPK_TIFFTAG_Feedback_pGain, &value))
        meta_store_double(container, "Feedback PGain", fabs(value), NULL);

    if (gwy_tiff_get_string0(tiff, JPK_TIFFTAG_Feedback_Mode, &string)) {
        if (gwy_strequal(string, "contact")) {
            jpk_load_meta_double(tiff, container,
                                 JPK_TIFFTAG_Feedback_Baseline, "V",
                                 "Feedback Baseline");
        }
        else if (gwy_strequal(string, "intermittent")) {
            jpk_load_meta_double(tiff, container,
                                 JPK_TIFFTAG_Feedback_Amplitude, "V",
                                 "Feedback Amplitude");
            jpk_load_meta_double(tiff, container,
                                 JPK_TIFFTAG_Feedback_Frequency, "Hz",
                                 "Feedback Frequency");
            jpk_load_meta_double(tiff, container,
                                 JPK_TIFFTAG_Feedback_Phaseshift, "deg",
                                 "Feedback Phaseshift");
        }
        g_free(string);
    }
}

static void
jpk_load_meta_string(GwyTIFF *tiff,
                      GwyContainer *container, guint tag, const gchar *name)
{
    gchar *string;

    if (gwy_tiff_get_string0(tiff, tag, &string))
        gwy_container_set_string_by_name(container, name, string);
}

static void
jpk_load_meta_double(GwyTIFF *tiff,
                      GwyContainer *container,
                      guint tag, const gchar *unit, const gchar *name)
{
    gdouble value;

    if (gwy_tiff_get_float0(tiff, tag, &value))
        meta_store_double(container, name, value, unit);
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

#ifdef HAVE_GWYZIP
static gint
jpkforce_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    GwyZipFile zipfile;
    guchar *content;
    gint score = 0;

    if (only_name)
        return 0;

    /* Generic ZIP file. */
    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    /* It contains segments/0 (possibly under index) and header.properties
     * (possibly also inside something). */
    if (!gwy_memmem(fileinfo->head, fileinfo->buffer_len,
                    MAGIC_FORCE1, MAGIC_FORCE1_SIZE)
        || !gwy_memmem(fileinfo->head, fileinfo->buffer_len,
                       MAGIC_FORCE2, MAGIC_FORCE2_SIZE))
        return 0;

    /* Look inside if there is header.properties in the main directory. */
    if ((zipfile = gwyzip_open(fileinfo->name))) {
        if (gwyzip_locate_file(zipfile, "header.properties", 1, NULL)
            && (content = gwyzip_get_file_content(zipfile, NULL, NULL))) {
            if (g_strstr_len(content, 4096, "jpk-data-file"))
                score = 100;
            g_free(content);
        }
        gwyzip_close(zipfile);
    }

    return score;
}

static void
jpk_force_file_free(JPKForceFile *jpkfile)
{
    GSList *l;

    if (jpkfile->header_properties)
        g_hash_table_destroy(jpkfile->header_properties);

    while ((l = jpkfile->buffers)) {
        jpkfile->buffers = g_slist_next(l);
        g_slist_free_1(l);
    }
}

static GwyContainer*
jpkforce_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GwyContainer *container = NULL;
    JPKForceFile jpkfile;
    GwyZipFile zipfile;

    zipfile = gwyzip_open(filename);
    if (!zipfile) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Minizip cannot open the file as a ZIP file."));
        return NULL;
    }

    gwy_clear(&jpkfile, 1);
    if (!(jpkfile.header_properties
          = jpk_parse_header_properties(zipfile, &jpkfile, "", error)))
        goto fail;

    err_NO_DATA(error);

fail:
    gwyzip_close(zipfile);
    jpk_force_file_free(&jpkfile);

    return container;
}

static GHashTable*
jpk_parse_header_properties(GwyZipFile zipfile, JPKForceFile *jpkfile,
                            const gchar *path, GError **error)
{
    GwyTextHeaderParser parser;
    GHashTable *hash;
    gchar *filename;
    guchar *contents;
    gsize size;

    if (strlen(path))
        filename = g_strconcat(path, "/", "header.properties", NULL);
    else
        filename = g_strdup_printf("header.properties");

    gwy_debug("trying to load %s", filename);
    if (!gwyzip_locate_file(zipfile, filename, TRUE, error)) {
        g_free(filename);
        return NULL;
    }
    g_free(filename);
    if (!(contents = gwyzip_get_file_content(zipfile, &size, error)))
        return NULL;

    jpkfile->buffers = g_slist_prepend(jpkfile->buffers, contents);
    hash = g_hash_table_new(g_str_hash, g_str_equal);

    gwy_clear(&parser, 1);
    parser.comment_prefix = "#";
    parser.key_value_separator = "=";
    hash = gwy_text_header_parse((gchar*)contents, &parser, NULL, error);
    if (hash) {
        gwy_debug("header.properties has %u entries", g_hash_table_size(hash));
    }

    return hash;
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
