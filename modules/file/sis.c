/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DEBUG 1

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define MAGIC "SIS&STB  SIScan"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".sis"

typedef enum {
    SIS_BLOCK_DOCUMENT     = 1,
    SIS_BLOCK_PREVIEW      = 2,
    SIS_BLOCK_CHANNEL      = 3,
    SIS_BLOCK_IMAGE        = 42,
} SISBlockType;

typedef enum {
    SIS_AQUISITION_MODE_CONTACT     = 1,
    SIS_AQUISITION_MODE_NON_CONTACT = 2,
} SISAquisitionMode;

typedef enum {
    SIS_SCANNING_DIRECTION_FORWARD  = 1,
    SIS_SCANNING_DIRECTION_BACKWARD = 2,
} SISScanningDirection;

typedef enum {
    SIS_OFF = FALSE,
    SIS_ON  = TRUE,
} SISOnOff;

typedef enum {
    SIS_PALETTE_GRAY    = 0,
    SIS_PALETTE_GLOW    = 1,
    SIS_PALETTE_RED     = 2,
    SIS_PALETTE_GREEN   = 3,
    SIS_PALETTE_BLUE    = 4,
    SIS_PALETTE_RAINBOW = 5,
} SISPaletteType;

typedef enum {
    SIS_DATA_TYPE_TOPOGRAPHY       = 1,
    SIS_DATA_TYPE_FIELD_CONTRAST   = 2,
    SIS_DATA_TYPE_ERROR            = 3,
    SIS_DATA_TYPE_EXTERM           = 4,
    SIS_DATA_TYPE_LOC              = 5,
    SIS_DATA_TYPE_PHASE            = 6,
    SIS_DATA_TYPE_CAPACITY         = 7,
    SIS_DATA_TYPE_AMPLITUDE        = 8,
    SIS_DATA_TYPE_FREQUENCY        = 9,
    SIS_DATA_TYPE_POTENTIAL        = 10,
    SIS_DATA_TYPE_FRICTION         = 11,
    SIS_DATA_TYPE_FORCE_MODULATION = 12,
    SIS_DATA_TYPE_USER             = 13,
} SISDataType;

typedef enum {
    SIS_SIGNAL_SOURCE_FEEDBACK         = 1,
    SIS_SIGNAL_SOURCE_ZSENSOR          = 2,
    SIS_SIGNAL_SOURCE_INTERFEROMETER   = 3,
    SIS_SIGNAL_SOURCE_FIELD            = 4,
    SIS_SIGNAL_SOURCE_NC_AMPLITUDE     = 5,
    SIS_SIGNAL_SOURCE_NC_PHASE         = 6,
    SIS_SIGNAL_SOURCE_FM_FREQUENCY     = 7,
    SIS_SIGNAL_SOURCE_LOC_AMPLITUDE    = 8,
    SIS_SIGNAL_SOURCE_LOC_PHASE        = 9,
    SIS_SIGNAL_SOURCE_PM_CHANNEL_1     = 10,
    SIS_SIGNAL_SOURCE_PM_CHANNEL_2     = 11,
    SIS_SIGNAL_SOURCE_PM_FEEDBACK      = 12,
    SIS_SIGNAL_SOURCE_CAPACITY         = 13,
    SIS_SIGNAL_SOURCE_LOC_SW_AMPLITUDE = 14,
    SIS_SIGNAL_SOURCE_LOC_SW_PHASE     = 15,
    SIS_SIGNAL_SOURCE_USER             = 16,
} SISSignalSource;

typedef struct {
    guint idx;
    GType type;
    const gchar *meta;
    const gchar *units;
} SISParameter;

typedef struct {
    SISDataType data_type;
    SISSignalSource signal_source;
    SISScanningDirection direction;
    gsize processing_steps;
    /* images */
} SISChannel;

typedef struct {
    gsize version_maj;
    gsize version_min;
    GHashTable *params;
    gsize nchannels;
    SISChannel *channels;
} SISFile;

static const GwyEnum sis_onoff[] = {
    { "Off", SIS_OFF },
    { "On",  SIS_ON },
};

/* Map between SIS palettes and Gwyddion palettes */
static const GwyEnum sis_palettes[] = {
    { "Gray",    SIS_PALETTE_GRAY    },
    { "Warm",    SIS_PALETTE_GLOW    },
    { "Red",     SIS_PALETTE_RED     },
    { "Green",   SIS_PALETTE_GREEN   },
    { "Blue",    SIS_PALETTE_BLUE    },
    { "Rainbow", SIS_PALETTE_RAINBOW },
};

