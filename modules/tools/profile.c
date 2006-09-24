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
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/datafield.h>
#include <libprocess/linestats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_PROFILE            (gwy_tool_profile_get_type())
#define GWY_TOOL_PROFILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_PROFILE, GwyToolProfile))
#define GWY_IS_TOOL_PROFILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_PROFILE))
#define GWY_TOOL_PROFILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_PROFILE, GwyToolProfileClass))

enum {
    NLINES = 18,
    MAX_THICKNESS = 128,
    MIN_RESOLUTION = 4,
    MAX_RESOLUTION = 1024
};

enum {
    COLUMN_I, COLUMN_X1, COLUMN_Y1, COLUMN_X2, COLUMN_Y2, NCOLUMNS
};

typedef struct _GwyToolProfile      GwyToolProfile;
typedef struct _GwyToolProfileClass GwyToolProfileClass;

typedef struct {
    gboolean options_visible;
    gint thickness;
    gint resolution;
    gboolean fixres;
    GwyInterpolationType interpolation;
    gboolean separate;
} ToolArgs;

struct _GwyToolProfile {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkTreeView *treeview;
    GtkTreeModel *model;

    GwyDataLine *line;
    GtkWidget *graph;
    GwyGraphModel *gmodel;

    GtkWidget *options;
    GtkObject *thickness;
    GtkObject *resolution;
    GtkWidget *fixres;
    GtkWidget *interpolation;
    GtkWidget *separate;
    GtkWidget *apply;

    /* potential class data */
    GwySIValueFormat *pixel_format;
    GType layer_type_line;
};

struct _GwyToolProfileClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType  gwy_tool_profile_get_type             (void) G_GNUC_CONST;
static void   gwy_tool_profile_finalize             (GObject *object);
static void   gwy_tool_profile_init_dialog          (GwyToolProfile *tool);
static void   gwy_tool_profile_data_switched        (GwyTool *gwytool,
                                                     GwyDataView *data_view);
static void   gwy_tool_profile_response             (GwyTool *tool,
                                                     gint response_id);
static void   gwy_tool_profile_data_changed         (GwyPlainTool *plain_tool);
static void   gwy_tool_profile_selection_changed    (GwyPlainTool *plain_tool,
                                                     gint hint);
static void   gwy_tool_profile_update_curve         (GwyToolProfile *tool,
                                                     gint i);
static void   gwy_tool_profile_update_all_curves    (GwyToolProfile *tool);
static void   gwy_tool_profile_render_cell          (GtkCellLayout *layout,
                                                     GtkCellRenderer *renderer,
                                                     GtkTreeModel *model,
                                                     GtkTreeIter *iter,
                                                     gpointer user_data);
static void   gwy_tool_profile_options_expanded     (GtkExpander *expander,
                                                     GParamSpec *pspec,
                                                     GwyToolProfile *tool);
static void   gwy_tool_profile_thickness_changed    (GwyToolProfile *tool,
                                                     GtkAdjustment *adj);
static void   gwy_tool_profile_resolution_changed   (GwyToolProfile *tool,
                                                     GtkAdjustment *adj);
static void   gwy_tool_profile_fixres_changed       (GtkToggleButton *check,
                                                     GwyToolProfile *tool);
static void   gwy_tool_profile_separate_changed     (GtkToggleButton *check,
                                                     GwyToolProfile *tool);
static void   gwy_tool_profile_interpolation_changed(GtkComboBox *combo,
                                                     GwyToolProfile *tool);
static void   gwy_tool_profile_apply                (GwyToolProfile *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Profile tool, creates profile graphs from selected lines."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static const gchar fixres_key[]          = "/module/profile/fixres";
static const gchar interpolation_key[]   = "/module/profile/interpolation";
static const gchar options_visible_key[] = "/module/profile/options_visible";
static const gchar resolution_key[]      = "/module/profile/resolution";
static const gchar separate_key[]        = "/module/profile/separate";
static const gchar thickness_key[]       = "/module/profile/thickness";

static const ToolArgs default_args = {
    FALSE,
    1,
    120,
    FALSE,
    GWY_INTERPOLATION_BILINEAR,
    FALSE,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolProfile, gwy_tool_profile, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_PROFILE);

    return TRUE;
}

