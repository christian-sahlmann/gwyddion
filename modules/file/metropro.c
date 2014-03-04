/*
 *  $Id$
 *  Copyright (C) 2006,2014 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-zygo-spm">
 *   <comment>Zygo SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="\x88\x1b\x03\x6f"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # Zygo MetroPro
 * 0 belong 0x881b036f Zygo MetroPro surface profilometry data
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Zygo MetroPro DAT
 * .dat
 * Read
 **/
#define DEBUG 1
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

#define MAGIC1 "\x88\x1b\x03\x6f"
#define MAGIC2 "\x88\x1b\x03\x70"
#define MAGIC3 "\x88\x1b\x03\x71"
#define MAGIC_SIZE (sizeof(MAGIC1) - 1)

#define HEADER_SIZE1 834
#define HEADER_SIZE2 834
#define HEADER_SIZE3 4096

typedef enum {
    MPRO_PHASE_RES_NORMAL = 0,
    MPRO_PHASE_RES_HIGH   = 1,
} MProPhaseResType;

/* Corresponds to MAGIC1 */
typedef struct {
    gint magic_number;
    gint header_format;
    gint header_size;
    gint swinfo_type;
    gchar swinfo_date[30];
    gint swinfo_vers_maj;
    gint swinfo_vers_min;
    gint swinfo_vers_bug;
    gint ac_org_x;
    gint ac_org_y;
    gint ac_width;
    gint ac_height;
    gint ac_n_buckets;
    gint ac_range;
    gint ac_n_bytes;
    gint cn_org_x;
    gint cn_org_y;
    gint cn_width;
    gint cn_height;
    gint cn_n_bytes;
    gint time_stamp;
    gchar comment[82];
    gint source;
    gdouble intf_scale_factor;
    gdouble wavelength_in;
    gdouble num_aperture;
    gdouble obliquity_factor;
    gdouble magnification;
    gdouble lateral_res;
    gint acq_type;
    gint intens_avg_cnt;
    gint ramp_cal;
    gint sfac_limit;
    gint ramp_gain;
    gdouble part_thickness;
    gint sw_llc;
    gdouble target_range;
    gint rad_drv_veasure_eeq;
    gint min_mod;
    gint min_mod_count;
    gint phase_res;
    gint min_area;
    gint discon_action;
    gdouble discon_filter;
    gint connect_order;
    gint sign;
    gint camera_width;
    gint camera_height;
    gint sys_type;
    gint sys_board;
    gint sys_serial;
    gint inst_id;
    gchar obj_name[12];
    gchar part_name[40];
    gint codev_type;
    gint phase_avg_cnt;
    gint sub_sys_err;
    /* Unused 16 bytes. */
    gchar part_ser_num[40];
    gdouble refractive_index;
    gint rem_tilt_bias;
    gint rem_fringes;
    gint max_area;
    gint setup_type;
    gint wrapped;
    gdouble pre_connect_filter;
    gdouble wavelength_in_2;
    gint wavelength_fold;
    gdouble wavelength_in_1;
    gdouble wavelength_in_3;
    gdouble wavelength_in_4;
    gchar wavelen_select[8];
    gint fda_res;
    gchar scan_descr[20];
    gint n_fiducials_a;
    gdouble fiducials_a[14];
    gdouble pixel_width;
    gdouble pixel_height;
    gdouble exit_pupil_diam;
    gdouble light_level_pct;
    gint coords_state;
    gdouble coords_x_pos;
    gdouble coords_y_pos;
    gdouble coords_z_pos;
    gdouble coords_x_rot;
    gdouble coords_y_rot;
    gdouble coords_z_rot;
    gint coherence_mode;
    gint surface_filter;
    gchar sys_err_file_name[28];
    gchar zoom_descr[8];
    gdouble alpha_part;
    gdouble beta_part;
    gdouble dist_part;
    gint cam_split_loc_x;
    gint cam_split_loc_y;
    gint cam_split_trans_x;
    gint cam_split_trans_y;
    gchar material_a[24];
    gchar material_b[24];
    gint cam_split_unused;
    /* Unused 2 bytes. */
    gdouble dmi_ctr_x;
    gdouble dmi_ctr_y;
    gint sph_dist_corr;
    /* Unused 2 bytes. */
    gdouble sph_dist_part_na;
    gdouble sph_dist_part_radius;
    gdouble sph_dist_cal_na;
    gdouble sph_dist_cal_radius;
    gint surface_type;
    gint ac_surface_type;
    gdouble z_zosition;
    gdouble power_rultiplier;
    gdouble focus_sultiplier;
    gdouble rad_drv_vocus_sal_lactor;
    gdouble rad_drv_vower_ral_lactor;
    gdouble ftp_left_pos;
    gdouble ftp_right_pos;
    gdouble ftp_pitch_pos;
    gdouble ftp_roll_pos;
    gdouble min_mod_pct;
    gint max_inten;
    gint ring_of_fire;
    /* Unused 1 byte. */
    gint rc_orientation;
    gdouble rc_distance;
    gdouble rc_angle;
    gdouble rc_diameter;
    gint rem_fringes_mode;
    /* Unused 1 byte. */
    gint ftpsi_phase_res;
    gint frames_acquired;
    gint cavity_type;
    gdouble cam_frame_rate;
    gdouble tune_range;
    gint cal_pix_loc_x;
    gint cal_pix_loc_y;
    gint n_tst_cal_pts;
    gint n_ref_cal_pts;
    gdouble tst_cal_pts[4];
    gdouble ref_cal_pts[4];
    gdouble tst_cal_pix_opd;
    gdouble ref_cal_pix_opd;
    gint sys_serial2;
    gdouble flash_phase_dc_mask;
    gdouble flash_phase_alias_mask;
    gdouble flash_phase_filter;
    gint scan_direction;
    /* Unused 1 byte. */
    gint pre_fda_filter;
    /* Unused 4 bytes. */
    gint ftpsi_res_factor;
    /* Unused 8 bytes. */

    /* Our stuff */
    GwyDataField **intensity_data;
    GwyDataField **intensity_mask;
    GwyDataField *phase_data;
    GwyDataField *phase_mask;
} MProFile1;

