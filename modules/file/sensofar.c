/*
 *  @(#) $Id: sensofar.c 8632 2007-10-11 07:59:01Z yeti-dn $
 *  Copyright (C) 2004-2007 David Necas (Yeti), Petr Klapetek, Jan Horak.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, xhorak@gmail.com.
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
/* TODO: multiprofile, metadata */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-sensofar-spm">
 *   <comment>Sensofar PLu data</comment>
 *   <glob pattern="*.plu"/>
 *   <glob pattern="*.PLU"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Sensofar PLu v2000
 * .plu
 * Read
 **/

#include "config.h"
#include <stdio.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define Micrometer (1e-6)

enum {
    DATE_SIZE = 128,
    COMMENT_SIZE = 256,
    HEADER_SIZE = 500,
};

typedef enum {
    MES_IMATGE      = 0,
    MES_PERFIL      = 1,
    MES_MULTIPERFIL = 2,
    MES_TOPO        = 3,
    MES_COORDENADES = 4,
    MES_GRUIX       = 5,
    MES_CUSTOM      = 6
} MeasurementType;

typedef enum {
    ALGOR_CONFOCAL_INTENSITY        = 0,
    ALGOR_CONFOCAL_GRADIENT         = 1,
    ALGOR_INTERFEROMETRIC_PSI       = 2,
    ALGOR_INTERFEROMETRIC_VSI       = 3,
    ALGOR_INTERFEROMETRIC_ePSI      = 3,
    ALGOR_CONFOCAL_THICKNESS        = 4,
    ALGOR_INTERFEROMETRIC_THICKNESS = 5,
} AcquisitionAlgorithm;

/* This seems to be context-dependent */
typedef enum {
    METHOD_CONVENTIONAL              = 0,
    METHOD_CONFOCAL                  = 1,
    METHOD_SINGLE_PROFILE            = 0,
    METHOD_EXTENDED_PROFILE          = 1,
    METHOD_TOPOGRAPHY                = 0,
    METHOD_EXTENDED_TOPOGRAPHY       = 1,
    METHOD_MULTIPLE_PROFILE          = 0,
    METHOD_EXTENDED_MULTIPLE_PROFILE = 1,
} MethodType;

typedef enum {
    OBJ_SLWD_10x  = 0,
    OBJ_SLWD_20x  = 1,
    OBJ_SLWD_50x  = 2,
    OBJ_SLWD_100x = 3,
    OBJ_EPI_20x   = 4,
    OBJ_EPI_50x   = 5,
    OBJ_EPI_10x   = 6,
    OBJ_EPI_100x  = 7,
    OBJ_ELWD_10x  = 8,
    OBJ_ELWD_20x  = 9,
    OBJ_ELWD_50x  = 10,
    OBJ_ELWD_100x = 11,
    OBJ_TI_2_5x   = 12,
    OBJ_TI_5x     = 13,
    OBJ_DI_10x    = 14,
    OBJ_DI_20x    = 15,
    OBJ_DI_50x    = 16,
    OBJ_EPI_5x    = 17,
    OBJ_EPI_150x  = 18,
} ObjectiveType;

typedef enum {
    AREA_128  = 0,
    AREA_256  = 1,
    AREA_512  = 2,
    AREA_MAX  = 3,
    AREA_L256 = 4,
    AREA_L128 = 5,
} AreaType;

typedef enum {
    PLU             = 0,
    PLU_2300_XGA    = 1,
    PLU_2300_XGA_T5 = 2,
    PLU_2300_SXGA   = 3,
    PLU_3300        = 4,
} HardwareConfiguration;

typedef enum {
    FORMAT_VERSION_2000 = 0,
    FORMAT_VERSION_2006 = 255,
} FormatVersion;

typedef struct {
    char str[DATE_SIZE];
    time_t t;
} SensofarDate;