static void
gwy_tool_profile_class_init(GwyToolProfileClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_profile_finalize;

    tool_class->stock_id = GWY_STOCK_PROFILE;
    tool_class->title = _("Profiles");
    tool_class->tooltip = _("Extract profiles");
    tool_class->prefix = "/module/profile";
    tool_class->default_width = 640;
    tool_class->default_height = 400;
    tool_class->data_switched = gwy_tool_profile_data_switched;
    tool_class->response = gwy_tool_profile_response;

    ptool_class->data_changed = gwy_tool_profile_data_changed;
    ptool_class->selection_changed = gwy_tool_profile_selection_changed;
}

static void
gwy_tool_profile_finalize(GObject *object)
{
    GwyToolProfile *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_PROFILE(object);

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_set_boolean_by_name(settings, options_visible_key,
                                      tool->args.options_visible);
    gwy_container_set_int32_by_name(settings, thickness_key,
                                    tool->args.thickness);
    gwy_container_set_int32_by_name(settings, resolution_key,
                                    tool->args.resolution);
    gwy_container_set_boolean_by_name(settings, fixres_key,
                                      tool->args.fixres);
    gwy_container_set_enum_by_name(settings, interpolation_key,
                                   tool->args.interpolation);
    gwy_container_set_boolean_by_name(settings, separate_key,
                                      tool->args.separate);

    gwy_object_unref(tool->line);
    if (tool->model) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        gwy_object_unref(tool->model);
    }
    if (tool->pixel_format)
        gwy_si_unit_value_format_free(tool->pixel_format);

    G_OBJECT_CLASS(gwy_tool_profile_parent_class)->finalize(object);
}

static void
gwy_tool_profile_init(GwyToolProfile *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_line = gwy_plain_tool_check_layer_type(plain_tool,
                                                            "GwyLayerLine");
    if (!tool->layer_type_line)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;
    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_boolean_by_name(settings, options_visible_key,
                                      &tool->args.options_visible);
    gwy_container_gis_int32_by_name(settings, thickness_key,
                                    &tool->args.thickness);
    gwy_container_gis_int32_by_name(settings, resolution_key,
                                    &tool->args.resolution);
    gwy_container_gis_boolean_by_name(settings, fixres_key,
                                      &tool->args.fixres);
    gwy_container_gis_enum_by_name(settings, interpolation_key,
                                   &tool->args.interpolation);
    gwy_container_gis_boolean_by_name(settings, separate_key,
                                      &tool->args.separate);

    tool->pixel_format = g_new0(GwySIValueFormat, 1);
    tool->pixel_format->magnitude = 1.0;
    tool->pixel_format->precision = 0;
    gwy_si_unit_value_format_set_units(tool->pixel_format, "px");

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_line,
                                     "line");

    gwy_tool_profile_init_dialog(tool);
}

