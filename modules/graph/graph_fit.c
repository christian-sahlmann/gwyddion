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
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwynlfitpreset.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-graph.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

enum { MAX_PARAMS = 4 };

enum {
    RESPONSE_ESTIMATE = 1,
    RESPONSE_FIT,
    RESPONSE_PLOT,
    RESPONSE_SAVE,
};

typedef struct {
    gboolean fix;
    gdouble init;
    gdouble value;
    gdouble error;
} FitParamArg;

typedef struct {
    gint function_type;
    gint curve;
    gdouble from;
    gdouble to;
    FitParamArg param[MAX_PARAMS];
    gdouble crit;
    GwyNLFitPreset *fitfunc;
    GwyGraph *parent_graph;
    GwyNLFitter *fitter;
    gboolean is_estimated;
    gboolean is_fitted;
    gboolean auto_estimate;
    gboolean auto_plot;
    GwyGraphModel *graph_model;
    GwyDataLine *xdata;
    GwyDataLine *ydata;
    GwyRGBA fitcolor;
    GwySIValueFormat *abscissa_vf;
} FitArgs;

typedef struct {
    GtkWidget *fix;
    GtkWidget *name;
    GtkWidget *equals;
    GtkWidget *value;
    GtkWidget *value_unit;
    GtkWidget *pm;
    GtkWidget *error;
    GtkWidget *error_unit;
    GtkWidget *copy;
    GtkWidget *init;
} FitParamControl;

typedef struct {
    FitArgs *args;
    GtkWidget *dialog;
    GtkWidget *graph;
    GtkWidget *from;
    GtkWidget *to;
    GtkWidget *curve;
    GtkWidget *chisq;
    GtkWidget *function;
    GtkWidget *formula;
    GtkWidget **covar;
    FitParamControl param[MAX_PARAMS];
    GtkWidget *auto_estimate;
    GtkWidget *auto_plot;
    gboolean in_update;
} FitControls;

static gboolean    module_register           (void);
static void        fit                       (GwyGraph *graph);
static void        fit_dialog                (FitArgs *args);
static void        fit_fetch_entry           (FitControls *controls);
static void        fit_param_row_create      (FitControls *controls,
                                              gint i,
                                              GtkTable *table,
                                              gint row);
static void        fit_do                    (FitControls *controls);
static void        curve_changed             (GtkComboBox *combo,
                                              FitControls *controls);
static void        auto_estimate_changed     (GtkToggleButton *check,
                                              FitControls *controls);
static void        auto_plot_changed         (GtkToggleButton *check,
                                              FitControls *controls);
static void        function_changed          (GtkComboBox *combo,
                                              FitControls *controls);
static void        range_changed             (GtkWidget *entry,
                                              FitControls *controls);
static void        fit_limit_selection       (FitControls *controls,
                                              gboolean curve_switch);
static void        fit_get_full_x_range      (FitControls *controls,
                                              gdouble *xmin,
                                              gdouble *xmax);
static void        param_initial_activate    (GtkWidget *entry,
                                              gpointer user_data);
static void        toggle_changed            (GtkToggleButton *button,
                                              gboolean *value);
static void        copy_param                (GObject *button,
                                              FitControls *controls);
static void        fit_plot_curve            (FitArgs *args);
static void        fit_set_state             (FitControls *controls,
                                              gboolean is_fitted,
                                              gboolean is_estimated);
static void        fit_estimate              (FitControls *controls);
static void        fit_param_row_update_value(FitControls *controls,
                                              gint i,
                                              gboolean errorknown);
static void        graph_selected            (GwySelection* selection,
                                              gint i,
                                              FitControls *controls);
static gint        normalize_data            (FitArgs *args);
static GtkWidget*  function_selector_new     (GCallback callback,
                                              gpointer cbdata,
                                              gint current);
static GtkWidget*  curve_selector_new        (GwyGraphModel *gmodel,
                                              GCallback callback,
                                              FitControls *controls,
                                              gint current);
static void        load_args                 (GwyContainer *container,
                                              FitArgs *args);
static void        save_args                 (GwyContainer *container,
                                              FitArgs *args);
static GString*    create_fit_report         (FitArgs *args);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Fit graph with function"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_graph_func_register("graph_fit",
                            (GwyGraphFunc)&fit,
                            N_("/_Fit Function..."),
                            GWY_STOCK_GRAPH_FUNCTION,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Fit a function on graph data"));

    return TRUE;
}

static void
fit(GwyGraph *graph)
{
    GwyContainer *settings;
    gint i;
    FitArgs args;

    memset(&args, 0, sizeof(FitArgs));

    args.auto_estimate = TRUE;
    args.auto_plot = TRUE;
    args.parent_graph = graph;
    args.xdata = gwy_data_line_new(1, 1.0, FALSE);
    args.ydata = gwy_data_line_new(1, 1.0, FALSE);
    for (i = 0; i < MAX_PARAMS; i++)
        args.param[i].fix = FALSE;

    settings = gwy_app_settings_get();
    load_args(settings, &args);
    fit_dialog(&args);
    save_args(settings, &args);

    g_object_unref(args.xdata);
    g_object_unref(args.ydata);
    if (args.fitter)
        gwy_math_nlfit_free(args.fitter);
    if (args.abscissa_vf)
        gwy_si_unit_value_format_free(args.abscissa_vf);
}

