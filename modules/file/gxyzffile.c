/*
 *  $Id$
 *  Copyright (C) 2013-2016 David Necas (Yeti).
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
 * <mime-type type="application/x-gxyzf-spm">
 *   <comment>Gwyddion XYZ data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="Gwyddion XYZ Field 1.0\n"/>
 *   </magic>
 *   <glob pattern="*.gxyzf"/>
 *   <glob pattern="*.GXYZF"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # Gwyddion simple XYZ files (GXYZF), see http://gwyddion.net/
 * 0 string Gwyddion\ XYZ\ Field\ 1.0\x0d\x0a Gwyddion XYZ field SPM data version 1.0
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Gwyddion XYZ data
 * .gxyzf
 * Read Export
 **/

#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/arithmetic.h>
#include <libprocess/surface.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwymoduleutils-file.h>
#include <app/gwyapp.h>

#include "err.h"

#define MAGIC "Gwyddion XYZ Field 1.0\n"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION ".gxyzf"

typedef struct {
    gboolean all_channels;
} GXYZExportArgs;

static gboolean      module_register              (void);
static gint          gxyzf_detect                 (const GwyFileDetectInfo *fileinfo,
                                                   gboolean only_name);
static GwyContainer* gxyzf_load                   (const gchar *filename,
                                                   GwyRunType mode,
                                                   GError **error);
static gboolean      gxyzf_export                 (GwyContainer *container,
                                                   const gchar *filename,
                                                   GwyRunType mode,
                                                   GError **error);
static gboolean      gxyzf_export_dialog          (GXYZExportArgs *args,
                                                   GwyAppPage pageno,
                                                   const gchar *title);
static gboolean      gxyzf_export_data_fields     (const gchar *filename,
                                                   GwyContainer *container,
                                                   gint id,
                                                   const GXYZExportArgs *args,
                                                   GError **error);
static gint*         gather_compatible_data_fields(GwyContainer *data,
                                                   GwyDataField *dfield,
                                                   guint *nchannels);
static gboolean      gxyzf_export_surfaces        (const gchar *filename,
                                                   GwyContainer *container,
                                                   gint id,
                                                   const GXYZExportArgs *args,
                                                   GError **error);
static gint*         gather_compatible_surfaces   (GwyContainer *data,
                                                   GwySurface *surface,
                                                   guint *nchannels);
static gboolean      write_header                 (FILE *fh,
                                                   guint nchannels,
                                                   guint npoints,
                                                   gchar **titles,
                                                   GwySIUnit *xyunit,
                                                   GwySIUnit **zunits,
                                                   gint xres,
                                                   gint yres,
                                                   GError **error);
static void          load_args                    (GwyContainer *settings,
                                                   GXYZExportArgs *args);
static void          save_args                    (GwyContainer *settings,
                                                   const GXYZExportArgs *args);

static const GXYZExportArgs gxyzf_defaults = {
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Gwyddion XYZ field files."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("gxyzfile",
                           N_("GwyXYZ data files"),
                           (GwyFileDetectFunc)&gxyzf_detect,
                           (GwyFileLoadFunc)&gxyzf_load,
                           NULL,
                           (GwyFileSaveFunc)&gxyzf_export);

    return TRUE;
}

static gint
gxyzf_detect(const GwyFileDetectInfo *fileinfo,
             gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 100;
}

static inline void
append_double(gdouble *target, const gdouble v)
{
    union { guchar pp[8]; double d; } u;
    u.d = v;
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    GWY_SWAP(guchar, u.pp[0], u.pp[7]);
    GWY_SWAP(guchar, u.pp[1], u.pp[6]);
    GWY_SWAP(guchar, u.pp[2], u.pp[5]);
    GWY_SWAP(guchar, u.pp[3], u.pp[4]);
#endif
    *target = u.d;
}

