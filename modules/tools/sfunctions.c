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
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/stats.h>
#include <libprocess/stats_uncertainty.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_SFUNCTIONS            (gwy_tool_sfunctions_get_type())
#define GWY_TOOL_SFUNCTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_SFUNCTIONS, GwyToolSFunctions))
#define GWY_IS_TOOL_SFUNCTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_SFUNCTIONS))
#define GWY_TOOL_SFUNCTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_SFUNCTIONS, GwyToolSFunctionsClass))

typedef enum {
    GWY_SF_DH                     = 0,
    GWY_SF_CDH                    = 1,
    GWY_SF_DA                     = 2,
    GWY_SF_CDA                    = 3,
    GWY_SF_ACF                    = 4,
    GWY_SF_HHCF                   = 5,
    GWY_SF_PSDF                   = 6,
    GWY_SF_MINKOWSKI_VOLUME       = 7,
    GWY_SF_MINKOWSKI_BOUNDARY     = 8,
    GWY_SF_MINKOWSKI_CONNECTIVITY = 9,
    GWY_SF_RPSDF                  = 10,
    GWY_SF_RACF                   = 11,
} GwySFOutputType;

enum {
    MIN_RESOLUTION = 4,
    MAX_RESOLUTION = 16384
};

typedef struct _GwyToolSFunctions      GwyToolSFunctions;
typedef struct _GwyToolSFunctionsClass GwyToolSFunctionsClass;

typedef struct {
    GwySFOutputType output_type;
    gboolean options_visible;
    gboolean instant_update;
    gint resolution;
    gboolean fixres;
    GwyOrientation direction;
    GwyInterpolationType interpolation;
    gboolean separate;
} ToolArgs;

struct _GwyToolSFunctions {
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
    GtkWidget *separate;

    gboolean has_calibration;
    gboolean has_uline;
    GwyDataLine *uline;
    GwyDataField *xunc;
    GwyDataField *yunc;
    GwyDataField *zunc;


    /* potential class data */
    GType layer_type_rect;
};

struct _GwyToolSFunctionsClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_sfunctions_get_type            (void) G_GNUC_CONST;
static void gwy_tool_sfunctions_finalize             (GObject *object);
static void gwy_tool_sfunctions_init_dialog          (GwyToolSFunctions *tool);
static void gwy_tool_sfunctions_data_switched        (GwyTool *gwytool,
                                                      GwyDataView *data_view);
static void gwy_tool_sfunctions_response             (GwyTool *tool,
                                                      gint response_id);
static void gwy_tool_sfunctions_data_changed         (GwyPlainTool *plain_tool);
static void gwy_tool_sfunctions_selection_changed    (GwyPlainTool *plain_tool,
                                                      gint hint);
static void gwy_tool_sfunctions_update_sensitivity   (GwyToolSFunctions *tool);
static void gwy_tool_sfunctions_update_curve         (GwyToolSFunctions *tool);
static void gwy_tool_sfunctions_instant_update_changed(GtkToggleButton *check,
                                                       GwyToolSFunctions *tool);
static void gwy_tool_sfunctions_resolution_changed   (GwyToolSFunctions *tool,
                                                      GtkAdjustment *adj);
static void gwy_tool_sfunctions_fixres_changed       (GtkToggleButton *check,
                                                      GwyToolSFunctions *tool);
static void gwy_tool_sfunctions_output_type_changed  (GtkComboBox *combo,
                                                      GwyToolSFunctions *tool);
static void gwy_tool_sfunctions_direction_changed    (GObject *button,
                                                      GwyToolSFunctions *tool);
static void gwy_tool_sfunctions_interpolation_changed(GtkComboBox *combo,
                                                      GwyToolSFunctions *tool);
static void gwy_tool_sfunctions_options_expanded     (GtkExpander *expander,
                                                      GParamSpec *pspec,
                                                      GwyToolSFunctions *tool);
static void gwy_tool_sfunctions_separate_changed     (GtkToggleButton *check,
                                                      GwyToolSFunctions *tool);

