/*
 *  $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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
 * <mime-type type="application/x-omicron-flat">
 *   <comment>Omicron flat file format</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="FLAT0100"/>
 *   </magic>
 *   <glob pattern="*.*_flat"/>
 *   <glob pattern="*.*_FLAT"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # Omicron flat data format.
 * 0 string FLAT0100 Omicron flat SPM data
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Omicron flat format
 * .*_flat
 * Read
 **/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <libprocess/dataline.h>
#include <libprocess/spectra.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define MAGIC "FLAT"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define FLAT_VERSION "0100"
#define FLAT_VERSION_SIZE (sizeof(FLAT_VERSION)-1)

#define EXTENSION "_flat"

#define STRING_MAXLENGTH 10000

#ifndef NAN
#define NAN (0.0/0.0)
#endif

typedef enum {
    OMICRON_VALUE_32BIT = 1,
    OMICRON_VALUE_DOUBLE = 2,
    OMICRON_VALUE_BOOLEAN = 3,
    OMICRON_VALUE_ENUM = 4,
    OMICRON_VALUE_STRING = 5,
} OmicronValueType;

typedef enum {
    OMICRON_VIEW_OTHER = 0,
    OMICRON_VIEW_SIMPLE_2D = 1,             /* Unused? */
    OMICRON_VIEW_SIMPLE_1D = 2,
    OMICRON_VIEW_FORWARD_BACKWARD_2D = 3,   /* Topography */
    OMICRON_VIEW_2D_OF_3D = 4,              /* Volume data */
    OMICRON_VIEW_SPECTROSCOPY = 5,
    OMICRON_VIEW_FORCE_CURVE = 6,
    OMICRON_VIEW_1D_PROFILE = 7,            /* Also atom manipulation path */
    OMICRON_VIEW_INTERFEROMETER = 8,
    OMICRON_VIEW_CONTINUOUS_CURVE = 9,      /* Seems often time dependency */
    OMICRON_VIEW_PHASE_AMPLITUDE_CURVE = 10,
    OMICRON_VIEW_CURVE_SET = 11,
    OMICRON_VIEW_PARAMETERISED_CURVE_SET = 12,
    OMICRON_VIEW_DISCRETE_ENERGY_MAP = 13,
    OMICRON_VIEW_ESP_IMAGE_MAP = 14,
    OMICRON_VIEW_DOWNWARD_2D = 15,
} OmicronViewType;

typedef struct {
    gchar *filename;    /* Full file name, useful for actualy loading. */
    gchar *stem;        /* The common part up to "--". */
    gchar *number;      /* The 123_4 part between "--" and the first dot. */
    gchar *extension;   /* The extension part between dot and "_flat" */
} OmicronFlatFileId;

typedef struct {
    guchar magic[4];
    guchar file_structure_level[4];
} OmicronFlatIdentification;

typedef struct {
    gint start_clock;
    gint stop_clock;
    gint step;
} OmicronFlatInterval;

typedef struct {
    gchar *name;
    guint interval_count;
    OmicronFlatInterval *intervals;
} OmicronFlatTableSet;

typedef struct {
    gchar *name;
    gchar *parent_name;
    gchar *unit_name;
    guint clock_count;   /* Already including mirroring for non-table-set. */
    gint raw_start_value;
    gint raw_increment;
    gdouble physical_start_value;
    gdouble physical_increment;
    gboolean is_mirrored;
    guint table_set_count;
    OmicronFlatTableSet *table_set_fields;
    /* Derived data */
    OmicronFlatTableSet *table_set_ref;
    guint single_data_len;
    guint mirror_mult;
    const gchar *unq_name;   /* Unquallified name. */
} OmicronFlatAxis;

typedef struct {
    gchar *name;
    gdouble value;
} OmicronFlatTransferParam;

typedef struct {
    gchar *name;
    gchar *transfer_func_name;
    gchar *unit_name;
    guint parameter_count;
    OmicronFlatTransferParam *parameters;
    guint data_view_type_count;
    OmicronViewType *view_types;
    /* Derived data */
    gdouble q;
    gdouble z0;
} OmicronFlatChannel;

typedef struct {
    guint64 timestamp;
    gchar *info;
} OmicronFlatCreation;

typedef struct {
    guint full_size;
    guint actual_item_count;
    const guchar *data;
} OmicronFlatRawData;

typedef struct {
    gchar *name;
    gchar *version;
    gchar *description;
    gchar *file_spec;
    gchar *file_creator_id;
    gchar *result_file_creator_id;
    gchar *user_name;
    gchar *account_name;
    gchar *result_data_file_spec;
    guint run_cycle_id;
    guint scan_cycle_id;
} OmicronFlatExperiment;

typedef struct {
    gchar *name;
    OmicronValueType value_type;
    gchar *unit;
    gchar *value;
} OmicronFlatExperimentParam;

typedef struct {
    gchar *name;
    guint parameter_count;
    OmicronFlatExperimentParam *parameters;
} OmicronFlatExperimentParamInstance;

typedef struct {
    gchar *name;
    gchar *value;
} OmicronFlatDeploymentParam;

typedef struct {
    gchar *name;
    guint parameter_count;
    OmicronFlatDeploymentParam *parameters;
} OmicronFlatDeploymentParamInstance;

typedef struct {
    OmicronFlatIdentification identification;
    guint axis_count;
    OmicronFlatAxis *axis_descriptions;
    OmicronFlatChannel channel;
    OmicronFlatCreation creation;
    OmicronFlatRawData raw_data;
    guint offset_count;
    gdouble *offsets;
    OmicronFlatExperiment experiment;
    guint exp_instance_count;
    OmicronFlatExperimentParamInstance *exp_instances;
    guint depl_instance_count;
    OmicronFlatDeploymentParamInstance *depl_instances;
    /* Derived data */
    const gchar *filename;
    guint single_data_len;
    guint mirror_mult;
    /* Raw file contents. */
    guchar *buffer;
    gsize size;
} OmicronFlatFile;

typedef struct {
    guint nfiles;
    OmicronFlatFileId *ids;
    OmicronFlatFile **files;
    /* Kluge for atom manipulation path. */
    GArray *offsets;
} OmicronFlatFileList;

static gboolean         module_register                (void);
static gint             omicronflat_detect             (const GwyFileDetectInfo *fileinfo,
                                                        gboolean only_name);
static GwyContainer*    omicronflat_load               (const gchar *filename,
                                                        GwyRunType mode,
                                                        GError **error);
static OmicronFlatFile* omicronflat_load_single        (const gchar *filename,
                                                        GError **error);
static void             gather_offsets                 (GArray *offsets,
                                                        OmicronFlatFile *fff);
static void             free_file                      (OmicronFlatFile *fff);
static void             free_file_id                   (OmicronFlatFileId *id);
static void             remove_from_filelist           (OmicronFlatFileList *filelist,
                                                        guint fileid);
static gboolean         find_related_files             (const gchar *filename,
                                                        OmicronFlatFileList *filelist,
                                                        GError **error);
static GwyContainer*    construct_metadata             (OmicronFlatFile *fff,
                                                        OmicronFlatFileId *id);
static gboolean         load_as_channel                (OmicronFlatFileList *filelist,
                                                        guint fileid,
                                                        GwyContainer *data,
                                                        gint *id);
static gboolean         load_as_curve                  (OmicronFlatFileList *filelist,
                                                        guint fileid,
                                                        GwyContainer *data,
                                                        gint *id);
static gboolean         load_as_sps                    (OmicronFlatFileList *filelist,
                                                        guint fileid,
                                                        GwyContainer *data,
                                                        gint *id);
static gboolean         load_as_volume                 (OmicronFlatFileList *filelist,
                                                        guint fileid,
                                                        GwyContainer *data,
                                                        gint *id);
static void             construct_axis_range           (const OmicronFlatAxis *axis,
                                                        guint interval_id,
                                                        gdouble *real,
                                                        gdouble *offset,
                                                        guint *n);
static gdouble*         construct_axis_xdata           (const OmicronFlatAxis *axis,
                                                        guint interval_id,
                                                        guint *n);
static gboolean         read_identification            (const guchar **p,
                                                        gsize *size,
                                                        OmicronFlatIdentification *identification,
                                                        GError **error);
static gboolean         read_axis_hierarchy_description(const guchar **p,
                                                        gsize *size,
                                                        OmicronFlatFile *fff,
                                                        GError **error);
static gboolean         read_channel_description       (const guchar **p,
                                                        gsize *size,
                                                        OmicronFlatChannel *channel,
                                                        GError **error);
static gboolean         read_creation_information      (const guchar **p,
                                                        gsize *size,
                                                        OmicronFlatCreation *creation,
                                                        GError **error);
static gboolean         read_raw_data                  (const guchar **p,
                                                        gsize *size,
                                                        OmicronFlatRawData *raw_data,
                                                        GError **error);
static gboolean         read_offsets                   (const guchar **p,
                                                        gsize *size,
                                                        OmicronFlatFile *fff,
                                                        GError **error);
static gboolean         read_experiment_information    (const guchar **p,
                                                        gsize *size,
                                                        OmicronFlatExperiment *experiment,
                                                        GError **error);
static gboolean         read_experiment_parameters     (const guchar **p,
                                                        gsize *size,
                                                        OmicronFlatFile *fff,
                                                        GError **error);
static gboolean         read_deployment_parameters     (const guchar **p,
                                                        gsize *size,
                                                        OmicronFlatFile *fff,
                                                        GError **error);
