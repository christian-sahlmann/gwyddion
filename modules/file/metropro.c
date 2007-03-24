/*
 *  $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-zygo-spm">
 *   <comment>Zygo SPM data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="\x88\x1b\x03\x6f"/>
 *   </magic>
 *   <glob pattern="*.dat"/>
 *   <glob pattern="*.DAT"/>
 * </mime-type>
 **/

#include <string.h>
#include <time.h>
#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define MAGIC "\x88\x1b\x03\x6f"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define HEADER_SIZE 834

typedef enum {
    MPRO_PHASE_RES_NORMAL = 0,
    MPRO_PHASE_RES_HIGH   = 1,
} MProPhaseResType;

typedef struct {
    gchar magic[4];
    gint header_format;
    gint header_size;
    gint software_type;
    gchar software_date[30];
    gint version_major;
    gint version_minor;
    gint version_micro;
    gint intens_xoff;
    gint intens_yoff;
    gint intens_xres;
    gint intens_yres;
    gint nbuckets;
    gint intens_range;
    gint intens_nbytes;
    gint phase_xoff;
    gint phase_yoff;
    gint phase_xres;
    gint phase_yres;
    gint phase_nbytes;
    gint timestamp;
    gchar comment[82];
    gint source;
    gdouble scale_factor;
    gdouble wavelength_in;
    gdouble numeric_aperture;
    gdouble obliquity_factor;
    gdouble magnification;
    gdouble camera_res;
    gint acquire_mode;
    gint intens_avgs;
    gint pzt_cal;
    gint pzt_gain_tolerance;
    gint pzt_gain;
    gdouble part_thickness;
    gint agc;
    gdouble target_range;
    gint min_mod;
    gint min_mod_pts;
    MProPhaseResType phase_res;
    gint min_area_size;
    gint discont_action;
    gdouble discont_filter;
    gint connection_order;
    gboolean data_inverted;
    gint camera_width;
    gint camera_height;
    gint system_type;
    gint system_board;
    gint system_serial;
    gint instrument_id;
    gchar objective_name[12];
    gchar part_num[40];
    gint code_vtype;
    gint phase_avgs;
    gint subtract_sys_err;
    gchar part_ser_num[40];
    gdouble refactive_index;
    gint remove_tilt_bias;
    gint remove_fringes;
    gint max_area_size;
    gint setup_type;
    gdouble pre_connect_filter;
    gint wavelength_fold;
    gdouble wavelength1;
    gdouble wavelength2;
    gdouble wavelength3;
    gdouble wavelength4;
    gchar wavelength_select[8];
    gint fda_res;
    gchar scan_description[20];
    gint nfiducials;
    gdouble fiducials[2*7];
    gdouble pixel_width;
    gdouble pixel_height;
    gdouble exit_pupil_diam;
    gdouble light_level_pct;
    gint coords_state;
    gdouble xpos;
    gdouble ypos;
    gdouble zpos;
    gdouble xrot;
    gdouble yrot;
    gdouble zrot;
    gint coherence_mode;
    gint surface_filter;
    gchar sys_err_file[28];
    gchar zoom_desc[8];

    /* Our stuff */
    GwyDataField **intensity_data;
    GwyDataField **intensity_mask;
    GwyDataField *phase_data;
    GwyDataField *phase_mask;
} MProFile;

