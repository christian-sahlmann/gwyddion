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
#include <app/settings.h>
#include <app/file.h>
#include <app/app.h>

enum { MAX_PARAMS = 4 };

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
    GwyNLFitPreset *fitfunc;
    GwyGraph *parent_graph;
    GwyNLFitter *fitter;
    gboolean is_fitted;
    GwyGraphModel *graph_model;
} FitArgs;

typedef struct {
    FitArgs *args;
    GtkWidget *graph;
    GtkWidget *from;
    GtkWidget *to;
    GtkObject *data;
    GtkWidget *chisq;
    GtkWidget *selector;
    GtkWidget *equation;
    GtkWidget **covar;
    GtkWidget **param_des;
    GtkWidget **param_fit;
    GtkWidget **param_init;
    GtkWidget **param_res;
    GtkWidget **param_err;
    GtkWidget **param_copy;
    GtkWidget *criterium;
} FitControls;

static gboolean    module_register           (const gchar *name);
static gboolean    fit                       (GwyGraph *graph);
static gboolean    fit_dialog                (FitArgs *args);
static void        recompute                 (FitArgs *args,
                                              FitControls *controls);
static void        reset                     (FitArgs *args,
                                              FitControls *controls);
static void        plot_inits                (FitArgs *args,
                                              FitControls *controls);
static void        curve_changed_cb          (GtkAdjustment *adj,
                                              FitControls *controls);
static void        type_changed_cb           (GtkWidget *combo,
                                              FitControls *controls);
static void        from_changed_cb           (GtkWidget *entry,
                                              FitControls *controls);
static void        to_changed_cb             (GtkWidget *entry,
                                              FitControls *controls);
static void        double_entry_changed_cb   (GtkWidget *entry,
                                              gdouble *value);
static void        toggle_changed_cb         (GtkToggleButton *button,
                                              gboolean *value);
static void        copy_param_cb             (GObject *button,
                                              FitControls *controls);
static void        dialog_update             (FitControls *controls,
                                              FitArgs *args);
static void        guess                     (FitControls *controls,
                                              FitArgs *args);
static void        graph_update              (FitControls *controls,
                                              FitArgs *args);