static GwyContainer*
gxyzf_load(const gchar *filename,
           G_GNUC_UNUSED GwyRunType mode,
           GError **error)
{
    GwyContainer *container = NULL;
    GwyTextHeaderParser parser;
    GHashTable *hash = NULL;
    guchar *p, *value, *buffer = NULL, *header = NULL, *datap;
    GwyXYZ *xyzpoints = NULL;
    gdouble *points = NULL;
    gsize size;
    GError *err = NULL;
    GwySIUnit **zunits = NULL;
    GwySIUnit *xyunit = NULL, *zunit = NULL;
    guint nchan = 0, pointlen, pointsize, npoints, i, id;

    if (!g_file_get_contents(filename, (gchar**)&buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    if (size < MAGIC_SIZE || memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Gwyddion XYZ Field");
        goto fail;
    }

    p = buffer + MAGIC_SIZE;
    datap = memchr(p, '\0', size - (p - buffer));
    if (!datap) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated."));
        goto fail;
    }
    header = g_strdup(p);
    datap += 8 - ((datap - buffer) % 8);

    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    if (!(hash = gwy_text_header_parse(header, &parser, NULL, NULL))) {
        g_propagate_error(error, err);
        goto fail;
    }

    if (!(value = g_hash_table_lookup(hash, "NChannels"))) {
        err_MISSING_FIELD(error, "NChannels");
        goto fail;
    }
    nchan = atoi(value);
    if (nchan < 1 || nchan > 1024) {
        err_INVALID(error, "NChannels");
        goto fail;
    }

    pointlen = nchan + 2;
    pointsize = pointlen*sizeof(gdouble);
    if ((size - (datap - buffer)) % pointsize) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Data size %lu is not a multiple of point size %u."),
                    (gulong)(size - (datap - buffer)), pointsize);
        goto fail;
    }
    npoints = (size - (datap - buffer))/pointsize;

    value = g_hash_table_lookup(hash, "XYUnits");
    xyunit = gwy_si_unit_new(value);

    /* If there is ZUnits it applies to all channels. */
    if ((value = g_hash_table_lookup(hash, "ZUnits")))
        zunit = gwy_si_unit_new(value);
    else {
        zunits = g_new0(GwySIUnit*, nchan);
        for (id = 0; id < nchan; id++) {
            gchar buf[16];
            g_snprintf(buf, sizeof(buf), "ZUnits%u", id+1);
            value = g_hash_table_lookup(hash, buf);
            zunits[id] = gwy_si_unit_new(value);
        }
    }

    points = (gdouble*)datap;
    xyzpoints = g_new(GwyXYZ, npoints);
    for (i = 0; i < npoints; i++) {
        append_double(&xyzpoints[i].x, points[i*pointlen]);
        append_double(&xyzpoints[i].y, points[i*pointlen + 1]);
    }

    container = gwy_container_new();
    for (id = 0; id < nchan; id++) {
        GwySurface *surface;
        GwySIUnit *unit;
        GQuark quark;
        gchar buf[24];

        for (i = 0; i < npoints; i++)
            append_double(&xyzpoints[i].z, points[i*pointlen + 2+id]);

        surface = gwy_surface_new_from_data(xyzpoints, npoints);
        unit = gwy_surface_get_si_unit_z(surface);
        if (zunit)
            gwy_serializable_clone(G_OBJECT(zunit), G_OBJECT(unit));
        else
            gwy_serializable_clone(G_OBJECT(zunits[id]), G_OBJECT(unit));
        unit = gwy_surface_get_si_unit_xy(surface);
        gwy_serializable_clone(G_OBJECT(xyunit), G_OBJECT(unit));

        quark = gwy_app_get_surface_key_for_id(id);
        gwy_container_set_object(container, quark, surface);
        g_object_unref(surface);

        g_snprintf(buf, sizeof(buf), "Title%u", id+1);
        if ((value = g_hash_table_lookup(hash, buf))) {
            quark = gwy_app_get_surface_title_key_for_id(id);
            gwy_container_set_const_string(container, quark, value);
        }
        gwy_file_xyz_import_log_add(container, id, NULL, filename);
    }

fail:
    g_free(xyzpoints);
    g_free(buffer);
    if (hash)
        g_hash_table_destroy(hash);
    GWY_OBJECT_UNREF(xyunit);
    GWY_OBJECT_UNREF(zunit);
    if (zunits) {
        for (i = 0; i < nchan; i++)
            GWY_OBJECT_UNREF(zunits[i]);
        g_free(zunits);
    }

    return container;
}

