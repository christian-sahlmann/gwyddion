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
#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/fractals.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define FRACTAL_RUN_MODES \
    (GWY_RUN_MODAL)

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
    GtkWidget *from;
    GtkWidget *to;
    GtkWidget *result;
    GtkWidget *interp;
    GtkWidget *out;
    GtkWidget *graph;
    GtkWidget *results[GWY_FRACTAL_LAST];
    GwyGraphModel *graph_model;
} FractalControls;

static gboolean    module_register            (const gchar *name);
static gboolean    fractal                    (GwyContainer *data,
                                               GwyRunType run);
static gboolean    fractal_dialog             (FractalArgs *args,
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
                                               FractalArgs *args,
                                               GwyContainer *data);
static void        ok_cb                      (FractalArgs *args,
                                               FractalControls *controls,
                                               GwyContainer *data);
static gboolean    update_graph               (FractalArgs *args,
                                               FractalControls *controls,
                                               GwyContainer *data);
static void        fractal_dialog_recompute    (FractalControls *controls,
                                               FractalArgs *args,
                                               GwyContainer *data);
static void        graph_selected             (GwyGraphArea *area,
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

FractalArgs fractal_defaults = {
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

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates fractal dimension using several methods "
       "(partitioning, box counting, triangulation, power spectrum)."),
    "Jindřich Bilek & Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek & Jindřich Bílek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo fractal_func_info = {
        "fractal",
        N_("/_Statistics/_Fractal Dimension..."),
        (GwyProcessFunc)&fractal,
        FRACTAL_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &fractal_func_info);

    return TRUE;
}

static gboolean
fractal(GwyContainer *data, GwyRunType run)
{
    FractalArgs args;
    gboolean ok;

    g_assert(run & FRACTAL_RUN_MODES);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = fractal_defaults;
    else
        fractal_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || fractal_dialog(&args, data);
    if (run == GWY_RUN_MODAL)
        fractal_save_args(data, &args);

    return FALSE;
}

static gboolean
fractal_dialog(FractalArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *hbox, *label, *vbox;

    FractalControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_RECOMPUTE = 2
    };
    gint response, row, i;
    gchar buffer[32];

    controls.args = args;
    controls.data = data;
    dialog = gtk_dialog_new_with_buttons(_("Fractal Dimension"), NULL, 0,
                                         _("Reco_mpute"), RESPONSE_RECOMPUTE,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       TRUE, TRUE, 4);

    /*controls*/
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    table = gtk_table_new(8, 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(interp_changed_cb), args,
                                 args->interp, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.interp);
    gtk_table_attach(GTK_TABLE(table), controls.interp, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 4);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.out = gwy_enum_combo_box_new(methods, G_N_ELEMENTS(methods),
                                          G_CALLBACK(out_changed_cb), &controls,
                                          args->out, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.out);
    gtk_table_attach(GTK_TABLE(table), controls.out, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Fit area</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.from = attach_value_row(table, row, _("From:"), _("minimum"));
    row++;

    controls.to = attach_value_row(table, row, _("To:"), _("maximum"));
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /*results*/

    table = gtk_table_new(5, 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Result</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    for (i = 0; i < GWY_FRACTAL_LAST; i++) {
        g_snprintf(buffer, sizeof(buffer), "%s:", _(methods[i].name));
        controls.results[i] = attach_value_row(table, row, buffer, NULL);
        gtk_label_set_selectable(GTK_LABEL(controls.results[i]), TRUE);
        row++;
    }

    /*graph*/
    controls.graph_model = gwy_graph_model_new();
    controls.graph = gwy_graph_new(controls.graph_model);
    gtk_widget_set_size_request(controls.graph, 400, 300);

    gtk_box_pack_start(GTK_BOX(hbox), controls.graph,
                       TRUE, TRUE, 4);
    gwy_graph_set_status(GWY_GRAPH(controls.graph), GWY_GRAPH_STATUS_XSEL);
    gwy_graph_set_selection_limit(GWY_GRAPH(controls.graph), 1);
    gwy_graph_enable_user_input(GWY_GRAPH(controls.graph), FALSE);
    g_signal_connect(GWY_GRAPH(controls.graph)->area,
                     "selected", G_CALLBACK(graph_selected), &controls);

    gtk_widget_show_all(dialog);
    //fractal_dialog_update(&controls, args, data);

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
            ok_cb(args, &controls, data);
            break;

            case RESPONSE_RESET:
            args->from[args->out] = 0;
            args->to[args->out] = 0;
            gwy_graph_clear_selection(GWY_GRAPH(controls.graph));
            fractal_dialog_update(&controls, args, data);
            break;

            case RESPONSE_RECOMPUTE:
            fractal_dialog_recompute(&controls, args, data);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static GtkWidget*
attach_value_row(GtkWidget *table, gint row,
                 const gchar *description, const gchar *value)
{
    GtkWidget *label;

    label = gtk_label_new(description);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new(value);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    return label;
}

static void
interp_changed_cb(GtkWidget *combo,
                  FractalArgs *args)
{
    args->interp = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

/*callback after changed output type*/
static void
out_changed_cb(GtkWidget *combo,
               FractalControls *controls)
{
    FractalArgs *args;

    g_assert(controls->args && controls->data);
    args = controls->args;
    args->out = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));

    gwy_graph_set_status(GWY_GRAPH(controls->graph), GWY_GRAPH_STATUS_XSEL);
    gwy_graph_clear_selection(GWY_GRAPH(controls->graph));
    fractal_dialog_update(controls, args, controls->data);
    update_labels(controls, args);
}


/*update dialog after any recomputation.*/
static void
fractal_dialog_update(FractalControls *controls,
                      FractalArgs *args,
                      GwyContainer *data)
{
    gchar buffer[16];

    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);

    if (update_graph(args, controls, data)) {
        g_snprintf(buffer, sizeof(buffer), "%2.3g", args->result[args->out]);
        gtk_label_set_text(GTK_LABEL(controls->results[args->out]), buffer);
    }
}