static void        graph_selected            (GwyGraph *graph,
                                              FitControls *controls);
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

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Fit graph with function"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.3",
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
        "graph_fit",
        N_("/_Fit Graph"),
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
normalize_data(FitArgs *args,
               GwyDataLine *xdata,
               GwyDataLine *ydata,
               gint curve)
{
    gint i, j, ns;
    gboolean skip_first_point = FALSE;
    GwyGraphCurveModel *cmodel;
    const gdouble *xs, *ys;
    const gchar *func_name;

    if (curve >= gwy_graph_model_get_n_curves(args->graph_model))
        return 0;

    cmodel = gwy_graph_model_get_curve_by_index(args->graph_model, curve);
    xs = gwy_graph_curve_model_get_xdata(cmodel);
    ys = gwy_graph_curve_model_get_ydata(cmodel);
    ns = gwy_graph_curve_model_get_ndata(cmodel);

    gwy_data_line_resample(xdata, ns, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(ydata, ns, GWY_INTERPOLATION_NONE);

    func_name = gwy_resource_get_name(GWY_RESOURCE(args->fitfunc));
    if (gwy_strequal(func_name, "Gaussian (PSDF)")
        || gwy_strequal(func_name, "Power"))
        skip_first_point = TRUE;

    j = 0;
    for (i = 0; i < xdata->res; i++)
    {
        if ((xs[i] >= args->from
             && xs[i] <= args->to)
            || (args->from == args->to))
        {
            if (skip_first_point && i == 0)
                continue;

            xdata->data[j] = xs[i];
            ydata->data[j] = ys[i];
            j++;
        }
    }
    if (j == 0)
        return 0;


    if (j < xdata->res) {
        gwy_data_line_resize(xdata, 0, j);
        gwy_data_line_resize(ydata, 0, j);
    }

    return j;
}



static gboolean
fit_dialog(FitArgs *args)
{
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_FIT = 2,
        RESPONSE_PLOT = 3
    };
    GtkWidget *label, *table, *dialog, *hbox, *hbox2, *table2, *vbox;
    GwyGraphModel *gmodel;
    FitControls controls;
    gint response, i, j;

    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Fit graph"), NULL, 0, NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Fit"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_FIT);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          _("_Reset inits"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          _("_Plot inits"), RESPONSE_PLOT);
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
                                           &controls, args->function_type);
    gtk_container_add(GTK_CONTAINER(vbox), controls.selector);

    controls.equation = gtk_label_new("f(x) =");
    gtk_misc_set_alignment(GTK_MISC(controls.equation), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), controls.equation);

    /*fit parameters*/
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Fitting parameters:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    table = gtk_table_new(MAX_PARAMS, 7, FALSE);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), " ");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Initial</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Result</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Error</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Fix</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 5, 6, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.param_des = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_des[i] = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(controls.param_des[i]), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), controls.param_des[i],
                         0, 1, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    controls.param_init = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_init[i] = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(controls.param_init[i]), 12);
        gtk_entry_set_width_chars(GTK_ENTRY(controls.param_init[i]), 12);
        g_signal_connect(controls.param_init[i], "changed",
                         G_CALLBACK(double_entry_changed_cb),
                         &args->par_init[i]);
        gtk_table_attach(GTK_TABLE(table), controls.param_init[i],
                         1, 2, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    controls.param_copy = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_copy[i] = gtk_button_new_with_label("←");
        g_signal_connect(controls.param_copy[i], "clicked",
                         G_CALLBACK(copy_param_cb), &controls);
        g_object_set_data(G_OBJECT(controls.param_copy[i]), "index",
                          GINT_TO_POINTER(i));
        gtk_table_attach(GTK_TABLE(table), controls.param_copy[i],
                         2, 3, i+1, i+2, 0, 0, 0, 2);
    }

    controls.param_res = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_res[i] = gtk_label_new(NULL);
        gtk_label_set_selectable(GTK_LABEL(controls.param_res[i]), TRUE);
        gtk_table_attach(GTK_TABLE(table), controls.param_res[i],
                         3, 4, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    controls.param_err = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_err[i] = gtk_label_new(NULL);
        gtk_label_set_selectable(GTK_LABEL(controls.param_res[i]), TRUE);
        gtk_table_attach(GTK_TABLE(table), controls.param_err[i],
                         4, 5, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    controls.param_fit = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_fit[i] = gtk_check_button_new();
        g_signal_connect(controls.param_fit[i], "toggled",
                         G_CALLBACK(toggle_changed_cb), &args->par_fix[i]);
        gtk_table_attach(GTK_TABLE(table), controls.param_fit[i],
                         5, 6, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    gtk_container_add(GTK_CONTAINER(vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Correlation matrix:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    controls.covar = g_new0(GtkWidget*, MAX_PARAMS*MAX_PARAMS);
    table = gtk_table_new(MAX_PARAMS, MAX_PARAMS, TRUE);
    for (i = 0; i < MAX_PARAMS; i++) {
        for (j = 0; j <= i; j++) {
            label = controls.covar[i*MAX_PARAMS + j] = gtk_label_new(NULL);
            gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
            gtk_table_attach(GTK_TABLE(table), label,
                             j, j+1, i, i+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
        }
    }
    gtk_container_add(GTK_CONTAINER(vbox), table);

    hbox2 = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Chi-square result:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(hbox2), label);

    controls.chisq = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.chisq), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(hbox2), controls.chisq);

    gtk_container_add(GTK_CONTAINER(vbox), hbox2);

    /*FIt area*/
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Fit area</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    table2 = gtk_table_new(2, 2, FALSE);
    gmodel = gwy_graph_get_model(GWY_GRAPH(args->parent_graph));
    controls.data = gtk_adjustment_new(args->curve, 1,
                                       gwy_graph_model_get_n_curves(gmodel),
                                       1, 5, 0);
    gwy_table_attach_spinbutton(table2, 1, _("Graph data curve"), "",
                                controls.data);
    gtk_container_add(GTK_CONTAINER(vbox), table2);
    g_signal_connect(controls.data, "changed",
                     G_CALLBACK(curve_changed_cb), &controls);

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_box_set_spacing(GTK_BOX(hbox2), 4);

    label = gtk_label_new(_("From"));
    gtk_container_add(GTK_CONTAINER(hbox2), label);

    controls.from = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(controls.from), 12);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.from), 12);
    gtk_container_add(GTK_CONTAINER(hbox2), controls.from);
    g_signal_connect(controls.from, "changed",
                     G_CALLBACK(from_changed_cb), &controls);

    label = gtk_label_new(_("To"));
    gtk_container_add(GTK_CONTAINER(hbox2), label);

    controls.to = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(controls.to), 12);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.to), 12);
    gtk_container_add(GTK_CONTAINER(hbox2), controls.to);
    g_signal_connect(controls.to, "changed",
                     G_CALLBACK(to_changed_cb), &controls);

    gtk_container_add(GTK_CONTAINER(vbox), hbox2);

    /*graph*/
    args->graph_model = gwy_graph_model_new();
    controls.graph = gwy_graph_new(args->graph_model);
    g_object_unref(args->graph_model);
    gwy_graph_enable_user_input(GWY_GRAPH(controls.graph), FALSE);
    gwy_graph_area_set_selection_limit(gwy_graph_get_area(GWY_GRAPH(controls.graph)), 1);
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, FALSE, FALSE, 0);
    gtk_widget_set_size_request(controls.graph, 400, 300);
    gwy_graph_set_status(GWY_GRAPH(controls.graph), GWY_GRAPH_STATUS_XSEL);
    g_signal_connect(controls.graph, "selected",
                     G_CALLBACK(graph_selected), &controls);

    args->fitfunc = gwy_inventory_get_nth_item(gwy_nlfit_presets(),
                                               args->function_type);

    reset(args, &controls);
    dialog_update(&controls, args);

    /*XXX Shouldn't need this */
    graph_update(&controls, args);

    graph_selected(GWY_GRAPH(controls.graph), &controls);

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
            if (args->is_fitted && args->fitter->covar)
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
destroy(FitArgs *args, FitControls *controls)
{
    g_free(controls->param_init);
    g_free(controls->param_res);
    g_free(controls->param_fit);
    g_free(controls->param_err);
    g_free(controls->param_copy);
    g_free(controls->covar);
    if (args->fitter)
        gwy_math_nlfit_free(args->fitter);
}

