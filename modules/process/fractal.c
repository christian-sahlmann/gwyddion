/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/fractals.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define FRACTAL_RUN_MODES GWY_RUN_INTERACTIVE

typedef enum {
    GWY_FRACTAL_PARTITIONING  = 0,
    GWY_FRACTAL_CUBECOUNTING  = 1,
    GWY_FRACTAL_TRIANGULATION = 2,
    GWY_FRACTAL_PSDF          = 3,
    GWY_FRACTAL_LAST          = 4
} GwyFractalMethod;

typedef void (*FractalMethodFunc)(GwyDataField *data_field,
                                  GwyDataLine *xresult,
                                  GwyDataLine *yresult,
                                  GwyInterpolationType interpolation);
typedef gdouble (*FractalDimFunc)(GwyDataLine *xresult,
                                  GwyDataLine *yresult,
                                  gdouble *a,
                                  gdouble *b);

typedef struct {
    gdouble from[GWY_FRACTAL_LAST];
    gdouble to[GWY_FRACTAL_LAST];
    gdouble result[GWY_FRACTAL_LAST];
    GwyInterpolationType interp;
    GwyFractalMethod out;
} FractalArgs;

typedef struct {
    FractalArgs *args;
    GwyContainer *data;
    GwyDataField *dfield;
    GtkWidget *from;
    GtkWidget *to;
    GtkWidget *result;
    GtkWidget *interp;
    GtkWidget *out;
    GtkWidget *graph;
    GtkWidget *results[GWY_FRACTAL_LAST];
    GwyGraphModel *graph_model;
} FractalControls;

static gboolean    module_register            (void);
static void        fractal                    (GwyContainer *data,
                                               GwyRunType run);
static void        fractal_dialog             (FractalArgs *args,
                                               GwyContainer *data);
static GtkWidget*  attach_value_row           (GtkWidget *table,
                                               gint row,
                                               const gchar *description,
                                               const gchar *value);
static void        interp_changed_cb          (GtkWidget *combo,
                                               FractalArgs *args);
static void        out_changed_cb             (GtkWidget *combo,
                                               FractalControls *controls);
static void        fractal_dialog_update      (FractalControls *controls,
                                               FractalArgs *args);
static void        ok_cb                      (FractalArgs *args,
                                               FractalControls *controls);
static gboolean    update_graph               (FractalArgs *args,
                                               FractalControls *controls);
static void        fractal_dialog_recompute   (FractalControls *controls,
                                               FractalArgs *args);
static void        graph_selected             (GwySelection *selection,
                                               gint hint,
                                               FractalControls *controls);
static gboolean    remove_datapoints          (GwyDataLine *xline,
                                               GwyDataLine *yline,
                                               GwyDataLine *newxline,
                                               GwyDataLine *newyline,
                                               FractalArgs *args);
static void        update_labels              (FractalControls *controls,
                                               FractalArgs *args);
static void        fractal_load_args          (GwyContainer *container,
                                               FractalArgs *args);
static void        fractal_save_args          (GwyContainer *container,
                                               FractalArgs *args);
static void        fractal_sanitize_args      (FractalArgs *args);

static const FractalArgs fractal_defaults = {
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    GWY_INTERPOLATION_BILINEAR,
    GWY_FRACTAL_PARTITIONING,
};

static const GwyEnum methods[] = {
    { N_("Partitioning"),   GWY_FRACTAL_PARTITIONING  },
    { N_("Cube counting"),  GWY_FRACTAL_CUBECOUNTING  },
    { N_("Triangulation"),  GWY_FRACTAL_TRIANGULATION },
    { N_("Power spectrum"), GWY_FRACTAL_PSDF          },
};

static const FractalMethodFunc method_funcs[] = {
    gwy_data_field_fractal_partitioning,
    gwy_data_field_fractal_cubecounting,
    gwy_data_field_fractal_triangulation,
    gwy_data_field_fractal_psdf,
};

