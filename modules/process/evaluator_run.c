/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyexpr.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/correct.h>
#include <libprocess/correlation.h>
#include <libgwymodule/gwymodule-process.h>
#include <libprocess/filters.h>
#include <libprocess/hough.h>
#include <libprocess/grains.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <app/gwyapp.h>
#include "evaluator.h"

#define EVALUATOR_RUN_RUN_MODES GWY_RUN_IMMEDIATE


typedef struct {
    gboolean bresult;
    gdouble result;
    gchar **variable_ids;
    gdouble *variables;
    gint nvariables;
    gchar *error;
} EvalData;

typedef struct {
     GwyEvaluator *evaluator;
     GwyDataField *dfield;
     GPtrArray *evaldata;
     GArray *evaluated_statements;
} ErunArgs;



static gboolean            module_register(void);
static void                evaluator_run(GwyContainer *data,
                                         GwyRunType run);
static void                evaluator_run_automatically(GwyContainer *data,
                                         GwyRunType run);
static gchar*              get_filename();
static GwyEvaluator*       get_evaluator(const gchar *filename);
static void                get_results(ErunArgs *args);
static void                save_report_cb(GtkWidget *button, GString *report);
static void                results_window_response_cb(GtkWidget *window,
                                                      gint response,
                                                      GString *report);
static void                create_results_window(ErunArgs *args);
static GString*            create_evaluator_report(ErunArgs *args);
static void                test_stupid_class_init();
static void                evaluate(ErunArgs *args);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates mask of evaluator_run."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("evaluator_run",
                              (GwyProcessFunc)&evaluator_run,
                              N_("/_Evaluator/Run"),
                              NULL,
                              EVALUATOR_RUN_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Run evaluator from stored preset"));

    gwy_process_func_register("evaluator_run_automatically",
                              (GwyProcessFunc)&evaluator_run_automatically,
                              N_("/_Evaluator/Run Automatically"),
                              NULL,
                              EVALUATOR_RUN_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Run evaluator from stored preset automatically"));

    evaluator_types_init();

    return TRUE;
}



static void
evaluator_run(GwyContainer *data, GwyRunType run)
{
    ErunArgs args;
    gchar *filename;

    g_return_if_fail(run & EVALUATOR_RUN_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &args.dfield,
                                     0);
    g_return_if_fail(args.dfield);

    /*get filename of evaluator preset*/
    filename = get_filename();
    if (!filename) return;

    args.evaluator = get_evaluator(filename);

    /*run preset*/
    args.evaldata = g_ptr_array_new();
    args.evaluated_statements = g_array_new(FALSE, FALSE, sizeof(gboolean));
    get_results(&args);

    /*output result window*/

    create_results_window(&args);

}

static void
evaluator_run_automatically(GwyContainer *data, GwyRunType run)
{
    static const gchar setup_key[]  = "/module/evaluator_run/setup/filename";
    static const gchar output_key[] = "/module/evaluator_run/output/filename";
    ErunArgs args;
    GwyContainer *settings;
    GString *report;
    const gchar *setup_filename, *output_filename;
    FILE *fh;

    g_return_if_fail(run & EVALUATOR_RUN_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &args.dfield,
                                     0);
    g_return_if_fail(args.dfield);

    settings = gwy_app_settings_get();
    setup_filename = gwy_container_get_string_by_name(settings, setup_key);
    output_filename = gwy_container_get_string_by_name(settings, output_key);
    g_return_if_fail(setup_filename && output_filename);
    args.evaluator = get_evaluator(setup_filename);
    g_return_if_fail(args.evaluator);

    /* run preset */
    args.evaldata = g_ptr_array_new();
    args.evaluated_statements = g_array_new(FALSE, FALSE, sizeof(gboolean));
    get_results(&args);

    report = create_evaluator_report(&args);

    fh = g_fopen(output_filename, "w");
    if (fh) {
         fputs(report->str, fh);
         fclose(fh);
    }
    else
        g_warning("Cannot write results to %s: %s",
                  output_filename, g_strerror(errno));

    g_string_free(report, TRUE);
    g_object_unref(args.evaluator);
    g_array_free(args.evaluated_statements, TRUE);
    g_ptr_array_free(args.evaldata, TRUE);
}

static gint
get_closest_point(GwyDataField *dfield, gdouble *xdata, gdouble *ydata, gdouble *zdata, gint ndata,
                  gdouble x, gdouble y, gint width, gint height)
{
    gint i, mini = -1;
    gdouble minval, val;

    minval = G_MAXDOUBLE;
    for (i = 0; i<ndata; i++){
        if (fabs(x - xdata[i]) < width/2 &&
            fabs(y - ydata[i]) < height/2)
        {
              val = sqrt((x - xdata[i])*(x - xdata[i])
                         + (y - ydata[i])*(y - ydata[i]));
              if (minval > val) {
                  minval = val;
                  mini = i;
              }
        }
    }
    return mini;

}

static void
get_detected_points(ErunArgs *args)
{
    GwyDataField *dfield;
    GwyDataField *filtered, *x_gradient, *y_gradient;
    gint ndata = 50, skip = 10;
    gint j;
    guint k;
    gdouble xdata[50], ydata[50];
    gdouble zdata[50];
    gdouble threshval, hmin, hmax;
    GwySearchPoint *pspoint;

    dfield = args->dfield;
    filtered = gwy_data_field_new_alike(dfield, FALSE);

    x_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(x_gradient, GWY_ORIENTATION_HORIZONTAL);
    y_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(y_gradient, GWY_ORIENTATION_VERTICAL);

    gwy_data_field_filter_harris(x_gradient, y_gradient, filtered, 4, 0.07);
    gwy_data_field_invert(filtered, FALSE, FALSE, TRUE);

    hmin = gwy_data_field_get_min(filtered);
    hmax = gwy_data_field_get_max(filtered);
    threshval = hmin + (hmax - hmin)*0.6;
    ndata = gwy_data_field_get_local_maxima_list(filtered,
                                         xdata,
                                         ydata,
                                         zdata,
                                         ndata,
                                         skip,
                                         threshval,
                                         TRUE);

    for (k=0; k<args->evaluator->detected_point_array->len; k++) {
        pspoint = g_ptr_array_index(args->evaluator->detected_point_array, k);

        j = get_closest_point(dfield, xdata, ydata, zdata, ndata,
                              gwy_data_field_rtoi(dfield, pspoint->xc),
                              gwy_data_field_rtoj(dfield, pspoint->yc),
                              pspoint->width, pspoint->height);

        pspoint->xc = gwy_data_field_itor(dfield, xdata[j]);
        pspoint->yc = gwy_data_field_jtor(dfield, ydata[j]);
    }

    gwy_object_unref(filtered);
    gwy_object_unref(x_gradient);
    gwy_object_unref(y_gradient);
}

