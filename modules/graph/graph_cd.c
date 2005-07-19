/*
 *  @(#) $Id$
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
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <libprocess/cdline.h>
#include <app/settings.h>
#include <app/app.h>

#define MAX_PARAMS 5

/* Data for this function.*/

typedef struct {
    GtkWidget *graph;
    GtkWidget *from;
    GtkWidget *to;
    GtkObject *data;
    GtkWidget *selector;
    GtkWidget **param_des;
    GtkWidget **param_fit;
    GtkWidget **param_init;
    GtkWidget **param_res;
    GtkWidget **param_err;
    GtkWidget *criterium;
    GtkWidget *image;
} FitControls;

typedef struct {
    gint function_type;
    gint curve;
    gdouble from;
    gdouble to;
    gboolean par_fix[MAX_PARAMS];
    gdouble par_init[MAX_PARAMS];
    gdouble par_res[MAX_PARAMS];
    gdouble err[MAX_PARAMS];
    gdouble crit;
    const GwyCDLinePreset *fitfunc;
    GwyGraph *parent_graph;
    gint parent_nofcurves;
    GwyNLFitter *fitter;
    gboolean is_fitted;
    GwyGraphModel *graph_model;
} FitArgs;


static gboolean    module_register           (const gchar *name);
static gboolean    fit                       (GwyGraph *graph);
static gboolean    fit_dialog                (FitArgs *args);
static void        recompute                 (FitArgs *args,
                                              FitControls *controls);
static void        reset                     (FitArgs *args,
                                              FitControls *controls);
static void        plot_inits                (FitArgs *args,
                                              FitControls *controls);
static void        type_changed_cb           (GObject *item,
                                              FitArgs *args);
static void        from_changed_cb           (GtkWidget *entry,
                                              FitArgs *args);
static void        to_changed_cb             (GtkWidget *entry,
                                              FitArgs *args);
static void        dialog_update             (FitControls *controls,
                                              FitArgs *args);
static void        graph_update              (FitControls *controls,
                                              FitArgs *args);
static void        graph_selected            (GwyGraph *graph,
                                              FitArgs *args);
static gint        normalize_data            (FitArgs *args,
                                              GwyDataLine *xdata,
                                              GwyDataLine *ydata,
                                              gint curve);
static GtkWidget*  create_preset_menu        (GCallback callback,
                                              gpointer cbdata,
                                              gint current);
static void        load_args                 (GwyContainer *container,
                                              FitArgs *args);
static void        save_args                 (GwyContainer *container,
                                              FitArgs *args);
static void        create_results_window     (FitArgs *args);
static GString*    create_fit_report         (FitArgs *args);
static void        destroy                   (FitArgs *args,
                                              FitControls *controls);


FitControls *pcontrols;


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Critical dimension measurements"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyGraphFuncInfo fit_func_info = {
        "graph_cd",
        N_("/_Critical dimension"),
        (GwyGraphFunc)&fit,
    };

    gwy_graph_func_register(name, &fit_func_info);

    return TRUE;
}

static gboolean
fit(GwyGraph *graph)
{
    GwyContainer *settings;
    gboolean ok;
    gint i;
    FitArgs args;

    args.fitfunc = NULL;
    args.function_type = 0;
    args.from = 0;
    args.to = 0;
    args.parent_graph = graph;
    for (i = 0; i < MAX_PARAMS; i++)
        args.par_fix[i] = FALSE;
    args.curve = 1;
    args.fitter = NULL;
    args.is_fitted = FALSE;

    settings = gwy_app_settings_get();
    load_args(settings, &args);

    ok = fit_dialog(&args);
    save_args(settings, &args);

    return ok;
}

