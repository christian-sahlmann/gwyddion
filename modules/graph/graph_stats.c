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
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwynlfitpreset.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

enum { MAX_PARAMS = 4 };

enum {
    PROFILE_1,
    PROFILE_2,
    PROFILE_3,
    PROFILE_4
};

typedef struct {
    GtkWidget *graph;

    GtkWidget *dialog;
    gulong response_id;
    gulong selection_id;
    gulong destroy_id;

    GtkWidget *selection_start_label;
    GtkWidget *selection_end_label;
    GwyGraphStatusType last_status;

    GtkWidget *area_under_curve_label;
    gdouble area_under_curve;
    GtkWidget *stat_label;
    gdouble stat;

    gint ncurves;
    gint curve_index;
} StatsControls;

static gboolean    module_register             (const gchar *name);
static gboolean    stats                       (GwyGraph *graph);
static gboolean    stats_dialog                (StatsControls *data);
static void        selection_updated_cb        (StatsControls *data);
static void        stats_dialog_closed_cb      (StatsControls *data);
static void        stats_dialog_response_cb    (StatsControls *data);
static gdouble     calc_stats                  (gdouble *y_actual,
                                                gdouble *y_fitted,
                                                gint ndata);
static gdouble     calc_integral_stats         (gdouble *xs,
                                                gdouble *ys,
                                                gint ns);
static gdouble     calc_integral_stats_whole_curve
                                               (StatsControls *data);
static void        fit_graph_cb                (StatsControls *data);
static void        stat_updated_cb             (StatsControls *data);
static void        combo_box_cb                (GtkWidget *combo,
                                                StatsControls *data);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Graph statistics."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_graph_func_register("graph_stats",
                            (GwyGraphFunc)&stats,
                            N_("/_Graph statistics"),
                            NULL,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Calculate graph data statistics"));

    return TRUE;
}

static gboolean
stats(GwyGraph *graph)
{
    StatsControls *data;
    GwyGraphArea *area;
    GwyGraphModel *gmodel;

    data = g_new(StatsControls, 1);
    data->dialog = NULL;

    /*
    if (!graph) {
        if (dialog)
            gtk_widget_destroy(dialog);
        dialog = NULL;
        return TRUE;
    } */

    data->graph = GTK_WIDGET(graph);

    gmodel = gwy_graph_get_model(graph);
    /* data->ncurves used in fit_graph_cb() */
    data->ncurves = gwy_graph_model_get_n_curves(gmodel);

    data->last_status = gwy_graph_get_status(graph);
    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_XSEL);

    area = GWY_GRAPH_AREA(gwy_graph_get_area(graph));
    gwy_graph_area_set_selection_limit(area, 1);

    if (!data->dialog)
        stats_dialog(data);

    return TRUE;
}

