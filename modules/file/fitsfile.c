/*
 *  $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
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
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <fitsio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC "SIMPLE  ="
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION1 ".fits"
#define EXTENSION2 ".fit"

static gboolean      module_register    (void);
static gint          fits_detect        (const GwyFileDetectInfo *fileinfo,
                                         gboolean only_name);
static GwyContainer* fits_load          (const gchar *filename,
                                         GwyRunType mode,
                                         GError **error);
static gboolean      get_real_and_offset(fitsfile *fptr,
                                         gint i,
                                         guint res,
                                         gdouble *real,
                                         gdouble *off);
static GwyDataField* mask_of_nans       (GwyDataField *dfield,
                                         const gchar *invalid);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Reads Flexible Image Transport System (FITS) files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("fitsfile",
                           N_("Flexible Image Transport System FITS (.fits)"),
                           (GwyFileDetectFunc)&fits_detect,
                           (GwyFileLoadFunc)&fits_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
fits_detect(const GwyFileDetectInfo *fileinfo,
            gboolean only_name)
{
    if (only_name) {
        if (g_str_has_suffix(fileinfo->name_lowercase, EXTENSION1))
            return 20;
        if (g_str_has_suffix(fileinfo->name_lowercase, EXTENSION2))
            return 15;
        return 0;
    }

    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    /* Permit some more specific importers to take over. */
    return 90;
}

static void
err_FITS(GError **error, gint status)
{
    gchar buf[31];   /* Max length is specified as 30. */

    fits_get_errstatus(status, buf);
    buf[sizeof(buf)-1] = '\0';
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_SPECIFIC,
                _("CFITSIO error while reading the FITS file: %s."),
                buf);
}

static GwyContainer*
fits_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *container = NULL;
    fitsfile *fptr = NULL;
    GwyDataField *field = NULL;
    gint status = 0;   /* Must be initialised to zero! */
    gint hdutype, naxis, anynull, nkeys;
    glong res[3];    /* First index is the fast looping one. */
    char strvalue[FLEN_VALUE];
    gchar *invalid = NULL;
    gdouble real, off;

    if (fits_open_image(&fptr, filename, READONLY, &status)) {
        err_FITS(error, status);
        return NULL;
    }

    if (fits_get_hdu_type(fptr, &hdutype, &status)) {
        err_FITS(error, status);
        goto fail;
    }

    gwy_debug("hdutype %d", hdutype);
    if (hdutype != IMAGE_HDU) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Only two-dimensional images are supported."));
        goto fail;
    }

    if (fits_get_img_dim(fptr, &naxis, &status)) {
        err_FITS(error, status);
        goto fail;
    }

    gwy_debug("naxis %d", naxis);
    if (naxis != 2 && naxis != 3) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Only two-dimensional images are supported."));
        goto fail;
    }

    if (fits_get_img_size(fptr, naxis, res, &status)) {
        err_FITS(error, status);
        goto fail;
    }

    if (naxis == 3 && res[2] != 1) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Only two-dimensional images are supported."));
        goto fail;
    }

    gwy_debug("xres %ld, yres %ld", res[0], res[1]);
    if (err_DIMENSION(error, res[0]) || err_DIMENSION(error, res[1]))
        goto fail;

    field = gwy_data_field_new(res[0], res[1], res[0], res[1], FALSE);
    invalid = g_new(gchar, res[0]*res[1]);
    if (fits_read_imgnull(fptr, TDOUBLE, 1, res[0]*res[1],
                          field->data, invalid, &anynull, &status)) {
        err_FITS(error, status);
        goto fail;
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", field);

    /* Failures here are non-fatal.  We already have an image. */
    if (fits_get_hdrspace(fptr, &nkeys, NULL, &status)) {
        g_warning("Cannot get the first hdrspace.");
        goto fail;
    }

    if (!fits_read_key(fptr, TSTRING, "BUINT   ", strvalue, NULL, &status)) {
        gint power10;

        gwy_debug("BUINT = <%s>", strvalue);
        gwy_si_unit_set_from_string_parse(gwy_data_field_get_si_unit_z(field),
                                          strvalue, &power10);
        if (power10)
            gwy_data_field_multiply(field, pow10(power10));
    }
    status = 0;

    if (get_real_and_offset(fptr, 1, res[0], &real, &off)) {
        if (real < 0.0) {
            off += real;
            real = -real;
            gwy_data_field_invert(field, FALSE, TRUE, FALSE);
        }
        gwy_data_field_set_xreal(field, real);
        gwy_data_field_set_xoffset(field, off);
    }

    if (get_real_and_offset(fptr, 2, res[1], &real, &off)) {
        if (real < 0.0) {
            off += real;
            real = -real;
            gwy_data_field_invert(field, TRUE, FALSE, FALSE);
        }
        gwy_data_field_set_yreal(field, real);
        gwy_data_field_set_yoffset(field, off);
    }

    /* Create a mask of invalid data. */
    if (anynull) {
        GwyDataField *mask = mask_of_nans(field, invalid);
        if (mask) {
            gwy_container_set_object_by_name(container, "/0/mask", mask);
            g_object_unref(mask);
        }
    }

fail:
    fits_close_file(fptr, &status);
    gwy_object_unref(field);
    g_free(invalid);

    return container;
}