static void gwy_tool_sfunctions_apply                (GwyToolSFunctions *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Statistical function tool, calculates one-dimensional statistical "
       "functions (height distribution, correlations, PSDF, Minkowski "
       "functionals) of selected part of data."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.8",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static const gchar direction_key[]       = "/module/sfunctions/direction";
static const gchar fixres_key[]          = "/module/sfunctions/fixres";
static const gchar instant_update_key[]  = "/module/sfunctions/instant_update";
static const gchar interpolation_key[]   = "/module/sfunctions/interpolation";
static const gchar options_visible_key[] = "/module/sfunctions/options_visible";
static const gchar output_type_key[]     = "/module/sfunctions/output_type";
static const gchar resolution_key[]      = "/module/sfunctions/resolution";
static const gchar separate_key[]      = "/module/sfunctions/separate";

static const ToolArgs default_args = {
    GWY_SF_DH,
    FALSE,
    TRUE,
    120,
    FALSE,
    GWY_ORIENTATION_HORIZONTAL,
    GWY_INTERPOLATION_LINEAR,
    FALSE,
};

static const GwyEnum sf_types[] =  {
    { N_("Height distribution"),         GWY_SF_DH,                     },
    { N_("Cum. height distribution"),    GWY_SF_CDH,                    },
    { N_("Distribution of angles"),      GWY_SF_DA,                     },
    { N_("Cum. distribution of angles"), GWY_SF_CDA,                    },
    { N_("ACF"),                         GWY_SF_ACF,                    },
    { N_("HHCF"),                        GWY_SF_HHCF,                   },
    { N_("PSDF"),                        GWY_SF_PSDF,                   },
    { N_("Radial PSDF"),                 GWY_SF_RPSDF,                  },
    { N_("Radial ACF"),                  GWY_SF_RACF,                   },
    { N_("Minkowski volume"),            GWY_SF_MINKOWSKI_VOLUME,       },
    { N_("Minkowski boundary"),          GWY_SF_MINKOWSKI_BOUNDARY,     },
    { N_("Minkowski connectivity"),      GWY_SF_MINKOWSKI_CONNECTIVITY, },
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolSFunctions, gwy_tool_sfunctions, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_SFUNCTIONS);

    return TRUE;
}

static void
gwy_tool_sfunctions_class_init(GwyToolSFunctionsClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_sfunctions_finalize;

    tool_class->stock_id = GWY_STOCK_GRAPH_HALFGAUSS;
    tool_class->title = _("Statistical Functions");
    tool_class->tooltip = _("Calculate 1D statistical functions");
    tool_class->prefix = "/module/sfunctions";
    tool_class->default_width = 640;
    tool_class->default_height = 400;
    tool_class->data_switched = gwy_tool_sfunctions_data_switched;
    tool_class->response = gwy_tool_sfunctions_response;

    ptool_class->data_changed = gwy_tool_sfunctions_data_changed;
    ptool_class->selection_changed = gwy_tool_sfunctions_selection_changed;
}

static void
gwy_tool_sfunctions_finalize(GObject *object)
{
    GwyToolSFunctions *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_SFUNCTIONS(object);

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
    gwy_object_unref(tool->gmodel);
    if (tool->xunc) g_object_unref(tool->xunc);
    if (tool->yunc) g_object_unref(tool->yunc);
    if (tool->zunc) g_object_unref(tool->zunc);

    G_OBJECT_CLASS(gwy_tool_sfunctions_parent_class)->finalize(object);
}

static void
gwy_tool_sfunctions_init(GwyToolSFunctions *tool)
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
    tool->uline = gwy_data_line_new(4, 1.0, FALSE);
    tool->xunc = NULL;
    tool->yunc = NULL;
    tool->zunc = NULL;

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_rect,
                                     "rectangle");

    gwy_tool_sfunctions_init_dialog(tool);
}

static void
gwy_tool_sfunctions_rect_updated(GwyToolSFunctions *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_rect_selection_labels_select(tool->rlabels,
                                     plain_tool->selection,
                                     plain_tool->data_field);
}