static void
gwy_tool_profile_init_dialog(GwyToolProfile *tool)
{
    static const gchar *column_titles[] = {
        "<b>n</b>",
        "<b>x<sub>1</sub></b>",
        "<b>y<sub>1</sub></b>",
        "<b>x<sub>2</sub></b>",
        "<b>y<sub>2</sub></b>",
    };
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkDialog *dialog;
    GtkWidget *scwin, *label, *hbox, *vbox, *hbox2;
    GtkTable *table;
    GwyNullStore *store;
    guint i, row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, TRUE, TRUE, 0);

    /* Left pane */
    vbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    /* Line coordinates */
    store = gwy_null_store_new(0);
    tool->model = GTK_TREE_MODEL(store);
    tool->treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(tool->model));

    for (i = 0; i < NCOLUMNS; i++) {
        column = gtk_tree_view_column_new();
        gtk_tree_view_column_set_expand(column, TRUE);
        gtk_tree_view_column_set_alignment(column, 0.5);
        g_object_set_data(G_OBJECT(column), "id", GUINT_TO_POINTER(i));
        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "xalign", 1.0, NULL);
        if (i == COLUMN_I)
            g_object_set(renderer, "foreground-set", TRUE, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                           gwy_tool_profile_render_cell, tool,
                                           NULL);
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), column_titles[i]);
        gtk_tree_view_column_set_widget(column, label);
        gtk_widget_show(label);
        gtk_tree_view_append_column(tool->treeview, column);
    }

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scwin), GTK_WIDGET(tool->treeview));
    gtk_box_pack_start(GTK_BOX(vbox), scwin, TRUE, TRUE, 0);

    /* Options */
    tool->options = gtk_expander_new(_("<b>Options</b>"));
    gtk_expander_set_use_markup(GTK_EXPANDER(tool->options), TRUE);
    gtk_expander_set_expanded(GTK_EXPANDER(tool->options),
                              tool->args.options_visible);
    g_signal_connect(tool->options, "notify::expanded",
                     G_CALLBACK(gwy_tool_profile_options_expanded), tool);
    gtk_box_pack_start(GTK_BOX(vbox), tool->options, FALSE, FALSE, 0);

    table = GTK_TABLE(gtk_table_new(5, 4, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(tool->options), GTK_WIDGET(table));
    row = 0;

    tool->thickness = gtk_adjustment_new(tool->args.thickness,
                                         1, MAX_THICKNESS, 1, 10, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Thickness:"), NULL,
                            tool->thickness, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(tool->thickness, "value-changed",
                             G_CALLBACK(gwy_tool_profile_thickness_changed),
                             tool);
    row++;

    tool->resolution = gtk_adjustment_new(tool->args.resolution,
                                          MIN_RESOLUTION, MAX_RESOLUTION,
                                          1, 10, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Fix res.:"), NULL,
                            tool->resolution,
                            GWY_HSCALE_CHECK | GWY_HSCALE_SQRT);
    g_signal_connect_swapped(tool->resolution, "value-changed",
                             G_CALLBACK(gwy_tool_profile_resolution_changed),
                             tool);
    tool->fixres = gwy_table_hscale_get_check(tool->resolution);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->fixres),
                                 tool->args.fixres);
    g_signal_connect(tool->fixres, "toggled",
                     G_CALLBACK(gwy_tool_profile_fixres_changed), tool);
    gwy_table_hscale_set_sensitive(tool->resolution, tool->args.fixres);
    row++;

    tool->separate
        = gtk_check_button_new_with_mnemonic(_("_Separate profiles"));
    gtk_table_attach(table, tool->separate,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->separate),
                                 tool->args.separate);
    g_signal_connect(tool->separate, "toggled",
                     G_CALLBACK(gwy_tool_profile_separate_changed), tool);
    row++;

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_table_attach(table, hbox2,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    tool->interpolation = gwy_enum_combo_box_new
                            (gwy_interpolation_type_get_enum(), -1,
                             G_CALLBACK(gwy_tool_profile_interpolation_changed),
                             tool,
                             tool->args.interpolation, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), tool->interpolation);
    gtk_box_pack_end(GTK_BOX(hbox2), tool->interpolation, FALSE, FALSE, 0);
    row++;

    tool->gmodel = gwy_graph_model_new();
    g_object_set(tool->gmodel, "title", _("Profiles"), NULL);

    tool->graph = gwy_graph_new(tool->gmodel);
    gwy_graph_enable_user_input(GWY_GRAPH(tool->graph), FALSE);
    g_object_set(tool->gmodel, "label-visible", FALSE, NULL);
    gtk_box_pack_start(GTK_BOX(hbox), tool->graph, TRUE, TRUE, 2);

    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_profile_data_switched(GwyTool *gwytool,
                               GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolProfile *tool;

    GWY_TOOL_CLASS(gwy_tool_profile_parent_class)->data_switched(gwytool,
                                                                 data_view);

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_PROFILE(gwytool);
    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_line,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, NLINES);
    }

    gwy_graph_model_remove_all_curves(tool->gmodel);
    gwy_tool_profile_update_all_curves(tool);
}

