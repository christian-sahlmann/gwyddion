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
#include <libprocess/peaks.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-graph.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

enum {
    COLUMN_POSITION,
    COLUMN_HEIGHT,
    COLUMN_AREA,
    COLUMN_WIDTH,
};

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
    GwyPeakBackgroundType background;
    GwyPeakOrderType order;
} PeaksArgs;

typedef struct {
    PeaksArgs *args;
    GwyGraphModel *parent_gmodel;
    GtkWidget *dialogue;
    GtkWidget *graph;
    GtkWidget *curve;
    GtkWidget *background;
    GtkWidget *order;
    GtkWidget *peaklist;
    GtkObject *npeaks;
    GArray *peaks;
    GArray *peaks_sorted;
    GwySIValueFormat *vf_x;
    GwySIValueFormat *vf_y;
    GwySIValueFormat *vf_area;
    GwySIValueFormat *vf_w;
} PeaksControls;

static gboolean   module_register     (void);
static void       graph_peaks         (GwyGraph *graph);
static void       graph_peaks_dialogue(GwyGraphModel *parent_gmodel,
                                       PeaksArgs *args);
static GtkWidget* create_peak_list    (PeaksControls *controls);
static GtkWidget* add_aux_button      (GtkWidget *hbox,
                                       const gchar *stock_id,
                                       const gchar *tooltip);
static void       curve_changed       (GtkComboBox *combo,
                                       PeaksControls *controls);
static void       background_changed  (GtkComboBox *combo,
                                       PeaksControls *controls);
static void       order_changed       (GtkComboBox *combo,
                                       PeaksControls *controls);
static void       update_value_formats(PeaksControls *controls);
static void       npeaks_changed      (GtkAdjustment *adj,
                                       PeaksControls *controls);
static void       sort_peaks          (GArray *peaks,
                                       GArray *peaks_sorted,
                                       gint npeaks,
                                       GwyPeakOrderType order);
static void       select_peaks        (PeaksControls *controls);
static void       graph_peaks_save    (PeaksControls *controls);
static void       graph_peaks_copy    (PeaksControls *controls);
static gchar*     format_report       (PeaksControls *controls);
static void       analyse_peaks       (GwyGraphCurveModel *gcmodel,
                                       GArray *peaks,
                                       GwyPeakBackgroundType background);
static void       load_args           (GwyContainer *container,
                                       PeaksArgs *args);
static void       save_args           (GwyContainer *container,
                                       PeaksArgs *args);

static const PeaksArgs peaks_defaults = {
    0, 5, GWY_PEAK_ORDER_ABSCISSA, GWY_PEAK_BACKGROUND_MMSTEP,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Finds peaks on graph curves."),
    "Yeti <yeti@gwyddion.net>",
    "1.2",
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
                            GWY_STOCK_FIND_PEAKS,
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
    static const GwyEnum order_types[] = {
        { N_("Position"),   GWY_PEAK_ORDER_ABSCISSA,   },
        { N_("Prominence"), GWY_PEAK_ORDER_PROMINENCE, },
    };

    static const GwyEnum background_types[] = {
        { N_("Zero"),              GWY_PEAK_BACKGROUND_ZERO,   },
        { N_("Bilateral minimum"), GWY_PEAK_BACKGROUND_MMSTEP, },
    };

    GtkWidget *dialogue, *hbox, *table, *label, *scwin, *hbox2, *button;
    GwyGraphModel *gmodel;
    GwyGraphArea *area;
    PeaksControls controls;
    gint response, row;

    gwy_clear(&controls, 1);
    controls.args = args;
    controls.parent_gmodel = parent_gmodel;
    controls.peaks = g_array_new(FALSE, FALSE, sizeof(Peak));
    controls.peaks_sorted = g_array_new(FALSE, FALSE, sizeof(Peak));
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
    table = gtk_table_new(6, 4, FALSE);
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

    label = gtk_label_new_with_mnemonic(_("_Background type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    controls.background
        = gwy_enum_combo_box_new(background_types,
                                 G_N_ELEMENTS(background_types),
                                 G_CALLBACK(background_changed), &controls,
                                 args->background, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.background);
    gtk_table_attach(GTK_TABLE(table), controls.background,
                     1, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("Order peaks _by:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    controls.order
        = gwy_enum_combo_box_new(order_types, G_N_ELEMENTS(order_types),
                                 G_CALLBACK(order_changed), &controls,
                                 args->order, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.order);
    gtk_table_attach(GTK_TABLE(table), controls.order,
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
    row++;

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table), hbox2,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);

    button = add_aux_button(hbox2, GTK_STOCK_SAVE, _("Save table to a file"));
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(graph_peaks_save), &controls);

    button = add_aux_button(hbox2, GTK_STOCK_COPY, _("Copy table to clipboard"));
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(graph_peaks_copy), &controls);
    row++;

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

    g_array_free(controls.peaks_sorted, TRUE);
    g_array_free(controls.peaks, TRUE);
    gwy_si_unit_value_format_free(controls.vf_x);
    gwy_si_unit_value_format_free(controls.vf_y);
    gwy_si_unit_value_format_free(controls.vf_area);
    gwy_si_unit_value_format_free(controls.vf_w);
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
    GwySIValueFormat *vf;
    const Peak *peak;
    gchar buf[32];
    gdouble v;
    guint i;

    gtk_tree_model_get(model, iter, 0, &i, -1);
    peak = &g_array_index(controls->peaks_sorted, Peak, i);
    if (id == COLUMN_POSITION) {
        vf = controls->vf_x;
        v = peak->x;
    }
    else if (id == COLUMN_HEIGHT) {
        vf = controls->vf_y;
        v = peak->height;
    }
    else if (id == COLUMN_AREA) {
        vf = controls->vf_area;
        v = peak->area;
    }
    else if (id == COLUMN_WIDTH) {
        vf = controls->vf_w;
        v = peak->dispersion;
    }
    else {
        g_return_if_reached();
    }

    g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, v/vf->magnitude);
    g_object_set(renderer, "text", buf, NULL);
}