static gboolean
stats_dialog(StatsControls *data)
{
    static const GwyEnum profiles1[] = {
        { N_("Profile 1"),  PROFILE_1,  },
    };

    static const GwyEnum profiles2[] = {
        { N_("Profile 1"),  PROFILE_1,  },
        { N_("Profile 2"),  PROFILE_2,  },
    };

    static const GwyEnum profiles3[] = {
        { N_("Profile 1"),  PROFILE_1,  },
        { N_("Profile 2"),  PROFILE_2,  },
        { N_("Profile 3"),  PROFILE_3,  },
    };

    static const GwyEnum profiles4[] = {
        { N_("Profile 1"),  PROFILE_1,  },
        { N_("Profile 2"),  PROFILE_2,  },
        { N_("Profile 3"),  PROFILE_3,  },
        { N_("Profile 4"),  PROFILE_4,  },
    };

    enum {
        RESPONSE_FIT = 0,
    };

    GwyGraph *graph;
    GwyGraphModel *gmodel;
    
    GtkDialog *dialog;
    GtkWidget *table, *label;
    GtkWidget *button;
    gchar buffer[100];
    GwySIValueFormat *format;
    GwySIUnit *si_unit;
    gint i;

    data->curve_index = 0;
    graph = GWY_GRAPH(data->graph);
    gmodel = gwy_graph_get_model(graph);

//     for (i = 0; i < data->ncurves; i++)
//     {
//         curve_desc[i] = 
//     }

    data->dialog = gtk_dialog_new_with_buttons(_("Graph statistics"),
                                               NULL,
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_STOCK_CLOSE,
                                               GTK_RESPONSE_CLOSE,
                                               NULL);
    dialog = GTK_DIALOG(data->dialog);
    gtk_dialog_set_has_separator(dialog, FALSE);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_CLOSE);
    g_signal_connect_swapped(dialog, "delete_event",
                             G_CALLBACK(stats_dialog_closed_cb), data);
    data->response_id =
        g_signal_connect_swapped(dialog, "response",
                                 G_CALLBACK(stats_dialog_response_cb),
                                 data);

    data->destroy_id =
        g_signal_connect_swapped(graph, "destroy", 
                                 G_CALLBACK(stats_dialog_closed_cb),
                                 data);

    table = gtk_table_new(5, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    switch (data->ncurves) {
        case 4:
        label = gwy_enum_combo_box_new(profiles4, 4,
                                       G_CALLBACK(combo_box_cb),
                                       data, 0, FALSE);
        break;

        case 3:
        label = gwy_enum_combo_box_new(profiles3, 3,
                                       G_CALLBACK(combo_box_cb),
                                       data, 0, FALSE);
        break;

        case 2:
        label = gwy_enum_combo_box_new(profiles2, 2,
                                       G_CALLBACK(combo_box_cb),
                                       data, 0, FALSE);
        break;

        default:
        label = gwy_enum_combo_box_new(profiles1, 1,
                                       G_CALLBACK(combo_box_cb),
                                       data, 0, FALSE);
        break;
    }

    gwy_table_attach_row(table, 0, _("Select Curve"), NULL, label);

    label = gtk_label_new("from");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, 0, 0, 2, 2);

    label = gtk_label_new("to");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, 0, 0, 2, 2);

    label = gtk_label_new("Area under curve");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, 0, 0, 2, 2);

    label = gtk_label_new("Std Dev");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5, 0, 0, 2, 2);

    label = gtk_label_new("=");
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, 0, 0, 2, 2);

    label = gtk_label_new("=");
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 3, 4, 0, 0, 2, 2);

    label = gtk_label_new("=");
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, 0, 0, 2, 2);

    label = gtk_label_new("=");
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, 0, 0, 2, 2);

    data->selection_start_label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(data->selection_start_label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), data->selection_start_label, 2, 3, 1, 2,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    data->selection_end_label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(data->selection_end_label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), data->selection_end_label, 2, 3, 2, 3,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    data->area_under_curve = calc_integral_stats_whole_curve(data);
    si_unit = gwy_si_unit_duplicate(gwy_graph_get_model(graph)->x_unit);
    gwy_si_unit_power(si_unit, 2, si_unit);
    format = gwy_si_unit_get_format(si_unit,
                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                    data->area_under_curve, NULL);
    g_snprintf(buffer, sizeof(buffer), "%.3f %s",
               data->area_under_curve/format->magnitude,
               format->units);
    data->area_under_curve_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(data->area_under_curve_label), buffer);

    button = gwy_stock_like_button_new(_("_Fit"), GTK_STOCK_EXECUTE);
    gtk_box_pack_end(GTK_BOX(dialog->action_area), button, FALSE, FALSE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(fit_graph_cb),
                             data);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(stat_updated_cb),
                             data);

    gtk_misc_set_alignment(GTK_MISC(data->area_under_curve_label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table),
                     data->area_under_curve_label,
                     2, 3, 3, 4,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    data->stat_label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(data->stat_label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), data->stat_label, 2, 3, 4, 5,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    data->selection_id =
        g_signal_connect_swapped(graph, "selected",
                                 G_CALLBACK(selection_updated_cb),
                                 data);

    gtk_widget_show_all(GTK_WIDGET(dialog));

    selection_updated_cb(data);
    stat_updated_cb(data);

    g_free(format->units);

    return TRUE;
}

/* callback function for combo box */
static void
combo_box_cb(GtkWidget *combo, StatsControls *data)
{
    gint id;

    id = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    data->curve_index = id;
}

/* code for calculating area under the WHOLE curve */
static gdouble
calc_integral_stats_whole_curve(StatsControls *data)
{
    GwyGraph *graph;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    const gdouble *xs, *ys;
    gint ns, i;
    gdouble sum;

    graph = GWY_GRAPH(data->graph);
    gmodel = gwy_graph_get_model(graph);
    gcmodel = gwy_graph_model_get_curve_by_index(gmodel, 0);
    xs = gwy_graph_curve_model_get_xdata(gcmodel);
    ys = gwy_graph_curve_model_get_ydata(gcmodel);
    ns = gwy_graph_curve_model_get_ndata(gcmodel);

    sum = 0;
    /* to calc initial step size, take i = 1 instead of 0 */
    for (i = 1; i < ns; i++)
        sum = sum + (ys[i] * (xs[i] - xs[i-1]));

    return sum;
}

/* code for calculating area under the selection */
static gdouble
calc_integral_stats(gdouble *xs, gdouble *ys, gint ns)
{
    gdouble sum;
    gint i;

    sum = 0;
    /* to calc initial step size, take i = 1 instead of 0 */
    for (i = 1; i < ns; i++)
        sum = sum + (ys[i] * (xs[i] - xs[i-1]));

    return sum;
}