/*extract relevant part of data and normalize it to be fitable*/
static gint
normalize_data(FitArgs *args, GwyDataLine *xdata, GwyDataLine *ydata, gint curve)
{
    gint i, j, ns;
    GwyGraphCurveModel *cmodel;
    gdouble *xs, *ys; 
   
    if (curve >= gwy_graph_model_get_n_curves(args->graph_model))
                                  return 0;
    cmodel = gwy_graph_model_get_curve_by_index(args->graph_model, curve);
    xs = gwy_graph_curve_model_get_xdata(cmodel);
    ys = gwy_graph_curve_model_get_ydata(cmodel);
    ns = gwy_graph_curve_model_get_ndata(cmodel);
    
    gwy_data_line_resample(xdata, ns,
                           GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(ydata, ns,
                           GWY_INTERPOLATION_NONE);


    j = 0;
    for (i = 0; i < xdata->res; i++)
    {
        if ((xs[i] >= args->from
             && xs[i] <= args->to)
              || (args->from == args->to))
        {
            xdata->data[j] = xs[i];
            ydata->data[j] = ys[i];                                                                         
            j++;
        }
    }
    if (j == 0)
        return 0;

    if (j < xdata->res)
    {
        gwy_data_line_resize(xdata, 0, j);
        gwy_data_line_resize(ydata, 0, j);
    }

    return j;
}



static gboolean
fit_dialog(FitArgs *args)
{
    GtkWidget *label;
    GtkWidget *table;
    GtkWidget *dialog;
    GtkWidget *hbox;
    GtkWidget *hbox2;
    GtkWidget *table2;
    GtkWidget *vbox;
    FitControls controls;
    gint response, i;
    char *p, *filename;

    enum {
        RESPONSE_RESET = 1,
        RESPONSE_FIT = 2,
        RESPONSE_PLOT = 3
    };

    pcontrols = &controls;
    dialog = gtk_dialog_new_with_buttons(_("Fit graph"), NULL, 0, NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Fit"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_FIT);
/*    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          _("_Reset inits"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          _("_Plot inits"), RESPONSE_PLOT);
                          */
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

    /*fit equation*/
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Function definition:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    controls.selector = create_preset_menu(G_CALLBACK(type_changed_cb),
                                           args, args->function_type);
    gtk_container_add(GTK_CONTAINER(vbox), controls.selector);

    p = gwy_find_self_dir("pixmaps");
    args->fitfunc = gwy_cdline_get_preset(args->function_type);
    filename = g_build_filename(p, gwy_cdline_get_preset_formula(args->fitfunc),
                                NULL);
    g_free(p);

    controls.image = gtk_image_new_from_file(filename);
    gtk_container_add(GTK_CONTAINER(vbox), controls.image);
    g_free(filename);

    /*fit parameters*/
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), N_("<b>Fitting parameters</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    table = gtk_table_new(MAX_PARAMS, 4, FALSE);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), " ");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    /*label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Initial</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Result</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Error</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    /*label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Fix</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    */

    controls.param_des = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_des[i] = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(controls.param_des[i]), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), controls.param_des[i],
                         0, 1, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    /*controls.param_init = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++)
    {
        controls.param_init[i] = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(controls.param_init[i]), 12);
        gtk_entry_set_width_chars(GTK_ENTRY(controls.param_init[i]), 12);
        g_signal_connect(controls.param_init[i], "changed",
                         G_CALLBACK(double_entry_changed_cb),
                         &args->par_init[i]);
        gtk_table_attach(GTK_TABLE(table), controls.param_init[i],
                         1, 2, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }*/

    controls.param_res = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++)
    {
        controls.param_res[i] = gtk_label_new(NULL);
        gtk_table_attach(GTK_TABLE(table), controls.param_res[i],
                         1, 2, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    }

    controls.param_err = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++)
    {
        controls.param_err[i] = gtk_label_new(NULL);
        gtk_table_attach(GTK_TABLE(table), controls.param_err[i],
                         2, 3, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    }

    /*
    controls.param_fit = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++)
    {
        controls.param_fit[i] = gtk_check_button_new();
        g_signal_connect(controls.param_fit[i], "toggled",
                         G_CALLBACK(toggle_changed_cb), &args->par_fix[i]);
        gtk_table_attach(GTK_TABLE(table), controls.param_fit[i],
                         4, 5, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }*/

    gtk_container_add(GTK_CONTAINER(vbox), table);


    /*FIt area*/
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Fit area</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    table2 = gtk_table_new(2, 2, FALSE);
    controls.data
        = gtk_adjustment_new(args->curve, 1,
                             gwy_graph_model_get_n_curves(GWY_GRAPH(args->parent_graph)->graph_model),
                             1, 5, 0);
    gwy_table_attach_spinbutton(table2, 1, _("Graph data curve"), "",
                                controls.data);
    gtk_container_add(GTK_CONTAINER(vbox), table2);


    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_box_set_spacing(GTK_BOX(hbox2), 4);

    label = gtk_label_new("From");
    gtk_container_add(GTK_CONTAINER(hbox2), label);

    controls.from = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(controls.from), 12);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.from), 12);
    gtk_container_add(GTK_CONTAINER(hbox2), controls.from);
    g_signal_connect(controls.from, "changed",
                      G_CALLBACK(from_changed_cb), args);


    label = gtk_label_new("to");
    gtk_container_add(GTK_CONTAINER(hbox2), label);

    controls.to = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(controls.to), 12);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.to), 12);
    gtk_container_add(GTK_CONTAINER(hbox2), controls.to);
    g_signal_connect(controls.to, "changed",
                      G_CALLBACK(to_changed_cb), args);

    gtk_container_add(GTK_CONTAINER(vbox), hbox2);


    args->graph_model = gwy_graph_model_new();
    controls.graph = gwy_graph_new(args->graph_model);
    gwy_graph_enable_user_input(GWY_GRAPH(controls.graph), FALSE);
    gwy_graph_set_selection_limit(GWY_GRAPH(controls.graph), 1);
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, FALSE, FALSE, 0);
    gtk_widget_set_size_request(controls.graph, 400, 300);
    gwy_graph_set_status(GWY_GRAPH(controls.graph), GWY_GRAPH_STATUS_XSEL);
    g_signal_connect(controls.graph, "selected",
                         G_CALLBACK(graph_selected), args);
                            
    
    args->fitfunc = gwy_cdline_get_preset(args->function_type);

    reset(args, &controls);
    dialog_update(&controls, args);
    graph_update(&controls, args);
    graph_selected(GWY_GRAPH(controls.graph), args);

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            destroy(args, &controls);
            gtk_widget_destroy(dialog);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            if (args->is_fitted)
                create_results_window(args);
            gtk_widget_destroy(dialog);
            break;

            case RESPONSE_RESET:
            reset(args, &controls);
            break;

            case RESPONSE_PLOT:
            plot_inits(args, &controls);
            break;

            case RESPONSE_FIT:
            recompute(args, &controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);


    return TRUE;
}