static void
add_peak_list_column(GtkTreeView *treeview,
                     PeaksControls *controls,
                     gint id)
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkWidget *label;

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_alignment(column, 0.5);

    label = gtk_label_new(NULL);
    gtk_tree_view_column_set_widget(column, label);
    gtk_widget_show(label);
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 1.0, NULL);
    g_object_set_data(G_OBJECT(renderer), "id", GUINT_TO_POINTER(id));
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_peak, controls, NULL);
}

static GtkWidget*
create_peak_list(PeaksControls *controls)
{
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GwyNullStore *store;

    store = gwy_null_store_new(0);
    treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
    g_object_unref(store);

    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

    add_peak_list_column(treeview, controls, COLUMN_POSITION);
    add_peak_list_column(treeview, controls, COLUMN_HEIGHT);
    add_peak_list_column(treeview, controls, COLUMN_AREA);
    add_peak_list_column(treeview, controls, COLUMN_WIDTH);

    return GTK_WIDGET(treeview);
}

static GtkWidget*
add_aux_button(GtkWidget *hbox,
               const gchar *stock_id,
               const gchar *tooltip)
{
    GtkTooltips *tips;
    GtkWidget *button;

    tips = gwy_app_get_tooltips();
    button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_tooltips_set_tip(tips, button, tooltip, NULL);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(stock_id,
                                               GTK_ICON_SIZE_SMALL_TOOLBAR));
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    return button;
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
    analyse_peaks(gcmodel, controls->peaks, args->background);
    update_value_formats(controls);
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
update_value_formats(PeaksControls *controls)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GwySIUnit *xunit, *yunit, *areaunit;
    GtkTreeView *treeview;
    GtkTreeViewColumn *column;
    GtkLabel *label;
    gdouble min, max, xrange, yrange;
    GArray *peaks = controls->peaks;
    gchar *title;
    guint i;

    gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
    gcmodel = gwy_graph_model_get_curve(gmodel, 0);
    g_object_get(gmodel,
                 "si-unit-x", &xunit,
                 "si-unit-y", &yunit,
                 NULL);
    areaunit = gwy_si_unit_multiply(xunit, yunit, NULL);

    gwy_graph_curve_model_get_x_range(gcmodel, &min, &max);
    xrange = max - min;
    controls->vf_x
        = gwy_si_unit_get_format_with_digits(xunit,
                                             GWY_SI_UNIT_FORMAT_MARKUP,
                                             xrange, 4, controls->vf_x);

    max = 0.0;
    for (i = 0; i < peaks->len; i++) {
        const Peak *peak = &g_array_index(peaks, Peak, i);
        if (peak->height > max)
            max = peak->height;
    }
    if (!(max > 0.0)) {
        gwy_graph_curve_model_get_y_range(gcmodel, &min, &max);
        max = 0.4*(max - min);
    }
    yrange = max;

    controls->vf_y
        = gwy_si_unit_get_format_with_digits(yunit,
                                             GWY_SI_UNIT_FORMAT_MARKUP,
                                             yrange, 4, controls->vf_y);

    max = 0.0;
    for (i = 0; i < peaks->len; i++) {
        const Peak *peak = &g_array_index(peaks, Peak, i);
        if (peak->area > max)
            max = peak->area;
    }
    if (!(max > 0.0))
        max = 0.1*xrange*yrange;

    controls->vf_area
        = gwy_si_unit_get_format_with_digits(areaunit,
                                             GWY_SI_UNIT_FORMAT_MARKUP,
                                             0.5*max, 4, controls->vf_area);

    max = 0.0;
    for (i = 0; i < peaks->len; i++) {
        const Peak *peak = &g_array_index(peaks, Peak, i);
        if (peak->dispersion > max)
            max = peak->dispersion;
    }
    if (!(max > 0.0))
        max = 0.05*xrange;

    controls->vf_w
        = gwy_si_unit_get_format_with_digits(xunit,
                                             GWY_SI_UNIT_FORMAT_MARKUP,
                                             max, 3, controls->vf_w);

    g_object_unref(areaunit);
    g_object_unref(yunit);
    g_object_unref(xunit);

    treeview = GTK_TREE_VIEW(controls->peaklist);

    column = gtk_tree_view_get_column(treeview, COLUMN_POSITION);
    title = g_strdup_printf("<b>x</b> [%s]", controls->vf_x->units);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
    gtk_label_set_markup(label, title);
    g_free(title);

    column = gtk_tree_view_get_column(treeview, COLUMN_HEIGHT);
    title = g_strdup_printf("<b>h</b> [%s]", controls->vf_y->units);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
    gtk_label_set_markup(label, title);
    g_free(title);

    column = gtk_tree_view_get_column(treeview, COLUMN_AREA);
    title = g_strdup_printf("<b>A</b> [%s]", controls->vf_area->units);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
    gtk_label_set_markup(label, title);
    g_free(title);

    column = gtk_tree_view_get_column(treeview, COLUMN_WIDTH);
    title = g_strdup_printf("<b>w</b> [%s]", controls->vf_w->units);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
    gtk_label_set_markup(label, title);
    g_free(title);
}