static gboolean
gxyzf_export(G_GNUC_UNUSED GwyContainer *data,
             const gchar *filename,
             GwyRunType mode,
             GError **error)
{
    GXYZExportArgs args;
    GwyDataField *dfield;
    GwySurface *surface;
    gint fid, sid;
    const guchar *title = NULL;
    GwyAppPage pageno;
    gboolean ok;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &fid,
                                     GWY_APP_SURFACE, &surface,
                                     GWY_APP_SURFACE_ID, &sid,
                                     GWY_APP_PAGE, &pageno,
                                     0);

    /* Ensure at most one is set.  We produce an error if no exportable data
     * type is available or both types are available but neither is active.
     * When only one is available or one is active we assume that is what the
     * user wants to export. */
    if (dfield && surface) {
        if (pageno != GWY_PAGE_CHANNELS)
            dfield = NULL;
        if (pageno != GWY_PAGE_XYZS)
            surface = NULL;
    }
    if (!dfield && !surface) {
        err_NO_CHANNEL_EXPORT(error);
        return FALSE;
    }

    if (dfield)
        pageno = GWY_PAGE_CHANNELS;
    if (surface)
        pageno = GWY_PAGE_XYZS;

    load_args(gwy_app_settings_get(), &args);

    if (dfield) {
        gwy_container_gis_string(data, gwy_app_get_data_title_key_for_id(fid),
                                 &title);
    }
    if (surface) {
        gwy_container_gis_string(data,
                                 gwy_app_get_surface_title_key_for_id(sid),
                                 &title);
    }

    if (mode == GWY_RUN_INTERACTIVE) {
        ok = gxyzf_export_dialog(&args, pageno, title);
        save_args(gwy_app_settings_get(), &args);
        if (!ok) {
            err_CANCELLED(error);
            return FALSE;
        }
    }

    if (dfield)
        return gxyzf_export_data_fields(filename, data, fid, &args, error);
    if (surface)
        return gxyzf_export_surfaces(filename, data, sid, &args, error);

    g_return_val_if_reached(FALSE);
}

static gboolean
gxyzf_export_dialog(GXYZExportArgs *args,
                    GwyAppPage pageno,
                    const gchar *title)
{
    GtkWidget *dialog, *vbox, *label, *all_channels;
    gchar *desc = NULL;
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Export GXYZF"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_file_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

    if (pageno == GWY_PAGE_CHANNELS)
        desc = g_strdup_printf("%s %s", _("Channel:"), title);
    else if (pageno == GWY_PAGE_XYZS)
        desc = g_strdup_printf("%s %s", _("XYZ data:"), title);
    label = gtk_label_new(desc);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 6);
    g_free(desc);

    gtk_box_pack_start(GTK_BOX(vbox), gwy_label_new_header(_("Options")),
                       FALSE, FALSE, 0);

    all_channels = gtk_check_button_new_with_mnemonic(_("Multi-channel "
                                                        "file with all "
                                                        "compatible data"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(all_channels),
                                 args->all_channels);
    gtk_box_pack_start(GTK_BOX(vbox), all_channels, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_NONE) {
        args->all_channels
            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(all_channels));
        gtk_widget_destroy(dialog);
    }

    return response == GTK_RESPONSE_OK;
}