static void
fit_dialog(FitArgs *args)
{
    GtkWidget *label, *dialog, *hbox, *hbox2, *table, *align, *expander;
    GtkTable *table2;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *cmodel;
    GwyGraphArea *area;
    GwySelection *selection;
    GwySIUnit *siunit;
    FitControls controls;
    gint response, i, j, row;
    GString *report;
    gdouble xmin, xmax;

    controls.args = args;
    controls.in_update = TRUE;

    gmodel = gwy_graph_get_model(GWY_GRAPH(args->parent_graph));
    gwy_graph_model_get_x_range(gmodel, &xmin, &xmax);
    g_object_get(gmodel, "si-unit-x", &siunit, NULL);
    args->abscissa_vf
        = gwy_si_unit_get_format_with_digits(siunit,
                                             GWY_SI_UNIT_FORMAT_VFMARKUP,
                                             MAX(fabs(xmin), fabs(xmax)), 4,
                                             NULL);
    g_object_unref(siunit);

    dialog = gtk_dialog_new_with_buttons(_("Fit Graph"), NULL, 0, NULL);
    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Fit"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_FIT);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          _("_Estimate"), RESPONSE_ESTIMATE);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          _("_Plot Inits"), RESPONSE_PLOT);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_SAVE, RESPONSE_SAVE);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    table = gtk_table_new(7, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    row = 0;

    /* Curve to fit */
    label = gtk_label_new_with_mnemonic(_("_Graph curve:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    controls.curve = curve_selector_new(gmodel,
                                        G_CALLBACK(curve_changed), &controls,
                                        args->curve);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.curve);
    gtk_table_attach(GTK_TABLE(table), controls.curve,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    /* Fitted function */
    label = gtk_label_new_with_mnemonic(_("F_unction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    controls.function = function_selector_new(G_CALLBACK(function_changed),
                                              &controls, args->function_type);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.function);
    gtk_table_attach(GTK_TABLE(table), controls.function,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.formula = gtk_label_new("f(x) =");
    gtk_misc_set_alignment(GTK_MISC(controls.formula), 0.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(controls.formula), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.formula,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 8);
    row++;

    /* Parameters sought */
    table2 = GTK_TABLE(gtk_table_new(MAX_PARAMS + 1, 10, FALSE));
    gtk_table_set_row_spacing(table2, 0, 2);
    gtk_table_set_col_spacings(table2, 2);
    gtk_table_set_col_spacing(table2, 0, 6);
    gtk_table_set_col_spacing(table2, 4, 6);
    gtk_table_set_col_spacing(table2, 5, 6);
    gtk_table_set_col_spacing(table2, 7, 6);
    gtk_table_set_col_spacing(table2, 8, 6);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(table2),
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_table_attach(table2, gwy_label_new_header(_("Fix")),
                     0, 1, 0, 1, GTK_FILL, 0, 0, 0);
    gtk_table_attach(table2, gwy_label_new_header(_("Parameter")),
                     1, 5, 0, 1, GTK_FILL, 0, 0, 0);
    gtk_table_attach(table2, gwy_label_new_header(_("Error")),
                     6, 8, 0, 1, GTK_FILL, 0, 0, 0);
    gtk_table_attach(table2, gwy_label_new_header(_("Initial")),
                     9, 10, 0, 1, GTK_FILL, 0, 0, 0);

    for (i = 0; i < MAX_PARAMS; i++)
        fit_param_row_create(&controls, i, table2, i+1);

    /* Chi^2 */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("χ<sup>2</sup> result:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    controls.chisq = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.chisq), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.chisq,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* Correlation matrix */
    expander = gtk_expander_new(NULL);
    gtk_expander_set_label_widget(GTK_EXPANDER(expander),
                                 gwy_label_new_header(_("Correlation Matrix")));
    gtk_table_attach(GTK_TABLE(table), expander,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
    gtk_container_add(GTK_CONTAINER(expander), align);
    row++;

    controls.covar = g_new0(GtkWidget*, MAX_PARAMS*MAX_PARAMS);
    g_signal_connect_swapped(dialog, "destroy",
                             G_CALLBACK(g_free), controls.covar);
    table2 = GTK_TABLE(gtk_table_new(MAX_PARAMS, MAX_PARAMS, TRUE));
    gtk_table_set_col_spacings(table2, 6);
    gtk_table_set_row_spacings(table2, 2);
    gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(table2));

    for (i = 0; i < MAX_PARAMS; i++) {
        for (j = 0; j <= i; j++) {
            label = controls.covar[i*MAX_PARAMS + j] = gtk_label_new(NULL);
            gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
            gtk_table_attach(table2, label,
                             j, j+1, i, i+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        }
    }

    /* Fit area */
    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_table_attach(GTK_TABLE(table), hbox2,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Range:"));
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    controls.from = gtk_entry_new();
    g_object_set_data(G_OBJECT(controls.from), "id", (gpointer)"from");
    gtk_entry_set_width_chars(GTK_ENTRY(controls.from), 8);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.from, FALSE, FALSE, 0);
    g_signal_connect(controls.from, "activate",
                     G_CALLBACK(range_changed), &controls);
    gwy_widget_set_activate_on_unfocus(controls.from, TRUE);

    label = gtk_label_new(_("range|to"));
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    controls.to = gtk_entry_new();
    g_object_set_data(G_OBJECT(controls.to), "id", (gpointer)"to");
    gtk_entry_set_width_chars(GTK_ENTRY(controls.to), 8);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.to, FALSE, FALSE, 0);
    g_signal_connect(controls.to, "activate",
                     G_CALLBACK(range_changed), &controls);
    gwy_widget_set_activate_on_unfocus(controls.to, TRUE);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), args->abscissa_vf->units);
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    /* Auto-update */
    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_table_attach(GTK_TABLE(table), hbox2,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Instant:"));
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    controls.auto_estimate = gtk_check_button_new_with_mnemonic("e_stimate");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.auto_estimate),
                                 args->auto_estimate);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.auto_estimate, FALSE, FALSE, 0);
    g_signal_connect(controls.auto_estimate, "toggled",
                     G_CALLBACK(auto_estimate_changed), &controls);

    controls.auto_plot = gtk_check_button_new_with_mnemonic("p_lot");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.auto_plot),
                                 args->auto_plot);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.auto_plot, FALSE, FALSE, 0);
    g_signal_connect(controls.auto_plot, "toggled",
                     G_CALLBACK(auto_plot_changed), &controls);

    /* Graph */
    args->graph_model = gwy_graph_model_new_alike(gmodel);
    controls.graph = gwy_graph_new(args->graph_model);
    g_object_unref(args->graph_model);
    gtk_widget_set_size_request(controls.graph, 400, 300);

    gwy_graph_enable_user_input(GWY_GRAPH(controls.graph), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, TRUE, TRUE, 0);
    gwy_graph_set_status(GWY_GRAPH(controls.graph), GWY_GRAPH_STATUS_XSEL);

    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls.graph)));
    selection = gwy_graph_area_get_selection(area, GWY_GRAPH_STATUS_XSEL);
    gwy_selection_set_max_objects(selection, 1);
    g_signal_connect(selection, "changed",
                     G_CALLBACK(graph_selected), &controls);

    gwy_graph_model_add_curve(controls.args->graph_model,
                              gwy_graph_model_get_curve(gmodel, args->curve));
    function_changed(GTK_COMBO_BOX(controls.function), &controls);
    graph_selected(selection, -1, &controls);

    controls.in_update = FALSE;
    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        fit_fetch_entry(&controls);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            return;
            break;

            case GTK_RESPONSE_OK:
            if (args->is_fitted && args->fitter->covar) {
                cmodel = gwy_graph_model_get_curve(args->graph_model, 1);
                gwy_graph_model_add_curve(gmodel, cmodel);
            }
            gtk_widget_destroy(dialog);
            break;

            case RESPONSE_SAVE:
            report = create_fit_report(args);
            gwy_save_auxiliary_data(_("Save Fit Report"), GTK_WINDOW(dialog),
                                    -1, report->str);
            g_string_free(report, TRUE);
            break;

            case RESPONSE_ESTIMATE:
            fit_estimate(&controls);
            break;

            case RESPONSE_PLOT:
            fit_set_state(&controls, FALSE, TRUE);
            fit_plot_curve(args);
            break;

            case RESPONSE_FIT:
            fit_do(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);
}

