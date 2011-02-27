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

#include <stdio.h>
#include "config.h"
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/gwyprocesstypes.h>
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

typedef enum {
    GWY_CC_DISPLAY_NONE = 0, 
    GWY_CC_DISPLAY_X_CORR  = 1,
    GWY_CC_DISPLAY_Y_CORR = 2,
    GWY_CC_DISPLAY_Z_CORR = 3,
    GWY_CC_DISPLAY_X_UNC = 4,
    GWY_CC_DISPLAY_Y_UNC = 5,
    GWY_CC_DISPLAY_Z_UNC = 6,
} GwyCCDisplayType;


enum {
    NLINES = 1024,
    MAX_THICKNESS = 128,
    MIN_RESOLUTION = 4,
    MAX_RESOLUTION = 16384
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
    gboolean export;
    gboolean both;
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
    GtkWidget *menu_display;
    GtkWidget *callabel;
    GtkWidget *export;
    GtkWidget *both;

    GwyDataField *xerr;
    GwyDataField *yerr;
    GwyDataField *zerr;
    GwyDataField *xunc;
    GwyDataField *yunc;
    GwyDataField *zunc;

    /*curves calculated and output*/
    GwyDataLine *line_xerr;
    GwyDataLine *line_yerr;
    GwyDataLine *line_zerr;
    GwyDataLine *line_xunc;
    GwyDataLine *line_yunc;
    GwyDataLine *line_zunc;

    gboolean has_calibration;
    GwyCCDisplayType display_type;

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
//static void   gwy_tool_profile_export_changed     (GtkToggleButton *check,
//                                                     GwyToolProfile *tool);
static void   gwy_tool_profile_both_changed     (GtkToggleButton *check,
                                                     GwyToolProfile *tool);
static void   gwy_tool_profile_interpolation_changed(GtkComboBox *combo,
                                                     GwyToolProfile *tool);
static void   gwy_tool_profile_apply                (GwyToolProfile *tool);
static GtkWidget*   menu_display           (GCallback callback,
                                            gpointer cbdata,
                                            GwyCCDisplayType current);
static void         display_changed        (GtkComboBox *combo,
                                            GwyToolProfile *tool);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Profile tool, creates profile graphs from selected lines."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.12",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static const gchar fixres_key[]          = "/module/profile/fixres";
static const gchar interpolation_key[]   = "/module/profile/interpolation";
static const gchar options_visible_key[] = "/module/profile/options_visible";
static const gchar resolution_key[]      = "/module/profile/resolution";
static const gchar separate_key[]        = "/module/profile/separate";
static const gchar export_key[]          = "/module/profile/export";
static const gchar both_key[]          = "/module/profile/both";
static const gchar thickness_key[]       = "/module/profile/thickness";

static const ToolArgs default_args = {
    FALSE,
    1,
    120,
    FALSE,
    GWY_INTERPOLATION_LINEAR,
    FALSE,
    TRUE,
    TRUE,
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
    gwy_container_set_boolean_by_name(settings, export_key,
                                      tool->args.export);
    gwy_container_set_boolean_by_name(settings, both_key,
                                      tool->args.both);



    gwy_object_unref(tool->line);
    if (tool->model) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        gwy_object_unref(tool->model);
    }
    gwy_object_unref(tool->gmodel);
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
    tool->args.interpolation
        = gwy_enum_sanitize_value(tool->args.interpolation,
                                  GWY_TYPE_INTERPOLATION_TYPE);
    gwy_container_gis_boolean_by_name(settings, separate_key,
                                      &tool->args.separate);
    gwy_container_gis_boolean_by_name(settings, export_key,
                                      &tool->args.export);
    gwy_container_gis_boolean_by_name(settings, both_key,
                                      &tool->args.both);



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
    GtkWidget *scwin, *label, *hbox, *vbox, *hbox2, *hbox3;
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
    gwy_plain_tool_enable_object_deletion(GWY_PLAIN_TOOL(tool), tool->treeview);

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
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Thickness:"), "px",
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