static void
clear(G_GNUC_UNUSED FitArgs *args, FitControls *controls)
{
    gint i, j;

    /*XXX Shouldn't need this */
    graph_update(controls, args);

    for (i = 0; i < MAX_PARAMS; i++) {
        gtk_label_set_markup(GTK_LABEL(controls->param_res[i]), " ");
        gtk_label_set_markup(GTK_LABEL(controls->param_err[i]), " ");
        for (j = 0; j <= i; j++)
            gtk_label_set_markup(GTK_LABEL(controls->covar[i*MAX_PARAMS + j]),
                                 " ");
    }

    gtk_label_set_markup(GTK_LABEL(controls->chisq), " ");
}

static void
plot_inits(FitArgs *args, FitControls *controls)
{
    GwyDataLine *xdata;
    GwyDataLine *ydata;
    GwyNLFitPreset *function;
    gint i;
    gboolean ok;
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

    function = gwy_inventory_get_nth_item(gwy_nlfit_presets(),
                                          args->function_type);

    for (i = 0; i < xdata->res; i++)
        ydata->data[i] = gwy_nlfit_preset_get_value(function, xdata->data[i],
                                                    args->par_init, &ok);

    /*XXX Shouldn't need this */
    graph_update(controls, args);

    cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_curve_type(cmodel, GWY_GRAPH_CURVE_LINE);
    gwy_graph_curve_model_set_description(cmodel, "fit");
    gwy_graph_curve_model_set_data(cmodel,
                             xdata->data,
                             ydata->data,
                             xdata->res);
    gwy_graph_model_add_curve(args->graph_model, cmodel);
    g_object_unref(cmodel);
    g_object_unref(xdata);
    g_object_unref(ydata);

}

 /*recompute fit and update everything*/
