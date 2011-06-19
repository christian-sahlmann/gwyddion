/*
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/inttrans.h>
#include <libprocess/gwycaldata.h>
#include <libprocess/gwycalibration.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define CNEW_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
       DUPLICATE_NONE = 0,
          DUPLICATE_OVERWRITE = 1,
             DUPLICATE_APPEND = 2
} ResponseDuplicate;

typedef struct {
    gchar *name;
    GwyCalData *caldata;
    ResponseDuplicate duplicate;
} CLoadArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *text;
    GtkWidget *okbutton;
    CLoadArgs *args;
    GtkEntry *name;
} CLoadControls;

enum { RESPONSE_LOAD = 1 };

static gboolean    module_register            (void);
static void        cload                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    cload_dialog                 (CLoadArgs *args,
                                                GwyDataField *dfield);
static void         load_caldata              (CLoadControls *controls);


static const CLoadArgs cload_defaults = {
    "new calibration",
    NULL,
    0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Load calibration data from text file"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("cload",
                              (GwyProcessFunc)&cload,
                              N_("/Cali_bration/_Load From Text File..."),
                              GWY_STOCK_CWT,
                              CNEW_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Load calibration data from text file."));

    return TRUE;
}


static void
cload(G_GNUC_UNUSED GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    CLoadArgs args;
    gboolean ok;
    gint oldid, n;
    GwyCalibration *calibration;
    GwyCalData *caldata = NULL;
    gchar *filename;
    gchar *contents;
    gsize len;
    GError *err = NULL;
    gsize pos = 0;
    GString *str;
    FILE *fh;


    g_return_if_fail(run & CNEW_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfield);

    //cload_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = cload_dialog(&args, dfield);
        //cload_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    if (!args.caldata) return;

    /*if append requested, copy newly created calibration into old one*/
    if (args.duplicate == DUPLICATE_APPEND && (calibration = gwy_inventory_get_item(gwy_calibrations(), args.name)))
    {

        filename = g_build_filename(gwy_get_user_dir(), "caldata", calibration->filename, NULL);
        if (!g_file_get_contents(filename,
                                 &contents, &len, &err))
        {
             g_warning(N_("Error loading file: %s\n"), err->message);
             g_clear_error(&err);
             return;
        }
        else {
            if (len)
              caldata = GWY_CALDATA(gwy_serializable_deserialize(contents, len, &pos));
            g_free(contents);
        }
        n = gwy_caldata_get_ndata(caldata) + gwy_caldata_get_ndata(args.caldata);
        gwy_caldata_append(args.caldata, caldata);
    }

    /*now create and save the resource*/
    if ((calibration = GWY_CALIBRATION(gwy_inventory_get_item(gwy_calibrations(), args.name)))==NULL)
    {
        calibration = gwy_calibration_new(args.name, g_strconcat(args.name, ".dat", NULL));
        gwy_inventory_insert_item(gwy_calibrations(), calibration);
        g_object_unref(calibration);
    }
    calibration->caldata = args.caldata;

    filename = gwy_resource_build_filename(GWY_RESOURCE(calibration));
    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        g_mkdir(g_build_filename(gwy_get_user_dir(), "calibrations", NULL), 0700);
    }
    fh = g_fopen(filename, "w");
    if (!fh) {
        g_warning(N_("Cannot save preset: %s"), filename);
        g_free(filename);
        return;
    }
    g_free(filename);

    str = gwy_resource_dump(GWY_RESOURCE(calibration));
    fwrite(str->str, 1, str->len, fh);
    fclose(fh);
    g_string_free(str, TRUE);

    gwy_resource_data_saved(GWY_RESOURCE(calibration));

    //debugcal(args.caldata);

    /*now save the calibration data*/
    gwy_caldata_save_data(args.caldata, calibration->filename);


}

