/*
 *  $Id$
 *  Copyright (C) 2005 Chris Anderson, Molecular Imaging Corp.
 *  E-mail: sidewinder.asu@gmail.com
 *
 *  This source code is based off of stpfile.c,
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klstptek.
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

 /*TODO: Make sure it can handle old beta .mi files that had "data" tag with no
reference to BINARY or ASCII. Also, implement loading of ASCII files */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-mi-spm">
 *   <comment>Molecular Imaging SPM data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="fileType      Image"/>
 *     <match type="string" offset="0" value="fileType      Spectroscopy"/>
 *   </magic>
 *   <glob pattern="*.mi"/>
 *   <glob pattern="*.MI"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define IMAGE_MAGIC "fileType      Image"
#define IMAGE_MAGIC_SIZE (sizeof(IMAGE_MAGIC) - 1)
#define SPECT_MAGIC "fileType      Spectroscopy"
#define SPECT_MAGIC_SIZE (sizeof(SPECT_MAGIC) - 1)

#define DATA_MAGIC "data          \n"
#define DATA_MAGIC_SIZE (sizeof(DATA_MAGIC) - 1)
#define BINARY_DATA_MAGIC "data          BINARY\n"
#define BINARY_DATA_MAGIC_SIZE (sizeof(BINARY_DATA_MAGIC) - 1)
#define ASCII_DATA_MAGIC  "data          ASCII\n"
#define ASCII_DATA_MAGIC_SIZE (sizeof(ASCII_DATA_MAGIC) - 1)

#define EXTENSION ".mi"
#define KEY_LEN 14
#define GRAPH_PREFIX "/0/graph/graph"

/* These two structs are for MI Image Files only */
typedef struct {
    gchar *id;
    const gint16 *data;
    GHashTable *meta;
} MIData;

typedef struct {
    gint xres;
    gint yres;
    guint n;
    MIData *buffers;
    GHashTable *meta;
} MIFile;

/* These two structs are for MI Spectroscopy Files only */
typedef struct {
    gchar *label;
    gchar *unit;
} MISpectData;

typedef struct {
    gint num_buffers;
    MISpectData *buffers;

    gint num_points;
    const gfloat *data;

    GHashTable *meta;
} MISpectFile;

typedef struct {
    const gchar *key;
    const gchar *meta;
    const gchar *format;
} MetaDataFormat;

typedef struct {
    gint key;
    gint meta;
    gint format;
} MetaDataFlatFormat;

/* Gwyddion File Module Functions */
static gboolean         module_register      (void);
static gint             mifile_detect        (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*    mifile_load          (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);

/* Helper Functions */
static guint        find_data_start         (const guchar *buffer, gsize size,
                                             gboolean *isbinary);
static guint        image_file_read_header  (MIFile *mifile,
                                             gchar *buffer,
                                             GError **error);
static guint        spect_file_read_header  (MISpectFile *mifile,
                                             gchar *buffer,
                                             GError **error);
static void         read_data_field         (GwyDataField *dfield,
                                             MIData *midata,
                                             gint xres, gint yres);
static void         image_file_free         (MIFile *mifile);
static void         spect_file_free         (MISpectFile *mifile);
static gboolean     mifile_get_double       (GHashTable *meta,
                                             const gchar *key,
                                             gdouble *value);
static void         process_metadata        (MIFile *mifile,
                                             guint id,
                                             GwyContainer *container,
                                             const gchar *container_key);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Molecular Imaging MI data files."),
    "Chris Anderson <sidewinder.asu@gmail.com>",
    "0.10",
    "Chris Anderson, Molecular Imaging Corp.",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("mifile",
                           N_("PicoView Data Files (.mi)"),
                           (GwyFileDetectFunc)&mifile_detect,
                           (GwyFileLoadFunc)&mifile_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
mifile_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > IMAGE_MAGIC_SIZE
        && (!memcmp(fileinfo->head, IMAGE_MAGIC, IMAGE_MAGIC_SIZE)
            || !memcmp(fileinfo->head, SPECT_MAGIC, SPECT_MAGIC_SIZE)))
        score = 100;

    return score;
}