static void
gwy_tool_sfunctions_init_dialog(GwyToolSFunctions *tool)
{
    static const GwyEnum directions[] = {
        { N_("_Horizontal direction"), GWY_ORIENTATION_HORIZONTAL, },
        { N_("_Vertical direction"),   GWY_ORIENTATION_VERTICAL,   },
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
                         (TRUE, G_CALLBACK(gwy_tool_sfunctions_rect_updated),
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
                            G_CALLBACK(gwy_tool_sfunctions_output_type_changed),
                            tool,
                            tool->args.output_type, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox2), tool->output_type, FALSE, FALSE, 0);

    /* Options */
    tool->options = gtk_expander_new(_("<b>Options</b>"));
    gtk_expander_set_use_markup(GTK_EXPANDER(tool->options), TRUE);
    gtk_expander_set_expanded(GTK_EXPANDER(tool->options),
                              tool->args.options_visible);
    g_signal_connect(tool->options, "notify::expanded",
                     G_CALLBACK(gwy_tool_sfunctions_options_expanded), tool);
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
                     G_CALLBACK(gwy_tool_sfunctions_instant_update_changed),
                     tool);
    row++;

    tool->resolution = gtk_adjustment_new(tool->args.resolution,
                                          MIN_RESOLUTION, MAX_RESOLUTION,
                                          1, 10, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Fix res.:"), NULL,
                            tool->resolution,
                            GWY_HSCALE_CHECK | GWY_HSCALE_SQRT);
    g_signal_connect_swapped(tool->resolution, "value-changed",
                             G_CALLBACK(gwy_tool_sfunctions_resolution_changed),
                             tool);
    tool->fixres = gwy_table_hscale_get_check(tool->resolution);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->fixres),
                                 tool->args.fixres);
    g_signal_connect(tool->fixres, "toggled",
                     G_CALLBACK(gwy_tool_sfunctions_fixres_changed), tool);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    tool->direction = gwy_radio_buttons_create
                        (directions, G_N_ELEMENTS(directions),
                        G_CALLBACK(gwy_tool_sfunctions_direction_changed), tool,
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
                          G_CALLBACK(gwy_tool_sfunctions_interpolation_changed),
                          tool,
                          tool->args.interpolation, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), tool->interpolation);
    gtk_box_pack_end(GTK_BOX(hbox2), tool->interpolation, FALSE, FALSE, 0);
    row++;

    tool->separate
        = gtk_check_button_new_with_mnemonic(_("_Separate uncertainty"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->separate),
                                 tool->args.separate);
    g_signal_connect(tool->separate, "toggled",
                     G_CALLBACK(gwy_tool_sfunctions_separate_changed), tool);
    gtk_table_attach(table, tool->separate,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

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

    gwy_tool_sfunctions_update_sensitivity(tool);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_sfunctions_data_switched(GwyTool *gwytool,
                                  GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolSFunctions *tool;
    gboolean ignore;
    gchar xukey[24];
    gchar yukey[24];
    gchar zukey[24];


    plain_tool = GWY_PLAIN_TOOL(gwytool);
    ignore = (data_view == plain_tool->data_view);
    GWY_TOOL_CLASS(gwy_tool_sfunctions_parent_class)->data_switched(gwytool,
                                                                    data_view);

    if (ignore || plain_tool->init_failed)
        return;

    tool = GWY_TOOL_SFUNCTIONS(gwytool);
    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_rect,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);


        g_snprintf(xukey, sizeof(xukey), "/%d/data/cal_xunc", plain_tool->id);
        g_snprintf(yukey, sizeof(yukey), "/%d/data/cal_yunc", plain_tool->id);
        g_snprintf(zukey, sizeof(zukey), "/%d/data/cal_zunc", plain_tool->id);

        if (gwy_container_gis_object_by_name(plain_tool->container, xukey, &(tool->xunc))
            && gwy_container_gis_object_by_name(plain_tool->container, yukey, &(tool->yunc))
            && gwy_container_gis_object_by_name(plain_tool->container, zukey, &(tool->zunc)))
        {
            tool->has_calibration = TRUE;
            gtk_widget_show(tool->separate);
            /*we need to resample uncertainties*/
            tool->xunc = gwy_data_field_new_resampled(tool->xunc,
                                                      gwy_data_field_get_xres(plain_tool->data_field),
                                                      gwy_data_field_get_yres(plain_tool->data_field),
                                                      GWY_INTERPOLATION_BILINEAR);
            tool->yunc = gwy_data_field_new_resampled(tool->yunc,
                                                      gwy_data_field_get_xres(plain_tool->data_field),
                                                      gwy_data_field_get_yres(plain_tool->data_field),
                                                      GWY_INTERPOLATION_BILINEAR);

            tool->zunc = gwy_data_field_new_resampled(tool->zunc,
                                                      gwy_data_field_get_xres(plain_tool->data_field),
                                                      gwy_data_field_get_yres(plain_tool->data_field),
                                                      GWY_INTERPOLATION_BILINEAR);

        } else {
            tool->has_calibration = FALSE;
            gtk_widget_hide(tool->separate);
        }
    }

    gwy_tool_sfunctions_update_curve(tool);
}