static void
background_changed(GtkComboBox *combo,
                   PeaksControls *controls)
{
    controls->args->background = gwy_enum_combo_box_get_active(combo);
    /* Force recalculation. */
    curve_changed(GTK_COMBO_BOX(controls->curve), controls);
}

static void
order_changed(GtkComboBox *combo,
              PeaksControls *controls)
{
    controls->args->order = gwy_enum_combo_box_get_active(combo);
    select_peaks(controls);
}

static void
npeaks_changed(GtkAdjustment *adj,
               PeaksControls *controls)
{
    controls->args->npeaks = gwy_adjustment_get_int(adj);
    select_peaks(controls);
}

static gint
compare_peak_abscissa(gconstpointer a, gconstpointer b)
{
    const gdouble xa = ((const Peak*)a)->x;
    const gdouble xb = ((const Peak*)b)->x;

    if (xa < xb)
        return -1;
    if (xa > xb)
        return 1;
    return 0;
}

static void
sort_peaks(GArray *peaks, GArray *peaks_sorted, gint npeaks,
           GwyPeakOrderType order)
{
    g_array_set_size(peaks_sorted, 0);
    g_array_append_vals(peaks_sorted, peaks->data, npeaks);
    if (order == GWY_PEAK_ORDER_ABSCISSA)
        g_array_sort(peaks_sorted, compare_peak_abscissa);
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

    sort_peaks(controls->peaks, controls->peaks_sorted, npeaks,
               controls->args->order);

    treeview = GTK_TREE_VIEW(controls->peaklist);
    store = GWY_NULL_STORE(gtk_tree_view_get_model(treeview));
    gwy_null_store_set_n_rows(store, npeaks);

    if (!npeaks)
        return;

    seldata = g_new(gdouble, npeaks);
    for (i = 0; i < npeaks; i++) {
        seldata[i] = g_array_index(peaks, Peak, i).x;
        gwy_null_store_row_changed(store, i);
    }
    gwy_selection_set_data(selection, npeaks, seldata);
    g_free(seldata);
}

static void
graph_peaks_save(PeaksControls *controls)
{
    gchar *text = format_report(controls);

    gwy_save_auxiliary_data(_("Save Peak Parameters"),
                            GTK_WINDOW(controls->dialogue),
                            -1, text);
    g_free(text);
}

