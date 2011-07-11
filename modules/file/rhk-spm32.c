/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-rhk-sm2-spm">
 *   <comment>RHK SM2 SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="STiMage 3.1"/>
 *   </magic>
 *   <glob pattern="*.sm2"/>
 *   <glob pattern="*.SM2"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * RHK Instruments SM2
 * .sm2
 * Read SPS:Limited[1]
 * [1] Spectra curves are imported as graphs, positional information is lost.
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <libprocess/spectra.h>
#include <app/data-browser.h>

#include "err.h"

#define HEADER_SIZE 512

#define MAGIC "STiMage 3.1"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)
#define EXTENSION ".sm2"

#define GRAPH_PREFIX "/0/graph/graph"

typedef enum {
    RHK_TYPE_IMAGE =            0,
    RHK_TYPE_LINE =             1,
    RHK_TYPE_ANNOTATED_LINE =   3
} RHKType;

typedef enum {
    RHK_SCAN_RIGHT = 0,
    RHK_SCAN_LEFT  = 1,
    RHK_SCAN_UP    = 2,
    RHK_SCAN_DOWN  = 3
} RHKScanType;

typedef enum {
    RHK_DATA_SINGLE    = 0,
    RHK_DATA_INT16     = 1,
    RHK_DATA_INT32     = 2,
    RHK_DATA_INT8      = 3
} RHKDataType;

typedef enum {
    RHK_IMAGE_UNDEFINED                = 0,
    RHK_IMAGE_TOPOGAPHIC               = 1,
    RHK_IMAGE_CURRENT                  = 2,
    RHK_IMAGE_AUX                      = 3,
    RHK_IMAGE_FORCE                    = 4,
    RHK_IMAGE_SIGNAL                   = 5,
    RHK_IMAGE_FFT                      = 6,
    RHK_IMAGE_NOISE_POWER_SPECTRUM     = 7,
    RHK_IMAGE_LINE_TEST                = 8,
    RHK_IMAGE_OSCILLOSCOPE             = 9,
    RHK_IMAGE_IV_SPECTRA               = 10,
    RHK_IMAGE_IV_4x4                   = 11,
    RHK_IMAGE_IV_8x8                   = 12,
    RHK_IMAGE_IV_16x16                 = 13,
    RHK_IMAGE_IV_32x32                 = 14,
    RHK_IMAGE_IV_CENTER                = 15,
    RHK_IMAGE_INTERACTIVE_SPECTRA      = 16,
    RHK_IMAGE_AUTOCORRELATION          = 17,
    RHK_IMAGE_IZ_SPECTRA               = 18,
    RHK_IMAGE_4_GAIN_TOPOGRAPHY        = 19,
    RHK_IMAGE_8_GAIN_TOPOGRAPHY        = 20,
    RHK_IMAGE_4_GAIN_CURRENT           = 21,
    RHK_IMAGE_8_GAIN_CURRENT           = 22,
    RHK_IMAGE_IV_64x64                 = 23,
    RHK_IMAGE_AUTOCORRELATION_SPECTRUM = 24,
    RHK_IMAGE_COUNTER                  = 25,
    RHK_IMAGE_MULTICHANNEL_ANALYSER    = 26,
    RHK_IMAGE_AFM_100                  = 27,
    RHK_IMAGE_LAST
} RHKPageType;

typedef struct {
    gdouble scale;
    gdouble offset;
    gchar *units;
} RHKRange;

typedef struct {
    gchar *date;
    guint xres;
    guint yres;
    RHKType type;
    RHKDataType data_type;
    guint item_size;
    guint line_type;
    guint size;
    RHKPageType page_type;
    RHKRange x;
    RHKRange y;
    RHKRange z;
    gdouble xyskew;
    gdouble alpha;
    gboolean e_alpha;
    RHKRange iv;
    guint scan;
    gdouble period;
    guint id;
    guint data_offset;
    gchar *label;
    gchar *comment;

    const guchar *buffer;
} RHKPage;