static const GwyEnum sis_data_types[] = {
    { "Topography",             SIS_DATA_TYPE_TOPOGRAPHY       },
    { "Field Contrast",         SIS_DATA_TYPE_FIELD_CONTRAST   },
    { "Error",                  SIS_DATA_TYPE_ERROR            },
    { "Exterm",                 SIS_DATA_TYPE_EXTERM           },
    { "Loc",                    SIS_DATA_TYPE_LOC              },
    { "Phase",                  SIS_DATA_TYPE_PHASE            },
    { "Capacity",               SIS_DATA_TYPE_CAPACITY         },
    { "Amplitude",              SIS_DATA_TYPE_AMPLITUDE        },
    { "Frequency",              SIS_DATA_TYPE_FREQUENCY        },
    { "Potential",              SIS_DATA_TYPE_POTENTIAL        },
    { "Friction",               SIS_DATA_TYPE_FRICTION         },
    { "Force Modulation (FMM)", SIS_DATA_TYPE_FORCE_MODULATION },
    { "User",                   SIS_DATA_TYPE_USER             },
};

static const GwyEnum sis_signal_sources[] = {
    { "Feedback",               SIS_SIGNAL_SOURCE_FEEDBACK         },
    { "ZSensor",                SIS_SIGNAL_SOURCE_ZSENSOR          },
    { "Interferometer",         SIS_SIGNAL_SOURCE_INTERFEROMETER   },
    { "Field",                  SIS_SIGNAL_SOURCE_FIELD            },
    { "NC Amplitude",           SIS_SIGNAL_SOURCE_NC_AMPLITUDE     },
    { "NC Phase",               SIS_SIGNAL_SOURCE_NC_PHASE         },
    { "FM Frequency",           SIS_SIGNAL_SOURCE_FM_FREQUENCY     },
    { "LOC amplitude",          SIS_SIGNAL_SOURCE_LOC_AMPLITUDE    },
    { "LOC phase",              SIS_SIGNAL_SOURCE_LOC_PHASE        },
    { "PM Channel 1",           SIS_SIGNAL_SOURCE_PM_CHANNEL_1     },
    { "PM Channel 2",           SIS_SIGNAL_SOURCE_PM_CHANNEL_2     },
    { "PM Feedback",            SIS_SIGNAL_SOURCE_PM_FEEDBACK      },
    { "Capacity",               SIS_SIGNAL_SOURCE_CAPACITY         },
    { "LOC Software Amplitude", SIS_SIGNAL_SOURCE_LOC_SW_AMPLITUDE },
    { "LOC Software Phase",     SIS_SIGNAL_SOURCE_LOC_SW_PHASE     },
    { "User",                   SIS_SIGNAL_SOURCE_USER             },
};