    hbox3 = gtk_hbox_new(FALSE, 6);
    gtk_table_attach(table, hbox3,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    tool->callabel = gtk_label_new_with_mnemonic(_("_Calibration data:"));
    gtk_misc_set_alignment(GTK_MISC(tool->callabel), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox3), tool->callabel, FALSE, FALSE, 0);

    tool->display_type = 0;
    tool->menu_display = menu_display(G_CALLBACK(display_changed),
                                      tool,
                                      tool->display_type);

    gtk_label_set_mnemonic_widget(GTK_LABEL(tool->callabel), tool->menu_display);
    gtk_box_pack_end(GTK_BOX(hbox3), tool->menu_display, FALSE, FALSE, 0);
    row++;

    tool->both
        = gtk_check_button_new_with_mnemonic(_("_Show profile"));
    gtk_table_attach(table, tool->both,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->both),
                                 tool->args.both);
    g_signal_connect(tool->both, "toggled",
                     G_CALLBACK(gwy_tool_profile_both_changed), tool);
    row++;

    /*tool->export
        = gtk_check_button_new_with_mnemonic(_("_Export uncertainties"));
    gtk_table_attach(table, tool->export,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->export),
                                 tool->args.export);
    g_signal_connect(tool->export, "toggled",
                     G_CALLBACK(gwy_tool_profile_export_changed), tool);
    row++;
    */
    tool->args.export = TRUE;


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
    gboolean ignore;
    gchar xekey[24];
    gchar yekey[24];
    gchar zekey[24];
    gchar xukey[24];
    gchar yukey[24];
    gchar zukey[24];

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    ignore = (data_view == plain_tool->data_view);

    GWY_TOOL_CLASS(gwy_tool_profile_parent_class)->data_switched(gwytool,
                                                                 data_view);

    if (ignore || plain_tool->init_failed)
        return;

    tool = GWY_TOOL_PROFILE(gwytool);
    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_line,
                                "thickness", tool->args.thickness,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, NLINES);

        g_snprintf(xekey, sizeof(xekey), "/%d/data/cal_xerr", plain_tool->id);
        g_snprintf(yekey, sizeof(yekey), "/%d/data/cal_yerr", plain_tool->id);
        g_snprintf(zekey, sizeof(zekey), "/%d/data/cal_zerr", plain_tool->id);
        g_snprintf(xukey, sizeof(xukey), "/%d/data/cal_xunc", plain_tool->id);
        g_snprintf(yukey, sizeof(yukey), "/%d/data/cal_yunc", plain_tool->id);
        g_snprintf(zukey, sizeof(zukey), "/%d/data/cal_zunc", plain_tool->id);

        if (gwy_container_gis_object_by_name(plain_tool->container, xekey, &(tool->xerr))
            && gwy_container_gis_object_by_name(plain_tool->container, yekey, &(tool->yerr))
            && gwy_container_gis_object_by_name(plain_tool->container, zekey, &(tool->zerr))
            && gwy_container_gis_object_by_name(plain_tool->container, xukey, &(tool->xunc))
            && gwy_container_gis_object_by_name(plain_tool->container, yukey, &(tool->yunc))
            && gwy_container_gis_object_by_name(plain_tool->container, zukey, &(tool->zunc)))
        {
            tool->has_calibration = TRUE;
            tool->line_xerr = gwy_data_line_new(gwy_data_field_get_xres(plain_tool->data_field), 
                                                gwy_data_field_get_xreal(plain_tool->data_field), 0);
            gtk_widget_show(tool->menu_display);
            gtk_widget_show(tool->callabel);
            gtk_widget_show(tool->both);
            //gtk_widget_show(tool->export);
        } else {
            tool->has_calibration = FALSE;
            gtk_widget_hide(tool->menu_display);
            gtk_widget_hide(tool->callabel);
            gtk_widget_hide(tool->both);
            //gtk_widget_hide(tool->export);

        }
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
    gwy_tool_profile_update_all_curves(GWY_TOOL_PROFILE(plain_tool));
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
gwy_data_line_sum(GwyDataLine *a, GwyDataLine *b)
{
    gint i;
    g_return_if_fail(GWY_IS_DATA_LINE(a));
    g_return_if_fail(GWY_IS_DATA_LINE(b));
    g_return_if_fail(a->res == b->res);

    for (i = 0; i < a->res; i++)
            a->data[i] += b->data[i];
}
static void
gwy_data_line_subtract(GwyDataLine *a, GwyDataLine *b)
{
    gint i;
    g_return_if_fail(GWY_IS_DATA_LINE(a));
    g_return_if_fail(GWY_IS_DATA_LINE(b));
    g_return_if_fail(a->res == b->res);

    for (i = 0; i < a->res; i++)
            a->data[i] -= b->data[i];
}