static void
gwy_tool_sfunctions_response(GwyTool *tool,
                             gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_sfunctions_parent_class)->response(tool,
                                                               response_id);



    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_sfunctions_apply(GWY_TOOL_SFUNCTIONS(tool));
    else if (response_id == GWY_TOOL_RESPONSE_UPDATE)
        gwy_tool_sfunctions_update_curve(GWY_TOOL_SFUNCTIONS(tool));
}

static void
gwy_tool_sfunctions_data_changed(GwyPlainTool *plain_tool)
{
    gchar xukey[24];
    gchar yukey[24];
    gchar zukey[24];

    g_snprintf(xukey, sizeof(xukey), "/%d/data/cal_xunc", plain_tool->id);
    g_snprintf(yukey, sizeof(yukey), "/%d/data/cal_yunc", plain_tool->id);
    g_snprintf(zukey, sizeof(zukey), "/%d/data/cal_zunc", plain_tool->id);

    if (gwy_container_gis_object_by_name(plain_tool->container, xukey, &(GWY_TOOL_SFUNCTIONS(plain_tool)->xunc))
        && gwy_container_gis_object_by_name(plain_tool->container, yukey, &(GWY_TOOL_SFUNCTIONS(plain_tool)->yunc))
        && gwy_container_gis_object_by_name(plain_tool->container, zukey, &(GWY_TOOL_SFUNCTIONS(plain_tool)->zunc)))
    {
        GWY_TOOL_SFUNCTIONS(plain_tool)->has_calibration = TRUE;
            gtk_widget_show(GWY_TOOL_SFUNCTIONS(plain_tool)->separate);
            /*we need to resample uncertainties*/
            GWY_TOOL_SFUNCTIONS(plain_tool)->xunc = gwy_data_field_new_resampled(GWY_TOOL_SFUNCTIONS(plain_tool)->xunc,
                                                      gwy_data_field_get_xres(plain_tool->data_field),
                                                      gwy_data_field_get_yres(plain_tool->data_field),
                                                      GWY_INTERPOLATION_BILINEAR);
            GWY_TOOL_SFUNCTIONS(plain_tool)->yunc = gwy_data_field_new_resampled(GWY_TOOL_SFUNCTIONS(plain_tool)->yunc,
                                                      gwy_data_field_get_xres(plain_tool->data_field),
                                                      gwy_data_field_get_yres(plain_tool->data_field),
                                                      GWY_INTERPOLATION_BILINEAR);

            GWY_TOOL_SFUNCTIONS(plain_tool)->zunc = gwy_data_field_new_resampled(GWY_TOOL_SFUNCTIONS(plain_tool)->zunc,
                                                      gwy_data_field_get_xres(plain_tool->data_field),
                                                      gwy_data_field_get_yres(plain_tool->data_field),
                                                      GWY_INTERPOLATION_BILINEAR);

    } else {
        GWY_TOOL_SFUNCTIONS(plain_tool)->has_calibration = FALSE;
    }

    gwy_tool_sfunctions_update_curve(GWY_TOOL_SFUNCTIONS(plain_tool));
}

static void
gwy_tool_sfunctions_selection_changed(GwyPlainTool *plain_tool,
                                      gint hint)
{
    GwyToolSFunctions *tool;
    gint n = 0;

    tool = GWY_TOOL_SFUNCTIONS(plain_tool);
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
        gwy_tool_sfunctions_update_curve(tool);
}

static void
gwy_tool_sfunctions_update_sensitivity(GwyToolSFunctions *tool)
{
    gboolean sensitive;
    GSList *l;

    gtk_widget_set_sensitive(tool->update, !tool->args.instant_update);
    gwy_table_hscale_set_sensitive(tool->resolution, tool->args.fixres);

    sensitive = (tool->args.output_type == GWY_SF_ACF
                 || tool->args.output_type == GWY_SF_HHCF
                 || tool->args.output_type == GWY_SF_PSDF
                 || tool->args.output_type == GWY_SF_RPSDF);
    gtk_widget_set_sensitive(tool->interpolation, sensitive);
    gtk_widget_set_sensitive(tool->interpolation_label, sensitive);

    sensitive = (tool->args.output_type == GWY_SF_DA
                 || tool->args.output_type == GWY_SF_CDA
                 || tool->args.output_type == GWY_SF_ACF
                 || tool->args.output_type == GWY_SF_HHCF
                 || tool->args.output_type == GWY_SF_PSDF);
    for (l = tool->direction; l; l = g_slist_next(l))
        gtk_widget_set_sensitive(GTK_WIDGET(l->data), sensitive);
}