static gboolean       module_register      (void);
static gint           rhkspm32_detect      (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer*  rhkspm32_load        (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static gboolean       rhkspm32_read_header (RHKPage *rhkpage,
                                            GError **error);
static gboolean       rhkspm32_read_range  (const gchar *buffer,
                                            const gchar *name,
                                            RHKRange *range);
static void           rhkspm32_free        (RHKPage *rhkpage);
static GwyContainer*  rhkspm32_get_metadata(RHKPage *rhkpage);
static GwyDataField*  rhkspm32_read_data   (RHKPage *rhkpage);
static GwySpectra*    rhkspm32_read_spectra(RHKPage *rhkpage);
static GwyGraphModel* spectra_to_graph     (GwySpectra *spectra);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports RHK Technology SPM32 data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.9",
    "David Nečas (Yeti) & Petr Klapetek, mod by Niv Levy",
    "2007",
};

static const GwyEnum scan_directions[] = {
    { "Right", RHK_SCAN_RIGHT, },
    { "Left",  RHK_SCAN_LEFT,  },
    { "Up",    RHK_SCAN_UP,    },
    { "Down",  RHK_SCAN_DOWN,  },
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("rhk-spm32",
                           N_("RHK SPM32 files (.sm2)"),
                           (GwyFileDetectFunc)&rhkspm32_detect,
                           (GwyFileLoadFunc)&rhkspm32_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
rhkspm32_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
rhkspm32_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GArray *rhkfile;
    RHKPage *rhkpage;
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gsize totalpos, pagesize;
    GString *key;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < HEADER_SIZE) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    // niv - rhkfile is an array of rhkpage's, but buffer is where the actual
    // raw file data is stored
    rhkfile = g_array_new(FALSE, TRUE, sizeof(RHKPage));
    totalpos = 0;

    while (totalpos < size) {
        g_array_set_size(rhkfile, rhkfile->len + 1);
        rhkpage = &g_array_index(rhkfile, RHKPage, rhkfile->len - 1);
        rhkpage->buffer = buffer + totalpos;
        // niv - if the header seems illegal, skip all the next ones as well
        // (and cancel the element addition to the g_array)
        if (!rhkspm32_read_header(rhkpage, &err)) {
            g_warning("failed to read rhk header after %u", rhkfile->len);
            g_array_set_size(rhkfile, rhkfile->len - 1);
            break;
        }

        pagesize = rhkpage->data_offset
                   + rhkpage->item_size*rhkpage->xres*rhkpage->yres;
        if (size < totalpos + pagesize) {
            rhkspm32_free(rhkpage);
            g_array_set_size(rhkfile, rhkfile->len - 1);
            break;
        }

        totalpos += pagesize;
    }

    /* Be tolerant and don't fail when we were able to import at least
     * something */
    if (!rhkfile->len) {
        if (err)
            g_propagate_error(error, err);
        else
            err_NO_DATA(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        g_array_free(rhkfile, TRUE);
        return NULL;
    }
    g_clear_error(&err);

    container = gwy_container_new();
    key = g_string_new(NULL);
    for (i = 0; i < rhkfile->len; i++) {
        const gchar *cs;
        gchar *s;

        gwy_debug("rhk-spm32: processing page %d of %d\n", i+1, rhkfile->len);
        rhkpage = &g_array_index(rhkfile, RHKPage, i);
        if (rhkpage->type == RHK_TYPE_IMAGE) { // niv - just leaving this alone
            dfield = rhkspm32_read_data(rhkpage);
            g_string_printf(key, "/%d/data", i);
            gwy_container_set_object_by_name(container, key->str, dfield);
            g_object_unref(dfield);
            g_string_append(key, "/title");
            cs = gwy_enum_to_string(rhkpage->scan,
                                    scan_directions,
                                    G_N_ELEMENTS(scan_directions));
            if (rhkpage->label) {
                if (cs)
                    s = g_strdup_printf("%s [%s]", rhkpage->label, cs);
                else
                    s = g_strdup(rhkpage->label);
                gwy_container_set_string_by_name(container, key->str, s);
            }
            else
                gwy_app_channel_title_fall_back(container, i);
        }
        else if (rhkpage->type == RHK_TYPE_LINE) { // niv - after omicron.c

            GwySpectra* spectra;
            GwyGraphModel *gmodel;

            spectra = rhkspm32_read_spectra(rhkpage);
            /* converting to graphs, as there is no point in leaving these as
             * sps - no xy coordinates, so the spectro tool is kinda clueless */
            gwy_debug("processing graph in page %d\n", i);
            if ((gmodel = spectra_to_graph(spectra)) != NULL) {
                gchar *container_key = NULL;
                /* add gmodel to container */
                container_key = g_strdup_printf("%s/%d", GRAPH_PREFIX, i);
                gwy_container_set_object_by_name(container, container_key,
                                                gmodel);
                g_free(container_key);
            }
            g_object_unref(gmodel);
            g_object_unref(spectra);
        }
        gwy_debug("rhk-spm32: finished parsing page %d \n", i);
        meta = rhkspm32_get_metadata(rhkpage);
        if (rhkpage->type == RHK_TYPE_IMAGE) {
        /* this doesn't really work, but at least the meta data stays
         with the graph, even if the metadata viewer won't show it*/
            g_string_printf(key, "/%d/meta", i);
        }
        else if (rhkpage->type == RHK_TYPE_LINE) {
            g_string_printf(key, "%s/%d/meta", GRAPH_PREFIX, i);
        }
        gwy_container_set_object_by_name(container, key->str, meta);
        g_object_unref(meta);
    }
    g_string_free(key, TRUE);

    gwy_file_abandon_contents(buffer, size, NULL);
    for (i = 0; i < rhkfile->len; i++)
        rhkspm32_free(&g_array_index(rhkfile, RHKPage, i));
    g_array_free(rhkfile, TRUE);

    return container;
}

static gboolean
rhkspm32_read_header(RHKPage *rhkpage,
                     GError **error)
{
    const gchar *buffer;
    gchar *end;
    guint pos;

    buffer = rhkpage->buffer;

    rhkpage->date = g_strstrip(g_strndup(buffer + MAGIC_SIZE,
                                         0x20 - MAGIC_SIZE));
    if (sscanf(buffer + 0x20, "%d %d %d %d %d %d %d",
               (gint*)&rhkpage->type, (gint*)&rhkpage->data_type,
               &rhkpage->line_type,
               &rhkpage->xres, &rhkpage->yres, &rhkpage->size,
               (gint*)&rhkpage->page_type) != 7) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Invalid file header."));
        return FALSE;
    }
    gwy_debug("type = %u, data = %u, line = %u, image = %u",
              rhkpage->type, rhkpage->data_type, rhkpage->line_type,
              rhkpage->page_type);
    gwy_debug("xres = %d, yres = %d", rhkpage->xres, rhkpage->yres);
    if (err_DIMENSION(error, rhkpage->xres)
        || err_DIMENSION(error, rhkpage->yres))
        return FALSE;
    if (!((rhkpage->type == RHK_TYPE_IMAGE)
          || (rhkpage->type == RHK_TYPE_LINE))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Only image and line files are supported."));
        return FALSE;
    }
    if ((rhkpage->type == RHK_TYPE_IMAGE)
        && (rhkpage->data_type != RHK_DATA_INT16)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Invalid data type %d for image data."),
                    rhkpage->data_type);
        return FALSE;
    }
    if ((rhkpage->type == RHK_TYPE_LINE)
        && !((rhkpage->data_type == RHK_DATA_INT16)
             || (rhkpage->data_type == RHK_DATA_SINGLE))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Invalid data type %d for line data."),
                    rhkpage->data_type);
        return FALSE;
    }

    if ((rhkpage->data_type) == RHK_DATA_INT8)
        rhkpage->item_size = 1;
    else if ((rhkpage->data_type) == RHK_DATA_INT16)
        rhkpage->item_size = 2;
    else if ((rhkpage->data_type) == RHK_DATA_INT32)
        rhkpage->item_size = 4;
    else if ((rhkpage->data_type) == RHK_DATA_SINGLE)
        rhkpage->item_size = 4;

    //rhkpage->item_size = rhkpage->data_type; // niv

    if (!rhkspm32_read_range(buffer + 0x40, "X", &rhkpage->x)
        || !rhkspm32_read_range(buffer + 0x60, "Y", &rhkpage->y)
        || !rhkspm32_read_range(buffer + 0x80, "Z", &rhkpage->z)) {
        err_INVALID(error, _("data ranges"));
        return FALSE;
    }

    /* Use negated positive conditions to catch NaNs */
    // niv - modifying this - otherwise it messes with the spectra
    // (but i don;t really understand it)
    if (!((fabs(rhkpage->x.scale)) > 0)) {
        g_warning("Real x scale is 0.0, fixing to 1.0");
        rhkpage->x.scale = 1.0;
    }
    if (!((fabs(rhkpage->y.scale)) > 0)) {
        /* The y scale seem unused for non-image data */
        if (rhkpage->type == RHK_TYPE_IMAGE)
            g_warning("Real y scale is 0.0, fixing to 1.0");
        rhkpage->y.scale = 1.0;
    }

    if (!g_str_has_prefix(buffer + 0xa0, "XY ")) {
        err_MISSING_FIELD(error, "XY");
        return FALSE;
    }
    pos = 0xa0 + sizeof("XY");
    rhkpage->xyskew = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos) {
        err_INVALID(error, "XY");
        return FALSE;
    }
    pos = (end - buffer) + 2;
    /* Don't check failure, it seems the value is optional */
    rhkpage->alpha = g_ascii_strtod(buffer + pos, &end);
    // not failing, but setting an existance flag, this happens for spectra,
    // but otherwise i want to add this to the metadata
    if (end == buffer + pos)
        rhkpage->e_alpha = FALSE;
    else
        rhkpage->e_alpha = TRUE;

    if (!rhkspm32_read_range(buffer + 0xc0, "IV", &rhkpage->iv)) {
        err_INVALID(error, "IV");
        return FALSE;
    }

    if (g_str_has_prefix(buffer + 0xe0, "scan "))
        pos = 0xe0 + sizeof("scan");
    rhkpage->scan = strtol(buffer + pos, &end, 10);
    if (end == buffer + pos) {
        err_INVALID(error, "scan");
        return FALSE;
    }
    pos = (end - buffer);
    rhkpage->period = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos) {
        err_INVALID(error, "period");
        return FALSE;
    }

    if (sscanf(buffer + 0x100, "id %u %u",
               &rhkpage->id, &rhkpage->data_offset) != 2) {
        /* XXX: Some braindamaged files encountered in practice do not contain
         * the data offset.  Cross fingers and substitute HEADER_SIZE.  */
        g_warning("Data offset is missing, just guessing from now...");
        rhkpage->id = 0;
        rhkpage->data_offset = HEADER_SIZE;
    }
    gwy_debug("data_offset = %u", rhkpage->data_offset);
    if (rhkpage->data_offset < HEADER_SIZE) {
        err_INVALID(error, _("data offset"));
        return FALSE;
    }

    /* XXX: The same braindamaged files overwrite the label and comment part
     * with some XML mumbo jumbo.  Sigh and ignore it.  */
    if (strncmp(buffer + 0x140, "\x0d\x0a<?", 4) != 0) {
        rhkpage->label = g_strstrip(g_strndup(buffer + 0x140, 0x20));
        rhkpage->comment = g_strstrip(g_strndup(buffer + 0x160,
                                                HEADER_SIZE - 0x160));
    }

    return TRUE;
}

