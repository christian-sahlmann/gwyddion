/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-graph.h>
#include <app/gwyapp.h>

enum {
    COLUMN_POSITION,
    COLUMN_HEIGHT,
    COLUMN_AREA,
    COLUMN_WIDTH,
};

typedef enum {
    PEAK_ORDER_ABSCISSA,
    PEAK_ORDER_PROMINENCE,
    PEAK_ORDER_NTYPES,
} PeaksOrderType;

typedef struct {
    gdouble prominence;
    gdouble x;
    gdouble height;
    gdouble area;
    gdouble dispersion;
    gint i;
} Peak;

typedef struct {
    gint curve;
    gint npeaks;
    PeaksOrderType order;
} PeaksArgs;

typedef struct {
    PeaksArgs *args;
    GwyGraphModel *parent_gmodel;
    GtkWidget *dialogue;
    GtkWidget *graph;
    GtkWidget *curve;
    GtkWidget *peaklist;
    GtkObject *npeaks;
    GArray *peaks;
} PeaksControls;

static gboolean   module_register     (void);
static void       graph_peaks         (GwyGraph *graph);
static void       graph_peaks_dialogue(GwyGraphModel *parent_gmodel,
                                       PeaksArgs *args);
static GtkWidget* create_peak_list    (PeaksControls *controls);
static void       curve_changed       (GtkComboBox *combo,
                                       PeaksControls *controls);
static void       npeaks_changed      (GtkAdjustment *adj,
                                       PeaksControls *controls);
static void       select_peaks        (PeaksControls *controls);
static void       analyse_peaks       (GwyGraphCurveModel *gcmodel,
                                       GArray *peaks);
static void       load_args           (GwyContainer *container,
                                       PeaksArgs *args);
static void       save_args           (GwyContainer *container,
                                       PeaksArgs *args);

static const PeaksArgs peaks_defaults = {
    0, 5, PEAK_ORDER_ABSCISSA,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Finds peaks on graph curves."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_graph_func_register("graph_peaks",
                            (GwyGraphFunc)&graph_peaks,
                            N_("/Find _Peaks..."),
                            NULL,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Find graph curve peaks"));

    return TRUE;
}

static void
graph_peaks(GwyGraph *graph)
{
    PeaksArgs args;
    GwyGraphModel *parent_gmodel;

    parent_gmodel = gwy_graph_get_model(graph);
    load_args(gwy_app_settings_get(), &args);
    graph_peaks_dialogue(parent_gmodel, &args);
    save_args(gwy_app_settings_get(), &args);
}

static void
graph_peaks_dialogue(GwyGraphModel *parent_gmodel, PeaksArgs *args)
{
    GtkWidget *dialogue, *hbox, *table, *label, *scwin;
    GwyGraphModel *gmodel;
    GwyGraphArea *area;
    PeaksControls controls;
    gint response, row;

    controls.args = args;
    controls.parent_gmodel = parent_gmodel;
    controls.peaks = g_array_new(FALSE, FALSE, sizeof(Peak));
    gmodel = gwy_graph_model_new_alike(parent_gmodel);

    dialogue = gtk_dialog_new_with_buttons(_("Graph Peaks"), NULL, 0, NULL);
    controls.dialogue = dialogue;
    gtk_dialog_add_button(GTK_DIALOG(dialogue),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialogue),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gwy_help_add_to_graph_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);
    gtk_dialog_set_default_response(GTK_DIALOG(dialogue), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialogue)->vbox), hbox,
                       TRUE, TRUE, 0);

    /* Parameters */
    table = gtk_table_new(2, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_Graph curve:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    controls.curve
        = gwy_combo_box_graph_curve_new(G_CALLBACK(curve_changed), &controls,
                                        parent_gmodel, args->curve);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.curve);
    gtk_table_attach(GTK_TABLE(table), controls.curve,
                     1, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.npeaks = gtk_adjustment_new(args->npeaks, 1, 128, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("Number of _peaks:"), NULL,
                            controls.npeaks, GWY_HSCALE_DEFAULT);
    g_signal_connect(controls.npeaks, "value-changed",
                     G_CALLBACK(npeaks_changed), &controls);
    row++;

    /* The list */
    controls.peaklist = create_peak_list(&controls);
    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scwin), controls.peaklist);
    gtk_table_attach(GTK_TABLE(table), scwin,
                     0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    /* Graph */
    controls.graph = gwy_graph_new(gmodel);
    g_object_unref(gmodel);
    gtk_widget_set_size_request(controls.graph, 400, 300);

    gwy_graph_enable_user_input(GWY_GRAPH(controls.graph), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, TRUE, TRUE, 0);
    gwy_graph_set_status(GWY_GRAPH(controls.graph), GWY_GRAPH_STATUS_XLINES);

    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls.graph)));
    gwy_graph_area_set_selection_editable(area, FALSE);

    curve_changed(GTK_COMBO_BOX(controls.curve), &controls);

    gtk_widget_show_all(dialogue);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialogue));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialogue);
            return;
            break;

            case GTK_RESPONSE_OK:
            gtk_widget_destroy(dialogue);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    g_array_free(controls.peaks, TRUE);
}