static void
gwy_tool_sfunctions_update_curve(GwyToolSFunctions *tool)
{
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *gcmodel, *ugcmodel;
    gdouble sel[4], *xdata, *ydata;
    gint isel[4] = { sizeof("Die, die, GCC!"), 0, 0, 0 };
    gint n, nsel, lineres, w = sizeof("Die, die, GCC!"), h = 0;
    const gchar *title, *xlabel, *ylabel;

    plain_tool = GWY_PLAIN_TOOL(tool);

    n = gwy_graph_model_get_n_curves(tool->gmodel);
    nsel = 0;
    if (plain_tool->data_field) {
        if (!plain_tool->selection
            || !gwy_selection_get_object(plain_tool->selection, 0, sel)) {
            isel[0] = isel[1] = 0;
            w = gwy_data_field_get_xres(plain_tool->data_field);
            h = gwy_data_field_get_yres(plain_tool->data_field);
        }
        else {
            isel[0] = floor(gwy_data_field_rtoj(plain_tool->data_field, sel[0]));
            isel[1] = floor(gwy_data_field_rtoi(plain_tool->data_field, sel[1]));
            isel[2] = floor(gwy_data_field_rtoj(plain_tool->data_field, sel[2]));
            isel[3] = floor(gwy_data_field_rtoi(plain_tool->data_field, sel[3]));

            w = ABS(isel[2] - isel[0]) + 1;
            h = ABS(isel[3] - isel[1]) + 1;
            isel[0] = MIN(isel[0], isel[2]);
            isel[1] = MIN(isel[1], isel[3]);
        }
        if (w >= 4 && h >= 4)
            nsel = 1;
    }

    gtk_widget_set_sensitive(tool->apply, nsel > 0);

    if (nsel == 0 && n == 0)
        return;

    if (nsel == 0 && n > 0) {
        gwy_graph_model_remove_all_curves(tool->gmodel);
        return;
    }

    tool->has_uline = FALSE;
    lineres = tool->args.fixres ? tool->args.resolution : -1;
    switch (tool->args.output_type) {
        case GWY_SF_DH:
        gwy_data_field_area_dh(plain_tool->data_field, NULL,
                               tool->line,
                               isel[0], isel[1], w, h,
                               lineres);
        xlabel = "z";
        ylabel = "ρ";
        if (tool->has_calibration) {
            gwy_data_field_area_dh_uncertainty(plain_tool->data_field,
                                           tool->zunc,
                                           plain_tool->mask_field,
                                           tool->uline,
                                           isel[0], isel[1], w, h,
                                           lineres);
            tool->has_uline = TRUE;
        }

        break;

        case GWY_SF_CDH:
        gwy_data_field_area_cdh(plain_tool->data_field, NULL,
                                tool->line,
                                isel[0], isel[1], w, h,
                                lineres);
        xlabel = "z";
        ylabel = "D";
       if (tool->has_calibration) {
            gwy_data_field_area_cdh_uncertainty(plain_tool->data_field,
                                           tool->zunc,
                                           plain_tool->mask_field,
                                           tool->uline,
                                           isel[0], isel[1], w, h,
                                           lineres);
            tool->has_uline = TRUE;
        }


        break;

        case GWY_SF_DA:
        gwy_data_field_area_da(plain_tool->data_field,
                               tool->line,
                               isel[0], isel[1], w, h,
                               tool->args.direction,
                               lineres);
        xlabel = "tan β";
        ylabel = "ρ";
        break;

        case GWY_SF_CDA:
        gwy_data_field_area_cda(plain_tool->data_field,
                                tool->line,
                                isel[0], isel[1], w, h,
                                tool->args.direction,
                                lineres);
        xlabel = "tan β";
        ylabel = "D";
        break;

        case GWY_SF_ACF:
        gwy_data_field_area_acf(plain_tool->data_field,
                                tool->line,
                                isel[0], isel[1], w, h,
                                tool->args.direction,
                                tool->args.interpolation,
                                lineres);
        xlabel = "τ";
        ylabel = "G";
        if (tool->has_calibration) {
            gwy_data_field_area_acf_uncertainty(plain_tool->data_field,
                                           tool->zunc,
                                           tool->uline,
                                           isel[0], isel[1], w, h,
                                           tool->args.direction,
                                           tool->args.interpolation,
                                           lineres); 
            tool->has_uline = TRUE;
        }

           
        break;

        case GWY_SF_HHCF:
        gwy_data_field_area_hhcf(plain_tool->data_field,
                                 tool->line,
                                 isel[0], isel[1], w, h,
                                 tool->args.direction,
                                 tool->args.interpolation,
                                 lineres);
        xlabel = "τ";
        ylabel = "H";
        if (tool->has_calibration) {
            gwy_data_field_area_hhcf_uncertainty(plain_tool->data_field,
                                           tool->zunc,
                                           tool->uline,
                                           isel[0], isel[1], w, h,
                                           tool->args.direction,
                                           tool->args.interpolation,
                                           lineres); 
      /*        gwy_data_field_hhcf_uncertainty(plain_tool->data_field, tool->zunc,
                          tool->uline, tool->args.direction);*/
            tool->has_uline = TRUE;
        }

        break;

        case GWY_SF_PSDF:
        gwy_data_field_area_psdf(plain_tool->data_field,
                                 tool->line,
                                 isel[0], isel[1], w, h,
                                 tool->args.direction,
                                 tool->args.interpolation,
                                 GWY_WINDOWING_HANN,
                                 lineres);
        xlabel = "k";
        ylabel = "W<sub>1</sub>";
        break;

        case GWY_SF_MINKOWSKI_VOLUME:
        gwy_data_field_area_minkowski_volume(plain_tool->data_field,
                                             tool->line,
                                             isel[0], isel[1], w, h,
                                             lineres);
        xlabel = "z";
        ylabel = "V";
        break;

        case GWY_SF_MINKOWSKI_BOUNDARY:
        gwy_data_field_area_minkowski_boundary(plain_tool->data_field,
                                               tool->line,
                                               isel[0], isel[1], w, h,
                                               lineres);
        xlabel = "z";
        ylabel = "S";
        break;

        case GWY_SF_MINKOWSKI_CONNECTIVITY:
        gwy_data_field_area_minkowski_euler(plain_tool->data_field,
                                            tool->line,
                                            isel[0], isel[1], w, h,
                                            lineres);
        xlabel = "z";
        ylabel = "χ";
        break;

        case GWY_SF_RPSDF:
        gwy_data_field_area_rpsdf(plain_tool->data_field,
                                  tool->line,
                                  isel[0], isel[1], w, h,
                                  tool->args.interpolation,
                                  GWY_WINDOWING_HANN,
                                  lineres);
        xlabel = "k";
        ylabel = "W<sub>r</sub>";
        break;

        case GWY_SF_RACF:
        gwy_data_field_area_racf(plain_tool->data_field,
                                 tool->line,
                                 isel[0], isel[1], w, h,
                                 lineres);
        xlabel = "τ";
        ylabel = "G<sub>r</sub>";
        break;

        default:
        g_return_if_reached();
        break;
    }


    if (nsel > 0 && n == 0) {
        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_model_add_curve(tool->gmodel, gcmodel);
        g_object_set(gcmodel, "mode", GWY_GRAPH_CURVE_LINE, NULL);
        g_object_unref(gcmodel);
        
        if (tool->has_calibration && tool->has_uline) {
           ugcmodel = gwy_graph_curve_model_new();
           gwy_graph_model_add_curve(tool->gmodel, ugcmodel);
           g_object_set(ugcmodel, "mode", GWY_GRAPH_CURVE_LINE, NULL);
           g_object_unref(ugcmodel);
        }
    }
    else {
        gcmodel = gwy_graph_model_get_curve(tool->gmodel, 0);
        if (tool->has_calibration && tool->has_uline)
           if (gwy_graph_model_get_n_curves(tool->gmodel)<2)
           {
               ugcmodel = gwy_graph_curve_model_new();
               gwy_graph_model_add_curve(tool->gmodel, ugcmodel);
               g_object_set(ugcmodel, "mode", GWY_GRAPH_CURVE_LINE, NULL);
               g_object_unref(ugcmodel);
           } 
           else ugcmodel = gwy_graph_model_get_curve(tool->gmodel, 1);
        else if (gwy_graph_model_get_n_curves(tool->gmodel)>1)
           gwy_graph_model_remove_curve(tool->gmodel, 1);
    }


    gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line, 0, 0);
    title = gwy_enum_to_string(tool->args.output_type,
                               sf_types, G_N_ELEMENTS(sf_types));

    g_object_set(gcmodel, "description", title, NULL);
    
    if (tool->has_calibration && tool->has_uline)
    {
        gwy_graph_curve_model_set_data_from_dataline(ugcmodel, tool->uline, 0, 0);
        g_object_set(ugcmodel, "description", "uncertainty", NULL);
    } 

    g_object_set(tool->gmodel,
                 "title", title,
                 "axis-label-bottom", xlabel,
                 "axis-label-left", ylabel,
                 NULL);

    gwy_graph_model_set_units_from_data_line(tool->gmodel, tool->line);

}