static gboolean
get_real_and_offset(fitsfile *fptr, gint i,
                    guint res, gdouble *real, gdouble *off)
{
    char keyname[FLEN_KEYWORD];
    gdouble delt, refval, refpix;
    gint status = 0;

    fits_make_keyn("CDELT", i, keyname, &status);
    gwy_debug("looking for %s", keyname);
    if (fits_read_key(fptr, TDOUBLE, keyname, &delt, NULL, &status))
        return FALSE;
    gwy_debug("%s = %g", keyname, delt);

    if (!(delt != 0.0))
        return FALSE;

    *real = res*delt;
    *off = 0.0;

    fits_make_keyn("CRPIX", i, keyname, &status);
    gwy_debug("looking for %s", keyname);
    if (fits_read_key(fptr, TDOUBLE, keyname, &refpix, NULL, &status))
        return TRUE;
    gwy_debug("%s = %g", keyname, refpix);

    fits_make_keyn("CRVAL", i, keyname, &status);
    gwy_debug("looking for %s", keyname);
    if (fits_read_key(fptr, TDOUBLE, keyname, &refval, NULL, &status))
        return TRUE;
    gwy_debug("%s = %g", keyname, refval);

    /* Not sure about their pixel numbering, so offset may be wrong. */
    *off = refval + delt*(1.0 - refpix);

    return TRUE;
}

static GwyDataField*
mask_of_nans(GwyDataField *dfield, const gchar *invalid)
{
    GwyDataField *mask = NULL;
    guint k, n = dfield->xres*dfield->yres;
    gdouble *d = dfield->data;
    gdouble avg;

    /* Use the union of NaNs found in the data and invalid point mask to mark
     * invalid points.  Gwyddion really does not like NaNs. */
    for (k = 0; k < n; k++) {
        if (gwy_isnan(d[k]) || gwy_isinf(d[k]) || invalid[k]) {
            if (G_UNLIKELY(!mask)) {
                mask = gwy_data_field_new_alike(dfield, TRUE);
                gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(mask),
                                            NULL);
            }
            mask->data[k] = 1.0;
        }
    }

    if (!mask)
        return mask;

    avg = gwy_data_field_area_get_avg_mask(dfield, mask, GWY_MASK_EXCLUDE,
                                           0, 0, dfield->xres, dfield->yres);
    if (gwy_isnan(avg) || gwy_isinf(avg))
        avg = 0.0;

    for (k = 0; k < n; k++) {
        if (gwy_isnan(d[k]) || gwy_isinf(d[k]))
            d[k] = avg;
    }

    return mask;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