static void
fit_fetch_entry(FitControls *controls)
{
    GtkWidget *entry;

    entry = gtk_window_get_focus(GTK_WINDOW(controls->dialog));
    if (entry
        && GTK_IS_ENTRY(entry)
        && g_object_get_data(G_OBJECT(entry), "id"))
        gtk_widget_activate(entry);
}

static void
fit_param_row_create(FitControls *controls,
                     gint i,
                     GtkTable *table,
                     gint row)
{
    FitParamControl *cntrl;
    FitParamArg *arg;

    cntrl = &controls->param[i];
    arg = &controls->args->param[i];

    /* Fix */
    cntrl->fix = gtk_check_button_new();
    gtk_table_attach(table, cntrl->fix, 0, 1, row, row+1, 0, 0, 0, 0);
    g_signal_connect(cntrl->fix, "toggled",
                     G_CALLBACK(toggle_changed), &arg->fix);

    /* Name */
    cntrl->name = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(cntrl->name), 1.0, 0.5);
    gtk_table_attach(table, cntrl->name,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    /* Equals */
    cntrl->equals = gtk_label_new("=");
    gtk_table_attach(table, cntrl->equals, 2, 3, row, row+1, 0, 0, 0, 0);

    /* Value */
    cntrl->value = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(cntrl->value), 1.0, 0.5);
    gtk_table_attach(table, cntrl->value,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);

    cntrl->value_unit = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(cntrl->value_unit), 0.0, 0.5);
    gtk_table_attach(table, cntrl->value_unit,
                     4, 5, row, row+1, GTK_FILL, 0, 0, 0);

    /* Plus-minus */
    cntrl->pm = gtk_label_new("±");
    gtk_table_attach(table, cntrl->pm, 5, 6, row, row+1, 0, 0, 0, 0);

    /* Error */
    cntrl->error = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(cntrl->error), 1.0, 0.5);
    gtk_table_attach(table, cntrl->error,
                     6, 7, row, row+1, GTK_FILL, 0, 0, 0);

    cntrl->error_unit = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(cntrl->error_unit), 0.0, 0.5);
    gtk_table_attach(table, cntrl->error_unit,
                     7, 8, row, row+1, GTK_FILL, 0, 0, 0);

    /* Copy */
    cntrl->copy = gtk_button_new_with_label("→");
    gtk_button_set_relief(GTK_BUTTON(cntrl->copy), GTK_RELIEF_NONE);
    gtk_table_attach(table, cntrl->copy, 8, 9, row, row+1, 0, 0, 0, 0);
    g_object_set_data(G_OBJECT(cntrl->copy), "id", GINT_TO_POINTER(i + 1));
    g_signal_connect(cntrl->copy, "clicked", G_CALLBACK(copy_param), controls);

    /* Initial */
    cntrl->init = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(cntrl->init), 12);
    gtk_table_attach(table, cntrl->init,
                     9, 10, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_object_set_data(G_OBJECT(cntrl->init), "id", GINT_TO_POINTER(i + 1));
    g_signal_connect(cntrl->init, "activate",
                     G_CALLBACK(param_initial_activate), controls);
    gwy_widget_set_activate_on_unfocus(cntrl->init, TRUE);
}