static GwyContainer*
mifile_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    MIFile *mifile = NULL;
    MISpectFile *mifile_spect = NULL;
    GwyContainer *container = NULL;
    gchar *container_key = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwyGraphCurveModel *cmodel;
    GwyGraphModel *gmodel;
    gdouble *xdata, *ydata;
    guint header_size;
    gchar *p;
    gboolean ok = TRUE;
    gboolean isbinary = TRUE;
    gboolean isimage = TRUE;
    guint i = 0, j = 0, pos, buffi;

    /* Open the file and load in its contents into "buffer" */
    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    /* Make sure file is of reasonable size */
    if (size <= BINARY_DATA_MAGIC_SIZE)
        ok = FALSE;

    /* Find out if this is an Image or Spectroscopy file */
    if (!strncmp(buffer, IMAGE_MAGIC, IMAGE_MAGIC_SIZE))
        isimage = TRUE;
    else if (!strncmp(buffer, SPECT_MAGIC, SPECT_MAGIC_SIZE))
        isimage = FALSE;
    else
        ok = FALSE;

    gwy_debug("*************************************");
    gwy_debug("*************************************");
    gwy_debug("*************************************");
    gwy_debug("isimage: %i    ok: %i", isimage, ok);

    /* Find out the length of the file header (and binary/ascii mode) */
    header_size = find_data_start(buffer, size, &isbinary);
    if (!header_size)
         ok = FALSE;

    gwy_debug("header_size: %i", header_size);
    gwy_debug("*************************************");
    gwy_debug("*************************************");
    gwy_debug("*************************************");

    /* Report error if file is invalid */
    if (!ok) {
        err_FILE_TYPE(error, "MI");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    /* Load the header information into the appropriate structure */
    p = g_strndup(buffer, header_size);
    if (isimage) {
        mifile = g_new0(MIFile, 1);
        ok = image_file_read_header(mifile, p, error);
        ok = ok && !(err_DIMENSION(error, mifile->xres)
                     || err_DIMENSION(error, mifile->yres));
        ok = ok && !err_SIZE_MISMATCH(error,
                                      header_size
                                      + mifile->n*2*mifile->xres*mifile->yres,
                                      size, FALSE);
        if (!ok)
            image_file_free(mifile);
    }
    else {
        mifile_spect = g_new0(MISpectFile, 1);
        ok = spect_file_read_header(mifile_spect, p, error);
        ok = ok && !err_DIMENSION(error, mifile_spect->num_points);
        if (!ok)
            spect_file_free(mifile_spect);
    }
    g_free(p);

    if (!ok) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    /* Load the image data or spectroscopy data */
    if (isimage) {
        /* Load image data: */
        pos = header_size;
        for (i = 0; i < mifile->n; i++) {
            mifile->buffers[i].data = (const gint16*)(buffer + pos);
            pos += 2*mifile->xres * mifile->yres;

            dfield = gwy_data_field_new(mifile->xres, mifile->yres,
                                        1.0, 1.0, FALSE);
            read_data_field(dfield, mifile->buffers + i,
                            mifile->xres, mifile->yres);

            if (!container)
                container = gwy_container_new();

            container_key = g_strdup_printf("/%i/data", i);
            gwy_container_set_object_by_name(container, container_key,
                                             dfield);
            g_object_unref(dfield);
            process_metadata(mifile, i, container, container_key);
            g_free(container_key);
        }

        image_file_free(mifile);
    }
    else {
        const guchar *pp;
        /* Load spectroscopy data: */

        /* create a container */
        container = gwy_container_new();

        /* create a dummy dfield and add to container
           XXX: This is a temporary cheap hack */
        dfield = gwy_data_field_new(256, 256, 1.0, 1.0, TRUE);
        container_key = g_strdup("/0/data");
        gwy_container_set_object_by_name(container, container_key, dfield);
        g_object_unref(dfield);
        g_free(container_key);
        container_key = g_strdup("/0/data/title");
        gwy_container_set_string_by_name(container, container_key,
                                         g_strdup("Ignore Me"));

        /* get a pointer to the spectroscopy data */
        pos = header_size;
        mifile_spect->data = (const gfloat*)(buffer + pos);

        /* load xdata */
        buffi = mifile_spect->num_points; /* skip time data */
        xdata = g_new0(gdouble, mifile_spect->num_points);
        for (i = 0; i < mifile_spect->num_points; i++) {
            pp = (const guchar*)(mifile_spect->data + buffi + i);
            xdata[i] = gwy_get_gfloat_le(&pp);
            gwy_debug("i: %i   xdata: %f", i, xdata[i]);
        }

        /* The first buffer always represents the x axis. All
           remaining buffers represent the corresponding Y axes of seperate
           graphs. As a result, we need to create a num_buffers-1 graphs. */
        for (j = 0; j < mifile_spect->num_buffers-1; j++) {
            buffi += mifile_spect->num_points;

            ydata = g_new0(gdouble, mifile_spect->num_points);
            for (i = 0; i < mifile_spect->num_points; i++) {
                pp = (const guchar*)(mifile_spect->data + buffi + i);
                ydata[i] = gwy_get_gfloat_le(&pp);
                gwy_debug("i: %i   ydata: %f", i, ydata[i]);
            }

            /* create graph model and curve model */
            gmodel = gwy_graph_model_new();
            cmodel = gwy_graph_curve_model_new();
            gwy_graph_model_add_curve(gmodel, cmodel);
            g_object_unref(cmodel);

            g_object_set(gmodel, "title", _("Spectroscopy Graph"), NULL);
            /* XXX: SET UNITS HERE gwy_graph_model_set_si_unit_x */
            g_object_set(cmodel,
                         "description", "Curve 1",
                         "mode", GWY_GRAPH_CURVE_POINTS,
                         NULL);
            gwy_graph_curve_model_set_data(cmodel, xdata, ydata,
                                           mifile_spect->num_points);
            g_free(ydata);

            /* add gmodel to container */
            container_key = g_strdup_printf("%s/%d", GRAPH_PREFIX, j+1);
            gwy_container_set_object_by_name(container, container_key,
                                             gmodel);
            g_object_unref(gmodel);
            g_free(container_key);
        }
        g_free(xdata);

        spect_file_free(mifile_spect);
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static guint
find_data_start(const guchar *buffer, gsize size, gboolean *isbinary)
{
    const guchar *locate_none, *locate_binary, *locate_ascii;
    guint value = 0;

    locate_none = g_strstr_len(buffer, size, DATA_MAGIC);
    locate_binary = g_strstr_len(buffer, size, BINARY_DATA_MAGIC);
    locate_ascii = g_strstr_len(buffer, size, ASCII_DATA_MAGIC);

    if (locate_none != NULL) {
        *isbinary = TRUE;
        value = (locate_none - buffer) + DATA_MAGIC_SIZE;
    }
    else if (locate_binary != NULL) {
        *isbinary = TRUE;
        value = (locate_binary - buffer) + BINARY_DATA_MAGIC_SIZE;
    }
    else if (locate_ascii != NULL) {
        *isbinary = FALSE;
        value = (locate_ascii - buffer) + ASCII_DATA_MAGIC_SIZE;
    }
    else
        value = 0;

    return value;
}

static guint
image_file_read_header(MIFile *mifile, gchar *buffer, GError **error)
{
    MIData *data = NULL;
    GHashTable *meta;
    gchar *line, *key, *value = NULL;

    mifile->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                         g_free, g_free);
    mifile->xres = mifile->yres = 0;
    meta = mifile->meta;
    while ((line = gwy_str_next_line(&buffer))) {
        if (!strncmp(line, "bufferLabel   ", KEY_LEN)) {
            mifile->n++;
            mifile->buffers = g_renew(MIData, mifile->buffers, mifile->n);
            data = mifile->buffers + (mifile->n - 1);
            data->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                               g_free, g_free);
            data->data = NULL;
            data->id = g_strstrip(g_strdup(line + KEY_LEN));
            meta = data->meta;
        }
        if (line[0] == ' ')
            continue;

        key = g_strstrip(g_strndup(line, KEY_LEN));
        value = g_strstrip(g_strdup(line + KEY_LEN));
        g_hash_table_replace(meta, key, value);

        if (!strcmp(key, "xPixels"))
            mifile->xres = atol(value);
        if (!strcmp(key, "yPixels"))
            mifile->yres = atol(value);
    }

    if (!mifile->n)
        err_NO_DATA(error);

    return mifile->n;
}