static void
get_detected_lines(ErunArgs *args)
{
    GwyDataField *dfield, *f1, *f2, *edgefield, *filtered, *water;
    gdouble xdata[10], ydata[10];
    gdouble zdata[10];
    GwySearchLine *psline;
    gint ndata = 10, skip = 10;
    gint i, j, px1, px2, py1, py2;
    gdouble threshval, hmin, hmax, rho, theta;

    dfield = args->dfield;
    edgefield = gwy_data_field_duplicate(dfield);
    f1 = gwy_data_field_duplicate(dfield);
    f2 = gwy_data_field_duplicate(dfield);
    filtered = gwy_data_field_new(3*(sqrt(gwy_data_field_get_xres(dfield)*gwy_data_field_get_xres(dfield)
                             +gwy_data_field_get_yres(dfield)*gwy_data_field_get_yres(dfield))),
                             360, 0, 0,
                             FALSE);


    gwy_data_field_filter_canny(edgefield, 0.1);
    gwy_data_field_filter_sobel(f1, GWY_ORIENTATION_HORIZONTAL);
    gwy_data_field_filter_sobel(f2, GWY_ORIENTATION_VERTICAL);
    gwy_data_field_hough_line(edgefield,
                              f1,
                              f2,
                              filtered,
                              1,
                              FALSE);

    water = gwy_data_field_duplicate(filtered);
    gwy_data_field_grains_splash_water(filtered, water, 2,
                                 0.005*(gwy_data_field_get_max(filtered) - gwy_data_field_get_min(filtered)));

    hmin = gwy_data_field_get_min(water);
    hmax = gwy_data_field_get_max(water);
    threshval = hmin + (hmax - hmin)*0.2;
    ndata = gwy_data_field_get_local_maxima_list(water,
                                         xdata,
                                         ydata,
                                         zdata,
                                         ndata,
                                         skip,
                                         threshval,
                                         TRUE);

    for (i=0; i<ndata; i++) {
        xdata[i] = ((gdouble)xdata[i])*
                                    gwy_data_field_get_xreal(filtered)/((gdouble)gwy_data_field_get_xres(filtered))
                                                            - gwy_data_field_get_xreal(filtered)/2.0;
        ydata[i] = ((gdouble)ydata[i])*G_PI/((gdouble)gwy_data_field_get_yres(filtered)) + G_PI/4;


        gwy_data_field_hough_polar_line_to_datafield(dfield,
                                                     xdata[i], ydata[i],
                                                     &px1, &px2, &py1, &py2);

        gwy_data_field_hough_datafield_line_to_polar(dfield,
                                                 px1, px2, py1, py2,
                                                 &rho,
                                                 &theta);
        xdata[i] = rho;
        ydata[i] = theta;
    }

    for (i=0; i<args->evaluator->detected_line_array->len; i++) {
        psline = g_ptr_array_index(args->evaluator->detected_line_array, i);


        j = get_closest_point(dfield, xdata, ydata, zdata, ndata,
                              psline->rhoc, psline->thetac, psline->rho, psline->theta);

        if (j == -1) continue;
        gwy_data_field_hough_polar_line_to_datafield(dfield,
                                                     xdata[i], ydata[i],
                                                     &px1, &px2, &py1, &py2);


        psline->xstart = gwy_data_field_itor(dfield, px1);
        psline->ystart = gwy_data_field_jtor(dfield, py1);
        psline->xend = gwy_data_field_itor(dfield, px2);
        psline->yend = gwy_data_field_jtor(dfield, py2);
        psline->rhoc = xdata[j];
        psline->thetac = ydata[j];

    }

    g_object_unref(filtered);
    g_object_unref(edgefield);
    g_object_unref(f1);
    g_object_unref(f2);
}

static void
get_maximum(GwyDataField *score, gint *maxcol, gint *maxrow)
{
    gint col, row;
    gdouble maxval = -G_MAXDOUBLE;


    for (row = 0; row < gwy_data_field_get_yres(score); row++) {    /*row */
           for (col = 0; col < gwy_data_field_get_xres(score); col++) {

               if (gwy_data_field_get_val(score, col, row) > maxval) {
                   maxval = gwy_data_field_get_val(score, col, row);
                   *maxcol = col;
                   *maxrow = row;
               }
           }
    }
}