static void
fix_minus(gchar *buf, guint size)
{
    guint len;

    if (buf[0] != '-')
        return;

    len = strlen(buf);
    if (len+2 >= size)
        return;

    g_memmove(buf+3, buf+1, len-1);
    buf[len+2] = '\0';
    buf[0] = '\xe2';
    buf[1] = '\x88';
    buf[2] = '\x92';
}

static void
fit_plot_curve(FitArgs *args)
{
    GwyGraphCurveModel *cmodel;
    gdouble *xd, *yd;
    gboolean initial, ok;   /* XXX: ignored */
    gint i, n;
    gdouble *param;

    if (!args->is_fitted && !args->is_estimated)
        return;

    initial = !args->is_fitted;
    n = gwy_nlfit_preset_get_nparams(args->fitfunc);
    param = g_newa(gdouble, n);
    for (i = 0; i < n; i++)
        param[i] = initial ? args->param[i].init : args->param[i].value;

    n = gwy_data_line_get_res(args->xdata);
    g_return_if_fail(n == gwy_data_line_get_res(args->ydata));
    xd = gwy_data_line_get_data(args->xdata);
    yd = gwy_data_line_get_data(args->ydata);

    for (i = 0; i < n; i++)
        yd[i] = gwy_nlfit_preset_get_value(args->fitfunc, xd[i], param, &ok);

    if (gwy_graph_model_get_n_curves(args->graph_model) == 2)
        cmodel = gwy_graph_model_get_curve(args->graph_model, 1);
    else {
        cmodel = gwy_graph_curve_model_new();
        g_object_set(cmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", &args->fitcolor,
                     NULL);
        gwy_graph_model_add_curve(args->graph_model, cmodel);
        g_object_unref(cmodel);
    }
    g_object_set(cmodel,
                 "description",
                 initial ? _("noun|Estimate") : gwy_sgettext("noun|Fit"),
                 NULL);
    gwy_graph_curve_model_set_data(cmodel, xd, yd, n);
}

