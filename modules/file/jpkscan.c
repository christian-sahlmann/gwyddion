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
#define DEBUG 1
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-jpk-image-scan">
 *   <comment>JPK image scan</comment>
 *   <magic priority="10">
 *     <match type="string" offset="0" value="MM\x00\x2a"/>
 *   </magic>
 *   <glob pattern="*.jpk"/>
 *   <glob pattern="*.JPK"/>
 *   <glob pattern="*.jpk-force"/>
 *   <glob pattern="*.JPK-FORCE"/>
 *   <glob pattern="*.jpk-force-map"/>
 *   <glob pattern="*.JPK-FORCE-MAP"/>
 *   <glob pattern="*.jpk-qi-image"/>
 *   <glob pattern="*.JPK-QI-IMAGE"/>
 *   <glob pattern="*.jpk-qi-data"/>
 *   <glob pattern="*.JPK-QI-DATA"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * JPK Instruments
 * .jpk .jpk-qi-image
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

typedef enum {
    JPK_FORCE_UNKNOWN = 0,
    /* This is just some graphs (curves). */
    JPK_FORCE_CURVES,
    /* This includes coordinates that may or may not be on a regular grid.
     * If they are not, we must load it as SPS.  Otherwise we can load it
     * as volume data. */
    JPK_FORCE_MAP,
    /* This is always on a fine grid and should be loaded as volume data. */
    JPK_FORCE_QI,
} JPKForceFileType;

typedef struct {
    GHashTable *header_properties;
    /* Concatenated data of all channels. */
    guint ndata;
    gdouble *data;
    const gchar **units;   /* For all channels */
    const gchar *segment_style;   /* This is extend, retract, pause. */
    const gchar *segment_type;    /* This is a more detailed type. */
    gchar *segment_name;
} JPKForceData;