static gboolean
rhkspm32_read_range(const gchar *buffer,
                    const gchar *name,
                    RHKRange *range)
{
    gchar *end;
    guint pos;

    if (!g_str_has_prefix(buffer, name))
        return FALSE;
    pos = strlen(name) + 1;

    // this is a bad idea - for spectra it's perfectly reasonable to have
    // negative scales (e.g. progress from positive to negative bias)
    //range->scale = fabs(g_ascii_strtod(buffer + pos, &end));
    range->scale = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos || pos > 0x20)
        return FALSE;
    pos = end - buffer;

    range->offset = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos || pos > 0x20)
        return FALSE;
    pos = end - buffer;

    range->units = g_strstrip(g_strndup(buffer + pos, 0x20 - pos));
    gwy_debug("<%s> %g %g <%s>",
              name, range->scale, range->offset, range->units);

    return TRUE;
}

static void
rhkspm32_free(RHKPage *rhkpage)
{
    g_free(rhkpage->date);
    g_free(rhkpage->x.units);
    g_free(rhkpage->y.units);
    g_free(rhkpage->z.units);
    g_free(rhkpage->iv.units);
    g_free(rhkpage->label);
    g_free(rhkpage->comment);
}

static GwyContainer*
rhkspm32_get_metadata(RHKPage *rhkpage)
{
    GwyContainer *meta;
    const gchar *s;

    meta = gwy_container_new();

    gwy_container_set_string_by_name(meta, "Tunneling voltage",
                                     g_strdup_printf("%g mV",
                                                     1e3*rhkpage->iv.offset));
    gwy_container_set_string_by_name(meta, "Current",
                                     g_strdup_printf("%g nA",
                                                     1e9*rhkpage->iv.scale));
    if (rhkpage->id)
        gwy_container_set_string_by_name(meta, "Id",
                                         g_strdup_printf("%u", rhkpage->id));
    if (rhkpage->date && *rhkpage->date)
        gwy_container_set_string_by_name(meta, "Date", g_strdup(rhkpage->date));
    if (rhkpage->comment && *rhkpage->comment)
        gwy_container_set_string_by_name(meta, "Comment",
                                         g_strdup(rhkpage->comment));
    if (rhkpage->label && *rhkpage->label)
        gwy_container_set_string_by_name(meta, "Label",
                                         g_strdup(rhkpage->label));

    s = gwy_enum_to_string(rhkpage->page_type,
                           scan_directions, G_N_ELEMENTS(scan_directions));
    if (s && *s)
        gwy_container_set_string_by_name(meta, "Image type", g_strdup(s));

    // FIXME - seems one can read 0 for spectra as well, but maybe it's not
    // that important - it should be clear that this is a nonsense value
    if (rhkpage->e_alpha)
        gwy_container_set_string_by_name(meta, "Angle",
                                         g_strdup_printf("%g deg",
                                                         rhkpage->alpha));

    return meta;
}