static void
destroy(G_GNUC_UNUSED FitArgs *args, FitControls *controls)
{
    /*g_free(controls->param_init);*/
    g_free(controls->param_res);
    /*g_free(controls->param_fit);*/
    g_free(controls->param_err);
}

static void
clear(G_GNUC_UNUSED FitArgs *args, FitControls *controls)
{
    gint i;

    graph_update(controls, args);

    for (i = 0; i < MAX_PARAMS; i++) {
        gtk_label_set_markup(GTK_LABEL(controls->param_res[i]), " ");
        gtk_label_set_markup(GTK_LABEL(controls->param_err[i]), " ");
    }
}

static void
plot_inits(FitArgs *args, FitControls *controls)
{
    GwyDataLine *xdata;
    GwyDataLine *ydata;
    const GwyCDLinePreset *function;
    gboolean ok;
    gint i;
    GwyGraphCurveModel *cmodel;
    
    xdata = gwy_data_line_new(10, 10, FALSE);
    ydata = gwy_data_line_new(10, 10, FALSE);


    args->curve = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->data));
    if (!normalize_data(args, xdata, ydata, args->curve - 1))
    {
        g_object_unref(xdata);
        g_object_unref(ydata);
        return;
    }

    function = gwy_cdline_get_preset(args->function_type);

    for (i = 0; i < xdata->res; i++)
        ydata->data[i] = function->function(xdata->data[i], function->nparams,
                                            args->par_init, NULL, &ok);

    graph_update(controls, args);

    cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_curve_type(cmodel, GWY_GRAPH_CURVE_LINE);
    gwy_graph_curve_model_set_description(cmodel, "fit");
    gwy_graph_curve_model_set_data(cmodel,
                                   xdata->data,
                                   ydata->data,
                                   xdata->res);
    gwy_graph_model_add_curve(args->graph_model, cmodel);
                    
    g_object_unref(xdata);
    g_object_unref(ydata);

}

 /*recompute fit and update everything*/