static void
render_peak(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            gpointer user_data)
{
    PeaksControls *controls = (PeaksControls*)user_data;
    gint id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(renderer), "id"));
    const Peak *peak;
    gchar buf[32];
    guint i;

    gtk_tree_model_get(model, iter, 0, &i, -1);
    peak = &g_array_index(controls->peaks, Peak, i);
    if (id == COLUMN_POSITION) {
        g_snprintf(buf, sizeof(buf), "%g", peak->x);
    }
    else if (id == COLUMN_HEIGHT) {
        g_snprintf(buf, sizeof(buf), "%g", peak->height);
    }
    else if (id == COLUMN_AREA) {
        g_snprintf(buf, sizeof(buf), "%g", peak->area);
    }
    else if (id == COLUMN_WIDTH) {
        g_snprintf(buf, sizeof(buf), "%g", peak->dispersion);
    }

    g_object_set(renderer, "text", buf, NULL);
}

static GtkWidget*
create_peak_list(PeaksControls *controls)
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeView *treeview;
    GwyNullStore *store;
    //GwySIUnit *areaunit, *xunit, *yunit;

    store = gwy_null_store_new(0);
    treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
    g_object_unref(store);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "x");
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 1.0, NULL);
    g_object_set_data(G_OBJECT(renderer), "id",
                      GUINT_TO_POINTER(COLUMN_POSITION));
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_peak, controls, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "h");
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 1.0, NULL);
    g_object_set_data(G_OBJECT(renderer), "id",
                      GUINT_TO_POINTER(COLUMN_HEIGHT));
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_peak, controls, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "A");
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 1.0, NULL);
    g_object_set_data(G_OBJECT(renderer), "id", GUINT_TO_POINTER(COLUMN_AREA));
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_peak, controls, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "w");
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 1.0, NULL);
    g_object_set_data(G_OBJECT(renderer), "id", GUINT_TO_POINTER(COLUMN_WIDTH));
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_peak, controls, NULL);

    return GTK_WIDGET(treeview);
}

static void
curve_changed(GtkComboBox *combo,
              PeaksControls *controls)
{
    PeaksArgs *args = controls->args;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GtkTreeView *treeview;
    GwyNullStore *store;

    treeview = GTK_TREE_VIEW(controls->peaklist);
    store = GWY_NULL_STORE(gtk_tree_view_get_model(treeview));
    gwy_null_store_set_n_rows(store, 0);

    args->curve = gwy_enum_combo_box_get_active(combo);
    gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
    gwy_graph_model_remove_all_curves(gmodel);
    gcmodel = gwy_graph_model_get_curve(controls->parent_gmodel, args->curve);
    gwy_graph_model_add_curve(gmodel, gcmodel);
    analyse_peaks(gcmodel, controls->peaks);
    select_peaks(controls);

    /* The adjustment does not clamp the value and we get a CRITICAL error
     * when we do not do this. */
    if (controls->args->npeaks > controls->peaks->len) {
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->npeaks),
                                 controls->peaks->len);
    }

    g_object_set(controls->npeaks,
                 "upper", (gdouble)controls->peaks->len,
                 NULL);
}