static const SISParameter sis_parameters[] = {
    {   0, G_TYPE_STRING, "Name of the sample", NULL },
    {   1, G_TYPE_STRING, "Comment of the sample", NULL },
    {   2, G_TYPE_DOUBLE, "Scanning range in x direction", "nm" },
    {   3, G_TYPE_DOUBLE, "Scanning range in y direction", "nm" },
    {   4, G_TYPE_DOUBLE, "Range in z direction", "nm" },
    {   5, G_TYPE_DOUBLE, "Offset in z direction", NULL },  /* ?? */
    {   6, G_TYPE_INT,    "Type of aquisition", NULL },
    {   7, G_TYPE_INT,    "Number of pixels in x direction", NULL },
    {   8, G_TYPE_INT,    "Number of pixels in y direction", NULL },
    {   9, G_TYPE_DOUBLE, "Speed of scanning", "lines/s" },
    {  10, G_TYPE_STRING, "Type of tip", NULL },
    {  11, G_TYPE_INT,    "Bits per pixels", NULL },
    {  12, G_TYPE_DOUBLE, "Value of the proportional part of feedback", NULL },
    {  13, G_TYPE_DOUBLE, "Value of the integral part of feedback", "µs" },
    {  14, G_TYPE_DOUBLE, "Load force of the tip", "nN" },
    {  15, G_TYPE_DOUBLE, "Resonance frequency of the cantilever", "kHz" },
    {  16, G_TYPE_STRING, "Date of the measurement", NULL },
    {  17, G_TYPE_DOUBLE, "Feedback", NULL },
    {  18, G_TYPE_DOUBLE, "Scanning direction", "°" },
    {  19, G_TYPE_DOUBLE, "Spring constant", "N/m" },
    {  20, G_TYPE_STRING, "HighVoltage in x and y direction", NULL },
    {  21, G_TYPE_STRING, "Measurement with x and y linearisation", NULL },
    {  22, G_TYPE_STRING, "Amplification of the interferometer signal", NULL },  /* ?? */
    {  23, G_TYPE_DOUBLE, "Free amplitude of the cantilever", "nm" },
    {  24, G_TYPE_DOUBLE, "Damping of the free amplitude of the cantilever during the measurement", "%" },
    {  25, G_TYPE_DOUBLE, "Voltage between the tip and the electrode under the sample", "V" },
    {  26, G_TYPE_DOUBLE, "Oscilation frequency of the cantilever during the measurement", "kHz" },
    {  27, G_TYPE_DOUBLE, "Field contrast", "nm" },
    {  28, G_TYPE_INT,    "Type of palette", NULL },
    { 100, G_TYPE_STRING, "Units of data in channel 1", NULL },
    { 101, G_TYPE_STRING, "Units of data in channel 2", NULL },
    { 102, G_TYPE_STRING, "Units of data in channel 3", NULL },
    { 103, G_TYPE_STRING, "Units of data in channel 4", NULL },
    { 104, G_TYPE_STRING, "Units of data in channel 5", NULL },
    { 105, G_TYPE_STRING, "Units of data in channel 6", NULL },
    { 106, G_TYPE_STRING, "Units of data in channel 7", NULL },
    { 107, G_TYPE_STRING, "Units of data in channel 8", NULL },
    { 108, G_TYPE_DOUBLE, "Range of of data in channel 1", NULL },
    { 109, G_TYPE_DOUBLE, "Range of of data in channel 2", NULL },
    { 110, G_TYPE_DOUBLE, "Range of of data in channel 3", NULL },
    { 111, G_TYPE_DOUBLE, "Range of of data in channel 4", NULL },
    { 112, G_TYPE_DOUBLE, "Range of of data in channel 5", NULL },
    { 113, G_TYPE_DOUBLE, "Range of of data in channel 6", NULL },
    { 114, G_TYPE_DOUBLE, "Range of of data in channel 7", NULL },
    { 115, G_TYPE_DOUBLE, "Range of of data in channel 8", NULL },
    { 116, G_TYPE_INT,    "Number of channels", NULL },
    { 117, G_TYPE_DOUBLE, "Offset in x direction in the scanning range", "nm" },
    { 118, G_TYPE_DOUBLE, "Offset in y direction in the scanning range", "nm" },
    { 119, G_TYPE_DOUBLE, "Maximum scanning range in x direction", "nm" },
    { 120, G_TYPE_DOUBLE, "Maximum scanning range in y direction", "nm" },
    { 121, G_TYPE_DOUBLE, "Minimum range of of data in channel 1", NULL },
    { 122, G_TYPE_DOUBLE, "Minimum range of of data in channel 2", NULL },
    { 123, G_TYPE_DOUBLE, "Minimum range of of data in channel 3", NULL },
    { 124, G_TYPE_DOUBLE, "Minimum range of of data in channel 4", NULL },
    { 125, G_TYPE_DOUBLE, "Minimum range of of data in channel 5", NULL },
    { 126, G_TYPE_DOUBLE, "Minimum range of of data in channel 6", NULL },
    { 127, G_TYPE_DOUBLE, "Minimum range of of data in channel 7", NULL },
    { 128, G_TYPE_DOUBLE, "Minimum range of of data in channel 8", NULL },
    { 129, G_TYPE_DOUBLE, "Maximum range of of data in channel 1", NULL },
    { 130, G_TYPE_DOUBLE, "Maximum range of of data in channel 2", NULL },
    { 131, G_TYPE_DOUBLE, "Maximum range of of data in channel 3", NULL },
    { 132, G_TYPE_DOUBLE, "Maximum range of of data in channel 4", NULL },
    { 133, G_TYPE_DOUBLE, "Maximum range of of data in channel 5", NULL },
    { 134, G_TYPE_DOUBLE, "Maximum range of of data in channel 6", NULL },
    { 135, G_TYPE_DOUBLE, "Maximum range of of data in channel 7", NULL },
    { 136, G_TYPE_DOUBLE, "Maximum range of of data in channel 8", NULL },
    { 137, G_TYPE_STRING, "Name of data in channel 1", NULL },
    { 138, G_TYPE_STRING, "Name of data in channel 2", NULL },
    { 139, G_TYPE_STRING, "Name of data in channel 3", NULL },
    { 140, G_TYPE_STRING, "Name of data in channel 4", NULL },
    { 141, G_TYPE_STRING, "Name of data in channel 5", NULL },
    { 142, G_TYPE_STRING, "Name of data in channel 6", NULL },
    { 143, G_TYPE_STRING, "Name of data in channel 7", NULL },
    { 144, G_TYPE_STRING, "Name of data in channel 8", NULL },
};

