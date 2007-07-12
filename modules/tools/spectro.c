/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2007 Owain Davies, David Necas (Yeti), Petr Klapetek.
 *  E-mail: owain.davies@blueyonder.co.uk, yeti@gwyddion.net,
 *          klapetek@gwyddion.net.
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
#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/datafield.h>
#include <libprocess/linestats.h>
#include <libprocess/spectra.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_SPECTRO            (gwy_tool_spectro_get_type())
#define GWY_TOOL_SPECTRO(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_SPECTRO, GwyToolSpectro))
#define GWY_IS_TOOL_SPECTRO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_SPECTRO))
#define GWY_TOOL_SPECTRO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_SPECTRO, GwyToolSpectroClass))

enum {
    MIN_RESOLUTION = 4,
    MAX_RESOLUTION = 16384
};

enum {
    COLUMN_I, COLUMN_X, COLUMN_Y, NCOLUMNS
};

typedef struct _GwyToolSpectro      GwyToolSpectro;
typedef struct _GwyToolSpectroClass GwyToolSpectroClass;

typedef struct {
    gboolean options_visible;
    gboolean separate;
} ToolArgs;

struct _GwyToolSpectro {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkTreeView *treeview;
    GtkTreeModel *model;

    GwyDataLine *line;
    GtkWidget *graph;
    GwyGraphModel *gmodel;
    GwySpectra *spectra;

    GtkWidget *options;
    GtkWidget *separate;
    GtkWidget *apply;
    gulong layer_object_chosen_id;
    gboolean ignore_tree_selection;

    /* potential class data */
    GType layer_type;
};

struct _GwyToolSpectroClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType  gwy_tool_spectro_get_type             (void) G_GNUC_CONST;
static void   gwy_tool_spectro_finalize             (GObject *object);
static void   gwy_tool_spectro_init_dialog          (GwyToolSpectro *tool);
static void   gwy_tool_spectro_data_switched        (GwyTool *gwytool,
                                                     GwyDataView *data_view);
static void   gwy_tool_spectro_spectra_switched     (GwyTool *gwytool,
                                                     GwySpectra *spectra);
static void   gwy_tool_spectro_response             (GwyTool *tool,
                                                     gint response_id);
static void   gwy_tool_spectro_tree_sel_changed     (GtkTreeSelection *selection,
                                                     gpointer user_data);
static void   gwy_tool_spectro_object_chosen        (GwyVectorLayer *gwyvectorlayer,
                                                     gint i,
                                                     gpointer *data);
static void   gwy_tool_spectro_show_curve           (GwyToolSpectro *tool,
                                                     gint i);
static void   gwy_tool_spectro_update_header        (GwyToolSpectro *tool,
                                                     guint col,
                                                     GString *str,
                                                     const gchar *title,
                                                     GwySIValueFormat *vf);
static void   gwy_tool_spectro_render_cell          (GtkCellLayout *layout,
                                                     GtkCellRenderer *renderer,
                                                     GtkTreeModel *model,
                                                     GtkTreeIter *iter,
                                                     gpointer user_data);
static void   gwy_tool_spectro_options_expanded     (GtkExpander *expander,
                                                     GParamSpec *pspec,
                                                     GwyToolSpectro *tool);
static void   gwy_tool_spectro_separate_changed     (GtkToggleButton *check,
                                                     GwyToolSpectro *tool);
static void   gwy_tool_spectro_apply                (GwyToolSpectro *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Point Spectrum, extracts point spectra to a graph."),
    "Owain Davies <owain.davies@blueyonder.co.uk>",
    "0.2",
    "Owain Davies, David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

static const gchar options_visible_key[] = "/module/spectro/options_visible";
static const gchar separate_key[]        = "/module/spectro/separate";

static const ToolArgs default_args = {
    FALSE,
    FALSE,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolSpectro, gwy_tool_spectro, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_SPECTRO);

    return TRUE;
}