static void
get_correlation_points(ErunArgs *args)
{
    GwyDataField *dfield, *part, *score;
    gint xstart, ystart, width, height, colmax, rowmax;
    guint k;
    GwyCorrelationPoint *pcpoint;

    dfield = args->dfield;

    for (k=0; k<args->evaluator->correlation_point_array->len; k++) {
        pcpoint = g_ptr_array_index(args->evaluator->correlation_point_array, k);

        xstart = gwy_data_field_rtoi(dfield, pcpoint->xc) - pcpoint->swidth/2;
        ystart = gwy_data_field_rtoj(dfield, pcpoint->yc) - pcpoint->sheight/2;
        xstart = MAX(0, xstart);
        ystart = MAX(0, ystart);
        width = MIN(pcpoint->swidth, gwy_data_field_get_xres(dfield) - xstart - 1);
        height = MIN(pcpoint->sheight, gwy_data_field_get_yres(dfield) - ystart - 1);

        part = gwy_data_field_area_extract(dfield,
                                           xstart, ystart,
                                           width, height);
        score = gwy_data_field_new_alike(part, FALSE);

        gwy_data_field_correlate(part, pcpoint->pattern, score, GWY_CORRELATION_NORMAL);

        get_maximum(score, &colmax, &rowmax);

        pcpoint->xc = gwy_data_field_itor(dfield, colmax + xstart);
        pcpoint->yc = gwy_data_field_jtor(dfield, rowmax + ystart);
        pcpoint->score = gwy_data_field_get_val(score, colmax, rowmax);

        g_object_unref(score);
        g_object_unref(part);
    }

}
static void
get_features(ErunArgs *args)
{
    GwySearchPoint *pspoint;
    GwySearchLine *psline;
    GwyCorrelationPoint *pcpoint;
    gint k;
    
    
    gwy_debug("Requested:\n");
    for (k=0; k<args->evaluator->detected_point_array->len; k++) {
        pspoint = g_ptr_array_index(args->evaluator->detected_point_array, k);
        gwy_debug("dpoint: %s: %g %g\n", pspoint->id, pspoint->xc, pspoint->yc);
    }
    for (k=0; k<args->evaluator->detected_line_array->len; k++) {
        psline = g_ptr_array_index(args->evaluator->detected_line_array, k);
        gwy_debug("dline: %s: %g %g\n", psline->id, psline->rhoc, psline->thetac);
    }
    for (k=0; k<args->evaluator->correlation_point_array->len; k++) {
        pcpoint = g_ptr_array_index(args->evaluator->correlation_point_array, k);
        gwy_debug("cpoint: %s: %g %g\n", pcpoint->id, pcpoint->xc, pcpoint->yc);
    }
    

    get_detected_points(args);
    get_detected_lines(args);
    get_correlation_points(args);

    /*print summary for debug*/
    
    gwy_debug("Detected:\n");
    for (k=0; k<args->evaluator->detected_point_array->len; k++) {
        pspoint = g_ptr_array_index(args->evaluator->detected_point_array, k);
        gwy_debug("dpoint: %s: %g %g\n", pspoint->id, pspoint->xc, pspoint->yc);
    }
    for (k=0; k<args->evaluator->detected_line_array->len; k++) {
        psline = g_ptr_array_index(args->evaluator->detected_line_array, k);
        gwy_debug("dline: %s: %g %g\n", psline->id, psline->rhoc, psline->thetac);
     }
    for (k=0; k<args->evaluator->correlation_point_array->len; k++) {
        pcpoint = g_ptr_array_index(args->evaluator->correlation_point_array, k);
        gwy_debug("cpoint: %s: %g %g score: %g\n", pcpoint->id, pcpoint->xc, pcpoint->yc, pcpoint->score);
     }
     

}

static void
get_results(ErunArgs *args)
{
    get_features(args);

    evaluate(args);
}

static gchar*
get_filename()
{
    GtkDialog *filedialog;
    gchar *filename;

    filedialog = GTK_DIALOG(gtk_file_chooser_dialog_new ("Choose evaluator",
                                                         GTK_WINDOW(gwy_app_get_current_window(GWY_APP_WINDOW_TYPE_DATA)),
                                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                         NULL));
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filedialog),
                                         gwy_app_get_current_directory());
    if (gtk_dialog_run (GTK_DIALOG (filedialog)) == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filedialog));
    }
    else filename = NULL;
    gtk_widget_destroy(GTK_WIDGET(filedialog));

    return filename;

}


/*FIXME why this stupix piece of code must precede any deserialization
 in order to prevent all types being unknown?*/
static void
test_stupid_class_init()
{
    GwyEvaluator *ev;
    GwySearchPoint *spset;
    GwySearchLine *slset;
    GwyFixedPoint *fpset;
    GwyFixedLine *flset;
    GwyCorrelationPoint *cpset;
    GwyEvaluatorTask *etset;

    ev = gwy_evaluator_new();
    spset = gwy_search_point_new();
    slset = gwy_search_line_new();
    fpset = gwy_fixed_point_new();
    flset = gwy_fixed_line_new();
    cpset = gwy_correlation_point_new();
    etset = gwy_evaluator_task_new();

    g_object_unref(ev);
    g_object_unref(spset);
    g_object_unref(slset);
    g_object_unref(fpset);
    g_object_unref(flset);
    g_object_unref(cpset);
    g_object_unref(etset);
}


static GwyEvaluator*
get_evaluator(const gchar *filename)
{
    guchar *buffer = NULL;
    GError *err = NULL;
    GwyEvaluator *evaluator;
    gsize size = 0;
    gsize pos = 0;



    test_stupid_class_init();
    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
            printf("get contetns failed\n");
                    return NULL;
    }

    evaluator = GWY_EVALUATOR(gwy_serializable_deserialize(buffer, size, &pos));
    if (!evaluator) {
         printf("deserialize failed\n");
         return NULL;
    }

    return evaluator;
}

