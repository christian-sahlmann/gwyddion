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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#include <string.h>
#include <stdlib.h>

#include "err.h"

#define HEADER_SIZE 512

#define MAGIC "STiMage 3.1"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)
#define EXTENSION ".sm2"

typedef enum {
    RHK_TYPE_IMAGE =            0,
    RHK_TYPE_LINE =             1,
    RHK_TYPE_ANNOTATED_LINE =   3
} RHKType;

typedef enum {
    RHK_DATA_SIGNLE    = 0,
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
    RHKRange iv;
    guint scan;
    gdouble period;
    guint id;
    guint data_offset;
    gchar *label;
    gchar *comment;

    const guchar *buffer;
} RHKPage;

static gboolean      module_register         (const gchar *name);
static gint          rhkspm32_detect         (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* rhkspm32_load           (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static gboolean      rhkspm32_read_header    (RHKPage *rhkpage,
                                              GError **error);
static gboolean      rhkspm32_read_range     (const gchar *buffer,
                                              const gchar *name,
                                              RHKRange *range);
static void          rhkspm32_free           (RHKPage *rhkpage);
static void          rhkspm32_store_metadata (RHKPage *rhkpage,
                                              GwyContainer *container);
static GwyDataField* rhkspm32_read_data      (RHKPage *rhkpage);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports RHK Technology SPM32 data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
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
        && memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE) == 0)
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
    GwyContainer *container = NULL;
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

    rhkfile = g_array_new(FALSE, TRUE, sizeof(RHKPage));
    totalpos = 0;

    while (totalpos < size) {
        g_array_set_size(rhkfile, rhkfile->len + 1);
        rhkpage = &g_array_index(rhkfile, RHKPage, rhkfile->len - 1);
        rhkpage->buffer = buffer + totalpos;
        if (!rhkspm32_read_header(rhkpage, &err)) {
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
    key = g_string_new("");
    for (i = 0; i < rhkfile->len; i++) {
        rhkpage = &g_array_index(rhkfile, RHKPage, i);
        dfield = rhkspm32_read_data(rhkpage);
        g_string_printf(key, "/%d/data", i);
        gwy_container_set_object_by_name(container, key->str, dfield);
        g_object_unref(dfield);
        g_string_append(key, "/title");
        gwy_container_set_string_by_name(container, key->str,
                                         g_strdup(rhkpage->label));
        /* FIXME: not yet
        rhkspm32_store_metadata(rhkpage, container); */
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
               (gint*)&rhkpage->page_type) != 7
        || rhkpage->xres <= 0 || rhkpage->yres <= 0) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Invalid file header."));
        return FALSE;
    }
    gwy_debug("type = %u, data = %u, line = %u, image = %u",
              rhkpage->type, rhkpage->data_type, rhkpage->line_type,
              rhkpage->page_type);
    gwy_debug("xres = %d, yres = %d", rhkpage->xres, rhkpage->yres);

    if (rhkpage->type != RHK_TYPE_IMAGE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Non-image files are not supported."));
        return FALSE;
    }
    /* FIXME */
    if (rhkpage->data_type != RHK_DATA_INT16) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unsupported or invalid data type %d."),
                    rhkpage->data_type);
        return FALSE;
    }
    rhkpage->item_size = 2;

    if (!rhkspm32_read_range(buffer + 0x40, "X", &rhkpage->x)
        || !rhkspm32_read_range(buffer + 0x60, "Y", &rhkpage->y)
        || !rhkspm32_read_range(buffer + 0x80, "Z", &rhkpage->z)) {
        err_INVALID(error, _("data ranges"));
        return FALSE;
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
    rhkpage->alpha = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos) {
        err_INVALID(error, "alpha");
        return FALSE;
    }

    if (!rhkspm32_read_range(buffer + 0xc0, "IV", &rhkpage->iv)) {
        err_INVALID(error, "IV");
        return FALSE;
    }

    if (!g_str_has_prefix(buffer + 0xe0, "scan "))
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
        err_INVALID(error, "id");
        return FALSE;
    }
    gwy_debug("data_offset = %u", rhkpage->data_offset);
    if (rhkpage->data_offset < HEADER_SIZE) {
        err_INVALID(error, _("data offset"));
        return FALSE;
    }

    rhkpage->label = g_strstrip(g_strndup(buffer + 0x140, 0x20));
    rhkpage->comment = g_strstrip(g_strndup(buffer + 0x160,
                                           HEADER_SIZE - 0x160));

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

    range->scale = fabs(g_ascii_strtod(buffer + pos, &end));
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