static void
gwy_tool_spectro_class_init(GwyToolSpectroClass *klass)
{
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_spectro_finalize;

    tool_class->stock_id = GWY_STOCK_SPECTRUM;
    tool_class->title = _("Point Spectroscopy");
    tool_class->tooltip = _("Extract and view point spectroscopy data");
    tool_class->prefix = "/module/spectro";
    tool_class->default_width = 640;
    tool_class->default_height = 400;
    tool_class->data_switched = gwy_tool_spectro_data_switched;
    tool_class->spectra_switched = gwy_tool_spectro_spectra_switched;
    tool_class->response = gwy_tool_spectro_response;
}

static void
gwy_tool_spectro_finalize(GObject *object)
{
    GwyToolSpectro *tool;
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(object);
    tool = GWY_TOOL_SPECTRO(object);

    /* Prevent bad things in the selection callback */
    tool->ignore_tree_selection = TRUE;

    settings = gwy_app_settings_get();
    gwy_container_set_boolean_by_name(settings, options_visible_key,
                                      tool->args.options_visible);
    gwy_container_set_boolean_by_name(settings, separate_key,
                                      tool->args.separate);

    gtk_tree_view_set_model(tool->treeview, NULL);
    gwy_object_unref(tool->model);
    gwy_object_unref(tool->spectra);
    gwy_debug("id: %u", (guint)tool->layer_object_chosen_id);
    gwy_signal_handler_disconnect(plain_tool->layer,
                                  tool->layer_object_chosen_id);

    G_OBJECT_CLASS(gwy_tool_spectro_parent_class)->finalize(object);
}

static void
gwy_tool_spectro_init(GwyToolSpectro *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type = gwy_plain_tool_check_layer_type(plain_tool,
                                                       "GwyLayerPoint");
    if (!tool->layer_type)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;
    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_boolean_by_name(settings, options_visible_key,
                                      &tool->args.options_visible);
    gwy_container_gis_boolean_by_name(settings, separate_key,
                                      &tool->args.separate);

    tool->spectra = NULL;

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type, "spec");
    gwy_tool_spectro_init_dialog(tool);
}

static void
gwy_tool_spectro_init_dialog(GwyToolSpectro *tool)
{
    static const gchar *column_titles[] = {
        "<b>n</b>",
        "<b>x</b>",
        "<b>y</b>",
        "<b>visible</b>",
    };
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkDialog *dialog;
    GtkWidget *scwin, *label, *hbox, *vbox, *hbox2;
    GtkTable *table;
    GtkTreeSelection *select;
    guint i, row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, TRUE, TRUE, 0);

    /* Left pane */
    vbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    /* Point coordinates */
    tool->model = GTK_TREE_MODEL(gwy_null_store_new(0));
    tool->treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(tool->model));

    for (i = 0; i < NCOLUMNS; i++) {
        column = gtk_tree_view_column_new();
        gtk_tree_view_column_set_expand(column, TRUE);
        gtk_tree_view_column_set_alignment(column, 0.5);
        g_object_set_data(G_OBJECT(column), "id", GUINT_TO_POINTER(i));
        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "xalign", 1.0, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                           gwy_tool_spectro_render_cell, tool,
                                           NULL);
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), column_titles[i]);
        gtk_tree_view_column_set_widget(column, label);
        gtk_widget_show(label);
        gtk_tree_view_append_column(tool->treeview, column);
    }

    select = gtk_tree_view_get_selection(GTK_TREE_VIEW (tool->treeview));
    gtk_tree_selection_set_mode(select, GTK_SELECTION_MULTIPLE);
    g_signal_connect(G_OBJECT(select), "changed",
                     G_CALLBACK(gwy_tool_spectro_tree_sel_changed),
                     tool);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scwin), GTK_WIDGET(tool->treeview));
    gtk_box_pack_start(GTK_BOX(vbox), scwin, TRUE, TRUE, 0);

    /*  */

    /* Options */
    tool->options = gtk_expander_new(_("<b>Options</b>"));
    gtk_expander_set_use_markup(GTK_EXPANDER(tool->options), TRUE);
    gtk_expander_set_expanded(GTK_EXPANDER(tool->options),
                              tool->args.options_visible);
    g_signal_connect(tool->options, "notify::expanded",
                     G_CALLBACK(gwy_tool_spectro_options_expanded), tool);
    gtk_box_pack_start(GTK_BOX(vbox), tool->options, FALSE, FALSE, 0);

    table = GTK_TABLE(gtk_table_new(5, 4, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(tool->options), GTK_WIDGET(table));
    row = 0;

    tool->separate
        = gtk_check_button_new_with_mnemonic(_("_Separate spectra"));
    gtk_table_attach(table, tool->separate,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->separate),
                                 tool->args.separate);
    g_signal_connect(tool->separate, "toggled",
                     G_CALLBACK(gwy_tool_spectro_separate_changed), tool);
    row++;

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_table_attach(table, hbox2,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    tool->gmodel = gwy_graph_model_new();
    tool->graph = gwy_graph_new(tool->gmodel);
    g_object_unref(tool->gmodel); /* The GwyGraph takes a ref */

    gwy_graph_enable_user_input(GWY_GRAPH(tool->graph), FALSE);
    g_object_set(tool->gmodel, "label-visible", FALSE, NULL);
    gtk_box_pack_start(GTK_BOX(hbox), tool->graph, TRUE, TRUE, 2);

    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_spectro_data_switched(GwyTool *gwytool,
                               GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolSpectro *tool;
    gboolean ignore;

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    tool = GWY_TOOL_SPECTRO(gwytool);
    ignore = (data_view == plain_tool->data_view);

    if (plain_tool->init_failed)
        return;

    if (!ignore) {
        gwy_debug("disconect obj-chosen handler: %u",
                  (guint)tool->layer_object_chosen_id);
        gwy_signal_handler_disconnect(plain_tool->layer,
                                      tool->layer_object_chosen_id);
    }

    GWY_TOOL_CLASS(gwy_tool_spectro_parent_class)->data_switched(gwytool,
                                                                 data_view);
    if (ignore)
        return;

    if (plain_tool->layer) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type,
                                "editable", FALSE,
                                "focus", -1,
                                NULL);
    }
    if (data_view) {
        tool->layer_object_chosen_id =
                g_signal_connect(G_OBJECT(plain_tool->layer),
                                 "object-chosen",
                                 G_CALLBACK(gwy_tool_spectro_object_chosen),
                                 tool);
    }

    gwy_graph_model_remove_all_curves(tool->gmodel);
}

