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
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_LINE_STATS            (gwy_tool_line_stats_get_type())
#define GWY_TOOL_LINE_STATS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_LINE_STATS, GwyToolLineStats))
#define GWY_IS_TOOL_LINE_STATS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_LINE_STATS))
#define GWY_TOOL_LINE_STATS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_LINE_STATS, GwyToolLineStatsClass))

enum {
    MIN_RESOLUTION = 4,
    MAX_RESOLUTION = 1024
};

typedef struct _GwyToolLineStats      GwyToolLineStats;
typedef struct _GwyToolLineStatsClass GwyToolLineStatsClass;

typedef struct {
    GwyLineStatQuantity output_type;
    gboolean options_visible;
    gboolean instant_update;
    gint resolution;
    gboolean fixres;
    GwyOrientation direction;
    GwyInterpolationType interpolation;
} ToolArgs;

struct _GwyToolLineStats {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GwyRectSelectionLabels *rlabels;

    GwyDataLine *line;

    GtkWidget *graph;
    GwyGraphModel *gmodel;

    GtkWidget *options;
    GtkWidget *output_type;
    GtkWidget *instant_update;
    GSList *direction;
    GtkObject *resolution;
    GtkWidget *fixres;
    GtkWidget *interpolation;
    GtkWidget *interpolation_label;
    GtkWidget *update;
    GtkWidget *apply;

    /* potential class data */
    GType layer_type_rect;
};

struct _GwyToolLineStatsClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_line_stats_get_type            (void) G_GNUC_CONST;
static void gwy_tool_line_stats_finalize             (GObject *object);
static void gwy_tool_line_stats_init_dialog          (GwyToolLineStats *tool);
static void gwy_tool_line_stats_data_switched        (GwyTool *gwytool,
                                                      GwyDataView *data_view);
static void gwy_tool_line_stats_response             (GwyTool *tool,
                                                      gint response_id);
static void gwy_tool_line_stats_data_changed         (GwyPlainTool *plain_tool);
static void gwy_tool_line_stats_selection_changed    (GwyPlainTool *plain_tool,
                                                      gint hint);
static void gwy_tool_line_stats_update_sensitivity   (GwyToolLineStats *tool);
static void gwy_tool_line_stats_update_curve         (GwyToolLineStats *tool);
static void gwy_tool_line_stats_instant_update_changed(GtkToggleButton *check,
                                                       GwyToolLineStats *tool);
static void gwy_tool_line_stats_resolution_changed   (GwyToolLineStats *tool,
                                                      GtkAdjustment *adj);
static void gwy_tool_line_stats_fixres_changed       (GtkToggleButton *check,
                                                      GwyToolLineStats *tool);
static void gwy_tool_line_stats_output_type_changed  (GtkComboBox *combo,
                                                      GwyToolLineStats *tool);
static void gwy_tool_line_stats_direction_changed    (GObject *button,
                                                      GwyToolLineStats *tool);
static void gwy_tool_line_stats_interpolation_changed(GtkComboBox *combo,
                                                      GwyToolLineStats *tool);
static void gwy_tool_line_stats_options_expanded     (GtkExpander *expander,
                                                      GParamSpec *pspec,
                                                      GwyToolLineStats *tool);
static void gwy_tool_line_stats_apply                (GwyToolLineStats *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Row/column statistical function tool, mean values, medians, maxima, "
       "minima, RMS, ..., of rows or columns."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

static const gchar direction_key[]       = "/module/linestats/direction";
static const gchar fixres_key[]          = "/module/linestats/fixres";
static const gchar instant_update_key[]  = "/module/linestats/instant_update";
static const gchar interpolation_key[]   = "/module/linestats/interpolation";
static const gchar options_visible_key[] = "/module/linestats/options_visible";
static const gchar output_type_key[]     = "/module/linestats/output_type";
static const gchar resolution_key[]      = "/module/linestats/resolution";

static const ToolArgs default_args = {
    GWY_LINE_STAT_MEAN,
    FALSE,
    TRUE,
    120,
    FALSE,
    GWY_ORIENTATION_HORIZONTAL,
    GWY_INTERPOLATION_LINEAR,
};

static const GwyEnum sf_types[] =  {
    { N_("Mean"),    GWY_LINE_STAT_MEAN,    },
    { N_("Median"),  GWY_LINE_STAT_MEDIAN,  },
    { N_("Minimum"), GWY_LINE_STAT_MINIMUM, },
    { N_("Maximum"), GWY_LINE_STAT_MAXIMUM, },
    { N_("RMS"),     GWY_LINE_STAT_RMS,     },
    { N_("Length"),  GWY_LINE_STAT_LENGTH,  },
    { N_("Slope"),   GWY_LINE_STAT_SLOPE,   },
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolLineStats, gwy_tool_line_stats, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_LINE_STATS);

    return TRUE;
}

