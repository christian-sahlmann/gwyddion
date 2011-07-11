/*
 *  @(#) $Id$
 *  Loader for JPK Image Scans.
 *  Copyright (C) 2005  JPK Instruments AG.
 *  Written by Sven Neumann <neumann@jpk.com>.
 *
 *  Rewritten to use GwyTIFF by Yeti <yeti@gwyddion.net>.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * JPK Instruments
 * .jpk
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "jpk.h"
#include "gwytiff.h"

static gboolean      module_register     (void);
static gint          jpkscan_detect      (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* jpkscan_load        (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static void          jpk_load_channel    (const GwyTIFF *tiff,
                                          const GwyTIFFImageReader *reader,
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


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports JPK image scans."),
    "Sven Neumann <neumann@jpk.com>, Yeti <yeti@gwyddion.net>",
    "0.8",
    "JPK Instruments AG",
    "2005-2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("jpkscan",
                           N_("JPK image scans (.jpk)"),
                           (GwyFileDetectFunc)&jpkscan_detect,
                           (GwyFileLoadFunc)&jpkscan_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
jpkscan_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    gdouble ulen, vlen;
    gchar *name = NULL;
    gint score = 0;

    if (only_name)
        return score;

    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
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

        jpk_load_channel(tiff, reader, container, meta, idx, ulen, vlen);
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