static void 
add_hidden_curve(GwyToolProfile *tool, GwyDataLine *line, gchar *str, GwyRGBA *color, gboolean hidden)
{
    GwyGraphCurveModel *gcmodel;

    gcmodel = gwy_graph_curve_model_new();
    if (hidden)
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_HIDDEN,
                 "description", str,
                 "color", color,
                 "line-style", GDK_LINE_ON_OFF_DASH,
                 NULL);
    else 
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "description", str,
                 "color", color,
                 "line-style", GDK_LINE_ON_OFF_DASH,
                 NULL);
    gwy_graph_curve_model_set_data_from_dataline(gcmodel, line, 0, 0);
    gwy_graph_model_add_curve(tool->gmodel, gcmodel);
    g_object_unref(gcmodel);

}
 
static void
gwy_tool_profile_update_curve(GwyToolProfile *tool,
                              gint i)
{
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *gcmodel;
    gdouble line[4];
    gint xl1, yl1, xl2, yl2;
    gint n, lineres, multpos;
    gdouble calxratio=1, calyratio=1;
    gchar *desc;
    GwyRGBA *color;
    GwyDataLine *upunc=NULL, *lowunc=NULL;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);
    g_return_if_fail(gwy_selection_get_object(plain_tool->selection, i, line));

    if (tool->has_calibration) {
        calxratio = ((gdouble)gwy_data_field_get_xres(tool->xerr))/gwy_data_field_get_xres(plain_tool->data_field);
        calyratio = ((gdouble)gwy_data_field_get_yres(tool->xerr))/gwy_data_field_get_yres(plain_tool->data_field);
    }

    if (tool->has_calibration) multpos = 9;
    else multpos = 1;

    i*=multpos;

    xl1 = floor(gwy_data_field_rtoj(plain_tool->data_field, line[0]));
    yl1 = floor(gwy_data_field_rtoi(plain_tool->data_field, line[1]));
    xl2 = floor(gwy_data_field_rtoj(plain_tool->data_field, line[2]));
    yl2 = floor(gwy_data_field_rtoi(plain_tool->data_field, line[3]));

    if (!tool->args.fixres) {
        lineres = GWY_ROUND(hypot(abs(xl1 - xl2) + 1, abs(yl1 - yl2) + 1));
        lineres = MAX(lineres, MIN_RESOLUTION);
    }
    else
        lineres = tool->args.resolution;

    tool->line = gwy_data_field_get_profile(plain_tool->data_field, tool->line,
                                            xl1, yl1, xl2, yl2,
                                            lineres,
                                            tool->args.thickness,
                                            tool->args.interpolation);

    if (tool->has_calibration) {
        tool->line_xerr = gwy_data_field_get_profile(tool->xerr, tool->line_xerr,
                                                xl1*calxratio, yl1*calyratio, xl2*calxratio, yl2*calyratio,
                                                lineres,
                                                tool->args.thickness,
                                                tool->args.interpolation);
        tool->line_yerr = gwy_data_field_get_profile(tool->yerr, tool->line_yerr,
                                                xl1*calxratio, yl1*calyratio, xl2*calxratio, yl2*calyratio,
                                                lineres,
                                                tool->args.thickness,
                                                tool->args.interpolation);
        tool->line_zerr = gwy_data_field_get_profile(tool->zerr, tool->line_zerr,
                                                xl1*calxratio, yl1*calyratio, xl2*calxratio, yl2*calyratio,
                                                lineres,
                                                tool->args.thickness,
                                                tool->args.interpolation);

        tool->line_xunc = gwy_data_field_get_profile(tool->xunc, tool->line_xunc,
                                                xl1*calxratio, yl1*calyratio, xl2*calxratio, yl2*calyratio,
                                                lineres,
                                                tool->args.thickness,
                                                tool->args.interpolation);
        tool->line_yunc = gwy_data_field_get_profile(tool->yunc, tool->line_yunc,
                                                xl1*calxratio, yl1*calyratio, xl2*calxratio, yl2*calyratio,
                                                lineres,
                                                tool->args.thickness,
                                                tool->args.interpolation);
        tool->line_zunc = gwy_data_field_get_profile(tool->zunc, tool->line_zunc,
                                                xl1*calxratio, yl1*calyratio, xl2*calxratio, yl2*calyratio,
                                                lineres,
                                                tool->args.thickness,
                                                tool->args.interpolation);

        upunc = gwy_data_line_new_alike(tool->line, FALSE);
        gwy_data_line_copy(tool->line, upunc);
        gwy_data_line_sum(upunc, tool->line_xerr); 

        lowunc = gwy_data_line_new_alike(tool->line, FALSE);
        gwy_data_line_copy(tool->line, lowunc);
        gwy_data_line_subtract(lowunc, tool->line_xerr);

    }

    n = gwy_graph_model_get_n_curves(tool->gmodel);
    if (i < n) {
        gcmodel = gwy_graph_model_get_curve(tool->gmodel, i);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line, 0, 0); 

        if (tool->has_calibration) {
            gcmodel = gwy_graph_model_get_curve(tool->gmodel, i+1);
            gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line_xerr, 0, 0);

            gcmodel = gwy_graph_model_get_curve(tool->gmodel, i+2);
            gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line_yerr, 0, 0);

            gcmodel = gwy_graph_model_get_curve(tool->gmodel, i+3);
            gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line_zerr, 0, 0);

            gcmodel = gwy_graph_model_get_curve(tool->gmodel, i+4);
            gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line_xunc, 0, 0);

            gcmodel = gwy_graph_model_get_curve(tool->gmodel, i+5);
            gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line_yunc, 0, 0);

            gcmodel = gwy_graph_model_get_curve(tool->gmodel, i+6);
            gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line_zunc, 0, 0);

            gcmodel = gwy_graph_model_get_curve(tool->gmodel, i+7);
            gwy_graph_curve_model_set_data_from_dataline(gcmodel, upunc, 0, 0);

            gcmodel = gwy_graph_model_get_curve(tool->gmodel, i+8);
            gwy_graph_curve_model_set_data_from_dataline(gcmodel, lowunc, 0, 0);
         }
    }
    else {
        gcmodel = gwy_graph_curve_model_new();
        desc = g_strdup_printf(_("Profile %d"), i/multpos+1);
        color = gwy_graph_get_preset_color(i);
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "description", desc,
                     "color", color,
                     NULL);
        g_free(desc);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line, 0, 0); 
        gwy_graph_model_add_curve(tool->gmodel, gcmodel);
        g_object_unref(gcmodel);

        if (i == 0)
            gwy_graph_model_set_units_from_data_line(tool->gmodel, tool->line);

        if (tool->has_calibration) {
            
            desc = g_strdup_printf(_("X error %d"), i/multpos+1);
            add_hidden_curve(tool, tool->line_xerr, desc, color, !(tool->display_type==1));
            g_free(desc);
            desc = g_strdup_printf(_("Y error %d"), i/multpos+1);
            add_hidden_curve(tool, tool->line_yerr, desc, color, !(tool->display_type==2));
            g_free(desc);
            desc = g_strdup_printf(_("Z error %d"), i/multpos+1);
            add_hidden_curve(tool, tool->line_zerr, desc, color, !(tool->display_type==3));
            g_free(desc);
            desc = g_strdup_printf(_("X uncertainty %d"), i/multpos+1);
            add_hidden_curve(tool, tool->line_xunc, desc, color, !(tool->display_type==4));
            g_free(desc);
            desc = g_strdup_printf(_("Y uncertainty %d"), i/multpos+1);
            add_hidden_curve(tool, tool->line_yunc, desc, color, !(tool->display_type==5));
            g_free(desc);
            desc = g_strdup_printf(_("Z uncertainty %d"), i/multpos+1);
            add_hidden_curve(tool, tool->line_zunc, desc, color, TRUE);
            g_free(desc);
            desc = g_strdup_printf(_("Zunc up bound %d"), i/multpos+1);
            add_hidden_curve(tool, upunc, desc, color, !(tool->display_type==6));
            g_free(desc);
            desc = g_strdup_printf(_("Zunc low bound %d"), i/multpos+1);
            add_hidden_curve(tool, lowunc, desc, color, !(tool->display_type==6));
            g_free(desc);
         }
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
        val = floor(gwy_data_field_rtoj(plain_tool->data_field, line[0]));
        break;

        case COLUMN_Y1:
        val = floor(gwy_data_field_rtoi(plain_tool->data_field, line[1]));
        break;

        case COLUMN_X2:
        val = floor(gwy_data_field_rtoj(plain_tool->data_field, line[2]));
        break;

        case COLUMN_Y2:
        val = floor(gwy_data_field_rtoi(plain_tool->data_field, line[3]));
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
    GwyPlainTool *plain_tool;

    tool->args.thickness = gwy_adjustment_get_int(adj);
    plain_tool = GWY_PLAIN_TOOL(tool);
    if (plain_tool->layer)
        g_object_set(plain_tool->layer,
                     "thickness", tool->args.thickness,
                     NULL);
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
/*
static void
gwy_tool_profile_export_changed(GtkToggleButton *check,
                                  GwyToolProfile *tool)
{
    tool->args.export = gtk_toggle_button_get_active(check);
}*/
static void
gwy_tool_profile_both_changed(GtkToggleButton *check,
                                  GwyToolProfile *tool)
{
    tool->args.both = gtk_toggle_button_get_active(check);
    display_changed(NULL, tool);
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
    GwyCurveCalibrationData *ccdata;
    GwyGraphModel *gmodel;
    gchar *s;
    gint i, n;
    gint multpos, id;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);
    n = gwy_selection_get_data(plain_tool->selection, NULL);
    g_return_if_fail(n);

    if (tool->has_calibration) multpos = 9;
    else multpos = 1;

    if (!tool->args.separate) {
        gmodel = gwy_graph_model_duplicate(tool->gmodel);
        g_object_set(gmodel, "label-visible", TRUE, NULL);
        gwy_app_data_browser_add_graph_model(gmodel, plain_tool->container,
                                             TRUE);
        g_object_unref(gmodel);

        return;
    }

    for (i = 0; i < n*multpos; i+=multpos) {

        gmodel = gwy_graph_model_new_alike(tool->gmodel);
        g_object_set(gmodel, "label-visible", TRUE, NULL);
        gcmodel = gwy_graph_model_get_curve(tool->gmodel, i);
        gcmodel = gwy_graph_curve_model_duplicate(gcmodel);

        /*add calibration data to the curve*/
        if (tool->has_calibration) 
        {
            ccdata = (GwyCurveCalibrationData *)g_malloc(sizeof(GwyCurveCalibrationData));

            ccdata->xerr = g_memdup(gwy_graph_curve_model_get_ydata(gwy_graph_model_get_curve(tool->gmodel, i+1)),
                                            gwy_graph_curve_model_get_ndata(gcmodel)*sizeof(gdouble));
            ccdata->yerr = g_memdup(gwy_graph_curve_model_get_ydata(gwy_graph_model_get_curve(tool->gmodel, i+2)),
                                            gwy_graph_curve_model_get_ndata(gcmodel)*sizeof(gdouble));
            ccdata->zerr = g_memdup(gwy_graph_curve_model_get_ydata(gwy_graph_model_get_curve(tool->gmodel, i+3)),
                                           gwy_graph_curve_model_get_ndata(gcmodel)*sizeof(gdouble));
            ccdata->xunc = g_memdup(gwy_graph_curve_model_get_ydata(gwy_graph_model_get_curve(tool->gmodel, i+4)),
                                            gwy_graph_curve_model_get_ndata(gcmodel)*sizeof(gdouble));
            ccdata->yunc = g_memdup(gwy_graph_curve_model_get_ydata(gwy_graph_model_get_curve(tool->gmodel, i+5)),
                                            gwy_graph_curve_model_get_ndata(gcmodel)*sizeof(gdouble));
            ccdata->zunc = g_memdup(gwy_graph_curve_model_get_ydata(gwy_graph_model_get_curve(tool->gmodel, i+6)),
                                            gwy_graph_curve_model_get_ndata(gcmodel)*sizeof(gdouble));
  
            gwy_graph_curve_model_set_calibration_data(gcmodel, ccdata);
        }

        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
        g_object_get(gcmodel, "description", &s, NULL);
        g_object_set(gmodel, "title", s, NULL);
        g_free(s);
        id = gwy_app_data_browser_add_graph_model(gmodel, plain_tool->container,
                                             TRUE);
        g_object_unref(gmodel);
  

        if (tool->args.export && tool->display_type>0) {
            gmodel = gwy_graph_model_new_alike(tool->gmodel);
            g_object_set(gmodel, "label-visible", TRUE, NULL);
            gcmodel = gwy_graph_model_get_curve(tool->gmodel, i+tool->display_type);
            gcmodel = gwy_graph_curve_model_duplicate(gcmodel);
            gwy_graph_model_add_curve(gmodel, gcmodel);
            g_object_unref(gcmodel);
            g_object_get(gcmodel, "description", &s, NULL);
            g_object_set(gmodel, "title", s, NULL);
            g_object_set(gcmodel, "mode", GWY_GRAPH_CURVE_LINE, NULL);
            g_free(s);
            gwy_app_data_browser_add_graph_model(gmodel, plain_tool->container,
                                             TRUE);
         }

    }
}