static gboolean         read_uint32                    (const guchar **p,
                                                        gsize *size,
                                                        guint *v,
                                                        GError **error);
static gboolean         read_sint32                    (const guchar **p,
                                                        gsize *size,
                                                        gint *v,
                                                        GError **error);
static gboolean         read_uint64                    (const guchar **p,
                                                        gsize *size,
                                                        guint64 *v,
                                                        GError **error);
static gboolean         read_double                    (const guchar **p,
                                                        gsize *size,
                                                        gdouble *v,
                                                        GError **error);
static gboolean         read_string                    (const guchar **p,
                                                        gsize *size,
                                                        gchar **v,
                                                        GError **error);
static void             err_UNKNOWN_DATA_TYPE          (GError **error,
                                                        const OmicronFlatFile *fff);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Omicron flat files."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("omicronflat",
                           N_("Omicron flat files (*.*_flat)"),
                           (GwyFileDetectFunc)&omicronflat_detect,
                           (GwyFileLoadFunc)&omicronflat_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
omicronflat_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    /* If we decide to try reading files with different values of the version,
     * modify this accordingly. */
    if (fileinfo->buffer_len > MAGIC_SIZE + FLAT_VERSION_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0
        && memcmp(fileinfo->head + MAGIC_SIZE,
                  FLAT_VERSION, FLAT_VERSION_SIZE) == 0)
        score = 100;

    return score;
}

static gboolean
filenames_match(const gchar *a, const gchar *b)
{
    gchar *ca = gwy_canonicalize_path(a);
    gchar *cb = gwy_canonicalize_path(b);
    gboolean names_match = gwy_strequal(a, b);

    g_free(ca);
    g_free(cb);
    return names_match;
}

/* XXX: Error reporting is convoluted.  Report an error when the file the user
 * actually requested cannot be loaded.  This permits useful feedback for data
 * that cannot be loaded but does not produce errors otherwise. */
static GwyContainer*
omicronflat_load(const gchar *filename,
                 G_GNUC_UNUSED GwyRunType mode,
                 GError **error)
{
    OmicronFlatFileList filelist = { 0, NULL, NULL, NULL };
    GwyContainer *data = NULL;
    guint i, id;

    /* Try to load all related files. */
    if (!find_related_files(filename, &filelist, error))
        return NULL;

    filelist.files = g_new0(OmicronFlatFile*, filelist.nfiles);
    for (i = 0; i < filelist.nfiles; i++) {
        const gchar *fnm = filelist.ids[i].filename;
        GError *err = NULL;

        filelist.files[i] = omicronflat_load_single(fnm, &err);
        /* Fail if the file that cannot be loaded matches the requested file. */
        if (!filelist.files[i]) {
            gwy_debug("Could not read %s: %s", fnm, err->message);
            if (filenames_match(fnm, filename)) {
                g_propagate_error(error, err);
                goto fail;
            }
            g_clear_error(&err);
        }
    }

    /* Discard any other files we could not load physically. */
    i = 0;
    while (i < filelist.nfiles) {
        if (filelist.files[i])
            i++;
        else
            remove_from_filelist(&filelist, i);
    }
    /* We must have loaded at least the requested file. */
    g_assert(filelist.nfiles);

    /* Gather offsets to create selections for atom manipulation paths. */
    filelist.offsets = g_array_new(FALSE, FALSE, 4*sizeof(gdouble));
    for (i = 0; i < filelist.nfiles; i++)
        gather_offsets(filelist.offsets, filelist.files[i]);

    /* Try to acually load the data. */
    data = gwy_container_new();

    /* Channels. They go by a single filelist entry. */
    i = id = 0;
    while (i < filelist.nfiles) {
        if (!load_as_channel(&filelist, i, data, &id))
            i++;
    }

    /* Curves (graphs). They go by a single filelist entry. */
    i = 0;
    id = 1;    /* Graphs start from 1. */
    while (i < filelist.nfiles) {
        if (!load_as_curve(&filelist, i, data, &id))
            i++;
    }

    /* SPS. They can consume multiple filelist entries. */
    i = id = 0;
    while (i < filelist.nfiles) {
        if (!load_as_sps(&filelist, i, data, &id))
            i++;
    }

    /* Volume. They go by a single filelist entry. */
    i = id = 0;
    while (i < filelist.nfiles) {
        if (!load_as_volume(&filelist, i, data, &id))
            i++;
    }

    /* The filelist now contain data types we do not know how to load.  Again,
     * if the requested file is one of them, fail and return an error.
     * Otherwise pretend everything is fine. */
    for (i = 0; i < filelist.nfiles; i++) {
        if (filenames_match(filelist.ids[i].filename, filename)) {
            err_UNKNOWN_DATA_TYPE(error, filelist.files[i]);
            gwy_object_unref(data);
            goto fail;
        }
    }

fail:
    while (filelist.nfiles)
        remove_from_filelist(&filelist, filelist.nfiles-1);
    g_free(filelist.files);
    g_free(filelist.ids);
    if (filelist.offsets)
        g_array_free(filelist.offsets, TRUE);

    return data;
}

static OmicronFlatFile*
omicronflat_load_single(const gchar *filename,
                        GError **error)
{
    OmicronFlatFile *fff = NULL;
    guchar* buffer = NULL;
    const guchar *p;
    gsize size, remsize, expected;
    GError *err = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    remsize = size;
    p = buffer;

    fff = g_new0(OmicronFlatFile, 1);
    fff->filename = filename;
    fff->buffer = buffer;
    fff->size = size;

    if (!read_identification(&p, &remsize, &fff->identification, error)
        || !read_axis_hierarchy_description(&p, &remsize, fff, error)
        || !read_channel_description(&p, &remsize, &fff->channel, error)
        || !read_creation_information(&p, &remsize, &fff->creation, error)
        || !read_raw_data(&p, &remsize, &fff->raw_data, error)
        || !read_offsets(&p, &remsize, fff, error)
        || !read_experiment_information(&p, &remsize, &fff->experiment, error)
        || !read_experiment_parameters(&p, &remsize, fff, error)
        || !read_deployment_parameters(&p, &remsize, fff, error))
        goto fail;

    /* Verify the data size is a multiple of mirror_mult. */
    expected = (fff->single_data_len/fff->mirror_mult)*fff->mirror_mult;
    if (err_SIZE_MISMATCH(error,
                          expected*sizeof(gint32),
                          fff->raw_data.full_size*sizeof(gint32),
                          TRUE))
        goto fail;

    gwy_debug("pos %lu, remaining size %lu",
              (gulong)(p-buffer), (gulong)remsize);
    return fff;

fail:
    free_file(fff);
    return NULL;
}

static void
free_file(OmicronFlatFile *fff)
{
    guint i, j;

    if (!fff)
        return;

    for (i = 0; i < fff->axis_count; i++) {
        OmicronFlatAxis *axis = fff->axis_descriptions + i;
        g_free(axis->name);
        g_free(axis->parent_name);
        g_free(axis->unit_name);
        for (j = 0; j < axis->table_set_count; j++) {
            OmicronFlatTableSet *table_set = axis->table_set_fields + j;
            g_free(table_set->name);
            g_free(table_set->intervals);
        }
        g_free(axis->table_set_fields);
    }
    g_free(fff->axis_descriptions);

    g_free(fff->channel.name);
    g_free(fff->channel.transfer_func_name);
    g_free(fff->channel.unit_name);
    for (i = 0; i < fff->channel.parameter_count; i++) {
        OmicronFlatTransferParam *param = fff->channel.parameters + i;
        g_free(param->name);
    }
    g_free(fff->channel.parameters);
    g_free(fff->channel.view_types);

    g_free(fff->creation.info);
    g_free(fff->offsets);

    g_free(fff->experiment.name);
    g_free(fff->experiment.version);
    g_free(fff->experiment.description);
    g_free(fff->experiment.file_spec);
    g_free(fff->experiment.file_creator_id);
    g_free(fff->experiment.result_file_creator_id);
    g_free(fff->experiment.user_name);
    g_free(fff->experiment.account_name);
    g_free(fff->experiment.result_data_file_spec);

    for (i = 0; i < fff->exp_instance_count; i++) {
        OmicronFlatExperimentParamInstance *instance = fff->exp_instances + i;
        g_free(instance->name);
        for (j = 0; j < instance->parameter_count; j++) {
            OmicronFlatExperimentParam *param = instance->parameters + j;
            g_free(param->name);
            g_free(param->unit);
            g_free(param->value);
        }
        g_free(instance->parameters);
    }
    g_free(fff->exp_instances);

    for (i = 0; i < fff->depl_instance_count; i++) {
        OmicronFlatDeploymentParamInstance *instance = fff->depl_instances + i;
        g_free(instance->name);
        for (j = 0; j < instance->parameter_count; j++) {
            OmicronFlatDeploymentParam *param = instance->parameters + j;
            g_free(param->name);
            g_free(param->value);
        }
        g_free(instance->parameters);
    }
    g_free(fff->depl_instances);

    if (fff->buffer)
        gwy_file_abandon_contents(fff->buffer, fff->size, NULL);

    g_free(fff);
}