static void
gwy_tool_line_stats_class_init(GwyToolLineStatsClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_line_stats_finalize;

    tool_class->stock_id = GWY_STOCK_GRAPH_VERTICAL;
    tool_class->title = _("Row/Column Statistics");
    tool_class->tooltip = _("Calculate row/column statistical functions");
    tool_class->prefix = "/module/line_stats";
    tool_class->default_width = 640;
    tool_class->default_height = 400;
    tool_class->data_switched = gwy_tool_line_stats_data_switched;
    tool_class->response = gwy_tool_line_stats_response;

    ptool_class->data_changed = gwy_tool_line_stats_data_changed;
    ptool_class->selection_changed = gwy_tool_line_stats_selection_changed;
}

static void
gwy_tool_line_stats_finalize(GObject *object)
{
    GwyToolLineStats *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_LINE_STATS(object);

    settings = gwy_app_settings_get();
    gwy_container_set_enum_by_name(settings, output_type_key,
                                   tool->args.output_type);
    gwy_container_set_boolean_by_name(settings, options_visible_key,
                                      tool->args.options_visible);
    gwy_container_set_boolean_by_name(settings, instant_update_key,
                                      tool->args.instant_update);
    gwy_container_set_int32_by_name(settings, resolution_key,
                                    tool->args.resolution);
    gwy_container_set_boolean_by_name(settings, fixres_key,
                                      tool->args.fixres);
    gwy_container_set_enum_by_name(settings, interpolation_key,
                                   tool->args.interpolation);
    gwy_container_set_enum_by_name(settings, direction_key,
                                   tool->args.direction);

    gwy_object_unref(tool->line);

    G_OBJECT_CLASS(gwy_tool_line_stats_parent_class)->finalize(object);
}

static void
gwy_tool_line_stats_init(GwyToolLineStats *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_rect = gwy_plain_tool_check_layer_type(plain_tool,
                                                           "GwyLayerRectangle");
    if (!tool->layer_type_rect)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;
    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_enum_by_name(settings, output_type_key,
                                   &tool->args.output_type);
    gwy_container_gis_boolean_by_name(settings, options_visible_key,
                                      &tool->args.options_visible);
    gwy_container_gis_boolean_by_name(settings, instant_update_key,
                                      &tool->args.instant_update);
    gwy_container_gis_int32_by_name(settings, resolution_key,
                                    &tool->args.resolution);
    gwy_container_gis_boolean_by_name(settings, fixres_key,
                                      &tool->args.fixres);
    gwy_container_gis_enum_by_name(settings, interpolation_key,
                                   &tool->args.interpolation);
    tool->args.interpolation
        = gwy_enum_sanitize_value(tool->args.interpolation,
                                  GWY_TYPE_INTERPOLATION_TYPE);
    gwy_container_gis_enum_by_name(settings, direction_key,
                                   &tool->args.direction);
    tool->args.direction
        = gwy_enum_sanitize_value(tool->args.direction, GWY_TYPE_ORIENTATION);

    tool->line = gwy_data_line_new(4, 1.0, FALSE);

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_rect,
                                     "rectangle");

    gwy_tool_line_stats_init_dialog(tool);
}

static void
gwy_tool_line_stats_rect_updated(GwyToolLineStats *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_rect_selection_labels_select(tool->rlabels,
                                     plain_tool->selection,
                                     plain_tool->data_field);
}