static gboolean
gxyzf_export_data_fields(const gchar *filename,
                         GwyContainer *container,
                         gint id,
                         const GXYZExportArgs *args,
                         GError **error)
{
    gdouble *ddbl = NULL;
    gint *ids = NULL;
    guint nchannels, ci, i, j, k, xres, yres;
    size_t npts;
    GwyDataField *dfield, *other;
    const gdouble **d;
    gdouble xreal, yreal, xoff, yoff;
    gchar **titles = NULL;
    GwySIUnit *xyunit, **zunits = NULL;
    GQuark quark;
    FILE *fh;

    if (!(fh = gwy_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    quark = gwy_app_get_data_key_for_id(id);
    dfield = gwy_container_get_object(container, quark);
    g_return_val_if_fail(dfield, FALSE);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    xoff = gwy_data_field_get_xoffset(dfield);
    yoff = gwy_data_field_get_yoffset(dfield);
    xyunit = gwy_data_field_get_si_unit_xy(dfield);

    if (args->all_channels)
        ids = gather_compatible_data_fields(container, dfield, &nchannels);
    else {
        nchannels = 1;
        ids = g_new(gint, 2);
        ids[0] = id;
        ids[1] = -1;
    }
    g_return_val_if_fail(nchannels, FALSE);

    zunits = g_new0(GwySIUnit*, nchannels + 1);
    titles = g_new0(gchar*, nchannels + 1);
    d = g_new0(const gdouble*, nchannels + 1);
    for (ci = 0; ci < nchannels; ci++) {
        quark = gwy_app_get_data_key_for_id(ids[ci]);
        other = gwy_container_get_object(container, quark);
        zunits[ci] = gwy_data_field_get_si_unit_z(other);
        d[ci] = gwy_data_field_get_data_const(other);
        titles[ci] = gwy_app_get_data_field_title(container, ids[ci]);
    }

    if (!write_header(fh, nchannels, xres*yres, titles, xyunit, zunits,
                      xres, yres, error))
        goto fail;

    npts = ((size_t)nchannels + 2)*xres*yres;
    ddbl = g_new(gdouble, npts);
    k = 0;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            append_double(ddbl + k++, (j + 0.5)*xreal/xres + xoff);
            append_double(ddbl + k++, (i + 0.5)*yreal/yres + yoff);
            for (ci = 0; ci < nchannels; ci++)
                append_double(ddbl + k++, *(d[ci]++));
        }
    }

    if (fwrite(ddbl, sizeof(gdouble), npts, fh) != npts) {
        err_WRITE(error);
        goto fail;
    }
    g_free(ddbl);
    fclose(fh);

    return TRUE;

fail:
    if (fh)
        fclose(fh);
    g_unlink(filename);

    if (titles)
        g_strfreev(titles);
    g_free(zunits);
    g_free(d);
    g_free(ddbl);
    g_free(ids);

    return FALSE;
}

static gint*
gather_compatible_data_fields(GwyContainer *container, GwyDataField *dfield,
                              guint *nchannels)
{
    GwyDataField *other;
    gint *ids;
    guint ci, n = 0;
    GQuark quark;

    ids = gwy_app_data_browser_get_data_ids(container);
    for (ci = 0; ids[ci] > -1; ci++) {
        quark = gwy_app_get_data_key_for_id(ids[ci]);
        other = gwy_container_get_object(container, quark);
        if (gwy_data_field_check_compatibility(dfield, other,
                                               GWY_DATA_COMPATIBILITY_RES
                                               | GWY_DATA_COMPATIBILITY_REAL
                                               | GWY_DATA_COMPATIBILITY_LATERAL))
            continue;
        ids[n] = ids[ci];
        n++;
    }
    ids[n] = -1;

    *nchannels = n;
    return ids;
}