static void
fit_do(FitControls *controls)
{
    FitArgs *args;
    GtkWidget *dialog;
    gdouble *param, *error;
    gboolean *fixed;
    gint i, j, nparams, nfree = 0;
    gboolean allfixed, errorknown;

    args = controls->args;
    nparams = gwy_nlfit_preset_get_nparams(args->fitfunc);
    fixed = g_newa(gboolean, nparams);

    allfixed = TRUE;
    for (i = 0; i < nparams; i++) {
        fixed[i] = args->param[i].fix;
        allfixed &= fixed[i];
        args->param[i].value = args->param[i].init;
        if (!fixed[i])
            nfree++;
    }
    if (allfixed)
        return;

    if (!normalize_data(args))
        return;

    if (gwy_data_line_get_res(args->xdata) <= nfree) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(controls->dialog),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("It is necessary to select more "
                                          "data points than free fit "
                                          "parameters"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    if (args->fitter)
        gwy_math_nlfit_free(args->fitter);

    param = g_newa(gdouble, nparams);
    error = g_newa(gdouble, nparams);
    for (i = 0; i < nparams; i++)
        param[i] = args->param[i].value;
    args->fitter
        = gwy_nlfit_preset_fit(args->fitfunc, NULL,
                               gwy_data_line_get_res(args->xdata),
                               gwy_data_line_get_data_const(args->xdata),
                               gwy_data_line_get_data_const(args->ydata),
                               param, error, fixed);
    errorknown = (args->fitter->covar != NULL);

    for (i = 0; i < nparams; i++) {
        args->param[i].value = param[i];
        args->param[i].error = error[i];
        fit_param_row_update_value(controls, i, errorknown);
    }

    if (errorknown) {
        gchar buf[16];

        /* FIXME: this is _scaled_ dispersion */
        g_snprintf(buf, sizeof(buf), "%2.3g",
                   gwy_math_nlfit_get_dispersion(args->fitter));
        gtk_label_set_markup(GTK_LABEL(controls->chisq), buf);

        for (i = 0; i < nparams; i++) {
            for (j = 0; j <= i; j++) {
                g_snprintf(buf, sizeof(buf), "%0.3f",
                           gwy_math_nlfit_get_correlations(args->fitter, i, j));
                fix_minus(buf, sizeof(buf));
                gtk_label_set_markup
                           (GTK_LABEL(controls->covar[i*MAX_PARAMS + j]), buf);
            }
         }
    }
    else
        gtk_label_set_markup(GTK_LABEL(controls->covar[0]), _("N.A."));

    fit_set_state(controls, TRUE, TRUE);
    fit_plot_curve(args);
}

static void
curve_changed(GtkComboBox *combo,
              FitControls *controls)
{
    GwyGraphModel *parent_gmodel, *graph_model;

    controls->args->curve = gwy_enum_combo_box_get_active(combo);
    graph_model = controls->args->graph_model;
    parent_gmodel = gwy_graph_get_model(controls->args->parent_graph);

    gwy_graph_model_remove_all_curves(graph_model);
    gwy_graph_model_add_curve(graph_model,
                              gwy_graph_model_get_curve(parent_gmodel,
                                                        controls->args->curve));
    fit_limit_selection(controls, TRUE);
    fit_set_state(controls, FALSE, FALSE);
}

static void
auto_estimate_changed(GtkToggleButton *check,
                      FitControls *controls)
{
    controls->args->auto_estimate = gtk_toggle_button_get_active(check);
    if (controls->args->auto_estimate
        && !controls->args->is_fitted
        && !controls->args->is_estimated)
        fit_estimate(controls);
}

static void
auto_plot_changed(GtkToggleButton *check,
                  FitControls *controls)
{
    controls->args->auto_plot = gtk_toggle_button_get_active(check);
    if (controls->args->auto_plot && !controls->args->is_fitted)
        fit_plot_curve(controls->args);
}

static void
function_changed(GtkComboBox *combo, FitControls *controls)
{
    FitArgs *args = controls->args;
    gint nparams, i;
    gboolean sens;

    args->function_type = gtk_combo_box_get_active(combo);
    args->fitfunc = gwy_inventory_get_nth_item(gwy_nlfit_presets(),
                                               args->function_type);
    nparams = gwy_nlfit_preset_get_nparams(args->fitfunc);
    gtk_label_set_markup(GTK_LABEL(controls->formula),
                         gwy_nlfit_preset_get_formula(args->fitfunc));

    for (i = 0; i < MAX_PARAMS; i++) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->param[i].fix),
                                     FALSE);
        if ((sens = (i < nparams))) {
            gtk_label_set_markup(GTK_LABEL(controls->param[i].name),
                                 gwy_nlfit_preset_get_param_name(args->fitfunc,
                                                                 i));
        }
        else {
            gtk_label_set_text(GTK_LABEL(controls->param[i].name), "");
        }
        gtk_entry_set_text(GTK_ENTRY(controls->param[i].init), "");
        gtk_widget_set_sensitive(controls->param[i].name, sens);
        gtk_widget_set_sensitive(controls->param[i].equals, sens);
        gtk_widget_set_sensitive(controls->param[i].init, sens);
        gtk_widget_set_sensitive(controls->param[i].pm, sens);
        gtk_widget_set_sensitive(controls->param[i].fix, sens);
        gtk_widget_set_sensitive(controls->param[i].copy, sens);
    }

    fit_set_state(controls, FALSE, FALSE);
}

static void
fit_set_state(FitControls *controls,
              gboolean is_fitted,
              gboolean is_estimated)
{
    FitArgs *args;
    gint i, j;

    args = controls->args;
    if (!args->is_fitted == !is_fitted
        && !args->is_estimated == !is_estimated
        && !args->auto_estimate)
        return;

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_SAVE, is_fitted);

    if (args->is_fitted && !is_fitted) {
        if (gwy_graph_model_get_n_curves(args->graph_model) == 2)
            gwy_graph_model_remove_curve(args->graph_model, 1);

        for (i = 0; i < MAX_PARAMS; i++) {
            gtk_label_set_text(GTK_LABEL(controls->param[i].value), "");
            gtk_label_set_text(GTK_LABEL(controls->param[i].value_unit), "");
            gtk_label_set_text(GTK_LABEL(controls->param[i].error), "");
            gtk_label_set_text(GTK_LABEL(controls->param[i].error_unit), "");
            for (j = 0; j <= i; j++)
                gtk_label_set_text(GTK_LABEL(controls->covar[i*MAX_PARAMS + j]),
                                   "");
        }
        gtk_label_set_markup(GTK_LABEL(controls->chisq), NULL);
    }
    args->is_fitted = is_fitted;
    args->is_estimated = is_estimated;

    if (!is_estimated && args->auto_estimate)
        fit_estimate(controls);
}