static void
save_report_cb(GtkWidget *button, GString *report)
{
    const gchar *filename;
    gchar *filename_sys, *filename_utf8;
    GtkWidget *dialog;
    FILE *fh;

    dialog = gtk_widget_get_toplevel(button);
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    filename_sys = g_strdup(filename);
    gtk_widget_destroy(dialog);

    fh = g_fopen(filename_sys, "a");
    if (fh) {
        fputs(report->str, fh);
        fclose(fh);
        return;
    }

    filename_utf8 = g_filename_to_utf8(filename_sys, -1, 0, 0, NULL);
    dialog = gtk_message_dialog_new(NULL,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    _("Cannot save report to %s.\n%s\n"),
                                    filename_utf8,
                                    g_strerror(errno));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void
results_window_response_cb(GtkWidget *window,
                           gint response,
                           GString *report)
{
    GtkWidget *dialog;

    if (response == GTK_RESPONSE_CLOSE
        || response == GTK_RESPONSE_DELETE_EVENT
        || response == GTK_RESPONSE_NONE) {
        if (report)
            g_string_free(report, TRUE);
        gtk_widget_destroy(window);
        return;
    }

    g_assert(report);
    dialog = gtk_file_selection_new(_("Save Evaluator Report"));

    g_signal_connect(GTK_FILE_SELECTION(dialog)->ok_button, "clicked",
                     G_CALLBACK(save_report_cb), report);
    g_signal_connect_swapped(GTK_FILE_SELECTION(dialog)->cancel_button,
                             "clicked",
                             G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_widget_show_all(dialog);
}


static void
create_results_window(ErunArgs *args)
{
    enum { RESPONSE_SAVE = 1 };
    GtkWidget *window, *table, *label;
    GString *str;
    gint k;
    GwyEvaluatorTask *petask;
    EvalData *edata;
        

    window = gtk_dialog_new_with_buttons(_("Evaluator results"), NULL, 0,
                                         GTK_STOCK_SAVE, RESPONSE_SAVE,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_CLOSE);

    table = gtk_table_new(args->evaluator->expression_task_array->len, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), table,
                       FALSE, FALSE, 0);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Statement </b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                                       GTK_EXPAND | GTK_FILL, 0, 2, 2);
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Expression </b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
                                       GTK_EXPAND | GTK_FILL, 0, 2, 2);
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Threshold </b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
                                       GTK_EXPAND | GTK_FILL, 0, 2, 2);
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Result </b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1,
                                       GTK_EXPAND | GTK_FILL, 0, 2, 2);
       
    for (k=0; k<args->evaluator->expression_task_array->len; k++) {
        petask = g_ptr_array_index(args->evaluator->expression_task_array, k);
        edata = g_ptr_array_index(args->evaldata, k);

        label = gtk_label_new("");
        if (edata->bresult) gtk_label_set_text(GTK_LABEL(label), "Success ");
        else gtk_label_set_text(GTK_LABEL(label), "Failed ");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, k+1, k+2,
                                             GTK_EXPAND | GTK_FILL, 0, 2, 2);
         
        label = gtk_label_new(petask->expression);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 1, 2, k+1, k+2,
                                             GTK_EXPAND | GTK_FILL, 0, 2, 2);
        
        label = gtk_label_new(petask->threshold);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 2, 3, k+1, k+2,
                                             GTK_EXPAND | GTK_FILL, 0, 2, 2);
  
        
        label = gtk_label_new(g_strdup_printf("%g", edata->result));
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 3, 4, k+1, k+2,
                                             GTK_EXPAND | GTK_FILL, 0, 2, 2);
       
    }

    str = create_evaluator_report(args);
    

    g_signal_connect(window, "response",
                     G_CALLBACK(results_window_response_cb), str);
    gtk_widget_show_all(window);
}


static GString*
create_evaluator_report(ErunArgs *args)
{
    GString *report;
    GwySearchPoint *pspoint;
    GwySearchLine *psline;
    GwyFixedPoint *pfpoint;
    GwyFixedLine *pfline;
    GwyCorrelationPoint *pcpoint;
    GwyEvaluatorTask *petask;
    EvalData *edata;

    gint k, i;

    report = g_string_new("");

    for (k=0; k<args->evaluator->detected_point_array->len; k++) {
        pspoint = g_ptr_array_index(args->evaluator->detected_point_array, k);
        g_string_append_printf(report, "point %s %d %d\n", pspoint->id,
                               (gint)gwy_data_field_rtoi(args->dfield, pspoint->xc),
                               (gint)gwy_data_field_rtoj(args->dfield, pspoint->yc));
    }
    for (k=0; k<args->evaluator->detected_line_array->len; k++) {
        psline = g_ptr_array_index(args->evaluator->detected_line_array, k);
        g_string_append_printf(report, "line %s %d %d %d %d\n", psline->id,
                               (gint)gwy_data_field_rtoi(args->dfield, psline->xstart),
                               (gint)gwy_data_field_rtoj(args->dfield, psline->ystart),
                               (gint)gwy_data_field_rtoi(args->dfield, psline->xend),
                               (gint)gwy_data_field_rtoj(args->dfield, psline->yend));
    }
    for (k=0; k<args->evaluator->fixed_point_array->len; k++) {
        pfpoint = g_ptr_array_index(args->evaluator->fixed_point_array, k);
        g_string_append_printf(report, "point %s %d %d\n", pfpoint->id,
                               (gint)gwy_data_field_rtoi(args->dfield, pfpoint->xc),
                               (gint)gwy_data_field_rtoj(args->dfield, pfpoint->yc));
    }
    for (k=0; k<args->evaluator->fixed_line_array->len; k++) {
        pfline = g_ptr_array_index(args->evaluator->fixed_line_array, k);
        g_string_append_printf(report, "line %s %d %d %d %d\n", pfline->id,
                               (gint)gwy_data_field_rtoi(args->dfield, pfline->xstart),
                               (gint)gwy_data_field_rtoj(args->dfield, pfline->ystart),
                               (gint)gwy_data_field_rtoi(args->dfield, pfline->xend),
                               (gint)gwy_data_field_rtoj(args->dfield, pfline->yend));
    }
     for (k=0; k<args->evaluator->correlation_point_array->len; k++) {
        pcpoint = g_ptr_array_index(args->evaluator->correlation_point_array, k);
        g_string_append_printf(report, "point %s %d %d\n", pcpoint->id,
                               (gint)gwy_data_field_rtoi(args->dfield, pcpoint->xc),
                               (gint)gwy_data_field_rtoj(args->dfield, pcpoint->yc));
    }
    for (k=0; k<args->evaluator->expression_task_array->len; k++) {
        petask = g_ptr_array_index(args->evaluator->expression_task_array, k);
        if (k < args->evaldata->len) edata = g_ptr_array_index(args->evaldata, k);
        else break;

        if (edata->bresult) g_string_append_printf(report, "SUCCESS ");
        else g_string_append_printf(report, "FAILED ");
        g_string_append_printf(report, "\"%s\" \"%s\" \"%g\" ", petask->expression, petask->threshold, edata->result);
        for (i=1; i<edata->nvariables; i++) g_string_append_printf(report, "%s ", edata->variable_ids[i]);
        g_string_append_printf(report, "\n");
    }

    return report;
}