static gboolean
gxyzf_export_surfaces(const gchar *filename,
                      GwyContainer *container,
                      gint id,
                      const GXYZExportArgs *args,
                      GError **error)
{
    gdouble *ddbl = NULL;
    gint *ids = NULL;
    guint nchannels, ci, i, k, n;
    size_t npts;
    GwySurface *surface, *other;
    const GwyXYZ **d;
    gchar **titles = NULL;
    GwySIUnit *xyunit, **zunits = NULL;
    GQuark quark;
    FILE *fh;

    if (!(fh = gwy_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    quark = gwy_app_get_surface_key_for_id(id);
    surface = gwy_container_get_object(container, quark);
    g_return_val_if_fail(surface, FALSE);

    xyunit = gwy_surface_get_si_unit_xy(surface);
    n = gwy_surface_get_npoints(surface);

    if (args->all_channels)
        ids = gather_compatible_surfaces(container, surface, &nchannels);
    else {
        nchannels = 1;
        ids = g_new(gint, 2);
        ids[0] = id;
        ids[1] = -1;
    }
    g_return_val_if_fail(nchannels, FALSE);

    zunits = g_new0(GwySIUnit*, nchannels + 1);
    titles = g_new0(gchar*, nchannels + 1);
    d = g_new0(const GwyXYZ*, nchannels + 1);
    for (ci = 0; ci < nchannels; ci++) {
        quark = gwy_app_get_surface_key_for_id(ids[ci]);
        other = gwy_container_get_object(container, quark);
        zunits[ci] = gwy_surface_get_si_unit_z(other);
        d[ci] = gwy_surface_get_data_const(other);
        titles[ci] = gwy_app_get_surface_title(container, ids[ci]);
    }

    if (!write_header(fh, nchannels, n, titles, xyunit, zunits,
                      0, 0, error))
        goto fail;

    npts = ((size_t)nchannels + 2)*n;
    ddbl = g_new(gdouble, npts);
    k = 0;
    for (i = 0; i < n; i++) {
        append_double(ddbl + k++, d[0]->x);
        append_double(ddbl + k++, d[0]->y);
        for (ci = 0; ci < nchannels; ci++) {
            append_double(ddbl + k++, d[ci]->z);
            d[ci]++;
        }
    }

    if (fwrite(ddbl, sizeof(gdouble), npts, fh) != npts) {
        err_WRITE(error);
        goto fail;
    }
    g_free(ddbl);
    fclose(fh);

    return TRUE;

fail:
    if (fh)
        fclose(fh);
    g_unlink(filename);

    if (titles)
        g_strfreev(titles);
    g_free(zunits);
    g_free(d);
    g_free(ddbl);
    g_free(ids);

    return FALSE;
}

static gint*
gather_compatible_surfaces(GwyContainer *container, GwySurface *surface,
                           guint *nchannels)
{
    GwySurface *other;
    gint *ids;
    guint ci, n = 0;
    GQuark quark;

    ids = gwy_app_data_browser_get_xyz_ids(container);
    for (ci = 0; ids[ci] > -1; ci++) {
        quark = gwy_app_get_surface_key_for_id(ids[ci]);
        other = gwy_container_get_object(container, quark);
        if (!gwy_surface_xy_is_compatible(surface, other))
            continue;

        ids[n] = ids[ci];
        n++;
    }
    ids[n] = -1;

    *nchannels = n;
    return ids;
}

static gboolean
write_header(FILE *fh, guint nchannels, guint npoints,
             gchar **titles, GwySIUnit *xyunit, GwySIUnit **zunits,
             gint xres, gint yres,
             GError **error)
{
    static const gchar zeros[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    GwySIUnit *emptyunit;
    GString *header;
    gchar *s;
    guint i, padding;

    header = g_string_new(MAGIC);
    g_string_append_printf(header, "NChannels = %u\n", nchannels);
    g_string_append_printf(header, "NPoints = %u\n", npoints);

    emptyunit = gwy_si_unit_new(NULL);

    if (!gwy_si_unit_equal(xyunit, emptyunit)) {
        s = gwy_si_unit_get_string(xyunit, GWY_SI_UNIT_FORMAT_PLAIN);
        g_string_append_printf(header, "XYUnits = %s\n", s);
        g_free(s);
    }

    for (i = 0; i < nchannels; i++) {
        if (!gwy_si_unit_equal(zunits[i], emptyunit)) {
            s = gwy_si_unit_get_string(zunits[i], GWY_SI_UNIT_FORMAT_PLAIN);
            g_string_append_printf(header, "ZUnits%u = %s\n", i+1, s);
            g_free(s);
        }
    }
    g_object_unref(emptyunit);

    for (i = 0; i < nchannels; i++)
        g_string_append_printf(header, "Title%u = %s\n", i, titles[i]);

    if (xres && yres) {
        g_string_append_printf(header, "XRes = %u\n", xres);
        g_string_append_printf(header, "YRes = %u\n", yres);
    }

    if (fwrite(header->str, 1, header->len, fh) != header->len) {
        err_WRITE(error);
        g_string_free(header, TRUE);
        return FALSE;
    }

    padding = 8 - (header->len % 8);
    g_string_free(header, TRUE);
    if (fwrite(zeros, 1, padding, fh) != padding) {
        err_WRITE(error);
        return FALSE;
    }

    return TRUE;
}

static const gchar all_channels_key[] = "/module/gxyzfile/all-channels";

static void
load_args(GwyContainer *settings,
          GXYZExportArgs *args)
{
    *args = gxyzf_defaults;

    gwy_container_gis_boolean_by_name(settings, all_channels_key,
                                      &args->all_channels);
}

static void
save_args(GwyContainer *settings,
          const GXYZExportArgs *args)
{
    gwy_container_set_boolean_by_name(settings, all_channels_key,
                                      args->all_channels);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
