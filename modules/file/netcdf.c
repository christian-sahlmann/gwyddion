/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek, Niv Levy.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, nivl2000@gmail.com.
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
 * XXX: Disabled, it conflicts with MIME types installed by other packages
 * way too often.  (It is this comment that disables the magic, do not remove
 * it.)
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-netcdf">
 *   <comment>NetCDF data</comment>
 *   <magic priority="10">
 *     <match type="string" offset="0" value="CDF\x01"/>
 *     <match type="string" offset="0" value="CDF\x02"/>
 *   </magic>
 *   <glob pattern="*.nc"/>
 *   <glob pattern="*.NC"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * GSXM NetCDF
 * .nc
 * Read
 **/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libprocess/brick.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC1 "CDF\x01"
#define MAGIC2 "CDF\x02"
#define MAGIC_SIZE (sizeof(MAGIC1) - 1)

#define EXTENSION ".nc"

/* Align a number to nearest larger multiple of 4 as things are aligned in
 * NetCDF */
#define ALIGN4(n) (n) += (4 - (n) % 4) % 4

enum {
    MIN_SIZE = 32
};

typedef enum {
    NETCDF_CLASSIC = 1,
    NETCDF_64BIT   = 2
} NetCDFVersion;

typedef enum {
    NC_BYTE      = 1,
    NC_CHAR      = 2,
    NC_SHORT     = 3,
    NC_INT       = 4,
    NC_FLOAT     = 5,
    NC_DOUBLE    = 6,
    NC_DIMENSION = 10,
    NC_VARIABLE  = 11,
    NC_ATTRIBUTE = 12
} NetCDFType;

typedef struct {
    gchar *name;
    gint length;
} NetCDFDim;

typedef struct {
    gchar *name;
    NetCDFType type;
    gint nelems;
    gconstpointer values;
} NetCDFAttr;

typedef struct {
    gchar *name;
    gint ndims;
    gint *dimids;
    gint nattrs;
    NetCDFAttr *attrs;
    NetCDFType type;
    gint vsize;
    /* FIXME: We cannot handle large files on 32bit systems anyway. */
    guint64 begin;
} NetCDFVar;

typedef struct {
    NetCDFVersion version;
    gint nrecs;
    gint ndims;
    NetCDFDim *dims;
    gint nattrs;
    NetCDFAttr *attrs;
    gint nvars;
    NetCDFVar *vars;
    /* our stuff to easily pass around the raw data */
    guchar *buffer;
    gsize size;
    gsize data_start;
} NetCDF;