static void
gwy_tool_spectro_spectra_switched(GwyTool *gwytool,
                                  GwySpectra *spectra)
{
    GwyToolSpectro *tool;
    GwyPlainTool *plain_tool;
    GtkTreeSelection *selection;
    GwyNullStore *store;
    GString *str;
    guint nspec, i;
    gdouble coords[2];

    gwy_debug("spectra: %p", spectra);

    tool = GWY_TOOL_SPECTRO(gwytool);
    plain_tool = GWY_PLAIN_TOOL(gwytool);

    store = GWY_NULL_STORE(tool->model);
    selection = gtk_tree_view_get_selection(tool->treeview);

    if (spectra) {
        if (!plain_tool->data_field)
            g_warning("Spectra made current without any channel?");
        else {
            GwySIUnit *spunit, *fieldunit;

            spunit = gwy_spectra_get_si_unit_xy(spectra);
            fieldunit = gwy_data_field_get_si_unit_xy(plain_tool->data_field);
            if (!gwy_si_unit_equal(spunit, fieldunit)) {
                gwy_debug("Spectra and channel units do not match");
                spectra = NULL;
            }
        }
    }

    if (!spectra) {
        g_object_set(tool->gmodel, "title", _("Spectroscopy"), NULL);
        tool->ignore_tree_selection = TRUE;
        gwy_null_store_set_n_rows(store, 0);
        tool->ignore_tree_selection = FALSE;
        gwy_tool_spectro_tree_sel_changed(selection, tool);
        gwy_object_unref(tool->spectra);
        return;
    }

    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_object_ref(spectra);
    gwy_object_unref(tool->spectra);
    tool->spectra = spectra;

    g_object_set(tool->gmodel,
                 "title", gwy_spectra_get_title(tool->spectra),
                 NULL);

    nspec = gwy_spectra_get_n_spectra(spectra);
    gwy_selection_set_max_objects(plain_tool->selection, nspec);

    /* Prevent treeview selection updates in a for-cycle as the handler
     * fully redraws the graph */
    tool->ignore_tree_selection = TRUE;

    /* Update point layer selection */
    gwy_selection_clear(plain_tool->selection);
    gwy_null_store_set_n_rows(store, 0);
    for (i = 0; i < nspec; i++) {
        gwy_spectra_itoxy(tool->spectra, i, &coords[0], &coords[1]);
        gwy_selection_set_object(plain_tool->selection, i, coords);
    }
    gwy_null_store_set_n_rows(store, nspec);

    /* Update tree view selection */
    gtk_tree_selection_unselect_all(selection);
    for (i = 0; i < nspec; i++) {
        if (gwy_spectra_get_spectrum_selected(tool->spectra, i)) {
            GtkTreeIter iter;

            gtk_tree_model_iter_nth_child(tool->model, &iter, NULL, i);
            gtk_tree_selection_select_iter(selection, &iter);
            gwy_debug("selecting %u", i);
        }
    }

    /* Finally update the selection */
    tool->ignore_tree_selection = FALSE;
    gwy_tool_spectro_tree_sel_changed(selection, tool);

    str = g_string_new(NULL);
    gwy_tool_spectro_update_header(tool, COLUMN_X, str, "x",
                                   plain_tool->coord_format);
    gwy_tool_spectro_update_header(tool, COLUMN_Y, str, "y",
                                   plain_tool->coord_format);
    g_string_free(str, TRUE);
}