static void
recompute(FitArgs *args, FitControls *controls)
{
    GwyDataLine *xdata;
    GwyDataLine *ydata;
    const GwyCDLinePreset *function;
    gboolean fixed[MAX_PARAMS];
    gchar buffer[64];
    gboolean ok;
    gint i, nparams;
    GwyGraphCurveModel *cmodel;
    
    xdata = gwy_data_line_new(10, 10, FALSE);
    ydata = gwy_data_line_new(10, 10, FALSE);


    args->curve = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->data));
    if (!normalize_data(args, xdata, ydata, args->curve - 1))
    {
        g_object_unref(xdata);
        g_object_unref(ydata);
        return;
    }

    function = gwy_cdline_get_preset(args->function_type);
    nparams = gwy_cdline_get_preset_nparams(args->fitfunc);

    for (i=0; i<MAX_PARAMS; i++)
    {
        fixed[i] = args->par_fix[i];
        args->par_res[i] = args->par_init[i];
    }

    gwy_cdline_fit_preset(function,
                                  xdata->res, xdata->data, ydata->data,
                                  function->nparams,
                                  args->par_res, args->err, fixed, NULL);

    for (i = 0; i < nparams; i++) {
        g_snprintf(buffer, sizeof(buffer), "%3.4g", args->par_res[i]);
        gtk_label_set_markup(GTK_LABEL(controls->param_res[i]), buffer);
    }
    for (i = 0; i < nparams; i++) {
        if (args->err[i] == -1)
            g_snprintf(buffer, sizeof(buffer), "-");
        else
            g_snprintf(buffer, sizeof(buffer), "%3.4g", args->err[i]);
        gtk_label_set_markup(GTK_LABEL(controls->param_err[i]), buffer);
    }



    for (i = 0; i < xdata->res; i++)
        ydata->data[i] = function->function(xdata->data[i], function->nparams,
                                            args->par_res, NULL, &ok);

    graph_update(controls, args);

    cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_curve_type(cmodel, GWY_GRAPH_CURVE_LINE);
    gwy_graph_curve_model_set_data(cmodel,
                                   xdata->data,
                                   ydata->data,
                                   xdata->res);
    gwy_graph_curve_model_set_description(cmodel, "fit");
    gwy_graph_model_add_curve(args->graph_model, cmodel);
                    
    args->is_fitted = TRUE;
    g_object_unref(xdata);
    g_object_unref(ydata);
}

/*get default parameters (guessed)*/
static void
reset(FitArgs *args, FitControls *controls)
{
    dialog_update(controls, args);
}


static void
type_changed_cb(GObject *item, FitArgs *args)
{
    char *p, *filename;

    args->function_type =
        GPOINTER_TO_INT(g_object_get_data(item, "cdline-preset"));

    args->fitfunc = gwy_cdline_get_preset(args->function_type);

    p = gwy_find_self_dir("pixmaps");
    filename = g_build_filename(p, gwy_cdline_get_preset_formula(args->fitfunc), NULL);
    g_free(p);

    gtk_image_set_from_file(GTK_IMAGE(pcontrols->image), filename);

    dialog_update(pcontrols, args);

    g_free(filename);
}