static gdouble
eval_line_min(ErunArgs *args, gpointer a)
{
    GwySearchLine *sline;
    GwyDataLine *dline;
    gdouble val;
    gint res, px1, py1, px2, py2;

    sline = GWY_SEARCH_LINE(a);

    px1 = gwy_data_field_rtoi(args->dfield, sline->xstart);
    px2 = gwy_data_field_rtoi(args->dfield, sline->xend);
    py1 = gwy_data_field_rtoj(args->dfield, sline->ystart);
    py2 = gwy_data_field_rtoj(args->dfield, sline->yend);

    res = sqrt((px1 - px2)*(px1 - px2) + (py1 - py2)*(py1 - py2));
    dline = gwy_data_field_get_profile(args->dfield, NULL,
                                       px1, py1, px2, py2, res, 1,
                                       GWY_INTERPOLATION_BILINEAR);

    val = gwy_data_line_get_min(dline);
    g_object_unref(dline);
    return val;
}
static gdouble
eval_line_max(ErunArgs *args, gpointer a)
{
    GwySearchLine *sline;
    GwyDataLine *dline;
    gdouble val;
    gint res, px1, py1, px2, py2;

    sline = GWY_SEARCH_LINE(a);

    px1 = gwy_data_field_rtoi(args->dfield, sline->xstart);
    px2 = gwy_data_field_rtoi(args->dfield, sline->xend);
    py1 = gwy_data_field_rtoj(args->dfield, sline->ystart);
    py2 = gwy_data_field_rtoj(args->dfield, sline->yend);

    res = sqrt((px1 - px2)*(px1 - px2) + (py1 - py2)*(py1 - py2));
    dline = gwy_data_field_get_profile(args->dfield, NULL,
                                       px1, py1, px2, py2, res, 1,
                                       GWY_INTERPOLATION_BILINEAR);

    val = gwy_data_line_get_max(dline);
    g_object_unref(dline);
    return val;
}
static gdouble
eval_line_avg(ErunArgs *args, gpointer a)
{
    GwySearchLine *sline;
    GwyDataLine *dline;
    gdouble val;
    gint res, px1, py1, px2, py2;

    sline = GWY_SEARCH_LINE(a);

    px1 = gwy_data_field_rtoi(args->dfield, sline->xstart);
    px2 = gwy_data_field_rtoi(args->dfield, sline->xend);
    py1 = gwy_data_field_rtoj(args->dfield, sline->ystart);
    py2 = gwy_data_field_rtoj(args->dfield, sline->yend);

    res = sqrt((px1 - px2)*(px1 - px2) + (py1 - py2)*(py1 - py2));
    dline = gwy_data_field_get_profile(args->dfield, NULL,
                                       px1, py1, px2, py2, res, 1,
                                       GWY_INTERPOLATION_BILINEAR);

    val = gwy_data_line_get_avg(dline);
    g_object_unref(dline);
    return val;
}
static gdouble
eval_line_rms(ErunArgs *args, gpointer a)
{
    GwySearchLine *sline;
    GwyDataLine *dline;
    gdouble val;
    gint res, px1, py1, px2, py2;

    sline = GWY_SEARCH_LINE(a);

    px1 = gwy_data_field_rtoi(args->dfield, sline->xstart);
    px2 = gwy_data_field_rtoi(args->dfield, sline->xend);
    py1 = gwy_data_field_rtoj(args->dfield, sline->ystart);
    py2 = gwy_data_field_rtoj(args->dfield, sline->yend);

    res = sqrt((px1 - px2)*(px1 - px2) + (py1 - py2)*(py1 - py2));
    dline = gwy_data_field_get_profile(args->dfield, NULL,
                                       px1, py1, px2, py2, res, 1,
                                       GWY_INTERPOLATION_BILINEAR);

    val = gwy_data_line_get_rms(dline);
    g_object_unref(dline);
    return val;
}

static gdouble
eval_point_value(ErunArgs *args, gpointer a)
{
    GwySearchPoint *spoint;

    spoint = GWY_SEARCH_POINT(a);
    return gwy_data_field_get_dval_real(args->dfield, spoint->xc, spoint->yc,
                                        GWY_INTERPOLATION_BILINEAR);
}

static gdouble
eval_correlation_score(ErunArgs *args, gpointer a)
{
    GwyCorrelationPoint *spoint;

    spoint = GWY_CORRELATION_POINT(a);
    return spoint->score;
}

static gdouble
eval_point_avg(ErunArgs *args, gpointer a, gint size)
{
    GwySearchPoint *spoint;

    spoint = GWY_SEARCH_POINT(a);

    return gwy_data_field_area_get_avg(args->dfield, NULL,
                                       gwy_data_field_rtoi(args->dfield, spoint->xc) - size/2,
                                       gwy_data_field_rtoj(args->dfield, spoint->xc) - size/2,
                                       size, size);
}

static gdouble
eval_point_neural(ErunArgs *args, gpointer a)
{
    GwySearchPoint *spoint;

    spoint = GWY_SEARCH_POINT(a);
    return 0.5;
}
static gdouble
eval_intersection_x(ErunArgs *args, gpointer a, gpointer b)
{
    GwySearchLine *sline1, *sline2;

    sline1 = GWY_SEARCH_LINE(a);
    sline2 = GWY_SEARCH_LINE(b);

    return 0;
}
static gdouble
eval_intersection_y(ErunArgs *args, gpointer a, gpointer b)
{
    GwySearchLine *sline1, *sline2;

    sline1 = GWY_SEARCH_LINE(a);
    sline2 = GWY_SEARCH_LINE(b);

    return 0;
}
static gdouble
eval_angle(ErunArgs *args, gpointer a, gpointer b)
{
    GwySearchLine *sline1, *sline2;

    sline1 = GWY_SEARCH_LINE(a);
    sline2 = GWY_SEARCH_LINE(b);

    return 0;
}