static gboolean      module_register      (void);
static gint          mprofile_detect      (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer* mprofile_load        (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static gboolean      mprofile_read_header (const guchar *buffer,
                                           gsize size,
                                           MProFile *mprofile,
                                           GError **error);
static gint          fill_data_fields     (MProFile *mprofile,
                                           const guchar *buffer);
static GwyContainer* mprofile_get_metadata(MProFile *mprofile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports binary MetroPro (Zygo) data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("metropro",
                           N_("MetroPro files (.dat)"),
                           (GwyFileDetectFunc)&mprofile_detect,
                           (GwyFileLoadFunc)&mprofile_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
mprofile_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, ".dat") ? 10 : 0;

    if (fileinfo->buffer_len < HEADER_SIZE)
        return 0;

    if (memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
mprofile_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    MProFile mprofile;
    GwyContainer *meta, *container = NULL;
    GwyDataField *dfield = NULL, *vpmask = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    gsize expected;
    GString *key;
    const gchar *title;
    guint n, i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (!mprofile_read_header(buffer, size, &mprofile, error))
        return NULL;

    expected = mprofile.header_size
               + 2*mprofile.nbuckets*mprofile.intens_xres*mprofile.intens_yres
               + 4*mprofile.phase_xres*mprofile.phase_yres;
    if (err_SIZE_MISMATCH(error, expected, size, TRUE)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    n = fill_data_fields(&mprofile, buffer);
    gwy_file_abandon_contents(buffer, size, NULL);
    if (!n) {
        err_NO_DATA(error);
        return NULL;
    }

    key = g_string_new("");
    container = gwy_container_new();
    for (i = 0; i < n; i++) {
        if (i < mprofile.nbuckets) {
            dfield = mprofile.intensity_data[i];
            vpmask = mprofile.intensity_mask[i];
            title = "Intensity";
        }
        else {
            dfield = mprofile.phase_data;
            vpmask = mprofile.phase_mask;
            title = "Phase";
        }
        g_string_printf(key, "/%d/data", i);
        gwy_container_set_object_by_name(container, key->str, dfield);
        g_string_printf(key, "/%d/data/title", i);
        gwy_container_set_string_by_name(container, key->str, g_strdup(title));
        if (vpmask) {
            g_string_printf(key, "/%d/mask", i);
            gwy_container_set_object_by_name(container, key->str, vpmask);
        }

        meta = mprofile_get_metadata(&mprofile);
        g_string_printf(key, "/%d/meta", i);
        gwy_container_set_object_by_name(container, key->str, meta);
        g_object_unref(meta);

    }
    g_string_free(key, TRUE);

    for (n = 0; n < mprofile.nbuckets; n++) {
        gwy_object_unref(mprofile.intensity_data[n]);
        gwy_object_unref(mprofile.intensity_mask[n]);
    }
    gwy_object_unref(mprofile.phase_data);
    gwy_object_unref(mprofile.phase_mask);

    return container;
}

static gboolean
mprofile_read_header(const guchar *buffer,
                     gsize size,
                     MProFile *mprofile,
                     GError **error)
{
    const guchar *p;
    guint i;

    p = buffer;
    if (size < HEADER_SIZE + 2) {
        err_TOO_SHORT(error);
        return FALSE;
    }
    get_CHARARRAY(mprofile->magic, &p);
    if (memcmp(mprofile->magic, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "MetroPro");
        return FALSE;
    }

    mprofile->header_format = gwy_get_guint16_be(&p);
    if (mprofile->header_format != 1) {
        err_UNSUPPORTED(error, "FormatVersion");
        return FALSE;
    }
    mprofile->header_size = gwy_get_guint32_be(&p);
    gwy_debug("header_format: %d, header_size: %d",
              mprofile->header_format, mprofile->header_size);
    if (mprofile->header_size < 570) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is too short."));
        return FALSE;
    }
    if (mprofile->header_size > size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is larger than file."));
        return FALSE;
    }
    mprofile->software_type = gwy_get_guint16_be(&p);
    get_CHARARRAY0(mprofile->software_date, &p);
    gwy_debug("software_type: %d, software_date: %s",
              mprofile->software_type, mprofile->software_date);

    mprofile->version_major = gwy_get_guint16_be(&p);
    mprofile->version_minor = gwy_get_guint16_be(&p);
    mprofile->version_micro = gwy_get_guint16_be(&p);
    gwy_debug("version: %d.%d.%d",
              mprofile->version_major,
              mprofile->version_minor,
              mprofile->version_micro);

    mprofile->intens_xoff = gwy_get_guint16_be(&p);
    mprofile->intens_yoff = gwy_get_guint16_be(&p);
    mprofile->intens_xres = gwy_get_guint16_be(&p);
    mprofile->intens_yres = gwy_get_guint16_be(&p);
    gwy_debug("INTENS xres: %d, yres: %d, xoff: %d, yoff: %d",
              mprofile->intens_xres, mprofile->intens_yres,
              mprofile->intens_xoff, mprofile->intens_yoff);
    mprofile->nbuckets = gwy_get_guint16_be(&p);
    mprofile->intens_range = gwy_get_guint16_be(&p);
    mprofile->intens_nbytes = gwy_get_guint32_be(&p);
    gwy_debug("intens_nbytes: %d, expecting: %d",
              mprofile->intens_nbytes,
              2*mprofile->intens_xres*mprofile->intens_yres*mprofile->nbuckets);

    mprofile->phase_xoff = gwy_get_guint16_be(&p);
    mprofile->phase_yoff = gwy_get_guint16_be(&p);
    mprofile->phase_xres = gwy_get_guint16_be(&p);
    mprofile->phase_yres = gwy_get_guint16_be(&p);
    gwy_debug("PHASE xres: %d, yres: %d, xoff: %d, yoff: %d",
              mprofile->phase_xres, mprofile->phase_yres,
              mprofile->phase_xoff, mprofile->phase_yoff);
    mprofile->phase_nbytes = gwy_get_guint32_be(&p);
    gwy_debug("phase_nbytes: %d, expecting: %d",
              mprofile->phase_nbytes,
              4*mprofile->phase_xres*mprofile->phase_yres);

    mprofile->timestamp = gwy_get_guint32_be(&p);
    get_CHARARRAY0(mprofile->comment, &p);
    gwy_debug("comment: %s", mprofile->comment);
    mprofile->source = gwy_get_guint16_be(&p);

    mprofile->scale_factor = gwy_get_gfloat_be(&p);
    mprofile->wavelength_in = gwy_get_gfloat_be(&p);
    mprofile->numeric_aperture = gwy_get_gfloat_be(&p);
    mprofile->obliquity_factor = gwy_get_gfloat_be(&p);
    mprofile->magnification = gwy_get_gfloat_be(&p);
    mprofile->camera_res = gwy_get_gfloat_be(&p);

    mprofile->acquire_mode = gwy_get_guint16_be(&p);
    gwy_debug("acquire_mode: %d", mprofile->acquire_mode);
    mprofile->intens_avgs = gwy_get_guint16_be(&p);
    if (!mprofile->intens_avgs)
        mprofile->intens_avgs = 1;
    mprofile->pzt_cal = gwy_get_guint16_be(&p);
    mprofile->pzt_gain_tolerance = gwy_get_guint16_be(&p);
    mprofile->pzt_gain = gwy_get_guint16_be(&p);
    mprofile->part_thickness = gwy_get_gfloat_be(&p);
    mprofile->agc = gwy_get_guint16_be(&p);
    mprofile->target_range = gwy_get_gfloat_be(&p);

    p += 2;
    mprofile->min_mod = gwy_get_guint32_be(&p);
    mprofile->min_mod_pts = gwy_get_guint32_be(&p);
    mprofile->phase_res = gwy_get_guint16_be(&p);
    mprofile->min_area_size = gwy_get_guint32_be(&p);
    mprofile->discont_action = gwy_get_guint16_be(&p);
    mprofile->discont_filter = gwy_get_gfloat_be(&p);
    mprofile->connection_order = gwy_get_guint16_be(&p);
    mprofile->data_inverted = gwy_get_guint16_be(&p);
    mprofile->camera_width = gwy_get_guint16_be(&p);
    mprofile->camera_height = gwy_get_guint16_be(&p);
    mprofile->system_type = gwy_get_guint16_be(&p);
    mprofile->system_board = gwy_get_guint16_be(&p);
    mprofile->system_serial = gwy_get_guint16_be(&p);
    mprofile->instrument_id = gwy_get_guint16_be(&p);
    get_CHARARRAY0(mprofile->objective_name, &p);
    get_CHARARRAY0(mprofile->part_num, &p);
    gwy_debug("part_num: %s", mprofile->part_num);
    mprofile->code_vtype = gwy_get_guint16_be(&p);
    mprofile->phase_avgs = gwy_get_guint16_be(&p);
    mprofile->subtract_sys_err = gwy_get_guint16_be(&p);
    p += 16;
    get_CHARARRAY0(mprofile->part_ser_num, &p);
    mprofile->refactive_index = gwy_get_gfloat_be(&p);
    mprofile->remove_tilt_bias = gwy_get_guint16_be(&p);
    mprofile->remove_fringes = gwy_get_guint16_be(&p);
    mprofile->max_area_size = gwy_get_guint32_be(&p);
    mprofile->setup_type = gwy_get_guint16_be(&p);
    p += 2;
    mprofile->pre_connect_filter = gwy_get_gfloat_be(&p);

    mprofile->wavelength2 = gwy_get_gfloat_be(&p);
    mprofile->wavelength_fold = gwy_get_guint16_be(&p);
    mprofile->wavelength1 = gwy_get_gfloat_be(&p);
    mprofile->wavelength3 = gwy_get_gfloat_be(&p);
    mprofile->wavelength4 = gwy_get_gfloat_be(&p);
    get_CHARARRAY0(mprofile->wavelength_select, &p);
    mprofile->fda_res = gwy_get_guint16_be(&p);
    get_CHARARRAY0(mprofile->scan_description, &p);
    gwy_debug("scan_description: %s", mprofile->scan_description);

    mprofile->nfiducials = gwy_get_guint16_be(&p);
    for (i = 0; i < G_N_ELEMENTS(mprofile->fiducials); i++)
        mprofile->fiducials[i] = gwy_get_gfloat_be(&p);

    mprofile->pixel_width = gwy_get_gfloat_be(&p);
    mprofile->pixel_height = gwy_get_gfloat_be(&p);
    mprofile->exit_pupil_diam = gwy_get_gfloat_be(&p);
    mprofile->light_level_pct = gwy_get_gfloat_be(&p);
    mprofile->coords_state = gwy_get_guint32_be(&p);
    mprofile->xpos = gwy_get_gfloat_be(&p);
    mprofile->ypos = gwy_get_gfloat_be(&p);
    mprofile->zpos = gwy_get_gfloat_be(&p);
    mprofile->xrot = gwy_get_gfloat_be(&p);
    mprofile->yrot = gwy_get_gfloat_be(&p);
    mprofile->zrot = gwy_get_gfloat_be(&p);
    mprofile->coherence_mode = gwy_get_guint16_be(&p);
    mprofile->surface_filter = gwy_get_guint16_be(&p);
    get_CHARARRAY0(mprofile->sys_err_file, &p);
    get_CHARARRAY0(mprofile->zoom_desc, &p);

    return TRUE;
}

static void
set_units(GwyDataField *dfield,
          const MProFile *mprofile,
          const gchar *zunit)
{
    GwySIUnit *siunit;

    if (mprofile->camera_res)
        siunit = gwy_si_unit_new("m");
    else
        siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new(zunit);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);
}

static void
fix_void_pixels(GwyDataField *dfield,
                GwyDataField *vpmask,
                gdouble avg)
{
    GwySIUnit *siunit;
    const gdouble *mask;
    gdouble *data;
    gint i, n;

    data = gwy_data_field_get_data(dfield);
    mask = gwy_data_field_get_data_const(vpmask);
    n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
    for (i = 0; i < n; i++) {
        if (mask[i])
            data[i] = avg;
    }

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    siunit = gwy_si_unit_duplicate(siunit);
    gwy_data_field_set_si_unit_xy(vpmask, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(vpmask, siunit);
    g_object_unref(siunit);
}

static gint
fill_data_fields(MProFile *mprofile,
                 const guchar *buffer)
{
    GwyDataField *dfield, *vpmask;
    gdouble *data, *mask;
    gdouble xreal, yreal, q, avg;
    const guchar *p;
    guint n, id, i, j, nvoid, ndata;

    ndata = 0;
    mprofile->intensity_data = NULL;
    mprofile->intensity_mask = NULL;
    mprofile->phase_data = NULL;
    mprofile->phase_mask = NULL;

    p = buffer + mprofile->header_size;

    /* Intensity data */
    n = mprofile->intens_xres * mprofile->intens_yres;
    /* Enorce consistency */
    if (!n && mprofile->nbuckets) {
        g_warning("nbuckets > 0, but intensity data have zero dimension");
        mprofile->nbuckets = 0;
    }

    if (mprofile->nbuckets) {
        const guint16 *d16;
        guint16 d;

        mprofile->intensity_data = g_new(GwyDataField*, mprofile->nbuckets);
        mprofile->intensity_mask = g_new(GwyDataField*, mprofile->nbuckets);

        q = mprofile->data_inverted ? -1.0 : 1.0;

        if (mprofile->camera_res) {
            xreal = mprofile->intens_xres * mprofile->camera_res;
            yreal = mprofile->intens_yres * mprofile->camera_res;
        }
        else {
            /* whatever */
            xreal = mprofile->intens_xres;
            yreal = mprofile->intens_yres;
        }

        for (id = 0; id < mprofile->nbuckets; id++) {
            ndata++;
            dfield = gwy_data_field_new(mprofile->intens_xres,
                                        mprofile->intens_yres,
                                        xreal, yreal,
                                        FALSE);
            vpmask = gwy_data_field_new_alike(dfield, TRUE);
            data = gwy_data_field_get_data(dfield);
            mask = gwy_data_field_get_data(vpmask);
            d16 = (const guint16*)p;
            avg = 0.0;
            nvoid = 0;
            for (i = 0; i < mprofile->intens_yres; i++) {
                for (j = 0; j < mprofile->intens_xres; j++) {
                    d = q*GUINT16_FROM_BE(*d16);
                    *data = q*d;
                    if (*d16 >= 65412) {
                        nvoid++;
                        mask[i*mprofile->intens_xres + j] = 1.0;
                    }
                    else
                        avg += *data;
                    d16++;
                    data++;
                }
            }

            gwy_debug("intens_nvoid[%u]: %u", id, nvoid);
            set_units(dfield, mprofile, "");
            if (nvoid)
                fix_void_pixels(dfield, vpmask,
                                nvoid == n ? 0.0 : avg/(n - nvoid));
            else
                gwy_object_unref(vpmask);

            mprofile->intensity_data[id] = dfield;
            mprofile->intensity_mask[id] = vpmask;
            p += sizeof(guint16)*n;
        }
    }

    /* Phase data */
    n = mprofile->phase_xres * mprofile->phase_yres;
    if (n) {
        const gint32 *d32;
        gint32 d;

        ndata++;

        i = 4096;
        if (mprofile->phase_res == 1)
            i = 32768;

        q = mprofile->scale_factor * mprofile->obliquity_factor
            * mprofile->wavelength_in/i;
        if (mprofile->data_inverted)
            q = -q;
        gwy_debug("q: %g", q);

        if (mprofile->camera_res) {
            xreal = mprofile->phase_xres * mprofile->camera_res;
            yreal = mprofile->phase_yres * mprofile->camera_res;
        }
        else {
            /* whatever */
            xreal = mprofile->phase_xres;
            yreal = mprofile->phase_yres;
        }
        dfield = gwy_data_field_new(mprofile->phase_xres,
                                    mprofile->phase_yres,
                                    xreal, yreal,
                                    FALSE);
        vpmask = gwy_data_field_new_alike(dfield, TRUE);
        data = gwy_data_field_get_data(dfield);
        mask = gwy_data_field_get_data(vpmask);
        d32 = (const gint32*)p;
        avg = 0.0;
        nvoid = 0;
        for (i = 0; i < mprofile->phase_yres; i++) {
            for (j = 0; j < mprofile->phase_xres; j++) {
                d = GINT32_FROM_BE(*d32);
                *data = q*d;
                if (d >= 2147483640) {
                    nvoid++;
                    mask[i*mprofile->phase_xres + j] = 1.0;
                }
                else
                    avg += *data;
                d32++;
                data++;
            }
        }

        gwy_debug("phase_nvoid: %u", nvoid);
        set_units(dfield, mprofile, "m");
        if (nvoid)
            fix_void_pixels(dfield, vpmask,
                            nvoid == n ? 0.0 : avg/(n - nvoid));
        else
            gwy_object_unref(vpmask);

        mprofile->phase_data = dfield;
        mprofile->phase_mask = vpmask;
        p += sizeof(gint32)*n;
    }

    return ndata;
}

#define HASH_STORE(key, fmt, field) \
    gwy_container_set_string_by_name(meta, key, \
                                     g_strdup_printf(fmt, mprofile->field))

#define HASH_STORE_ENUM(key, field, e) \
    s = gwy_enum_to_string(mprofile->field, e, G_N_ELEMENTS(e)); \
    if (s && *s) \
        gwy_container_set_string_by_name(meta, key, g_strdup(s));

static void
store_meta_string(GwyContainer *container,
                  const gchar *key,
                  gchar *field)
{
    gchar *p;

    g_strstrip(field);
    if (field[0]
        && (p = g_locale_to_utf8(field, strlen(field), NULL, NULL, NULL)))
        gwy_container_set_string_by_name(container, key, p);
}

/* Quite incomplete... */
static GwyContainer*
mprofile_get_metadata(MProFile *mprofile)
{
    static const GwyEnum yesno[] = { { "No", 0, }, { "Yes", 1, } };
    static const GwyEnum software_types[] = {
        { "MetroPro",   1, },
        { "MetroBasic", 2, },
        { "dbug",       3, },
    };
    static const GwyEnum discont_actions[] = {
        { "Delete", 0, },
        { "Filter", 1, },
        { "Ignore", 2, },
    };
    static const GwyEnum system_types[] = {
        { "softwate generated data", 0, },
        { "Mark IVxp",               1, },
        { "Maxim 3D",                2, },
        { "Maxim NT",                3, },
        { "GPI-XP",                  4, },
        { "NewView",                 5, },
        { "Maxim GP",                6, },
        { "NewView/GP",              7, },
        { "Mark to GPI conversion",  8, },
    };
    GwyContainer *meta;
    time_t tp;
    struct tm *tm;
    const gchar *s;
    gchar buffer[24];
    gchar *p;

    meta = gwy_container_new();

    /* Version */
    p = g_strdup_printf("%d.%d.%d",
                        mprofile->version_major,
                        mprofile->version_minor,
                        mprofile->version_micro);
    gwy_container_set_string_by_name(meta, "Version", p);

    /* Timestamp */
    tp = mprofile->timestamp;
    tm = localtime(&tp);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
    gwy_container_set_string_by_name(meta, "Date", g_strdup(buffer));

    /* Comments */
    store_meta_string(meta, "Software date",
                      mprofile->software_date);
    store_meta_string(meta, "Comment",
                      mprofile->comment);
    store_meta_string(meta, "Objective name",
                      mprofile->objective_name);
    store_meta_string(meta, "Part measured",
                      mprofile->part_num);
    store_meta_string(meta, "Part serial number",
                      mprofile->part_ser_num);
    store_meta_string(meta, "Description",
                      mprofile->scan_description);
    store_meta_string(meta, "System error file",
                      mprofile->sys_err_file);
    store_meta_string(meta, "Zoom description",
                      mprofile->zoom_desc);
    store_meta_string(meta, "Wavelength select",
                      mprofile->wavelength_select);

    /* Misc */
    HASH_STORE_ENUM("Software type", software_type, software_types);
    HASH_STORE("Wavelength", "%g m", wavelength_in);
    HASH_STORE("Intensity averages", "%d", intens_avgs);
    HASH_STORE("Minimum modulation points", "%d", min_mod_pts);
    HASH_STORE_ENUM("Automatic gain control", agc, yesno);
    HASH_STORE_ENUM("Discontinuity action", discont_action, discont_actions);
    HASH_STORE("Discontinuity filter", "%g %%", discont_filter);
    HASH_STORE_ENUM("System type", system_type, system_types);
    HASH_STORE("System board", "%d", system_board);
    HASH_STORE("System serial", "%d", system_serial);
    HASH_STORE("Instrument id", "%d", instrument_id);
    HASH_STORE_ENUM("System error subtracted", subtract_sys_err, yesno);
    HASH_STORE("Refractive index", "%g", refactive_index);
    HASH_STORE_ENUM("Removed tilt bias", remove_tilt_bias, yesno);
    HASH_STORE_ENUM("Removed fringes", remove_fringes, yesno);
    HASH_STORE_ENUM("Wavelength folding", wavelength_fold, yesno);

    p = g_strdup_printf("%.2g", mprofile->min_mod/10.23);
    gwy_container_set_string_by_name(meta, "Minimum modulation", p);

    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