static void
fit_estimate(FitControls *controls)
{
    FitArgs *args;
    guint nparams, i;
    gdouble *param;
    gchar buf[24];
    gboolean ok;

    args = controls->args;
    nparams = gwy_nlfit_preset_get_nparams(args->fitfunc);

    param = g_newa(gdouble, nparams);
    for (i = 0; i < nparams; i++)
        param[i] = args->param[i].init;

    if (!normalize_data(args))
        return;

    gwy_nlfit_preset_guess(args->fitfunc,
                           gwy_data_line_get_res(args->xdata),
                           gwy_data_line_get_data_const(args->xdata),
                           gwy_data_line_get_data_const(args->ydata),
                           param, &ok);

    for (i = 0; i < nparams; i++) {
        args->param[i].value = args->param[i].init = param[i];
        g_snprintf(buf, sizeof(buf), "%0.6g", param[i]);
        gtk_entry_set_text(GTK_ENTRY(controls->param[i].init), buf);
    }

    fit_set_state(controls, FALSE, TRUE);
    if (args->auto_plot)
        fit_plot_curve(controls->args);
}

static void
fit_param_row_update_value(FitControls *controls,
                           gint i,
                           gboolean errorknown)
{
    GwyGraphCurveModel *cmodel;
    FitParamControl *cntrl;
    FitParamArg *arg;
    GwySIValueFormat *vf;
    GwySIUnit *unitx, *unity, *unitp;
    char buf[16];

    cmodel = gwy_graph_model_get_curve(controls->args->graph_model, 0);
    cntrl = &controls->param[i];
    arg = &controls->args->param[i];

    if (!controls->args->fitter->eval) {
        gtk_label_set_text(GTK_LABEL(cntrl->value), "");
        gtk_label_set_text(GTK_LABEL(cntrl->value_unit), "");
        gtk_label_set_text(GTK_LABEL(cntrl->error), "");
        gtk_label_set_text(GTK_LABEL(cntrl->error_unit), "");
        return;
    }

    g_object_get(controls->args->graph_model,
                 "si-unit-x", &unitx,
                 "si-unit-y", &unity,
                 NULL);
    unitp = gwy_nlfit_preset_get_param_units(controls->args->fitfunc, i,
                                             unitx, unity);
    g_object_unref(unitx);
    g_object_unref(unity);
    vf = gwy_si_unit_get_format_with_digits(unitp, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            arg->value, 4, NULL);

    g_snprintf(buf, sizeof(buf), "%.*f",
               vf->precision, arg->value/vf->magnitude);
    fix_minus(buf, sizeof(buf));
    gtk_label_set_text(GTK_LABEL(cntrl->value), buf);
    gtk_label_set_markup(GTK_LABEL(cntrl->value_unit), vf->units);

    if (!errorknown) {
        gtk_label_set_text(GTK_LABEL(cntrl->error), "");
        gtk_label_set_text(GTK_LABEL(cntrl->error_unit), "");
        gwy_si_unit_value_format_free(vf);
        return;
    }

    vf = gwy_si_unit_get_format_with_digits(unitp, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            arg->error, 1, vf);
    g_snprintf(buf, sizeof(buf), "%.*f",
               vf->precision, arg->error/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(cntrl->error), buf);
    gtk_label_set_markup(GTK_LABEL(cntrl->error_unit), vf->units);

    gwy_si_unit_value_format_free(vf);
}

static void
graph_selected(GwySelection* selection,
               gint i,
               FitControls *controls)
{
    FitArgs *args;
    gchar buffer[24];
    gdouble range[2];
    gint nselections;
    gdouble power10;

    g_return_if_fail(i <= 0);

    args = controls->args;
    nselections = gwy_selection_get_data(selection, NULL);
    gwy_selection_get_object(selection, 0, range);

    if (nselections <= 0 || range[0] == range[1])
        fit_get_full_x_range(controls, &args->from, &args->to);
    else {
        args->from = MIN(range[0], range[1]);
        args->to = MAX(range[0], range[1]);
    }
    controls->in_update = TRUE;
    power10 = pow10(args->abscissa_vf->precision);
    g_snprintf(buffer, sizeof(buffer), "%.*f",
               args->abscissa_vf->precision,
               floor(args->from*power10/args->abscissa_vf->magnitude)/power10);
    gtk_entry_set_text(GTK_ENTRY(controls->from), buffer);
    g_snprintf(buffer, sizeof(buffer), "%.*f",
               args->abscissa_vf->precision,
               ceil(args->to*power10/args->abscissa_vf->magnitude)/power10);
    gtk_entry_set_text(GTK_ENTRY(controls->to), buffer);
    controls->in_update = FALSE;

    fit_set_state(controls, FALSE, FALSE);
}