typedef struct {
    GHashTable *hash;
    GwyDataField *data_field;
} SISData;

static gboolean       module_register     (const gchar *name);
static gint           sis_detect          (const gchar *filename,
                                           gboolean only_name);
static GwyContainer*  sis_load            (const gchar *filename);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "sisfile",
    "Load SIS data.",
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Neačs (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo sis_func_info = {
        "sisfile",
        "SIS files (" EXTENSION ")",
        (GwyFileDetectFunc)&sis_detect,
        (GwyFileLoadFunc)&sis_load,
        NULL,
    };

    gwy_file_func_register(name, &sis_func_info);

    return TRUE;
}

static gint
sis_detect(const gchar *filename,
           gboolean only_name)
{
    FILE *fh;
    gchar magic[MAGIC_SIZE];
    gint score;

    gwy_debug("");
    if (only_name)
        return g_str_has_suffix(filename, EXTENSION) ? 20 : 0;

    if (!(fh = fopen(filename, "rb")))
        return 0;
    score = 0;
    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && memcmp(magic, MAGIC, MAGIC_SIZE) == 0)
        score = 100;
    fclose(fh);

    return score;
}

static inline gsize
get_WORD(const guchar **p)
{
    gsize z = (gsize)(*p)[0] + ((gsize)(*p)[1] << 8);
    *p += 2;
    return z;
}

static inline gsize
get_DWORD(const guchar **p)
{
    gsize z = (gsize)(*p)[0] + ((gsize)(*p)[1] << 8)
              + ((gsize)(*p)[2] << 16) + ((gsize)(*p)[3] << 24);
    *p += 4;
    return z;
}

static inline gsize
get_DOUBLE(const guchar **p)
{
    union { guchar pp[8]; double d; } z;
    /* FIXME: assumes Intel endian */
    memcpy(z.pp, *p, sizeof(double));
    *p += sizeof(double);
    return z.d;
}