static void
gwy_tool_spectro_response(GwyTool *tool,
                          gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_spectro_parent_class)->response(tool, response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_spectro_apply(GWY_TOOL_SPECTRO(tool));
}

static void
gwy_tool_spectro_tree_sel_changed(GtkTreeSelection *selection,
                                  gpointer user_data)
{
    GwyToolSpectro *tool = (GwyToolSpectro*)user_data;
    GtkTreeIter iter;
    guint i, n;

    gwy_debug("ignored: %d", tool->ignore_tree_selection);
    if (tool->ignore_tree_selection)
        return;

    /* FIXME: Inefficient */
    gwy_graph_model_remove_all_curves(tool->gmodel);
    n = gwy_null_store_get_n_rows(GWY_NULL_STORE(tool->model));
    if (!n)
        return;

    g_assert(tool->spectra);
    gtk_tree_model_get_iter_first(tool->model, &iter);
    for (i = 0; i < n; i++) {
        gboolean sel = gtk_tree_selection_iter_is_selected(selection, &iter);

        gwy_debug("i: %u selected: %d", i, sel);
        gwy_spectra_set_spectrum_selected(tool->spectra, i, sel);
        if (sel)
            gwy_tool_spectro_show_curve(tool, i);

        gtk_tree_model_iter_next(tool->model, &iter);
    }
}

static void
gwy_tool_spectro_object_chosen(G_GNUC_UNUSED GwyVectorLayer *gwyvectorlayer,
                               gint i,
                               gpointer *data)
{
    GwyToolSpectro *tool;
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    g_return_if_fail(GWY_IS_TOOL_SPECTRO(data));
    tool = GWY_TOOL_SPECTRO(data);

    if (i < 0)
        return;
    gwy_debug("obj-chosen: %d", i);

    if (gtk_tree_model_iter_nth_child(tool->model, &iter, NULL, i)) {
        selection = gtk_tree_view_get_selection(tool->treeview);
        if (gtk_tree_selection_iter_is_selected(selection, &iter))
            gtk_tree_selection_unselect_iter(selection, &iter);
        else
            gtk_tree_selection_select_iter(selection, &iter);
    }
}

static void
gwy_tool_spectro_show_curve(GwyToolSpectro *tool,
                            gint id)
{
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *gcmodel=NULL;
    gint i, n;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);

    tool->line = gwy_spectra_get_spectrum(tool->spectra, id);
    n = gwy_graph_model_get_n_curves(tool->gmodel);

    for (i = 0; i < n; i++) {
        guint idx;

        gcmodel = gwy_graph_model_get_curve(tool->gmodel, i);
        idx = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(gcmodel), "sid"));
        if (idx == id)
            break;
        else
            gcmodel = NULL;
    }

    if (gcmodel) {
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line, 0, 0);
    } else {
        const GwyRGBA *rgba;
        gchar *desc;

        gcmodel = gwy_graph_curve_model_new();
        g_object_set_data(G_OBJECT(gcmodel), "sid", GUINT_TO_POINTER(id));
        desc = g_strdup_printf("%s %d",
                               gwy_spectra_get_title(tool->spectra), id + 1);
        rgba = gwy_graph_get_preset_color(n);
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "description", desc,
                     "color", rgba,
                     NULL);
        g_free(desc);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line, 0, 0);
        gwy_graph_model_add_curve(tool->gmodel, gcmodel);
        g_object_unref(gcmodel);

        if (n == 0)
            gwy_graph_model_set_units_from_data_line(tool->gmodel, tool->line);
    }
    tool->line = NULL;
}

