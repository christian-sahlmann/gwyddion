/*
 *  @(#) $Id$
 *  Copyright (C) 2004 Rok Zitko.
 *  E-mail: rok.zitko@ijs.si.
 *  Copyright (C) 2009,2012 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  Based on nanoscope.c, Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

/* TODO:
 * - multiple images
 * - constant height or current
 * - saving
*/

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-createc-spm">
 *   <comment>Createc SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="[Parameter]"/>
 *     <match type="string" offset="0" value="[Paramco32]"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Createc DAT
 * .dat
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/filters.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define Angstrom (1e-10)

enum {
    HEADER_SIZE = 16384,
};

typedef enum {
    CREATEC_NONE,
    CREATEC_1,
    CREATEC_2,
    CREATEC_2Z,
} CreatecVersion;

typedef enum {
    CHANNEL_TOPOGRAPHY = 1,
    CHANNEL_CURRENT = 2,
} CreatecChannelFlags;

static gboolean       module_register       (void);
static gint           createc_detect        (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer*  createc_load          (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static CreatecVersion createc_get_version   (const gchar *buffer,
                                             gsize size);
static GwyDataField*  hash_to_data_field    (GHashTable *hash,
                                             gint version,
                                             guint channelbit,
                                             const gchar *buffer,
                                             gsize size,
                                             gsize *offset,
                                             GError **error);
static gsize          first_channel_offset  (gint version);
static guint          channel_bpp           (gint version);
static void           read_binary_data      (gint n,
                                             gdouble *data,
                                             const gchar *buffer,
                                             gint bpp);
static GwyContainer*  createc_get_metadata  (GHashTable *hash);
static gchar*         unpack_compressed_data(const guchar *buffer,
                                             gsize size,
                                             gsize imagesize,
                                             gsize *datasize,
                                             GError **error);

static const GwyEnum versions[] = {
    { "[Parameter]", CREATEC_1,  },
    { "[Paramet32]", CREATEC_2,  },
    { "[Paramco32]", CREATEC_2Z, },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Createc data files."),
    "Rok Zitko <rok.zitko@ijs.si>",
    "0.12",
    "Rok Zitko, David Nečas (Yeti)",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("createc",
                           N_("Createc files (.dat)"),
                           (GwyFileDetectFunc)&createc_detect,
                           (GwyFileLoadFunc)&createc_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
createc_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, ".dat") ? 10 : 0;

    if (createc_get_version(fileinfo->head, fileinfo->buffer_len))
        return 100;

    return 0;
}

#define createc_atof(x) g_ascii_strtod(x, NULL)

/* Macros to extract integer/double variables in hash_to_data_field() */
/* Any missing keyword/value pair is fatal, so we return a NULL pointer. */
#define HASH_GET(key, var, typeconv, err) \
    if (!(s = g_hash_table_lookup(hash, key))) { \
        err_MISSING_FIELD(err, key); \
        goto fail; \
    } \
    var = typeconv(s)

/* Support for alternative keywords in some (apparently newer) versions of dat
 * files */
#define HASH_GET2(key1, key2, var, typeconv, err) \
    if (!(s = g_hash_table_lookup(hash, key1))) { \
      if (!(s = g_hash_table_lookup(hash, key2))) { \
          g_set_error(err, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA, \
                      _("Neither `%s' nor `%s' header field found."), \
                      key1, key2); \
        goto fail; \
      } \
    } \
    var = typeconv(s)

#define HASH_INT(key, var, err)    HASH_GET(key, var, atoi, err)
#define HASH_DOUBLE(key, var, err) HASH_GET(key, var, createc_atof, err)
#define HASH_STRING(key, var, err) HASH_GET(key, var, /* */, err)

#define HASH_INT2(key1, key2, var, err)    HASH_GET2(key1, key2, var, atoi, err)
#define HASH_DOUBLE2(key1, key2, var, err) HASH_GET2(key1, key2, var, createc_atof, err)
#define HASH_STRING2(key1, key2, var, err) HASH_GET2(key1, key2, var, /* */, err)

static GwyContainer*
createc_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL, *zbuffer = NULL, *imagedata;
    gchar *p, *head;
    gsize size = 0, datasize = 0, offset;
    guint len, nchannels, id, channelbit, channelselect;
    GError *err = NULL;
    const gchar *s; /* for HASH_GET macros */
    GwyTextHeaderParser parser;
    GHashTable *hash = NULL;
    GwyDataField *dfield;
    CreatecVersion version;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (!(version = createc_get_version(buffer, size))) {
        err_FILE_TYPE(error, "Createc");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    head = g_memdup(buffer, HEADER_SIZE + 1);
    head[HEADER_SIZE] = '\0';
    len = strlen(gwy_enum_to_string(version, versions, G_N_ELEMENTS(versions)));
    for (p = head + len; g_ascii_isspace(*p); p++)
        ;

    /* Lots of lines contain just an equal sign, make them comments. */
    gwy_clear(&parser, 1);
    parser.comment_prefix = "=";
    parser.key_value_separator = "=";
    hash = gwy_text_header_parse(p, &parser, NULL, NULL);

    offset = first_channel_offset(version);
    if (size < offset) {
        err_TOO_SHORT(error);
        goto fail;
    }

    HASH_INT2("Channels", "Channels / Channels", nchannels, error);
    gwy_debug("nchannels: %u", nchannels);

    if (version == CREATEC_2Z) {
        guint bpp = channel_bpp(version);
        guint xres, yres;

        HASH_INT2("Num.X", "Num.X / Num.X", xres, error);
        HASH_INT2("Num.Y", "Num.Y / Num.Y", yres, error);

        if (!(zbuffer = unpack_compressed_data(buffer + offset,
                                               size - offset,
                                               nchannels*bpp*xres*yres,
                                               &datasize, error)))
            goto fail;

        imagedata = zbuffer;
        offset = 4;   /* the usual 4 unused bytes */
    }
    else {
        imagedata = buffer;
        datasize = size;
    }

    channelbit = 1;
    channelselect = 0;
    if ((s = g_hash_table_lookup(hash, "Channelselectval / Channelselectval")))
        channelselect = atoi(s);

    for (id = 0; id < nchannels; id++) {
        while (channelselect && channelbit && !(channelbit & channelselect))
            channelbit <<= 1;
        if (!channelselect || !channelbit)
            channelbit = 1;

        dfield = hash_to_data_field(hash, version, channelbit,
                                    imagedata, datasize, &offset, error);
        if (!dfield)
            break;

        channelbit <<= 1;
        if (!container)
            container = gwy_container_new();

        gwy_container_set_object(container,
                                 gwy_app_get_data_key_for_id(id), dfield);
        g_object_unref(dfield);

        gwy_app_channel_title_fall_back(container, id);

        meta = createc_get_metadata(hash);
        if (meta) {
            gchar *key = g_strdup_printf("/%u/meta", id);
            gwy_container_set_object_by_name(container, "/0/meta", meta);
            g_free(key);
            g_object_unref(meta);
        }
    }

fail:
    /* Must not free earlier, it holds the hash's strings */
    g_free(head);
    g_free(zbuffer);
    if (hash)
        g_hash_table_destroy(hash);
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static CreatecVersion
createc_get_version(const gchar *buffer,
                    gsize size)
{
    guint i, len;

    for (i = 0; i < G_N_ELEMENTS(versions); i++) {
        len = strlen(versions[i].name);
        if (size > len && memcmp(versions[i].name, buffer, len) == 0) {
            gwy_debug("header=%s, version=%u",
                      versions[i].name, versions[i].value);
            return versions[i].value;
        }
    }
    gwy_debug("header=%.*s, no version matched",
              (int)strlen(versions[0].name), buffer);
    return CREATEC_NONE;
}

static gsize
first_channel_offset(gint version)
{
    if (version == CREATEC_1)
        return 16384 + 2; /* header + 2 unused bytes */
    if (version == CREATEC_2)
        return 16384 + 4; /* header + 4 unused bytes */
    if (version == CREATEC_2Z)
        return 16384; /* header */
    g_return_val_if_reached(16384);
}

static guint
channel_bpp(gint version)
{
    if (version == CREATEC_1)
        return 2;
    if (version == CREATEC_2 || version == CREATEC_2Z)
        return 4;
    g_return_val_if_reached(4);
}

static GwyDataField*
hash_to_data_field(GHashTable *hash,
                   gint version,
                   guint channelbit,
                   const gchar *buffer,
                   gsize size,
                   gsize *offset,
                   GError **error)
{
    GwyDataField *dfield = NULL;
    GwySIUnit *unit;
    const gchar *s; /* for HASH_GET macros */
    guint xres, yres, bpp;
    gchar *imagedata = NULL;
    gdouble xreal, yreal, q = 1.0;
    gdouble *data;
    gint ti1, ti2; /* temporary storage */
    gdouble td; /* temporary storage */

    if (!hash)
        return NULL;

    bpp = channel_bpp(version);

    HASH_INT2("Num.X", "Num.X / Num.X", xres, error);
    HASH_INT2("Num.Y", "Num.Y / Num.Y", yres, error);
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        return NULL;

    if (err_SIZE_MISMATCH(error, *offset + bpp*xres*yres, size, FALSE))
        goto fail;

    if ((s = g_hash_table_lookup(hash, "Length x[A]")))
        xreal = Angstrom * createc_atof(s);
    else {
        HASH_INT2("Delta X", "Delta X / Delta X [Dac]", ti1, error);
        HASH_INT2("GainX", "GainX / GainX", ti2, error);
        HASH_DOUBLE("Xpiezoconst", td, error); /* lowcase p, why? */
        xreal = xres * ti1; /* dacs */
        xreal *= 20.0/65536.0 * ti2; /* voltage per dac */
        xreal *= Angstrom * td; /* piezoconstant [A/V] */
    }
    if (!(xreal = fabs(xreal))) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }

    if ((s = g_hash_table_lookup(hash, "Length y[A]")))
        yreal = Angstrom * createc_atof(s);
    else {
        HASH_INT2("Delta Y", "Delta Y / Delta Y [Dac]", ti1, error);
        HASH_INT2("GainY", "GainY / GainY", ti2, error);
        HASH_DOUBLE("YPiezoconst", td, error); /* upcase P */
        yreal = yres * ti1;
        yreal *= 20.0/65536.0 * ti2;
        yreal *= Angstrom * td;
    }
    if (!(yreal = fabs(yreal))) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    if ((s = g_hash_table_lookup(hash, "Dacto[A]z"))) {
        zres = Angstrom * createc_atof(s);
    }
    else {
        HASH_INT2("GainZ", "GainZ / GainZ", ti2, error);
        HASH_DOUBLE("ZPiezoconst", td, error); /* upcase P */
        q = 1.0; /* unity dac */
        q *= 20.0/65536.0 * ti2; /* voltage per dac */
        q *= Angstrom * td; /* piezoconstant [A/V] */
    }
    /* FIXME: */
    if (channelbit & CHANNEL_TOPOGRAPHY)
        q = 103.0/30.0 * 1e-4 * Angstrom;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    read_binary_data(xres*yres, data, buffer + *offset, bpp);
    *offset += bpp*xres*yres;
    gwy_data_field_multiply(dfield, q);

    unit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_from_string(unit, "m");

    unit = gwy_data_field_get_si_unit_z(dfield);
    if (channelbit & CHANNEL_TOPOGRAPHY)
        gwy_si_unit_set_from_string(unit, "m");
    else if (channelbit & CHANNEL_CURRENT)
        gwy_si_unit_set_from_string(unit, "A");
    else
        gwy_si_unit_set_from_string(unit, "V");

fail:
    g_free(imagedata);

    return dfield;
}

/* Macro for storing meta data */

#define HASH_STORE(key) \
    if ((val = g_hash_table_lookup(hash, key))) { \
        gwy_debug("key = %s, val = %s", key, val); \
        gwy_container_set_string_by_name(meta, key, g_strdup(val)); \
    }

static GwyContainer*
createc_get_metadata(GHashTable *hash)
{
    /* Relocation-less storage */
    static const gchar tobestored[] =
        "ActGainXYZ\0"
        "AproBurst\0"
        "AproPeriod\0"
        "BiasVoltage / BiasVolt.[mV]\0"
        "BiasVoltage\0"
        "Biasvolt[mV]\0"
        "CHModeBias[mV] / CHModeBias[mV]\0"
        "CHModeGainpreamp / CHModeGainpreamp\0"
        "CHModeZoff / CHModeZoff\0"
        "CP_V_Limit\0"
        "Chan(1,2,4) / Chan(1,2,4)\0"
        "Chan(1,2,4)\0"
        "Channels / Channels\0"
        "Channels\0"
        "Channelselectval / Channelselectval\0"
        "CurrentRC\0"
        "Current[A]\0"
        "D-DeltaX / D-DeltaX\0"
        "D-DeltaX\0"
        "DAC-Type\0"
        "DAC5[V]\0"
        "DSP_Clock\0"
        "DX_DIV_DDelta-X / DX/DDeltaX\0"
        "Dacto[A]xy\0"
        "Dacto[A]z\0"
        "Dactonmx\0"
        "Dactonmz\0"
        "Delay X+ / Delay X+\0"
        "Delay X+\0"
        "Delay X- / Delay X-\0"
        "Delay X-\0"
        "Delay Y / Delay Y\0"
        "Delay Y\0"
        "Delta X / Delta X [Dac]\0"
        "Delta X\0"
        "Delta Y / Delta Y [Dac]\0"
        "Delta Y\0"
        "DigZoomX\0"
        "DigZoomZ\0"
        "DispAutoXmax\0"
        "DispAutoXmin\0"
        "FBBackLogIset\0"
        "FBChannel\0"
        "FBIntegral\0"
        "FBIset\0"
        "FBLingain\0"
        "FBLogIset\0"
        "FBLog\0"
        "FBPropGain\0"
        "FBProp\0"
        "FBRC\0"
        "FBVoltGain\0"
        "FBVoltRC\0"
        "FFTPoints\0"
        "Frameimageoffset\0"
        "GainX / GainX\0"
        "GainX\0"
        "GainY / GainY\0"
        "GainY\0"
        "GainZ / GainZ\0"
        "GainZ\0"
        "Gainpreamp / GainPre 10^\0"
        "Gainpreamp\0"
        "HPIB_Address\0"
        "HP_Ch1\0"
        "HP_Ch2\0"
        "HP_Ch3\0"
        "HP_Ch4\0"
        "ImageAutoMode\0"
        "ImageAutoScale\0"
        "Imagebackoffset\0"
        "Imageframe\0"
        "Imagegrayfactor\0"
        "ImaxZret\0"
        "Imaxcurrent\0"
        "Imaxdelay\0"
        "Latm0Delay\0"
        "LatmResist\0"
        "LatmanVolt\0"
        "Latmanccdz\0"
        "Latmanddx\0"
        "Latmandelay\0"
        "Latmanextension\0"
        "Latmangain\0"
        "Latmanlgi\0"
        "Latmanmode\0"
        "Length x[A]\0"
        "Length y[A]\0"
        "LockinAmpl\0"
        "LockinChannel\0"
        "LockinFreq\0"
        "LockinMode\0"
        "LockinPhase2\0"
        "LockinPhase\0"
        "LockinRC\0"
        "LockinRefAmpl\0"
        "LockinRefPhase\0"
        "MVolt_1 / MVolt_1\0"
        "MVolt_10 / MVolt_10\0"
        "MVolt_2 / MVolt_2\0"
        "MVolt_3 / MVolt_3\0"
        "MVolt_4 / MVolt_4\0"
        "MVolt_5 / MVolt_5\0"
        "MVolt_6 / MVolt_6\0"
        "MVolt_7 / MVolt_7\0"
        "MVolt_8 / MVolt_8\0"
        "MVolt_9 / MVolt_9\0"
        "Mcurrent_1 / Mcurrent_1\0"
        "Mcurrent_10 / Mcurrent_10\0"
        "Mcurrent_2 / Mcurrent_2\0"
        "Mcurrent_3 / Mcurrent_3\0"
        "Mcurrent_4 / Mcurrent_4\0"
        "Mcurrent_5 / Mcurrent_5\0"
        "Mcurrent_6 / Mcurrent_6\0"
        "Mcurrent_7 / Mcurrent_7\0"
        "Mcurrent_8 / Mcurrent_8\0"
        "Mcurrent_9 / Mcurrent_9\0"
        "Num.X / Num.X\0"
        "Num.Y / Num.Y\0"
        "OrgPlanOff\0"
        "OrgPlanX\0"
        "OrgPlanY\0"
        "PLLAmplGain\0"
        "PLLAmplMin\0"
        "PLLAmplPostFilter\0"
        "PLLAmplitude\0"
        "PLLCenterFreq\0"
        "PLLControl\0"
        "PLLFreqOffMax\0"
        "PLLFreqPostFilter\0"
        "PLLGain\0"
        "PLLPhase\0"
        "PLLRC\0"
        "PLLScanDelay\0"
        "PLLScanInc\0"
        "PLLScanStartFreq\0"
        "PLLScanStopFreq\0"
        "PlanDx / PlanDx\0"
        "PlanDy / PlanDy\0"
        "Planavrgnr\0"
        "Plany2\0"
        "Preamptype / Preamptype\0"
        "RepeatRotinc / RepeatRotinc\0"
        "RepeatXoffset / RepeatXoffset\0"
        "RepeatYoffset / RepeatYoffset\0"
        "Repeatcounter / Repeatcounter\0"
        "Repeatinterval / Repeatinterval\0"
        "RotBurst\0"
        "RotCMode / RotCMode\0"
        "Rotation / Rotation\0"
        "Rotation\0"
        "Rotcount\0"
        "Rotinc\0"
        "Rotincquad\0"
        "RptBVoltinc / RptBVoltinc\0"
        "SBC_Clk[MHz]\0"
        "SRS_Frequency\0"
        "SRS_InpGain[V]\0"
        "SRS_InpTimeC[s]\0"
        "SRS_ModVoltage\0"
        "ScanYDirec / ScanYDirec\0"
        "ScanYMode / ScanYMode\0"
        "Scancoarse / Scancoarse\0"
        "Scancoarse\0"
        "Scandvinc[DAC]\0"
        "Scanmode / ScanXMode\0"
        "ScanmodeSine / ScanmodeSine\0"
        "Scanrotoffx / OffsetX\0"
        "Scanrotoffy / OffsetY\0"
        "Scantype / Scantype\0"
        "Scantype\0"
        "Sec/Image:\0"
        "Sec/line:\0"
        "SpecAvrgnr\0"
        "SpecFreq\0"
        "SpecXGrid\0"
        "SpecXYGridDelay\0"
        "SpecYGrid\0"
        "T-STM:\0"
        "T_ADC2[K]\0"
        "T_ADC3[K]\0"
        "TipForm_Volt\0"
        "TipForm_ZOffset\0"
        "TipForm_Z\0"
        "Tip_Delay\0"
        "Tip_Gain\0"
        "Tip_LatDelay\0"
        "Tip_Latddx\0"
        "Titel / Titel\0"
        "Titel\0"
        "Upcount\0"
        "Upinc\0"
        "UserPreampCode\0"
        "VFBMode / VFBMode\0"
        "VertAvrgdelay\0"
        "VertFBLogiset\0"
        "VertFBMode\0"
        "VertLatddx\0"
        "VertLatdelay\0"
        "VertLineCount\0"
        "VertRepeatCounter\0"
        "VertSpecAvrgnr\0"
        "VertSpecBack\0"
        "VertStartdly[ms]\0"
        "Vertchannelselectval\0"
        "Vertmandelay\0"
        "Vertmangain\0"
        "VerttreshImax\0"
        "VerttreshImin\0"
        "Vpoint0.V\0"
        "Vpoint0.t\0"
        "Vpoint1.V\0"
        "Vpoint1.t\0"
        "Vpoint2.V\0"
        "Vpoint2.t\0"
        "Vpoint3.V\0"
        "Vpoint3.t\0"
        "Vpoint4.V\0"
        "Vpoint4.t\0"
        "Vpoint5.V\0"
        "Vpoint5.t\0"
        "Vpoint6.V\0"
        "Vpoint6.t\0"
        "Vpoint7.V\0"
        "Vpoint7.t\0"
        "X-Puls-Count\0"
        "XYBurst\0"
        "Xpiezoconst\0"
        "Y-Puls-Count\0"
        "YPiezoconst\0"
        "Z-Puls-Count\0"
        "ZPiezoconst\0"
        "Zdrift\0"
        "Zoffset\0"
        "Zoom\0"
        "Zpoint0.t\0"
        "Zpoint0.z\0"
        "Zpoint1.t\0"
        "Zpoint1.z\0"
        "Zpoint2.t\0"
        "Zpoint2.z\0"
        "Zpoint3.t\0"
        "Zpoint3.z\0"
        "Zpoint4.t\0"
        "Zpoint4.z\0"
        "Zpoint5.t\0"
        "Zpoint5.z\0"
        "Zpoint6.t\0"
        "Zpoint6.z\0"
        "Zpoint7.t\0"
        "Zpoint7.z\0"
        "memo:0\0"
        "memo:1\0"
        "memo:2\0"
        "\0";
    GwyContainer *meta;
    const gchar *ctr;
    gchar *val;

    meta = gwy_container_new();

    for (ctr = tobestored; *ctr; ctr += strlen(ctr) + 1)
        HASH_STORE(ctr);

    if (!gwy_container_get_n_items(meta)) {
        g_object_unref(meta);
        meta = NULL;
    }

    return meta;
}

static void
read_binary_data(gint n,
                 gdouble *data,
                 const gchar *buffer,
                 gint bpp)
{
    gint i;
    gdouble q;

    q = 1.0/(1 << (8*bpp));
    switch (bpp) {
        case 2:
        {
            const gint16 *p = (const gint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*GINT16_FROM_LE(p[i]);
        }
        break;

        case 4:
        {
            const guchar *p = (const guchar*)buffer;

            for (i = 0; i < n; i++)
                data[i] = gwy_get_gfloat_le(&p);
        }

        break;

        default:
        g_return_if_reached();
        break;
    }
}

#ifdef HAVE_ZLIB
/* XXX: Common with matfile.c */
static inline gboolean
zinflate_into(z_stream *zbuf,
              gint flush_mode,
              gsize csize,
              const guchar *compressed,
              GByteArray *output,
              GError **error)
{
    gint status;
    gboolean retval = TRUE;

    gwy_clear(zbuf, 1);
    zbuf->next_in = (char*)compressed;
    zbuf->avail_in = csize;
    zbuf->next_out = output->data;
    zbuf->avail_out = output->len;

    /* XXX: zbuf->msg does not seem to ever contain anything, so just report
     * the error codes. */
    if ((status = inflateInit(zbuf)) != Z_OK) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("zlib initialization failed with error %d, "
                      "cannot decompress data."),
                    status);
        return FALSE;
    }

    if ((status = inflate(zbuf, flush_mode)) != Z_OK
        /* zlib return Z_STREAM_END also when we *exactly* exhaust all input.
         * But this is no error, in fact it should happen every time, so check
         * for it specifically. */
        && !(status == Z_STREAM_END
             && zbuf->total_in == csize
             && zbuf->total_out == output->len)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Decompression of compressed data failed with "
                      "error %d."),
                    status);
        retval = FALSE;
    }

    status = inflateEnd(zbuf);
    /* This should not really happen whatever data we pass in.  And we have
     * already our output, so just make some noise and get over it.  */
    if (status != Z_OK)
        g_critical("inflateEnd() failed with error %d", status);

    return retval;
}

static gchar*
unpack_compressed_data(const guchar *buffer,
                       gsize size,
                       gsize imagesize,
                       gsize *datasize,
                       GError **error)
{
    gsize expected_size = imagesize + 4;  /* 4 unused bytes, as usual */
    z_stream zbuf; /* decompression stream */
    GByteArray *output;
    gboolean ok;

    gwy_debug("Expecting to decompress %u data fields", ndata);
    output = g_byte_array_sized_new(expected_size);
    g_byte_array_set_size(output, expected_size);
    ok = zinflate_into(&zbuf, Z_SYNC_FLUSH, size, buffer, output, error);
    *datasize = output->len;

    return g_byte_array_free(output, !ok);
}
#else
static gchar*
unpack_compressed_data(G_GNUC_UNUSED const guchar *buffer,
                       G_GNUC_UNUSED gsize size,
                       G_GNUC_UNUSED gsize imagesize,
                       gsize *datasize,
                       GError **error)
{
    *datasize = 0;
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_SPECIFIC,
                _("Cannot decompress compressed data.  Zlib support was "
                  "not built in."));
    return NULL;
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