static guint
spect_file_read_header(MISpectFile *mifile, gchar *buffer, GError **error)
{
    MISpectData *data = NULL;
    GHashTable *meta;
    gchar *line, *key, *value = NULL;

    mifile->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                         g_free, g_free);
    meta = mifile->meta;

    while ((line = gwy_str_next_line(&buffer))) {
        if (!strncmp(line, "bufferLabel   ", KEY_LEN)) {
            mifile->num_buffers++;
            mifile->buffers = g_renew(MISpectData, mifile->buffers,
                                      mifile->num_buffers);
            data = mifile->buffers + (mifile->num_buffers - 1);

            data->label = NULL;
            data->unit = NULL;

            /* store buffer label */
            data->label = g_strstrip(g_strdup(line + KEY_LEN));

            /* store buffer unit (by geting next line which should be
               "bufferUnit") */
            if ((line = gwy_str_next_line(&buffer))) {
                if (!strncmp(line, "bufferUnit    ", KEY_LEN)) {
                    data->unit = g_strstrip(g_strdup(line + KEY_LEN));
                }
                else {
                    err_INVALID(error, "bufferUnit");
                    return 0;
                }
            } else {
                err_INVALID(error, "bufferLabel");
                return 0;
            }
        }
        if (line[0] == ' ')
            continue;

        key = g_strstrip(g_strndup(line, KEY_LEN));
        value = g_strstrip(g_strdup(line + KEY_LEN));
        g_hash_table_replace(meta, key, value);

        if (!strcmp(key, "DataPoints"))
            mifile->num_points = atol(value);
    }

    if (!mifile->num_buffers)
        err_NO_DATA(error);

    return mifile->num_buffers;
}