static GtkWidget*
menu_display(GCallback callback, gpointer cbdata,
             GwyCCDisplayType current)
{
    static const GwyEnum entries[] = {
        { N_("None"),        GWY_CC_DISPLAY_NONE,      },
        { N_("X correction"),   GWY_CC_DISPLAY_X_CORR, },
        { N_("Y correction"),   GWY_CC_DISPLAY_Y_CORR, },
        { N_("Z correction"),   GWY_CC_DISPLAY_Z_CORR, },
        { N_("X uncertainty"),  GWY_CC_DISPLAY_X_UNC,  },
        { N_("Y uncertainty"),  GWY_CC_DISPLAY_Y_UNC,  },
        { N_("Z uncertainty"),  GWY_CC_DISPLAY_Z_UNC,  },

    };
    return gwy_enum_combo_box_new(entries, G_N_ELEMENTS(entries),
                                  callback, cbdata, current, TRUE);
}

static void
display_changed(GtkComboBox *combo, GwyToolProfile *tool)
{
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *gcmodel;
    gint i, n;
    gint multpos = 9;

    if (!tool->has_calibration) return;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);
    n = gwy_selection_get_data(plain_tool->selection, NULL);
    if (!n) return;

    tool->display_type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(tool->menu_display));

    
    /*change the visibility of all the affected curves*/
    for (i = 0; i < n*multpos; i++) {
        gcmodel = gwy_graph_model_get_curve(tool->gmodel, i);

        if (i%multpos==0) { 
            if (tool->args.both) g_object_set(gcmodel, "mode", GWY_GRAPH_CURVE_LINE, NULL);
            else g_object_set(gcmodel, "mode", GWY_GRAPH_CURVE_HIDDEN, NULL);
        }
        else if ((tool->display_type<=5 && (i-(int)tool->display_type)>=0 && (i-(int)tool->display_type)%multpos == 0) 
            || (tool->display_type==6 && ((i-7)%multpos==0 || (i-8)%multpos==0))) {
            g_object_set(gcmodel, "mode", GWY_GRAPH_CURVE_LINE, NULL);
        }
        else {
            g_object_set(gcmodel, "mode", GWY_GRAPH_CURVE_HIDDEN, NULL);
        }
    }

}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