/*FIXME we expect that there are no expressions within brackets, only variables*/
static gint
twoexpr_parse(GString *expression, gchar *name,
              GString **arg1, GString **arg2, gint *len)
{
    char *pos, *opos, *epos, *cpos;

    pos = strstr(expression->str, name);
    if (pos == NULL) return -1;

    opos = strstr(pos, "(");
    if (opos == NULL) return -1;

    cpos = strstr(opos, ",");
    if (cpos == NULL) return -1;

    epos = strstr(cpos, ")");
    if (epos == NULL) return -1;

    *len = epos - pos + 1;

    *arg1 = g_string_new_len(opos + 1, cpos - opos - 1);
    *arg2 = g_string_new_len(cpos +1, epos - cpos - 1);
    return (gint)(pos - expression->str);
}

static gint
oneexpr_parse(GString *expression, gchar *name,
              GString **arg1, gint *len)
{
    char *pos, *opos, *epos;

    pos = strstr(expression->str, name);
    if (pos == NULL) return -1;

    opos = strstr(pos, "(");
    if (opos == NULL) return -1;

    epos = strstr(opos, ")");
    if (epos == NULL) return -1;

    *len = epos - pos + 1;

    *arg1 = g_string_new_len(opos + 1, epos - opos - 1);
    return (gint)(pos - expression->str);
}

static gdouble
variable_parse(ErunArgs *args, gchar *variable, gboolean *err)
{
    GwySearchPoint *spset;
    GwySearchLine *slset;
    GwyFixedPoint *fpset;
    GwyFixedLine *flset;
    GwyCorrelationPoint *cpset;

    char *pos;
    char *numstr;
    int num;

    *err = FALSE;
    gwy_debug("parsing variable: %s\n", variable);
    if ((pos = strstr(variable, "dp"))!=NULL) {
       numstr = g_strndup(pos+2,  strstr(pos + 2, ".") - (pos + 2));
       num = atoi(numstr) - 1;
       if (num < args->evaluator->detected_point_array->len) {
            spset = g_ptr_array_index(args->evaluator->detected_point_array, num);
            pos = strstr(pos + 2, ".");
            if (strstr(pos + 1, "x")) {gwy_debug("dp.x component\n"); return spset->xc;}
            if (strstr(pos + 1, "y")) {gwy_debug("dp.y component\n"); return spset->yc;}
       }
       *err = TRUE;
       return 0;
    }
    if ((pos = strstr(variable, "fp"))!=NULL) {
       numstr = g_strndup(pos+2, strstr(pos + 2, ".") - (pos + 2));
       num = atoi(numstr) - 1;
       if (num < args->evaluator->fixed_point_array->len) {
            fpset = g_ptr_array_index(args->evaluator->fixed_point_array, num);
            pos = strstr(pos + 2, ".");
            if (strstr(pos + 1, "x")) {gwy_debug("fp.x component\n"); return fpset->xc;}
            if (strstr(pos + 1, "y")) {gwy_debug("fp.y component\n"); return fpset->yc;}
       }
       *err = TRUE;
       return 0;
    }
    if ((pos = strstr(variable, "cp"))!=NULL) {
       numstr = g_strndup(pos+2, strstr(pos + 2, ".") - (pos + 2));
       num = atoi(numstr) - 1;
       if (num < args->evaluator->correlation_point_array->len) {
            cpset = g_ptr_array_index(args->evaluator->correlation_point_array, num);
            pos = strstr(pos + 2, ".");
            if (strstr(pos + 1, "x")) {gwy_debug("cp.x component\n"); return cpset->xc;}
            if (strstr(pos + 1, "y")) {gwy_debug("cp.y component\n"); return cpset->yc;}
       }
       *err = TRUE;
       return 0;
    }
    if ((pos = strstr(variable, "dl"))!=NULL) {
       numstr = g_strndup(pos+2, strstr(pos + 2, ".") - (pos + 2));
       num = atoi(numstr) - 1;
       if (num < args->evaluator->detected_line_array->len) {
            slset = g_ptr_array_index(args->evaluator->detected_line_array, num);
            pos = strstr(pos + 2, ".");
            if (strstr(pos + 1, "x1")) {gwy_debug("dl.x1 component\n"); return slset->xstart;}
            if (strstr(pos + 1, "y1")) {gwy_debug("dl.y1 component\n"); return slset->ystart;}
            if (strstr(pos + 1, "x2")) {gwy_debug("dl.x2 component\n"); return slset->xend;}
            if (strstr(pos + 1, "y2")) {gwy_debug("dl.y2 component\n"); return slset->yend;}
       }
       *err = TRUE;
        return 0;
    }
    if ((pos = strstr(variable, "fl"))!=NULL) {
       numstr = g_strndup(pos+2, strstr(pos + 2, ".") - (pos + 2));
       num = atoi(numstr) - 1;
       if (num < args->evaluator->fixed_line_array->len) {
            flset = g_ptr_array_index(args->evaluator->fixed_line_array, num);
            pos = strstr(pos + 2, ".");
            if (strstr(pos + 1, "x1")) {gwy_debug("fl.x1 component\n"); return flset->xstart;}
            if (strstr(pos + 1, "y1")) {gwy_debug("fl.y1 component\n"); return flset->ystart;}
            if (strstr(pos + 1, "x2")) {gwy_debug("fl.x2 component\n"); return flset->xend;}
            if (strstr(pos + 1, "y2")) {gwy_debug("fl.y2 component\n"); return flset->yend;}
       }
       *err = TRUE;
        return 0;
    }
    *err = TRUE;
    return 0;
}