static void
read_data_field(GwyDataField *dfield,
                MIData *midata,
                gint xres, gint yres)
{
    gdouble *data;
    const gint16 *row;
    gint i, j;

    gwy_data_field_resample(dfield, xres, yres, GWY_INTERPOLATION_NONE);
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < yres; i++) {
        row = midata->data + (yres-1 - i)*xres;
        for (j = 0; j < xres; j++)
            data[i*xres + j] = GINT16_FROM_LE(row[j]);
    }
    gwy_data_field_data_changed(dfield);
}

static void
image_file_free(MIFile *mifile)
{
    guint i;

    for (i = 0; i < mifile->n; i++) {
        g_hash_table_destroy(mifile->buffers[i].meta);
        g_free(mifile->buffers[i].id);
    }
    g_free(mifile->buffers);
    g_hash_table_destroy(mifile->meta);
    g_free(mifile);
}

static void
spect_file_free(MISpectFile *mifile)
{

    guint i;

    for (i = 0; i < mifile->num_buffers; i++) {
        if (mifile->buffers[i].label)
            g_free(mifile->buffers[i].label);
        if (mifile->buffers[i].unit)
            g_free(mifile->buffers[i].unit);
    }
    g_free(mifile->buffers);
    g_hash_table_destroy(mifile->meta);
    g_free(mifile);
}

static gboolean
mifile_get_double(GHashTable *meta,
                  const gchar *key,
                  gdouble *value)
{
    gchar *p, *end;
    gdouble r;

    p = g_hash_table_lookup(meta, key);
    if (!p)
        return FALSE;

    r = g_ascii_strtod(p, &end);
    if (end == p)
        return FALSE;

    *value = r;
    return TRUE;
}