/* code for calculating statistics (std dev) */
static gdouble
calc_stats(gdouble *y_actual, gdouble *y_fitted, gint ndata)
{
    gdouble stat, sum, avg;
    gint i;

    sum = 0;
    for (i = 0; i < ndata; i++)
        sum = sum + pow((y_actual[i] - y_fitted[i]), 2.0);
    avg = sum/(ndata-1);
    stat = sqrt(avg);

    return stat;
}

/* code for fitting the graphs */
static void
fit_graph_cb(StatsControls *data)
{
    GwyGraph *graph;
    GwyGraphArea *area;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *current_cmodel, *new_cmodel;
    const gdouble *xs, *ys, *xvals;
    GwyDataLine *xdata, *ydata, *y_actual;
    GwyNLFitPreset *fitfunc;
    gint i, j, ns;
    gboolean ok;
    gdouble from, to;
    gdouble selection[2];
    gint count, ndata;
    gint selection_start_index, selection_end_index;
    gint ncurves;

    /* NOTE: these will change depending on the deg of poly */
    const gchar *fit_func_name = "Polynom (order 1)";
    gint deg_of_poly = 1;
    gdouble coeffs[deg_of_poly + 1];
    gint fit_func_index;

    graph = GWY_GRAPH(data->graph);
    gmodel = gwy_graph_get_model(graph);

    xdata = gwy_data_line_new(10, 10, FALSE);
    ydata = gwy_data_line_new(10, 10, FALSE);
    y_actual = gwy_data_line_new(10, 10, FALSE);

    /* normalize data */
    /* FIXME: may put this piece of code in a seperate function */
    current_cmodel = gwy_graph_model_get_curve_by_index(gmodel,
                                                        data->curve_index);
    xvals = gwy_graph_curve_model_get_xdata(current_cmodel);

    /* remove the old fit */
    ncurves = gwy_graph_model_get_n_curves(gmodel);
    if (ncurves > data->ncurves)
    {
        for (i = ncurves-1; i > data->ncurves-1; i--)
            gwy_graph_model_remove_curve_by_index(gmodel, i);
    }

    /* get selection values */
    area = GWY_GRAPH_AREA(gwy_graph_get_area(graph));
    if (gwy_graph_area_get_selection_number(area))
    {
        gwy_graph_area_get_selection(area, selection);
        from = selection[0];
        to = selection[1];
        if (from > to)
            GWY_SWAP(gdouble, from, to);
        ndata = gwy_graph_curve_model_get_ndata(current_cmodel);
        if ((from > xvals[ndata - 1]) && (to > xvals[ndata - 1]))
        {
            from = xvals[0];
            to = xvals[ndata - 1];
        }
    }
    else
    {
        from = xvals[0];
        to = xvals[gwy_graph_curve_model_get_ndata(current_cmodel) - 1];
    }

    xs = gwy_graph_curve_model_get_xdata(current_cmodel);
    ys = gwy_graph_curve_model_get_ydata(current_cmodel);
    ns = gwy_graph_curve_model_get_ndata(current_cmodel);

    gwy_data_line_resample(xdata, ns, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(ydata, ns, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(y_actual, ns, GWY_INTERPOLATION_NONE);

    selection_start_index = 0;
    selection_end_index = ns-1;

    /* modify 'ns' according to selection */
    if ((from > xs[0]) || (to < xs[ns-1]))
    {
        for (i = 0; i < ns; i++)
        {
            /* check if 'from' and xs[i] are approx. equal */
            if (from <= xs[i])
            {
                selection_start_index = i;
                break;
            }
        }
        for (i = 0; i < ns; i++)
        {
            /* check if 'to' and 'xs[i]' are approx. equal */
            if (to <= xs[i])
            {
                selection_end_index = i-1;
                break;
            }
        }

        count = 0;
        for (i = selection_start_index; i <= selection_end_index; i++)
            count = count + 1;
        ns = count;
    }

    j = 0;
    for (i = 0; i < xdata->res; i++)
    {
        if ((xs[i] >= from && xs[i] <= to) || (from == to))
        {
            if (i == 0)
                continue;

            xdata->data[j] = xs[i];
            ydata->data[j] = ys[i];
            y_actual->data[j] = ys[i]; /* will be passed to calc_stats func */
            j++;
        }
    }

    if (j < ns) {
        gwy_data_line_resize(xdata, 0, j);
        gwy_data_line_resize(ydata, 0, j);
    }
    /* end normalize */

    fit_func_index = gwy_inventory_get_item_position(gwy_nlfit_presets(),
                                                     fit_func_name);
    fitfunc = gwy_inventory_get_nth_item(gwy_nlfit_presets(), fit_func_index);
    gwy_math_fit_polynom(ns, xdata->data, ydata->data, deg_of_poly, coeffs);

    data->area_under_curve = calc_integral_stats(xdata->data, ydata->data, ns);

    for (i = 0; i < ns; i++)
        ydata->data[i] = gwy_nlfit_preset_get_value(fitfunc,
                                                    xdata->data[i],
                                                    coeffs,
                                                    &ok);
    new_cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_curve_type(new_cmodel, GWY_GRAPH_CURVE_LINE);
    gwy_graph_curve_model_set_data(new_cmodel,
                                   xdata->data,
                                   ydata->data,
                                   ns);
    gwy_graph_curve_model_set_description(new_cmodel, "fit");
    gwy_graph_model_add_curve(gmodel, new_cmodel);

    data->stat = calc_stats(y_actual->data, ydata->data, ns);

    g_object_unref(new_cmodel);
    g_object_unref(xdata);
    g_object_unref(ydata);
    g_object_unref(y_actual);
}

static void
stat_updated_cb(StatsControls *data)
{
    GwyGraph *graph;
    GwyGraphModel *gmodel;
    gchar buffer[100];
    GwySIValueFormat *format;
    GwySIUnit *si_unit;

    graph = GWY_GRAPH(data->graph);
    g_return_if_fail(GWY_IS_GRAPH(graph));
    gmodel = gwy_graph_get_model(graph);

    if ((gwy_graph_model_get_n_curves(gmodel)) > data->ncurves)
    {
        format = gwy_si_unit_get_format((gwy_graph_get_model(graph))->y_unit,
                                        GWY_SI_UNIT_FORMAT_VFMARKUP,
                                        data->stat, NULL);
        g_snprintf(buffer, sizeof(buffer), "%.3f %s",
                   data->stat/format->magnitude,                        
                   format->units);
        gtk_label_set_markup(GTK_LABEL(data->stat_label), buffer);

        si_unit = gwy_si_unit_duplicate(gwy_graph_get_model(graph)->x_unit);
        gwy_si_unit_power(si_unit, 2, si_unit);
        format = gwy_si_unit_get_format(si_unit,
                                        GWY_SI_UNIT_FORMAT_VFMARKUP,
                                        data->area_under_curve, format);
        g_snprintf(buffer, sizeof(buffer), "%.3f %s",
                   data->area_under_curve/format->magnitude,
                   format->units);
        gtk_label_set_markup(GTK_LABEL(data->area_under_curve_label), buffer);

        g_free(format->units);
    }
}

static void
selection_updated_cb(StatsControls *data)
{
    GwyGraph *graph;
    GwyGraphArea *area;
    gdouble selection[2];
    gdouble from, to;
    gchar buffer[100];
    GwySIValueFormat *format;

    graph = GWY_GRAPH(data->graph);
    g_return_if_fail(GWY_IS_GRAPH(graph));
    area = GWY_GRAPH_AREA(gwy_graph_get_area(graph));

    if (gwy_graph_area_get_selection_number(area))
    {
        gwy_graph_area_get_selection(area, selection);
        from = selection[0];
        to = selection[1];
    }
    else
    {
        from = gwy_graph_get_model(graph)->x_min;
        to = gwy_graph_get_model(graph)->x_max;
    }

    format = gwy_si_unit_get_format((gwy_graph_get_model(graph))->x_unit,
                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                    from, NULL);
    g_snprintf(buffer, sizeof(buffer), "%.3f %s", from/format->magnitude,
               format->units);
    gtk_label_set_markup(GTK_LABEL(data->selection_start_label), buffer);

    format = gwy_si_unit_get_format((gwy_graph_get_model(graph))->x_unit,
                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                    to, format);
    g_snprintf(buffer, sizeof(buffer), "%.3f %s", to/format->magnitude,
               format->units);
    gtk_label_set_markup(GTK_LABEL(data->selection_end_label), buffer);

    g_free(format->units);
}

static void
stats_dialog_closed_cb(StatsControls *data)
{
    GwyGraph *graph;
    graph = GWY_GRAPH(data->graph);

    gwy_graph_set_status(graph, data->last_status);

    if (data->dialog) {
        g_signal_handler_disconnect(data->graph, data->destroy_id);
        g_signal_handler_disconnect(data->dialog, data->response_id);
        g_signal_handler_disconnect(data->graph, data->selection_id);
        data->response_id = 0;
        data->selection_id = 0;
        gtk_widget_destroy(data->dialog);
        data->dialog = NULL;
    }

    g_free(data);
}


static void
stats_dialog_response_cb(StatsControls *data)
{
    stats_dialog_closed_cb(data);
}

/* vim: set cin et ts=4 sw=4
cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