static void
recompute(FitArgs *args, FitControls *controls)
{
    GwyDataLine *xdata;
    GwyDataLine *ydata;
    GwyNLFitPreset *function;
    gboolean fixed[MAX_PARAMS];
    gchar buffer[64];
    gint i, j, nparams;
    gboolean ok, allfixed;
    GwyGraphCurveModel *cmodel;

    function = gwy_inventory_get_nth_item(gwy_nlfit_presets(),
                                          args->function_type);
    nparams = gwy_nlfit_preset_get_nparams(function);

    allfixed = TRUE;
    for (i = 0; i < nparams; i++) {
        fixed[i] = args->par_fix[i];
        allfixed &= fixed[i];
        args->par_res[i] = args->par_init[i];
    }
    if (allfixed)
        return;

    xdata = gwy_data_line_new(10, 10, FALSE);
    ydata = gwy_data_line_new(10, 10, FALSE);

    args->curve = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->data));


    if (!normalize_data(args, xdata, ydata, args->curve - 1))
    {
        g_object_unref(xdata);
        g_object_unref(ydata);
        return;
    }


    if (args->fitter)
        gwy_math_nlfit_free(args->fitter);
    args->fitter = gwy_nlfit_preset_fit(function, NULL,
                                        xdata->res, xdata->data, ydata->data,
                                        args->par_res, args->err, fixed);

    for (i = 0; i < nparams; i++) {
        g_snprintf(buffer, sizeof(buffer), "%2.3g", args->par_res[i]);
        gtk_label_set_markup(GTK_LABEL(controls->param_res[i]), buffer);
    }

    if (args->fitter->covar)
    {
        /* FIXME: this is _scaled_ dispersion */
        g_snprintf(buffer, sizeof(buffer), "%2.3g",
                   gwy_math_nlfit_get_dispersion(args->fitter));
        gtk_label_set_markup(GTK_LABEL(controls->chisq), buffer);

        for (i = 0; i < nparams; i++) {
            g_snprintf(buffer, sizeof(buffer), "%2.3g", args->err[i]);
            gtk_label_set_markup(GTK_LABEL(controls->param_err[i]), buffer);
        }

        for (i = 0; i < nparams; i++) {
            for (j = 0; j <= i; j++) {
                g_snprintf(buffer, sizeof(buffer), "% 0.3f",
                           gwy_math_nlfit_get_correlations(args->fitter, i, j));
                gtk_label_set_markup
                    (GTK_LABEL(controls->covar[i*MAX_PARAMS + j]), buffer);
            }
         }
    }
    else
        gtk_label_set_markup(GTK_LABEL(controls->covar[0]), _("N.A."));

    for (i = 0; i < xdata->res; i++)
        ydata->data[i] = gwy_nlfit_preset_get_value(function, xdata->data[i],
                                                    args->par_res, &ok);

    /*XXX Shouldn't need this */
    graph_update(controls, args);

    cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_curve_type(cmodel, GWY_GRAPH_CURVE_LINE);

    gwy_graph_curve_model_set_data(cmodel,
                                   xdata->data,
                                   ydata->data,
                                   xdata->res);
    gwy_graph_curve_model_set_description(cmodel, "fit");
    gwy_graph_model_add_curve(args->graph_model, cmodel);
    g_object_unref(cmodel);

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
curve_changed_cb(GtkAdjustment *adj,
                 FitControls *controls)
{
    controls->args->curve = gwy_adjustment_get_int(adj);
    /* FIXME: Anything else? */
}