static void
param_initial_activate(GtkWidget *entry,
                       gpointer user_data)
{
    FitControls *controls = (FitControls*)user_data;
    gint i;

    i = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(entry), "id")) - 1;
    controls->args->param[i].init = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
    fit_set_state(controls, FALSE, TRUE);
    if (controls->args->auto_plot)
        fit_plot_curve(controls->args);
}

static void
range_changed(GtkWidget *entry,
              FitControls *controls)
{
    const gchar *id;
    gdouble *x, newval;

    id = g_object_get_data(G_OBJECT(entry), "id");
    if (gwy_strequal(id, "from"))
        x = &controls->args->from;
    else
        x = &controls->args->to;

    newval = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
    newval *= controls->args->abscissa_vf->magnitude;
    if (newval == *x)
        return;
    *x = newval;

    if (controls->in_update)
        return;

    fit_limit_selection(controls, FALSE);
}

static void
fit_limit_selection(FitControls *controls,
                    gboolean curve_switch)
{
    GwySelection *selection;
    GwyGraphArea *area;
    gdouble xmin, xmax;

    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls->graph)));
    selection = gwy_graph_area_get_selection(area, GWY_GRAPH_STATUS_XSEL);

    if (curve_switch && !gwy_selection_get_data(selection, NULL)) {
        graph_selected(selection, -1, controls);
        return;
    }

    fit_get_full_x_range(controls, &xmin, &xmax);
    controls->args->from = CLAMP(controls->args->from, xmin, xmax);
    controls->args->to = CLAMP(controls->args->to, xmin, xmax);

    if (controls->args->from == xmin && controls->args->to == xmax)
        gwy_selection_clear(selection);
    else {
        gdouble range[2];

        range[0] = controls->args->from;
        range[1] = controls->args->to;
        gwy_selection_set_object(selection, 0, range);
    }
}

static void
fit_get_full_x_range(FitControls *controls,
                     gdouble *xmin,
                     gdouble *xmax)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;

    gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
    gcmodel = gwy_graph_model_get_curve(gmodel, 0);
    gwy_graph_curve_model_get_x_range(gcmodel, xmin, xmax);
}

static void
toggle_changed(GtkToggleButton *button, gboolean *value)
{
    *value = gtk_toggle_button_get_active(button);
}

static void
copy_param(GObject *button,
           FitControls *controls)
{
    gchar buffer[20];
    gint i;

    i = GPOINTER_TO_INT(g_object_get_data(button, "id")) - 1;
    g_snprintf(buffer, sizeof(buffer), "%.4g", controls->args->param[i].value);
    gtk_entry_set_text(GTK_ENTRY(controls->param[i].init), buffer);
    gtk_widget_activate(controls->param[i].init);
}

static GtkWidget*
function_selector_new(GCallback callback,
                      gpointer cbdata,
                      gint current)
{
    GtkCellRenderer *renderer;
    GtkWidget *combo;
    GwyInventoryStore *store;
    gint i;

    store = gwy_inventory_store_new(gwy_nlfit_presets());

    combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 2);
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    i = gwy_inventory_store_get_column_by_name(store, "name");
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), renderer, "text", i);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current);
    g_signal_connect(combo, "changed", callback, cbdata);

    return combo;
}

static GtkWidget*
curve_selector_new(GwyGraphModel *gmodel,
                   GCallback callback,
                   FitControls *controls,
                   gint current)
{
    GwyGraphCurveModel *curve;
    GtkWidget *combo;
    GwyEnum *curves;
    gint ncurves, i;

    ncurves = gwy_graph_model_get_n_curves(gmodel);
    controls->args->fitcolor = *gwy_graph_get_preset_color(ncurves);

    curves = g_new(GwyEnum, ncurves + 1);
    for (i = 0; i < ncurves; i++) {
        curve = gwy_graph_model_get_curve(gmodel, i);
        g_object_get(curve, "description", &curves[i].name, NULL);
        curves[i].value = i;
    }
    curves[ncurves].name = NULL;
    combo = gwy_enum_combo_box_new(curves, ncurves, callback, controls, current,
                                   FALSE);
    g_signal_connect_swapped(combo, "destroy",
                             G_CALLBACK(gwy_enum_freev), curves);

    return combo;
}