/* Checks if the specified view is present. */
static gboolean
has_view(const OmicronFlatFile *fff, OmicronViewType vt)
{
    guint i;

    for (i = 0; i < fff->channel.data_view_type_count; i++) {
        if (vt == fff->channel.view_types[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
has_axes(const OmicronFlatFile *fff, guint naxes, ...)
{
    va_list ap;
    guint i, j;

    if (fff->axis_count != naxes)
        return FALSE;

    va_start(ap, naxes);
    for (i = 0; i < naxes; i++) {
        const gchar *name = va_arg(ap, const gchar*);
        guint *axisid = va_arg(ap, guint*);
        gboolean found = FALSE;

        for (j = 0; j < fff->axis_count; j++) {
            if (gwy_strequal(fff->axis_descriptions[j].unq_name, name)) {
                *axisid = j;
                found = TRUE;
                break;
            }
        }
        if (!found)
            return FALSE;
    }
    va_end(ap);

    return TRUE;
}

static void
gather_offsets(GArray *offsets, OmicronFlatFile *fff)
{
    gdouble *myoff;
    guint i, j;
    gdouble off[4];

    /* There must be one axis. */
    if (fff->axis_count != 1)
        return;

    /* The view type should be some of the plain graph curve types. */
    if (!has_view(fff, OMICRON_VIEW_CONTINUOUS_CURVE)
        && !has_view(fff, OMICRON_VIEW_1D_PROFILE)
        && !has_view(fff, OMICRON_VIEW_INTERFEROMETER)
        && !has_view(fff, OMICRON_VIEW_FORCE_CURVE))
        return;

    if (fff->offset_count % 2)
        return;

    myoff = (gdouble*)offsets->data;
    for (j = 0; j < fff->offset_count/2; j++) {
        gboolean already_have = FALSE;

        off[0] = fff->offsets[4*j + 0];
        off[1] = fff->offsets[4*j + 1];
        off[2] = fff->offsets[4*j + 2];
        off[3] = fff->offsets[4*j + 3];

        for (i = 0; i < offsets->len; i++) {
            if (off[0] == myoff[4*i + 0] && off[1] == myoff[4*i + 1]
                && off[2] == myoff[4*i + 2] && off[3] == myoff[4*i + 3]) {
                already_have = TRUE;
                break;
            }
        }
        if (!already_have) {
            g_array_append_val(offsets, off);
            myoff = (gdouble*)offsets->data;
        }
    }
}

static gboolean
load_as_channel(OmicronFlatFileList *filelist, guint fileid,
                GwyContainer *data, gint *id)
{
    struct {
        GwyDataField *dfield;
        gboolean mirrorx;
        gboolean mirrory;
        const gchar *title;
    }
    field_specs[4] = {
        { NULL, FALSE, TRUE, "Trace Up" },
        { NULL, TRUE, TRUE, "reTrace Up" },
        { NULL, FALSE, FALSE, "Trace Down" },
        { NULL, TRUE, FALSE, "reTrace Down" },
    };

    OmicronFlatFile *fff = filelist->files[fileid];
    OmicronFlatAxis *axisx, *axisy;
    guint idx, idy, xres, yres, i, nitems, fid, nfields = 1;
    gdouble dx, dy;
    const gint32 *d32;

    /* There must be two axes, X and Y.   At this moment we require X to be
     * the trigger axis. */
    if (!has_axes(fff, 2, "X", &idx, "Y", &idy) || idx != 0 || idy != 1)
        return FALSE;

    axisx = fff->axis_descriptions + idx;
    axisy = fff->axis_descriptions + idy;

    /* The view type should be vtc_ForwardBackward2D */
    if (!has_view(fff, OMICRON_VIEW_FORWARD_BACKWARD_2D))
        return FALSE;

    /* We cannot deal with table sets here. */
    if (axisx->table_set_ref || axisy->table_set_ref)
        return FALSE;

    gwy_debug("file %s seems to be 2D image", fff->filename);
    if (axisx->is_mirrored)
        nfields = axisy->is_mirrored ? 4 : 2;
    else if (axisy->is_mirrored) {
        field_specs[1] = field_specs[2];
        nfields = 2;
    }

    xres = axisx->clock_count/axisx->mirror_mult;
    yres = axisy->clock_count/axisy->mirror_mult;
    dx = axisx->physical_increment;
    dy = axisy->physical_increment;
    for (i = 0; i < nfields; i++) {
        field_specs[i].dfield = gwy_data_field_new(xres, yres, xres*dx, yres*dy,
                                                   FALSE);
        gwy_data_field_fill(field_specs[i].dfield, NAN);
        gwy_data_field_invalidate(field_specs[i].dfield);
        gwy_debug("%u (%s)", i, field_specs[i].title);
    }

    d32 = (const gint32*)fff->raw_data.data;
    nitems = fff->raw_data.actual_item_count;
    fid = i = 0;
    while (nitems) {
        guint rowlen = (nitems >= xres) ? xres : nitems;
        gwy_convert_raw_data(d32, rowlen, 1,
                             GWY_RAW_DATA_SINT32, GWY_BYTE_ORDER_LITTLE_ENDIAN,
                             field_specs[fid].dfield->data + i*xres,
                             fff->channel.q, fff->channel.z0);
        /* When the fast axis is mirrored the rows are interlaced so put them
         * alternately to two fields. */
        if (axisx->is_mirrored) {
            fid ^= 1;
            if (!(fid & 1)) {
                i++;
                if (i == yres) {
                    i = 0;
                    fid += 2;
                }
            }
        }
        else {
            i++;
            if (i == yres) {
                i = 0;
                fid++;
            }
        }
        d32 += rowlen;
        nitems -= rowlen;
    }

    for (i = 0; i < nfields; i++) {
        GwyDataField *dfield = field_specs[i].dfield, *mask;
        GwyContainer *meta;
        gchar *title;
        gchar key[40];

        /* gwy_data_field_invert() has a strange XY convention. */
        gwy_data_field_invert(dfield,
                              field_specs[i].mirrory, field_specs[i].mirrorx,
                              FALSE);
        gwy_data_field_set_xoffset(dfield, axisx->physical_start_value);
        gwy_data_field_set_yoffset(dfield, axisy->physical_start_value);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield),
                                    axisx->unit_name);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield),
                                    fff->channel.unit_name);

        g_snprintf(key, sizeof(key), "/%i/data", *id);
        gwy_container_set_object_by_name(data, key, dfield);
        g_object_unref(dfield);

        g_snprintf(key, sizeof(key), "/%i/data/title", *id);
        title = g_strconcat(fff->channel.name, " ", field_specs[i].title,
                            NULL);
        gwy_container_set_string_by_name(data, key, title);

        meta = construct_metadata(fff, filelist->ids + fileid);
        g_snprintf(key, sizeof(key), "/%i/meta", *id);
        gwy_container_set_object_by_name(data, key, meta);
        g_object_unref(meta);

        if ((mask = gwy_app_channel_mask_of_nans(dfield, TRUE))) {
            GQuark quark = gwy_app_get_mask_key_for_id(*id);

            gwy_container_set_object(data, quark, mask);
            g_object_unref(mask);
        }

        if (filelist->offsets->len) {
            GType gtype = g_type_from_name("GwySelectionLine");
            GwySelection *selection;

            /* TODO: Create line selection from offsets. */
        }

        gwy_file_channel_import_log_add(data, *id, NULL, fff->filename);
        (*id)++;
    }

    remove_from_filelist(filelist, fileid);

    return TRUE;
}

static gboolean
load_as_curve(OmicronFlatFileList *filelist, guint fileid,
              GwyContainer *data, gint *id)
{
    static const gchar *titles[3] = { "Trace", "reTrace", "Other" };

    OmicronFlatFile *fff = filelist->files[fileid];
    OmicronFlatAxis *axis;
    GwyGraphModel *gmodel;
    GwySIUnit *xunit, *yunit;
    guint res, i, j, nitems, ncurves;
    const gint32 *d32;

    /* There must be one axis. */
    if (fff->axis_count != 1)
        return FALSE;
    axis = fff->axis_descriptions + 0;

    /* The view type should be some of the plain graph curve types. */
    if (!has_view(fff, OMICRON_VIEW_CONTINUOUS_CURVE)
        && !has_view(fff, OMICRON_VIEW_1D_PROFILE)
        && !has_view(fff, OMICRON_VIEW_INTERFEROMETER)
        && !has_view(fff, OMICRON_VIEW_FORCE_CURVE))
        return FALSE;

    gwy_debug("file seems to be graph curves");
    ncurves = (axis->table_set_ref
               ? axis->table_set_ref->interval_count
               : axis->mirror_mult);

    gmodel = gwy_graph_model_new();
    res = axis->clock_count/axis->mirror_mult;
    d32 = (const gint32*)fff->raw_data.data;
    nitems = fff->raw_data.actual_item_count;
    i = 0;
    for (i = 0; nitems && i < ncurves; i++) {
        GwyGraphCurveModel *gcmodel = gwy_graph_curve_model_new();
        guint rowlen = (nitems >= res) ? res : nitems;
        guint n = 0;
        gdouble *xdata, *ydata;

        xdata = construct_axis_xdata(axis, i, &n);
        g_assert(n >= rowlen);
        ydata = g_new0(gdouble, n);
        gwy_convert_raw_data(d32, rowlen, 1,
                             GWY_RAW_DATA_SINT32, GWY_BYTE_ORDER_LITTLE_ENDIAN,
                             ydata, fff->channel.q, fff->channel.z0);
        d32 += rowlen;

        /* XXX: Assume the data are mirrored for the other interval(s).
         * There does not seem to be any way to actually tell for axes with
         * table sets. */
        if (i) {
            for (j = 0; j < n/2; j++)
                GWY_SWAP(gdouble, ydata[j], ydata[n-1-j]);
        }

        /* Set the short item count, not n. */
        gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, rowlen);
        g_free(xdata);
        g_free(ydata);

        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", gwy_graph_get_preset_color(i),
                     "description", titles[MIN(i, G_N_ELEMENTS(titles))],
                     NULL);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }

    xunit = gwy_si_unit_new(axis->unit_name);
    yunit = gwy_si_unit_new(fff->channel.unit_name);
    g_object_set(gmodel,
                 "axis-label-left", fff->channel.name,
                 "axis-label-bottom", axis->unq_name,
                 "title", fff->channel.name,
                 "si-unit-x", xunit,
                 "si-unit-y", yunit,
                 NULL);
    g_object_unref(xunit);
    g_object_unref(yunit);

    gwy_container_set_object(data,
                             gwy_app_get_graph_key_for_id(*id), gmodel);
    g_object_unref(gmodel);

    remove_from_filelist(filelist, fileid);
    (*id)++;

    return TRUE;
}

