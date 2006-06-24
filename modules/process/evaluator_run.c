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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/correct.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <glib/gstdio.h>
#include "evaluator.h"
#include <errno.h>

#define EVALUATOR_RUN_RUN_MODES GWY_RUN_IMMEDIATE

typedef struct {
     GwyEvaluator *evaluator;
     GwyDataField *dfield;
     GPtrArray *evaluated_results;
     GArray *evaluated_statements;
} ErunArgs;



static gboolean            module_register(void);
static void                evaluator_run(GwyContainer *data,
                                         GwyRunType run);
static gchar*              get_filename();
static GwyEvaluator*       get_evaluator(gchar *filename);
static void                get_results(ErunArgs *args);
static void                save_report_cb(GtkWidget *button, GString *report);
static void                results_window_response_cb(GtkWidget *window,
                                                      gint response,
                                                      GString *report);
static void                create_results_window(ErunArgs *args);
static GString*            create_evaluator_report(ErunArgs *args);



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
    args.evaluated_results = g_ptr_array_new();
    args.evaluated_statements = g_array_new(FALSE, FALSE, sizeof(gboolean));
    get_results(&args);

    /*output result window*/

    create_results_window(&args);
    
}

static void
get_results(ErunArgs *args)
{
}

static gchar* 
get_filename()
{
    GtkDialog *filedialog;
    gchar *filename;
    GString *string = g_string_new("");

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

static GwyEvaluator* 
get_evaluator(gchar *filename)
{
    GwyEvaluator *evaluator;

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
    GtkWidget *window, *tab, *table;
    GString *str;
    gint row;

    window = gtk_dialog_new_with_buttons(_("Evaluator results"), NULL, 0,
                                         GTK_STOCK_SAVE, RESPONSE_SAVE,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_CLOSE);

    table = gtk_table_new(9, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), table,
                       FALSE, FALSE, 0);
    row = 0;


    str = create_evaluator_report(args);

    g_signal_connect(window, "response",
                     G_CALLBACK(results_window_response_cb), str);
    gtk_widget_show_all(window);
}


static GString*
create_evaluator_report(ErunArgs *args)
{
    GString *report;

    report = g_string_new("");

    g_string_append_printf(report, _("\n===== Fit Results =====\n"));
    return report;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