static const FractalDimFunc dim_funcs[] = {
    gwy_data_field_fractal_partitioning_dim,
    gwy_data_field_fractal_cubecounting_dim,
    gwy_data_field_fractal_triangulation_dim,
    gwy_data_field_fractal_psdf_dim,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates fractal dimension using several methods "
       "(partitioning, box counting, triangulation, power spectrum)."),
    "Jindřich Bilek & Petr Klapetek <klapetek@gwyddion.net>",
    "1.7",
    "David Nečas (Yeti) & Petr Klapetek & Jindřich Bílek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fractal",
                              (GwyProcessFunc)&fractal,
                              N_("/_Statistics/_Fractal Dimension..."),
                              GWY_STOCK_FRACTAL,
                              FRACTAL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Calculate fractal dimension"));

    return TRUE;
}

static void
fractal(GwyContainer *data, GwyRunType run)
{
    FractalArgs args;

    g_return_if_fail(run & FRACTAL_RUN_MODES);
    fractal_load_args(gwy_app_settings_get(), &args);
    fractal_dialog(&args, data);
    fractal_save_args(gwy_app_settings_get(), &args);
}

static void
fractal_dialog(FractalArgs *args, GwyContainer *data)
{
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_RECOMPUTE = 2
    };

    GtkWidget *dialog, *table, *hbox, *vbox, *label, *button;
    GwyGraphArea *area;
    GwySelection *selection;
    FractalControls controls;
    gint response, row, i;
    GString *str;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &controls.dfield, 0);
    g_return_if_fail(controls.dfield);

    controls.args = args;
    controls.data = data;

    dialog = gtk_dialog_new_with_buttons(_("Fractal Dimension"), NULL, 0, NULL);
    button = gwy_stock_like_button_new(_("Reco_mpute"), GTK_STOCK_EXECUTE);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button,
                                 RESPONSE_RECOMPUTE);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    /* Parameters */
    table = gtk_table_new(4, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(interp_changed_cb), args,
                                 args->interp, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.interp);
    gtk_table_attach(GTK_TABLE(table), controls.interp,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.out = gwy_enum_combo_box_new(methods, G_N_ELEMENTS(methods),
                                          G_CALLBACK(out_changed_cb), &controls,
                                          args->out, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.out);
    gtk_table_attach(GTK_TABLE(table), controls.out,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    /* Area */
    table = gtk_table_new(3, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gwy_label_new_header(_("Fit Area"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.from = attach_value_row(table, row, _("From:"), _("minimum"));
    row++;

    controls.to = attach_value_row(table, row, _("To:"), _("maximum"));
    row++;

    /* Results */
    table = gtk_table_new(GWY_FRACTAL_LAST + 1, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gwy_label_new_header(_("Result"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    /* FIXME: This is a latinism. */
    str = g_string_new(NULL);
    for (i = 0; i < GWY_FRACTAL_LAST; i++) {
        g_string_assign(str, _(methods[i].name));
        g_string_append_c(str, ':');
        controls.results[i] = attach_value_row(table, row, str->str, NULL);
        gtk_label_set_selectable(GTK_LABEL(controls.results[i]), TRUE);
        row++;
    }
    g_string_free(str, TRUE);

    /* Graph */
    controls.graph_model = gwy_graph_model_new();
    controls.graph = gwy_graph_new(controls.graph_model);
    g_object_unref(controls.graph_model);
    gtk_widget_set_size_request(controls.graph, 400, 300);

    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, TRUE, TRUE, 4);
    gwy_graph_set_status(GWY_GRAPH(controls.graph), GWY_GRAPH_STATUS_XSEL);
    gwy_graph_enable_user_input(GWY_GRAPH(controls.graph), FALSE);

    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls.graph)));
    selection = gwy_graph_area_get_selection(area, GWY_GRAPH_STATUS_XSEL);

    gwy_selection_set_max_objects(selection, 1);
    g_signal_connect(selection, "changed",
                     G_CALLBACK(graph_selected), &controls);

    gtk_widget_show_all(dialog);
    fractal_dialog_recompute(&controls, args);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            ok_cb(args, &controls);
            break;

            case RESPONSE_RESET:
            gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls.interp),
                                          args->interp);
            gwy_selection_clear(selection);
            break;

            case RESPONSE_RECOMPUTE:
            fractal_dialog_recompute(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
}

static GtkWidget*
attach_value_row(GtkWidget *table, gint row,
                 const gchar *description, const gchar *value)
{
    GtkWidget *label;

    label = gtk_label_new(description);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(value);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    return label;
}

static void
interp_changed_cb(GtkWidget *combo,
                  FractalArgs *args)
{
    args->interp = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

/* callback after changed output type */
static void
out_changed_cb(GtkWidget *combo,
               FractalControls *controls)
{
    GwyGraphArea *area;
    GwySelection *selection;
    FractalArgs *args;

    g_assert(controls->args && controls->data);
    args = controls->args;
    args->out = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));

    gwy_graph_set_status(GWY_GRAPH(controls->graph), GWY_GRAPH_STATUS_XSEL);

    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls->graph)));
    selection = gwy_graph_area_get_selection(area, GWY_GRAPH_STATUS_XSEL);
    gwy_selection_clear(selection);
    fractal_dialog_update(controls, args);
    update_labels(controls, args);
}