static GwyDataField*
rhkspm32_read_data(RHKPage *rhkpage)
{
    GwyDataField *dfield;
    const guint16 *p;
    GwySIUnit *siunit;
    gdouble *data;
    const gchar *s;
    gdouble q;
    gint power10;
    guint i, j, xres, yres;

    p = (const guint16*)(rhkpage->buffer + rhkpage->data_offset);
    xres = rhkpage->xres;
    yres = rhkpage->yres;
    // the scales are no longer gurunteed to be positive,
    // so they must be "fixed" here (to enable spectra)
    dfield = gwy_data_field_new(xres, yres,
                                xres*fabs(rhkpage->x.scale),
                                yres*fabs(rhkpage->y.scale),
                                FALSE);

    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++)
            data[i*xres + xres-1 - j] = GINT16_FROM_LE(p[i*xres + j]);
    }

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_from_string_parse(siunit, rhkpage->x.units, &power10);
    if (power10) {
        q = pow10(power10);
        gwy_data_field_set_xreal(dfield, q*gwy_data_field_get_xreal(dfield));
        gwy_data_field_set_yreal(dfield, q*gwy_data_field_get_yreal(dfield));
    }

    siunit = gwy_data_field_get_si_unit_z(dfield);
    s = rhkpage->z.units;
    /* Fix some silly units */
    if (gwy_strequal(s, "N/sec"))
        s = "s^-1";
    gwy_si_unit_set_from_string_parse(siunit, s, &power10);
    q = pow10(power10);

    gwy_data_field_multiply(dfield, q*fabs(rhkpage->z.scale));
    gwy_data_field_add(dfield, q*rhkpage->z.offset);

    return dfield;
}