typedef struct {
    GRegex *segment_regex;
    GRegex *index_segment_regex;
    GString *str;     /* General scratch buffer. */
    GString *sstr;    /* Inner scratch buffer for lookup_property(). */
    GString *qstr;    /* Inner scratch buffer for find_scaling_parameters(). */

    GHashTable *header_properties;
    GHashTable *shared_header_properties;
    JPKForceFileType type;
    guint nids;
    guint *ids;
    guint nsegs;
    guint npoints;
    guint nchannels;
    gchar **channel_names;
    JPKForceData *data;

    /* For maps/QI */
    GHashTable **point_header_properties;
    guint xres;
    guint yres;

    /* The backend storage for all the hash tables.  We must keep those we
     * created hashes from because the strings point directly to the buffers. */
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
static gint          jpkforce_detect              (const GwyFileDetectInfo *fileinfo,
                                                   gboolean only_name);
static GwyContainer* jpkforce_load                (const gchar *filename,
                                                   GwyRunType mode,
                                                   GError **error);
static gboolean      read_curve_data              (GwyZipFile zipfile,
                                                   JPKForceFile *jpkfile,
                                                   GError **error);
static gboolean      read_raw_data                (GwyZipFile zipfile,
                                                   JPKForceFile *jpkfile,
                                                   JPKForceData *data,
                                                   const gchar *datatype,
                                                   guint i,
                                                   GError **error);
static guint         find_segment_npoints         (JPKForceFile *jpkfile,
                                                   GHashTable *header_properties,
                                                   GError **error);
static gchar*        find_sgement_name            (GHashTable *header_properties);
static gboolean      apply_default_channel_scaling(JPKForceFile *jpkfile,
                                                   JPKForceData *data,
                                                   guint i);
static gboolean      find_scaling_parameters      (JPKForceFile *jpkfile,
                                                   GHashTable *header_properties,
                                                   const gchar *subkey,
                                                   guint i,
                                                   gdouble *multiplier,
                                                   gdouble *offset,
                                                   const gchar **unit);
static const gchar*  lookup_channel_property      (JPKForceFile *jpkfile,
                                                   GHashTable *header_properties,
                                                   const gchar *subkey,
                                                   guint i,
                                                   gboolean fail_if_not_found,
                                                   GError **error);
static const gchar*  lookup_property              (JPKForceFile *jpkfile,
                                                   GHashTable *header_properties,
                                                   const gchar *key,
                                                   gboolean fail_if_not_found,
                                                   GError **error);
static gboolean      enumerate_channels           (JPKForceFile *jpkfile,
                                                   GHashTable *header_properties,
                                                   gboolean needslist,
                                                   GError **error);
static gboolean      analyse_segment_ids          (JPKForceFile *jpkfile,
                                                   GError **error);
static gboolean      analyse_map_segment_ids      (JPKForceFile *jpkfile,
                                                   GError **error);
static gboolean      enumerate_segments           (GwyZipFile zipfile,
                                                   JPKForceFile *jpkfile);
static gboolean      enumerate_map_segments       (GwyZipFile zipfile,
                                                   JPKForceFile *jpkfile);
static void          jpk_force_file_free          (JPKForceFile *jpkfile);
static GHashTable*   parse_header_properties      (GwyZipFile zipfile,
                                                   JPKForceFile *jpkfile,
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

    /* The main properties must exist. */
    if (!gwyzip_locate_file(zipfile, "header.properties", TRUE, error)
        || !(jpkfile.header_properties = parse_header_properties(zipfile,
                                                                 &jpkfile,
                                                                 error)))
        goto fail;

    /* Optional. */
    if (gwyzip_locate_file(zipfile, "shared-data/header.properties", TRUE,
                           NULL)) {
        jpkfile.shared_header_properties = parse_header_properties(zipfile,
                                                                   &jpkfile,
                                                                   NULL);
    }

    jpkfile.str = g_string_new(NULL);
    jpkfile.sstr = g_string_new(NULL);
    jpkfile.qstr = g_string_new(NULL);
    jpkfile.segment_regex
        = g_regex_new("^segments/([0-9]+)/(.*)$", G_REGEX_OPTIMIZE, 0, NULL);
    jpkfile.index_segment_regex
        = g_regex_new("^index/([0-9]+)/segments/([0-9]+)/(.*)$",
                      G_REGEX_OPTIMIZE, 0, NULL);

    if (enumerate_segments(zipfile, &jpkfile)) {
        jpkfile.type = JPK_FORCE_CURVES;
        if (!analyse_segment_ids(&jpkfile, error))
            goto fail;
    }
    else if (enumerate_map_segments(zipfile, &jpkfile)) {
        jpkfile.type = JPK_FORCE_MAP;
        if (gwyzip_locate_file(zipfile, "data-image.jpk-qi-image", TRUE, NULL))
            jpkfile.type = JPK_FORCE_QI;
        if (!analyse_map_segment_ids(&jpkfile, error))
            goto fail;
    }
    else {
        err_NO_DATA(error);
        goto fail;
    }

    if (!enumerate_channels(&jpkfile, jpkfile.shared_header_properties, FALSE,
                            error))
        goto fail;

    jpkfile.data = g_new0(JPKForceData, jpkfile.nids);
    if (jpkfile.type == JPK_FORCE_CURVES) {
        if (!read_curve_data(zipfile, &jpkfile, error))
            goto fail;
    }
    else {
    }

    err_NO_DATA(error);

fail:
    gwyzip_close(zipfile);
    jpk_force_file_free(&jpkfile);

    return container;
}

static gboolean
err_IRREGULAR_NUMBERING(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Non-uniform point and/or segment numbering "
                  "is not supported."));
    return FALSE;
}

static gboolean
err_NONUNIFORM_CHANNELS(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Non-uniform channel lists are not supported."));
    return FALSE;
}

static gboolean
err_DATA_FILE_NAME(GError **error, const gchar *expected, const gchar *found)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Data file %s was found instead of expected %s."),
                found, expected);
    return FALSE;
}

static int
compare_uint(gconstpointer a, gconstpointer b)
{
    const guint ia = *(const guint*)a;
    const guint ib = *(const guint*)b;

    if (ia < ib)
        return -1;
    if (ia > ib)
        return 1;
    return 0;
}

static int
compare_uint2(gconstpointer a, gconstpointer b)
{
    const guint ia = *(const guint*)a;
    const guint ib = *(const guint*)b;
    const guint iaa = *((const guint*)a + 1);
    const guint ibb = *((const guint*)b + 1);

    if (ia < ib)
        return -1;
    if (ia > ib)
        return 1;
    if (iaa < ibb)
        return -1;
    if (iaa > ibb)
        return 1;
    return 0;
}

static gchar*
match_segment_filename(const gchar *filename, GRegex *regex, gint *id)
{
    GMatchInfo *info;
    gchar *s;

    if (!g_regex_match(regex, filename, 0, NULL))
        return NULL;

    g_regex_match(regex, filename, 0, &info);

    s = g_match_info_fetch(info, 1);
    *id = atoi(s);
    g_free(s);

    s = g_match_info_fetch(info, 2);
    g_match_info_free(info);
    return s;
}