static gboolean
sis_real_load(const guchar *buffer,
              gsize size,
              SISFile *sisfile)
{
    gsize start, id, i, j, len;
    gsize docinfosize, nparams;
    const guchar *p;
    gpointer idp;
    gdouble d;

    p = memchr(buffer, '\x1a', size);
    if (!p)
        return FALSE;
    start = p-buffer + 1;
    gwy_debug("%.*s", start, buffer);

    if (size - start < 6)
        return FALSE;

    p = buffer + start;
    id = get_WORD(&p);
    gwy_debug("block id = %u", id);
    if (id != SIS_BLOCK_DOCUMENT)
        return FALSE;

    docinfosize = get_DWORD(&p);
    gwy_debug("doc info size = %u", docinfosize);
    if (size - (p - buffer) < docinfosize - 6
        || docinfosize < 8)
        return FALSE;

    sisfile->version_maj = get_WORD(&p);
    sisfile->version_min = get_WORD(&p);
    gwy_debug("version = %d.%d", sisfile->version_maj, sisfile->version_min);

    nparams = get_WORD(&p);
    sisfile->nchannels = get_WORD(&p);
    gwy_debug("nparams = %d, nchannels = %d", nparams, sisfile->nchannels);

    for (i = 0; i < nparams; i++) {
        const SISParameter *sisparam;

        if (size - (p - buffer) < 4)
            return FALSE;
        id = get_WORD(&p);
        len = get_WORD(&p);
        if (!len || size - (p - buffer) < len)
            return FALSE;

        sisparam = NULL;
        for (j = 0; j < G_N_ELEMENTS(sis_parameters); j++) {
            if (sis_parameters[j].idx == id) {
                gwy_debug("Parameter %s", sis_parameters[j].meta);
                sisparam = sis_parameters + j;
                break;
            }
        }
        if (!sisparam) {
            g_warning("Unknown parameter id %u", id);
            p += len;
            continue;
        }

        idp = GUINT_TO_POINTER(id);
        switch (sisparam->type) {
            case G_TYPE_STRING:
            g_hash_table_insert(sisfile->params, idp, g_strndup(p, len));
            gwy_debug("Value = %s", g_hash_table_lookup(sisfile->params, idp));
            p += len;
            break;

            case G_TYPE_INT:
            g_assert(len == 2);
            j = get_WORD(&p);
            g_hash_table_insert(sisfile->params, idp, g_memdup(&j, sizeof(j)));
            gwy_debug("Value = %u", j);
            break;

            case G_TYPE_DOUBLE:
            g_assert(len == sizeof(double));
            d = get_DOUBLE(&p);
            g_hash_table_insert(sisfile->params, idp, g_memdup(&d, sizeof(d)));
            gwy_debug("Value = %g", d);
            break;

            default:
            g_assert_not_reached();
            p += len;
            break;
        }
    }

    sisfile->channels = g_new(SISChannel, sisfile->nchannels);
    for (i = 0; i <= sisfile->nchannels; ) {
        gwy_debug("%06x", p - buffer);
        if (size - (p - buffer) < 6)
            return FALSE;

        id = get_WORD(&p);
        len = get_DWORD(&p);
        gwy_debug("id = %u, len = %u", id, len);
        if (!len || size - (p - buffer) < len)
            return FALSE;

        switch (id) {
            case SIS_BLOCK_PREVIEW:
            gwy_debug("Preview");
            p += len;
            break;

            case SIS_BLOCK_IMAGE:
            j++;
            gwy_debug("Image #%u of channel %u", j, i);
            /* XXX: This is unreliable bogus, some data files have samples
             * instead of bytes here... */
            p += len;
            break;

            case SIS_BLOCK_CHANNEL:
            i++;
            j = 0;
            gwy_debug("Channel %u", i);
            p += len;
            break;

            default:
            g_warning("Funny stuff");
            p += len;
            break;
        }
    }

    return FALSE;
}

static GwyContainer*
sis_load(const gchar *filename)
{
    guchar *buffer;
    gsize size;
    GError *err = NULL;
    GwyContainer *data = NULL;
    SISFile sisfile;

    gwy_debug("");
    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_clear_error(&err);
        return NULL;
    }
    memset(&sisfile, 0, sizeof(sisfile));
    sisfile.params = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                           NULL, g_free);
    sis_real_load(buffer, size, &sisfile);
    gwy_file_abandon_contents(buffer, size, NULL);
    g_hash_table_destroy(sisfile.params);
    g_free(sisfile.channels);

    return data;
}