static gboolean
cload_dialog(CLoadArgs *args,
            G_GNUC_UNUSED GwyDataField *dfield)
{
    GtkWidget *dialog, *dialog2, *table, *label;
    gint row = 0;
    CLoadControls controls;
    enum { RESPONSE_RESET = 1,
        RESPONSE_DUPLICATE_OVERWRITE = 2,
        RESPONSE_DUPLICATE_APPEND = 3 };

    gint response;

    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(N_("Load Calibration Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    controls.okbutton = gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(N_("verb|_Load"),
                                                           GTK_STOCK_OPEN),
                                 RESPONSE_LOAD);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    label = gtk_label_new_with_mnemonic(N_("Calibration name:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    args->name = g_strdup("new"); //FIXME this should not be here
    controls.name = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_text(controls.name, args->name);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(controls.name),
                     1, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    row++;
    controls.text = gtk_label_new(N_("No data loaded"));
    gtk_misc_set_alignment(GTK_MISC(controls.text), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.text,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    gtk_widget_set_sensitive(controls.okbutton, FALSE);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            /*check whether this resource already exists*/
            args->name = g_strdup(gtk_entry_get_text(controls.name));
            if (gwy_inventory_get_item(gwy_calibrations(), args->name))
            {
                dialog2 = gtk_message_dialog_new (GTK_WINDOW(dialog),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_WARNING,
                                                  GTK_BUTTONS_CANCEL,
                                                  N_("Calibration '%s' already exists"),
                                                  args->name);
                gtk_dialog_add_button(GTK_DIALOG(dialog2), N_("Overwrite"), RESPONSE_DUPLICATE_OVERWRITE);
                gtk_dialog_add_button(GTK_DIALOG(dialog2), N_("Append"), RESPONSE_DUPLICATE_APPEND);
                response = gtk_dialog_run(GTK_DIALOG(dialog2));
                if (response == RESPONSE_DUPLICATE_OVERWRITE) {
                    args->duplicate = DUPLICATE_OVERWRITE;
                    response = GTK_RESPONSE_OK;
                } else if (response == RESPONSE_DUPLICATE_APPEND) {
                    args->duplicate = DUPLICATE_APPEND;
                    response = GTK_RESPONSE_OK;
                }
                gtk_widget_destroy (dialog2);
            } else args->duplicate = DUPLICATE_NONE;
            break;

            case RESPONSE_LOAD:
            load_caldata(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}


static void
load_caldata(CLoadControls *controls)
{
    GtkWidget *dialog, *msgdialog;
    gchar *filename;
    GwyCalData *caldata = controls->args->caldata;
    gint i, ndata=0;
    gchar mtext[50];
    gdouble xfrom, xto, yfrom, yto, zfrom, zto;
    gdouble x, y, z, xerr, yerr, zerr, xunc, yunc, zunc;
    gdouble *px, *py, *pz, *pxerr, *pyerr, *pzerr, *pxunc, *pyunc, *pzunc;
    gchar *line, *text = NULL;
    gsize size;
    gint xpower10, ypower10, zpower10;
    gboolean ok = TRUE;
    GError *err = NULL;

    dialog = gtk_file_chooser_dialog_new (N_("Load calibration data"),
                      GTK_WINDOW(controls->dialog),
                      GTK_FILE_CHOOSER_ACTION_OPEN,
                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                      NULL);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        if (!g_file_get_contents(filename, &text, &size, &err)) {
            msgdialog = gtk_message_dialog_new (GTK_WINDOW(dialog),
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_CLOSE,
                                             N_("Error loading file '%s'"),
                                             filename);
            gtk_dialog_run(GTK_DIALOG(msgdialog));
            gtk_widget_destroy(msgdialog);
            gtk_widget_destroy(dialog);
        } else {

            line = gwy_str_next_line(&text);
            g_strstrip(line);
            ndata = g_ascii_strtod(line, &line);

            line = gwy_str_next_line(&text);
            g_strstrip(line);
            xfrom = g_ascii_strtod(line, &line);
            xto = g_ascii_strtod(line, &line);

            line = gwy_str_next_line(&text);
            g_strstrip(line);
            yfrom = g_ascii_strtod(line, &line);
            yto = g_ascii_strtod(line, &line);

            line = gwy_str_next_line(&text);
            g_strstrip(line);
            zfrom = g_ascii_strtod(line, &line);
            zto = g_ascii_strtod(line, &line);

            if (caldata) gwy_object_unref(caldata);
            caldata = gwy_caldata_new(ndata);

            line = gwy_str_next_line(&text);
            g_strstrip(line);
            gwy_caldata_set_si_unit_x(caldata, gwy_si_unit_new_parse(line, &xpower10));

            line = gwy_str_next_line(&text);
            g_strstrip(line);
            gwy_caldata_set_si_unit_y(caldata, gwy_si_unit_new_parse(line, &ypower10));

            line = gwy_str_next_line(&text);
            g_strstrip(line);
            gwy_caldata_set_si_unit_z(caldata, gwy_si_unit_new_parse(line, &zpower10));

            gwy_caldata_set_range(caldata,
                                  xfrom*pow10(xpower10), xto*pow10(xpower10),
                                  yfrom*pow10(ypower10), yto*pow10(ypower10),
                                  zfrom*pow10(zpower10), zto*pow10(zpower10));

            px = gwy_caldata_get_x(caldata);
            py = gwy_caldata_get_y(caldata);
            pz = gwy_caldata_get_z(caldata);
            pxerr = gwy_caldata_get_xerr(caldata);
            pyerr = gwy_caldata_get_yerr(caldata);
            pzerr = gwy_caldata_get_zerr(caldata);
            pxunc = gwy_caldata_get_xunc(caldata);
            pyunc = gwy_caldata_get_yunc(caldata);
            pzunc = gwy_caldata_get_zunc(caldata);

            for (i=0; i<gwy_caldata_get_ndata(caldata); i++) {
                line = gwy_str_next_line(&text);
                if (!line) {
                    g_snprintf(mtext, sizeof(mtext), N_("Error: not enough points."));
                    gtk_label_set_text(GTK_LABEL(controls->text), mtext);
                    ok = FALSE;
                    break;
                }
                g_strstrip(line);
                x = g_ascii_strtod(line, &line);
                y = g_ascii_strtod(line, &line);
                z = g_ascii_strtod(line, &line);
                xerr = g_ascii_strtod(line, &line);
                yerr = g_ascii_strtod(line, &line);
                zerr = g_ascii_strtod(line, &line);
                xunc = g_ascii_strtod(line, &line);
                yunc = g_ascii_strtod(line, &line);
                zunc = g_ascii_strtod(line, &line);

                px[i] = x;
                py[i] = y;
                pz[i] = z;
                pxerr[i] = xerr;
                pyerr[i] = yerr;
                pzerr[i] = zerr;
                pxunc[i] = xunc;
                pyunc[i] = yunc;
                pzunc[i] = zunc;
            }
            if (ok) {
                g_snprintf(mtext, sizeof(mtext), N_("Loaded %d data points"), gwy_caldata_get_ndata(caldata));
                gtk_label_set_text(GTK_LABEL(controls->text), mtext);
                gtk_widget_set_sensitive(controls->okbutton, TRUE);
            }
        }
        g_free (filename);
        if (ok) controls->args->caldata = caldata;
    }
    gtk_widget_destroy (dialog);

}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