static void
gwy_tool_line_stats_init_dialog(GwyToolLineStats *tool)
{
    static const GwyEnum directions[] = {
        { N_("Ro_ws"),    GWY_ORIENTATION_HORIZONTAL, },
        { N_("Co_lumns"), GWY_ORIENTATION_VERTICAL,   },
    };
    GtkDialog *dialog;
    GtkWidget *label, *hbox, *vbox, *hbox2, *image;
    GtkTable *table;
    guint row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, TRUE, TRUE, 0);

    /* Left pane */
    vbox = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    /* Selection info */
    tool->rlabels = gwy_rect_selection_labels_new
                         (TRUE, G_CALLBACK(gwy_tool_line_stats_rect_updated),
                          tool);
    gtk_box_pack_start(GTK_BOX(vbox),
                       gwy_rect_selection_labels_get_table(tool->rlabels),
                       FALSE, FALSE, 0);

    /* Output type */
    hbox2 = gtk_hbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(hbox2), 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    tool->output_type = gwy_enum_combo_box_new
                           (sf_types, G_N_ELEMENTS(sf_types),
                            G_CALLBACK(gwy_tool_line_stats_output_type_changed),
                            tool,
                            tool->args.output_type, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox2), tool->output_type, FALSE, FALSE, 0);

    /* Options */
    tool->options = gtk_expander_new(_("<b>Options</b>"));
    gtk_expander_set_use_markup(GTK_EXPANDER(tool->options), TRUE);
    gtk_expander_set_expanded(GTK_EXPANDER(tool->options),
                              tool->args.options_visible);
    g_signal_connect(tool->options, "notify::expanded",
                     G_CALLBACK(gwy_tool_line_stats_options_expanded), tool);
    gtk_box_pack_start(GTK_BOX(vbox), tool->options, FALSE, FALSE, 0);

    table = GTK_TABLE(gtk_table_new(6, 4, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(tool->options), GTK_WIDGET(table));
    row = 0;

    tool->instant_update
        = gtk_check_button_new_with_mnemonic(_("_Instant updates"));
    gtk_table_attach(table, tool->instant_update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->instant_update),
                                 tool->args.instant_update);
    g_signal_connect(tool->instant_update, "toggled",
                     G_CALLBACK(gwy_tool_line_stats_instant_update_changed),
                     tool);
    row++;

    tool->resolution = gtk_adjustment_new(tool->args.resolution,
                                          MIN_RESOLUTION, MAX_RESOLUTION,
                                          1, 10, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Fix res.:"), NULL,
                            tool->resolution,
                            GWY_HSCALE_CHECK | GWY_HSCALE_SQRT);
    g_signal_connect_swapped(tool->resolution, "value-changed",
                             G_CALLBACK(gwy_tool_line_stats_resolution_changed),
                             tool);
    tool->fixres = gwy_table_hscale_get_check(tool->resolution);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->fixres),
                                 tool->args.fixres);
    g_signal_connect(tool->fixres, "toggled",
                     G_CALLBACK(gwy_tool_line_stats_fixres_changed), tool);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    tool->direction = gwy_radio_buttons_create
                        (directions, G_N_ELEMENTS(directions),
                        G_CALLBACK(gwy_tool_line_stats_direction_changed), tool,
                        tool->args.direction);
    row = gwy_radio_buttons_attach_to_table(tool->direction, table, 3, row);
    gtk_table_set_row_spacing(table, row-1, 8);

    hbox2 = gtk_hbox_new(FALSE, 4);
    gtk_table_attach(table, hbox2,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
    tool->interpolation_label = label;

    tool->interpolation = gwy_enum_combo_box_new
                         (gwy_interpolation_type_get_enum(), -1,
                          G_CALLBACK(gwy_tool_line_stats_interpolation_changed),
                          tool,
                          tool->args.interpolation, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), tool->interpolation);
    gtk_box_pack_end(GTK_BOX(hbox2), tool->interpolation, FALSE, FALSE, 0);
    row++;

    tool->gmodel = gwy_graph_model_new();

    tool->graph = gwy_graph_new(tool->gmodel);
    gwy_graph_enable_user_input(GWY_GRAPH(tool->graph), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), tool->graph, TRUE, TRUE, 2);

    tool->update = gtk_dialog_add_button(dialog, _("_Update"),
                                         GWY_TOOL_RESPONSE_UPDATE);
    image = gtk_image_new_from_stock(GTK_STOCK_EXECUTE, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(tool->update), image);
    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gwy_tool_line_stats_update_sensitivity(tool);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_line_stats_data_switched(GwyTool *gwytool,
                                  GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolLineStats *tool;

    GWY_TOOL_CLASS(gwy_tool_line_stats_parent_class)->data_switched(gwytool,
                                                                 data_view);

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_LINE_STATS(gwytool);
    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_rect,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
    }

    gwy_tool_line_stats_update_curve(tool);
}