static void
gwy_tool_sfunctions_instant_update_changed(GtkToggleButton *check,
                                           GwyToolSFunctions *tool)
{
    tool->args.instant_update = gtk_toggle_button_get_active(check);
    gwy_tool_sfunctions_update_sensitivity(tool);
    if (tool->args.instant_update)
        gwy_tool_sfunctions_update_curve(tool);
}

static void
gwy_tool_sfunctions_resolution_changed(GwyToolSFunctions *tool,
                                       GtkAdjustment *adj)
{
    tool->args.resolution = gwy_adjustment_get_int(adj);
    /* Resolution can be changed only when fixres == TRUE */
    gwy_tool_sfunctions_update_curve(tool);
}

static void
gwy_tool_sfunctions_fixres_changed(GtkToggleButton *check,
                                   GwyToolSFunctions *tool)
{
    tool->args.fixres = gtk_toggle_button_get_active(check);
    gwy_tool_sfunctions_update_curve(tool);
}

static void
gwy_tool_sfunctions_output_type_changed(GtkComboBox *combo,
                                        GwyToolSFunctions *tool)
{
    tool->args.output_type = gwy_enum_combo_box_get_active(combo);
    gwy_tool_sfunctions_update_sensitivity(tool);
    gwy_tool_sfunctions_update_curve(tool);
}

