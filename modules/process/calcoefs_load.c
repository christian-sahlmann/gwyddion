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
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
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
    CLoadArgs *args;
    GtkEntry *name;
} CLoadControls;

enum { RESPONSE_LOAD = 1 };

static gboolean    module_register            (void);
static void        cload                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    cload_dialog                 (CLoadArgs *args, 
                                                GwyDataField *dfield);
static void        cload_dialog_update          (CLoadControls *controls,
                                               CLoadArgs *args);
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
                              N_("/Cali_bration/_3D calibration/Load from text file..."),
                              GWY_STOCK_CWT,
                              CNEW_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Load calibration data from text file."));

    return TRUE;
}

/*
static void
debugcal(GwyCalData *caldata)
{
    gint i;

    printf("######## Calibration data: ###########\n");
    printf("%d data, range %g %g %g x %g %g %g", caldata->ndata,
           caldata->x_from, caldata->y_from, caldata->z_from,
           caldata->x_to, caldata->y_to, caldata->z_to);
    for (i=0; i<caldata->ndata; i++)
    {
        printf("%d   %g %g %g   %g %g %g    %g %g %g\n", i, caldata->x[i], caldata->y[i], caldata->z[i],
               caldata->xerr[i], caldata->yerr[i], caldata->zerr[i], caldata->xunc[i], caldata->yunc[i], caldata->zunc[i]);
    }

}*/