static void
gwy_tool_line_stats_response(GwyTool *tool,
                             gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_line_stats_parent_class)->response(tool,
                                                               response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_line_stats_apply(GWY_TOOL_LINE_STATS(tool));
    else if (response_id == GWY_TOOL_RESPONSE_UPDATE)
        gwy_tool_line_stats_update_curve(GWY_TOOL_LINE_STATS(tool));
}

static void
gwy_tool_line_stats_data_changed(GwyPlainTool *plain_tool)
{
    gwy_tool_line_stats_update_curve(GWY_TOOL_LINE_STATS(plain_tool));
}

static void
gwy_tool_line_stats_selection_changed(GwyPlainTool *plain_tool,
                                      gint hint)
{
    GwyToolLineStats *tool;
    gint n = 0;

    tool = GWY_TOOL_LINE_STATS(plain_tool);
    g_return_if_fail(hint <= 0);

    if (plain_tool->selection) {
        n = gwy_selection_get_data(plain_tool->selection, NULL);
        g_return_if_fail(n == 0 || n == 1);
        gwy_rect_selection_labels_fill(tool->rlabels,
                                       plain_tool->selection,
                                       plain_tool->data_field,
                                       NULL, NULL);
    }
    else
        gwy_rect_selection_labels_fill(tool->rlabels, NULL, NULL, NULL, NULL);

    if (tool->args.instant_update)
        gwy_tool_line_stats_update_curve(tool);
}

static void
gwy_tool_line_stats_update_sensitivity(GwyToolLineStats *tool)
{
    gtk_widget_set_sensitive(tool->update, !tool->args.instant_update);

    gwy_table_hscale_set_sensitive(tool->resolution, tool->args.fixres);
    gtk_widget_set_sensitive(tool->interpolation, tool->args.fixres);
    gtk_widget_set_sensitive(tool->interpolation_label, tool->args.fixres);
}

static void
gwy_tool_line_stats_update_curve(GwyToolLineStats *tool)
{
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *gcmodel;
    gdouble sel[4];
    gint isel[4] = { sizeof("Die, die, GCC!"), 0, 0, 0 };
    gint n, nsel, w = sizeof("Die, die, GCC!"), h = 0;
    const gchar *title;

    plain_tool = GWY_PLAIN_TOOL(tool);

    if (tool->args.output_type == GWY_LINE_STAT_LENGTH) {
        GwySIUnit *xyunit, *zunit;

        xyunit = gwy_data_field_get_si_unit_xy(plain_tool->data_field);
        zunit = gwy_data_field_get_si_unit_z(plain_tool->data_field);
        if (!gwy_si_unit_equal(xyunit, zunit)) {
            gwy_graph_model_remove_all_curves(tool->gmodel);
            gtk_widget_set_sensitive(tool->apply, FALSE);
            return;
        }
    }

    n = gwy_graph_model_get_n_curves(tool->gmodel);
    nsel = 0;
    if (plain_tool->data_field) {
        if (!plain_tool->selection
            || !gwy_selection_get_object(plain_tool->selection, 0, sel)) {
            nsel = 1;
            isel[0] = isel[1] = 0;
            w = gwy_data_field_get_xres(plain_tool->data_field);
            h = gwy_data_field_get_yres(plain_tool->data_field);
        }
        else {
            isel[0] = gwy_data_field_rtoj(plain_tool->data_field, sel[0]);
            isel[1] = gwy_data_field_rtoi(plain_tool->data_field, sel[1]);
            isel[2] = gwy_data_field_rtoj(plain_tool->data_field, sel[2]);
            isel[3] = gwy_data_field_rtoi(plain_tool->data_field, sel[3]);

            w = ABS(isel[2] - isel[0]) + 1;
            h = ABS(isel[3] - isel[1]) + 1;
            isel[0] = MIN(isel[0], isel[2]);
            isel[1] = MIN(isel[1], isel[3]);
            if (w >= 4 && h >= 4)
                nsel = 1;
        }
    }

    gtk_widget_set_sensitive(tool->apply, nsel > 0);

    if (nsel == 0 && n == 0)
        return;

    if (nsel == 0 && n > 0) {
        gwy_graph_model_remove_all_curves(tool->gmodel);
        return;
    }

    gwy_data_field_area_get_line_stats(plain_tool->data_field, NULL,
                                       tool->line,
                                       isel[0], isel[1], w, h,
                                       tool->args.output_type,
                                       tool->args.direction);
    if (tool->args.fixres)
        gwy_data_line_resample(tool->line,
                               tool->args.resolution,
                               tool->args.interpolation);

    if (nsel > 0 && n == 0) {
        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_model_add_curve(tool->gmodel, gcmodel);
        g_object_set(gcmodel, "mode", GWY_GRAPH_CURVE_LINE, NULL);
        g_object_unref(gcmodel);
    }
    else
        gcmodel = gwy_graph_model_get_curve(tool->gmodel, 0);

    gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line, 0, 0);
    title = gwy_enum_to_string(tool->args.output_type,
                               sf_types, G_N_ELEMENTS(sf_types));
    g_object_set(gcmodel, "description", title, NULL);
    g_object_set(tool->gmodel, "title", title, NULL);
    gwy_graph_model_set_units_from_data_line(tool->gmodel, tool->line);
}