static gchar*
match_map_segment_filename(const gchar *filename, GRegex *regex,
                           gint *id1, gint *id2)
{
    GMatchInfo *info;
    gchar *s;

    if (!g_regex_match(regex, filename, 0, NULL))
        return NULL;

    g_regex_match(regex, filename, 0, &info);

    s = g_match_info_fetch(info, 1);
    *id1 = atoi(s);
    g_free(s);

    s = g_match_info_fetch(info, 2);
    *id2 = atoi(s);
    g_free(s);

    s = g_match_info_fetch(info, 3);
    g_match_info_free(info);
    return s;
}

/* Expect the files in order.  We could read everything into memory first but
 * that would be insane for QI. */
static gboolean
read_curve_data(GwyZipFile zipfile, JPKForceFile *jpkfile, GError **error)
{
    GString *str = jpkfile->str;

    if (!gwyzip_first_file(zipfile, NULL)) {
        /* Internal error ??? */
        err_NO_DATA(error);
        return FALSE;
    }

    do {
        gchar *filename = NULL, *suffix = NULL;
        GHashTable *hash;
        guint id, i, ndata;
        gboolean ok;

        if (!gwyzip_get_current_filename(zipfile, &filename, NULL))
            continue;

        /* Find the header. */
        suffix = match_segment_filename(filename, jpkfile->segment_regex, &id);
        g_free(filename);

        if (!suffix)
            continue;
        ok = gwy_strequal(suffix, "segment-header.properties");
        g_free(suffix);
        if (!ok)
            continue;

        g_assert(id <= jpkfile->nids);
        g_assert(jpkfile->data[id].header_properties == NULL);

        hash = parse_header_properties(zipfile, jpkfile, error);
        jpkfile->data[id].header_properties = hash;
        /* FIXME FIXME: A segment many not have numpoints if data were not
         * collected. */
        if (!hash
            || !enumerate_channels(jpkfile, hash, TRUE, error)
            || !(ndata = find_segment_npoints(jpkfile, hash, error)))
            return FALSE;

        jpkfile->data[id].segment_style
            = g_hash_table_lookup(hash, "force-segment-header.settings"
                                        ".segment-settings.style");
        jpkfile->data[id].segment_type
            = g_hash_table_lookup(hash, "force-segment-header.settings"
                                        ".segment-settings.type");
        jpkfile->data[id].segment_name = find_sgement_name(hash);

        gwy_debug("%u, npts = %u", id, ndata);
        jpkfile->data[id].ndata = ndata;
        jpkfile->data[id].data = g_new(gdouble, ndata*jpkfile->nchannels);
        jpkfile->data[id].units = g_new(const gchar*, jpkfile->nchannels);
        /* Expect corresponding data files next. */
        for (i = 0; i < jpkfile->nchannels; i++) {
            const gchar *datafilename, *datatype;

            if (!(datatype = lookup_channel_property(jpkfile, hash, "type",
                                                     i, TRUE, error)))
                return FALSE;

            gwy_debug("data.type %s", datatype);
            if (gwy_strequal(datatype, "constant-data")) {
                g_warning("Cannot handle constant-data yet.");
                /* TODO: handle constant data. */
                continue;
            }

            if (gwy_strequal(datatype, "raster-data")) {
                g_warning("Cannot handle raster-data yet.");
                /* TODO: handle ramp data. */
                continue;
            }

            /* Otherwise we have actual data and expect a file name. */
            if (!gwyzip_next_file(zipfile, error))
                return FALSE;

            if (!(datafilename = lookup_channel_property(jpkfile, hash,
                                                         "file.name",
                                                         i, TRUE, error)))
                return FALSE;

            g_string_printf(str, "segments/%u/%s", id, datafilename);
            if (!gwyzip_get_current_filename(zipfile, &filename, error))
                return FALSE;
            gwy_debug("expecting file <%s>, found <%s>", str->str, filename);
            if (!gwy_strequal(filename, str->str)) {
                err_DATA_FILE_NAME(error, str->str, filename);
                g_free(filename);
                return FALSE;
            }

            /* Read the data. */
            if (!read_raw_data(zipfile, jpkfile, jpkfile->data + id,
                               datatype, i, error)) {
                g_free(filename);
                return FALSE;
            }
            g_free(filename);
            apply_default_channel_scaling(jpkfile, jpkfile->data + id, i);
        }
    } while (gwyzip_next_file(zipfile, NULL));

    return TRUE;
}