/* update dialog after any recomputation. */
static void
fractal_dialog_update(FractalControls *controls,
                      FractalArgs *args)
{
    gchar buffer[16];

    if (update_graph(args, controls)) {
        g_snprintf(buffer, sizeof(buffer), "%.2f", args->result[args->out]);
        gtk_label_set_text(GTK_LABEL(controls->results[args->out]), buffer);
    }
}

static void
ok_cb(FractalArgs *args,
      FractalControls *controls)
{
    update_graph(args, controls);
    gwy_app_data_browser_add_graph_model(controls->graph_model, controls->data,
                                         TRUE);
}

static gboolean
update_graph(FractalArgs *args,
             FractalControls *controls)
{
    GwyDataLine *xline, *yline, *xfit, *yfit, *xnline, *ynline;
    GwyGraphCurveModel *gcmodel;
    gint i, res;
    gboolean is_line;
    gdouble a, b;
    gdouble *xdata, *ydata;

    g_return_val_if_fail(args->out < G_N_ELEMENTS(methods), FALSE);

    xline = gwy_data_line_new(1, 1.0, FALSE);
    yline = gwy_data_line_new(1, 1.0, FALSE);
    xnline = gwy_data_line_new(1, 1.0, FALSE);
    ynline = gwy_data_line_new(1, 1.0, FALSE);

    method_funcs[args->out](controls->dfield, xline, yline, args->interp);
    if ((is_line = remove_datapoints(xline, yline, xnline, ynline, args)))
        args->result[args->out] = dim_funcs[args->out](xnline, ynline, &a, &b);

    gwy_graph_model_remove_all_curves(controls->graph_model);

    gcmodel = gwy_graph_curve_model_new();
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_POINTS,
                 "description", gettext(methods[args->out].name),
                 NULL);
    gwy_graph_curve_model_set_data(gcmodel,
                                   gwy_data_line_get_data_const(xline),
                                   gwy_data_line_get_data_const(yline),
                                   gwy_data_line_get_res(xline));
    g_object_set(controls->graph_model,
                 "title", gettext(methods[args->out].name),
                 NULL);
    gwy_graph_model_add_curve(controls->graph_model, gcmodel);
    g_object_unref(gcmodel);

    res = gwy_data_line_get_res(xnline);
    if (is_line) {
        xfit = gwy_data_line_duplicate(xnline);
        yfit = gwy_data_line_new(res, res, FALSE);
        xdata = gwy_data_line_get_data(xfit);
        ydata = gwy_data_line_get_data(yfit);
        for (i = 0; i < res; i++)
            ydata[i] = xdata[i]*a + b;

        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_curve_model_set_data(gcmodel,
                                       gwy_data_line_get_data_const(xfit),
                                       gwy_data_line_get_data_const(yfit),
                                       gwy_data_line_get_res(xfit));
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "description", _("Linear fit"),
                     NULL);
        gwy_graph_model_add_curve(controls->graph_model, gcmodel);
        g_object_unref(gcmodel);
        g_object_unref(yfit);
    }
    g_object_unref(xline);
    g_object_unref(yline);
    g_object_unref(xnline);
    g_object_unref(ynline);

    return is_line;
}