/* XXX: I do not have any file with mirrored axis. */
static gboolean
load_as_sps(OmicronFlatFileList *filelist,
            guint fileid,
            GwyContainer *data,
            gint *id)
{
    /*static const gchar *titles[2] = { "Trace", "reTrace" };*/

    OmicronFlatFile *fff = filelist->files[fileid];
    GwySpectra **allspectra;
    OmicronFlatAxis *axis;
    guint res, i, nitems, nsps, nfiles;
    gdouble step, xoff, yoff;
    const gint32 *d32;

    /* There must be one axis, usually V. */
    if (fff->axis_count != 1)
        return FALSE;
    axis = fff->axis_descriptions + 0;

    /* The view type should be vtc_Spectroscopy */
    if (!has_view(fff, OMICRON_VIEW_SPECTROSCOPY))
        return FALSE;

    /* We cannot deal with table sets here. */
    if (axis->table_set_ref)
        return FALSE;

    gwy_debug("file seems to be single point spectroscopy");
    nsps = axis->mirror_mult;
    allspectra = g_new0(GwySpectra*, nsps);

    /* Find all spetrum curves with the same extension, i.e. type of spectrum,
     * and put them to the same Spectra object. */
    nfiles = 0;
    do {
        fff = filelist->files[fileid + nfiles];

        /* XXX: This is desperate. */
        gwy_debug("sps curve file %s", fff->filename);
        if (fff->axis_count != 1)
            break;

        axis = fff->axis_descriptions + 0;

        /* The view type should be vtc_Spectroscopy */
        if (!has_view(fff, OMICRON_VIEW_SPECTROSCOPY))
            break;

        /* We cannot deal with table sets here. */
        if (axis->table_set_ref)
            break;

        nfiles++;

        gwy_debug("offset_count %u", fff->offset_count);
        if (fff->offset_count) {
            xoff = fff->offsets[0];
            /* FIXME: What is the proper way of correcting the inversion
             * of vertical coordinate?  We would need to know the centre
             * of the image data here!  They seem to be typically (0,0)
             * though.  */
            yoff = -fff->offsets[1];
        }
        else {
            g_warning("SPS needs an offset to position the curve correctly.");
            xoff = yoff = 0.0;
        }

        res = axis->clock_count/axis->mirror_mult;
        step = axis->physical_increment;
        d32 = (const gint32*)fff->raw_data.data;
        nitems = fff->raw_data.actual_item_count;
        i = 0;
        for (i = 0; nitems && i < nsps; i++) {
            GwySpectra *spectra;
            guint rowlen = (nitems >= res) ? res : nitems;
            GwyDataLine *dline = gwy_data_line_new(rowlen, rowlen*step, FALSE);

            if (!allspectra[i]) {
                spectra = allspectra[i] = gwy_spectra_new();

                gwy_si_unit_set_from_string(gwy_spectra_get_si_unit_xy(spectra),
                                            "m");
                gwy_spectra_set_title(spectra, fff->channel.name);
                gwy_spectra_set_spectrum_x_label(spectra, axis->unq_name);
                gwy_spectra_set_spectrum_y_label(spectra, fff->channel.name);
            }
            else
                spectra = allspectra[i];

            gwy_data_line_set_offset(dline, axis->physical_start_value);
            gwy_si_unit_set_from_string(gwy_data_line_get_si_unit_x(dline),
                                        axis->unit_name);
            gwy_si_unit_set_from_string(gwy_data_line_get_si_unit_y(dline),
                                        fff->channel.unit_name);

            gwy_convert_raw_data(d32, rowlen, 1,
                                 GWY_RAW_DATA_SINT32,
                                 GWY_BYTE_ORDER_LITTLE_ENDIAN,
                                 dline->data, fff->channel.q, fff->channel.z0);
            d32 += rowlen;
            if (i)
                gwy_data_line_invert(dline, TRUE, FALSE);

            gwy_spectra_add_spectrum(spectra, dline, xoff, yoff);
            g_object_unref(dline);
        }
    } while (fileid + nfiles < filelist->nfiles
             && gwy_strequal(filelist->ids[fileid].extension,
                             filelist->ids[fileid + nfiles].extension));

    gwy_debug("merged sps nfiles %u", nfiles);
    for (i = 0; i < nsps; i++) {
        GQuark quark = gwy_app_get_spectra_key_for_id(*id);
        gwy_container_set_object(data, quark, allspectra[i]);
        g_object_unref(allspectra[i]);
        (*id)++;
    }
    g_free(allspectra);

    for (i = 0; i < nfiles; i++)
        remove_from_filelist(filelist, fileid);

    return TRUE;
}