static void
type_changed_cb(GtkWidget *combo, FitControls *controls)
{
    FitArgs *args = controls->args;
    gint active, i;

    active = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (active == args->function_type)
        return;

    args->function_type = active;
    args->fitfunc = gwy_inventory_get_nth_item(gwy_nlfit_presets(),
                                               args->function_type);
    for (i = 0; i < MAX_PARAMS; i++)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->param_fit[i]),
                                     FALSE);

    dialog_update(controls, args);
}

static void
dialog_update(FitControls *controls, FitArgs *args)
{
    char buffer[20];
    gboolean sens;
    gint i;

    clear(args, controls);
    guess(controls, args);

    gtk_label_set_markup(GTK_LABEL(controls->equation),
                         gwy_nlfit_preset_get_formula(args->fitfunc));

    for (i = 0; i < MAX_PARAMS; i++) {
        sens = (i < gwy_nlfit_preset_get_nparams(args->fitfunc));
        if (sens) {
            gtk_label_set_markup(GTK_LABEL(controls->param_des[i]),
                                 gwy_nlfit_preset_get_param_name(args->fitfunc,
                                                                 i));

            g_snprintf(buffer, sizeof(buffer), "%.4g", args->par_init[i]);
            gtk_entry_set_text(GTK_ENTRY(controls->param_init[i]), buffer);
        }
        else {
            gtk_entry_set_text(GTK_ENTRY(controls->param_init[i]), "");
            gtk_label_set_text(GTK_LABEL(controls->param_des[i]), "");
        }
        gtk_widget_set_sensitive(controls->param_des[i], sens);
        gtk_widget_set_sensitive(controls->param_init[i], sens);
        gtk_widget_set_sensitive(controls->param_fit[i], sens);
        gtk_widget_set_sensitive(controls->param_copy[i], sens);
    }
}

static void
guess(G_GNUC_UNUSED FitControls *controls, FitArgs *args)
{
    GwyDataLine *xdata, *ydata;
    GwyNLFitPreset *function;
    gboolean ok;

    function = gwy_inventory_get_nth_item(gwy_nlfit_presets(),
                                          args->function_type);

    xdata = GWY_DATA_LINE(gwy_data_line_new(10, 10, FALSE));
    ydata = GWY_DATA_LINE(gwy_data_line_new(10, 10, FALSE));

    if (!normalize_data(args, xdata, ydata, args->curve - 1)) {
        g_object_unref(xdata);
        g_object_unref(ydata);
        return;
    }
    gwy_nlfit_preset_guess(function,
                           gwy_data_line_get_res(xdata),
                           gwy_data_line_get_data_const(xdata),
                           gwy_data_line_get_data_const(ydata),
                           args->par_init, &ok);
    memcpy(args->par_res, args->par_init, MAX_PARAMS*sizeof(gdouble));

    g_object_unref(xdata);
    g_object_unref(ydata);
}

/*
 * XXX XXX XXX: Get rid of this brain damage XXX XXX XXX
 * No one any longer needs to update graphs this way.
 */
static void
graph_update(G_GNUC_UNUSED FitControls *controls, FitArgs *args)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    gint i, n;

    /*clear graph*/
    gwy_graph_model_remove_all_curves(args->graph_model);

    gmodel = gwy_graph_get_model(GWY_GRAPH(args->parent_graph));
    n = gwy_graph_model_get_n_curves(gmodel);
    for (i = 0; i < n; i++) {
        gcmodel = gwy_graph_model_get_curve_by_index(gmodel, i);
        gwy_graph_model_add_curve(args->graph_model, gcmodel);
    }
    args->is_fitted = FALSE;
}