static gboolean
read_raw_data(GwyZipFile zipfile, JPKForceFile *jpkfile,
              JPKForceData *data, const gchar *datatype, guint i,
              GError **error)
{
    GwyRawDataType rawtype;
    GHashTable *hash = data->header_properties;
    const gchar *encoder = "";
    gsize size;
    guchar *bytes;
    gdouble q, off;

    if (gwy_stramong(datatype, "float-data", "float", NULL))
        rawtype = GWY_RAW_DATA_FLOAT;
    else if (gwy_stramong(datatype, "double-data", "double", NULL))
        rawtype = GWY_RAW_DATA_DOUBLE;
    else if (gwy_stramong(datatype, "short-data", "memory-short-data", "short",
                          NULL)) {
        if (!(encoder = lookup_channel_property(jpkfile, hash, "encoder.type",
                                                i, TRUE, error)))
            return FALSE;
        if (gwy_stramong(encoder, "unsignedshort", "unsignedshort-limited",
                         NULL))
            rawtype = GWY_RAW_DATA_UINT16;
        else if (gwy_stramong(encoder, "signedshort", "signedshort-limited",
                              NULL))
            rawtype = GWY_RAW_DATA_SINT16;
        else {
            err_UNSUPPORTED(error, "data.encoder.type");
            return FALSE;
        }
    }
    else if (gwy_stramong(datatype, "integer-data", "memory-integer-data",
                          NULL)) {
        if (!(encoder = lookup_channel_property(jpkfile, hash, "encoder.type",
                                                i, TRUE, error)))
            return FALSE;
        if (gwy_stramong(encoder, "unsignedinteger", "unsignedinteger-limited",
                         NULL))
            rawtype = GWY_RAW_DATA_UINT32;
        else if (gwy_stramong(encoder, "signedinteger", "signedinteger-limited",
                              NULL))
            rawtype = GWY_RAW_DATA_SINT32;
        else {
            err_UNSUPPORTED(error, "data.encoder.type");
            return FALSE;
        }
    }
    else if (gwy_stramong(datatype, "long-data", "memory-long-data", "long",
                          NULL)) {
        if (!(encoder = lookup_channel_property(jpkfile, hash, "encoder.type",
                                                i, TRUE, error)))
            return FALSE;
        if (gwy_stramong(encoder, "unsignedlong", "unsignedlong-limited",
                         NULL))
            rawtype = GWY_RAW_DATA_UINT64;
        else if (gwy_stramong(encoder, "signedlong", "signedlong-limited",
                              NULL))
            rawtype = GWY_RAW_DATA_SINT64;
        else {
            err_UNSUPPORTED(error, "data.encoder.type");
            return FALSE;
        }
    }
    else {
        err_UNSUPPORTED(error, "data.type");
        return FALSE;
    }

    if (!(bytes = gwyzip_get_file_content(zipfile, &size, error)))
        return FALSE;

    if (err_SIZE_MISMATCH(error, data->ndata*gwy_raw_data_size(rawtype), size,
                          TRUE)) {
        g_free(bytes);
        return FALSE;
    }

    /* Apply the encoder conversion factors.  These convert raw data to some
     * sensor physical values, typically Volts.  Conversions to values we
     * actually want to display are done later.  */
    find_scaling_parameters(jpkfile, hash, "encoder", i,
                            &q, &off, data->units + i);
    gwy_convert_raw_data(bytes, data->ndata, 1, rawtype,
                         GWY_BYTE_ORDER_BIG_ENDIAN,
                         data->data + i*data->ndata, q, off);
    g_free(bytes);
    gwy_debug("read %u (%s,%s) data points",
              data->ndata, datatype, encoder);
    return TRUE;
}

static guint
find_segment_npoints(JPKForceFile *jpkfile,
                     GHashTable *header_properties, GError **error)
{
    guint i, npts = 0;
    const gchar *s;

    for (i = 0; i < jpkfile->nchannels; i++) {
        if (!(s = lookup_channel_property(jpkfile, header_properties,
                                          "num-points", i, TRUE, error)))
            return 0;
        if (i) {
            if (atoi(s) != npts) {
                err_INVALID(error, jpkfile->str->str);
                return 0;
            }
        }
        else {
            npts = atoi(s);
            if (err_DIMENSION(error, npts))
                return FALSE;
        }
    }

    return npts;
}