static void
ok_cb(FractalArgs *args,
      FractalControls *controls,
      GwyContainer *data)
{
    GtkWidget *graph;
    GwyDataWindow *data_window;

    graph = gwy_graph_new(controls->graph_model);
    update_graph(args, controls, data);
    data_window = gwy_app_data_window_get_for_data(data);
    gwy_app_graph_window_create(GWY_GRAPH(graph), data);
}
static gboolean
update_graph(FractalArgs *args,
             FractalControls *controls,
             GwyContainer *data)
{
    GwyDataField *dfield;
    GwyDataLine *xline, *yline, *xfit, *yfit, *xnline, *ynline;
    GwyGraphCurveModel *gcmodel;
    gint i, res;
    gboolean is_line;
    gdouble a, b;
    gdouble *xdata, *ydata;

    g_return_val_if_fail(args->out < G_N_ELEMENTS(methods), FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    xline = gwy_data_line_new(10, 10, FALSE);
    yline = gwy_data_line_new(10, 10, FALSE);
    xnline = gwy_data_line_new(10, 10, FALSE);
    ynline = gwy_data_line_new(10, 10, FALSE);

    method_funcs[args->out](dfield, xline, yline, args->interp);
    if ((is_line = remove_datapoints(xline, yline, xnline, ynline, args)))
        args->result[args->out] = dim_funcs[args->out](xnline, ynline, &a, &b);

    gwy_graph_model_remove_all_curves(controls->graph_model);

    gcmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_curve_type(gcmodel, GWY_GRAPH_CURVE_POINTS);
    gwy_graph_curve_model_set_data(gcmodel,
                                   xline->data, yline->data, xline->res);
    gwy_graph_curve_model_set_description(gcmodel,
                                          gettext(methods[args->out].name));
    gwy_graph_model_set_title(controls->graph_model,
                              gettext(methods[args->out].name));
    gwy_graph_model_add_curve(controls->graph_model, gcmodel);

    res = gwy_data_line_get_res(xnline);
    if (is_line) {
        xfit = gwy_data_line_duplicate(xnline);
        yfit = gwy_data_line_new(res, res, FALSE);
        xdata = gwy_data_line_get_data(xfit);
        ydata = gwy_data_line_get_data(yfit);
        for (i = 0; i < res; i++)
            ydata[i] = xdata[i]*a + b;

        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_curve_model_set_data(gcmodel, xfit->data, yfit->data, xfit->res);
        gwy_graph_curve_model_set_curve_type(gcmodel, GWY_GRAPH_CURVE_LINE);
        gwy_graph_curve_model_set_description(gcmodel, _("Linear fit"));
        gwy_graph_model_add_curve(controls->graph_model, gcmodel);
    }
    g_object_unref(xline);
    g_object_unref(yline);
    g_object_unref(xnline);
    g_object_unref(ynline);

    return is_line;
}



/*(re)compute data and dimension and fits*/
static void
fractal_dialog_recompute(FractalControls *controls,
                         FractalArgs *args,
                         GwyContainer *data)
{
    GwyDataField *dfield;

    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    fractal_dialog_update(controls, args, data);
}


/*update data after selecting something in graph*/
static void
graph_selected(GwyGraphArea *area, FractalControls *controls)
{
    FractalArgs *args;
    gdouble from, to;
    gdouble selection[2];

    gwy_graph_get_selection(GWY_GRAPH(controls->graph), selection);

    args = controls->args;
    if (gwy_graph_get_selection_number(GWY_GRAPH(controls->graph)) == 0
        || selection[0] == selection[1]) {
        gtk_label_set_text(GTK_LABEL(controls->from), _("minimum"));
        gtk_label_set_text(GTK_LABEL(controls->to), _("maximum"));
        args->from[args->out] = 0;
        args->to[args->out] = 0;
    }
    else {
        from = selection[0];
        to = selection[1];

        if (from > to)
            GWY_SWAP(gdouble, from, to);

        args->from[args->out] = from;
        args->to[args->out] = to;
        update_labels(controls, args);
    }
}

/*update from and to labels*/
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
        g_snprintf(buffer, sizeof(buffer), "%2.3g", from);
        gtk_label_set_text(GTK_LABEL(controls->from), buffer);
        g_snprintf(buffer, sizeof(buffer), "%2.3g", to);
        gtk_label_set_text(GTK_LABEL(controls->to), buffer);
    }
}