static void
dialog_update(FitControls *controls, FitArgs *args)
{
    gint i;

    clear(args, controls);

    /*TODO change chema image*/


    for (i = 0; i < MAX_PARAMS; i++) {
        if (i < gwy_cdline_get_preset_nparams(args->fitfunc)) {
            gtk_widget_set_sensitive(controls->param_des[i], TRUE);
            gtk_label_set_markup(GTK_LABEL(controls->param_des[i]),
                      gwy_cdline_get_preset_param_name(args->fitfunc, i));

        /*    gtk_widget_set_sensitive(controls->param_init[i], TRUE);
            gtk_widget_set_sensitive(controls->param_fit[i], TRUE);
            g_snprintf(buffer, sizeof(buffer), "%.3g", args->par_init[i]);
            gtk_entry_set_text(GTK_ENTRY(controls->param_init[i]), buffer);
            */
        }
        else {
            gtk_label_set_markup(GTK_LABEL(controls->param_des[i]), " ");
            gtk_widget_set_sensitive(controls->param_des[i], FALSE);
            /*gtk_widget_set_sensitive(controls->param_init[i], FALSE);
            gtk_widget_set_sensitive(controls->param_fit[i], FALSE);*/
            /*gtk_entry_set_text(GTK_ENTRY(controls->param_init[i]), " ");*/
        }
    }

}


static void
graph_update(FitControls *controls, FitArgs *args)
{
    gint i;

    /*clear graph*/
    gwy_graph_model_remove_all_curves(args->graph_model);
                                                                                                                                           
    for (i=0; i<gwy_graph_model_get_n_curves(GWY_GRAPH(args->parent_graph)->graph_model); i++)
                  gwy_graph_model_add_curve(args->graph_model,
                                       gwy_graph_model_get_curve_by_index(
                                             GWY_GRAPH(args->parent_graph)->graph_model, i));

    args->is_fitted = FALSE;

}

static void
graph_selected(GwyGraph *graph, FitArgs *args)
{
    gchar buffer[24];
    gdouble xmin, xmax, ymin, ymax;
    gdouble selection[2];

    gwy_graph_get_selection(graph, selection);
    
    if (gwy_graph_get_selection_number(graph) <= 0 || selection[0] == selection[1]) {

        xmin = args->graph_model->x_min;
        ymin = args->graph_model->y_min;
        xmax = args->graph_model->x_max;
        ymax = args->graph_model->y_max;                        
        
        args->from = xmin;
        args->to = xmax;
        g_snprintf(buffer, sizeof(buffer), "%.3g", args->from);
        gtk_entry_set_text(GTK_ENTRY(pcontrols->from), buffer);
        g_snprintf(buffer, sizeof(buffer), "%.3g", args->to);
        gtk_entry_set_text(GTK_ENTRY(pcontrols->to), buffer);
    }
    else {
        args->from = selection[0];
        args->to = selection[1];
        if (args->from > args->to) GWY_SWAP(gdouble, args->from, args->to);
        g_snprintf(buffer, sizeof(buffer), "%.3g", args->from);
        gtk_entry_set_text(GTK_ENTRY(pcontrols->from), buffer);
        g_snprintf(buffer, sizeof(buffer), "%.3g", args->to);
        gtk_entry_set_text(GTK_ENTRY(pcontrols->to), buffer);
    }
    dialog_update(pcontrols, args);
}

static void
from_changed_cb(GtkWidget *entry, FitArgs *args)
{
    args->from = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
    dialog_update(pcontrols, args);
}

static void
to_changed_cb(GtkWidget *entry, FitArgs *args)
{
    args->to = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
    dialog_update(pcontrols, args);
}

static GtkWidget*
create_preset_menu(GCallback callback,
                   gpointer cbdata,
                   gint current)
{
    static GwyEnum *entries = NULL;
    static gint nentries = 0;

    if (!entries) {
        const GwyCDLinePreset *func;
        gint i;

        nentries = gwy_cdline_get_npresets();
        entries = g_new(GwyEnum, nentries);
        for (i = 0; i < nentries; i++) {
            entries[i].value = i;
            func = gwy_cdline_get_preset(i);
            entries[i].name = gwy_cdline_get_preset_name(func);
        }
    }

    return gwy_option_menu_create(entries, nentries,
                                  "cdline-preset", callback, cbdata,
                                  current);
}

static const gchar *preset_key = "/module/graph_cd/preset";

static void
load_args(GwyContainer *container,
          FitArgs *args)
{
    const GwyCDLinePreset *func;
    static const guchar *preset;

    if (gwy_container_gis_string_by_name(container, preset_key, &preset)) {
        func = gwy_cdline_get_preset_by_name((const gchar*)preset);
        args->function_type = gwy_cdline_get_preset_id(func);
    }
}