static gpointer
object_parse(ErunArgs *args, gchar *variable)
{
    char * pos;
    int num;

    gwy_debug("parsing variable: %s\n", variable);
    if ((pos = strstr(variable, "dp"))!=NULL) {
       num = atoi(pos + 2) - 1;
       if (num >= args->evaluator->detected_point_array->len) return NULL;
       return (gpointer)g_ptr_array_index(args->evaluator->detected_point_array, num);
    }
    if ((pos = strstr(variable, "fp"))!=NULL) {
       num = atoi(pos + 2) - 1;
       if (num >= args->evaluator->fixed_point_array->len) return NULL;
       return (gpointer)g_ptr_array_index(args->evaluator->fixed_point_array, num);
    }
    if ((pos = strstr(variable, "cp"))!=NULL) {
       num = atoi(pos + 2) - 1;
       if (num >= args->evaluator->correlation_point_array->len) return NULL;
       return (gpointer)g_ptr_array_index(args->evaluator->correlation_point_array, num);
    }
    if ((pos = strstr(variable, "dl"))!=NULL) {
       num = atoi(pos + 2) - 1;
       if (num >= args->evaluator->detected_line_array->len) return NULL;
       return (gpointer)g_ptr_array_index(args->evaluator->detected_line_array, num);
    }
    if ((pos = strstr(variable, "fl"))!=NULL) {
       num = atoi(pos + 2) - 1;
       if (num >= args->evaluator->fixed_line_array->len) return NULL;
       return (gpointer)g_ptr_array_index(args->evaluator->fixed_line_array, num);
    }
    return NULL;
}


static GString *
replace_function(GString *expression, gint pos, gint len, gdouble value)
{
    GString *valstr;
    valstr = g_string_new("");
    g_string_printf(valstr, "%g", value);

    gwy_debug("erasing %s from %d, %d characters (of total %d)\n", expression->str, pos, len, expression->len);
    expression = g_string_erase(expression, pos, len);
    gwy_debug("inserting value %s at position %d (from total length %d)\n", valstr->str, pos, expression->len);
    expression = g_string_insert(expression, pos, valstr->str);
    //g_free(valstr);
    return expression;
}

static GString *
preparse_expression(ErunArgs *args, GString *expression, GPtrArray *object_variables, gboolean *done)
{
    GString *arg1, *arg2;
    gint pos, len;
    gboolean err1, err2;
    gdouble value;
    gpointer object1, object2;
    gdouble var1, var2;

    if (strstr(expression->str, "PointAvg"))
    {
        pos = twoexpr_parse(expression, "PointAvg", &arg1, &arg2, &len);
        gwy_debug("function arguments: %s %s\n", arg1->str, arg2->str);
        g_ptr_array_add(object_variables, arg1);
        g_ptr_array_add(object_variables, arg2);

        object1 = object_parse(args, arg1->str);
        var1 = variable_parse(args, arg2->str, &err2);
        if (err1 || !object1) return NULL;
        value = eval_point_avg(args, object1, var1);
        expression = replace_function(expression, pos, len, value);
    }
    else if (strstr(expression->str, "IntersectX"))
    {
        pos = twoexpr_parse(expression, "IntersectX", &arg1, &arg2, &len);
        gwy_debug("function arguments: %s %s\n", arg1->str, arg2->str);
        g_ptr_array_add(object_variables, arg1);
        g_ptr_array_add(object_variables, arg2);


        object1 = object_parse(args, arg1->str);
        object2 = object_parse(args, arg2->str);
        if (!object1 || !object2) return NULL;
        value = eval_intersection_x(args, object1, object2);
        expression = replace_function(expression, pos, len, value);
    }
    else if (strstr(expression->str, "IntersectY"))
    {
        pos = twoexpr_parse(expression, "IntersectY", &arg1, &arg2, &len);
        gwy_debug("function arguments: %s %s\n", arg1->str, arg2->str);
        g_ptr_array_add(object_variables, arg1);
        g_ptr_array_add(object_variables, arg2);


        object1 = object_parse(args, arg1->str);
        object2 = object_parse(args, arg2->str);
        if (!object1 || !object2) return NULL;
        value = eval_intersection_y(args, object1, object2);
        expression = replace_function(expression, pos, len, value);
    }
    else if (strstr(expression->str, "Angle"))
    {
        pos = twoexpr_parse(expression, "Angle", &arg1, &arg2, &len);
        gwy_debug("function arguments: %s %s\n", arg1->str, arg2->str);
        g_ptr_array_add(object_variables, arg1);
        g_ptr_array_add(object_variables, arg2);


        object1 = object_parse(args, arg1->str);
        object2 = object_parse(args, arg2->str);
        if (!object1 || !object2) return NULL;
        value = eval_angle(args, object1, object2);
        expression = replace_function(expression, pos, len, value);
    }
    else if (strstr(expression->str, "LineMin"))
    {
        pos = oneexpr_parse(expression, "LineMin", &arg1, &len);
        gwy_debug("function argument: %s\n", arg1->str);
        g_ptr_array_add(object_variables, arg1);


        object1 = object_parse(args, arg1->str);
        if (!object1) return NULL;
        value = eval_line_min(args, object1);
        expression = replace_function(expression, pos, len, value);
    }
    else if (strstr(expression->str, "LineMax"))
    {
        pos = oneexpr_parse(expression, "LineMax", &arg1, &len);
        gwy_debug("function argument: %s\n", arg1->str);
        g_ptr_array_add(object_variables, arg1);


        object1 = object_parse(args, arg1->str);
        if (!object1) return NULL;
        value = eval_line_max(args, object1);
        expression = replace_function(expression, pos, len, value);
    }
    else if (strstr(expression->str, "LineAvg"))
    {
        pos = oneexpr_parse(expression, "LineAvg", &arg1, &len);
        gwy_debug("function argument: %s\n", arg1->str);
        g_ptr_array_add(object_variables, arg1);


        object1 = object_parse(args, arg1->str);
        if (!object1) return NULL;
        value = eval_line_avg(args, object1);
        expression = replace_function(expression, pos, len, value);
    }
    else if (strstr(expression->str, "LineRMS"))
    {
        pos = oneexpr_parse(expression, "LineRMS", &arg1, &len);
        gwy_debug("function argument: %s\n", arg1->str);
        g_ptr_array_add(object_variables, arg1);


        object1 = object_parse(args, arg1->str);
        if (!object1) return NULL;
        value = eval_line_rms(args, object1);
        expression = replace_function(expression, pos, len, value);
    }
    else if (strstr(expression->str, "PointValue"))
    {
        pos = oneexpr_parse(expression, "PointValue", &arg1, &len);
        gwy_debug("function argument: %s\n", arg1->str);
        g_ptr_array_add(object_variables, arg1);


        object1 = object_parse(args, arg1->str);
        if (!object1) return NULL;
        value = eval_point_value(args, object1);
        expression = replace_function(expression, pos, len, value);
    }
    else if (strstr(expression->str, "CorrelationScore"))
    {
        pos = oneexpr_parse(expression, "CorrelationScore", &arg1, &len);
        gwy_debug("function argument: %s\n", arg1->str);
        g_ptr_array_add(object_variables, arg1);


        object1 = object_parse(args, arg1->str);
        if (!object1) return NULL;
        value = eval_correlation_score(args, object1);
        expression = replace_function(expression, pos, len, value);
    }
    else if (strstr(expression->str, "PointNeural"))
    {
        pos = oneexpr_parse(expression, "PointNeural", &arg1, &len);
        gwy_debug("function argument: %s\n", arg1->str);
        g_ptr_array_add(object_variables, arg1);


        object1 = object_parse(args, arg1->str);
        if (!object1) return NULL;
        value = eval_point_neural(args, object1);
        expression = replace_function(expression, pos, len, value);
    }
    else *done = TRUE;


    return expression;
}