static void
npeaks_changed(GtkAdjustment *adj,
               PeaksControls *controls)
{
    controls->args->npeaks = gwy_adjustment_get_int(adj);
    select_peaks(controls);
}

static void
select_peaks(PeaksControls *controls)
{
    GwyGraphArea *area;
    GwySelection *selection;
    GArray *peaks = controls->peaks;
    gint npeaks, i;
    gdouble *seldata;
    GtkTreeView *treeview;
    GwyNullStore *store;

    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls->graph)));
    selection = gwy_graph_area_get_selection(area, GWY_GRAPH_STATUS_XLINES);
    npeaks = MIN((gint)peaks->len, controls->args->npeaks);
    gwy_selection_set_max_objects(selection, MAX(npeaks, 1));
    gwy_selection_clear(selection);

    treeview = GTK_TREE_VIEW(controls->peaklist);
    store = GWY_NULL_STORE(gtk_tree_view_get_model(treeview));
    gwy_null_store_set_n_rows(store, npeaks);

    if (!npeaks)
        return;

    seldata = g_new(gdouble, npeaks);
    for (i = 0; i < npeaks; i++)
        seldata[i] = g_array_index(peaks, Peak, i).x;
    gwy_selection_set_data(selection, npeaks, seldata);
    g_free(seldata);
}

static gint
compare_double_descending(gconstpointer a, gconstpointer b)
{
    const double da = *(const double*)a;
    const double db = *(const double*)b;

    if (da > db)
        return -1;
    if (da < db)
        return 1;
    return 0;
}

