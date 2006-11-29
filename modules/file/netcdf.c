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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>

#include "get.h"
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
    gint ndimids;
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
static void          cdffile_free           (NetCDF *cdffile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports network Common Data Form (netCDF) files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
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
    cdffile.nrecs = get_DWORD_BE(&p);
    if (cdffile_read_dim_array(&cdffile.dims, &cdffile.ndims,
                               fileinfo->head, fileinfo->buffer_len-1, &p,
                               NULL)) {
        gboolean has_dimx = FALSE, has_dimy = FALSE;
        gint i;

        for (i = 0; i < cdffile.ndims; i++) {
            if (gwy_strequal(cdffile.dims[i].name, "dimx"))
                has_dimx = TRUE;
            else if (gwy_strequal(cdffile.dims[i].name, "dimy"))
                has_dimy = TRUE;
        }

        if (has_dimx && has_dimy)
            score = 80;
    }
    cdffile_free(&cdffile);

    return score;
}

static GwyContainer*
gxsm_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    NetCDF cdffile;

    if (!cdffile_load(&cdffile, filename, error))
        return NULL;

    err_NO_DATA(error);

    return NULL;
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
    cdffile->nrecs = get_DWORD_BE(&p);
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

    n = get_DWORD_BE(p);
    gwy_debug("dims %d", n);
    if (n != 0 && n != NC_DIMENSION) {
        err_CDF_EXPECTED(error, "NC_DIMENSION");
        return FALSE;
    }
    ndims = get_DWORD_BE(p);
    if (ndims && !n) {
        err_CDF_ZELEMENTS(error, "dim_array");
        return FALSE;
    }

    if (!ndims)
        return TRUE;

    dims = g_new0(NetCDFDim, ndims);
    *pdims = dims;
    *pndims = ndims;
    for (i = 0; i < ndims; i++) {
        if (!cdffile_check_size(error, "dim_array", buf, size, *p, 4))
            return FALSE;
        n = get_DWORD_BE(p);
        ALIGN4(n);
        if (!cdffile_check_size(error, "dim_array", buf, size, *p, n + 4))
            return FALSE;
        dims[i].name = g_strndup((const gchar*)*p, n);
        *p += n;
        dims[i].length = get_DWORD_BE(p);
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

    n = get_DWORD_BE(p);
    gwy_debug("attrs %d", n);
    if (n != 0 && n != NC_ATTRIBUTE) {
        err_CDF_EXPECTED(error, "NC_ATTRIBUTE");
        return FALSE;
    }
    nattrs = get_DWORD_BE(p);
    if (nattrs && !n) {
        err_CDF_ZELEMENTS(error, "attr_array");
        return FALSE;
    }

    if (!nattrs)
        return TRUE;

    attrs = g_new0(NetCDFAttr, nattrs);
    *pattrs = attrs;
    *pnattrs = nattrs;
    for (i = 0; i < nattrs; i++) {
        if (!cdffile_check_size(error, "attr_array", buf, size, *p, 4))
            return FALSE;
        n = get_DWORD_BE(p);
        n += (4 - n % 4) % 4;
        if (!cdffile_check_size(error, "attr_array", buf, size, *p, n + 8))
            return FALSE;
        attrs[i].name = g_strndup((const gchar*)*p, n);
        *p += n;
        attrs[i].type = get_DWORD_BE(p);
        attrs[i].nelems = get_DWORD_BE(p);
        gwy_debug("attr_array[%d]: <%s> %d of %d",
                  i, attrs[i].name, attrs[i].nelems, attrs[i].type);
        ts = cdffile_type_size(attrs[i].type);
        if (!ts) {
            g_warning("bad element type %d", attrs[i].type);
            return FALSE;
        }
        n = ts*attrs[i].nelems;
        ALIGN4(n);
        if (!cdffile_check_size(error, "attr_array", buf, size, *p, n))
            return FALSE;
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
    gint nvars, n, i, offset_size;

    if (!cdffile_check_size(error, "var_array", buf, size, *p, 8))
        return FALSE;

    n = get_DWORD_BE(p);
    gwy_debug("vars %d", n);
    if (n != 0 && n != NC_VARIABLE) {
        err_CDF_EXPECTED(error, "NC_VARIABLE");
        return FALSE;
    }
    nvars = get_DWORD_BE(p);
    if (nvars && !n) {
        err_CDF_ZELEMENTS(error, "var_array");
        return FALSE;
    }

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
        n = get_DWORD_BE(p);
        ALIGN4(n);
        if (!cdffile_check_size(error, "var_array", buf, size, *p, n + 4))
            return FALSE;
        vars[i].name = g_strndup((const gchar*)*p, n);
        *p += n;
        vars[i].ndimids = get_DWORD_BE(p);
        gwy_debug("var_array[%d]: <%s> %d", i, vars[i].name, vars[i].ndimids);
        if (!cdffile_check_size(error, "var_array", buf, size, *p,
                                4*vars[i].ndimids))
            return FALSE;
        vars[i].dimids = g_new(gint, vars[i].ndimids);
        for (n = 0; n < vars[i].ndimids; n++) {
            vars[i].dimids[n] = get_DWORD_BE(p);
            gwy_debug("var_array[%d][%d]: %d", vars[i].dimids[n]);
        }
        if (!cdffile_read_attr_array(&vars[i].attrs, &vars[i].nattrs,
                                     buf, size, p, error))
            return FALSE;
        if (!cdffile_check_size(error, "var_array", buf, size, *p,
                                8 + offset_size))
            return FALSE;
        vars[i].type = get_DWORD_BE(p);
        vars[i].vsize = get_DWORD_BE(p);
        switch (version) {
            case NETCDF_CLASSIC:
            vars[i].begin = get_DWORD_BE(p);
            break;

            case NETCDF_64BIT:
            vars[i].begin = get_QWORD_BE(p);
            break;
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

    /* Global attributes */
    for (i = 0; i < cdffile->nattrs; i++)
        g_free(cdffile->attrs[i].name);
    g_free(cdffile->attrs);

    /* Variables */
    for (i = 0; i < cdffile->nvars; i++) {
        g_free(cdffile->vars[i].name);
        g_free(cdffile->vars[i].dimids);
        for (j = 0; j < cdffile->vars[i].nattrs; j++)
            g_free(cdffile->vars[i].attrs[j].name);
        g_free(cdffile->vars[i].attrs);
    }
    g_free(cdffile->vars);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