static gboolean
load_as_volume(OmicronFlatFileList *filelist,
               guint fileid,
               GwyContainer *data,
               gint *id)
{
    struct {
        GwyBrick *brick;
        gboolean mirrorz;
        gboolean mirrorx;
        gboolean mirrory;
        const gchar *title;
    }
    brick_specs[8] = {
        { NULL, FALSE, FALSE, TRUE, "Trace Up Forward" },
        { NULL, TRUE, FALSE, TRUE, "Trace Up Backward" },
        { NULL, FALSE, TRUE, TRUE, "reTrace Up Forward" },
        { NULL, TRUE, TRUE, TRUE, "reTrace Up Backward" },
        { NULL, FALSE, FALSE, FALSE, "Trace Down Forward" },
        { NULL, TRUE, FALSE, FALSE, "Trace Down Backward" },
        { NULL, FALSE, TRUE, FALSE, "reTrace Down Forward" },
        { NULL, TRUE, TRUE, FALSE, "reTrace Down Backward" },
    };

    OmicronFlatFile *fff = filelist->files[fileid];
    OmicronFlatAxis *axisx, *axisy, *axisz;
    guint idx, idy, idz, xres, yres, zres, i, j, k, b, nitems, bid;
    gdouble xreal, yreal, zreal, xoff, yoff, zoff, q, z0;
    gboolean mirror_count[3], mirror_state[3];
    const gint32 *d32;

    /* There must be three axes, X, Y and something.   The third axis must
     * be the trigger axis. */
    if (fff->axis_count != 3)
        return FALSE;
    axisz = fff->axis_descriptions + 0;
    if (!has_axes(fff, 3, "X", &idx, "Y", &idy, axisz->unq_name, &idz)
        || idx != 1 || idy != 2 || idz != 0)
        return FALSE;

    axisx = fff->axis_descriptions + idx;
    axisy = fff->axis_descriptions + idy;

    /* The view type should be vtc_2D_Of_3D, occasionally vtc_Spectroscopy. */
    if (!has_view(fff, OMICRON_VIEW_2D_OF_3D)
        && !has_view(fff, OMICRON_VIEW_SPECTROSCOPY))
        return FALSE;

    gwy_debug("file %s seems to be volume data", fff->filename);
    mirror_count[0] = (axisz->table_set_ref
                       ? axisz->table_set_ref->interval_count
                       : axisz->mirror_mult);
    mirror_count[1] = (axisx->table_set_ref
                       ? axisx->table_set_ref->interval_count
                       : axisx->mirror_mult);
    mirror_count[2] = (axisy->table_set_ref
                       ? axisy->table_set_ref->interval_count
                       : axisy->mirror_mult);
    mirror_state[0] = mirror_state[1] = mirror_state[2] = 0;
    gwy_debug("%u %u %u", mirror_count[0], mirror_count[1], mirror_count[2]);
    bid = 0;
    do {
        GwyBrick *brick;

        construct_axis_range(axisx, mirror_state[idx], &xreal, &xoff, &xres);
        construct_axis_range(axisy, mirror_state[idy], &yreal, &yoff, &yres);
        construct_axis_range(axisz, mirror_state[idz], &zreal, &zoff, &zres);
        brick_specs[bid].brick = brick = gwy_brick_new(xres, yres, zres,
                                                       xreal, yreal, zreal,
                                                       TRUE);
        gwy_debug("xreal %g, yreal %g, zreal %g", xreal, yreal, zreal);
        gwy_debug("xoff %g, yoff %g, zoff %g", xoff, yoff, zoff);
        gwy_brick_set_xoffset(brick_specs[bid].brick, xoff);
        gwy_brick_set_yoffset(brick_specs[bid].brick, yoff);
        gwy_brick_set_zoffset(brick_specs[bid].brick, zoff);
        gwy_si_unit_set_from_string(gwy_brick_get_si_unit_x(brick),
                                    axisx->unit_name);
        gwy_si_unit_set_from_string(gwy_brick_get_si_unit_y(brick),
                                    axisy->unit_name);
        gwy_si_unit_set_from_string(gwy_brick_get_si_unit_z(brick),
                                    axisz->unit_name);
        gwy_si_unit_set_from_string(gwy_brick_get_si_unit_w(brick),
                                    fff->channel.unit_name);
        /* gwy_brick_invalidate(brick_specs[bid].brick); */
        gwy_debug("%u (%s)", bid, brick_specs[bid].title);

        for (b = 0; b < 3; b++) {
            if (++mirror_state[b] < mirror_count[b])
                break;
            mirror_state[b] = 0;
        }
        bid = 0;
        for (b = 0; b < 3; b++) {
            if (mirror_state[b])
                bid |= (1 << b);
        }
    } while (bid);

    d32 = (const gint32*)fff->raw_data.data;
    nitems = fff->raw_data.actual_item_count;
    q = fff->channel.q;
    z0 = fff->channel.z0;
    bid = i = j = 0;
    mirror_state[0] = mirror_state[1] = mirror_state[2] = 0;
    while (nitems) {
        gdouble *d = brick_specs[bid].brick->data;
        guint rowlen;

        xres = brick_specs[bid].brick->xres;
        yres = brick_specs[bid].brick->yres;
        zres = brick_specs[bid].brick->zres;
        rowlen = (nitems >= zres) ? zres : nitems;

        d += (brick_specs[bid].mirrorx ? xres-1 - j : j);
        d += xres*(brick_specs[bid].mirrory ? yres-1 - i : i);

        /* Read one spectrum. */
        if (mirror_state[0]) {
            d += xres*yres*(zres - 1);
            for (k = 0; k < rowlen; k++) {
                *d = q*GINT32_FROM_LE(d32[k]) + z0;
                d -= xres*yres;
            }
        }
        else {
            for (k = 0; k < rowlen; k++) {
                *d = q*GINT32_FROM_LE(d32[k]) + z0;
                d += xres*yres;
            }
        }

        /* Advance to the right pixel and right field for the next spectrum. */
        if (++mirror_state[0] == mirror_count[0]) {
            mirror_state[0] = 0;
            j++;
            if (j == xres) {
                j = 0;
                if (++mirror_state[1] == mirror_count[1]) {
                    mirror_state[1] = 0;
                    i++;
                    if (i == yres) {
                        i = 0;
                        if (++mirror_state[2] == mirror_count[2]) {
                            mirror_state[2] = 0;
                        }
                    }
                }
            }
        }

        bid = 0;
        for (b = 0; b < 3; b++) {
            if (mirror_state[b])
                bid |= (1 << b);
        }

        d32 += rowlen;
        nitems -= rowlen;
    }

    for (i = 0; i < 8; i++) {
        GwyBrick *brick = brick_specs[i].brick;
        GwyDataField *dfield;
        GwyContainer *meta;
        gchar *title;
        gchar key[40];

        if (!brick)
            continue;

        g_snprintf(key, sizeof(key), "/brick/%i", *id);
        gwy_container_set_object_by_name(data, key, brick);
        g_object_unref(brick);

        g_snprintf(key, sizeof(key), "/brick/%i/title", *id);
        title = g_strconcat(fff->channel.name, " ", brick_specs[i].title,
                            NULL);
        gwy_container_set_string_by_name(data, key, title);

        dfield = gwy_data_field_new(brick->xres, brick->yres,
                                    brick->xreal, brick->yreal,
                                    TRUE);
        gwy_brick_mean_plane(brick, dfield, 0, 0, 0,
                             brick->xres, brick->yres, -1, FALSE);
        g_snprintf(key, sizeof(key), "/brick/%i/preview", *id);
        gwy_container_set_object_by_name(data, key, dfield);
        g_object_unref(dfield);

        meta = construct_metadata(fff, filelist->ids + fileid);
        g_snprintf(key, sizeof(key), "/brick/%i/meta", *id);
        gwy_container_set_object_by_name(data, key, meta);
        g_object_unref(meta);

        gwy_file_volume_import_log_add(data, *id, NULL, fff->filename);
        (*id)++;
    }

    remove_from_filelist(filelist, fileid);

    return TRUE;
}

static void
remove_from_filelist(OmicronFlatFileList *filelist, guint fileid)
{
    guint i;

    g_assert(fileid < filelist->nfiles);

    free_file_id(filelist->ids + fileid);
    free_file(filelist->files[fileid]);

    for (i = fileid + 1; i < filelist->nfiles; i++) {
        filelist->ids[i-1] = filelist->ids[i];
        filelist->files[i-1] = filelist->files[i];
    }

    filelist->nfiles--;
}

static void
free_file_id(OmicronFlatFileId *id)
{
    g_free(id->filename);
    g_free(id->stem);
    g_free(id->number);
    g_free(id->extension);
}

static gint
compare_file_ids(gconstpointer pa, gconstpointer pb)
{
    const OmicronFlatFileId *a = (const OmicronFlatFileId*)pa;
    const OmicronFlatFileId *b = (const OmicronFlatFileId*)pb;
    gint r;

    if ((r = strcmp(a->extension, b->extension)))
        return r;

    return strcmp(a->number, b->number);
}

/* Given filename Foobar--123_45.I(V)_flat remove the 123_45 part. */
static gboolean
parse_filename(const gchar *filename,
               OmicronFlatFileId *id,
               const gchar *dirname)
{
    gchar *fnm, *ddash, *num, *dot, *ext, *flat;
    gboolean ok = FALSE;
    guint len;

    fnm = g_path_get_basename(filename);
    g_return_val_if_fail(fnm, FALSE);

    /* The name starts with an arbitrary identifier up to "--". */
    if (!(ddash = g_strrstr(fnm, "--")) || ddash == fnm)
        goto fail;

    /* The next part has the form [0-9]+_[0-9]+. */
    num = ddash+2;
    dot = num;
    if (!g_ascii_isdigit(*dot))
        goto fail;

    do {
       dot++;
    } while (g_ascii_isdigit(*dot));

    if (*dot != '_')
        goto fail;
    dot++;

    if (!g_ascii_isdigit(*dot))
        goto fail;

    do {
       dot++;
    } while (g_ascii_isdigit(*dot));

    /* Then there is a dot */
    if (*dot != '.')
        goto fail;

    /* Then there is a complex extension, that can contain more dots, dashes,
     * underscores, parentheses and whatever.  We only know it ends "_flat". */
    ext = dot+1;
    len = strlen(ext);
    if (len < 6 || !gwy_strequal(ext + len-5, EXTENSION))
        goto fail;

    flat = ext + (len-5);

    /* Now we know the file name contains all the expected parts, so create
     * the parsed record. */
    ok = TRUE;
    id->filename = g_build_filename(dirname, filename, NULL);
    id->stem = g_strndup(fnm, ddash-fnm);
    id->number = g_strndup(num, dot-num);
    id->extension = g_strndup(ext, flat-ext);
    gwy_debug("Parsed %s into <%s> <%s> <%s>",
              fnm, id->stem, id->number, id->extension);

fail:
    g_free(fnm);
    return ok;
}

static gboolean
find_related_files(const gchar *filename,
                   OmicronFlatFileList *filelist,
                   GError **error)
{
    OmicronFlatFileId origid;
    GDir *dir;
    gchar *dirname;
    GArray *filenames;

    if (!parse_filename(filename, &origid, ".")) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("File name does not have the expected form "
                      "for Omicron Flat files."));
        return FALSE;
    }

    dirname = g_path_get_dirname(filename);
    gwy_debug("scanning directory %s", dirname);
    if (!(dir = g_dir_open(dirname, 0, error))) {
        free_file_id(&origid);
        g_free(dirname);
        return FALSE;
    }

    filenames = g_array_new(FALSE, FALSE, sizeof(OmicronFlatFileId));
    while ((filename = g_dir_read_name(dir))) {
        OmicronFlatFileId id;

        gwy_debug("found %s", filename);
        if (parse_filename(filename, &id, dirname)) {
            if (gwy_strequal(id.stem, origid.stem)) {
                gwy_debug("stem of %s matches original file", filename);
                g_array_append_val(filenames, id);
            }
            else
                free_file_id(&id);
        }
    }
    g_free(dirname);
    g_dir_close(dir);
    free_file_id(&origid);

    if (filenames->len) {
        filelist->nfiles = filenames->len;
        filelist->ids = (OmicronFlatFileId*)g_array_free(filenames, FALSE);
        qsort(filelist->ids, filelist->nfiles, sizeof(OmicronFlatFileId),
              &compare_file_ids);
        return TRUE;
    }

    /* XXX: Someone is messing with the directory while we are scanning it? */
    g_set_error(error, GWY_MODULE_FILE_ERROR,
                GWY_MODULE_FILE_ERROR_SPECIFIC,
                _("File name does not have the expected form "
                  "for Omicron Flat files."));
    return FALSE;
}