static gchar*
find_sgement_name(GHashTable *header_properties)
{
    const gchar *t, *name, *prefix, *suffix;
    gchar *s;

    t = g_hash_table_lookup(header_properties,
                            "force-segment-header.settings"
                            ".segment-settings.identifier.type");
    name = g_hash_table_lookup(header_properties,
                               "force-segment-header.settings"
                               ".segment-settings.identifier.name");
    if (!name)
        return NULL;

    if (!t) {
        g_warning("Missing identifier type.");
        return g_strdup(name);
    }

    if (gwy_strequal(t, "standard")) {
        s = g_strdup(name);
        s[0] = g_ascii_toupper(s[0]);
        return s;
    }
    if (gwy_strequal(t, "ExtendedStandard")) {
        prefix = g_hash_table_lookup(header_properties,
                                     "force-segment-header.settings"
                                     ".segment-settings.identifier.prefix");
        suffix = g_hash_table_lookup(header_properties,
                                     "force-segment-header.settings"
                                     ".segment-settings.identifier.suffix");
        if (prefix && suffix)
            return g_strconcat(prefix, name, suffix, NULL);
        g_warning("Prefix or suffix missing for ExtendedStandard identifier.");
        return g_strdup(name);
    }
    if (gwy_strequal(t, "user"))
        return g_strdup(name);

    g_warning("Unknown identifier type %s.", t);
    return g_strdup(name);
}

static gboolean
apply_default_channel_scaling(JPKForceFile *jpkfile,
                              JPKForceData *data,
                              guint i)
{
    GHashTable *header_properties = data->header_properties;
    const gchar *default_cal;
    gdouble q, off;
    gdouble *d;
    gchar *key;
    guint j, ndata;

    default_cal = lookup_channel_property(jpkfile, header_properties,
                                          "conversion-set.conversions.default",
                                          i, FALSE, NULL);
    if (!default_cal) {
        g_warning("Cannot find the default conversion.");
        return FALSE;
    }

    key = g_strconcat("conversion-set.conversion.", default_cal, NULL);
    if (!find_scaling_parameters(jpkfile, header_properties, key, i,
                                 &q, &off, data->units + i)) {
        g_free(key);
        return FALSE;
    }
    g_free(key);

    ndata = data->ndata;
    d = data->data + i*ndata;
    for (j = 0; j < ndata; j++)
        d[j] = q*d[j] + off;

    return TRUE;
}

static const gchar*
lookup_scaling_property(JPKForceFile *jpkfile,
                        GHashTable *hash,
                        const gchar *subkey,
                        guint len,
                        guint i,
                        const gchar *expected_value)
{
    GString *key = jpkfile->qstr;
    const gchar *s;

    g_string_truncate(key, len);
    g_string_append(key, subkey);
    s = lookup_channel_property(jpkfile, hash, key->str, i, FALSE, NULL);
    if (!s) {
        g_warning("Cannot find %s.", key->str);
        return NULL;
    }
    if (expected_value && !gwy_strequal(s, expected_value)) {
        g_warning("Value of %s is not %s.", key->str, expected_value);
        return NULL;
    }
    return s;
}

/* Subkey is typically something like "data.encoder" for conversion from
 * integral data; or "conversion-set.conversion.force" for calibrations.
 * Note calibrations can be nested, we it can refer recursively to
 * "base-calibration-slot" and we have to perform that calibration first. */