static void
save_args(GwyContainer *container,
          FitArgs *args)
{
    const GwyCDLinePreset *func;

    func = gwy_cdline_get_preset(args->function_type);
    gwy_container_set_string_by_name
        (container, preset_key,
         g_strdup(gwy_cdline_get_preset_name(func)));
}


/************************* fit report *****************************/
static void
attach_label(GtkWidget *table, const gchar *text,
             gint row, gint col, gdouble halign)
{
    GtkWidget *label;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_misc_set_alignment(GTK_MISC(label), halign, 0.5);

    gtk_table_attach(GTK_TABLE(table), label,
                     col, col+1, row, row+1, GTK_FILL, 0, 2, 2);
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
    dialog = gtk_message_dialog_new(NULL, 0,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    N_("Cannot save report to %s.\n%s\n"),
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
    dialog = gtk_file_selection_new(_("Save Fit Report"));
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog),
                                    gwy_app_get_current_directory());

    g_signal_connect(GTK_FILE_SELECTION(dialog)->ok_button, "clicked",
                     G_CALLBACK(save_report_cb), report);
    g_signal_connect_swapped(GTK_FILE_SELECTION(dialog)->cancel_button,
                             "clicked",
                             G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_widget_show_all(dialog);
}

static gchar*
format_magnitude(GString *str,
                 gdouble magnitude)
{
    if (magnitude)
        g_string_printf(str, "× 10<sup>%d</sup>",
                        (gint)floor(log10(magnitude) + 0.5));
    else
        g_string_assign(str, "");

    return str->str;
}

static gint
count_really_fitted_points(FitArgs *args)
{
    gint i, n, curve;
    GwyGraphCurveModel *cmodel;
    gdouble *xs, *ys;
    gint ns;

    curve = args->curve - 1;
    n = 0;
    cmodel = gwy_graph_model_get_curve_by_index(args->graph_model, curve);
    xs = gwy_graph_curve_model_get_xdata(cmodel);
    ys = gwy_graph_curve_model_get_ydata(cmodel);
    ns = gwy_graph_curve_model_get_ndata(cmodel);
                                                                                                                                                                 for (i = 0; i < ns; i++) {
        if ((xs[i] >= args->from
                  && xs[i] <= args->to)
                     || (args->from == args->to))
            n++;
    }
    return n;
}