static GwyContainer*
construct_metadata(OmicronFlatFile *fff,
                   OmicronFlatFileId *id)
{
    GwyContainer *meta = gwy_container_new();
    OmicronFlatChannel *channel = &fff->channel;
    OmicronFlatExperiment *experiment = &fff->experiment;
    GString *key = g_string_new(NULL);
    GString *value = g_string_new(NULL);
    time_t timestamp;
    gchar creation_time[48];
    struct tm *t;
    guint i, j;

    g_string_printf(value, "%.4s", fff->identification.magic);
    gwy_container_set_const_string_by_name(meta, "File::Magic", value->str);

    g_string_printf(value, "%.4s", fff->identification.file_structure_level);
    gwy_container_set_const_string_by_name(meta, "File::Structure level",
                                           value->str);

    gwy_container_set_const_string_by_name(meta, "File::Base name", id->stem);
    gwy_container_set_const_string_by_name(meta, "File::Number", id->number);
    gwy_container_set_const_string_by_name(meta, "File::Extension",
                                           id->extension);

    for (i = 0; i < fff->axis_count; i++) {
        OmicronFlatAxis *axis = fff->axis_descriptions + i;
        if (strlen(axis->parent_name)) {
            g_string_printf(key, "Axis::%s::Parent axis", axis->name);
            gwy_container_set_const_string_by_name(meta, key->str,
                                                   axis->parent_name);
        }

        g_string_printf(key, "Axis::%s::Mirrored", axis->name);
        gwy_container_set_const_string_by_name(meta, key->str,
                                               axis->is_mirrored ? "Yes" : "No");

        g_string_printf(key, "Axis::%s::Units", axis->name);
        gwy_container_set_const_string_by_name(meta, key->str, axis->unit_name);

        for (j = 0; j < axis->table_set_count; j++) {
            OmicronFlatTableSet *table_set = axis->table_set_fields + j;

            g_string_printf(key, "Axis::%s::TableSet %u::Name",
                            axis->name, j+1);
            gwy_container_set_const_string_by_name(meta, key->str,
                                                   table_set->name);

            g_string_printf(key, "Axis::%s::TableSet %u::Interval count",
                            axis->name, j+1);
            g_string_printf(value, "%u", table_set->interval_count);
            gwy_container_set_const_string_by_name(meta, key->str, value->str);
        }
    }

    gwy_container_set_const_string_by_name(meta, "Channel::Name",
                                           channel->name);
    gwy_container_set_const_string_by_name(meta, "Channel::Transfer function",
                                           channel->transfer_func_name);
    gwy_container_set_const_string_by_name(meta, "Channel::Units",
                                           channel->unit_name);

    for (i = 0; i < channel->parameter_count; i++) {
        OmicronFlatTransferParam *param = channel->parameters + i;
        g_string_printf(key, "Channel::Transfer function::%s", param->name);
        g_string_printf(value, "%g", param->value);
        gwy_container_set_const_string_by_name(meta, key->str, value->str);
    }

    for (i = 0; i < channel->data_view_type_count; i++) {
        g_string_printf(key, "Channel::View type %u", i+1);
        g_string_printf(value, "%u", channel->view_types[i]);
        gwy_container_set_const_string_by_name(meta, key->str, value->str);
    }

    g_string_printf(value, "%" G_GUINT64_FORMAT,
                    (guint64)fff->creation.timestamp);
    gwy_container_set_const_string_by_name(meta, "Creation::Timestamp",
                                           value->str);
    t = localtime(&timestamp);
    strftime(creation_time, sizeof(creation_time), "%Y-%m-%d %H:%M:%S", t);
    gwy_container_set_const_string_by_name(meta, "Creation::Date and time",
                                           creation_time);

    if (strlen(fff->creation.info)) {
        gwy_container_set_const_string_by_name(meta, "Creation::Info",
                                               fff->creation.info);
    }

    gwy_container_set_const_string_by_name(meta, "Experiment::Name",
                                           experiment->name);
    gwy_container_set_const_string_by_name(meta, "Experiment::Version",
                                           experiment->version);
    gwy_container_set_const_string_by_name(meta, "Experiment::Description",
                                           experiment->description);
    gwy_container_set_const_string_by_name(meta, "Experiment::File specification",
                                           experiment->file_spec);
    gwy_container_set_const_string_by_name(meta, "Experiment::File creator ID",
                                           experiment->file_creator_id);
    gwy_container_set_const_string_by_name(meta, "Experiment::Result file creator ID",
                                           experiment->result_file_creator_id);
    gwy_container_set_const_string_by_name(meta, "Experiment::User name",
                                           experiment->user_name);
    gwy_container_set_const_string_by_name(meta, "Experiment::Account name",
                                           experiment->account_name);
    gwy_container_set_const_string_by_name(meta, "Experiment::Result data file specification",
                                           experiment->result_data_file_spec);

    g_string_printf(value, "%u", experiment->run_cycle_id);
    gwy_container_set_const_string_by_name(meta, "Experiment::Run cycle ID",
                                           value->str);

    g_string_printf(value, "%u", experiment->scan_cycle_id);
    gwy_container_set_const_string_by_name(meta, "Experiment::Scan cycle ID",
                                           value->str);

    for (i = 0; i < fff->exp_instance_count; i++) {
        OmicronFlatExperimentParamInstance *instance = fff->exp_instances + i;
        for (j = 0; j < instance->parameter_count; j++) {
            OmicronFlatExperimentParam *param = instance->parameters + j;
            g_string_printf(key, "Experiment::%s::%s",
                            instance->name, param->name);
            g_string_printf(value, "%s %s", param->value, param->unit);
            gwy_container_set_const_string_by_name(meta, key->str, value->str);
        }
    }

    for (i = 0; i < fff->depl_instance_count; i++) {
        OmicronFlatDeploymentParamInstance *instance = fff->depl_instances + i;
        for (j = 0; j < instance->parameter_count; j++) {
            OmicronFlatDeploymentParam *param = instance->parameters + j;
            g_string_printf(key, "Deployment::%s::%s",
                            instance->name, param->name);
            gwy_container_set_const_string_by_name(meta, key->str,
                                                   param->value);
        }
    }

    return meta;
}

/* Construct explicit range and offset for an interval on an axis The range is
 * always positive, regardless of mirroring. */
static void
construct_axis_range(const OmicronFlatAxis *axis, guint interval_id,
                     gdouble *real, gdouble *offset, guint *n)
{
    gdouble phys_start = axis->physical_start_value,
            phys_step = axis->physical_increment;
    guint ndata, start, stop, step;

    if (axis->table_set_ref) {
        OmicronFlatTableSet *table_set = axis->table_set_ref;
        OmicronFlatInterval *interval = table_set->intervals + interval_id;

        gwy_debug("%s has table set", axis->name);
        g_return_if_fail(interval_id < table_set->interval_count);
        /* Subtract 1 to ensure first value == axis->physical_start_value
         * when start_clock == 1.  FIXME: Correct? */
        start = interval->start_clock - 1;
        stop = interval->stop_clock - 1;
        step = interval->step;
    }
    else {
        g_return_if_fail(interval_id < axis->mirror_mult);
        gwy_debug("%s has no table set", axis->name);
        start = 0;
        stop = axis->clock_count/axis->mirror_mult - 1;
        step = 1;
    }
    gwy_debug("start %u, stop %u, step %u", start, stop, step);
    gwy_debug("phys_start %g, phys_step %g", phys_start, phys_step);
    ndata = *n = (stop - start)/step + 1;
    *offset = phys_start - 0.5*phys_step;
    *real = ndata*step*phys_step;
}

/* Construct explicit list of abscissa values for an interval on an axis.
 * The list is always returned in ascending order, regardless of mirroring. */
static gdouble*
construct_axis_xdata(const OmicronFlatAxis *axis, guint interval_id, guint *n)
{
    gdouble *xdata;
    gdouble phys_start = axis->physical_start_value,
            phys_step = axis->physical_increment;
    guint i, ndata, start, stop, step;

    if (axis->table_set_ref) {
        OmicronFlatTableSet *table_set = axis->table_set_ref;
        OmicronFlatInterval *interval = table_set->intervals + interval_id;

        g_return_val_if_fail(interval_id < table_set->interval_count, NULL);
        /* Subtract 1 to ensure first value == axis->physical_start_value
         * when start_clock == 1.  FIXME: Correct? */
        start = interval->start_clock - 1;
        stop = interval->stop_clock - 1;
        step = interval->step;
    }
    else {
        g_return_val_if_fail(interval_id < axis->mirror_mult, NULL);
        start = 0;
        stop = axis->clock_count/axis->mirror_mult - 1;
        step = 1;
    }
    ndata = *n = (stop - start)/step + 1;
    xdata = g_new(gdouble, ndata);
    for (i = start; i <= stop; i += step)
        xdata[i] = phys_start + i*phys_step;

    return xdata;
}

static void
err_TRUNCATED(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("File is truncated."));
}