static void
gwy_tool_sfunctions_direction_changed(G_GNUC_UNUSED GObject *button,
                                      GwyToolSFunctions *tool)
{
    tool->args.direction = gwy_radio_buttons_get_current(tool->direction);
    gwy_tool_sfunctions_update_curve(tool);
}

static void
gwy_tool_sfunctions_interpolation_changed(GtkComboBox *combo,
                                          GwyToolSFunctions *tool)
{
    tool->args.interpolation = gwy_enum_combo_box_get_active(combo);
    gwy_tool_sfunctions_update_curve(tool);
}

static void
gwy_tool_sfunctions_options_expanded(GtkExpander *expander,
                                     G_GNUC_UNUSED GParamSpec *pspec,
                                     GwyToolSFunctions *tool)
{
    tool->args.options_visible = gtk_expander_get_expanded(expander);
}

static void
gwy_tool_sfunctions_separate_changed(GtkToggleButton *check,
                                       GwyToolSFunctions *tool)
{
    tool->args.separate = gtk_toggle_button_get_active(check);
}


static void
gwy_tool_sfunctions_apply(GwyToolSFunctions *tool)
{
    GwyPlainTool *plain_tool;
    GwyGraphModel *gmodel, *ugmodel;
    GwyGraphCurveModel *ugcmodel;
    gchar *str, title[50];

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);

    gmodel = gwy_graph_model_duplicate(tool->gmodel);
    if (tool->has_calibration && tool->has_uline && tool->args.separate 
        && gwy_graph_model_get_n_curves(gmodel)==2)
    {
        ugmodel = gwy_graph_model_duplicate(tool->gmodel);
        g_object_get(ugmodel,"title", &str, NULL);
        g_snprintf(title, sizeof(title), "%s uncertainty", str);
        g_object_set(ugmodel, "title", title, NULL);
        g_free(str);

        gwy_graph_model_remove_curve(ugmodel, 0);
        gwy_graph_model_remove_curve(gmodel, 1);

        gwy_app_data_browser_add_graph_model(gmodel, plain_tool->container, TRUE);
        gwy_app_data_browser_add_graph_model(ugmodel, plain_tool->container, TRUE);
        
    }
    else gwy_app_data_browser_add_graph_model(gmodel, plain_tool->container, TRUE);


    g_object_unref(gmodel);


}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