static void
create_results_window(FitArgs *args)
{
    enum { RESPONSE_SAVE = 1 };
    GtkWidget *window, *tab, *table, *label;
    gdouble mag, value, sigma;
    gint row, curve, n, i;
    gint precision;
    gchar *p, *filename;
    GString *str, *su;
    GtkWidget *image;
    const gchar *s;

    g_return_if_fail(args->is_fitted);

    window = gtk_dialog_new_with_buttons(_("Fit results"), NULL, 0,
                                         GTK_STOCK_SAVE, RESPONSE_SAVE,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_CLOSE);

    table = gtk_table_new(9, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), table,
                       FALSE, FALSE, 0);
    row = 0;
    curve = args->curve - 1;

    attach_label(table, _("<b>Data:</b>"), row, 0, 0.0);
    str = g_string_new(gwy_graph_curve_model_get_description(
                    gwy_graph_model_get_curve_by_index(args->graph_model, curve)));
    
    attach_label(table, str->str, row, 1, 0.0);
    row++;

    str = g_string_new("");
    su = g_string_new("");
    attach_label(table, _("Num of points:"), row, 0, 0.0);
    g_string_printf(str, "%d of %d",
                    count_really_fitted_points(args),
                    gwy_graph_curve_model_get_ndata(
                     gwy_graph_model_get_curve_by_index(args->graph_model, curve)));
   
    attach_label(table, str->str, row, 1, 0.0);
    row++;

    attach_label(table, _("X range:"), row, 0, 0.0);
    mag = gwy_math_humanize_numbers((args->to - args->from)/120,
                                    MAX(fabs(args->from), fabs(args->to)),
                                    &precision);
    g_string_printf(str, "%.*f–%.*f %s",
                    precision, args->from/mag,
                    precision, args->to/mag,
                    format_magnitude(su, mag));
    attach_label(table, str->str, row, 1, 0.0);
    row++;

    attach_label(table, _("<b>Function:</b>"), row, 0, 0.0);
    row++;

    p = gwy_find_self_dir("pixmaps");
    args->fitfunc = gwy_cdline_get_preset(args->function_type);
    filename = g_build_filename(p, gwy_cdline_get_preset_formula(args->fitfunc), NULL);
    g_free(p);

    image = gtk_image_new_from_file(filename);
    gtk_table_attach(GTK_TABLE(table), image,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_free(filename);
    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label),
                         gwy_cdline_get_preset_formula(args->fitfunc));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    attach_label(table, _("<b>Results</b>"), row, 0, 0.0);
    row++;

    n = gwy_cdline_get_preset_nparams(args->fitfunc);
    tab = gtk_table_new(n, 6, FALSE);
    gtk_table_attach(GTK_TABLE(table), tab, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    for (i = 0; i < n; i++) {
        attach_label(tab, "=", i, 1, 0.5);
        attach_label(tab, "±", i, 3, 0.5);
        s = gwy_cdline_get_preset_param_name(args->fitfunc, i);
        attach_label(tab, s, i, 0, 0.0);
        value = args->par_res[i];
        sigma = args->err[i];
        if (sigma == -1)
        {
            g_string_printf(str, "%g", value);
            attach_label(tab, str->str, i, 2, 1.0);
            g_string_printf(str, "-");
            attach_label(tab, str->str, i, 4, 1.0);
        }
        else
        {
            mag = gwy_math_humanize_numbers(sigma/12, fabs(value), &precision);
            g_string_printf(str, "%.*f", precision, value/mag);
            attach_label(tab, str->str, i, 2, 1.0);
            g_string_printf(str, "%.*f", precision, sigma/mag);
            attach_label(tab, str->str, i, 4, 1.0);
            attach_label(tab, format_magnitude(su, mag), i, 5, 0.0);
        }
    }
    row++;

    g_string_free(str, TRUE);
    g_string_free(su, TRUE);
    str = create_fit_report(args);

    g_signal_connect(window, "response",
                     G_CALLBACK(results_window_response_cb), str);
    gtk_widget_show_all(window);
}

static GString*
create_fit_report(FitArgs *args)
{
    GString *report, *str;
    gchar *s, *s2;
    gint i, curve, n;

    report = g_string_new("");

    curve = args->curve - 1;
    g_string_append_printf(report, _("\n===== Fit Results =====\n"));

    str = g_string_new(gwy_graph_curve_model_get_description(
              gwy_graph_model_get_curve_by_index(args->graph_model, curve)));
    g_string_append_printf(report, _("Data: %s\n"), str->str);
    str = g_string_new("");
    g_string_append_printf(report, _("Number of points: %d of %d\n"),
                           count_really_fitted_points(args),
                           gwy_graph_curve_model_get_ndata(
                             gwy_graph_model_get_curve_by_index(args->graph_model, curve)));
    
    g_string_append_printf(report, _("X range:          %g to %g\n"),
                           args->from, args->to);
    g_string_append_printf(report, _("Fitted function:  %s\n"),
                           gwy_cdline_get_preset_name(args->fitfunc));
    g_string_append_printf(report, _("\nResults\n"));
    n = gwy_cdline_get_preset_nparams(args->fitfunc);
    for (i = 0; i < n; i++) {
        /* FIXME: how to do this better? use pango_parse_markup()? */
        s = gwy_strreplace(gwy_cdline_get_preset_param_name(args->fitfunc,
                                                                i),
                           "<sub>", "", (gsize)-1);
        s2 = gwy_strreplace(s, "</sub>", "", (gsize)-1);
        g_string_append_printf(report, "%s = %g ± %g\n",
                               s2, args->par_res[i], args->err[i]);
        g_free(s2);
        g_free(s);
    }

    g_string_free(str, TRUE);

    return report;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