typedef struct {
    MeasurementType type;
    AcquisitionAlgorithm algorithm;
    MethodType method;
    ObjectiveType objective;
    AreaType area;
    gint32 xres_area;
    gint32 yres_area;
    gint32 xres;
    gint32 yres;
    gint32 na;
    gdouble incr_z;
    gdouble range;
    gint32 n_planes;
    gint32 tpc_umbral_F;
    gboolean restore;
    guint num_layers;
    FormatVersion version;
    HardwareConfiguration config_hardware;
    guint stack_im_num;
    guint reserved;
    gint32 factor_delmacio;
} SensofarConfigMesura;

typedef struct  {
    guint32 yres;
    guint32 xres;
    guint32 N_tall;
    gdouble dy_multip;
    gdouble mppx;
    gdouble mppy;
    gdouble x_0;
    gdouble y_0;
    gdouble mpp_tall;
    gdouble z_0;
} SensofarCalibratEixos_Arxiu;

typedef struct {
    SensofarDate date;
    gchar user_comment[COMMENT_SIZE];
    SensofarCalibratEixos_Arxiu axes_config;
    SensofarConfigMesura measure_config;
} SensofarDataDesc;

static gboolean      module_register         (void);
static gint          sensofar_detect         (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* sensofar_load           (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static GwyDataField* sensofar_read_data_field(SensofarDataDesc *data_desc,
                                              GwyDataField **maskfield,
                                              const guchar **p,
                                              gsize size,
                                              GError **error);
static GwyGraphModel* sensofar_read_profile  (SensofarDataDesc *data_desc,
                                              const guchar **p,
                                              gsize size,
                                              GError **error);
static gboolean      parses_as_date          (const gchar *str);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Sensofar PLu file format, "
       "version 2000 or newer."),
    "Jan Hořák <xhorak@gmail.com>, Yeti <yeti@gwyddion.net>",
    "0.3",
    "David Nečas (Yeti) & Jan Hořák",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("sensofar",
                           N_("Sensofar PLu files (.plu)"),
                           (GwyFileDetectFunc)&sensofar_detect,
                           (GwyFileLoadFunc)&sensofar_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
sensofar_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, ".plu") ? 20 : 0;

    if (fileinfo->file_size >= HEADER_SIZE + 12
        && fileinfo->buffer_len > 24 && parses_as_date(fileinfo->head))
        return 85;

    return 0;
}