static gboolean
find_scaling_parameters(JPKForceFile *jpkfile,
                        GHashTable *hash,
                        const gchar *subkey,
                        guint i,
                        gdouble *multiplier,
                        gdouble *offset,
                        const gchar **unit)
{
    /* There seem to be different unit styles.  Documentation says just "unit"
     * but I see "unit.type" and "unit.unit" for the actual unit.  Try both. */
    static const gchar *unit_keys[] = { "unit.unit", "unit" };

    gdouble base_multipler, base_offset;
    const gchar *base_unit;  /* we ignore that; they do not specify factors
                                but directly units of the results. */
    /* NB: This function can recurse.  Must avoid overwriting! */
    GString *key = jpkfile->qstr;
    const gchar *s, *bcs;
    gchar *bcskey;
    guint len, j;

    *multiplier = 1.0;
    *offset = 0.0;
    /* Do not set the unit unless some unit is found. */

    g_string_assign(key, subkey);
    g_string_append_c(key, '.');
    len = key->len;

    /* If the scaling has defined=false, it means there is no scaling to
     * perform.  This occurs for the base calibration.  In principle, we should
     * already know we are at the base calibration by looking at
     * "conversions.base" but we do not bother at present. */
    g_string_append(key, "defined");
    if ((s = lookup_channel_property(jpkfile, hash, key->str, i, FALSE, NULL))
        && gwy_strequal(s, "false"))
        return TRUE;

    g_string_truncate(key, len);
    g_string_append(key, "scaling.");
    len = key->len;

    if (!lookup_scaling_property(jpkfile, hash, "type", len, i, "linear"))
        return FALSE;
    if (!lookup_scaling_property(jpkfile, hash, "style", len, i,
                                 "offsetmultiplier"))
        return FALSE;
    if ((s = lookup_scaling_property(jpkfile, hash, "offset", len, i, NULL)))
        *offset = g_ascii_strtod(s, NULL);
    if ((s = lookup_scaling_property(jpkfile, hash, "multiplier", len, i, NULL)))
        *multiplier = g_ascii_strtod(s, NULL);

    for (j = 0; j < G_N_ELEMENTS(unit_keys); j++) {
        g_string_truncate(key, len);
        g_string_append(key, unit_keys[j]);
        s = lookup_channel_property(jpkfile, hash, key->str, i, FALSE, NULL);
        if (s) {
            *unit = s;
            break;
        }
    }
    if (!*unit)
        g_warning("Cannot find scaling unit.");

    /* If there is no base calibration slot we have the final calibration
     * parameters. */
    g_string_assign(key, subkey);
    len = key->len;
    g_string_append(key, ".base-calibration-slot");
    bcs = lookup_channel_property(jpkfile, hash, key->str, i, FALSE, NULL);
    if (!bcs)
        return TRUE;

    /* Otherwise we have to recurse.  First assume the calibration slot name
     * is the same as the calibration name (yes, there seems another level
     * of indirection). */
    if (!(s = strrchr(subkey, '.'))) {
        g_warning("Cannot form base calibration name becaue there is no dot "
                  "in the original name.");
        return FALSE;
    }
    g_string_truncate(key, s+1 - subkey);
    g_string_append(key, bcs);
    bcskey = g_strdup(key->str);
    if (find_scaling_parameters(jpkfile, hash, bcskey, i,
                                &base_multipler, &base_offset, &base_unit)) {
        g_free(bcskey);
        *multiplier *= base_multipler;
        *offset += *multiplier * base_offset;
        /* Ignore base unit. */
        return TRUE;
    }

    /* XXX: The name does not necessarily have to be the same.  We should
     * look for base calibration with "calibration-slot" equal to @bcskey, but
     * that requires scanning the entire dictionary (XXX: maybe not, there
     * is "conversions.list" field listing all the conversions – but whether
     * the names are slot or conversion names, no one knows. */
    g_warning("Cannot figure out base calibration (trying %s).", bcskey);
    g_free(bcskey);
    return FALSE;
}

static const gchar*
lookup_channel_property(JPKForceFile *jpkfile,
                        GHashTable *header_properties, const gchar *subkey,
                        guint i,
                        gboolean fail_if_not_found, GError **error)
{
    GError *err = NULL;
    GString *str = jpkfile->str;
    const gchar *retval;
    guint len;

    g_return_val_if_fail(i < jpkfile->nchannels, NULL);
    g_string_assign(str, "channel.");
    g_string_append(str, jpkfile->channel_names[i]);
    g_string_append_c(str, '.');

    /* Some things are found under "data" in documentation but under "lcd-info"
     * in real files.  Some may be only in one location but we simply try both
     * for all keys. */
    len = str->len;
    g_string_append(str, "lcd-info.");
    g_string_append(str, subkey);
    if ((retval = lookup_property(jpkfile, header_properties, str->str,
                                  fail_if_not_found, &err)))
        return retval;

    g_string_truncate(str, len);
    g_string_append(str, "data.");
    g_string_append(str, subkey);
    if ((retval = lookup_property(jpkfile, header_properties, str->str,
                                  FALSE, NULL)))
        return retval;

    if (fail_if_not_found) {
        /* @err cannot be set otherwise. */
        g_propagate_error(error, err);
    }
    return NULL;
}

/* Look up a property in provided @header_properties and, failing that, in
 * the shared properties. */
static const gchar*
lookup_property(JPKForceFile *jpkfile,
                GHashTable *header_properties, const gchar *key,
                gboolean fail_if_not_found, GError **error)
{
    GString *sstr = jpkfile->sstr;
    const gchar *s, *dot;
    guint len;

    /* Direct lookup. */
    if ((s = g_hash_table_lookup(header_properties, key)))
        return s;

    /* If there are shared properties and a *-reference we have a second
     * chance. */
    if (jpkfile->shared_header_properties) {
        g_string_assign(sstr, key);
        while ((dot = strrchr(sstr->str, '.'))) {
            len = dot - sstr->str;
            g_string_truncate(sstr, len+1);
            g_string_append_c(sstr, '*');
            if ((s = g_hash_table_lookup(header_properties, sstr->str)))
                break;
            g_string_truncate(sstr, len);
        }
    }

    /* Not found or we have zero prefix. */
    if (!s || !len)
        goto fail;

    /* Try to look it up in the shared properties.  The part just before .*
     * is the beginning of the property name in the shared properties.  */
    g_string_truncate(sstr, len);
    if ((dot = strrchr(sstr->str, '.')))
        g_string_erase(sstr, 0, dot+1 - sstr->str);
    g_string_append_c(sstr, '.');
    g_string_append(sstr, s);
    g_string_append(sstr, key + len);
    gwy_debug("shared properties key <%s>", sstr->str);

    if ((s = g_hash_table_lookup(jpkfile->shared_header_properties, sstr->str)))
        return s;

fail:
    if (fail_if_not_found)
        err_MISSING_FIELD(error, key);
    return NULL;
}

