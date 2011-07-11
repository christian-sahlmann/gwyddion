/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2007 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-USERGUIDE]
 * Text matrix of data values
 * .txt
 * Export
 **/

#include "config.h"
#include <stdio.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#include "err.h"

#define EXTENSION ".txt"

typedef struct {
    gboolean add_comment;
    gboolean decimal_dot;
    guint precision;
} ASCIIExportArgs;

static gboolean module_register          (void);
static gint     asciiexport_detect       (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static gboolean asciiexport_export       (GwyContainer *data,
                                          const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static gboolean asciiexport_export_dialog(ASCIIExportArgs *args,
                                          gboolean needs_decimal_dot);
static void     asciiexport_load_args    (GwyContainer *settings,
                                          ASCIIExportArgs *args);
static void     asciiexport_save_args    (GwyContainer *settings,
                                          ASCIIExportArgs *args);

static const ASCIIExportArgs asciiexport_defaults = {
    FALSE, TRUE, 5
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Exports data as simple ASCII matrix."),
    "Yeti <yeti@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti)",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("asciiexport",
                           N_("ASCII data matrix (.txt)"),
                           (GwyFileDetectFunc)&asciiexport_detect,
                           NULL,
                           NULL,
                           (GwyFileSaveFunc)&asciiexport_export);

    return TRUE;
}

static gint
asciiexport_detect(const GwyFileDetectInfo *fileinfo,
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
asciiexport_export(G_GNUC_UNUSED GwyContainer *data,
                   const gchar *filename,
                   GwyRunType mode,
                   GError **error)
{
    ASCIIExportArgs args;
    GwyDataField *dfield;
    gint xres, yres, i, id;
    struct lconv *locale_data;
    const gchar *decimal_dot;
    guint precision, decimal_dot_len;
    gboolean needs_decimal_dot;
    gdouble *d;
    gchar buf[40];
    FILE *fh;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    if (!dfield) {
        err_NO_CHANNEL_EXPORT(error);
        return FALSE;
    }

    asciiexport_load_args(gwy_app_settings_get(), &args);
    locale_data = localeconv();
    decimal_dot = locale_data->decimal_point;
    g_return_val_if_fail(decimal_dot, FALSE);
    decimal_dot_len = strlen(decimal_dot);
    g_return_val_if_fail(decimal_dot_len, FALSE);
    needs_decimal_dot = !gwy_strequal(decimal_dot, ".");

    if (mode == GWY_RUN_INTERACTIVE) {
        if (!asciiexport_export_dialog(&args, needs_decimal_dot)) {
            err_CANCELLED(error);
            return FALSE;
        }
    }

    if (!(fh = g_fopen(filename, "w"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);
    if (args.add_comment) {
        GwySIUnit *units;
        GwySIValueFormat *vf = NULL;
        const guchar *title = "Unknown channel";
        gchar *s;

        g_snprintf(buf, sizeof(buf), "/%d/data/title", id);
        gwy_container_gis_string_by_name(data, buf, &title);
        fprintf(fh, "# %s %s\n", _("Channel:"), title);

        vf = gwy_data_field_get_value_format_xy(dfield,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                vf);
        if (args.decimal_dot) {
            fprintf(fh, "# %s ", _("Width:"));
            g_snprintf(buf, sizeof(buf), "%.*f",
                       vf->precision,
                       gwy_data_field_get_xreal(dfield)/vf->magnitude);
            print_with_decimal_dot(fh, buf, decimal_dot, decimal_dot_len);
            fprintf(fh, " %s\n", vf->units);

            fprintf(fh, "# %s ", _("Height:"));
            g_snprintf(buf, sizeof(buf), "%.*f",
                       vf->precision,
                       gwy_data_field_get_yreal(dfield)/vf->magnitude);
            print_with_decimal_dot(fh, buf, decimal_dot, decimal_dot_len);
            fprintf(fh, " %s\n", vf->units);
        }
        else {
            fprintf(fh, "# %s %.*f %s\n", _("Width:"),
                    vf->precision,
                    gwy_data_field_get_xreal(dfield)/vf->magnitude,
                    vf->units);
            fprintf(fh, "# %s %.*f %s\n", _("Height:"),
                    vf->precision,
                    gwy_data_field_get_yreal(dfield)/vf->magnitude,
                    vf->units);
        }

        units = gwy_data_field_get_si_unit_z(dfield);
        s = gwy_si_unit_get_string(units, GWY_SI_UNIT_FORMAT_VFMARKUP);
        fprintf(fh, "# %s %s\n", _("Value units:"), s);
        g_free(s);

        gwy_si_unit_value_format_free(vf);
    }

    precision = args.precision;
    if (args.decimal_dot) {
        for (i = 0; i < xres*yres; i++) {
            g_snprintf(buf, sizeof(buf), "%.*g%c",
                       precision, d[i], (i + 1) % xres ? '\t' : '\n');
            if (print_with_decimal_dot(fh, buf,
                                       decimal_dot, decimal_dot_len) == EOF)
                goto fail;
        }
    }
    else {
        for (i = 0; i < xres*yres; i++) {
            if (fprintf(fh, "%.*g%c", precision, d[i],
                        (i + 1) % xres ? '\t' : '\n') < 2) {
                goto fail;
            }
        }
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
asciiexport_export_dialog(ASCIIExportArgs *args,
                          gboolean needs_decimal_dot)
{
    GtkWidget *dialog, *vbox, *hbox, *label, *decimal_dot, *add_comment,
              *precision;
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Export Text"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

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
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("_Precision:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    precision = gtk_spin_button_new_with_range(0, 16, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(precision), args->precision);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), precision);
    gtk_box_pack_start(GTK_BOX(hbox), precision, FALSE, FALSE, 0);

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
        asciiexport_save_args(gwy_app_settings_get(), args);
        gtk_widget_destroy(dialog);
    }

    return response == GTK_RESPONSE_OK;
}

static const gchar decimal_dot_key[] = "/module/asciiexport/decimal-dot";
static const gchar add_comment_key[] = "/module/asciiexport/add-comment";
static const gchar precision_key[]   = "/module/asciiexport/precision";

static void
asciiexport_load_args(GwyContainer *settings,
                      ASCIIExportArgs *args)
{
    *args = asciiexport_defaults;

    gwy_container_gis_boolean_by_name(settings, decimal_dot_key,
                                      &args->decimal_dot);
    gwy_container_gis_boolean_by_name(settings, add_comment_key,
                                      &args->add_comment);
    gwy_container_gis_int32_by_name(settings, precision_key, &args->precision);

    args->precision = MIN(args->precision, 16);
}

static void
asciiexport_save_args(GwyContainer *settings,
                      ASCIIExportArgs *args)
{
    gwy_container_set_boolean_by_name(settings, decimal_dot_key,
                                      args->decimal_dot);
    gwy_container_set_boolean_by_name(settings, add_comment_key,
                                      args->add_comment);
    gwy_container_set_int32_by_name(settings, precision_key, args->precision);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