static void
rhkspm32_store_metadata(RHKPage *rhkpage,
                        GwyContainer *container)
{
    const GwyEnum page_types[] = {
        { "Topographic",              RHK_IMAGE_TOPOGAPHIC               },
        { "Current",                  RHK_IMAGE_CURRENT                  },
        { "Aux",                      RHK_IMAGE_AUX                      },
        { "Force",                    RHK_IMAGE_FORCE                    },
        { "Signal",                   RHK_IMAGE_SIGNAL                   },
        { "FFT transform",            RHK_IMAGE_FFT                      },
        { "Noise power spectrum",     RHK_IMAGE_NOISE_POWER_SPECTRUM     },
        { "Line test",                RHK_IMAGE_LINE_TEST                },
        { "Oscilloscope",             RHK_IMAGE_OSCILLOSCOPE             },
        { "IV spectra",               RHK_IMAGE_IV_SPECTRA               },
        { "Image IV 4x4",             RHK_IMAGE_IV_4x4                   },
        { "Image IV 8x8",             RHK_IMAGE_IV_8x8                   },
        { "Image IV 16x16",           RHK_IMAGE_IV_16x16                 },
        { "Image IV 32x32",           RHK_IMAGE_IV_32x32                 },
        { "Image IV Center",          RHK_IMAGE_IV_CENTER                },
        { "Interactive spectra",      RHK_IMAGE_INTERACTIVE_SPECTRA      },
        { "Autocorrelation",          RHK_IMAGE_AUTOCORRELATION          },
        { "IZ spectra",               RHK_IMAGE_IZ_SPECTRA               },
        { "4 gain topography",        RHK_IMAGE_4_GAIN_TOPOGRAPHY        },
        { "8 gain topography",        RHK_IMAGE_8_GAIN_TOPOGRAPHY        },
        { "4 gain current",           RHK_IMAGE_4_GAIN_CURRENT           },
        { "8 gain current",           RHK_IMAGE_8_GAIN_CURRENT           },
        { "Image IV 64x64",           RHK_IMAGE_IV_64x64                 },
        { "Autocorrelation spectrum", RHK_IMAGE_AUTOCORRELATION_SPECTRUM },
        { "Counter data",             RHK_IMAGE_COUNTER                  },
        { "Multichannel analyser",    RHK_IMAGE_MULTICHANNEL_ANALYSER    },
        { "AFM using AFM-100",        RHK_IMAGE_AFM_100                  },
    };
    const gchar *s;

    gwy_container_set_string_by_name(container, "/meta/Tunneling voltage",
                                     g_strdup_printf("%g mV",
                                                     1e3*rhkpage->iv.offset));
    gwy_container_set_string_by_name(container, "/meta/Current",
                                     g_strdup_printf("%g nA",
                                                     1e9*rhkpage->iv.scale));
    gwy_container_set_string_by_name(container, "/meta/Id",
                                     g_strdup_printf("%u", rhkpage->id));
    if (rhkpage->date && *rhkpage->date)
        gwy_container_set_string_by_name(container, "/meta/Date",
                                         g_strdup(rhkpage->date));
    if (rhkpage->comment && *rhkpage->comment)
        gwy_container_set_string_by_name(container, "/meta/Comment",
                                         g_strdup(rhkpage->comment));
    if (rhkpage->label && *rhkpage->label) {
        gwy_container_set_string_by_name(container, "/meta/Label",
                                         g_strdup(rhkpage->label));
    }

    s = gwy_enum_to_string(rhkpage->page_type,
                           page_types, G_N_ELEMENTS(page_types));
    if (s && *s)
        gwy_container_set_string_by_name(container, "/meta/Image type",
                                         g_strdup(s));
}

static GwyDataField*
rhkspm32_read_data(RHKPage *rhkpage)
{
    GwyDataField *dfield;
    const guint16 *p;
    GwySIUnit *siunit;
    gdouble *data;
    guint i;

    p = (const guint16*)(rhkpage->buffer + rhkpage->data_offset);
    dfield = gwy_data_field_new(rhkpage->xres, rhkpage->yres,
                                rhkpage->xres * rhkpage->x.scale,
                                rhkpage->yres * rhkpage->y.scale,
                                FALSE);

    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < rhkpage->xres*rhkpage->yres; i++)
        data[i] = GINT16_FROM_LE(p[i]);

    gwy_data_field_multiply(dfield, rhkpage->z.scale);

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_unit_string(siunit, rhkpage->x.units);

    siunit = gwy_data_field_get_si_unit_z(dfield);
    gwy_si_unit_set_unit_string(siunit, rhkpage->z.units);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