static gboolean
read_identification(const guchar **p, gsize *size,
                    OmicronFlatIdentification *identification,
                    GError **error)
{
    if (*size < MAGIC_SIZE + FLAT_VERSION_SIZE) {
        err_TOO_SHORT(error);
        return 0;
    }

    get_CHARARRAY(identification->magic, p);
    *size -= sizeof(identification->magic);
    gwy_debug("magic %.4s", identification->magic);

    get_CHARARRAY(identification->file_structure_level, p);
    *size -= sizeof(identification->file_structure_level);
    gwy_debug("fsl %.4s", identification->file_structure_level);

    if (memcmp(identification->magic, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Omicron Flat");
        return FALSE;
    }
    if (memcmp(identification->file_structure_level,
               FLAT_VERSION, FLAT_VERSION_SIZE) != 0) {
        err_FILE_TYPE(error, "Omicron Flat");
        return FALSE;
    }

    return TRUE;
}

static gboolean
read_axis_hierarchy_description(const guchar **p, gsize *size,
                                OmicronFlatFile *fff,
                                GError **error)
{
    guint i, j, k;

    if (!read_uint32(p, size, &fff->axis_count, error))
        return FALSE;

    gwy_debug("axis_count %u", fff->axis_count);
    if (!fff->axis_count)
        goto fail;

    fff->axis_descriptions = g_new0(OmicronFlatAxis, fff->axis_count);
    for (i = 0; i < fff->axis_count; i++) {
        OmicronFlatAxis *axis = fff->axis_descriptions + i;
        if (!read_string(p, size, &axis->name, error))
            return FALSE;

        gwy_debug("axis %s", axis->name);
        if ((axis->unq_name = strrchr(axis->name, ':')))
            axis->unq_name++;
        else
            axis->unq_name = axis->name;

        if (!read_string(p, size, &axis->parent_name, error)
            || !read_string(p, size, &axis->unit_name, error)
            || !read_uint32(p, size, &axis->clock_count, error)
            || !read_sint32(p, size, &axis->raw_start_value, error)
            || !read_sint32(p, size, &axis->raw_increment, error)
            || !read_double(p, size, &axis->physical_start_value, error)
            || !read_double(p, size, &axis->physical_increment, error)
            || !read_sint32(p, size, &axis->is_mirrored, error)
            || !read_uint32(p, size, &axis->table_set_count, error))
            return FALSE;

        gwy_debug("axis [%s] (parent %s) %u [%d->%d] [%g->%g] %s",
                  axis->unit_name, axis->parent_name, axis->clock_count,
                  axis->raw_start_value, axis->raw_increment,
                  axis->physical_start_value, axis->physical_increment,
                  axis->is_mirrored ? "mirrored" : "not mirrored");

        if (i) {
            /* We can only read axes are in the bottom-up hierarchy order. */
            if (!gwy_strequal(axis->name,
                              fff->axis_descriptions[i-1].parent_name))
                goto fail;
            /* We have no idea what to do with table sets on non-trigger
             * axes. */
            if (axis->table_set_count)
                goto fail;
        }

        if (!axis->table_set_count)
            continue;
        gwy_debug("%u table sets", axis->table_set_count);
        axis->table_set_fields = g_new0(OmicronFlatTableSet,
                                        axis->table_set_count);
        for (j = 0; j < axis->table_set_count; j++) {
            OmicronFlatTableSet *table_set = axis->table_set_fields + j;

            if (!read_string(p, size, &table_set->name, error)
                || !read_uint32(p, size, &table_set->interval_count, error))
                return FALSE;

            gwy_debug("axis name %s (interval count %u)",
                      table_set->name, table_set->interval_count);
            if (!table_set->interval_count)
                continue;

            gwy_debug("%u intervals in set %u", table_set->interval_count, j);
            table_set->intervals = g_new0(OmicronFlatInterval,
                                          table_set->interval_count);

            for (k = 0; k < table_set->interval_count; k++) {
                OmicronFlatInterval *interval = table_set->intervals + k;
                if (!read_sint32(p, size, &interval->start_clock, error)
                    || !read_sint32(p, size, &interval->stop_clock, error)
                    || !read_sint32(p, size, &interval->step, error))
                    return FALSE;
                gwy_debug("interval %u [%d..%d] @%d",
                          k, interval->start_clock, interval->stop_clock,
                          interval->step);
            }
        }
    }

    /* Find out which table set belongs to which axis in the list. */
    if (fff->axis_descriptions[0].table_set_count) {
        OmicronFlatAxis *axis0 = fff->axis_descriptions;
        gboolean found = FALSE;

        for (i = 0; i < axis0->table_set_count; i++) {
            OmicronFlatTableSet *table_set = axis0->table_set_fields + i;
            for (j = 0; j < fff->axis_count; j++) {
                OmicronFlatAxis *axis = fff->axis_descriptions + j;
                if (gwy_strequal(axis->name, table_set->name)) {
                    axis->table_set_ref = table_set;
                    /* The table set intervals must fit inside. */
                    for (k = 0; k < table_set->interval_count; k++) {
                        OmicronFlatInterval *interval = table_set->intervals + k;
                        if (interval->start_clock < 1
                            || interval->stop_clock > axis->clock_count)
                            goto fail;
                    }
                    found = TRUE;
                    break;
                }
            }
            if (!found)
                goto fail;
        }
    }

    /* Check if can calculate the correct data size.  Otherwise we probably
     * do not understand the axis hierarchy. */
    fff->single_data_len = fff->mirror_mult = 1;
    for (i = 0; i < fff->axis_count; i++) {
        OmicronFlatAxis *axis = fff->axis_descriptions + i;
        OmicronFlatTableSet *table_set = axis->table_set_ref;
        if (table_set) {
            guint cnt = 0;
            for (j = 0; j < table_set->interval_count; j++) {
                OmicronFlatInterval *interval = table_set->intervals + j;
                cnt += (interval->stop_clock
                        - interval->start_clock)/interval->step + 1;
            }
            axis->single_data_len = cnt;
            /* Mirroring plays no role here.  The axis must have separate
             * mirrored intervals for physical mirroring. */
            axis->mirror_mult = 1;
        }
        else {
            axis->single_data_len = axis->clock_count;
            axis->mirror_mult = axis->is_mirrored ? 2 : 1;
        }
        gwy_debug("axis %u single data len %u", i, axis->single_data_len);
        gwy_debug("axis %u mirroring multiplier %u", i, axis->mirror_mult);

        fff->single_data_len *= axis->single_data_len;
        fff->mirror_mult *= axis->mirror_mult;
    }
    gwy_debug("file single data len %u", fff->single_data_len);
    gwy_debug("file mirroring multiplier %u", fff->mirror_mult);

    return TRUE;

fail:
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Cannot understand the axis hierarchy."));
    return FALSE;
}

static gboolean
find_transfer_params(OmicronFlatChannel *channel,
                     ...)
{
    va_list ap;
    const gchar *name;

    va_start(ap, channel);
    while ((name = va_arg(ap, const gchar*))) {
        gdouble *pval = va_arg(ap, gdouble*);
        gboolean found = FALSE;
        guint i;

        for (i = 0; i < channel->parameter_count; i++) {
            OmicronFlatTransferParam *param = channel->parameters + i;
            if (gwy_strequal(param->name, name)) {
                *pval = param->value;
                found = TRUE;
                break;
            }
        }
        if (!found)
            return FALSE;
    }
    va_end(ap);

    return TRUE;
}

static gboolean
read_channel_description(const guchar **p, gsize *size,
                         OmicronFlatChannel *channel,
                         GError **error)
{
    guint i;

    if (!read_string(p, size, &channel->name, error))
        return FALSE;
    gwy_debug("channel %s", channel->name);

    if (!read_string(p, size, &channel->transfer_func_name, error)
        || !read_string(p, size, &channel->unit_name, error)
        || !read_uint32(p, size, &channel->parameter_count, error))
        return FALSE;
    gwy_debug("channel (%s) [%s]",
              channel->transfer_func_name, channel->unit_name);
    gwy_debug("channel param count %u", channel->parameter_count);

    if (channel->parameter_count) {
        channel->parameters = g_new0(OmicronFlatTransferParam,
                                     channel->parameter_count);
        for (i = 0; i < channel->parameter_count; i++) {
            OmicronFlatTransferParam *param = channel->parameters + i;
            if (!read_string(p, size, &param->name, error)
                || !read_double(p, size, &param->value, error))
                return FALSE;
            gwy_debug("param %s = %g", param->name, param->value);
        }
    }

    if (!read_uint32(p, size, &channel->data_view_type_count, error))
        return FALSE;
    gwy_debug("channel data view type count %u",
              channel->data_view_type_count);

    if (channel->data_view_type_count) {
        channel->view_types = g_new0(guint, channel->data_view_type_count);
        for (i = 0; i < channel->data_view_type_count; i++) {
            if (!read_uint32(p, size, channel->view_types + i, error))
                return FALSE;
        }
    }

    /* Figure out scaling factors and offsets. */
    channel->q = 1.0;
    channel->z0 = 0.0;
    if (gwy_strequal(channel->transfer_func_name, "TFF_MultiLinear1D")) {
        gdouble offset, preoffset, raw1, prefactor, neutralfactor;
        if (find_transfer_params(channel,
                                  "Offset", &offset,
                                  "PreOffset", &preoffset,
                                  "Raw_1", &raw1,
                                  "PreFactor", &prefactor,
                                  "NeutralFactor", &neutralfactor,
                                  NULL)) {
            channel->q = (raw1 - preoffset)/(prefactor*neutralfactor);
            channel->z0 = -offset*channel->q;
        }
        else {
            g_warning("Cannot find transfer parameters for %s.",
                      channel->transfer_func_name);
        }
    }
    else if (gwy_strequal(channel->transfer_func_name, "TFF_Linear1D")) {
        gdouble offset, factor;
        if (find_transfer_params(channel,
                                  "Offset", &offset,
                                  "Factor", &factor,
                                  NULL)) {
            channel->q = 1.0/factor;
            channel->z0 = -offset/factor;
        }
        else {
            g_warning("Cannot find transfer parameters for %s.",
                      channel->transfer_func_name);
        }
    }
    else if (!gwy_strequal(channel->transfer_func_name, "TFF_Identity")) {
            g_warning("Unknown transfer function %s.",
                      channel->transfer_func_name);
    }

    return TRUE;
}

static gboolean
read_creation_information(const guchar **p, gsize *size,
                          OmicronFlatCreation *creation,
                          GError **error)
{
    if (!read_uint64(p, size, &creation->timestamp, error)
        || !read_string(p, size, &creation->info, error))
        return FALSE;
    gwy_debug("timestamp %" G_GUINT64_FORMAT, creation->timestamp);
    gwy_debug("info %s", creation->info);
    return TRUE;
}

static gboolean
read_raw_data(const guchar **p, gsize *size, OmicronFlatRawData *raw_data,
              GError **error)
{
    gsize data_size;

    if (!read_uint32(p, size, &raw_data->full_size, error)
        || !read_uint32(p, size, &raw_data->actual_item_count, error))
        return FALSE;
    gwy_debug("full size %u, actual item count %u",
              raw_data->full_size, raw_data->actual_item_count);

    if (raw_data->actual_item_count > raw_data->full_size) {
        err_INVALID(error, "Data Item Count");
        return FALSE;
    }

    /* The number of data values stored in the file is the short count,
     * not the full bricklet size. */
    if (*size/sizeof(gint32) < raw_data->actual_item_count) {
        err_TRUNCATED(error);
        return FALSE;
    }

    raw_data->data = *p;
    data_size = raw_data->actual_item_count * sizeof(gint32);
    *p += data_size;
    *size -= data_size;

    return TRUE;
}

static gboolean
read_offsets(const guchar **p, gsize *size, OmicronFlatFile *fff,
             GError **error)
{
    guint i;

    if (!read_uint32(p, size, &fff->offset_count, error))
        return FALSE;
    gwy_debug("offset count %u", fff->offset_count);

    if (fff->offset_count) {
        fff->offsets = g_new0(gdouble, 2*fff->offset_count);
        for (i = 0; i < 2*fff->offset_count; i++) {
            if (!read_double(p, size, fff->offsets + i, error))
                return FALSE;
            if (i & 1) {
                gwy_debug("offset (%g, %g)",
                          fff->offsets[i-1], fff->offsets[i]);
            }
        }
    }

    return TRUE;
}

static gboolean
read_experiment_information(const guchar **p, gsize *size,
                            OmicronFlatExperiment *experiment,
                            GError **error)
{
    if (!read_string(p, size, &experiment->name, error))
        return FALSE;
    gwy_debug("experient name %s", experiment->name);

    if (!read_string(p, size, &experiment->version, error)
        || !read_string(p, size, &experiment->description, error)
        || !read_string(p, size, &experiment->file_spec, error)
        || !read_string(p, size, &experiment->file_creator_id, error)
        || !read_string(p, size, &experiment->result_file_creator_id, error)
        || !read_string(p, size, &experiment->user_name, error)
        || !read_string(p, size, &experiment->account_name, error)
        || !read_string(p, size, &experiment->result_data_file_spec, error)
        || !read_uint32(p, size, &experiment->run_cycle_id, error)
        || !read_uint32(p, size, &experiment->scan_cycle_id, error))
        return FALSE;

    return TRUE;
}

static gboolean
read_experiment_parameters(const guchar **p, gsize *size,
                           OmicronFlatFile *fff,
                           GError **error)
{
    guint i, j;

    if (!read_uint32(p, size, &fff->exp_instance_count, error))
        return FALSE;
    gwy_debug("exp instance count %u", fff->exp_instance_count);

    if (fff->exp_instance_count) {
        fff->exp_instances = g_new0(OmicronFlatExperimentParamInstance,
                                    fff->exp_instance_count);
        for (i = 0; i < fff->exp_instance_count; i++) {
            OmicronFlatExperimentParamInstance *instance = fff->exp_instances + i;
            if (!read_string(p, size, &instance->name, error))
                return FALSE;
            gwy_debug("exp instance %s", instance->name);
            if (!read_uint32(p, size, &instance->parameter_count, error))
                return FALSE;
            gwy_debug("exp parameter count %u", instance->parameter_count);

            if (instance->parameter_count) {
                instance->parameters = g_new0(OmicronFlatExperimentParam,
                                              instance->parameter_count);
                for (j = 0; j < instance->parameter_count; j++) {
                    OmicronFlatExperimentParam *param = instance->parameters + j;
                    if (!read_string(p, size, &param->name, error)
                        || !read_uint32(p, size, &param->value_type, error)
                        || !read_string(p, size, &param->unit, error)
                        || !read_string(p, size, &param->value, error))
                        return FALSE;
                    gwy_debug("exp parameter %s @%u = %s [%s]",
                              param->name, param->value_type,
                              param->value, param->unit);
                }
            }
        }
    }

    return TRUE;
}

static gboolean
read_deployment_parameters(const guchar **p, gsize *size,
                           OmicronFlatFile *fff,
                           GError **error)
{
    guint i, j;

    if (!read_uint32(p, size, &fff->depl_instance_count, error))
        return FALSE;
    gwy_debug("depl instance count %u", fff->depl_instance_count);

    if (fff->depl_instance_count) {
        fff->depl_instances = g_new0(OmicronFlatDeploymentParamInstance,
                                    fff->depl_instance_count);
        for (i = 0; i < fff->depl_instance_count; i++) {
            OmicronFlatDeploymentParamInstance *instance = fff->depl_instances + i;
            if (!read_string(p, size, &instance->name, error))
                return FALSE;
            gwy_debug("depl instance %s", instance->name);
            if (!read_uint32(p, size, &instance->parameter_count, error))
                return FALSE;
            gwy_debug("depl parameter count %u", instance->parameter_count);

            if (instance->parameter_count) {
                instance->parameters = g_new0(OmicronFlatDeploymentParam,
                                              instance->parameter_count);
                for (j = 0; j < instance->parameter_count; j++) {
                    OmicronFlatDeploymentParam *param = instance->parameters + j;
                    if (!read_string(p, size, &param->name, error)
                        || !read_string(p, size, &param->value, error))
                        return FALSE;
                    gwy_debug("depl parameter %s = %s",
                              param->name, param->value);
                }
            }
        }
    }

    return TRUE;
}

static gboolean
read_uint32(const guchar **p, gsize *size, guint *v, GError **error)
{
    if (*size < sizeof(guint32)) {
        err_TRUNCATED(error);
        return FALSE;
    }

    *v = gwy_get_guint32_le(p);
    *size -= sizeof(guint32);
    return TRUE;
}

static gboolean
read_sint32(const guchar **p, gsize *size, gint *v, GError **error)
{
    if (*size < sizeof(gint32)) {
        err_TRUNCATED(error);
        return FALSE;
    }

    *v = gwy_get_gint32_le(p);
    *size -= sizeof(gint32);
    return TRUE;
}

static gboolean
read_uint64(const guchar **p, gsize *size, guint64 *v, GError **error)
{
    if (*size < sizeof(guint64)) {
        err_TRUNCATED(error);
        return FALSE;
    }

    *v = gwy_get_guint64_le(p);
    *size -= sizeof(guint64);
    return TRUE;
}

static gboolean
read_double(const guchar **p, gsize *size, gdouble *v, GError **error)
{
    if (*size < sizeof(gdouble)) {
        err_TRUNCATED(error);
        return FALSE;
    }

    *v = gwy_get_gdouble_le(p);
    *size -= sizeof(gdouble);
    return TRUE;
}

static gboolean
read_string(const guchar **p, gsize *size, gchar **v, GError **error)
{
    GError *err = NULL;
    guint len;

    if (!read_uint32(p, size, &len, error))
        return FALSE;

    if (!len) {
        *v = g_strdup("");
        return TRUE;
    }

    if (len > STRING_MAXLENGTH || len > *size/2) {
        gwy_debug("overrun string");
        err_TRUNCATED(error);
        return FALSE;
    }

    *v = g_utf16_to_utf8((gunichar2*)*p, len, NULL, NULL, &err);
    if (!*v) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Cannot convert string from UTF-16: %s"),
                    err->message);
        g_clear_error(&err);
        return FALSE;
    }

    *p += 2*len;
    *size -= 2*len;
    return TRUE;
}

static void
err_UNKNOWN_DATA_TYPE(GError **error, const OmicronFlatFile *fff)
{
    GString *s = g_string_new(NULL);
    guint i;

    g_string_append(s, "axes (");
    for (i = 0; i < fff->axis_count; i++) {
        OmicronFlatAxis *axis = fff->axis_descriptions + i;
        g_string_append_printf(s, "%s%s %u%s%s",
                               i ? ", " : "",
                               axis->unq_name, axis->clock_count,
                               axis->is_mirrored ? "M" : "",
                               axis->table_set_ref ? "T" : "");
    }
    g_string_append(s, "), views (");
    for (i = 0; i < fff->channel.data_view_type_count; i++) {
        g_string_append_printf(s, "%s%d",
                               i ? ", " : "",
                               fff->channel.view_types[i]);
    }
    g_string_append(s, ")");

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
    /* TRANSLATORS: %s is replaced with an expression of the data shape. */
                _("Cannot figure out how to load data in the following form: "
                  "%s."), s->str);
    g_string_free(s, TRUE);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