static GwyContainer*
sensofar_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    SensofarDataDesc data_desc;
    GwyContainer *container = NULL;
    GwyDataField *dfield, *mfield;
    GwyGraphModel *gmodel;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    const guchar *p;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < HEADER_SIZE + 12) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated"));
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    /* Date block */
    p = buffer;
    memcpy(&data_desc.date.str, p, DATE_SIZE);
    data_desc.date.str[DATE_SIZE-1] = '\0';
    p += DATE_SIZE;
    data_desc.date.t = gwy_get_guint32_le(&p);

    /* Comment block */
    memcpy(&data_desc.user_comment, p, COMMENT_SIZE);
    data_desc.user_comment[COMMENT_SIZE-1] = '\0';
    p += COMMENT_SIZE;

    /* Calbration block */
    data_desc.axes_config.yres = gwy_get_guint32_le(&p);
    data_desc.axes_config.xres = gwy_get_guint32_le(&p);
    data_desc.axes_config.N_tall = gwy_get_guint32_le(&p);
    data_desc.axes_config.dy_multip = gwy_get_gfloat_le(&p);
    data_desc.axes_config.mppx = gwy_get_gfloat_le(&p);
    data_desc.axes_config.mppy = gwy_get_gfloat_le(&p);
    data_desc.axes_config.x_0 = gwy_get_gfloat_le(&p);
    data_desc.axes_config.y_0 = gwy_get_gfloat_le(&p);
    data_desc.axes_config.mpp_tall = gwy_get_gfloat_le(&p);
    data_desc.axes_config.z_0 = gwy_get_gfloat_le(&p);

    /* Measurement block */
    data_desc.measure_config.type = gwy_get_guint32_le(&p);
    data_desc.measure_config.algorithm = gwy_get_guint32_le(&p);
    data_desc.measure_config.method = gwy_get_guint32_le(&p);
    data_desc.measure_config.objective = gwy_get_guint32_le(&p);
    data_desc.measure_config.area = gwy_get_guint32_le(&p);
    data_desc.measure_config.xres_area = gwy_get_guint32_le(&p);
    data_desc.measure_config.yres_area = gwy_get_guint32_le(&p);
    data_desc.measure_config.xres = gwy_get_guint32_le(&p);
    data_desc.measure_config.yres = gwy_get_guint32_le(&p);
    data_desc.measure_config.na = gwy_get_guint32_le(&p);
    data_desc.measure_config.incr_z = gwy_get_gdouble_le(&p);
    data_desc.measure_config.range = gwy_get_gfloat_le(&p);
    data_desc.measure_config.n_planes = gwy_get_guint32_le(&p);
    data_desc.measure_config.tpc_umbral_F = gwy_get_guint32_le(&p);
    data_desc.measure_config.restore = gwy_get_gboolean8(&p);
    data_desc.measure_config.num_layers = *(p++);
    data_desc.measure_config.version = *(p++);
    data_desc.measure_config.config_hardware = *(p++);
    data_desc.measure_config.stack_im_num = *(p++);
    data_desc.measure_config.reserved = *(p++);
    p += 2; // struct padding
    data_desc.measure_config.factor_delmacio = gwy_get_guint32_le(&p);

    gwy_debug("File date=<%s>, data type=%d, xres=%d, yres=%d, version=%d", 
              data_desc.date.str, 
              data_desc.measure_config.type, 
              data_desc.measure_config.xres, 
              data_desc.measure_config.yres,
              data_desc.measure_config.version);

    switch (data_desc.measure_config.type) {
        case MES_TOPO:
        case MES_IMATGE:
        dfield = sensofar_read_data_field(&data_desc, &mfield,
                                          &p, size - (p - buffer), error);
        if (!dfield) {
            gwy_file_abandon_contents(buffer, size, NULL);
            return NULL;
        }

        container = gwy_container_new();
        gwy_container_set_object(container, gwy_app_get_data_key_for_id(0),
                                 dfield);
        g_object_unref(dfield);
        if (mfield) {
            gwy_container_set_object(container, gwy_app_get_mask_key_for_id(0),
                                     mfield);
            g_object_unref(mfield);
        }
        gwy_app_channel_title_fall_back(container, 0);
        break;

        case MES_PERFIL:
        case MES_GRUIX:
        gmodel = sensofar_read_profile(&data_desc,
                                       &p, size - (p - buffer), error);
        if (!gmodel) {
            gwy_file_abandon_contents(buffer, size, NULL);
            return NULL;
        }

        container = gwy_container_new();
        gwy_container_set_object(container, gwy_app_get_graph_key_for_id(0),
                                 gmodel);
        g_object_unref(gmodel);
        break;

        default:
        err_DATA_TYPE(error, data_desc.measure_config.type);
        break;
    }

    return container;
}