/*remove datapoints that are below or above selection. New data are in
  newxline and newyline and can be directly used for fitting and fractal
  dimension evaluation.*/
static gboolean
remove_datapoints(GwyDataLine *xline, GwyDataLine *yline,
                  GwyDataLine *newxline, GwyDataLine *newyline,
                  FractalArgs *args)
{
    gint i, j;
    gdouble from = 0, to = 0;

    from = args->from[args->out];
    to = args->to[args->out];
    gwy_data_line_resample(newxline, xline->res, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(newyline, yline->res, GWY_INTERPOLATION_NONE);

    if (from == to) {
        gwy_data_line_copy(xline, newxline);
        gwy_data_line_copy(yline, newyline);
        return TRUE;
    }

    j = 0;
    for (i = 0; i < xline->res; i++) {
        if (xline->data[i] >= from && xline->data[i] <= to) {
            newxline->data[j] = xline->data[i];
            newyline->data[j] = yline->data[i];
            j++;
        }
    }
    if (j < 2)
        return FALSE;

    gwy_data_line_resize(newxline, 0, j);
    gwy_data_line_resize(newyline, 0, j);

    return TRUE;
}

static const gchar *interp_key = "/module/fractal/interp";
static const gchar *out_key = "/module/fractal/out";

static void
fractal_sanitize_args(FractalArgs *args)
{
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->out = MIN(args->out, GWY_FRACTAL_PSDF);
}

/*load last used parameters*/
static void
fractal_load_args(GwyContainer *container,
                 FractalArgs *args)
{
    *args = fractal_defaults;

    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, out_key, &args->out);
    fractal_sanitize_args(args);
}

/*save preferences (last used)*/
static void
fractal_save_args(GwyContainer *container,
                 FractalArgs *args)
{
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, out_key, args->out);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