static gboolean      module_register        (void);
static gint          gxsm_detect            (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer* gxsm_load              (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static gboolean      cdffile_load           (NetCDF *cdffile,
                                             const gchar *filename,
                                             GError **error);
static GwySIUnit*    read_real_size         (const NetCDF *cdffile,
                                             const gchar *name,
                                             gdouble *real,
                                             gint *power10);
static GwyDataField* read_data_field        (const guchar *buffer,
                                             gint xres,
                                             gint yres,
                                             NetCDFType type,
                                              gint frame_i);
static gboolean      cdffile_read_dim_array (NetCDFDim **pdims,
                                             gint *pndims,
                                             const guchar *buf,
                                             gsize size,
                                             const guchar **p,
                                             GError **error);
static gboolean      cdffile_read_attr_array(NetCDFAttr **pattrs,
                                             gint *pnattrs,
                                             const guchar *buf,
                                             gsize size,
                                             const guchar **p,
                                             GError **error);
static gboolean      cdffile_read_var_array (NetCDFVar **pvars,
                                             gint *pnvars,
                                             NetCDFVersion version,
                                             const guchar *buf,
                                             gsize size,
                                             const guchar **p,
                                             GError **error);
static NetCDFDim*    cdffile_get_dim        (const NetCDF *cdffile,
                                             const gchar *name);
static NetCDFVar*    cdffile_get_var        (const NetCDF *cdffile,
                                             const gchar *name);
static const NetCDFAttr* cdffile_get_attr   (const NetCDFAttr *attrs,
                                             gint nattrs,
                                             const gchar *name);
static gboolean      cdffile_validate_vars  (const NetCDF *cdffile,
                                             GError **error);
static void          cdffile_free           (NetCDF *cdffile);

static GwyBrick*     read_brick            (const guchar *buffer,
                                              gint xres,
                                              gint yres,
                                              gint zres,
                                              NetCDFType type);

static GwyContainer* create_meta        (const NetCDF cdffile);
static void              add_size_to_meta   (GwyContainer* meta,
                                             GwyDataField* dfield);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports network Common Data Form (netCDF) files created by GXSM."),
    "Yeti <yeti@gwyddion.net>",
    "0.5",
    "David Nečas (Yeti), Petr Klapetek & Niv Levy",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("gsxmfile",
                           N_("GSXM netCDF files (.nc)"),
                           (GwyFileDetectFunc)&gxsm_detect,
                           (GwyFileLoadFunc)&gxsm_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
gxsm_detect(const GwyFileDetectInfo *fileinfo,
            gboolean only_name)
{
    NetCDF cdffile;
    const guchar *p;
    gint score;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    /* Weed out non-netCDF files quickly */
    if (fileinfo->buffer_len < MAGIC_SIZE
        || (memcmp(fileinfo->head, MAGIC1, MAGIC_SIZE) != 0
            && memcmp(fileinfo->head, MAGIC2, MAGIC_SIZE) != 0))
        return 0;

    score = 0;
    gwy_clear(&cdffile, 1);
    p = fileinfo->head + MAGIC_SIZE;
    cdffile.nrecs = gwy_get_guint32_be(&p);
    if (cdffile_read_dim_array(&cdffile.dims, &cdffile.ndims,
                               fileinfo->head, fileinfo->buffer_len-1, &p,
                               NULL)) {
        if (cdffile_get_dim(&cdffile, "dimx")
            && cdffile_get_dim(&cdffile, "dimy"))
            score = 80;
    }
    cdffile_free(&cdffile);

    return score;
}

static inline void
err_CDF_INTEGRITY(GError **error, const gchar *field_name)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Variable `%s' refers to invalid or nonexistent data."),
                field_name);
}

/**
 * gwy_brick_invert:
 * @brick: A brick.
 * @x: %TRUE to reflect about YZ plane.
 * @y: %TRUE to reflect about XZ plane.
 * @z: %TRUE to reflect about XY plane.
 * Reflects a brick around one of the three axes.
 * Note : xz and yz inversions are not implemented.
 * Note : this is not exactly analogous to the datafield funciton,
 * since brick inversion exists in a separate function (volume_invert)
 * Should move eventually to a public location, but for now only used here
 * and mostly untested.
 **/
void
gwy_brick_invert(GwyBrick *brick,
                      gboolean x,
                      gboolean y,
                      gboolean z)
{
    gint i, j, k, n, n_layer;
    gdouble *data, *flip;

    g_return_if_fail(GWY_IS_BRICK(brick));
    n = brick->xres * brick->yres * brick->zres;
    n_layer = brick->xres * brick->yres;

    // not implementing xz or yz flips given that we're not doing true 3D data
    // x&y&z is odd as well, but easy
    if (x && y && z) {
        data = brick->data;
        flip = data + n-1;
        for (i = 0; i < n/2; i++, data++, flip--)
            GWY_SWAP(gdouble, *data, *flip);
    }
    else if (x && y) {
        for (k = 0; k < brick->zres ; k++) {
            data = brick->data + k * n_layer;
            flip = data + n_layer -1;
            for (i = 0; i < n_layer/2; i++, data++, flip--)
                GWY_SWAP(gdouble, *data, *flip);
        }
    }
    else if ((x && z) || (y && z)) {
      return;
    }
    else if (z) { // TEST
        for (i = 0 ; i < brick->xres ; i++) {
            for (j = 0 ; j < brick->yres ; j++) {
                data = brick->data + i + j * brick->xres;
                flip = data + (brick->zres -1) * n_layer;
                for (k = 0 ; k < brick->zres/2 ; k++, data+=n_layer, flip-=n_layer)
                    GWY_SWAP(gdouble, *data, *flip);
            }
        }
    }
    else if (y) {
        for (k = 0; k < brick->zres ; k++) {
            for (i = 0; i < brick->yres; i++) {
                data = brick->data + i * brick->xres + k * n_layer;
                flip = data + brick->xres-1;
                for (j = 0; j < brick->xres/2; j++, data++, flip--)
                    GWY_SWAP(gdouble, *data, *flip);
            }
        }
    }
    else if (x) {
        for (k = 0; k < brick->zres; k++) {
            for (j = 0; j < brick->yres/2; i++) {
                data = brick->data + j*brick->xres + k * n_layer;
                flip = brick->data + (brick->yres - 1 - j) * brick->xres + k * n_layer;
                for (i = 0; i < brick->xres; i++, data++, flip++)
                    GWY_SWAP(gdouble, *data, *flip);
            }
        }
    }

    else
        return;
}


/*TODO:
 * deal with fast scan (just issue a warning statement for now)
 * deal with rotated scans
 * Find some way to avoid duplicate code between field and brick e.g. real size
 * ive meaningful field/ brick titles - useful for multidata analysis
 */
static GwyContainer*
gxsm_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    static const gchar *dimensions[] = { "time", "value", "dimy", "dimx" };
    GwyContainer *data = NULL, *meta = NULL;
    GwyDataField *dfield;
    GwyBrick *dbrick;
    GwySIUnit *siunit;
    NetCDF cdffile;
    const NetCDFDim *dim;
    const NetCDFVar *var, *field_var;
    const NetCDFAttr *attr;
    gdouble real, offset, *times;
    gint dim_time, dim_value;
    gfloat *values;
    gint i, power10, value_i, time_i, frame_i;
    const guchar *p ;
    gboolean good_time_series = FALSE, good_value_Series = FALSE;
    /*allowed deviation from linearity (beyond which we don't load as volume)*/
    const gfloat value_series_deviation = 0.01, time_series_deviation = 0.01;

    if (!cdffile_load(&cdffile, filename, error))
        return NULL;
    gwy_debug("Parsing %s", filename);

    if (cdffile.nrecs) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("NetCDF records are not supported."));
        goto gxsm_load_fail;
    }

    /* Look for variable "H" or "FloatField".  This seems to be how GXSM calls
     * data. */
    if (!(field_var = cdffile_get_var(&cdffile, "H"))
        && !(field_var = cdffile_get_var(&cdffile, "FloatField"))) {
        err_NO_DATA(error);
        goto gxsm_load_fail;
    }

    /* Checks that all these dimensions exist! */
    for (i = 0; i < field_var->ndims; i++) {
        dim = cdffile.dims + field_var->dimids[i];
        if (!gwy_strequal(dim->name, dimensions[i])) {
            err_NO_DATA(error);
            goto gxsm_load_fail;
        }
    }

    if (err_DIMENSION(error, cdffile.dims[field_var->dimids[3]].length)
    || err_DIMENSION(error, cdffile.dims[field_var->dimids[2]].length)
        || err_DIMENSION(error, cdffile.dims[field_var->dimids[1]].length)
            || err_DIMENSION(error, cdffile.dims[field_var->dimids[0]].length))
        goto gxsm_load_fail;

    /* we know these exist, so just read them */

    dim = cdffile.dims + field_var->dimids[0];
    dim_time = dim->length ;
    times = g_new(gdouble, dim_time);

    dim = cdffile.dims + field_var->dimids[1];
    dim_value = dim->length ;
    values = g_new(gfloat, dim_value);

    gwy_debug("dimensions: time = %d  value = %d", dim_time, dim_value) ;
    /* create volume data only if equally spaced - to better than some
     * arbitrary limit currently set at 1%
     * This is meant mostly for time movies where the clock can occasionnaly
     * be off by a tick, but it still would be better to have the stack in
     * that case - i think.
     * */

    if (dim_value > 1) {
        good_value_Series = TRUE;
        var = cdffile_get_var(&cdffile, "value");
        if ((attr = cdffile_get_attr(var->attrs, var->nattrs, "value"))
                    && attr->type != NC_FLOAT) {
            g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Value series values must be of type FLOAT."));
            goto gxsm_load_fail;
        }
        p = (const guchar*) (cdffile.buffer + var->begin) ;
        for (i = 0; i < dim_value; i++) {
            values[i] = gwy_get_gfloat_be(&p);
            if (i > 1 && good_value_Series && fabs(((values[i] - values[i-1])
                / (values[i-1] - values[i-2])) - 1)
                  > value_series_deviation) {
                good_value_Series = FALSE;
                g_warning("value spacing uniformity below %g - loading as 2D images",
                          value_series_deviation);
            }
        }
    }

    if (dim_time > 1) {
        good_time_series = TRUE;
        var = cdffile_get_var(&cdffile, "time");
        if ((attr = cdffile_get_attr(var->attrs, var->nattrs, "time"))
                    && attr->type != NC_DOUBLE) {
            g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Time values must be of type DOUBLE."));
            goto gxsm_load_fail;
        }
        p = (const guchar*) (cdffile.buffer + var->begin) ;
        for (i = 1; i < dim_time; i++) {
            times[i] = gwy_get_gdouble_be(&p);
            if (i > 1 && good_time_series &&
                fabs(((times[i] - times[i-1])
                / (times[i-1] - times[i-2])) - 1) > time_series_deviation) {
                    good_time_series = FALSE;
                    g_warning(
                    "time series is not equally spaced to within %g- loading as stack of 2D images",
                    time_series_deviation);
            }
        }
    }

    if ((var = cdffile_get_var(&cdffile, "sranger_mk2_hwi_fast_scan_flag"))) {
        p = (const guchar*) (cdffile.buffer + var->begin) ;
        if (gwy_get_gint16_be(&p) == 1) {
            g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_UNIMPLEMENTED,
                    _("Reading of fast scan files is not implemented - yet."));
            goto gxsm_load_fail;
        }
    }


    /* TODO : add the label of the field getting modified in gxsm2 !
     * currently it does not save it, so there is no way to know what the
     * value meant */
    data = gwy_container_new();
    /* create meta before data since i want to use it for some conditional
     * stuff*/
    meta = create_meta(cdffile) ;
    /* also FIXME -check if recent gxsm does put a name for the value*/
    gwy_container_set_object_by_name(data, "/brick/0/meta", meta);
    g_object_unref(meta);

    if ((good_value_Series && (dim_time == 1))
      ||  (good_time_series && (dim_value == 1))) {
        if (good_value_Series) {
            gwy_debug("Reading brick, xres=%d yres=%d zres=%d",
                                    cdffile.dims[field_var->dimids[3]].length,
                                    cdffile.dims[field_var->dimids[2]].length,
                                    dim_value);
            dbrick = read_brick((const guchar*)(
                                    cdffile.buffer + field_var->begin),
                                    cdffile.dims[field_var->dimids[3]].length,
                                    cdffile.dims[field_var->dimids[2]].length,
                                    dim_value,
                                    field_var->type) ;
            /* NOTE :
             * 1. flip the Z direction so that "up" is "correctly oriented"
             * note that this opens issues is one tries to correlate voltage
             * with scan time!,
             * but i don't currently have a better idea.
             * 2. The zreal calculated does not match the extreme of the
             * values.
             * I decided to leave it this way since it corresponds more to the
             * usual convention for data where the value corresponds to the
             * center of a pixel, rather than the border e.g. if one take a
             * series of images at -0.95, -0.85.... 0.95V (20 steps),
             * zreal = 2.0 rather than 1.9V. In a sense, it's treating it as
             * if we took the images in a cell at [-1,-0.9] ; it seems to be
             * self consistent.
             */
            if ((real = (values[1] - values[0]) * dim_value) < 0 ) {
                g_warning("Flipping brick since values have negative step");
                gwy_brick_invert(dbrick, FALSE, FALSE, TRUE);
                real = -real ;
                offset = values[dim_value-1];
                gwy_container_set_string_by_name(meta, "Z flipped",
                                                g_strdup("True"));
                gwy_container_set_string_by_name(meta, "Z values",
                                                g_strdup_printf("%g, %g... %g",
                                                values[dim_value-1],
                                                values[dim_value-2],
                                                values[0]));
            }
            else {
                offset = values[0];
                gwy_container_set_string_by_name(meta, "Z flipped",
                                                g_strdup("False"));
                gwy_container_set_string_by_name(meta, "Z values",
                                                g_strdup_printf("%g, %g... %g",
                                                values[0],
                                                values[1],
                                                values[dim_value-1]));
            }

            gwy_brick_set_zoffset(dbrick, offset) ;
            gwy_brick_set_zreal(dbrick, real);
            // duplicating code, yuck
            var = cdffile_get_var(&cdffile, "value");
            attr = cdffile_get_attr(var->attrs, var->nattrs, "unit");
            if (!attr->nelems)
                p = NULL;
            else
                p = g_strndup(attr->values, attr->nelems);
            siunit = gwy_si_unit_new_parse(p, 0);
            gwy_brick_set_si_unit_z(dbrick, siunit);
            g_object_unref(siunit);
        }
        else {
            dbrick = read_brick((const guchar*)(cdffile.buffer + field_var->begin),
                                    cdffile.dims[field_var->dimids[3]].length,
                                    cdffile.dims[field_var->dimids[2]].length,
                                    dim_time,
                                    field_var->type) ;
            g_return_if_fail(times[1] < times[0]);// no negative time series
            gwy_brick_set_zoffset(dbrick, times[0]) ;
            gwy_brick_set_zreal(dbrick, (times[1] - times[0]) * dim_time);
            siunit = gwy_si_unit_new_parse("s", 0);
            gwy_brick_set_si_unit_z(dbrick, siunit);
            g_object_unref(siunit);
        }

        if ((siunit = read_real_size(&cdffile, "rangex", &real, &power10))) {
            /* Use negated positive conditions to catch NaNs */
            if (!((real = fabs(real)) > 0)) {
                g_warning("Real x size is 0.0, fixing to 1.0");
                real = 1.0;
            }
            gwy_brick_set_xreal(dbrick, real*pow10(power10));
            gwy_brick_set_si_unit_x(dbrick, siunit);
            g_object_unref(siunit);
        }
        else {
            g_warning("Failed to read rangex when creating brick");
        }

        if ((siunit = read_real_size(&cdffile, "rangey", &real, &power10))) {
            /* Use negated positive conditions to catch NaNs */
            if (!((real = fabs(real)) > 0)) {
                g_warning("Real y size is 0.0, fixing to 1.0");
                real = 1.0;
            }
            gwy_brick_set_yreal(dbrick, real*pow10(power10));
            gwy_brick_set_si_unit_y(dbrick, siunit);
            g_object_unref(siunit);
        }
        else {
            g_warning("Failed to read rangey when creating brick");
        }

        // add the offsets - mostly important for adding spectra later
        if ((siunit = read_real_size(&cdffile, "offsetx", &real, &power10))) {
            /* some minimal checking */
            if (!(fabs(real) >= 0)) {
                g_warning("x offset reading failed, not using it");
            }
            else {
                gwy_brick_set_xoffset(dbrick, real*pow10(power10)
                                            - 0.5*gwy_brick_get_xreal(dbrick));
            }
            g_object_unref(siunit);
        }
        if ((siunit = read_real_size(&cdffile, "offsety", &real, &power10))) {
            /* some minimal checking */
            if (!(fabs(real) >= 0)) {
                g_warning("y offset reading failed, not using it");
            }
            else {
                gwy_brick_set_yoffset(dbrick, real*pow10(power10)
                                            - 0.5*gwy_brick_get_yreal(dbrick));
            }
            g_object_unref(siunit);
        }

        // zrange is what gwyddion calls "w" for volume data
        if ((siunit = read_real_size(&cdffile, "rangez", &real, &power10))) {
            /* rangez seems to be some bogus value, take only units */
            gwy_brick_set_si_unit_w(dbrick, siunit);
            gwy_brick_multiply(dbrick, pow10(power10));
            g_object_unref(siunit);
        }
        else {
            g_warning("Failed to read rangez when creating brick");
        }

        if ((siunit = read_real_size(&cdffile, "dz", &real, &power10))) {
            /* on the other hand the units seem to be bogus here, take the range */
            gwy_brick_multiply(dbrick, real);
            g_object_unref(siunit);
        }
        else
            g_warning("Failed to read dz when creating brick");

        gwy_container_set_object_by_name(data, "/brick/0", dbrick);

        if ((var = cdffile_get_var(&cdffile, "basename"))) {
            /* FIXME - i should do something safer - even a name including this
            * would be a problem! */
            if (g_strstr_len(cdffile.buffer + var->begin, var->vsize, "-Xm-")
                && !g_strstr_len(cdffile.buffer + var->begin, var->vsize, "-Xp-")) {
                gwy_debug("gxsm netcdf: flip data field since basename contains "
                            "'-Xm-' and not '-Xp-'");
                g_warning("netcdf brick: inverting across y");
                gwy_brick_invert(dbrick, FALSE, TRUE, FALSE);
                gwy_container_set_string_by_name(meta, "Fast scan",
                                                g_strdup("right to left"));
            }
            else {
                /* just assume it left to right (default scan direction) in this
                * case */
                gwy_container_set_string_by_name(meta, "Fast scan",
                                                g_strdup("left to right"));
            }
        }

        //add the false 2D image in /brick/0/preview
        dfield = gwy_data_field_new(cdffile.dims[field_var->dimids[3]].length,
                                    cdffile.dims[field_var->dimids[2]].length,
                                    1.0, 1.0, FALSE) ;
        gwy_brick_extract_plane(dbrick, dfield, 0, 0, 0,
                                    cdffile.dims[field_var->dimids[3]].length,
                                    cdffile.dims[field_var->dimids[2]].length,
                                    -1, TRUE);
        gwy_container_set_object_by_name(data, "/brick/0/preview", dfield) ;
        if ((attr = cdffile_get_attr(field_var->attrs, field_var->nattrs, "long_name"))
                    && attr->type == NC_CHAR
                    && attr->nelems) {
                    gwy_container_set_string_by_name(data,
                                                    "/brick/0/title",
                                                    g_strndup(attr->values, attr->nelems));
                }
        add_size_to_meta(meta, dfield);
        g_object_unref(dfield);
        g_object_unref(dbrick);
    }
    // either single field, 4D (unlikely for gxsm), or not equally enough spaced
    else {
        for (value_i = 0 ; value_i < dim_value ; value_i++) {
            for (time_i = 0 ; time_i < dim_time ; time_i++) {
                frame_i = value_i * dim_time + time_i ;

                dfield = read_data_field((const guchar*)(cdffile.buffer + field_var->begin),
                                        cdffile.dims[field_var->dimids[3]].length,
                                        cdffile.dims[field_var->dimids[2]].length,
                                        field_var->type,
                                        frame_i);

                if ((siunit = read_real_size(&cdffile, "rangex", &real, &power10))) {
                    /* Use negated positive conditions to catch NaNs */
                    if (!((real = fabs(real)) > 0)) {
                        g_warning("Real x size is 0.0, fixing to 1.0");
                        real = 1.0;
                    }
                    gwy_data_field_set_xreal(dfield, real*pow10(power10));
                    gwy_data_field_set_si_unit_xy(dfield, siunit);
                    g_object_unref(siunit);
                }

                if ((siunit = read_real_size(&cdffile, "rangey", &real, &power10))) {
                    /* Use negated positive conditions to catch NaNs */
                    if (!((real = fabs(real)) > 0)) {
                        g_warning("Real y size is 0.0, fixing to 1.0");
                        real = 1.0;
                    }
                    gwy_data_field_set_yreal(dfield, real*pow10(power10));
                    /* must be the same gwy_data_field_set_si_unit_xy(dfield, siunit); */
                    g_object_unref(siunit);
                }

                // add the offsets - mostly important for adding spectra later
                if ((siunit = read_real_size(&cdffile, "offsetx", &real, &power10))) {
                    /* some minimal checking */
                    if (!(fabs(real) >= 0)) {
                        g_warning("x offset reading failed, not using it");
                    }
                    else {
                        gwy_data_field_set_xoffset(dfield, real*pow10(power10)
                                                    - 0.5*gwy_data_field_get_xreal(dfield));
                    }
                    g_object_unref(siunit);
                }
                if ((siunit = read_real_size(&cdffile, "offsety", &real, &power10))) {
                    /* some minimal checking */
                    if (!(fabs(real) >= 0)) {
                        g_warning("y offset reading failed, not using it");
                    }
                    else {
                        gwy_data_field_set_yoffset(dfield, real*pow10(power10)
                                                    - 0.5*gwy_data_field_get_yreal(dfield));
                    }
                    g_object_unref(siunit);
                }

                if ((siunit = read_real_size(&cdffile, "rangez", &real, &power10))) {
                    /* rangez seems to be some bogus value, take only units */
                    gwy_data_field_set_si_unit_z(dfield, siunit);
                    gwy_data_field_multiply(dfield, pow10(power10));
                    g_object_unref(siunit);
                }
                if ((siunit = read_real_size(&cdffile, "dz", &real, &power10))) {
                    /* on the other hand the units seem to be bogus here, take the range */
                    gwy_data_field_multiply(dfield, real);
                    g_object_unref(siunit);
                }

                if ((attr = cdffile_get_attr(field_var->attrs, field_var->nattrs,
                    "long_name"))
                    && attr->type == NC_CHAR
                    && attr->nelems) {
                    gwy_container_set_string_by_name(data,
                                            g_strdup_printf("/%d/data/title", frame_i),
                                            g_strndup(attr->values, attr->nelems));
                }

                /*TODO copy meta instead of creating a new one when dealing with
                 * multiple frames? */
                meta = create_meta(cdffile);
                gwy_container_set_object_by_name(data,
                                                g_strdup_printf("/%d/meta",
                                                                frame_i), meta);
                add_size_to_meta(meta, dfield);
                g_object_unref(meta);

                //flip the data if we're reading a right to left scan!
                if ((var = cdffile_get_var(&cdffile, "basename"))) {
                    /* FIXME - i should do something safer - even a name including this
                    * would be a problem! */
                    if (g_strstr_len(cdffile.buffer + var->begin, var->vsize, "-Xm-")
                        && !g_strstr_len(cdffile.buffer + var->begin, var->vsize, "-Xp-")) {
                        gwy_debug("gxsm netcdf: data field since basename contains "
                                    "'-Xm-' and not '-Xp-'");
                        // flip horizontally
                        gwy_data_field_invert(dfield, FALSE, TRUE, FALSE);
                        gwy_container_set_string_by_name(meta, "Fast scan",
                                                        g_strdup("right to left"));
                    }
                    else {
                        /* just assume it left to right (default scan direction) in this
                        * case */
                        gwy_container_set_string_by_name(meta, "Fast scan",
                                                        g_strdup("left to right"));
                    }
                }

                if (dim_value > 1) {
                    gwy_container_set_string_by_name(meta, "layer value",
                                                        g_strdup_printf("%5.2f",
                                                                        values[value_i]));
                }

                if (dim_time > 1) {
                    gwy_container_set_string_by_name(meta, "time",
                                                        g_strdup_printf("%5.2f",
                                                                        times[time_i]));
                }

                gwy_container_set_object_by_name(data,
                                              g_strdup_printf("/%d/data", frame_i), dfield);
                g_object_unref(dfield);
            }
        }
    }
    g_free(times);
    g_free(values);

gxsm_load_fail:
    gwy_file_abandon_contents(cdffile.buffer, cdffile.size, NULL);
    cdffile_free(&cdffile);

    return data;
}

static GwySIUnit*
read_real_size(const NetCDF *cdffile,
               const gchar *name,
               gdouble *real,
               gint *power10)
{
    const NetCDFVar *var;
    const NetCDFAttr *attr;
    GwySIUnit *siunit;
    const guchar *p;
    gchar *s;

    *real = 1.0;
    *power10 = 0;

    if (!(var = cdffile_get_var(cdffile, name)))
        return NULL;

    /*
     * Warning: Madness ahead.
     *
     * Older GXSM files contain "AA" in "unit", that makes sense, as the units
     * are Angstroems.
     *
     * Newer GXSM files contain "nm" in "unit" though, in spite of the units
     * being still Angstroems.  They seem to contain the real units in
     * "var_unit".  The info field reads:
     *
     *   This number is alwalys stored in Angstroem. Unit is used for user
     *   display only.
     *
     * In addition they invented yet another Angstroem abbreviation: Ang.
     */
    attr = cdffile_get_attr(var->attrs, var->nattrs, "var_unit");
    if (!attr || attr->type != NC_CHAR) {
        attr = cdffile_get_attr(var->attrs, var->nattrs, "unit");
        if (!attr || attr->type != NC_CHAR)
            return NULL;
    }

    if (!attr->nelems)
        s = NULL;
    else
        s = g_strndup(attr->values, attr->nelems);

    siunit = gwy_si_unit_new_parse(s, power10);
    g_free(s);

    p = (const guchar*)(cdffile->buffer + var->begin);
    if (var->type == NC_DOUBLE)
        *real = gwy_get_gdouble_be(&p);
    else if (var->type == NC_FLOAT)
        *real = gwy_get_gfloat_be(&p);
    else
        g_warning("Size is not a floating point number");

    return siunit;
}

/* Notes:
 * 1. I assume that all frames have the same xres, yres - which seems reasonable
 * 2. afaik only float data actually exists -
 *  i haven't actually checked this for anything else
*/
static GwyDataField*
read_data_field(const guchar *buffer,
                gint xres,
                gint yres,
                NetCDFType type,
                gint frame_i)
{
    GwyDataField *dfield;
    gdouble *data;
    gint i;
    const guchar* shifted_buffer ;

    gwy_debug("processing frame %d", frame_i);
    dfield = gwy_data_field_new(xres, yres, 1.0, 1.0, FALSE);
    data = gwy_data_field_get_data(dfield);

    switch (type) {
        case NC_BYTE:
        case NC_CHAR:
        {
            const gint8 *d8 = (const gint8*)buffer;

            for (i = 0; i < xres*yres; i++)
                data[i] = d8[i + xres * yres * frame_i];
        }
        break;

        case NC_SHORT:
        {
            const gint16 *d16 = (const gint16*)buffer;

            for (i = 0; i < xres*yres; i++)
                data[i] = GINT16_FROM_BE(d16[i + xres * yres * frame_i]);
        }
        break;

        case NC_INT:
        {
            const gint32 *d32 = (const gint32*)buffer;

            for (i = 0; i < xres*yres; i++)
                data[i] = GINT32_FROM_BE(d32[i + xres * yres * frame_i]);
        }
        break;

        case NC_FLOAT:
        shifted_buffer = buffer + sizeof(gfloat) * xres * yres * frame_i;
        for (i = 0; i < xres*yres; i++)
            data[i] = gwy_get_gfloat_be(&shifted_buffer);
        break;

        case NC_DOUBLE:
        shifted_buffer = buffer + sizeof(gdouble) * xres * yres * frame_i;
        for (i = 0; i < xres*yres; i++)
            data[i] = gwy_get_gdouble_be(&shifted_buffer);
        break;

        /* not using frame_i if we got here - no point*/
        default:
        g_return_val_if_reached(dfield);
        break;
    }

    return dfield;
}

static GwyBrick*
read_brick(const guchar *buffer,
          gint xres,
          gint yres,
          gint zres,
          NetCDFType type)
{
    GwyBrick *brick ;
    gdouble * data;
    gint i;

    brick = gwy_brick_new(xres, yres, zres, 1.0, 1.0, 1.0, FALSE) ;
    data = gwy_brick_get_data(brick) ;
    switch (type) {
        case NC_BYTE:
        case NC_CHAR:
        {
            const gint8 *d8 = (const gint8*)buffer;

            for (i = 0; i < xres*yres*zres; i++)
                data[i] = d8[i];
        }
        break;

        case NC_SHORT:
        {
            const gint16 *d16 = (const gint16*)buffer;

            for (i = 0; i < xres*yres*zres; i++)
                data[i] = GINT16_FROM_BE(d16[i]);
        }
        break;

        case NC_INT:
        {
            const gint32 *d32 = (const gint32*)buffer;

            for (i = 0; i < xres*yres*zres; i++)
                data[i] = GINT32_FROM_BE(d32[i]);
        }
        break;

        case NC_FLOAT:
        for (i = 0; i < xres*yres*zres; i++)
            data[i] = gwy_get_gfloat_be(&buffer);
        break;

        case NC_DOUBLE:
        for (i = 0; i < xres*yres*zres; i++)
            data[i] = gwy_get_gdouble_be(&buffer);
        break;

        default:
        g_return_val_if_reached(brick);
        break;
    }

    return brick;
}

static GwyContainer*
create_meta
(const NetCDF cdffile)
{
    GwyContainer *meta ;
    const NetCDFVar *var;
    GwySIUnit* siunit ;
    gdouble real ;
    gint power10 ;

    meta = gwy_container_new();

    if ((var = cdffile_get_var(&cdffile, "comment"))) {
        /* not sure if this would be a valid string, so create a new one with
        * fixed length */
        gwy_container_set_string_by_name(meta, "Comments",
                                        g_strndup(cdffile.buffer + var->begin,
                                                  var->vsize));
    }
    if ((var = cdffile_get_var(&cdffile, "dateofscan"))) {
        /* not sure if this would be a valid string, so create a new one with
        * fixed length */
        gwy_container_set_string_by_name(meta, "Date and time",
                                        g_strndup(cdffile.buffer + var->begin,
                                                  var->vsize));
    }
    /* NOTE: i know this is bad as it's hardware dependent (i.e. only someone
    * using the sranger 2 dsp gets this); but since these details depend in
    * gxsm on the plugin, i see no better option */
    if ((siunit = read_real_size(&cdffile, "sranger_mk2_hwi_bias",
                                &real, &power10))) {
        gwy_container_set_string_by_name(meta, "V_bias",
                                        g_strdup_printf("%5.2g V",
                                                        real*pow10(power10)));
        g_object_unref(siunit);
    }
    if ((siunit = read_real_size(&cdffile,
                                "sranger_mk2_hwi_mix0_current_set_point",
                                &real, &power10))) {
        gwy_container_set_string_by_name(meta, "I_setpoint",
                                        g_strdup_printf("%5.2g A",
                                                        real*pow10(power10)));
        g_object_unref(siunit);
    }
    if ((var = cdffile_get_var(&cdffile, "spm_scancontrol"))) {
        gwy_container_set_string_by_name(meta, "Slow scan",
                                        g_strndup(cdffile.buffer + var->begin,
                                                  var->vsize));
    }
    return meta ;
}


/* Add the lateral size information to the meta data; used for both brick
 * and field option
 * */

static void
add_size_to_meta(GwyContainer* meta, GwyDataField* dfield)
{
    gwy_container_set_string_by_name(meta, "Size",
                  g_strdup_printf("%0.2g x %0.2g %s",
                  gwy_data_field_get_xreal(dfield),
                  gwy_data_field_get_yreal(dfield),
                  gwy_si_unit_get_string(gwy_data_field_get_si_unit_xy(dfield),
                      GWY_SI_UNIT_FORMAT_PLAIN)));
    /* Should work, but it seems there's a problem when the units are um,
     * so i'm using the simpler, inelegant solution above
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    siunit = gwy_data_field_get_si_unit_xy(dfield);
    vf = gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                sqrt(xreal*yreal), NULL);
    gwy_container_set_string_by_name(meta, "Size",
                        g_strdup_printf("%.*f×%.*f%s%s",
                        vf->precision, xreal/vf->magnitude,
                        vf->precision, yreal/vf->magnitude,
                        (vf->units && *vf->units) ? " " : "", vf->units));
    g_object_unref(siunit);
    gwy_si_unit_value_format_free(vf);*/
    gwy_container_set_string_by_name(meta, "Resolution",
                        g_strdup_printf("%u x %u",
                                        gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield)));
}


static gboolean
cdffile_load(NetCDF *cdffile,
             const gchar *filename,
             GError **error)
{
    GError *err = NULL;
    const guchar *p;

    gwy_clear(cdffile, 1);

    if (!gwy_file_get_contents(filename, &cdffile->buffer, &cdffile->size,
                               &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return FALSE;
    }

    if (cdffile->size < 32) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(cdffile->buffer, cdffile->size, NULL);
        return FALSE;
    }

    p = cdffile->buffer;

    /* Header */
    if (memcmp(p, MAGIC1, MAGIC_SIZE) == 0)
        cdffile->version = 1;
    else if (memcmp(p, MAGIC2, MAGIC_SIZE) == 0)
        cdffile->version = 2;
    else {
        err_FILE_TYPE(error, "NetCDF");
        gwy_file_abandon_contents(cdffile->buffer, cdffile->size, NULL);
        return FALSE;
    }
    gwy_debug("Header: CDF v%d", (gint)cdffile->version);
    p += MAGIC_SIZE;

    /* N Records */
    cdffile->nrecs = gwy_get_guint32_be(&p);
    gwy_debug("nrecs %d", cdffile->nrecs);

    /* Dimensions */
    if (!cdffile_read_dim_array(&cdffile->dims, &cdffile->ndims,
                                cdffile->buffer, cdffile->size, &p, error)) {
        cdffile_free(cdffile);
        gwy_file_abandon_contents(cdffile->buffer, cdffile->size, NULL);
        return FALSE;
    }

    /* Global attributes */
    if (!cdffile_read_attr_array(&cdffile->attrs, &cdffile->nattrs,
                                 cdffile->buffer, cdffile->size, &p, error)) {
        cdffile_free(cdffile);
        gwy_file_abandon_contents(cdffile->buffer, cdffile->size, NULL);
        return FALSE;
    }

    /* Variables */
    if (!cdffile_read_var_array(&cdffile->vars, &cdffile->nvars,
                                cdffile->version,
                                cdffile->buffer, cdffile->size, &p, error)) {
        cdffile_free(cdffile);
        gwy_file_abandon_contents(cdffile->buffer, cdffile->size, NULL);
        return FALSE;
    }

    cdffile->data_start = (gsize)(p - cdffile->buffer);

    /* Sanity check */
    if (!cdffile_validate_vars(cdffile, error)) {
        cdffile_free(cdffile);
        gwy_file_abandon_contents(cdffile->buffer, cdffile->size, NULL);
        return FALSE;
    }

    return TRUE;
}

static inline guint
cdffile_type_size(NetCDFType type)
{
    switch (type) {
        case NC_BYTE:
        case NC_CHAR:
        return 1;
        break;

        case NC_SHORT:
        return 2;
        break;

        case NC_INT:
        case NC_FLOAT:
        return 4;
        break;

        case NC_DOUBLE:
        return 8;
        break;

        default:
        return 0;
        break;
    }
}

static inline gboolean
cdffile_check_size(GError **error,
                   const gchar *field_name,
                   const guchar *buf,
                   gsize size,
                   const guchar *p,
                   gsize needed)
{
    if (size >= (gsize)(p - (const guchar*)buf) + needed)
        return TRUE;

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("File ended unexpectedly inside `%s'."), field_name);
    return FALSE;
}

static inline void
err_CDF_EXPECTED(GError **error,
                 const gchar *field_name)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Expected `%s' array or `ABSENT'."), field_name);
}

static inline void
err_CDF_ZELEMENTS(GError **error,
                  const gchar *field_name)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Array `%s' has non-zero number of elements in spite "
                  "of being absent."), field_name);
}

static gboolean
cdffile_read_dim_array(NetCDFDim **pdims,
                       gint *pndims,
                       const guchar *buf,
                       gsize size,
                       const guchar **p,
                       GError **error)
{
    NetCDFDim *dims;
    gint ndims, n, i;

    if (!cdffile_check_size(error, "dim_array", buf, size, *p, 8))
        return FALSE;

    n = gwy_get_guint32_be(p);
    gwy_debug("dims (%d)", n);
    if (n != 0 && n != NC_DIMENSION) {
        err_CDF_EXPECTED(error, "NC_DIMENSION");
        return FALSE;
    }
    ndims = gwy_get_guint32_be(p);
    if (ndims && !n) {
        err_CDF_ZELEMENTS(error, "dim_array");
        return FALSE;
    }
    gwy_debug("ndims: %d", ndims);

    if (!ndims)
        return TRUE;

    dims = g_new0(NetCDFDim, ndims);
    *pdims = dims;
    *pndims = ndims;
    for (i = 0; i < ndims; i++) {
        if (!cdffile_check_size(error, "dim_array", buf, size, *p, 4))
            return FALSE;
        n = gwy_get_guint32_be(p);
        ALIGN4(n);
        if (!cdffile_check_size(error, "dim_array", buf, size, *p, n + 4))
            return FALSE;
        dims[i].name = g_strndup((const gchar*)*p, n);
        *p += n;
        dims[i].length = gwy_get_guint32_be(p);
        gwy_debug("dim_array[%d]: <%s> %d%s",
                  i, dims[i].name, dims[i].length,
                  dims[i].length ? "" : "(record)");
    }

    /* Check record dimensions */
    n = -1;
    for (i = 0; i < ndims; i++) {
        if (!dims[i].length) {
            if (n > -1) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("More than one record dimension found."));
                return FALSE;
            }
            n = i;
        }
    }

    return TRUE;
}

static gboolean
cdffile_read_attr_array(NetCDFAttr **pattrs,
                        gint *pnattrs,
                        const guchar *buf,
                        gsize size,
                        const guchar **p,
                        GError **error)
{
    NetCDFAttr *attrs;
    gint nattrs, n, ts, i;

    if (!cdffile_check_size(error, "attr_array", buf, size, *p, 8))
        return FALSE;

    n = gwy_get_guint32_be(p);
    gwy_debug("attrs (%d)", n);
    if (n != 0 && n != NC_ATTRIBUTE) {
        err_CDF_EXPECTED(error, "NC_ATTRIBUTE");
        return FALSE;
    }
    nattrs = gwy_get_guint32_be(p);
    if (nattrs && !n) {
        err_CDF_ZELEMENTS(error, "attr_array");
        return FALSE;
    }
    gwy_debug("nattrs: %d", nattrs);

    if (!nattrs)
        return TRUE;

    attrs = g_new0(NetCDFAttr, nattrs);
    *pattrs = attrs;
    *pnattrs = nattrs;
    for (i = 0; i < nattrs; i++) {
        if (!cdffile_check_size(error, "attr_array", buf, size, *p, 4))
            return FALSE;
        n = gwy_get_guint32_be(p);
        n += (4 - n % 4) % 4;
        if (!cdffile_check_size(error, "attr_array", buf, size, *p, n + 8))
            return FALSE;
        attrs[i].name = g_strndup((const gchar*)*p, n);
        *p += n;
        attrs[i].type = gwy_get_guint32_be(p);
        attrs[i].nelems = gwy_get_guint32_be(p);
        gwy_debug("attr_array[%d]: <%s> %d of %d",
                  i, attrs[i].name, attrs[i].nelems, attrs[i].type);
        ts = cdffile_type_size(attrs[i].type);
        if (!ts) {
            err_DATA_TYPE(error, attrs[i].type);
            return FALSE;
        }
        n = ts*attrs[i].nelems;
        ALIGN4(n);
        if (!cdffile_check_size(error, "attr_array", buf, size, *p, n))
            return FALSE;
        attrs[i].values = *p;
        *p += n;
    }

    return TRUE;
}

static gboolean
cdffile_read_var_array(NetCDFVar **pvars,
                       gint *pnvars,
                       NetCDFVersion version,
                       const guchar *buf,
                       gsize size,
                       const guchar **p,
                       GError **error)
{
    NetCDFVar *vars;
    gint nvars, n, i, ts, offset_size;

    if (!cdffile_check_size(error, "var_array", buf, size, *p, 8))
        return FALSE;

    n = gwy_get_guint32_be(p);
    gwy_debug("vars (%d)", n);
    if (n != 0 && n != NC_VARIABLE) {
        err_CDF_EXPECTED(error, "NC_VARIABLE");
        return FALSE;
    }
    nvars = gwy_get_guint32_be(p);
    if (nvars && !n) {
        err_CDF_ZELEMENTS(error, "var_array");
        return FALSE;
    }
    gwy_debug("nvars: %d", nvars);

    if (!nvars)
        return TRUE;

    switch (version) {
        case NETCDF_CLASSIC:
        offset_size = 4;
        break;

        case NETCDF_64BIT:
        offset_size = 8;
        break;

        default:
        g_return_val_if_reached(FALSE);
        break;
    }

    vars = g_new0(NetCDFVar, nvars);
    *pvars = vars;
    *pnvars = nvars;
    for (i = 0; i < nvars; i++) {
        if (!cdffile_check_size(error, "var_array", buf, size, *p, 4))
            return FALSE;
        n = gwy_get_guint32_be(p);
        ALIGN4(n);
        if (!cdffile_check_size(error, "var_array", buf, size, *p, n + 4))
            return FALSE;
        vars[i].name = g_strndup((const gchar*)*p, n);
        *p += n;
        vars[i].ndims = gwy_get_guint32_be(p);
        gwy_debug("var_array[%d]: <%s> %d", i, vars[i].name, vars[i].ndims);
        if (!cdffile_check_size(error, "var_array", buf, size, *p,
                                4*vars[i].ndims))
            return FALSE;
        vars[i].dimids = g_new(gint, vars[i].ndims);
        for (n = 0; n < vars[i].ndims; n++) {
            vars[i].dimids[n] = gwy_get_guint32_be(p);
            gwy_debug("var_array[%d][%d]: %d", i, n, vars[i].dimids[n]);
        }
        if (!cdffile_read_attr_array(&vars[i].attrs, &vars[i].nattrs,
                                     buf, size, p, error))
            return FALSE;
        if (!cdffile_check_size(error, "var_array", buf, size, *p,
                                8 + offset_size))
            return FALSE;
        vars[i].type = gwy_get_guint32_be(p);
        ts = cdffile_type_size(vars[i].type);
        if (!ts) {
            err_DATA_TYPE(error, vars[i].type);
            return FALSE;
        }
        vars[i].vsize = gwy_get_guint32_be(p);
        switch (version) {
            case NETCDF_CLASSIC:
            vars[i].begin = gwy_get_guint32_be(p);
            break;

            case NETCDF_64BIT:
            vars[i].begin = gwy_get_guint64_be(p);
            break;
        }
    }

    return TRUE;
}

static NetCDFDim*
cdffile_get_dim(const NetCDF *cdffile,
                const gchar *name)
{
    gint i;

    for (i = 0; i < cdffile->ndims; i++) {
        if (gwy_strequal(cdffile->dims[i].name, name))
            return cdffile->dims + i;
    }

    return NULL;
}

static NetCDFVar*
cdffile_get_var(const NetCDF *cdffile,
                const gchar *name)
{
    gint i;

    for (i = 0; i < cdffile->nvars; i++) {
        if (gwy_strequal(cdffile->vars[i].name, name))
            return cdffile->vars + i;
    }

    return NULL;
}

static const NetCDFAttr*
cdffile_get_attr(const NetCDFAttr *attrs,
                 gint nattrs,
                 const gchar *name)
{
    gint i;

    for (i = 0; i < nattrs; i++) {
        if (gwy_strequal(attrs[i].name, name))
            return attrs + i;
    }

    return NULL;
}

static gboolean
cdffile_validate_vars(const NetCDF *cdffile,
                      GError **error)
{
    NetCDFDim *dim;
    NetCDFVar *var;
    gint i, j, size;

    for (i = 0; i < cdffile->nvars; i++) {
        var = cdffile->vars + i;
        size = cdffile_type_size(var->type);
        for (j = 0; j < var->ndims; j++) {
            /* Bogus dimension id */
            if (var->dimids[j] >= cdffile->ndims) {
                err_CDF_INTEGRITY(error, var->name);
                return FALSE;
            }
            dim = cdffile->dims + var->dimids[j];
            /* XXX: record vars have length == 0 for the first dimension, but
             * frankly, we don not care. */
            if (dim->length <= 0) {
                err_CDF_INTEGRITY(error, var->name);
                return FALSE;
            }
            size *= dim->length;
            if (size <= 0) {
                err_CDF_INTEGRITY(error, var->name);
                return FALSE;
            }
        }
        ALIGN4(size);
        /* Sizes do not match */
        if (size != var->vsize) {
            err_CDF_INTEGRITY(error, var->name);
            return FALSE;
        }
        /* Data sticks out */
        if (var->begin < (guint64)cdffile->data_start
            || var->begin + var->vsize > (guint64)cdffile->size) {
            err_CDF_INTEGRITY(error, var->name);
            return FALSE;
        }
    }

    return TRUE;
}

static void
cdffile_free(NetCDF *cdffile)
{
    gint i, j;

    /* Dimensions */
    for (i = 0; i < cdffile->ndims; i++)
        g_free(cdffile->dims[i].name);
    g_free(cdffile->dims);

    cdffile->ndims = 0;
    cdffile->dims = 0;

    /* Global attributes */
    for (i = 0; i < cdffile->nattrs; i++)
        g_free(cdffile->attrs[i].name);
    g_free(cdffile->attrs);

    cdffile->nattrs = 0;
    cdffile->attrs = 0;

    /* Variables */
    for (i = 0; i < cdffile->nvars; i++) {
        g_free(cdffile->vars[i].name);
        g_free(cdffile->vars[i].dimids);
        for (j = 0; j < cdffile->vars[i].nattrs; j++)
            g_free(cdffile->vars[i].attrs[j].name);
        g_free(cdffile->vars[i].attrs);

        cdffile->vars[i].nattrs = 0;
        cdffile->vars[i].attrs = 0;
    }
    g_free(cdffile->vars);

    cdffile->nvars = 0;
    cdffile->vars = 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