static GwySpectra*
rhkspm32_read_spectra(RHKPage *rhkpage)
{
    guint i, j;
    gdouble *data;
    GwySIUnit *siunit = NULL;
    GwyDataLine *dline;
    GwySpectra *spectra = NULL;
    GPtrArray *spectrum = NULL;
    // i'm leaving this alone, though it probably doesn't make sense,
    // and i should just create graphs straight away - but in case of
    // future use, i'll just convert the data later to graphs

    // xres stores number of data points per spectra,
    // yres stores the number of spectra

    // reading data
    gwy_debug("rhk-spm32: %d spectra in this page\n", rhkpage->yres);
    for (i = 0; i < rhkpage->yres; i++) {
        dline = gwy_data_line_new(rhkpage->xres, rhkpage->x.scale, FALSE);
        gwy_data_line_set_offset(dline, (rhkpage->x.offset));
        data = gwy_data_line_get_data(dline);
        // store line data in physical units - which are the z values, not y
        if ((rhkpage->data_type) == RHK_DATA_INT16) {
                const guint16 *p = (const guint16*)(rhkpage->buffer
                                                    + rhkpage->data_offset);
                for (j = 0; j < rhkpage->xres; j++) {
                    data[j] = GINT16_FROM_LE(p[i*(rhkpage->xres) + j])
                            *(rhkpage->z.scale)+(rhkpage->z.offset);
                }
        }
        else if ((rhkpage->data_type) == RHK_DATA_SINGLE) {
                const guchar *p = (const guchar*)(rhkpage->buffer
                                                  + rhkpage->data_offset);
                for (j = 0; j < rhkpage->xres; j++) {
                    data[j] = gwy_get_gfloat_le(&p)*rhkpage->z.scale
                              + rhkpage->z.offset;
                }
        }
        siunit = gwy_si_unit_new(rhkpage->x.units);
        gwy_data_line_set_si_unit_x(dline, siunit);
        g_object_unref(siunit);

        // the y units (and data) for a 1D graph are stored in Z in the rhk
        // spm32 format!
        /* Fix "/\xfbHz" to "/Hz".
         * XXX: It might be still wrong as the strange character might mean
         * sqrt. */
        if (g_str_has_suffix(rhkpage->z.units, "/\xfbHz")) {
            gchar *s = gwy_strkill(g_strdup(rhkpage->z.units), "\xfb");
            siunit = gwy_si_unit_new(s);
            g_free(s);
        }
        else
            siunit = gwy_si_unit_new(rhkpage->z.units);
        gwy_data_line_set_si_unit_y(dline, siunit);
        g_object_unref(siunit);

        if (!spectrum)
            spectrum = g_ptr_array_sized_new(rhkpage->yres);
        g_ptr_array_add(spectrum, dline);
    }
    gwy_debug("rhk-spm32: finished parsing sps data\n");
    spectra = gwy_spectra_new();

    for (i = 0; i < rhkpage->yres; i++) {
        dline = g_ptr_array_index(spectrum, i);
        // since RHK spm32 does not record where it took the spectra,
        // i'm setting these to zero
        gwy_spectra_add_spectrum(spectra, dline, 0, 0);
        g_object_unref(dline);
    }
    gwy_spectra_set_title(spectra, rhkpage->label);

    if (spectrum)
        g_ptr_array_free(spectrum, TRUE);

    return spectra;
}