static void
gwy_tool_line_stats_instant_update_changed(GtkToggleButton *check,
                                           GwyToolLineStats *tool)
{
    tool->args.instant_update = gtk_toggle_button_get_active(check);
    gwy_tool_line_stats_update_sensitivity(tool);
    if (tool->args.instant_update)
        gwy_tool_line_stats_update_curve(tool);
}

static void
gwy_tool_line_stats_resolution_changed(GwyToolLineStats *tool,
                                       GtkAdjustment *adj)
{
    tool->args.resolution = gwy_adjustment_get_int(adj);
    /* Resolution can be changed only when fixres == TRUE */
    gwy_tool_line_stats_update_curve(tool);
}

static void
gwy_tool_line_stats_fixres_changed(GtkToggleButton *check,
                                   GwyToolLineStats *tool)
{
    tool->args.fixres = gtk_toggle_button_get_active(check);
    gwy_tool_line_stats_update_sensitivity(tool);
    gwy_tool_line_stats_update_curve(tool);
}

static void
gwy_tool_line_stats_output_type_changed(GtkComboBox *combo,
                                        GwyToolLineStats *tool)
{
    tool->args.output_type = gwy_enum_combo_box_get_active(combo);
    gwy_tool_line_stats_update_sensitivity(tool);
    gwy_tool_line_stats_update_curve(tool);
}

static void
gwy_tool_line_stats_direction_changed(G_GNUC_UNUSED GObject *button,
                                      GwyToolLineStats *tool)
{
    tool->args.direction = gwy_radio_buttons_get_current(tool->direction);
    gwy_tool_line_stats_update_curve(tool);
}

static void
gwy_tool_line_stats_interpolation_changed(GtkComboBox *combo,
                                          GwyToolLineStats *tool)
{
    tool->args.interpolation = gwy_enum_combo_box_get_active(combo);
    gwy_tool_line_stats_update_curve(tool);
}

static void
gwy_tool_line_stats_options_expanded(GtkExpander *expander,
                                     G_GNUC_UNUSED GParamSpec *pspec,
                                     GwyToolLineStats *tool)
{
    tool->args.options_visible = gtk_expander_get_expanded(expander);
}

static void
gwy_tool_line_stats_apply(GwyToolLineStats *tool)
{
    GwyPlainTool *plain_tool;
    GwyGraphModel *gmodel;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);

    gmodel = gwy_graph_model_duplicate(tool->gmodel);
    gwy_app_data_browser_add_graph_model(gmodel, plain_tool->container, TRUE);
    g_object_unref(gmodel);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