static void
graph_peaks_copy(PeaksControls *controls)
{
    GtkClipboard *clipboard;
    GdkDisplay *display;
    gchar *text = format_report(controls);

    display = gtk_widget_get_display(controls->dialogue);
    clipboard = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, text, -1);
    g_free(text);
}

static gchar*
format_report(PeaksControls *controls)
{
    GString *text = g_string_new(NULL);
    GArray *peaks = controls->peaks_sorted;
    GwySIValueFormat *vf_x = controls->vf_x;
    GwySIValueFormat *vf_y = controls->vf_y;
    GwySIValueFormat *vf_area = controls->vf_area;
    GwySIValueFormat *vf_w = controls->vf_w;
    guint i;

    g_string_append_printf(text,
                           "x [%s]\t"
                           "h [%s]\t"
                           "A [%s]\t"
                           "w [%s]\n",
                           vf_x->units,
                           vf_y->units,
                           vf_area->units,
                           vf_w->units);
    for (i = 0; i < peaks->len; i++) {
        const Peak *peak = &g_array_index(peaks, Peak, i);

        g_string_append_printf(text, "%.*f\t",
                               vf_x->precision,
                               peak->x/vf_x->magnitude);
        g_string_append_printf(text, "%.*f\t",
                               vf_y->precision,
                               peak->height/vf_y->magnitude);
        g_string_append_printf(text, "%.*f\t",
                               vf_area->precision,
                               peak->area/vf_area->magnitude);
        g_string_append_printf(text, "%.*f\n",
                               vf_w->precision,
                               peak->dispersion/vf_w->magnitude);
    }

    return g_string_free(text, FALSE);
}

static void
analyse_peaks(GwyGraphCurveModel *gcmodel, GArray *peaks,
              GwyPeakBackgroundType background)
{
    GwyPeaks *analyser;
    gint n, i;

    analyser = gwy_peaks_new();
    gwy_peaks_set_order(analyser, GWY_PEAK_ORDER_PROMINENCE);
    gwy_peaks_set_background(analyser, background);
    /* We want all the peaks because we control the number in the GUI
     * ourselves. */
    n = gwy_peaks_analyze(analyser,
                          gwy_graph_curve_model_get_xdata(gcmodel),
                          gwy_graph_curve_model_get_ydata(gcmodel),
                          gwy_graph_curve_model_get_ndata(gcmodel),
                          G_MAXUINT);
    g_array_set_size(peaks, n);
    if (n) {
        Peak *p = &g_array_index(peaks, Peak, 0);
        gdouble *values = g_new(gdouble, n);

        gwy_peaks_get_quantity(analyser, GWY_PEAK_PROMINENCE, values);
        for (i = 0; i < n; i++)
            p[i].prominence = values[i];

        gwy_peaks_get_quantity(analyser, GWY_PEAK_ABSCISSA, values);
        for (i = 0; i < n; i++)
            p[i].x = values[i];

        gwy_peaks_get_quantity(analyser, GWY_PEAK_HEIGHT, values);
        for (i = 0; i < n; i++)
            p[i].height = values[i];

        gwy_peaks_get_quantity(analyser, GWY_PEAK_AREA, values);
        for (i = 0; i < n; i++)
            p[i].area = values[i];

        gwy_peaks_get_quantity(analyser, GWY_PEAK_WIDTH, values);
        for (i = 0; i < n; i++)
            p[i].dispersion = values[i];

        g_free(values);
    }

    gwy_peaks_free(analyser);
}

static const gchar background_key[] = "/module/graph_peaks/background";
static const gchar npeaks_key[]     = "/module/graph_peaks/npeaks";
static const gchar order_key[]      = "/module/graph_peaks/order";

static void
sanitize_args(PeaksArgs *args)
{
    args->npeaks = CLAMP(args->npeaks, 1, 128);
    args->order = MIN(args->order, GWY_PEAK_ORDER_PROMINENCE);
    args->background = MIN(args->background, GWY_PEAK_BACKGROUND_MMSTEP);
}

static void
load_args(GwyContainer *container,
          PeaksArgs *args)
{
    *args = peaks_defaults;
    gwy_container_gis_int32_by_name(container, npeaks_key, &args->npeaks);
    gwy_container_gis_enum_by_name(container, order_key, &args->order);
    gwy_container_gis_enum_by_name(container, background_key,
                                   &args->background);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          PeaksArgs *args)
{
    gwy_container_set_int32_by_name(container, npeaks_key, args->npeaks);
    gwy_container_set_enum_by_name(container, order_key, args->order);
    gwy_container_set_enum_by_name(container, background_key, args->background);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