static gboolean
get_bresult(gdouble result, gchar *threshop)
{
    gdouble num;
    gchar *pos;

    if ((pos = strstr(threshop, "<"))!=NULL) {
       num = atof(pos + 1);
       if (result < num) return TRUE;
       else return FALSE;
    }
    if ((pos = strstr(threshop, ">"))!=NULL) {
       num = atof(pos + 1);
       if (result > num) return TRUE;
       else return FALSE;
    }
    if ((pos = strstr(threshop, "="))!=NULL) {
       num = atof(pos + 1);
       if (result == num) return TRUE;
       else return FALSE;
    }
    if ((pos = strstr(threshop, ">="))!=NULL) {
       num = atof(pos + 2);
       if (result >= num) return TRUE;
       else return FALSE;
    }
    if ((pos = strstr(threshop, "<="))!=NULL) {
       num = atof(pos + 2);
       if (result <= num) return TRUE;
       else return FALSE;
    }
    if ((pos = strstr(threshop, "!="))!=NULL) {
       num = atof(pos + 2);
       if (result != num) return TRUE;
       else return FALSE;
    }

    return TRUE;
}


static void evaluate(ErunArgs *args)
{
    GwyExpr *expr;
    gint k, i;
    GString *expression;
    GwyEvaluatorTask *etset;
    EvalData *edata;
    GError *err;
    gboolean error;
    GPtrArray *objvar;
    gint nobjects;
    gchar **variable_ids;
    gboolean done;
    GString *object_id;

    for (k = 0; k<args->evaluator->expression_task_array->len; k++)
    {
        edata = g_new(EvalData, 1);
        etset = g_ptr_array_index(args->evaluator->expression_task_array, k);
        expression = g_string_new(etset->expression);
        objvar = g_ptr_array_new();
        
        done = FALSE;
        do
            expression = preparse_expression(args, expression, objvar, &done);
        while (!done);

        if (expression == NULL) {
            gwy_debug("preparse failed\n");
            return;
        }
        gwy_debug("after preparse: %s\n", expression->str);

        expr = gwy_expr_new();
        if (!gwy_expr_compile(expr, expression->str, &err)){
            g_warning("Error compiling expression: %s\n", err->message);
            edata->error = g_strdup("error compiling expression");
            g_ptr_array_add(args->evaldata, edata);
            return;
        }

        edata->nvariables = gwy_expr_get_variables(expr, &edata->variable_ids);
        gwy_debug("%d variables in expression %s\n", edata->nvariables, expression->str);
        edata->variables = g_malloc(edata->nvariables*sizeof(gdouble));

        for (i = 1; i<edata->nvariables; i++)
        {
            gwy_debug("variable: %s\n", edata->variable_ids[i]);
            edata->variables[i] = variable_parse(args, edata->variable_ids[i], &error);
            if (error) {
                edata->error = g_strdup("error resolving variable");
                gwy_debug("cannot resolve variable\n");}
        }
        edata->result = gwy_expr_execute(expr, edata->variables);
        gwy_debug("Execution result: %g\n", edata->result);
        edata->error = g_strdup("none");

        edata->bresult = get_bresult(edata->result, etset->threshold);
       
        /*add object variables to the fields*/
        nobjects = objvar->len;
        
        variable_ids = g_new(gchar *, edata->nvariables + nobjects);
        for (i = 1; i<edata->nvariables; i++)
        {
            variable_ids[i] = g_strdup(edata->variable_ids[i]);
            g_free(edata->variable_ids[i]);
        }
        for (i = edata->nvariables; i<(edata->nvariables + nobjects); i++)
        {
            object_id = (GString *)(g_ptr_array_index(objvar, i - edata->nvariables));
            variable_ids[i] = g_strdup(object_id->str);
        }
         
        edata->nvariables += nobjects;
        g_free(edata->variable_ids);
        edata->variable_ids = variable_ids;
        
        g_ptr_array_add(args->evaldata, edata);
        g_ptr_array_free(objvar, TRUE);
    }


}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
