/*
 *  @(#) $Id: graph_cut.c 8017 2007-05-09 20:44:12Z yeti-dn $
 *  Copyright (C) 2003-2007 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or CUTNESS FOR A PARTICULAR PURPOSE.  See the
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
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-graph.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>



typedef struct {
    gint curve;
    gdouble from;
    gdouble to;
    GwyGraph *parent_graph;
    GwyGraphModel *graph_model;
    GwyRGBA cutcolor;
    GwySIValueFormat *abscissa_vf;
} CutArgs;

typedef struct {
    CutArgs *args;
    GtkWidget *dialog;
    GtkWidget *graph;
    GtkWidget *from;
    GtkWidget *to;
    GtkWidget *curve;
} CutControls;

static gboolean    module_register           (void);
static void        cut                       (GwyGraph *graph);
static void        cut_dialog                (CutArgs *args);
static void        cut_fetch_entry           (CutControls *controls);
static void        curve_changed             (GtkComboBox *combo,
                                              CutControls *controls);
static void        range_changed             (GtkWidget *entry,
                                              CutControls *controls);
static void        cut_limit_selection       (CutControls *controls,
                                              gboolean curve_switch);
static void        cut_get_full_x_range      (CutControls *controls,
                                              gdouble *xmin,
                                              gdouble *xmax);
static void        graph_selected            (GwySelection* selection,
                                              gint i,
                                              CutControls *controls);
static GtkWidget*  curve_selector_new        (GwyGraphModel *gmodel,
                                              GCallback callback,
                                              CutControls *controls,
                                              gint current);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Cut graph with function"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_graph_func_register("graph_cut",
                            (GwyGraphFunc)&cut,
                            N_("/_Cut Graph..."),
                            GWY_STOCK_GRAPH_FUNCTION,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Extract part of graph into new one"));

    return TRUE;
}

static void
cut(GwyGraph *graph)
{
    GwyContainer *settings;
    CutArgs args;

    memset(&args, 0, sizeof(CutArgs));

    args.parent_graph = graph;

    settings = gwy_app_settings_get();
    cut_dialog(&args);
}

static void
cut_dialog(CutArgs *args)
{
    GtkWidget *label, *dialog, *hbox, *hbox2, *table;
    GwyGraphModel *gmodel, *ngmodel;
    GwyGraphArea *area;
    GwyContainer *data;
    GwySelection *selection;
    GwySIUnit *siunit;
    CutControls controls;
    gint response, row;
    gdouble xmin, xmax;

    controls.args = args;

    gmodel = gwy_graph_get_model(GWY_GRAPH(args->parent_graph));
    gwy_graph_model_get_x_range(gmodel, &xmin, &xmax);
    g_object_get(gmodel, "si-unit-x", &siunit, NULL);
    args->abscissa_vf
        = gwy_si_unit_get_format_with_digits(siunit,
                                             GWY_SI_UNIT_FORMAT_VFMARKUP,
                                             MAX(fabs(xmin), fabs(xmax)), 4,
                                             NULL);
    g_object_unref(siunit);

    dialog = gtk_dialog_new_with_buttons(_("Cut Graph"), NULL, 0, NULL);
    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
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

    /* Curve to cut */
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

    /* Cut area */
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

    label = gtk_label_new(gwy_sgettext("range|to"));
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
    graph_selected(selection, -1, &controls);

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        cut_fetch_entry(&controls);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            return;
            break;

            case GTK_RESPONSE_OK:
            ngmodel = gwy_graph_model_duplicate(gmodel);
            gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, NULL);
            gwy_app_data_browser_add_graph_model(ngmodel, data, TRUE);
            g_object_unref(ngmodel);
            gtk_widget_destroy(dialog);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);
}


static void
cut_fetch_entry(CutControls *controls)
{
    GtkWidget *entry;

    entry = gtk_window_get_focus(GTK_WINDOW(controls->dialog));
    if (entry
        && GTK_IS_ENTRY(entry)
        && g_object_get_data(G_OBJECT(entry), "id"))
        gtk_widget_activate(entry);
}


static void
curve_changed(GtkComboBox *combo,
              CutControls *controls)
{
    GwyGraphModel *parent_gmodel, *graph_model;

    controls->args->curve = gwy_enum_combo_box_get_active(combo);
    graph_model = controls->args->graph_model;
    parent_gmodel = gwy_graph_get_model(controls->args->parent_graph);

    gwy_graph_model_remove_all_curves(graph_model);
    gwy_graph_model_add_curve(graph_model,
                              gwy_graph_model_get_curve(parent_gmodel,
                                                        controls->args->curve));
    cut_limit_selection(controls, TRUE);
}


static void
graph_selected(GwySelection* selection,
               gint i,
               CutControls *controls)
{
    CutArgs *args;
    gchar buffer[24];
    gdouble range[2];
    gint nselections;
    gdouble power10;

    g_return_if_fail(i <= 0);

    args = controls->args;
    nselections = gwy_selection_get_data(selection, NULL);
    gwy_selection_get_object(selection, 0, range);

    if (nselections <= 0 || range[0] == range[1])
        cut_get_full_x_range(controls, &args->from, &args->to);
    else {
        args->from = MIN(range[0], range[1]);
        args->to = MAX(range[0], range[1]);
    }
    power10 = pow10(args->abscissa_vf->precision);
    g_snprintf(buffer, sizeof(buffer), "%.*f",
               args->abscissa_vf->precision,
               floor(args->from*power10/args->abscissa_vf->magnitude)/power10);
    gtk_entry_set_text(GTK_ENTRY(controls->from), buffer);
    g_snprintf(buffer, sizeof(buffer), "%.*f",
               args->abscissa_vf->precision,
               ceil(args->to*power10/args->abscissa_vf->magnitude)/power10);
    gtk_entry_set_text(GTK_ENTRY(controls->to), buffer);

}


static void
range_changed(GtkWidget *entry,
              CutControls *controls)
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
    cut_limit_selection(controls, FALSE);
}

static void
cut_limit_selection(CutControls *controls,
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

    cut_get_full_x_range(controls, &xmin, &xmax);
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
cut_get_full_x_range(CutControls *controls,
                     gdouble *xmin,
                     gdouble *xmax)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;

    gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
    gcmodel = gwy_graph_model_get_curve(gmodel, 0);
    gwy_graph_curve_model_get_x_range(gcmodel, xmin, xmax);
}



static GtkWidget*
curve_selector_new(GwyGraphModel *gmodel,
                   GCallback callback,
                   CutControls *controls,
                   gint current)
{
    GwyGraphCurveModel *curve;
    GtkWidget *combo;
    GwyEnum *curves;
    gint ncurves, i;

    ncurves = gwy_graph_model_get_n_curves(gmodel);
    controls->args->cutcolor = *gwy_graph_get_preset_color(ncurves);

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



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