static void
gwy_tool_spectro_update_header(GwyToolSpectro *tool,
                               guint col,
                               GString *str,
                               const gchar *title,
                               GwySIValueFormat *vf)
{
    GtkTreeViewColumn *column;
    GtkLabel *label;

    column = gtk_tree_view_get_column(tool->treeview, col);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));

    g_string_assign(str, "<b>");
    g_string_append(str, title);
    g_string_append(str, "</b>");
    if (vf)
        g_string_append_printf(str, " [%s]", vf->units);
    gtk_label_set_markup(label, str->str);
}

static void
gwy_tool_spectro_render_cell(GtkCellLayout *layout,
                             GtkCellRenderer *renderer,
                             GtkTreeModel *model,
                             GtkTreeIter *iter,
                             gpointer user_data)
{
    GwyToolSpectro *tool = (GwyToolSpectro*)user_data;
    const GwySIValueFormat *vf;
    gchar buf[48];
    gdouble val;
    guint idx, id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(layout), "id"));
    gtk_tree_model_get(model, iter, 0, &idx, -1);
    if (id == COLUMN_I) {
        g_snprintf(buf, sizeof(buf), "%d", idx + 1);
        g_object_set(renderer, "text", buf, NULL);
        return;
    }

    vf = GWY_PLAIN_TOOL(tool)->coord_format;
    switch (id) {
        case COLUMN_X:
        gwy_spectra_itoxy(tool->spectra, idx, &val, NULL);
        break;

        case COLUMN_Y:
        gwy_spectra_itoxy(tool->spectra, idx, NULL, &val);
        break;

        default:
        g_return_if_reached();
        break;
    }

    if (vf)
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, val/vf->magnitude);
    else
        g_snprintf(buf, sizeof(buf), "%.3g", val);

    g_object_set(renderer, "text", buf, NULL);
}

static void
gwy_tool_spectro_options_expanded(GtkExpander *expander,
                                  G_GNUC_UNUSED GParamSpec *pspec,
                                  GwyToolSpectro *tool)
{
    tool->args.options_visible = gtk_expander_get_expanded(expander);
}

static void
gwy_tool_spectro_separate_changed(GtkToggleButton *check,
                                  GwyToolSpectro *tool)
{
    tool->args.separate = gtk_toggle_button_get_active(check);
}

static void
gwy_tool_spectro_apply(GwyToolSpectro *tool)
{
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *gcmodel;
    GwyGraphModel *gmodel;
    gchar *s;
    gint i, n;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);
    n = gwy_graph_model_get_n_curves(tool->gmodel);
    g_return_if_fail(n);

    if (!tool->args.separate) {
        gmodel = gwy_graph_model_duplicate(tool->gmodel);
        g_object_set(gmodel, "label-visible", TRUE, NULL);
        gwy_app_data_browser_add_graph_model(gmodel, plain_tool->container,
                                             TRUE);
        g_object_unref(gmodel);
        return;
    }

    for (i = 0; i < n; i++) {
        gmodel = gwy_graph_model_new_alike(tool->gmodel);
        g_object_set(gmodel, "label-visible", TRUE, NULL);
        gcmodel = gwy_graph_model_get_curve(tool->gmodel, i);
        gcmodel = gwy_graph_curve_model_duplicate(gcmodel);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
        g_object_get(gcmodel, "description", &s, NULL);
        g_object_set(gmodel, "title", s, NULL);
        g_free(s);
        gwy_app_data_browser_add_graph_model(gmodel, plain_tool->container,
                                             TRUE);
        g_object_unref(gmodel);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