static void
cload(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    CLoadArgs args;
    gboolean ok;
    gint oldid, i, n;
    GwyCalibration *calibration;
    GwyCalData *caldata = NULL;
    gchar *filename;
    gchar *contents;
    gsize len;
    GError *err = NULL;
    gsize pos = 0;
    GString *str;
    GByteArray *barray;
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

    /*if append requested, copy newly created calibration into old one*/
    if (args.duplicate == DUPLICATE_APPEND && (calibration = gwy_inventory_get_item(gwy_calibrations(), args.name)))
    {
        
        filename = g_build_filename(gwy_get_user_dir(), "caldata", calibration->filename, NULL);
        if (!g_file_get_contents(filename,
                                 &contents, &len, &err))
        {
             g_warning("Error loading file: %s\n", err->message);
             g_clear_error(&err);
             return;
        }
        else {
            if (len)
              caldata = GWY_CALDATA(gwy_serializable_deserialize(contents, len, &pos));
            g_free(contents);
        }
        n = caldata->ndata + args.caldata->ndata;

        //add to args->caldata
        args.caldata->x = g_realloc(args.caldata->x, n*sizeof(gdouble));
        args.caldata->y = g_realloc(args.caldata->y, n*sizeof(gdouble));
        args.caldata->z = g_realloc(args.caldata->z, n*sizeof(gdouble));
        args.caldata->xerr = g_realloc(args.caldata->xerr, n*sizeof(gdouble));
        args.caldata->yerr = g_realloc(args.caldata->yerr, n*sizeof(gdouble));
        args.caldata->zerr = g_realloc(args.caldata->zerr, n*sizeof(gdouble));
        args.caldata->xunc = g_realloc(args.caldata->xunc, n*sizeof(gdouble));
        args.caldata->yunc = g_realloc(args.caldata->yunc, n*sizeof(gdouble));
        args.caldata->zunc = g_realloc(args.caldata->zunc, n*sizeof(gdouble));

        for (i=args.caldata->ndata; i<n; i++)
        {
           args.caldata->x[i] = caldata->x[i-caldata->ndata];
           args.caldata->y[i] = caldata->y[i-caldata->ndata];
           args.caldata->z[i] = caldata->z[i-caldata->ndata];
           args.caldata->xerr[i] = caldata->xerr[i-caldata->ndata];
           args.caldata->yerr[i] = caldata->yerr[i-caldata->ndata];
           args.caldata->zerr[i] = caldata->zerr[i-caldata->ndata];
           args.caldata->xunc[i] = caldata->xunc[i-caldata->ndata];
           args.caldata->yunc[i] = caldata->yunc[i-caldata->ndata];
           args.caldata->zunc[i] = caldata->zunc[i-caldata->ndata];
        }
        args.caldata->ndata = n; 
    }

    /*now create and save the resource*/
    if ((calibration = GWY_CALIBRATION(gwy_inventory_get_item(gwy_calibrations(), args.name)))==NULL)
    {
        calibration = gwy_calibration_new(args.name, 8, g_strconcat(args.name, ".dat", NULL));
        gwy_inventory_insert_item(gwy_calibrations(), calibration);
        g_object_unref(calibration);
    }

    filename = gwy_resource_build_filename(GWY_RESOURCE(calibration));
    fh = g_fopen(filename, "w");
    if (!fh) {
        g_warning("Cannot save preset: %s", filename);
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
    if (!g_file_test(g_build_filename(gwy_get_user_dir(), "caldata", NULL), G_FILE_TEST_EXISTS)) {
        g_mkdir(g_build_filename(gwy_get_user_dir(), "caldata", NULL), 0700);
    }
    fh = g_fopen(g_build_filename(gwy_get_user_dir(), "caldata", calibration->filename, NULL), "w");
    if (!fh) {
        g_warning("Cannot save caldata\n");
        return;
    }
    barray = gwy_serializable_serialize(G_OBJECT(args.caldata), NULL);
    //g_file_set_contents(fh, barray->data, sizeof(guint8)*barray->len, NULL);
    fwrite(barray->data, sizeof(guint8), barray->len, fh);
    fclose(fh);

}

static gboolean
cload_dialog(CLoadArgs *args,
            GwyDataField *dfield)
{
    GtkWidget *dialog, *dialog2, *table, *label;
    gint row = 0;
    CLoadControls controls;
    enum { RESPONSE_RESET = 1,
        RESPONSE_DUPLICATE_OVERWRITE = 2,
        RESPONSE_DUPLICATE_APPEND = 3 };

    gint response;

    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Load calibration data"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Load"),
                                                           GTK_STOCK_OPEN),
                                 RESPONSE_LOAD);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    label = gtk_label_new_with_mnemonic(_("Calibration name:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    args->name = g_strdup("new"); //FIXME this should not be here
    controls.name = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_text(controls.name, args->name);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(controls.name),
                     1, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    row++;
    controls.text = gtk_label_new(_("No data loaded"));
    gtk_misc_set_alignment(GTK_MISC(controls.text), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.text,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);


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
                                                  "Calibration '%s' alerady exists",
                                                  args->name);
                gtk_dialog_add_button(GTK_DIALOG(dialog2), "Overwrite", RESPONSE_DUPLICATE_OVERWRITE);
                gtk_dialog_add_button(GTK_DIALOG(dialog2), "Append", RESPONSE_DUPLICATE_APPEND);
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
cload_dialog_update(CLoadControls *controls,
                        CLoadArgs *args)
{

}

static void
load_caldata(CLoadControls *controls)
{
    GtkWidget *dialog;
    gchar *filename;
    GwyCalData *caldata = controls->args->caldata;
    FILE *fr;
    gint i, ndata;
    gchar text[50];
    gdouble xfrom, xto, yfrom, yto, zfrom, zto;
    gdouble x, y, z, xerr, yerr, zerr, xunc, yunc, zunc;
    gchar six[50], siy[50], siz[50];

    dialog = gtk_file_chooser_dialog_new ("Load calibration data",
                      GTK_WINDOW(controls->dialog),
                      GTK_FILE_CHOOSER_ACTION_OPEN,
                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                      NULL);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        fr = fopen(filename, "r");
        if (!fr) { printf("Error: no file open\n");
        } else {
            
            fscanf(fr, "%d", &ndata);
            fscanf(fr, "%lf", &xfrom);
            fscanf(fr, "%lf", &xto);
            fscanf(fr, "%lf", &yfrom);
            fscanf(fr, "%lf", &yto);
            fscanf(fr, "%lf", &zfrom);
            fscanf(fr, "%lf", &zto);

            //printf("loading %d caldata\n", ndata);
            caldata = gwy_caldata_new(ndata);    //FIXME free it somewhere if allocated previously
            caldata->ndata = ndata;
            caldata->x_from = xfrom;
            caldata->x_to = xto;
            caldata->y_from = yfrom;
            caldata->y_to = yto;
            caldata->z_from = zfrom;
            caldata->z_to = zto;
            fscanf(fr, "%s", six);
            fscanf(fr, "%s", siy);
            fscanf(fr, "%s", siz);
            caldata->si_unit_x = gwy_si_unit_new(six);
            caldata->si_unit_y = gwy_si_unit_new(siy);
            caldata->si_unit_z = gwy_si_unit_new(siz);

            for (i=0; i<caldata->ndata; i++)
            {
                fscanf(fr, "%lf", &x);
                fscanf(fr, "%lf", &y);
                fscanf(fr, "%lf", &z);
                fscanf(fr, "%lf", &xerr);
                fscanf(fr, "%lf", &yerr);
                fscanf(fr, "%lf", &zerr);
                fscanf(fr, "%lf", &xunc);
                fscanf(fr, "%lf", &yunc);
                fscanf(fr, "%lf", &zunc);
                caldata->x[i] = x;
                caldata->y[i] = y;
                caldata->z[i] = z;
                caldata->xerr[i] = xerr;
                caldata->yerr[i] = yerr;
                caldata->zerr[i] = zerr;
                caldata->xunc[i] = xunc;
                caldata->yunc[i] = yunc;
                caldata->zunc[i] = zunc;
               /* printf("adding %g %g %g  %g %g %g   %g %g %g\n",
                       caldata->x[i], caldata->y[i], caldata->z[i],
                       caldata->xerr[i], caldata->yerr[i], caldata->zerr[i],
                       caldata->xunc[i], caldata->yunc[i], caldata->zunc[i]);*/
              }
            
            fclose(fr);
            //printf("done.\n");
            g_snprintf(text, sizeof(text), "Loaded %d data points", caldata->ndata);
            gtk_label_set_text(GTK_LABEL(controls->text), text);
        }
        g_free (filename);
        controls->args->caldata = caldata;
    }
    gtk_widget_destroy (dialog);

}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