static gboolean      module_register       (void);
static gint          mprofile_detect       (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* mprofile_load         (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static GwyContainer* mprofile_load1        (const gchar *filename,
                                            const guchar *buffer,
                                            gsize size,
                                            GError **error);
static GwyContainer* mprofile_load2        (const gchar *filename,
                                            const guchar *buffer,
                                            gsize size,
                                            GError **error);
static GwyContainer* mprofile_load3        (const gchar *filename,
                                            const guchar *buffer,
                                            gsize size,
                                            GError **error);
static gboolean      mprofile_read_header1 (const guchar *buffer,
                                            gsize size,
                                            MProFile1 *mprofile,
                                            GError **error);
static gint          fill_data_fields1     (MProFile1 *mprofile,
                                            const guchar *buffer);
static GwyContainer* mprofile_get_metadata1(MProFile1 *mprofile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports binary MetroPro (Zygo) data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.7",
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

    if (fileinfo->buffer_len < HEADER_SIZE1)
        return 0;

    if (memcmp(fileinfo->head, MAGIC1, MAGIC_SIZE) == 0
        || memcmp(fileinfo->head, MAGIC2, MAGIC_SIZE) == 0
        || memcmp(fileinfo->head, MAGIC3, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
mprofile_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MAGIC_SIZE)
        err_TOO_SHORT(error);
    else if (memcmp(buffer, MAGIC1, MAGIC_SIZE) == 0)
        container = mprofile_load1(filename, buffer, size, error);
    else if (memcmp(buffer, MAGIC2, MAGIC_SIZE) == 0)
        container = mprofile_load2(filename, buffer, size, error);
    else if (memcmp(buffer, MAGIC3, MAGIC_SIZE) == 0)
        container = mprofile_load3(filename, buffer, size, error);
    else
        err_FILE_TYPE(error, "MetroPro");

    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static GwyContainer*
mprofile_load1(const gchar *filename,
               const guchar *buffer, gsize size, GError **error)
{
    MProFile1 mprofile;
    GwyContainer *meta, *container = NULL;
    GwyDataField *dfield = NULL, *vpmask = NULL;
    gsize expected;
    GString *key;
    const gchar *title;
    guint n, i;

    if (!mprofile_read_header1(buffer, size, &mprofile, error))
        return NULL;

    expected = mprofile.header_size
               + 2*mprofile.ac_n_buckets*mprofile.ac_width*mprofile.ac_height
               + 4*mprofile.cn_width*mprofile.cn_height;
    if (err_SIZE_MISMATCH(error, expected, size, TRUE))
        return NULL;

    n = fill_data_fields1(&mprofile, buffer);
    if (!n) {
        err_NO_DATA(error);
        return NULL;
    }

    key = g_string_new(NULL);
    container = gwy_container_new();
    for (i = 0; i < n; i++) {
        if (i > 0) {
            dfield = mprofile.intensity_data[i-1];
            vpmask = mprofile.intensity_mask[i-1];
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

        meta = mprofile_get_metadata1(&mprofile);
        g_string_printf(key, "/%d/meta", i);
        gwy_container_set_object_by_name(container, key->str, meta);
        g_object_unref(meta);

        gwy_file_channel_import_log_add(container, i, "metropro", filename);
    }
    g_string_free(key, TRUE);

    for (n = 0; n < mprofile.ac_n_buckets; n++) {
        gwy_object_unref(mprofile.intensity_data[n]);
        gwy_object_unref(mprofile.intensity_mask[n]);
    }
    gwy_object_unref(mprofile.phase_data);
    gwy_object_unref(mprofile.phase_mask);

    return container;
}

static gboolean
mprofile_read_header1(const guchar *buffer,
                      gsize size,
                      MProFile1 *mprofile,
                      GError **error)
{
    const guchar *p;
    guint i;

    if (size < HEADER_SIZE1 + 2) {
        err_TOO_SHORT(error);
        return FALSE;
    }

    p = buffer;
    mprofile->magic_number = gwy_get_gint32_be(&p);
    mprofile->header_format = gwy_get_gint16_be(&p);
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

    mprofile->swinfo_type = gwy_get_gint16_be(&p);
    get_CHARARRAY(mprofile->swinfo_date, &p);
    gwy_debug("software_type: %d, software_date: %s",
              mprofile->swinfo_type, mprofile->swinfo_date);

    mprofile->swinfo_vers_maj = gwy_get_gint16_be(&p);
    mprofile->swinfo_vers_min = gwy_get_gint16_be(&p);
    mprofile->swinfo_vers_bug = gwy_get_gint16_be(&p);
    gwy_debug("version: %d.%d.%d",
              mprofile->swinfo_vers_maj,
              mprofile->swinfo_vers_min,
              mprofile->swinfo_vers_bug);

    mprofile->ac_org_x = gwy_get_gint16_be(&p);
    mprofile->ac_org_y = gwy_get_gint16_be(&p);
    mprofile->ac_width = gwy_get_gint16_be(&p);
    mprofile->ac_height = gwy_get_gint16_be(&p);
    gwy_debug("INTENS xres: %d, yres: %d, xoff: %d, yoff: %d",
              mprofile->ac_width, mprofile->ac_height,
              mprofile->ac_org_x, mprofile->ac_org_y);

    mprofile->ac_n_buckets = gwy_get_gint16_be(&p);
    mprofile->ac_range = gwy_get_gint16_be(&p);
    mprofile->ac_n_bytes = gwy_get_gint32_be(&p);
    gwy_debug("intens_nbytes: %d, expecting: %d",
              mprofile->ac_n_bytes,
              2*mprofile->ac_width*mprofile->ac_height*mprofile->ac_n_buckets);

    mprofile->cn_org_x = gwy_get_gint16_be(&p);
    mprofile->cn_org_y = gwy_get_gint16_be(&p);
    mprofile->cn_width = gwy_get_gint16_be(&p);
    mprofile->cn_height = gwy_get_gint16_be(&p);
    gwy_debug("PHASE xres: %d, yres: %d, xoff: %d, yoff: %d",
              mprofile->cn_width, mprofile->cn_height,
              mprofile->cn_org_x, mprofile->cn_org_y);

    mprofile->cn_n_bytes = gwy_get_gint32_be(&p);
    gwy_debug("phase_nbytes: %d, expecting: %d",
              mprofile->cn_n_bytes,
              4*mprofile->cn_width*mprofile->cn_height);

    mprofile->time_stamp = gwy_get_gint32_be(&p);
    get_CHARARRAY(mprofile->comment, &p);
    gwy_debug("comment: %s", mprofile->comment);

    mprofile->source = gwy_get_gint16_be(&p);
    mprofile->intf_scale_factor = gwy_get_gfloat_be(&p);
    mprofile->wavelength_in = gwy_get_gfloat_be(&p);
    mprofile->num_aperture = gwy_get_gfloat_be(&p);
    mprofile->obliquity_factor = gwy_get_gfloat_be(&p);
    mprofile->magnification = gwy_get_gfloat_be(&p);
    mprofile->lateral_res = gwy_get_gfloat_be(&p);
    mprofile->acq_type = gwy_get_gint16_be(&p);
    mprofile->intens_avg_cnt = gwy_get_gint16_be(&p);
    mprofile->ramp_cal = gwy_get_gint16_be(&p);
    mprofile->sfac_limit = gwy_get_gint16_be(&p);
    mprofile->ramp_gain = gwy_get_gint16_be(&p);
    mprofile->part_thickness = gwy_get_gfloat_be(&p);
    mprofile->sw_llc = gwy_get_gint16_be(&p);
    mprofile->target_range = gwy_get_gfloat_be(&p);
    mprofile->rad_drv_veasure_eeq = gwy_get_gint16_le(&p);
    mprofile->min_mod = gwy_get_gint32_be(&p);
    mprofile->min_mod_count = gwy_get_gint32_be(&p);
    mprofile->phase_res = gwy_get_gint16_be(&p);
    mprofile->min_area = gwy_get_gint32_be(&p);
    mprofile->discon_action = gwy_get_gint16_be(&p);
    mprofile->discon_filter = gwy_get_gfloat_be(&p);
    mprofile->connect_order = gwy_get_gint16_be(&p);
    mprofile->sign = gwy_get_gint16_be(&p);
    mprofile->camera_width = gwy_get_gint16_be(&p);
    mprofile->camera_height = gwy_get_gint16_be(&p);
    mprofile->sys_type = gwy_get_gint16_be(&p);
    mprofile->sys_board = gwy_get_gint16_be(&p);
    mprofile->sys_serial = gwy_get_gint16_be(&p);
    mprofile->inst_id = gwy_get_gint16_be(&p);
    get_CHARARRAY(mprofile->obj_name, &p);
    get_CHARARRAY(mprofile->part_name, &p);
    mprofile->codev_type = gwy_get_gint16_be(&p);
    mprofile->phase_avg_cnt = gwy_get_gint16_be(&p);
    mprofile->sub_sys_err = gwy_get_gint16_be(&p);
    p += 16;
    get_CHARARRAY(mprofile->part_ser_num, &p);
    mprofile->refractive_index = gwy_get_gfloat_be(&p);
    mprofile->rem_tilt_bias = gwy_get_gint16_be(&p);
    mprofile->rem_fringes = gwy_get_gint16_be(&p);
    mprofile->max_area = gwy_get_gint32_be(&p);
    mprofile->setup_type = gwy_get_gint16_be(&p);
    mprofile->wrapped = gwy_get_gint16_be(&p);
    mprofile->pre_connect_filter = gwy_get_gfloat_be(&p);
    mprofile->wavelength_in_2 = gwy_get_gfloat_be(&p);
    mprofile->wavelength_fold = gwy_get_gint16_be(&p);
    mprofile->wavelength_in_1 = gwy_get_gfloat_be(&p);
    mprofile->wavelength_in_3 = gwy_get_gfloat_be(&p);
    mprofile->wavelength_in_4 = gwy_get_gfloat_be(&p);
    get_CHARARRAY(mprofile->wavelen_select, &p);
    mprofile->fda_res = gwy_get_gint16_be(&p);
    get_CHARARRAY(mprofile->scan_descr, &p);
    gwy_debug("scan_description: %s", mprofile->scan_descr);

    mprofile->n_fiducials_a = gwy_get_gint16_be(&p);
    for (i = 0; i < G_N_ELEMENTS(mprofile->fiducials_a); i++)
        mprofile->fiducials_a[i] = gwy_get_gfloat_be(&p);
    mprofile->pixel_width = gwy_get_gfloat_be(&p);
    mprofile->pixel_height = gwy_get_gfloat_be(&p);
    mprofile->exit_pupil_diam = gwy_get_gfloat_be(&p);
    mprofile->light_level_pct = gwy_get_gfloat_be(&p);
    mprofile->coords_state = gwy_get_gint32_le(&p);
    mprofile->coords_x_pos = gwy_get_gfloat_le(&p);
    mprofile->coords_y_pos = gwy_get_gfloat_le(&p);
    mprofile->coords_z_pos = gwy_get_gfloat_le(&p);
    mprofile->coords_x_rot = gwy_get_gfloat_le(&p);
    mprofile->coords_y_rot = gwy_get_gfloat_le(&p);
    mprofile->coords_z_rot = gwy_get_gfloat_le(&p);
    mprofile->coherence_mode = gwy_get_gint16_le(&p);
    mprofile->surface_filter = gwy_get_gint16_le(&p);
    get_CHARARRAY(mprofile->sys_err_file_name, &p);
    get_CHARARRAY(mprofile->zoom_descr, &p);
    mprofile->alpha_part = gwy_get_gfloat_le(&p);
    mprofile->beta_part = gwy_get_gfloat_le(&p);
    mprofile->dist_part = gwy_get_gfloat_le(&p);
    mprofile->cam_split_loc_x = gwy_get_gint16_le(&p);
    mprofile->cam_split_loc_y = gwy_get_gint16_le(&p);
    mprofile->cam_split_trans_x = gwy_get_gint16_le(&p);
    mprofile->cam_split_trans_y = gwy_get_gint16_le(&p);
    get_CHARARRAY(mprofile->material_a, &p);
    get_CHARARRAY(mprofile->material_b, &p);
    mprofile->cam_split_unused = gwy_get_gint16_le(&p);
    p += 2;
    mprofile->dmi_ctr_x = gwy_get_gfloat_le(&p);
    mprofile->dmi_ctr_y = gwy_get_gfloat_le(&p);
    mprofile->sph_dist_corr = gwy_get_gint16_le(&p);
    p += 2;
    mprofile->sph_dist_part_na = gwy_get_gfloat_le(&p);
    mprofile->sph_dist_part_radius = gwy_get_gfloat_le(&p);
    mprofile->sph_dist_cal_na = gwy_get_gfloat_le(&p);
    mprofile->sph_dist_cal_radius = gwy_get_gfloat_le(&p);
    mprofile->surface_type = gwy_get_gint16_le(&p);
    mprofile->ac_surface_type = gwy_get_gint16_le(&p);
    mprofile->z_zosition = gwy_get_gfloat_le(&p);
    mprofile->power_rultiplier = gwy_get_gfloat_le(&p);
    mprofile->focus_sultiplier = gwy_get_gfloat_le(&p);
    mprofile->rad_drv_vocus_sal_lactor = gwy_get_gfloat_le(&p);
    mprofile->rad_drv_vower_ral_lactor = gwy_get_gfloat_le(&p);
    mprofile->ftp_left_pos = gwy_get_gfloat_le(&p);
    mprofile->ftp_right_pos = gwy_get_gfloat_le(&p);
    mprofile->ftp_pitch_pos = gwy_get_gfloat_le(&p);
    mprofile->ftp_roll_pos = gwy_get_gfloat_le(&p);
    mprofile->min_mod_pct = gwy_get_gfloat_le(&p);
    mprofile->max_inten = gwy_get_gint32_le(&p);
    mprofile->ring_of_fire = gwy_get_gint16_le(&p);
    p += 1;
    mprofile->rc_orientation = *(p++);
    mprofile->rc_distance = gwy_get_gfloat_le(&p);
    mprofile->rc_angle = gwy_get_gfloat_le(&p);
    mprofile->rc_diameter = gwy_get_gfloat_le(&p);
    mprofile->rem_fringes_mode = gwy_get_gint16_be(&p);
    p += 1;
    mprofile->ftpsi_phase_res = *(p++);
    mprofile->frames_acquired = gwy_get_gint16_le(&p);
    mprofile->cavity_type = gwy_get_gint16_le(&p);
    mprofile->cam_frame_rate = gwy_get_gfloat_le(&p);
    mprofile->tune_range = gwy_get_gfloat_le(&p);
    mprofile->cal_pix_loc_x = gwy_get_gint16_le(&p);
    mprofile->cal_pix_loc_y = gwy_get_gint16_le(&p);
    mprofile->n_tst_cal_pts = gwy_get_gint16_le(&p);
    mprofile->n_ref_cal_pts = gwy_get_gint16_le(&p);
    for (i = 0; i < G_N_ELEMENTS(mprofile->tst_cal_pts); i++)
        mprofile->tst_cal_pts[i] = gwy_get_gfloat_le(&p);
    for (i = 0; i < G_N_ELEMENTS(mprofile->ref_cal_pts); i++)
        mprofile->ref_cal_pts[i] = gwy_get_gfloat_le(&p);
    mprofile->tst_cal_pix_opd = gwy_get_gfloat_le(&p);
    mprofile->ref_cal_pix_opd = gwy_get_gfloat_le(&p);
    mprofile->sys_serial2 = gwy_get_gint32_le(&p);
    mprofile->flash_phase_dc_mask = gwy_get_gfloat_le(&p);
    mprofile->flash_phase_alias_mask = gwy_get_gfloat_le(&p);
    mprofile->flash_phase_filter = gwy_get_gfloat_le(&p);
    mprofile->scan_direction = *(p++);
    p += 1;
    mprofile->pre_fda_filter = gwy_get_gint16_be(&p);
    p += 4;
    mprofile->ftpsi_res_factor = gwy_get_gint32_le(&p);
    p += 8;

    return TRUE;
}

static void
set_units1(GwyDataField *dfield,
           const MProFile1 *mprofile,
           const gchar *zunit)
{
    GwySIUnit *siunit;

    if (mprofile->lateral_res)
        siunit = gwy_si_unit_new("m");
    else
        siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new(zunit);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);
}

static gint
fill_data_fields1(MProFile1 *mprofile,
                  const guchar *buffer)
{
    GwyDataField *dfield, *vpmask;
    gdouble *data, *mask;
    gdouble xreal, yreal, q;
    const guchar *p;
    guint n, id, i, j, ndata;

    ndata = 0;
    mprofile->intensity_data = NULL;
    mprofile->intensity_mask = NULL;
    mprofile->phase_data = NULL;
    mprofile->phase_mask = NULL;

    p = buffer + mprofile->header_size;

    /* Intensity data */
    n = mprofile->ac_width * mprofile->ac_height;
    /* Enorce consistency */
    if (!n && mprofile->ac_n_buckets) {
        g_warning("ac_n_buckets > 0, but intensity data have zero dimension");
        mprofile->ac_n_buckets = 0;
    }

    if (mprofile->ac_n_buckets) {
        const guint16 *d16;

        mprofile->intensity_data = g_new(GwyDataField*, mprofile->ac_n_buckets);
        mprofile->intensity_mask = g_new(GwyDataField*, mprofile->ac_n_buckets);

        q = mprofile->sign ? -1.0 : 1.0;

        if (mprofile->lateral_res) {
            xreal = mprofile->ac_width * mprofile->lateral_res;
            yreal = mprofile->ac_height * mprofile->lateral_res;
        }
        else {
            /* whatever */
            xreal = mprofile->ac_width;
            yreal = mprofile->ac_height;
        }

        for (id = 0; id < mprofile->ac_n_buckets; id++) {
            ndata++;
            dfield = gwy_data_field_new(mprofile->ac_width,
                                        mprofile->ac_height,
                                        xreal, yreal,
                                        FALSE);
            vpmask = gwy_data_field_new_alike(dfield, FALSE);
            gwy_data_field_fill(vpmask, 1.0);
            data = gwy_data_field_get_data(dfield);
            mask = gwy_data_field_get_data(vpmask);
            d16 = (const guint16*)p;
            for (i = 0; i < mprofile->ac_height; i++) {
                for (j = 0; j < mprofile->ac_width; j++) {
                    guint v16 = GUINT16_FROM_BE(*d16);
                    if (v16 >= 65412) {
                        mask[i*mprofile->ac_width + j] = 0.0;
                    }
                    else
                        *data = q*v16;
                    d16++;
                    data++;
                }
            }

            set_units1(dfield, mprofile, "");
            if (!gwy_app_channel_remove_bad_data(dfield, vpmask))
                gwy_object_unref(vpmask);

            mprofile->intensity_data[id] = dfield;
            mprofile->intensity_mask[id] = vpmask;
            p += sizeof(guint16)*n;
        }
    }

    /* Phase data */
    n = mprofile->cn_width * mprofile->cn_height;
    if (n) {
        const gint32 *d32;
        gint32 d;

        ndata++;

        i = 4096;
        if (mprofile->phase_res == 1)
            i = 32768;

        q = mprofile->intf_scale_factor * mprofile->obliquity_factor
            * mprofile->wavelength_in/i;
        if (mprofile->sign)
            q = -q;
        gwy_debug("q: %g", q);

        if (mprofile->lateral_res) {
            xreal = mprofile->cn_width * mprofile->lateral_res;
            yreal = mprofile->cn_height * mprofile->lateral_res;
        }
        else {
            /* whatever */
            xreal = mprofile->cn_width;
            yreal = mprofile->cn_height;
        }
        dfield = gwy_data_field_new(mprofile->cn_width,
                                    mprofile->cn_height,
                                    xreal, yreal,
                                    FALSE);
        vpmask = gwy_data_field_new_alike(dfield, FALSE);
        gwy_data_field_fill(vpmask, 1.0);
        data = gwy_data_field_get_data(dfield);
        mask = gwy_data_field_get_data(vpmask);
        d32 = (const gint32*)p;
        for (i = 0; i < mprofile->cn_height; i++) {
            for (j = 0; j < mprofile->cn_width; j++) {
                d = GINT32_FROM_BE(*d32);
                if (d >= 2147483640) {
                    mask[i*mprofile->cn_width + j] = 0.0;
                }
                else
                    *data = q*d;
                d32++;
                data++;
            }
        }

        set_units1(dfield, mprofile, "m");
        if (!gwy_app_channel_remove_bad_data(dfield, vpmask))
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
mprofile_get_metadata1(MProFile1 *mprofile)
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
                        mprofile->swinfo_vers_maj,
                        mprofile->swinfo_vers_min,
                        mprofile->swinfo_vers_bug);
    gwy_container_set_string_by_name(meta, "Version", p);

    /* Timestamp */
    tp = mprofile->time_stamp;
    tm = localtime(&tp);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
    gwy_container_set_string_by_name(meta, "Date", g_strdup(buffer));

    /* Comments */
    store_meta_string(meta, "Software date",
                      mprofile->swinfo_date);
    store_meta_string(meta, "Comment",
                      mprofile->comment);
    store_meta_string(meta, "Objective name",
                      mprofile->obj_name);
    store_meta_string(meta, "Part measured",
                      mprofile->part_name);
    store_meta_string(meta, "Part serial number",
                      mprofile->part_ser_num);
    store_meta_string(meta, "Description",
                      mprofile->scan_descr);
    store_meta_string(meta, "System error file",
                      mprofile->sys_err_file_name);
    store_meta_string(meta, "Zoom description",
                      mprofile->zoom_descr);
    store_meta_string(meta, "Wavelength select",
                      mprofile->wavelen_select);

    /* Misc */
    HASH_STORE_ENUM("Software type", swinfo_type, software_types);
    HASH_STORE("Wavelength", "%g m", wavelength_in);
    HASH_STORE("Intensity averages", "%d", intens_avg_cnt);
    HASH_STORE("Minimum modulation points", "%d", min_mod_count);
    HASH_STORE_ENUM("Discontinuity action", discon_action, discont_actions);
    HASH_STORE("Discontinuity filter", "%g %%", discon_filter);
    HASH_STORE_ENUM("System type", sys_type, system_types);
    HASH_STORE("System board", "%d", sys_board);
    HASH_STORE("System serial", "%d", sys_serial);
    HASH_STORE("Instrument id", "%d", inst_id);
    HASH_STORE_ENUM("System error subtracted", sub_sys_err, yesno);
    HASH_STORE("Refractive index", "%g", refractive_index);
    HASH_STORE_ENUM("Removed tilt bias", rem_tilt_bias, yesno);
    HASH_STORE_ENUM("Removed fringes", rem_fringes, yesno);
    HASH_STORE_ENUM("Wavelength folding", wavelength_fold, yesno);

    p = g_strdup_printf("%.2g", mprofile->min_mod/10.23);
    gwy_container_set_string_by_name(meta, "Minimum modulation", p);

    return meta;
}

static GwyContainer*
mprofile_load2(const gchar *filename,
               const guchar *buffer,
               gsize size,
               GError **error)
{
    err_UNSUPPORTED(error, "header_format");
    return NULL;
}

static GwyContainer*
mprofile_load3(const gchar *filename,
               const guchar *buffer,
               gsize size,
               GError **error)
{
    err_UNSUPPORTED(error, "header_format");
    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