static void
gwy_tool_profile_response(GwyTool *tool,
                          gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_profile_parent_class)->response(tool, response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_profile_apply(GWY_TOOL_PROFILE(tool));
}

static void
gwy_tool_profile_data_changed(GwyPlainTool *plain_tool)
{
    GwyToolProfile *tool;
    GtkWidget *spin;
    gint m;

    tool = GWY_TOOL_PROFILE(plain_tool);
    if (plain_tool->data_field) {
        spin = gwy_table_hscale_get_middle_widget(tool->resolution);
        m = MAX(gwy_data_field_get_xres(plain_tool->data_field),
                gwy_data_field_get_yres(plain_tool->data_field));
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(spin), MIN_RESOLUTION, m);
    }

    gwy_tool_profile_update_all_curves(tool);
}

static void
gwy_tool_profile_selection_changed(GwyPlainTool *plain_tool,
                                   gint hint)
{
    GwyToolProfile *tool;
    GwyNullStore *store;
    gint n;

    tool = GWY_TOOL_PROFILE(plain_tool);
    store = GWY_NULL_STORE(tool->model);
    n = gwy_null_store_get_n_rows(store);
    g_return_if_fail(hint <= n);

    if (hint < 0) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        if (plain_tool->selection)
            n = gwy_selection_get_data(plain_tool->selection, NULL);
        else
            n = 0;
        gwy_null_store_set_n_rows(store, n);
        gtk_tree_view_set_model(tool->treeview, tool->model);
        gwy_graph_model_remove_all_curves(tool->gmodel);
        gwy_tool_profile_update_all_curves(tool);
    }
    else {
        if (hint < n)
            gwy_null_store_row_changed(store, hint);
        else
            gwy_null_store_set_n_rows(store, n+1);
        gwy_tool_profile_update_curve(tool, hint);
        n++;
    }

    gtk_widget_set_sensitive(tool->apply, n > 0);
}

static void
gwy_tool_profile_update_curve(GwyToolProfile *tool,
                              gint i)
{
    const GwyRGBA *rgba;
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *gcmodel;
    gdouble line[4];
    gint xl1, yl1, xl2, yl2;
    gint n, lineres;
    gchar *desc;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);
    g_return_if_fail(gwy_selection_get_object(plain_tool->selection, i, line));

    xl1 = gwy_data_field_rtoj(plain_tool->data_field, line[0]);
    yl1 = gwy_data_field_rtoi(plain_tool->data_field, line[1]);
    xl2 = gwy_data_field_rtoj(plain_tool->data_field, line[2]);
    yl2 = gwy_data_field_rtoi(plain_tool->data_field, line[3]);

    if (!tool->args.fixres) {
        lineres = ROUND(hypot(xl1 - xl2, yl1 - yl2));
        lineres = MAX(lineres, MIN_RESOLUTION);
    }
    else
        lineres = tool->args.resolution;

    tool->line = gwy_data_field_get_profile(plain_tool->data_field, tool->line,
                                            xl1, yl1, xl2, yl2,
                                            lineres,
                                            tool->args.thickness,
                                            tool->args.interpolation);

    n = gwy_graph_model_get_n_curves(tool->gmodel);
    if (i < n) {
        gcmodel = gwy_graph_model_get_curve(tool->gmodel, i);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line, 0, 0);
    }
    else {
        gcmodel = gwy_graph_curve_model_new();
        desc = g_strdup_printf(_("Profile %d"), i+1);
        rgba = gwy_graph_get_preset_color(i);
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "description", desc,
                     "color", rgba,
                     NULL);
        g_free(desc);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line, 0, 0);
        gwy_graph_model_add_curve(tool->gmodel, gcmodel);
        g_object_unref(gcmodel);

        if (i == 0)
            gwy_graph_model_set_units_from_data_line(tool->gmodel, tool->line);
    }
}