/*extract relevant part of data and normalize it to be fitable*/
static gint
normalize_data(FitArgs *args)
{
    gint i, j, ns;
    gboolean skip_first_point = FALSE;
    GwyGraphCurveModel *cmodel;
    const gdouble *xs, *ys;
    const gchar *func_name;
    gdouble *xd, *yd;

    cmodel = gwy_graph_model_get_curve(args->graph_model, 0);
    xs = gwy_graph_curve_model_get_xdata(cmodel);
    ys = gwy_graph_curve_model_get_ydata(cmodel);
    ns = gwy_graph_curve_model_get_ndata(cmodel);

    gwy_data_line_resample(args->xdata, ns, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(args->ydata, ns, GWY_INTERPOLATION_NONE);
    xd = gwy_data_line_get_data(args->xdata);
    yd = gwy_data_line_get_data(args->ydata);

    /* FIXME: Unhardcode and fix to actually check the support interval */
    func_name = gwy_resource_get_name(GWY_RESOURCE(args->fitfunc));
    if (gwy_strequal(func_name, "Gaussian (PSDF)")
        || gwy_strequal(func_name, "Power"))
        skip_first_point = TRUE;

    j = 0;
    for (i = 0; i < ns; i++) {
        if (((args->from == args->to)
             || (xs[i] >= args->from && xs[i] <= args->to))
            && !(skip_first_point && i == 0)) {

            xd[j] = xs[i];
            yd[j] = ys[i];
            j++;
        }
    }
    if (j == 0)
        return 0;

    if (j < ns) {
        gwy_data_line_resize(args->xdata, 0, j);
        gwy_data_line_resize(args->ydata, 0, j);
    }

    return j;
}

static const gchar preset_key[]        = "/module/graph_fit/preset";
static const gchar auto_estimate_key[] = "/module/graph_fit/auto_estimate";
static const gchar auto_plot_key[]     = "/module/graph_fit/auto_plot";

static void
load_args(GwyContainer *container,
          FitArgs *args)
{
    static const guchar *preset;

    if (gwy_container_gis_string_by_name(container, preset_key, &preset)) {
        args->function_type
            = gwy_inventory_get_item_position(gwy_nlfit_presets(),
                                              (const gchar*)preset);
        args->function_type = MAX(args->function_type, 0);
    }
    gwy_container_gis_boolean_by_name(container, auto_estimate_key,
                                      &args->auto_estimate);
    gwy_container_gis_boolean_by_name(container, auto_plot_key,
                                      &args->auto_plot);
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
    gwy_container_set_boolean_by_name(container, auto_estimate_key,
                                      args->auto_estimate);
    gwy_container_set_boolean_by_name(container, auto_plot_key,
                                      args->auto_plot);
}

/************************* fit report *****************************/
static gint
count_really_fitted_points(FitArgs *args)
{
    gint i, n;
    GwyGraphCurveModel *cmodel;
    const gdouble *xs, *ys;
    gint ns;

    n = 0;
    cmodel = gwy_graph_model_get_curve(args->graph_model, 0);
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

static GString*
create_fit_report(FitArgs *args)
{
    GString *report;
    GwyGraphCurveModel *gcmodel;
    GwySIUnit *unitx, *unity, *unitp;
    gchar *s, *unitstr;
    gint i, j, n;

    report = g_string_new(NULL);
    g_return_val_if_fail(args->fitter->covar, report);

    gcmodel = gwy_graph_model_get_curve(args->graph_model, 0);
    g_string_append(report, _("===== Fit Results ====="));
    g_string_append_c(report, '\n');

    g_object_get(gcmodel, "description", &s, NULL);
    g_string_append_printf(report, _("Data:             %s\n"), s);
    g_free(s);

    g_string_append_printf(report, _("Number of points: %d of %d\n"),
                           count_really_fitted_points(args),
                           gwy_graph_curve_model_get_ndata(gcmodel));

    g_string_append_printf(report, _("X range:          %.*f to %.*f %s\n"),
                           args->abscissa_vf->precision,
                           args->from/args->abscissa_vf->magnitude,
                           args->abscissa_vf->precision,
                           args->to/args->abscissa_vf->magnitude,
                           args->abscissa_vf->units);
    g_string_append_printf(report, _("Fitted function:  %s\n"),
                           gwy_resource_get_name(GWY_RESOURCE(args->fitfunc)));
    g_string_append_c(report, '\n');
    g_string_append_printf(report, _("Results\n"));
    n = gwy_nlfit_preset_get_nparams(args->fitfunc);
    g_object_get(args->graph_model,
                 "si-unit-x", &unitx,
                 "si-unit-y", &unity,
                 NULL);
    for (i = 0; i < n; i++) {
        const gchar *name;

        name = gwy_nlfit_preset_get_param_name(args->fitfunc, i);
        if (!pango_parse_markup(name, -1, 0, NULL, &s, NULL, NULL)) {
            g_warning("Parameter name is not valid Pango markup");
            s = g_strdup(name);
        }
        unitp = gwy_nlfit_preset_get_param_units(args->fitfunc, i,
                                                 unitx, unity);
        unitstr = gwy_si_unit_get_string(unitp, GWY_SI_UNIT_FORMAT_PLAIN);
        g_object_unref(unitp);
        g_string_append_printf(report, "%4s = %g ± %g %s\n",
                               s, args->param[i].value, args->param[i].error,
                               unitstr);
        g_free(s);
    }
    g_object_unref(unitx);
    g_object_unref(unity);
    g_string_append_c(report, '\n');
    g_string_append_printf(report, _("Residual sum:   %g\n"),
                           gwy_math_nlfit_get_dispersion(args->fitter));
    g_string_append_c(report, '\n');
    g_string_append_printf(report, _("Correlation matrix\n"));
    for (i = 0; i < n; i++) {
        g_string_append(report, "  ");
        for (j = 0; j <= i; j++) {
            g_string_append_printf
                (report, "% .03f",
                 gwy_math_nlfit_get_correlations(args->fitter, i, j));
            if (j != i)
                g_string_append_c(report, ' ');
        }
        g_string_append_c(report, '\n');
    }

    return report;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