static void
analyse_peaks(GwyGraphCurveModel *gcmodel, GArray *peaks)
{
    gdouble *ydata_filtered;
    const gdouble *xdata, *ydata;
    gint n, i, k, flatsize;

    g_array_set_size(peaks, 0);
    n = gwy_graph_curve_model_get_ndata(gcmodel);
    xdata = gwy_graph_curve_model_get_xdata(gcmodel);
    ydata = gwy_graph_curve_model_get_ydata(gcmodel);
    ydata_filtered = g_new(gdouble, n);

    /* Perform simple closing first. */
    ydata_filtered[0] = ydata[0];
    for (i = 1; i+1 < n; i++) {
        gdouble y = ydata[i];
        gdouble yl = 0.5*(ydata[i+1] + ydata[i-1]);
        ydata_filtered[i] = MAX(y, yl);
    }
    ydata_filtered[n-1] = ydata[n-1];

    /* Find local maxima. */
    flatsize = 0;
    for (i = 1; i+1 < n; i++) {
        gdouble y = ydata_filtered[i];
        gdouble yp = ydata_filtered[i-1];
        gdouble yn = ydata_filtered[i+1];

        /* The normal cases. */
        if (y < yp || y < yn)
            continue;
        if (y > yp && y > yn) {
            Peak peak;
            peak.i = i;
            g_array_append_val(peaks, peak);
            continue;
        }

        /* Flat tops. */
        if (y == yn && y > yp)
            flatsize = 0;
        else if (y == yn && y == yp)
            flatsize++;
        else if (y == yp && y > yn) {
            Peak peak;
            peak.i = i - flatsize/2;
            g_array_append_val(peaks, peak);
        }
    }

    g_free(ydata_filtered);

    /* Analyse prominence. */
    for (k = 0; k < peaks->len; k++) {
        Peak *peak = &g_array_index(peaks, Peak, k);
        gint ileft, iright;
        gdouble yleft, yright, arealeft, arearight, disp2left, disp2right;

        for (ileft = peak->i - 1;
             ileft && ydata[ileft] == ydata[ileft+1];
             ileft--)
            ;
        while (ileft && ydata[ileft] > ydata[ileft-1])
            ileft--;
        yleft = ydata[ileft];

        for (iright = peak->i + 1;
             iright+1 < n && ydata[iright] == ydata[iright-1];
             iright++)
            ;
        while (iright+1 < n && ydata[iright] > ydata[iright+1])
            iright++;
        yright = ydata[iright];

        arealeft = arearight = 0.0;
        disp2left = disp2right = 0.0;
        peak->x = xdata[peak->i];
        for (i = ileft; i < peak->i; i++) {
            gdouble xl = xdata[i] - peak->x, xr = xdata[i+1] - peak->x,
                    yl = ydata[i] - yleft, yr = ydata[i+1] - yleft;
            arealeft += (xr - xl)*(yl + yr)/2.0;
            disp2left += (xr*xr*xr*(3.0*yr + yl)
                          - xl*xl*xl*(3.0*yl + yr)
                          - xl*xr*(xl + xr)*(yr - yl))/12.0;
        }
        for (i = iright; i > peak->i; i--) {
            gdouble xl = xdata[i-1] - peak->x, xr = xdata[i] - peak->x,
                    yl = ydata[i-1] - yright, yr = ydata[i] - yright;
            arearight += (xr - xl)*(yl + yr)/2.0;
            disp2right += (xr*xr*xr*(3.0*yr + yl)
                           - xl*xl*xl*(3.0*yl + yr)
                           - xl*xr*(xl + xr)*(yr - yl))/12.0;
        }

        peak->area = arealeft + arearight;
        peak->dispersion = sqrt(0.5*(disp2left/arealeft
                                     + disp2right/arearight));
        i = peak->i;
        peak->height = ydata[i] - 0.5*(yleft + yright);
        if (ydata[i] > ydata[i-1] || ydata[i] > ydata[i+1]) {
            gdouble epsp = ydata[i] - ydata[i+1];
            gdouble epsm = ydata[i] - ydata[i-1];
            gdouble dp = xdata[i+1] - xdata[i];
            gdouble dm = xdata[i] - xdata[i-1];
            peak->x += 0.5*(epsm*dp*dp - epsp*dm*dm)/(epsm*dp + epsp*dm);
        }
    }

    for (k = 0; k < peaks->len; k++) {
        Peak *peak = &g_array_index(peaks, Peak, k);
        gdouble xleft = (k > 0
                         ? g_array_index(peaks, Peak, k-1).x
                         : xdata[0]);
        gdouble xright = (k+1 < peaks->len
                          ? g_array_index(peaks, Peak, k+1).x
                          : xdata[n-1]);

        peak->prominence = log(peak->height * peak->area
                               * (xright - peak->x) * (peak->x - xleft));
    }

    g_array_sort(peaks, compare_double_descending);
}

static const gchar npeaks_key[] = "/module/graph_peaks/npeaks";
static const gchar order_key[]  = "/module/graph_peaks/order";

static void
sanitize_args(PeaksArgs *args)
{
    args->npeaks = CLAMP(args->npeaks, 1, 128);
    args->order = MIN(args->order, PEAK_ORDER_NTYPES-1);
}

static void
load_args(GwyContainer *container,
          PeaksArgs *args)
{
    *args = peaks_defaults;
    gwy_container_gis_int32_by_name(container, npeaks_key, &args->npeaks);
    gwy_container_gis_enum_by_name(container, order_key, &args->order);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          PeaksArgs *args)
{
    gwy_container_set_int32_by_name(container, npeaks_key, args->npeaks);
    gwy_container_set_enum_by_name(container, order_key, args->order);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