static void
gwy_tool_profile_update_all_curves(GwyToolProfile *tool)
{
    GwyPlainTool *plain_tool;
    gint n, i;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (!plain_tool->selection
        || !(n = gwy_selection_get_data(plain_tool->selection, NULL))) {
        gwy_graph_model_remove_all_curves(tool->gmodel);
        return;
    }

    for (i = 0; i < n; i++)
        gwy_tool_profile_update_curve(tool, i);
}

static void
gwy_tool_profile_render_cell(GtkCellLayout *layout,
                             GtkCellRenderer *renderer,
                             GtkTreeModel *model,
                             GtkTreeIter *iter,
                             gpointer user_data)
{
    GwyToolProfile *tool = (GwyToolProfile*)user_data;
    GwyPlainTool *plain_tool;
    const GwySIValueFormat *vf;
    gchar buf[32];
    gdouble line[4];
    gdouble val;
    guint idx, id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(layout), "id"));
    gtk_tree_model_get(model, iter, 0, &idx, -1);
    if (id == COLUMN_I) {
        GwyGraphCurveModel *gcmodel;
        GwyRGBA *rgba;
        GdkColor gdkcolor;

        g_snprintf(buf, sizeof(buf), "%d", idx + 1);
        gcmodel = gwy_graph_model_get_curve(tool->gmodel, idx);
        g_object_get(gcmodel, "color", &rgba, NULL);
        gwy_rgba_to_gdk_color(rgba, &gdkcolor);
        g_object_set(renderer, "foreground-gdk", &gdkcolor, "text", buf, NULL);
        gwy_rgba_free(rgba);
        return;
    }

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_selection_get_object(plain_tool->selection, idx, line);

    vf = tool->pixel_format;
    switch (id) {
        case COLUMN_X1:
        val = gwy_data_field_rtoj(plain_tool->data_field, line[0]);
        break;

        case COLUMN_Y1:
        val = gwy_data_field_rtoi(plain_tool->data_field, line[1]);
        break;

        case COLUMN_X2:
        val = gwy_data_field_rtoj(plain_tool->data_field, line[2]);
        break;

        case COLUMN_Y2:
        val = gwy_data_field_rtoi(plain_tool->data_field, line[3]);
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
gwy_tool_profile_options_expanded(GtkExpander *expander,
                                  G_GNUC_UNUSED GParamSpec *pspec,
                                  GwyToolProfile *tool)
{
    tool->args.options_visible = gtk_expander_get_expanded(expander);
}

static void
gwy_tool_profile_thickness_changed(GwyToolProfile *tool,
                                   GtkAdjustment *adj)
{
    tool->args.thickness = gwy_adjustment_get_int(adj);
    gwy_tool_profile_update_all_curves(tool);
}

static void
gwy_tool_profile_resolution_changed(GwyToolProfile *tool,
                                    GtkAdjustment *adj)
{
    tool->args.resolution = gwy_adjustment_get_int(adj);
    /* Resolution can be changed only when fixres == TRUE */
    gwy_tool_profile_update_all_curves(tool);
}

static void
gwy_tool_profile_fixres_changed(GtkToggleButton *check,
                                GwyToolProfile *tool)
{
    tool->args.fixres = gtk_toggle_button_get_active(check);
    gwy_tool_profile_update_all_curves(tool);
}

static void
gwy_tool_profile_separate_changed(GtkToggleButton *check,
                                  GwyToolProfile *tool)
{
    tool->args.separate = gtk_toggle_button_get_active(check);
}

static void
gwy_tool_profile_interpolation_changed(GtkComboBox *combo,
                                       GwyToolProfile *tool)
{
    tool->args.interpolation = gwy_enum_combo_box_get_active(combo);
    gwy_tool_profile_update_all_curves(tool);
}

static void
gwy_tool_profile_apply(GwyToolProfile *tool)
{
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *gcmodel;
    GwyGraphModel *gmodel;
    gchar *s;
    gint i, n;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);
    n = gwy_selection_get_data(plain_tool->selection, NULL);
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