/* (re)compute data and dimension and fits */
static void
fractal_dialog_recompute(FractalControls *controls,
                         FractalArgs *args)
{
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
    fractal_dialog_update(controls, args);
}

/* update data after selecting something in graph */
static void
graph_selected(GwySelection *selection, gint hint, FractalControls *controls)
{
    FractalArgs *args;
    gint nselections;
    gdouble sel[2];

    g_return_if_fail(hint <= 0);

    nselections = gwy_selection_get_data(selection, NULL);
    gwy_selection_get_object(selection, 0, sel);

    args = controls->args;
    if (nselections == 0 || sel[0] == sel[1]) {
        gtk_label_set_text(GTK_LABEL(controls->from), _("minimum"));
        gtk_label_set_text(GTK_LABEL(controls->to), _("maximum"));
        args->from[args->out] = 0.0;
        args->to[args->out] = 0.0;
    }
    else {
        if (sel[0] > sel[1])
            GWY_SWAP(gdouble, sel[0], sel[1]);
        args->from[args->out] = sel[0];
        args->to[args->out] = sel[1];
        update_labels(controls, args);
    }
}

/* update from and to labels */
static void
update_labels(FractalControls *controls, FractalArgs *args)
{
    gdouble from = 0, to = 0;
    gchar buffer[16];

    from = args->from[args->out];
    to = args->to[args->out];
    if (from == to) {
        gtk_label_set_text(GTK_LABEL(controls->from), _("minimum"));
        gtk_label_set_text(GTK_LABEL(controls->to), _("maximum"));
    }
    else {
        g_snprintf(buffer, sizeof(buffer), "%.2f", from);
        gtk_label_set_text(GTK_LABEL(controls->from), buffer);
        g_snprintf(buffer, sizeof(buffer), "%.2f", to);
        gtk_label_set_text(GTK_LABEL(controls->to), buffer);
    }
}

/* remove datapoints that are below or above selection. New data are in
   newxline and newyline and can be directly used for fitting and fractal
   dimension evaluation. */
static gboolean
remove_datapoints(GwyDataLine *xline, GwyDataLine *yline,
                  GwyDataLine *newxline, GwyDataLine *newyline,
                  FractalArgs *args)
{
    gint i, j, res;
    const gdouble *xdata, *ydata;
    gdouble *newxdata, *newydata;
    gdouble from = 0, to = 0;

    from = args->from[args->out];
    to = args->to[args->out];
    res = gwy_data_line_get_res(xline);
    g_assert(res == gwy_data_line_get_res(yline));
    gwy_data_line_resample(newxline, res, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(newyline, res, GWY_INTERPOLATION_NONE);

    if (from == to) {
        gwy_data_line_copy(xline, newxline);
        gwy_data_line_copy(yline, newyline);
        return TRUE;
    }

    j = 0;
    xdata = gwy_data_line_get_data_const(xline);
    ydata = gwy_data_line_get_data_const(yline);
    newxdata = gwy_data_line_get_data(newxline);
    newydata = gwy_data_line_get_data(newyline);
    for (i = 0; i < res; i++) {
        if (xdata[i] >= from && xdata[i] <= to) {
            newxdata[j] = xdata[i];
            newydata[j] = ydata[i];
            j++;
        }
    }
    if (j < 2)
        return FALSE;

    gwy_data_line_resize(newxline, 0, j);
    gwy_data_line_resize(newyline, 0, j);

    return TRUE;
}

static const gchar interp_key[] = "/module/fractal/interp";
static const gchar out_key[]    = "/module/fractal/out";

static void
fractal_sanitize_args(FractalArgs *args)
{
    args->interp = gwy_enum_sanitize_value(args->interp,
                                           GWY_TYPE_INTERPOLATION_TYPE);
    args->out = MIN(args->out, GWY_FRACTAL_LAST-1);
}

static void
fractal_load_args(GwyContainer *container,
                 FractalArgs *args)
{
    *args = fractal_defaults;

    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, out_key, &args->out);
    fractal_sanitize_args(args);
}

static void
fractal_save_args(GwyContainer *container,
                 FractalArgs *args)
{
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, out_key, args->out);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