static gboolean
enumerate_channels(JPKForceFile *jpkfile, GHashTable *header_properties,
                   gboolean needslist, GError **error)
{
    const gchar *s, *ss;
    gchar **fields;
    guint i, n, len;

    if (!header_properties
        || !(s = g_hash_table_lookup(header_properties, "channels.list"))) {
        if (!needslist || jpkfile->channel_names)
            return TRUE;
        err_MISSING_FIELD(error, "channels.list");
        return FALSE;
    }

    /* If we already have some channel list, check if it matches. */
    if (jpkfile->channel_names) {
        n = jpkfile->nchannels;
        for (i = 0; i < n-1; i++) {
            ss = jpkfile->channel_names[i];
            len = strlen(ss);
            if (memcmp(s, ss, len) != 0 || s[len] != ' ')
                return err_NONUNIFORM_CHANNELS(error);
            s += len+1;
        }
        ss = jpkfile->channel_names[i];
        if (!gwy_strequal(s, ss))
            return err_NONUNIFORM_CHANNELS(error);
        /* There is a perfect match. */
        return TRUE;
    }

    /* There is no channel yet list so construct it from what we found. */
    fields = g_strsplit(s, " ", -1);
    n = g_strv_length(fields);
    if (!n) {
        g_free(fields);
        err_NO_DATA(error);
        return NULL;
    }

    jpkfile->nchannels = n;
    jpkfile->channel_names = g_new(gchar*, n);
    for (i = 0; i < n; i++) {
        jpkfile->channel_names[i] = fields[i];
        gwy_debug("channel[%u] = <%s>", i, fields[i]);
    }
    g_free(fields);

    return TRUE;
}

static gboolean
analyse_segment_ids(JPKForceFile *jpkfile, GError **error)
{
    guint i, nids = jpkfile->nids;

    g_assert(jpkfile->type == JPK_FORCE_CURVES);
    for (i = 0; i < nids; i++) {
        if (jpkfile->ids[i] != i)
            return err_IRREGULAR_NUMBERING(error);
    }

    jpkfile->nsegs = nids;
    jpkfile->npoints = 1;
    return TRUE;
}

static gboolean
analyse_map_segment_ids(JPKForceFile *jpkfile, GError **error)
{
    guint i, j, k, nids = jpkfile->nids;
    guint nsegs, npoints, idx0, idx;

    g_assert(jpkfile->type == JPK_FORCE_MAP || jpkfile->type == JPK_FORCE_QI);
    idx0 = jpkfile->ids[0];
    for (i = 1; i < nids; i++) {
        idx = jpkfile->ids[2*i];
        if (idx != idx0)
            break;
    }
    nsegs = i;
    if (nids % nsegs != 0)
        return err_IRREGULAR_NUMBERING(error);

    npoints = nids/nsegs;
    for (i = 0; i < npoints; i++) {
        for (j = 0; j < nsegs; j++) {
            k = i*nsegs + j;
            if (jpkfile->ids[2*k] != i || jpkfile->ids[2*k + 1] != j)
                return err_IRREGULAR_NUMBERING(error);
        }
    }

    jpkfile->nsegs = nsegs;
    jpkfile->npoints = npoints;
    return TRUE;
}

static gboolean
enumerate_segments(GwyZipFile zipfile, JPKForceFile *jpkfile)
{
    GArray *ids;

    if (!gwyzip_first_file(zipfile, NULL))
        return FALSE;

    ids = g_array_new(FALSE, FALSE, sizeof(gint));
    do {
        gchar *filename = NULL, *suffix = NULL;
        guint id;

        if (!gwyzip_get_current_filename(zipfile, &filename, NULL))
            continue;

        if ((suffix = match_segment_filename(filename,
                                             jpkfile->segment_regex,
                                             &id))) {
            if (gwy_strequal(suffix, "segment-header.properties")) {
                g_array_append_val(ids, id);
                gwy_debug("segment: %s -> %u", filename, id);
            }
            g_free(suffix);
        }
        g_free(filename);
    } while (gwyzip_next_file(zipfile, NULL));

    if (!ids->len) {
        g_array_free(ids, TRUE);
        return FALSE;
    }

    g_array_sort(ids, compare_uint);
    jpkfile->nids = ids->len;
    jpkfile->ids = (guint*)g_array_free(ids, FALSE);
    return TRUE;
}