static void
graph_selected(GwyGraph* graph, FitControls *controls)
{
    FitArgs *args = controls->args;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    gchar buffer[24];
    gdouble selection[2];
    const gdouble *data;

    gwy_graph_area_get_selection(gwy_graph_get_area(graph), selection);

    if (gwy_graph_area_get_selection_number(gwy_graph_get_area(graph)) <= 0
        || selection[0] == selection[1]) {
        gmodel = gwy_graph_get_model(graph);
        gcmodel = gwy_graph_model_get_curve_by_index(gmodel,
                                                     controls->args->curve - 1);
        data = gwy_graph_curve_model_get_xdata(gcmodel);
        args->from = data[0];
        args->to = data[gwy_graph_curve_model_get_ndata(gcmodel) - 1];
    }
    else {
        args->from = selection[0];
        args->to = selection[1];
        if (args->from > args->to)
            GWY_SWAP(gdouble, args->from, args->to);
    }
    g_snprintf(buffer, sizeof(buffer), "%.3g", args->from);
    gtk_entry_set_text(GTK_ENTRY(controls->from), buffer);
    g_snprintf(buffer, sizeof(buffer), "%.3g", args->to);
    gtk_entry_set_text(GTK_ENTRY(controls->to), buffer);
    dialog_update(controls, args);
}

static void
double_entry_changed_cb(GtkWidget *entry, gdouble *value)
{
    *value = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
}

static void
from_changed_cb(GtkWidget *entry, FitControls *controls)
{
    controls->args->from = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
    dialog_update(controls, controls->args);
}

static void
to_changed_cb(GtkWidget *entry, FitControls *controls)
{
    controls->args->to = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
    dialog_update(controls, controls->args);
}

static void
toggle_changed_cb(GtkToggleButton *button, gboolean *value)
{
    *value = gtk_toggle_button_get_active(button);
}

static void
copy_param_cb(GObject *button,
              FitControls *controls)
{
    gchar buffer[20];
    gint i;

    i = GPOINTER_TO_INT(g_object_get_data(button, "index"));
    g_snprintf(buffer, sizeof(buffer), "%.4g", controls->args->par_res[i]);
    gtk_entry_set_text(GTK_ENTRY(controls->param_init[i]), buffer);
}

static GtkWidget*
create_preset_menu(GCallback callback,
                   gpointer cbdata,
                   gint current)
{
    GtkCellRenderer *renderer;
    GtkWidget *combo;
    GwyInventoryStore *store;
    gint i;

    store = gwy_inventory_store_new(gwy_nlfit_presets());

    combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 2);
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    i = gwy_inventory_store_get_column_by_name(store, "name");
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), renderer, "text", i);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current);
    g_signal_connect(combo, "changed", callback, cbdata);

    return combo;
}

static const gchar *preset_key = "/module/graph_fit/preset";

static void
load_args(GwyContainer *container,
          FitArgs *args)
{
    static const guchar *preset;

    if (gwy_container_gis_string_by_name(container, preset_key, &preset))
        args->function_type
            = gwy_inventory_get_item_position(gwy_nlfit_presets(),
                                              (const gchar*)preset);
}

