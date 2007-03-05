/*
 *  @(#) $Id$
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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

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
                                             NetCDFType type);
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

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports network Common Data Form (netCDF) files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek",
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
    memset(&cdffile, 0, sizeof(NetCDF));
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

static GwyContainer*
gxsm_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    static const gchar *dimensions[] = { "time", "value", "dimy", "dimx" };
    GwyContainer *data = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    NetCDF cdffile;
    const NetCDFDim *dim;
    const NetCDFVar *var;
    const NetCDFAttr *attr;
    gdouble real;
    gint i, power10;

    if (!cdffile_load(&cdffile, filename, error))
        return NULL;

    if (cdffile.nrecs) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("NetCDF records are not supported."));
        goto gxsm_load_fail;
    }

    /* Look for variable "H".  This seems to be how GXSM calls data. */
    if (!(var = cdffile_get_var(&cdffile, "H"))) {
        err_NO_DATA(error);
        goto gxsm_load_fail;
    }

    /* Check the dimensions.  We only know how to handle time=1 and value=1. */
    for (i = 0; i < var->ndims; i++) {
        dim = cdffile.dims + var->dimids[i];
        if (!gwy_strequal(dim->name, dimensions[i])
            || (i < 2 && dim->length != 1)) {
            /* XXX */
            err_NO_DATA(error);
            goto gxsm_load_fail;
        }
    }

    dfield = read_data_field((const guchar*)(cdffile.buffer + var->begin),
                             cdffile.dims[var->dimids[3]].length,
                             cdffile.dims[var->dimids[2]].length,
                             var->type);

    if ((siunit = read_real_size(&cdffile, "rangex", &real, &power10))) {
        gwy_data_field_set_xreal(dfield, real*pow10(power10));
        gwy_data_field_set_si_unit_xy(dfield, siunit);
        g_object_unref(siunit);
    }

    if ((siunit = read_real_size(&cdffile, "rangey", &real, &power10))) {
        gwy_data_field_set_yreal(dfield, real*pow10(power10));
        /* must be the same gwy_data_field_set_si_unit_xy(dfield, siunit); */
        g_object_unref(siunit);
    }

    if ((siunit = read_real_size(&cdffile, "rangez", &real, &power10))) {
        /* rangez seems to be some bogus value, take only units */
        gwy_data_field_set_si_unit_z(dfield, siunit);
        gwy_data_field_multiply(dfield, pow10(power10));
        g_object_unref(siunit);
    }
    if ((siunit = read_real_size(&cdffile, "dz", &real, &power10))) {
        /* on the other hand the units seem to be bogues here, take the range */
        gwy_data_field_multiply(dfield, real);
        g_object_unref(siunit);
    }

    data = gwy_container_new();
    gwy_container_set_object_by_name(data, "/0/data", dfield);
    g_object_unref(dfield);

    if ((attr = cdffile_get_attr(var->attrs, var->nattrs, "long_name"))
        && attr->type == NC_CHAR
        && attr->nelems) {
        gwy_container_set_string_by_name(data, "/0/data/title",
                                         g_strndup(attr->values, attr->nelems));
    }

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

    attr = cdffile_get_attr(var->attrs, var->nattrs, "unit");
    if (!attr || attr->type != NC_CHAR)
        return NULL;

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

static GwyDataField*
read_data_field(const guchar *buffer,
                gint xres,
                gint yres,
                NetCDFType type)
{
    GwyDataField *dfield;
    gdouble *data;
    gint i;

    dfield = gwy_data_field_new(xres, yres, 1.0, 1.0, FALSE);
    data = gwy_data_field_get_data(dfield);

    switch (type) {
        case NC_BYTE:
        case NC_CHAR:
        {
            const gint8 *d8 = (const gint8*)buffer;

            for (i = 0; i < xres*yres; i++)
                data[i] = d8[i];
        }
        break;

        case NC_SHORT:
        {
            const gint16 *d16 = (const gint16*)buffer;

            for (i = 0; i < xres*yres; i++)
                data[i] = GINT16_FROM_BE(d16[i]);
        }
        break;

        case NC_INT:
        {
            const gint32 *d32 = (const gint32*)buffer;

            for (i = 0; i < xres*yres; i++)
                data[i] = GINT32_FROM_BE(d32[i]);
        }
        break;

        case NC_FLOAT:
        for (i = 0; i < xres*yres; i++)
            data[i] = gwy_get_gfloat_be(&buffer);
        break;

        case NC_DOUBLE:
        for (i = 0; i < xres*yres; i++)
            data[i] = gwy_get_gdouble_be(&buffer);
        break;

        default:
        g_return_val_if_reached(dfield);
        break;
    }

    return dfield;
}

static gboolean
cdffile_load(NetCDF *cdffile,
             const gchar *filename,
             GError **error)
{
    GError *err = NULL;
    const guchar *p;

    memset(cdffile, 0, sizeof(NetCDF));

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
