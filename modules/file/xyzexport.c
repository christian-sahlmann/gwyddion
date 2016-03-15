/*
 *  @(#) $Id$
 *  Copyright (C) 2015-2016 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

/* NB: Magic is in rawxyz. */

#include "config.h"
#include <locale.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#include "err.h"

#define EXTENSION ".xyz"

typedef struct {
    gboolean add_comment;
    gboolean decimal_dot;
    guint precision;
    GwyMaskingType masking;
} XYZExportArgs;

static gboolean module_register            (void);
static gint     xyzexport_detect           (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static gboolean xyzexport_export           (GwyContainer *data,
                                            const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static gboolean xyzexport_export_dialog    (XYZExportArgs *args,
                                            gboolean have_mask,
                                            gboolean needs_decimal_dot,
                                            GwyAppPage pageno,
                                            const gchar *title);
static gboolean xyzexport_export_data_field(GwyDataField *dfield,
                                            GwyDataField *mfield,
                                            FILE *fh,
                                            const XYZExportArgs *args,
                                            gboolean needs_decimal_dot,
                                            const gchar *decimal_dot,
                                            guint decimal_dot_len);
static gboolean xyzexport_export_surface   (GwySurface *surface,
                                            FILE *fh,
                                            const XYZExportArgs *args,
                                            gboolean needs_decimal_dot,
                                            const gchar *decimal_dot,
                                            guint decimal_dot_len);
static void     xyzexport_load_args        (GwyContainer *settings,
                                            XYZExportArgs *args);
static void     xyzexport_save_args        (GwyContainer *settings,
                                            XYZExportArgs *args);

static const XYZExportArgs xyzexport_defaults = {
    FALSE, TRUE, 5, GWY_MASK_IGNORE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Exports data as simple XYZ text file."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("xyzexport",
                           N_("XYZ text data (.xyz)"),
                           (GwyFileDetectFunc)&xyzexport_detect,
                           NULL,
                           NULL,
                           (GwyFileSaveFunc)&xyzexport_export);

    return TRUE;
}

static gint
xyzexport_detect(const GwyFileDetectInfo *fileinfo,
                 G_GNUC_UNUSED gboolean only_name)
{
    return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;
}

static inline gint
print_with_decimal_dot(FILE *fh,
                       gchar *formatted_number,
                       const gchar *decimal_dot,
                       guint decimal_dot_len)
{
    gchar *pos = strstr(formatted_number, decimal_dot);

    if (!pos)
        return fputs(formatted_number, fh);
    else {
        pos[0] = '.';
        if (decimal_dot_len == 1)
            return fputs(formatted_number, fh);
        else {
            gint l1, l2;

            pos[1] = '\0';
            if ((l1 = fputs(formatted_number, fh)) == EOF)
                return EOF;
            if ((l2 = fputs(pos + decimal_dot_len, fh)) == EOF)
                return EOF;
            return l1 + l2;
        }
    }
}

static gboolean
xyzexport_export(G_GNUC_UNUSED GwyContainer *data,
                 const gchar *filename,
                 GwyRunType mode,
                 GError **error)
{
    XYZExportArgs args;
    GwyDataField *dfield, *mfield;
    GwySurface *surface;
    gint fid, sid;
    struct lconv *locale_data;
    const gchar *decimal_dot;
    guint decimal_dot_len;
    gboolean needs_decimal_dot;
    const guchar *title = NULL;
    GwyAppPage pageno;
    FILE *fh;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &fid,
                                     GWY_APP_MASK_FIELD, &mfield,
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

    if (dfield) {
        pageno = GWY_PAGE_CHANNELS;
    }
    if (surface) {
        pageno = GWY_PAGE_XYZS;
        mfield = NULL;
    }

    xyzexport_load_args(gwy_app_settings_get(), &args);
    locale_data = localeconv();
    decimal_dot = locale_data->decimal_point;
    g_return_val_if_fail(decimal_dot, FALSE);
    decimal_dot_len = strlen(decimal_dot);
    g_return_val_if_fail(decimal_dot_len, FALSE);
    needs_decimal_dot = !gwy_strequal(decimal_dot, ".");

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
        if (!xyzexport_export_dialog(&args, !!mfield, needs_decimal_dot,
                                     pageno, title)) {
            err_CANCELLED(error);
            return FALSE;
        }
    }

    if (!(fh = gwy_fopen(filename, "w"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    if (args.add_comment) {
        GwySIUnit *xyunit = NULL, *zunit = NULL;
        gchar *s;

        if (pageno == GWY_PAGE_CHANNELS) {
            gwy_fprintf(fh, "# %s %s\n", _("Channel:"), title);
            xyunit = gwy_data_field_get_si_unit_xy(dfield);
            zunit = gwy_data_field_get_si_unit_z(dfield);
        }
        if (pageno == GWY_PAGE_XYZS) {
            gwy_fprintf(fh, "# %s %s\n", _("XYZ data:"), title);
            xyunit = gwy_surface_get_si_unit_xy(surface);
            zunit = gwy_surface_get_si_unit_z(surface);
        }

        s = gwy_si_unit_get_string(xyunit, GWY_SI_UNIT_FORMAT_VFMARKUP);
        gwy_fprintf(fh, "# %s %s\n", _("Lateral units:"), s);
        g_free(s);

        s = gwy_si_unit_get_string(zunit, GWY_SI_UNIT_FORMAT_VFMARKUP);
        gwy_fprintf(fh, "# %s %s\n", _("Value units:"), s);
        g_free(s);
    }

    if (pageno == GWY_PAGE_CHANNELS) {
        if (!xyzexport_export_data_field(dfield, mfield, fh, &args,
                                         needs_decimal_dot,
                                         decimal_dot, decimal_dot_len))
            goto fail;
    }
    if (pageno == GWY_PAGE_XYZS) {
        if (!xyzexport_export_surface(surface, fh, &args,
                                      needs_decimal_dot,
                                      decimal_dot, decimal_dot_len))
            goto fail;
    }

    fclose(fh);

    return TRUE;

fail:
    err_WRITE(error);
    fclose(fh);
    g_unlink(filename);

    return FALSE;
}

static gboolean
xyzexport_export_dialog(XYZExportArgs *args,
                        gboolean have_mask,
                        gboolean needs_decimal_dot,
                        GwyAppPage pageno,
                        const gchar *title)
{
    GtkWidget *dialog, *vbox, *hbox, *label, *decimal_dot, *add_comment,
              *precision;
    GSList *masking = NULL;
    gchar *desc = NULL;
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Export XYZ"), NULL, 0,
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
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);
    g_free(desc);

    gtk_box_pack_start(GTK_BOX(vbox), gwy_label_new_header(_("Options")),
                       FALSE, FALSE, 0);

    decimal_dot = gtk_check_button_new_with_mnemonic(_("Use _dot "
                                                       "as decimal separator"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decimal_dot),
                                 args->decimal_dot || !needs_decimal_dot);
    gtk_widget_set_sensitive(decimal_dot, needs_decimal_dot);
    gtk_box_pack_start(GTK_BOX(vbox), decimal_dot, FALSE, FALSE, 0);

    add_comment = gtk_check_button_new_with_mnemonic(_("Add _informational "
                                                       "comment header"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(add_comment),
                                 args->add_comment);
    gtk_box_pack_start(GTK_BOX(vbox), add_comment, FALSE, FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

    label = gtk_label_new_with_mnemonic(_("_Precision:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    precision = gtk_spin_button_new_with_range(0, 16, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(precision), args->precision);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), precision);
    gtk_box_pack_start(GTK_BOX(hbox), precision, FALSE, FALSE, 0);

    if (have_mask) {
        GSList *l;

        label = gwy_label_new_header(_("Masking Mode"));
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

        l = masking = gwy_radio_buttons_create(gwy_masking_type_get_enum(), -1,
                                               NULL, NULL, args->masking);
        while (l) {
            gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(l->data),
                               FALSE, FALSE, 0);
            l = g_slist_next(l);
        }
    }

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_NONE) {
        if (needs_decimal_dot)
            args->decimal_dot
                = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(decimal_dot));
        args->add_comment
            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(add_comment));
        args->precision
            = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(precision));
        if (masking)
            args->masking = gwy_radio_buttons_get_current(masking);
        xyzexport_save_args(gwy_app_settings_get(), args);
        gtk_widget_destroy(dialog);
    }

    return response == GTK_RESPONSE_OK;
}

static gboolean
xyzexport_export_data_field(GwyDataField *dfield, GwyDataField *mfield,
                            FILE *fh, const XYZExportArgs *args,
                            gboolean needs_decimal_dot,
                            const gchar *decimal_dot, guint decimal_dot_len)
{
    GwyMaskingType masking = args->masking;
    guint precision = args->precision;
    gdouble dx, dy, xoff, yoff;
    const gdouble *d, *m = NULL;
    gint xres, yres, i, j;
    gchar buf[40];

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    dx = gwy_data_field_get_xmeasure(dfield);
    dy = gwy_data_field_get_ymeasure(dfield);
    xoff = gwy_data_field_get_xoffset(dfield);
    yoff = gwy_data_field_get_yoffset(dfield);

    d = gwy_data_field_get_data_const(dfield);
    if (mfield && masking != GWY_MASK_IGNORE)
        m = gwy_data_field_get_data_const(mfield);

    if (needs_decimal_dot && args->decimal_dot) {
        for (i = 0; i < yres; i++) {
            gdouble y = dy*(i + 0.5) + yoff;
            for (j = 0; j < xres; j++) {
                gdouble x = dx*(j + 0.5) + xoff;
                gdouble z = d[i*xres + j];

                if (m) {
                    if (masking == GWY_MASK_EXCLUDE && m[i*xres + j] > 0.5)
                        continue;
                    if (masking == GWY_MASK_INCLUDE && m[i*xres + j] < 0.5)
                        continue;
                }

                g_snprintf(buf, sizeof(buf), "%.*g\t", precision, x);
                if (print_with_decimal_dot(fh, buf,
                                           decimal_dot, decimal_dot_len) == EOF)
                    return FALSE;
                g_snprintf(buf, sizeof(buf), "%.*g\t", precision, y);
                if (print_with_decimal_dot(fh, buf,
                                           decimal_dot, decimal_dot_len) == EOF)
                    return FALSE;
                g_snprintf(buf, sizeof(buf), "%.*g\n", precision, z);
                if (print_with_decimal_dot(fh, buf,
                                           decimal_dot, decimal_dot_len) == EOF)
                    return FALSE;
            }
        }
    }
    else {
        for (i = 0; i < yres; i++) {
            gdouble y = dy*(i + 0.5) + yoff;
            for (j = 0; j < xres; j++) {
                gdouble x = dx*(j + 0.5) + xoff;
                gdouble z = d[i*xres + j];

                if (m) {
                    if (masking == GWY_MASK_EXCLUDE && m[i*xres + j] > 0.5)
                        continue;
                    if (masking == GWY_MASK_INCLUDE && m[i*xres + j] < 0.5)
                        continue;
                }

                if (gwy_fprintf(fh, "%.*g\t%.*g\t%.*g\n",
                                precision, x, precision, y, precision, z) < 3)
                    return FALSE;
            }
        }
    }

    return TRUE;
}

static gboolean
xyzexport_export_surface(GwySurface *surface,
                         FILE *fh, const XYZExportArgs *args,
                         gboolean needs_decimal_dot,
                         const gchar *decimal_dot, guint decimal_dot_len)
{
    guint precision = args->precision;
    const GwyXYZ *data;
    guint n, i;
    gchar buf[40];

    data = surface->data;
    n = surface->n;

    if (needs_decimal_dot && args->decimal_dot) {
        for (i = 0; i < n; i++) {
            gdouble x = data[i].x;
            gdouble y = data[i].y;
            gdouble z = data[i].z;

            g_snprintf(buf, sizeof(buf), "%.*g\t", precision, x);
            if (print_with_decimal_dot(fh, buf,
                                       decimal_dot, decimal_dot_len) == EOF)
                return FALSE;
            g_snprintf(buf, sizeof(buf), "%.*g\t", precision, y);
            if (print_with_decimal_dot(fh, buf,
                                       decimal_dot, decimal_dot_len) == EOF)
                return FALSE;
            g_snprintf(buf, sizeof(buf), "%.*g\n", precision, z);
            if (print_with_decimal_dot(fh, buf,
                                       decimal_dot, decimal_dot_len) == EOF)
                return FALSE;
        }
    }
    else {
        for (i = 0; i < n; i++) {
            gdouble x = data[i].x;
            gdouble y = data[i].y;
            gdouble z = data[i].z;

            if (gwy_fprintf(fh, "%.*g\t%.*g\t%.*g\n",
                            precision, x, precision, y, precision, z) < 3)
                return FALSE;
        }
    }

    return TRUE;
}

static const gchar add_comment_key[] = "/module/xyzexport/add-comment";
static const gchar decimal_dot_key[] = "/module/xyzexport/decimal-dot";
static const gchar masking_key[]     = "/module/xyzexport/masking";
static const gchar precision_key[]   = "/module/xyzexport/precision";

static void
xyzexport_load_args(GwyContainer *settings,
                    XYZExportArgs *args)
{
    *args = xyzexport_defaults;

    gwy_container_gis_boolean_by_name(settings, decimal_dot_key,
                                      &args->decimal_dot);
    gwy_container_gis_boolean_by_name(settings, add_comment_key,
                                      &args->add_comment);
    gwy_container_gis_int32_by_name(settings, precision_key, &args->precision);
    gwy_container_gis_enum_by_name(settings, masking_key, &args->masking);

    args->precision = MIN(args->precision, 16);
    args->masking = gwy_enum_sanitize_value(args->masking,
                                            GWY_TYPE_MASKING_TYPE);
}

static void
xyzexport_save_args(GwyContainer *settings,
                    XYZExportArgs *args)
{
    gwy_container_set_boolean_by_name(settings, decimal_dot_key,
                                      args->decimal_dot);
    gwy_container_set_boolean_by_name(settings, add_comment_key,
                                      args->add_comment);
    gwy_container_set_int32_by_name(settings, precision_key, args->precision);
    gwy_container_set_enum_by_name(settings, masking_key, args->masking);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