static GwyDataField*
sensofar_read_data_field(SensofarDataDesc *data_desc,
                         GwyDataField **maskfield,
                         const guchar **p,
                         gsize size,
                         GError **error)
{
    GwyDataField *dfield, *mfield;
    guint xres, yres, i, j, mcount;
    GwySIUnit *units = NULL;
    gdouble *data, *mdata;

    if (maskfield)
        *maskfield = NULL;

    yres = gwy_get_guint32_le(p);
    xres = gwy_get_guint32_le(p);
    gwy_debug("Data size: %dx%d", xres, yres);
    if (err_SIZE_MISMATCH(error, xres*yres*sizeof(gfloat),
                          size - 2*sizeof(guint32), FALSE))
        return NULL;
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        return NULL;
    if (!((data_desc->axes_config.mppx
           = fabs(data_desc->axes_config.mppx)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        data_desc->axes_config.mppx = 1.0;
    }
    if (!((data_desc->axes_config.mppy
           = fabs(data_desc->axes_config.mppy)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        data_desc->axes_config.mppy = 1.0;
    }

    dfield = gwy_data_field_new(xres, yres, 
                                data_desc->axes_config.mppx * xres * Micrometer,
                                data_desc->axes_config.mppy * yres * Micrometer,
                                FALSE);
    units = gwy_si_unit_new("m"); // values are in um only
    gwy_data_field_set_si_unit_xy(dfield, units);
    g_object_unref(units);

    units = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, units);
    g_object_unref(units);

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    gwy_data_field_fill(mfield, 1.0);

    data = gwy_data_field_get_data(dfield);
    mdata = gwy_data_field_get_data(mfield);
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble v = gwy_get_gfloat_le(p);
            if (v == 1000001.0)
                mdata[i*xres + j] = 0.0;
            else
                data[i*xres + j] = v*Micrometer;
        }
    }

    gwy_debug("Offset: %g %g",
              data_desc->axes_config.x_0, data_desc->axes_config.y_0);
    //FIXME: offset later, support of offset determined by version?
    //gwy_data_field_set_xoffset(d, pow10(power10)*data_desc.axes_config.x_0);
    //gwy_data_field_set_yoffset(d, pow10(power10)*data_desc.axes_config.y_0);

    mcount = gwy_app_channel_remove_bad_data(dfield, mfield);

    if (maskfield && mcount)
        *maskfield = mfield;
    else
        g_object_unref(mfield);

    return dfield;
}

static GwyGraphModel*
sensofar_read_profile(SensofarDataDesc *data_desc,
                      const guchar **p,
                      gsize size,
                      GError **error)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    guint xres, yres, j, n;
    GwySIUnit *units = NULL;
    gdouble *xdata, *ydata;
    gdouble dx;

    yres = gwy_get_guint32_le(p);
    if (yres != 1)
        g_warning("ysize is not 1 for profile");
    xres = gwy_get_guint32_le(p);
    gwy_debug("Data size: %dx%d", xres, yres);
    if (err_SIZE_MISMATCH(error, xres*yres*sizeof(gfloat),
                          size - 2*sizeof(guint32), FALSE))
        return NULL;
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        return NULL;
    if (!((data_desc->axes_config.mppx
           = fabs(data_desc->axes_config.mppx)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        data_desc->axes_config.mppx = 1.0;
    }

    xdata = g_new(gdouble, xres);
    ydata = g_new(gdouble, xres);
    dx = data_desc->axes_config.mppx * Micrometer;

    for (j = n = 0; j < xres; j++) {
        gdouble v = gwy_get_gfloat_le(p);
        if (v != 1000001.0) {
            xdata[n] = dx*j;
            ydata[n] = v*Micrometer;
            n++;
        }
    }

    if (!n) {
        g_free(xdata);
        g_free(ydata);
        err_NO_DATA(error);
        return NULL;
    }

    gmodel = gwy_graph_model_new();
    g_object_set(gmodel, "title", _("Profile"), NULL);

    units = gwy_si_unit_new("m"); // values are in um only
    g_object_set(gmodel, "si-unit-x", units, NULL);
    g_object_unref(units);

    units = gwy_si_unit_new("m"); // values are in um only
    g_object_set(gmodel, "si-unit-y", units, NULL);
    g_object_unref(units);

    gcmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, n);
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "description", _("Profile"),
                 NULL);
    gwy_graph_model_add_curve(gmodel, gcmodel);
    g_object_unref(gcmodel);

    return gmodel;
}

/* File starts with date, try to parse it.
 * FIXME: this is stupid */
static gboolean
parses_as_date(const gchar *str)
{
    char day_name[4], month_name[4];
    int month_day, hour, min, sec, year;

    if (str[24] != '\0')
        return FALSE;

    if (sscanf(str, "%3s %3s %u %u:%u:%u %u", 
               day_name, month_name, &month_day, &hour, &min, &sec, &year) != 7)
        return FALSE;

    if (strlen(day_name) != 3 || strlen(month_name) != 3)
        return FALSE;

    if (!gwy_stramong(day_name,
                      "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", NULL))
        return FALSE;

    if (!gwy_stramong(month_name,
                      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL))
        return FALSE;

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