static GwyGraphModel*
spectra_to_graph(GwySpectra *spectra)
{
    GwyGraphModel *gmodel;
    const gchar* graph_title;
    GwyGraphCurveModel *cmodel;
    gchar *curve_title = NULL;
    guint j, k, n_spectra, n_points;
    GwyDataLine *dline;
    gdouble *data, *xdata, *ydata, x_offset, x_realsize;
    GwySIUnit *x_si_unit, *y_si_unit;

    if (!(n_spectra = gwy_spectra_get_n_spectra(spectra))) {
        gwy_debug("rhk-spm32: no spectra in rhkpage - something is odd\n");
        return NULL;
    }
    dline = gwy_spectra_get_spectrum(spectra, 0);
    n_points = gwy_data_line_get_res(dline);
    x_si_unit = gwy_data_line_get_si_unit_x(dline);
    y_si_unit = gwy_data_line_get_si_unit_y(dline);
    xdata = g_new0(gdouble, n_points);
    ydata = g_new0(gdouble, n_points);
    x_offset = gwy_data_line_get_offset(dline);
    x_realsize = gwy_data_line_get_real(dline);
    for (j = 0; j < n_points; j++)
        xdata[j] = x_offset+j*x_realsize;
    gmodel = gwy_graph_model_new();
    g_object_set(gmodel, "si-unit-x", x_si_unit, "si-unit-y", y_si_unit, NULL);
    graph_title = gwy_spectra_get_title(spectra);
    g_object_set(gmodel, "title", graph_title, NULL);
    // tends to obstruct the curves - if there are more than a few - not
    // good - makes it hard to grab curves?
    //g_object_set(gmodel, "label-visible", FALSE, NULL);
    for (k = 1; k <= n_spectra; k++) {
        dline = gwy_spectra_get_spectrum(spectra, k-1);
        data = gwy_data_line_get_data(dline);
        for (j = 0; j < n_points; j++)
            ydata[j] = data[j];
        cmodel = gwy_graph_curve_model_new();
        gwy_graph_model_add_curve(gmodel, cmodel);
        g_object_unref(cmodel);
        curve_title = g_strdup_printf("%s %d", graph_title, k);
        g_object_set(cmodel, "description", curve_title,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", gwy_graph_get_preset_color(k),
                     NULL);
        gwy_graph_curve_model_set_data(cmodel, xdata, ydata, n_points);
    }
    g_free(ydata);
    g_free(xdata);

    return gmodel;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