static gboolean
enumerate_map_segments(GwyZipFile zipfile, JPKForceFile *jpkfile)
{
    GArray *ids;

    if (!gwyzip_first_file(zipfile, NULL))
        return FALSE;

    ids = g_array_new(FALSE, FALSE, 2*sizeof(gint));
    do {
        gchar *filename = NULL, *suffix = NULL;
        guint id[2];

        if (!gwyzip_get_current_filename(zipfile, &filename, NULL))
            continue;

        if ((suffix = match_map_segment_filename(filename,
                                                 jpkfile->index_segment_regex,
                                                 id + 0, id + 1))) {
            if (gwy_strequal(suffix, "segment-header.properties")) {
                g_array_append_val(ids, id);
                gwy_debug("map segment: %s -> %u,%u", filename, id[0], id[1]);
            }
            g_free(suffix);
        }
        g_free(filename);
    } while (gwyzip_next_file(zipfile, NULL));

    if (!ids->len) {
        g_array_free(ids, TRUE);
        return FALSE;
    }

    g_array_sort(ids, compare_uint2);
    jpkfile->nids = ids->len;
    jpkfile->ids = (guint*)g_array_free(ids, FALSE);
    return TRUE;
}

static GHashTable*
parse_header_properties(GwyZipFile zipfile, JPKForceFile *jpkfile,
                        GError **error)
{
    GwyTextHeaderParser parser;
    GHashTable *hash;
    guchar *contents;
    gsize size;

    if (!(contents = gwyzip_get_file_content(zipfile, &size, error)))
        return NULL;

    jpkfile->buffers = g_slist_prepend(jpkfile->buffers, contents);
    hash = g_hash_table_new(g_str_hash, g_str_equal);

    gwy_clear(&parser, 1);
    parser.comment_prefix = "#";
    parser.key_value_separator = "=";
    hash = gwy_text_header_parse((gchar*)contents, &parser, NULL, error);
#ifdef DEBUG
    if (hash) {
        gchar *filename;
        if (gwyzip_get_current_filename(zipfile, &filename, NULL)) {
            gwy_debug("%s has %u entries", filename, g_hash_table_size(hash));
            g_free(filename);
        }
        else {
            gwy_debug("UNKNOWNFILE? has %u entries", g_hash_table_size(hash));
        }
    }
#endif

    return hash;
}

static void
jpk_force_file_free(JPKForceFile *jpkfile)
{
    GSList *l;
    guint i;

    g_free(jpkfile->ids);

    for (i = 0; i < jpkfile->nchannels; i++)
        g_free(jpkfile->channel_names[i]);
    g_free(jpkfile->channel_names);

    if (jpkfile->segment_regex)
        g_regex_unref(jpkfile->segment_regex);
    if (jpkfile->index_segment_regex)
        g_regex_unref(jpkfile->index_segment_regex);

    if (jpkfile->str)
        g_string_free(jpkfile->str, TRUE);
    if (jpkfile->sstr)
        g_string_free(jpkfile->sstr, TRUE);
    if (jpkfile->qstr)
        g_string_free(jpkfile->qstr, TRUE);

    if (jpkfile->header_properties)
        g_hash_table_destroy(jpkfile->header_properties);

    if (jpkfile->shared_header_properties)
        g_hash_table_destroy(jpkfile->shared_header_properties);

    if (jpkfile->point_header_properties) {
        for (i = 0; i < jpkfile->npoints; i++) {
            if (jpkfile->point_header_properties[i])
                g_hash_table_destroy(jpkfile->point_header_properties[i]);
        }
        g_free(jpkfile->point_header_properties);
    }

    if (jpkfile->data) {
        for (i = 0; i < jpkfile->nids; i++) {
            if (jpkfile->data[i].header_properties)
                g_hash_table_destroy(jpkfile->data[i].header_properties);
            g_free(jpkfile->data[i].data);
            g_free(jpkfile->data[i].segment_name);
            g_free(jpkfile->data[i].units);
        }
        g_free(jpkfile->data);
    }

    while ((l = jpkfile->buffers)) {
        jpkfile->buffers = g_slist_next(l);
        g_slist_free_1(l);
    }
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