static void
process_metadata(MIFile *mifile,
                 guint id,
                 GwyContainer *container,
                 const gchar *container_key)
{
#ifdef GWY_RELOC_SOURCE
    /* @flat: MetaDataFlatFormat */
    /* @fields: key, meta, format */
    static const MetaDataFormat global_metadata[] = {
        { "version", "Version", "%s" },
        { "dateAcquired", "Date acquired", "%s" },
        { "mode", "mode", "%s" },
        { "xSensitivity", "xSensitivity", "%s" },
        { "xNonlinearity", "xNonlinearity", "%s" },
        { "xHysteresis", "xHysteresis", "%s" },
        { "ySensitivity", "ySensitivity", "%s" },
        { "yNonlinearity", "yNonlinearity", "%s" },
        { "yHysteresis", "yHysteresis", "%s" },
        { "zSensitivity", "zSensitivity", "%s" },
        { "reverseX", "reverseX", "%s" },
        { "reverseY", "reverseY", "%s" },
        { "reverseZ", "reverseZ", "%s" },
        { "xDacRange", "xDacRange", "%s" },
        { "yDacRange", "yDacRange", "%s" },
        { "zDacRange", "zDacRange", "%s" },
        { "xPixels", "xPixels", "%s" },
        { "yPixels", "yPixels", "%s" },
        { "xOffset", "xOffset", "%s" },
        { "yOffset", "yOffset", "%s" },
        { "xLength", "xLength", "%s" },
        { "yLength", "yLength", "%s" },
        /*{ "scanUp", "scanUp", "%s" },*/
        { "scanSpeed", "scanSpeed", "%s" },
        { "scanAngle", "scanAngle", "%s" },
        { "servoSetpoint", "servoSetpoint", "%s" },
        { "biasSample", "biasSample", "%s" },
        { "bias", "bias", "%s" },
        { "servoIGain", "servoIGain", "%s" },
        { "servoPGain", "servoPGain", "%s" },
        { "servoRange", "servoRange", "%s" },
        { "servoInputGain", "servoInputGain", "%s" },
    };
#else  /* {{{ */
    /* This code block was GENERATED by flatten.py.
       When you edit global_metadata[] data above,
       re-run flatten.py SOURCE.c. */
    static const gchar global_metadata_key[] =
        "version\000dateAcquired\000mode\000xSensitivity\000xNonlinearity\000"
        "xHysteresis\000ySensitivity\000yNonlinearity\000yHysteresis\000zSens"
        "itivity\000reverseX\000reverseY\000reverseZ\000xDacRange\000yDacRang"
        "e\000zDacRange\000xPixels\000yPixels\000xOffset\000yOffset\000xLengt"
        "h\000yLength\000scanSpeed\000scanAngle\000servoSetpoint\000biasSampl"
        "e\000bias\000servoIGain\000servoPGain\000servoRange\000servoInputGai"
        "n";

    static const gchar global_metadata_meta[] =
        "Version\000Date acquired\000mode\000xSensitivity\000xNonlinearity"
        "\000xHysteresis\000ySensitivity\000yNonlinearity\000yHysteresis\000z"
        "Sensitivity\000reverseX\000reverseY\000reverseZ\000xDacRange\000yDac"
        "Range\000zDacRange\000xPixels\000yPixels\000xOffset\000yOffset\000xL"
        "ength\000yLength\000scanSpeed\000scanAngle\000servoSetpoint\000biasS"
        "ample\000bias\000servoIGain\000servoPGain\000servoRange\000servoInpu"
        "tGain";

    static const gchar global_metadata_format[] =
        "%s\000%s\000%s\000%s\000%s\000%s\000%s\000%s\000%s\000%s\000%s\000%s"
        "\000%s\000%s\000%s\000%s\000%s\000%s\000%s\000%s\000%s\000%s\000%s"
        "\000%s\000%s\000%s\000%s\000%s\000%s\000%s\000%s";

    static const MetaDataFlatFormat global_metadata[] = {
        { 0, 0, 0 },
        { 8, 8, 3 },
        { 21, 22, 6 },
        { 26, 27, 9 },
        { 39, 40, 12 },
        { 53, 54, 15 },
        { 65, 66, 18 },
        { 78, 79, 21 },
        { 92, 93, 24 },
        { 104, 105, 27 },
        { 117, 118, 30 },
        { 126, 127, 33 },
        { 135, 136, 36 },
        { 144, 145, 39 },
        { 154, 155, 42 },
        { 164, 165, 45 },
        { 174, 175, 48 },
        { 182, 183, 51 },
        { 190, 191, 54 },
        { 198, 199, 57 },
        { 206, 207, 60 },
        { 214, 215, 63 },
        { 222, 223, 66 },
        { 232, 233, 69 },
        { 242, 243, 72 },
        { 256, 257, 75 },
        { 267, 268, 78 },
        { 272, 273, 81 },
        { 283, 284, 84 },
        { 294, 295, 87 },
        { 305, 306, 90 },
    };
#endif  /* }}} */

#ifdef GWY_RELOC_SOURCE
    /* @flat: MetaDataFlatFormat */
    /* @fields: key, meta, format */
    static const MetaDataFormat local_metadata[] = {
        { "trace", "trace", "%s" },
    };
#else  /* {{{ */
    /* This code block was GENERATED by flatten.py.
       When you edit local_metadata[] data above,
       re-run flatten.py SOURCE.c. */
    static const gchar local_metadata_key[] =
        "trace";

    static const gchar local_metadata_meta[] =
        "trace";

    static const gchar local_metadata_format[] =
        "%s";

    static const MetaDataFlatFormat local_metadata[] = {
        { 0, 0, 0 },
    };
#endif  /* }}} */

    MIData *data;
    GwyContainer *meta;
    GwyDataField *dfield;
    const gchar *mode, *s;
    gchar *bufferUnit;
    GwySIUnit *siunit;
    gint power10;
    gdouble bufferRange, conversion;
    GString *str;
    gchar *p;
    guint i;
    gdouble xLength, yLength;
    gchar *channel_key = NULL;
    gchar *channel_title = NULL;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(container,
                                                             container_key));
    /* Make "data" point to the selected buffer */
    data = mifile->buffers + id;

    /* Get the buffer mode */
    mode = NULL;
    mode = g_hash_table_lookup(data->meta, "bufferLabel");

    /* Set the container's title to whatever the buffer mode is */
    if (mode)
        channel_title = g_strdup(mode);
    else
        channel_title = g_strdup("Unknown Channel");
    channel_key = g_strdup_printf("%s/title", container_key);
    gwy_container_set_string_by_name(container, channel_key,
                                     g_strdup(channel_title));

    /* If this is the first channel, store the title under /filename/title as
    well for compatability with 1.x. */
    if (id == 0)
        gwy_container_set_string_by_name(container, "/filename/title",
                                         g_strdup(channel_title));
    g_free(channel_key);
    g_free(channel_title);

    /* Fix z-value scale */
    bufferUnit = NULL;
    power10 = 0;
    bufferUnit = g_hash_table_lookup(data->meta, "bufferUnit");
    if (bufferUnit) {
        siunit = gwy_si_unit_new_parse(bufferUnit, &power10);
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);
    }
    if (mifile_get_double(data->meta, "bufferRange", &bufferRange)) {
        bufferRange *= pow10(power10);
        conversion = bufferRange / 32768;
        gwy_data_field_multiply(dfield, conversion);
    }

    /* Fix x-y value scale */
    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    if (!mifile_get_double(mifile->meta, "xLength", &xLength)) {
        g_warning("Missing or invalid x length");
        xLength = 1e-9;
    }
    gwy_data_field_set_xreal(dfield, xLength);

    if (!mifile_get_double(mifile->meta, "yLength", &yLength)) {
        g_warning("Missing or invalid y length");
        yLength = 1e-9;
    }
    gwy_data_field_set_yreal(dfield, yLength);

    /* Store Metadata */
    meta = gwy_container_new();
    str = g_string_new(NULL);

    /* Global */
    for (i = 0; i < G_N_ELEMENTS(global_metadata); i++) {
        s = global_metadata_key + global_metadata[i].key;
        if (!(p = g_hash_table_lookup(mifile->meta, s)))
            continue;

        s = global_metadata_format + global_metadata[i].format;
        g_string_printf(str, s, p);
        s = global_metadata_meta + global_metadata[i].meta;
        gwy_container_set_string_by_name(meta, s, g_strdup(str->str));
    }

    /* Local */
    for (i = 0; i < G_N_ELEMENTS(local_metadata); i++) {
        s = local_metadata_key + local_metadata[i].key;
        if (!(p = g_hash_table_lookup(data->meta, s)))
            continue;

        s = local_metadata_format + local_metadata[i].format;
        g_string_printf(str, s, p);
        s = local_metadata_meta + local_metadata[i].meta;
        gwy_container_set_string_by_name(meta, s, g_strdup(str->str));
    }


    /* Store "Special" metadata */

    /*
    if ((p = g_hash_table_lookup(data->meta, "Date"))
        && (s = g_hash_table_lookup(data->meta, "time")))
        gwy_container_set_string_by_name(meta, "Date",
                                         g_strconcat(p, " ", s, NULL));
    */

    if ((p = g_hash_table_lookup(mifile->meta, "scanUp"))) {
        if (g_str_equal(p, "FALSE"))
            gwy_container_set_string_by_name(meta, "Scanning direction",
                                             g_strdup("Top to bottom"));
        else if (g_str_equal(p, "TRUE"))
            gwy_container_set_string_by_name(meta, "Scanning direction",
                                             g_strdup("Bottom to top"));
    }

    /*
    if ((p = g_hash_table_lookup(data->meta, "collect_mode"))) {
        if (!strcmp(p, "1"))
            gwy_container_set_string_by_name(meta, "Line direction",
                                             g_strdup("Left to right"));
        else if (!strcmp(p, "2"))
            gwy_container_set_string_by_name(meta, "Line direction",
                                             g_strdup("Right to left"));
    }
    */

    if (gwy_container_get_n_items(meta)) {
        g_string_printf(str, "/%d/meta", id);
        gwy_container_set_object_by_name(container, str->str, meta);
    }
    g_object_unref(meta);

    g_string_free(str, TRUE);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