#if 0
static GwyContainer*
sis_load(const gchar *filename)
{
    GObject *object = NULL;
    GError *err = NULL;
    gchar *buffer = NULL;
    gchar *p;
    gsize size = 0;
    SISFileType file_type;
    SISData *ndata;
    GHashTable *hash;
    GList *l, *list = NULL;
    /* FIXME defaults */
    gdouble zscale = 9.583688e-9;
    gdouble curscale = 10.0e-9;
    gboolean ok;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    file_type = NANOSCOPE_FILE_TYPE_NONE;
    if (size > MAGIC_SIZE) {
        if (!memcmp(buffer, MAGIC_TXT, MAGIC_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_TXT;
        else if (!memcmp(buffer, MAGIC_BIN, MAGIC_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_BIN;
    }
    if (!file_type) {
        g_warning("File %s doesn't seem to be a nanoscope file", filename);
        g_free(buffer);
        return NULL;
    }
    gwy_debug("File type: %d", file_type);
    /* as already know file_type, fix the first char for hash reading */
    *buffer = '\\';

    p = buffer;
    while ((hash = read_hash(&p))) {
        ndata = g_new0(SISData, 1);
        ndata->hash = hash;
        list = g_list_append(list, ndata);
    }
    ok = TRUE;
    for (l = list; ok && l; l = g_list_next(l)) {
        ndata = (SISData*)l->data;
        hash = ndata->hash;
        if (strcmp(g_hash_table_lookup(hash, "#self"), "Scanner list") == 0) {
            get_value_scales(hash, &zscale, &curscale);
            continue;
        }
        if (strcmp(g_hash_table_lookup(hash, "#self"), "Ciao image list"))
            continue;

        ndata->data_field = hash_to_data_field(hash, file_type,
                                               size, buffer,
                                               zscale, curscale, &p);
        ok = ok && ndata->data_field;
    }

    /* select (let user select) which data to load */
    ndata = ok ? select_which_data(list) : NULL;
    if (ndata) {
        object = gwy_container_new();
        gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                         G_OBJECT(ndata->data_field));
    }

    /* unref all data fields, the container already keeps a reference to the
     * right one */
    g_free(buffer);
    for (l = list; l; l = g_list_next(l)) {
        ndata = (SISData*)l->data;
        g_hash_table_destroy(ndata->hash);
        gwy_object_unref(ndata->data_field);
        g_free(ndata);
    }
    g_list_free(list);

    return (GwyContainer*)object;
}

static void
get_value_scales(GHashTable *hash,
                 gdouble *zscale,
                 gdouble *curscale)
{
    gchar *s, un[6];

    /* z sensitivity */
    if (!(s = g_hash_table_lookup(hash, "@Sens. Zscan"))) {
        g_warning("`@Sens. Zscan' not found");
        return;
    }
    if (s[0] != 'V' || s[1] != ' '
        || sscanf(s+2, "%lf %5s", zscale, un) != 2) {
        g_warning("Cannot parse `@Sens. Zscan': <%s>", s+2);
        return;
    }
    if (strcmp(un, "nm/V") == 0)
        *zscale /= 1e9;
    else if (strcmp(un, "~m/V") == 0 || strcmp(un, "um/V") == 0)
        *zscale /= 1e6;
    else {
        g_warning("Cannot understand z units: <%s>", un);
        *zscale /= 1e9;
    }

    /* current sensitivity */
    if (!(s = g_hash_table_lookup(hash, "@Sens. Current"))) {
        g_warning("`@Sens. Current' not found");
        return;
    }
    if (s[0] != 'V' || s[1] != ' '
        || sscanf(s+2, "%lf %5s", curscale, un) != 2) {
        g_warning("Cannot parse `@Sens. Current': <%s>", s+2);
        return;
    }
    if (strcmp(un, "pA/V") == 0)
        *curscale /= 1e12;
    if (strcmp(un, "nA/V") == 0)
        *curscale /= 1e9;
    else if (strcmp(un, "~A/V") == 0 || strcmp(un, "uA/V") == 0)
        *curscale /= 1e6;
    else {
        g_warning("Cannot understand z units: <%s>", un);
        *curscale /= 1e9;
    }
}

static GwyDataField*
hash_to_data_field(GHashTable *hash,
                   SISFileType file_type,
                   gsize bufsize,
                   gchar *buffer,
                   gdouble zscale,
                   gdouble curscale,
                   gchar **p)
{
    GwyDataField *dfield;
    const gchar *s;
    gchar *t;
    gchar un[5];
    gint xres, yres, bpp, offset, size;
    gdouble xreal, yreal, q, zmagnify = 1.0, zscalesens = 1.0;
    gdouble *data;
    gboolean is_current;  /* assume height if not current */

    if (!(s = g_hash_table_lookup(hash, "@2:Image Data"))) {
        g_warning("No `@2 Image Data' found");
        return NULL;
    }
    is_current = strstr(s, "[Current]") || strstr(s, "\"Current\"");
    gwy_debug("is current = %d", is_current);

    if (!(s = g_hash_table_lookup(hash, "Samps/line"))) {
        g_warning("`Samps/line' not found");
        return NULL;
    }
    xres = atoi(s);

    if (!(s = g_hash_table_lookup(hash, "Number of lines"))) {
        g_warning("`Number of lines' not found");
        return NULL;
    }
    yres = atoi(s);

    if (!(s = g_hash_table_lookup(hash, "Bytes/pixel"))) {
        g_warning("`Bytes/pixel' not found");
        return NULL;
    }
    bpp = atoi(s);

    /* scan size */
    if (!(s = g_hash_table_lookup(hash, "Scan size"))) {
        g_warning("`Scan size' not found");
        return NULL;
    }
    if (sscanf(s, "%lf %lf %4s", &xreal, &yreal, un) != 3) {
        g_warning("Cannot parse `Scan size': <%s>", s);
        return NULL;
    }
    if (strcmp(un, "nm") == 0) {
        xreal /= 1e9;
        yreal /= 1e9;
    }
    else if (strcmp(un, "~m") == 0 || strcmp(un, "um") == 0) {
        xreal /= 1e6;
        yreal /= 1e6;
    }
    else {
        g_warning("Cannot understand size units: <%s>", un);
        xreal /= 1e9;
        yreal /= 1e9;
    }

    offset = size = 0;
    if (file_type == NANOSCOPE_FILE_TYPE_BIN) {
        if (!(s = g_hash_table_lookup(hash, "Data offset"))) {
            g_warning("`Data offset' not found");
            return NULL;
        }
        offset = atoi(s);

        if (!(s = g_hash_table_lookup(hash, "Data length"))) {
            g_warning("`Data length' not found");
            return NULL;
        }
        size = atoi(s);
        if (size != bpp*xres*yres) {
            g_warning("Data size %d != %d bpp*xres*yres", size, bpp*xres*yres);
            return NULL;
        }

        if (offset + size > (gint)bufsize) {
            g_warning("Data don't fit to the file");
            return NULL;
        }
    }

    /* XXX: now ignored */
    if (!(t = g_hash_table_lookup(hash, "@Z magnify")))
        g_warning("`@Z magnify' not found");
    else {
        if (!(s = strchr(t, ']')))
            g_warning("Cannot parse `@Z magnify': <%s>", t);
        else {
            zmagnify = strtod(s+1, &t);
            if (t == s+1) {
                g_warning("Cannot parse `@Z magnify' value: <%s>", s+1);
                zmagnify = 1.0;
            }
        }
    }

    if (!(t = g_hash_table_lookup(hash, "@2:Z scale")))
        g_warning("`@2:Z scale' not found");
    else {
        if (!(s = strchr(t, '(')))
            g_warning("Cannot parse `@2:Z scale': <%s>", t);
        else {
            zscalesens = strtod(s+1, &t);
            if (t == s+1) {
                g_warning("Cannot parse `@2:Z scale' value: <%s>", s+1);
                zscalesens = 1.0;
            }
        }
    }

    dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, xreal, yreal,
                                               FALSE));
    data = gwy_data_field_get_data(dfield);
    switch (file_type) {
        case NANOSCOPE_FILE_TYPE_TXT:
        if (!read_ascii_data(xres*yres, data, p, bpp)) {
            g_object_unref(dfield);
            return NULL;
        }
        break;

        case NANOSCOPE_FILE_TYPE_BIN:
        if (!read_binary_data(xres*yres, data, buffer + offset, bpp)) {
            g_object_unref(dfield);
            return NULL;
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }
    /*q = (is_current ? curscale : zscale) * zscalesens/zmagnify*10;*/
    q = (is_current ? curscale : zscale) * zscalesens*10;
    gwy_debug("curscale = %fe-9, zscale = %fe-9, zscalesens = %f, zmagnify = %f",
              curscale*1e9, zscale*1e9, zscalesens, zmagnify);
    gwy_data_field_multiply(dfield, q);
    gwy_data_field_set_si_unit_xy(dfield, GWY_SI_UNIT(gwy_si_unit_new("m")));
    gwy_data_field_set_si_unit_z(dfield,
                                 GWY_SI_UNIT(gwy_si_unit_new(is_current
                                                             ? "A" : "m")));

    return dfield;
}

static SISData*
select_which_data(GList *list)
{
    SISData *ndata, *ndata0;
    GtkWidget *dialog, *omenu, *menu, *label, *table;
    GObject *item;
    GwyEnum *choices;
    GList *l;
    gint i, count, response;

    count = 0;
    ndata0 = NULL;
    l = NULL;
    while (list) {
        ndata = (SISData*)list->data;
        if (ndata->data_field) {
            count++;
            l = g_list_append(l, ndata);
            if (!ndata0)
                ndata0 = ndata;
        }
        list = g_list_next(list);
    }
    list = l;
    if (count == 0 || count == 1) {
        g_list_free(list);
        return ndata0;
    }

    choices = g_new(GwyEnum, count);
    i = 0;
    for (l = list; l; l = g_list_next(l)) {
        ndata = (SISData*)l->data;
        choices[i].name = g_hash_table_lookup(ndata->hash, "@2:Image Data");
        if (choices[i].name[0] && choices[i].name[1])
            choices[i].name += 2;
        choices[i].value = i;
        i++;
    }

    dialog = gtk_dialog_new_with_buttons(_("Select data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    table = gtk_table_new(2, 1, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    label = gtk_label_new(_("Data to load:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = gwy_option_menu_create(choices, count, "data", NULL, NULL, -1);
    gtk_table_attach(GTK_TABLE(table), omenu, 0, 1, 1, 2,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_free(choices);
            g_list_free(list);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    item = G_OBJECT(gtk_menu_get_active(GTK_MENU(menu)));
    response = GPOINTER_TO_INT(g_object_get_data(item, "data"));
    gtk_widget_destroy(dialog);

    l = g_list_nth(list, response);
    ndata0 = (SISData*)l->data;

    g_free(choices);
    g_list_free(list);

    return ndata0;
}

static gboolean
read_ascii_data(gint n, gdouble *data,
                gchar **buffer,
                gint bpp)
{
    gint i;
    gdouble q;
    gchar *end;
    long l, min, max;

    q = 1.0/(1 << (8*bpp));
    min = 10000000;
    max = -10000000;
    for (i = 0; i < n; i++) {
        /*data[i] = q*strtol(*buffer, &end, 10);*/
        l = strtol(*buffer, &end, 10);
        min = MIN(l, min);
        max = MAX(l, max);
        data[i] = q*l;
        if (end == *buffer) {
            g_warning("Garbage after data sample #%d", i);
            return FALSE;
        }
        *buffer = end;
    }
    gwy_debug("min = %ld, max = %ld", min, max);
    return TRUE;
}

static gboolean
read_binary_data(gint n, gdouble *data,
                 gchar *buffer,
                 gint bpp)
{
    gint i;
    gdouble q;

    q = 1.0/(1 << (8*bpp));
    switch (bpp) {
        case 1:
        for (i = 0; i < n; i++)
            data[i] = q*buffer[i];
        break;

        case 2:
        {
            gint16 *p = (gint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*p[i];
        }
        break;

        case 4:
        {
            gint32 *p = (gint32*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*p[i];
        }
        break;

        default:
        g_warning("bpp = %d unimplemented", bpp);
        return FALSE;
        break;
    }

    return TRUE;
}

static GHashTable*
read_hash(gchar **buffer)
{
    GHashTable *hash;
    gchar *line, *colon;

    line = next_line(buffer);
    if (line[0] != '\\' || line[1] != '*')
        return NULL;
    if (!strcmp(line, "\\*File list end")) {
        gwy_debug("FILE LIST END");
        return NULL;
    }

    hash = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(hash, "#self", line + 2);    /* self */
    gwy_debug("hash table <%s>", line + 2);
    while ((*buffer)[0] == '\\' && (*buffer)[1] && (*buffer)[1] != '*') {
        line = next_line(buffer) + 1;
        if (!line || !line[0] || !line[1] || !line[2])
            goto fail;
        colon = strchr(line + 3, ':');
        if (!colon || !isspace(colon[1]))
            goto fail;
        *colon = '\0';
        g_hash_table_insert(hash, line, colon + 2);
        gwy_debug("<%s>: <%s>", line, colon + 2);
    }

    return hash;

fail:
    g_hash_table_destroy(hash);
    return NULL;
}


/**
 * next_line:
 * @buffer: A character buffer containing some text.
 *
 * Extracts a next line from @buffer.
 *
 * @buffer is updated to point after the end of the line and the "\n" 
 * (or "\r\n") is replaced with "\0", if present.
 *
 * Returns: The start of the line.  %NULL if the buffer is empty or %NULL.
 *          The line is not duplicated, the returned pointer points somewhere
 *          to @buffer.
 **/
static gchar*
next_line(gchar **buffer)
{
    gchar *p, *q;

    if (!buffer || !*buffer)
        return NULL;

    q = *buffer;
    p = strchr(*buffer, '\n');
    if (p) {
        if (p > *buffer && *(p-1) == '\r')
            *(p-1) = '\0';
        *buffer = p+1;
        *p = '\0';
    }
    else
        *buffer = NULL;

    return q;
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