static void
save_args(GwyContainer *container,
          FitArgs *args)
{
    GwyNLFitPreset *func;
    const gchar *name;

    func = gwy_inventory_get_nth_item(gwy_nlfit_presets(), args->function_type);
    name = gwy_resource_get_name(GWY_RESOURCE(func));
    gwy_container_set_string_by_name(container, preset_key, g_strdup(name));
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
    const gdouble *xs, *ys;
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
    GwyNLFitter *fitter = args->fitter;
    GtkWidget *window, *tab, *table, *label;
    gdouble mag, value, sigma;
    gint row, curve, n, i, j;
    gint precision;
    GString *str, *su;
    const gchar *s;

    g_return_if_fail(args->is_fitted);
    g_return_if_fail(fitter->covar);

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

    attach_label(table, _(_("<b>Data:</b>")), row, 0, 0.0);
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
    attach_label(table, gwy_resource_get_name(GWY_RESOURCE(args->fitfunc)),
                 row, 1, 0.0);
    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label),
                         gwy_nlfit_preset_get_formula(args->fitfunc));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    attach_label(table, _("<b>Results</b>"), row, 0, 0.0);
    row++;

    n = gwy_nlfit_preset_get_nparams(args->fitfunc);
    tab = gtk_table_new(n, 6, FALSE);
    gtk_table_attach(GTK_TABLE(table), tab, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    for (i = 0; i < n; i++) {
        attach_label(tab, "=", i, 1, 0.5);
        attach_label(tab, "±", i, 3, 0.5);
        s = gwy_nlfit_preset_get_param_name(args->fitfunc, i);
        attach_label(tab, s, i, 0, 0.0);
        value = args->par_res[i];
        sigma = args->err[i];
        mag = gwy_math_humanize_numbers(sigma/12, fabs(value), &precision);
        g_string_printf(str, "%.*f", precision, value/mag);
        attach_label(tab, str->str, i, 2, 1.0);
        g_string_printf(str, "%.*f", precision, sigma/mag);
        attach_label(tab, str->str, i, 4, 1.0);
        attach_label(tab, format_magnitude(su, mag), i, 5, 0.0);
    }
    row++;

    attach_label(table, _("Residual sum:"), row, 0, 0.0);
    sigma = gwy_math_nlfit_get_dispersion(fitter);
    mag = gwy_math_humanize_numbers(sigma/120, sigma, &precision);
    g_string_printf(str, "%.*f %s",
                    precision, sigma/mag, format_magnitude(su, mag));
    attach_label(table, str->str, row, 1, 0.0);
    row++;

    attach_label(table, _("<b>Correlation matrix</b>"), row, 0, 0.0);
    row++;

    tab = gtk_table_new(n, n, TRUE);
    for (i = 0; i < n; i++) {
        for (j = 0; j <= i; j++) {
            g_string_printf(str, "% .03f",
                            gwy_math_nlfit_get_correlations(fitter, i, j));
            attach_label(tab, str->str, i, j, 1.0);
        }
    }
    gtk_table_attach(GTK_TABLE(table), tab, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
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
    gint i, j, curve, n;

    g_assert(args->fitter->covar);
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
                           gwy_resource_get_name(GWY_RESOURCE(args->fitfunc)));
    g_string_append_printf(report, _("\nResults\n"));
    n = gwy_nlfit_preset_get_nparams(args->fitfunc);
    for (i = 0; i < n; i++) {
        /* FIXME: how to do this better? use pango_parse_markup()? */
        s = gwy_strreplace(gwy_nlfit_preset_get_param_name(args->fitfunc, i),
                           "<sub>", "", (gsize)-1);
        s2 = gwy_strreplace(s, "</sub>", "", (gsize)-1);
        g_string_append_printf(report, "%s = %g ± %g\n",
                               s2, args->par_res[i], args->err[i]);
        g_free(s2);
        g_free(s);
    }
    g_string_append_printf(report, _("\nResidual sum:   %g\n"),
                           gwy_math_nlfit_get_dispersion(args->fitter));
    g_string_append_printf(report, _("\nCorrelation matrix\n"));
    for (i = 0; i < n; i++) {
        for (j = 0; j <= i; j++) {
            g_string_append_printf
                (report, "% .03f",
                 gwy_math_nlfit_get_correlations(args->fitter, i, j));
            if (j != i)
                g_string_append_c(report, ' ');
        }
        g_string_append_c(report, '\n');
    }

    g_string_free(str, TRUE);

    return report;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
