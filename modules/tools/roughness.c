/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/linestats.h>
#include <libprocess/inttrans.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-tool.h>
#include <app/gwyapp.h>

/******************************************************************************
 * Define
 *****************************************************************************/
#define GWY_TYPE_TOOL_ROUGHNESS           (gwy_tool_roughness_get_type())
#define GWY_TOOL_ROUGHNESS(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                           GWY_TYPE_TOOL_ROUGHNESS, \
                                           GwyToolRoughness))
#define GWY_IS_TOOL_ROUGHNESS(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                           GWY_TYPE_TOOL_ROUGHNESS))
#define GWY_TOOL_ROUGHNESS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                           GWY_TYPE_TOOL_ROUGHNESS, \
                                           GwyToolRoughnessClass))

typedef enum {
    UNITS_NONE,
    UNITS_COORDS,
    UNITS_VALUE,
    UNITS_SLOPE
} UnitsType;

typedef enum {
   ROUGHNESS_SET_AMPLITUDE,
   ROUGHNESS_SET_SPATIAL,
   ROUGHNESS_SET_HYBRID,
   ROUGHNESS_SET_FUNCTIONAL
} GwyRoughnessSet;

typedef enum {
    GWY_ROUGHNESS_GRAPH_TEXTURE   = 0,
    GWY_ROUGHNESS_GRAPH_WAVINESS  = 1,
    GWY_ROUGHNESS_GRAPH_ROUGHNESS = 2,
    GWY_ROUGHNESS_GRAPH_ADF       = 3,
    GWY_ROUGHNESS_GRAPH_BRC       = 4,
    GWY_ROUGHNESS_GRAPH_PC        = 5
} GwyRoughnessGraph;

typedef struct {
    GwyDataLine *texture;   /**/
    GwyDataLine *roughness;   /**/
    GwyDataLine *waviness;   /**/

    GwyDataLine *adf; /* */
    GwyDataLine *brc;
    GwyDataLine *pc;  /* Peak count */
} GwyRoughnessProfiles;

typedef enum {
    PARAM_RA,
    PARAM_RQ,
    PARAM_RT,
    PARAM_RV,
    PARAM_RP,
    PARAM_RTM,
    PARAM_RVM,
    PARAM_RPM,
    PARAM_R3Z,
    PARAM_R3Z_ISO,
    PARAM_RZ,
    PARAM_RZ_ISO,
    PARAM_RSK,
    PARAM_RKU,
    PARAM_PT,
    PARAM_WA,
    PARAM_WQ,
    PARAM_WY,
    PARAM_PC,
    PARAM_S,
    PARAM_SM,
    PARAM_LA,
    PARAM_LQ,
    PARAM_HSC,
    PARAM_D,
    PARAM_DA,
    PARAM_DQ,
    PARAM_L0,
    PARAM_L,
    PARAM_LR,
    PARAM_HTP,
    PARAM_RK,
    PARAM_RKP,
    PARAM_RVK,
    PARAM_MR1,
    PARAM_MR2,
    ROUGHNESS_NPARAMS
} GwyRoughnessParameter;

typedef struct {
    GwyRoughnessParameter param;
    GwyRoughnessSet set;
    const gchar *symbol;
    const gchar *name;
    UnitsType units;
    gboolean same_units;
} GwyRoughnessParameterInfo;

typedef struct {
    gint thickness;
    gint cutoff;
    gint tm;
    gint vm;
    gint pm;
    gdouble pc_threshold;
    GwyInterpolationType interpolation;
} ToolArgs;

static const ToolArgs default_args = {
    1,
    5,
    5,
    5,
    5,
    0.9,
    GWY_INTERPOLATION_LINEAR,
};

typedef struct _GwyToolRoughness      GwyToolRoughness;
typedef struct _GwyToolRoughnessClass GwyToolRoughnessClass;

struct _GwyToolRoughness {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkTreeStore *store;
    gdouble *params;

    /* data */
    GwyDataLine *dataline;
    GwyRoughnessProfiles profiles;
    GwyRoughnessGraph graph_type;

    /* graph */
    GwyGraphModel *graphmodel;
    GtkWidget *graph;

    GwyGraphModel *graphmodel_profile;
    GtkWidget *graph_profile;

    GwySIValueFormat *slope_format;

    //GtkWidget *save;
    GtkWidget *graph_out;

    GtkWidget *options;
    GtkObject *thickness;
    GtkObject *cutoff;
    GtkWidget *cutlength;
    GtkWidget *interpolation;
    GtkWidget *apply;

    /*  */
    GtkWidget *pm;
    GtkWidget *vm;
    GtkWidget *tm;
    GtkWidget *pc_threshold;

    /* potential class data */
    GwySIValueFormat *none_format;
    GType layer_type_line;
};

struct _GwyToolRoughnessClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register                      (void);
static GType    gwy_tool_roughness_get_type          (void) G_GNUC_CONST;
static void     gwy_tool_roughness_finalize          (GObject *object);
static void     gwy_tool_roughness_init_params       (GwyToolRoughness *tool);
static void     gwy_tool_roughness_init_dialog       (GwyToolRoughness *tool);
static GtkWidget* gwy_tool_roughness_param_view_new(GwyToolRoughness *tool);
static void     gwy_tool_roughness_data_switched     (GwyTool *gwytool,
                                                      GwyDataView *data_view);
static void     gwy_tool_roughness_response          (GwyTool *tool,
                                                      gint response_id);
static void     gwy_tool_roughness_data_changed      (GwyPlainTool *plain_tool);
static void     gwy_tool_roughness_update            (GwyToolRoughness *tool);
static void     gwy_tool_roughness_update_parameters (GwyToolRoughness *tool);
static void     gwy_tool_roughness_update_graphs     (GwyToolRoughness *tool);

static void     gwy_tool_roughness_selection_changed (GwyPlainTool *plain_tool,
                                                      gint hint);
static void     gwy_tool_roughness_interpolation_changed
                                                     (GtkComboBox *combo,
                                                      GwyToolRoughness *tool);
static void     gwy_tool_roughness_thickness_changed (GwyToolRoughness *tool,
                                                      GtkAdjustment *adj);
static void     gwy_tool_roughness_cutoff_changed    (GwyToolRoughness *tool,
                                                      GtkAdjustment *adj);
static void     gwy_tool_roughness_graph_changed     (GtkWidget *combo,
                                                      GwyToolRoughness *tool);
static void     gwy_tool_roughness_apply             (GwyToolRoughness *tool);

static void     gwy_data_line_data_discrete          (gdouble *x,
                                                      gdouble *y,
                                                      gint a_res,
                                                      GwyDataLine *data_line,
                                                      GwyInterpolationType interpolation);
static void     gwy_data_line_line_rotate2           (GwyDataLine *data_line,
                                                      gdouble angle,
                                                      GwyInterpolationType interpolation);
static void     gwy_math_quicksort(gdouble *array, gint *index, gint low, gint high);
//static void     gwy_tool_stats_save                  (GwyToolStats *tool);
static void     gwy_tool_roughness_set_data_from_profile (GwyRoughnessProfiles profiles,
                                                      GwyDataLine *a,
                                                      gint cutoff,
                                                      gint interpolation);
static gint  gwy_tool_roughness_peaks                (GwyDataLine *data_line,
                                                      gdouble *peaks,
                                                      gint from, gint to,
                                                      gdouble threshold, gint k,
                                                      gboolean symmetrical);
static gdouble  gwy_tool_roughness_Xa                (GwyDataLine *data_line);
static gdouble  gwy_tool_roughness_Xq                (GwyDataLine *data_line);
static gdouble  gwy_tool_roughness_Xvm               (GwyDataLine *data_line,
                                                      gint m, gint k);
static gdouble  gwy_tool_roughness_Xpm               (GwyDataLine *data_line,
                                                      gint m, gint k);
static gdouble  gwy_tool_roughness_Xtm               (GwyDataLine *data_line,
                                                      gint m, gint k);
static gdouble  gwy_tool_roughness_Xz                (GwyDataLine *data_line);
static gdouble  gwy_tool_roughness_Xsk               (GwyDataLine *data_line);
static gdouble  gwy_tool_roughness_Xku               (GwyDataLine *data_line);

static gdouble  gwy_tool_roughness_Pc                (GwyDataLine *data_line,
                                                      gdouble threshold);
/*
static gdouble  gwy_tool_roughness_HSC               (GwyDataLine *data_line);
*/

static gdouble  gwy_tool_roughness_Da                (GwyDataLine *data_line);
static gdouble  gwy_tool_roughness_Dq                (GwyDataLine *data_line);
/*
static gdouble  gwy_tool_roughness_l                 (GwyDataLine *data_line);
*/
static gdouble  gwy_tool_roughness_l0                (GwyDataLine *data_line);
static gdouble  gwy_tool_roughness_lr                (GwyDataLine *data_line);

static void     gwy_tool_roughness_distribution   (GwyDataLine *data_line, GwyDataLine *distr);
static void     gwy_tool_roughness_graph_adf         (GwyRoughnessProfiles profiles);
static void     gwy_tool_roughness_graph_brc         (GwyRoughnessProfiles profiles);
static void     gwy_tool_roughness_graph_pc          (GwyRoughnessProfiles profiles);

static const GwyRoughnessParameterInfo parameters[] = {
    { -1,            ROUGHNESS_SET_AMPLITUDE,  NULL,                                   N_("Amplitude"),                                                0,            FALSE, },
    { PARAM_RA,      ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>a</sub>",                 N_("Roughness average"),                                        UNITS_VALUE,  FALSE, },
    { PARAM_RQ,      ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>q</sub>",                 N_("Root mean square roughness"),                               UNITS_VALUE,  FALSE, },
    { PARAM_RT,      ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>t</sub>",                 N_("Maximum height of the roughness"),                          UNITS_VALUE,  FALSE, },
    { PARAM_RV,      ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>v</sub>",                 N_("Maximum roughness valley depth"),                           UNITS_VALUE,  FALSE, },
    { PARAM_RP,      ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>p</sub>",                 N_("Maximum roughness peak height"),                            UNITS_VALUE,  FALSE, },
    { PARAM_RTM,     ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>tm</sub>",                N_("Average maximum height of the roughness"),                  UNITS_VALUE,  FALSE, },
    { PARAM_RVM,     ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>vm</sub>",                N_("Average maximum roughness valley depth"),                   UNITS_VALUE,  FALSE, },
    { PARAM_RPM,     ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>pm</sub>",                N_("Average maximum roughness peak height"),                    UNITS_VALUE,  FALSE, },
    { PARAM_R3Z,     ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>3z</sub>",                N_("Average third highest peak to third lowest valley height"), UNITS_VALUE,  FALSE, },
    { PARAM_R3Z_ISO, ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>3z ISO</sub>",            N_("Average third highest peak to third lowest valley height"), UNITS_VALUE,  FALSE, },
    { PARAM_RZ,      ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>z</sub>",                 N_("Average maximum height of the profile"),                    UNITS_VALUE,  FALSE, },
    { PARAM_RZ_ISO,  ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>z ISO</sub>",             N_("Average maximum height of the roughness"),                  UNITS_VALUE,  FALSE, },
    { PARAM_RSK,     ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>sk</sub>",                N_("Skewness"),                                                 UNITS_NONE,   FALSE, },
    { PARAM_RKU,     ROUGHNESS_SET_AMPLITUDE,  "<i>R</i><sub>ku</sub>",                N_("Kurtosis"),                                                 UNITS_NONE,   FALSE, },
    { PARAM_PT,      ROUGHNESS_SET_AMPLITUDE,  "<i>W</i><sub>a</sub>",                 N_("Waviness average"),                                         UNITS_VALUE,  FALSE, },
    { PARAM_WA,      ROUGHNESS_SET_AMPLITUDE,  "<i>W</i><sub>q</sub>",                 N_("Root mean square waviness"),                                UNITS_VALUE,  FALSE, },
    { PARAM_WQ,      ROUGHNESS_SET_AMPLITUDE,  "<i>W</i><sub>y</sub>=W<sub>max</sub>", N_("Waviness maximum height"),                                  UNITS_VALUE,  FALSE, },
    { PARAM_WY,      ROUGHNESS_SET_AMPLITUDE,  "<i>P</i><sub>t</sub>",                 N_("Maximum height of the profile"),                            UNITS_VALUE,  FALSE, },
    { -1,            ROUGHNESS_SET_SPATIAL,    NULL,                                   N_("Spatial"),                                                  0,            FALSE, },
    { PARAM_PC,      ROUGHNESS_SET_SPATIAL,    "<i>S</i>",                             N_("Mean spacing of local peaks of the profile"),               UNITS_COORDS, FALSE, },
    { PARAM_S,       ROUGHNESS_SET_SPATIAL,    "<i>S</i><sub>m</sub>",                 N_("Mean spacing of profile irregularities"),                   UNITS_COORDS, FALSE, },
    { PARAM_SM,      ROUGHNESS_SET_SPATIAL,    "<i>D</i>",                             N_("Profile peak density"),                                     UNITS_COORDS, FALSE, },
    { PARAM_LA,      ROUGHNESS_SET_SPATIAL,    "<i>P</i><sub>c</sub>",                 N_("Peak count (peak density)"),                                UNITS_NONE,   FALSE, },
    { PARAM_LQ,      ROUGHNESS_SET_SPATIAL,    "HSC",                                  N_("Hight spot count"),                                         UNITS_NONE,   FALSE, },
    { PARAM_HSC,     ROUGHNESS_SET_SPATIAL,    "λ<sub>a</sub>",                        N_("Average wavelength of the profile"),                        UNITS_COORDS, FALSE, },
    { PARAM_D,       ROUGHNESS_SET_SPATIAL,    "λ<sub>q</sub>",                        N_("Root mean square (RMS) wavelength of the profile"),         UNITS_COORDS, FALSE, },
    { -1,            ROUGHNESS_SET_HYBRID,     NULL,                                   N_("Hybrid"),                                                   0,            FALSE, },
    { PARAM_DA,      ROUGHNESS_SET_HYBRID,     "Δ<sub>a</sub>",                        N_("Average absolute slope"),                                   UNITS_SLOPE,  FALSE, },
    { PARAM_DQ,      ROUGHNESS_SET_HYBRID,     "Δ<sub>q</sub>",                        N_("Root mean square (RMS) slope"),                             UNITS_SLOPE,  FALSE, },
    { PARAM_L0,      ROUGHNESS_SET_HYBRID,     "L<sub>0</sub>",                        N_("Developed profile length"),                                 UNITS_COORDS, TRUE,  },
    { PARAM_L,       ROUGHNESS_SET_HYBRID,     "<i>l</i><sub>r</sub>",                 N_("Profile length ratio"),                                     UNITS_COORDS, TRUE,  },
    { PARAM_LR,      ROUGHNESS_SET_HYBRID,     "<i>L</i>",                             N_("Length"),                                                   UNITS_NONE,   TRUE,  },
    { -1,            ROUGHNESS_SET_FUNCTIONAL, NULL,                                   N_("Functional"),                                               0,            FALSE, },
    { PARAM_HTP,     ROUGHNESS_SET_FUNCTIONAL, "<i>H</i><sub>tp</sub>",                N_("XXX"),                                                      UNITS_COORDS, FALSE, },
    { PARAM_RK,      ROUGHNESS_SET_FUNCTIONAL, "<i>R</i><sub>k</sub>",                 N_("XXX"),                                                      UNITS_COORDS, FALSE, },
    { PARAM_RKP,     ROUGHNESS_SET_FUNCTIONAL, "<i>R</i><sub>pk</sub>",                N_("XXX"),                                                      UNITS_COORDS, FALSE, },
    { PARAM_RVK,     ROUGHNESS_SET_FUNCTIONAL, "<i>R</i><sub>vk</sub>",                N_("XXX"),                                                      UNITS_COORDS, FALSE, },
    { PARAM_MR1,     ROUGHNESS_SET_FUNCTIONAL, "<i>M</i><sub>r1</sub>",                N_("XXX"),                                                      UNITS_COORDS, FALSE, },
    { PARAM_MR2,     ROUGHNESS_SET_FUNCTIONAL, "<i>M</i><sub>r2</sub>",                N_("XXX"),                                                      UNITS_COORDS, FALSE, },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculate surface profile parameters."),
    "Martin Hasoň <hasonm@physics.muni.cz>",
    "1.0",
    "Martin Hasoň",
    "2006",
};

static const gchar interpolation_key[] = "/module/roughness/interpolation";
static const gchar cutoff_key[]        = "/module/roughness/cutoff";
static const gchar thickness_key[]     = "/module/roughness/thickness";
static const gchar pm_key[]            = "/module/roughness/pm";
static const gchar vm_key[]            = "/module/roughness/vm";
static const gchar tm_key[]            = "/module/roughness/tm";
static const gchar pc_threshold_key[]  = "/module/roughness/pc_threshold";

GWY_MODULE_QUERY(module_info)
G_DEFINE_TYPE(GwyToolRoughness, gwy_tool_roughness, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_ROUGHNESS);
    return TRUE;
}

static void
gwy_tool_roughness_class_init(GwyToolRoughnessClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_roughness_finalize;

    tool_class->stock_id = GWY_STOCK_ISO_ROUGHNESS;
    tool_class->title = _("Roughness");
    tool_class->tooltip = _("Calculate roughness parameters.");
    tool_class->prefix = "/module/roughness";
    tool_class->default_width = 400;
    tool_class->default_height = 400;
    tool_class->data_switched = gwy_tool_roughness_data_switched;
    tool_class->response = gwy_tool_roughness_response;

    ptool_class->data_changed = gwy_tool_roughness_data_changed;
    ptool_class->selection_changed = gwy_tool_roughness_selection_changed;
}

static void
gwy_tool_roughness_finalize(GObject *object)
{
    GwyToolRoughness *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_ROUGHNESS(object);

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_set_int32_by_name(settings, thickness_key,
                                    tool->args.thickness);
    gwy_container_set_int32_by_name(settings, cutoff_key,
                                    tool->args.cutoff);
    gwy_container_set_enum_by_name(settings, interpolation_key,
                                   tool->args.interpolation);

    g_free(tool->params);
    gwy_object_unref(tool->store);
    gwy_object_unref(tool->dataline);
    gwy_si_unit_value_format_free(tool->none_format);
    if (tool->slope_format)
        gwy_si_unit_value_format_free(tool->slope_format);

    G_OBJECT_CLASS(gwy_tool_roughness_parent_class)->finalize(object);
}

static void
gwy_tool_roughness_init(GwyToolRoughness *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_line = gwy_plain_tool_check_layer_type(plain_tool,
                                                            "GwyLayerLine");
    if (!tool->layer_type_line)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_VFMARKUP;
    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_int32_by_name(settings, thickness_key,
                                    &tool->args.thickness);
    gwy_container_gis_int32_by_name(settings, cutoff_key,
                                    &tool->args.cutoff);
    gwy_container_gis_enum_by_name(settings, interpolation_key,
                                   &tool->args.interpolation);

    tool->none_format = g_new0(GwySIValueFormat, 1);
    tool->none_format->magnitude = 1.0;
    tool->none_format->precision = 3;
    gwy_si_unit_value_format_set_units(tool->none_format, "");

    /* FIXME: To prevent crashes. Remove here, add real format calculation. */
    tool->slope_format = g_new0(GwySIValueFormat, 1);
    tool->slope_format->magnitude = 1.0;
    tool->slope_format->precision = 3;
    gwy_si_unit_value_format_set_units(tool->slope_format, "");

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_line,
                                     "line");

    gwy_tool_roughness_init_params(tool);
    gwy_tool_roughness_init_dialog(tool);
}

static void
gwy_tool_roughness_init_params(GwyToolRoughness *tool)
{
    const GwyRoughnessParameterInfo *pinfo;
    GtkTreeIter siter, iter;
    guint i;

    tool->store = gtk_tree_store_new(1, G_TYPE_POINTER);
    tool->params = g_new0(gdouble, ROUGHNESS_NPARAMS);

    for (i = 0; i < G_N_ELEMENTS(parameters); i++) {
        pinfo = parameters + i;
        if (pinfo->param == -1)
            gtk_tree_store_insert_with_values(tool->store, &siter, NULL,
                                              G_MAXINT, 0, pinfo, -1);
        else
            gtk_tree_store_insert_with_values(tool->store, &iter, &siter,
                                              G_MAXINT, 0, pinfo, -1);
    }
}

static void
gwy_tool_roughness_init_dialog(GwyToolRoughness *tool)
{
    static const GwyEnum graph_types[] =  {
        { N_("Texture"),    GWY_ROUGHNESS_GRAPH_TEXTURE,   },
        { N_("Waviness"),   GWY_ROUGHNESS_GRAPH_WAVINESS,  },
        { N_("Roughness"),  GWY_ROUGHNESS_GRAPH_ROUGHNESS, },
        { N_("ADF"),        GWY_ROUGHNESS_GRAPH_ADF,       },
        { N_("BRC"),        GWY_ROUGHNESS_GRAPH_BRC,       },
        { N_("Peak Count"), GWY_ROUGHNESS_GRAPH_PC,        },
    };

    /*
    static const GwyEnum graphs[] = {
        { N_("adf"), GWY_ROUGHNESS_GRAPH_ADF, },
        { N_("brc"), GWY_ROUGHNESS_GRAPH_BRC, },
        { N_("pc"), GWY_ROUGHNESS_GRAPH_PC, },
    };
    */

    GtkDialog *dialog;
    GtkWidget *dialog_vbox, *hbox, *vbox_left, *vbox_right, *table;
    GtkWidget *scwin, *treeview;
    GtkWidget *label;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    dialog_vbox = GTK_DIALOG(dialog)->vbox;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox, TRUE, TRUE, 0);

    vbox_left = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(vbox_left);
    gtk_box_pack_start(GTK_BOX(hbox), vbox_left, TRUE, TRUE, 0);

    vbox_right = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(vbox_right);
    gtk_box_pack_start(GTK_BOX(hbox), vbox_right, TRUE, TRUE, 0);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox_left), scwin, TRUE, TRUE, 0);

    treeview = gwy_tool_roughness_param_view_new(tool);
    gtk_container_add(GTK_CONTAINER(scwin), treeview);

    table = gtk_table_new(1, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox_left), table, FALSE, FALSE, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Graph:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    tool->graph_out
        = gwy_enum_combo_box_new(graph_types, G_N_ELEMENTS(graph_types),
                                 G_CALLBACK(gwy_tool_roughness_graph_changed), tool,
                                 tool->graph_type, TRUE);
    gtk_table_attach(GTK_TABLE(table), tool->graph_out, 1, 2, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    table = gtk_table_new(4, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox_left), table, FALSE, FALSE, 0);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    /* cut-off */
    tool->cutoff = gtk_adjustment_new(tool->args.cutoff, 0, 20, 1, 5, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), 1, _("_Cut-off (%):"), NULL,
                            tool->cutoff, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(tool->cutoff, "value-changed",
                             G_CALLBACK(gwy_tool_roughness_cutoff_changed),
                             tool);

    /*label = tool->cutlength = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 1, 2,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);*/

    tool->thickness = gtk_adjustment_new(tool->args.thickness, 1, 128, 1, 10, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), 2, _("_Thickness:"), NULL,
                            tool->thickness, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(tool->thickness, "value-changed",
                             G_CALLBACK(gwy_tool_roughness_thickness_changed),
                             tool);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    tool->interpolation = gwy_enum_combo_box_new
                           (gwy_interpolation_type_get_enum(), -1,
                            G_CALLBACK(gwy_tool_roughness_interpolation_changed),
                            tool, tool->args.interpolation, TRUE);
    gtk_table_attach(GTK_TABLE(table), tool->interpolation,
                     1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    tool->graphmodel_profile = gwy_graph_model_new();
    tool->graph_profile = gwy_graph_new(tool->graphmodel_profile);
    gtk_widget_set_size_request(tool->graph_profile, 300, 250);
    gwy_graph_enable_user_input(GWY_GRAPH(tool->graph_profile), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox_right), tool->graph_profile, TRUE, TRUE, 0);

    tool->graphmodel = gwy_graph_model_new();
    tool->graph = gwy_graph_new(tool->graphmodel);
    gtk_widget_set_size_request(tool->graph, 300, 250);
    gwy_graph_enable_user_input(GWY_GRAPH(tool->graph), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox_right), tool->graph, TRUE, TRUE, 2);

    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    //tool->save = gtk_dialog_add_button(dialog, GTK_STOCK_SAVE,
    //                                   GWY_TOOL_ROUGHNESS_RESPONSE_SAVE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);
    //gtk_widget_set_sensitive(tool->save, FALSE);

    gtk_widget_show_all(dialog_vbox);
}

static void
render_symbol(G_GNUC_UNUSED GtkTreeViewColumn *column,
              GtkCellRenderer *renderer,
              GtkTreeModel *model,
              GtkTreeIter *iter,
              G_GNUC_UNUSED gpointer user_data)
{
    const GwyRoughnessParameterInfo *pinfo;

    gtk_tree_model_get(model, iter, 0, &pinfo, -1);
    if (pinfo->symbol)
        g_object_set(renderer, "markup", pinfo->symbol, NULL);
    else
        g_object_set(renderer, "text", "", NULL);
}

static void
render_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer user_data)
{
    const GwyRoughnessParameterInfo *pinfo;
    gboolean header;

    gtk_tree_model_get(model, iter, 0, &pinfo, -1);
    header = (pinfo->param == -1);
    g_object_set(renderer,
                 "ellipsize", header ? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_END ,
                 "weight", header ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
                 "text", pinfo->name,
                 NULL);
}

static void
render_value(G_GNUC_UNUSED GtkTreeViewColumn *column,
             GtkCellRenderer *renderer,
             GtkTreeModel *model,
             GtkTreeIter *iter,
             gpointer user_data)
{
    GwyToolRoughness *tool = (GwyToolRoughness*)user_data;
    const GwyRoughnessParameterInfo *pinfo;
    const GwySIValueFormat *vf;
    gdouble value;
    gchar buf[64];

    gtk_tree_model_get(model, iter, 0, &pinfo, -1);
    if (pinfo->param == -1) {
        g_object_set(renderer, "text", "", NULL);
        return;
    }
    /* TODO:
    if (tool->same_units) ...
    */
    switch (pinfo->units) {
        case UNITS_NONE:
        vf = tool->none_format;
        break;

        case UNITS_COORDS:
        vf = GWY_PLAIN_TOOL(tool)->coord_format;
        break;

        case UNITS_VALUE:
        vf = GWY_PLAIN_TOOL(tool)->value_format;
        break;

        case UNITS_SLOPE:
        vf = tool->slope_format;
        break;

        default:
        g_return_if_reached();
        break;
    }
    value = tool->params[pinfo->param];
    g_snprintf(buf, sizeof(buf), "%.*f%s%s",
               vf->precision, value/vf->magnitude,
               *vf->units ? " " : "", vf->units);
    g_object_set(renderer, "text", buf, NULL);
}

static GtkWidget*
gwy_tool_roughness_param_view_new(GwyToolRoughness *tool)
{
    GtkWidget *treeview;
    GtkTreeSelection *selection;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tool->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

    column = gtk_tree_view_column_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_symbol, tool, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
                 "weight-set", TRUE,
                 "ellipsize-set", TRUE,
                 NULL);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_name, tool, NULL);
    /* FIXME: The column is not actually set expandable without this explicit
     * call.  A Gtk+ bug? */
    gtk_tree_view_column_set_expand(column, TRUE);

    column = gtk_tree_view_column_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_value, tool, NULL);

    return treeview;
}

static void
gwy_tool_roughness_response(GwyTool *tool,
                          gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_roughness_parent_class)->response(tool, response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_roughness_apply(GWY_TOOL_ROUGHNESS(tool));
    //if (response_id == GWY_TOOL_ROUGHNESS_RESPONSE_SAVE)
    //    gwy_tool_roughness_save(GWY_TOOL_ROUGHNESS(tool));
}

static void
gwy_tool_roughness_data_switched(GwyTool *gwytool,
                                 GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolRoughness *tool;

    GWY_TOOL_CLASS(gwy_tool_roughness_parent_class)->data_switched(gwytool,
                                                                   data_view);

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_ROUGHNESS(gwytool);
    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_line,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
    }

    gwy_tool_roughness_update(tool);
}

static void
gwy_tool_roughness_data_changed(GwyPlainTool *plain_tool)
{
    GwyToolRoughness *tool;

    tool = GWY_TOOL_ROUGHNESS(plain_tool);
    gwy_tool_roughness_update(tool);

    //gtk_widget_set_sensitive(tool->save, data_view != NULL);
}

static void
gwy_tool_roughness_selection_changed(GwyPlainTool *plain_tool,
                                     gint hint)
{
    GwyToolRoughness *tool;
    gint n;

    tool = GWY_TOOL_ROUGHNESS(plain_tool);
    g_return_if_fail(hint <= 0);


    if (plain_tool->selection)
    {
        n = gwy_selection_get_data(plain_tool->selection, NULL);
        g_return_if_fail(n == 0 || n == 1);
    }

    gwy_tool_roughness_update(tool);
    //gtk_widget_set_sensitive(tool->apply, n > 0);*/
}

static void
gwy_tool_roughness_interpolation_changed(GtkComboBox *combo,
                                        GwyToolRoughness *tool)
{
    tool->args.interpolation = gwy_enum_combo_box_get_active(combo);
    gwy_tool_roughness_update(tool);
}

static void
gwy_tool_roughness_thickness_changed(GwyToolRoughness *tool,
                                     GtkAdjustment *adj)
{
    tool->args.thickness = gwy_adjustment_get_int(adj);
    gwy_tool_roughness_update(tool);
}

static void
gwy_tool_roughness_cutoff_changed(GwyToolRoughness *tool,
                                  GtkAdjustment *adj)
{
    tool->args.cutoff = gwy_adjustment_get_int(adj);
    gwy_tool_roughness_update(tool);
}

static void
gwy_tool_roughness_graph_changed(GtkWidget *combo, GwyToolRoughness *tool)
{
    tool->graph_type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    gwy_tool_roughness_update_graphs(tool);
}

static void
gwy_tool_roughness_apply(GwyToolRoughness *tool)
{
    GwyPlainTool *plain_tool;
    GwyGraphModel *graphmodel;
    GwyGraphCurveModel *graphcmodel;
    gchar *s;
    gint n;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);
    n = gwy_selection_get_data(plain_tool->selection, NULL);
    g_return_if_fail(n);

    graphmodel = gwy_graph_model_new_alike(tool->graphmodel);
    g_object_set(graphmodel, "label-visible", TRUE, NULL);
    graphcmodel = gwy_graph_model_get_curve(tool->graphmodel, 0);
    graphcmodel = gwy_graph_curve_model_duplicate(graphcmodel);
    gwy_graph_model_add_curve(graphmodel, graphcmodel);
    g_object_unref(graphcmodel);
    g_object_get(graphcmodel, "description", &s, NULL);
    g_object_set(graphcmodel, "title", s, NULL);
    g_free(s);
    gwy_app_data_browser_add_graph_model(graphmodel, plain_tool->container,
                                         TRUE);
    g_object_unref(graphmodel);
}

/*
static void
gwy_tool_roughness_save(GwyToolStats *tool)
{
    GwyContainer *container;
    GwyDataField *data_field;
    GtkWidget *dialog;
    GwySIUnit *siunitxy, *siunitarea;
    gdouble xreal, yreal, q;
    GwyPlainTool *plain_tool;
    GwySIValueFormat *vf = NULL;
    const guchar *title;
    gboolean mask_in_use;
    gint response, id;
    gchar *key, *filename_sys;
    gchar *ix, *iy, *iw, *ih, *rx, *ry, *rw, *rh, *muse, *uni;
    gchar *avg, *min, *max, *median, *rms, *ra, *skew, *kurtosis;
    gchar *area, *projarea, *theta, *phi;
    FILE *fh;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->container);
    if (!tool->results_up_to_date)
        gwy_tool_stats_update_labels(tool);
    */
    /* Copy everything as user can switch data during the Save dialog (though
     * he cannot destroy them, so references are not necessary) */
/*    mask_in_use = tool->args.use_mask && plain_tool->mask_field;
    res = tool->results;
    container = plain_tool->container;
    data_field = plain_tool->data_field;
    id = plain_tool->id;

    dialog = gtk_file_chooser_dialog_new(_("Save Statistical Quantities"),
                                         GTK_WINDOW(GWY_TOOL(tool)->dialog),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        gwy_app_get_current_directory());
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    filename_sys = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    gtk_widget_destroy(dialog);

    if (!filename_sys || response != GTK_RESPONSE_OK) {
        g_free(filename_sys);
        return;
    }

    fh = g_fopen(filename_sys, "w");
    if (!fh) {
        gint myerrno;
        gchar *filename_utf8;

        myerrno = errno;
        filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
        dialog = gtk_message_dialog_new(GTK_WINDOW(GWY_TOOL(tool)->dialog), 0,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Saving of `%s' failed"),
                                        filename_utf8);
        g_free(filename_sys);
        g_free(filename_utf8);
        gtk_message_dialog_format_secondary_text
                                       (GTK_MESSAGE_DIALOG(dialog),
                                        _("Cannot open file for writing: %s."),
                                        g_strerror(myerrno));
        gtk_widget_show_all(dialog);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    g_free(filename_sys);

    fputs(_("Statistical Quantities"), fh);
    fputs("\n\n", fh);
    */
    /* Channel information */
/*    if (gwy_container_gis_string_by_name(container, "/filename", &title))
        fprintf(fh, _("File:              %s\n"), title);

    key = g_strdup_printf("/%d/data/title", id);
    if (gwy_container_gis_string_by_name(container, key, &title))
        fprintf(fh, _("Data channel:      %s\n"), title);
    g_free(key);

    fputs("\n", fh);

    iw = g_strdup_printf("%d", res.isel[2]);
    ih = g_strdup_printf("%d", res.isel[3]);
    ix = g_strdup_printf("%d", res.isel[0]);
    iy = g_strdup_printf("%d", res.isel[1]);

    vf = gwy_data_field_get_value_format_xy(data_field,
                                            GWY_SI_UNIT_FORMAT_PLAIN, vf);
    rw = g_strdup_printf("%.*f", vf->precision, res.sel[2]/vf->magnitude);
    rh = g_strdup_printf("%.*f", vf->precision, res.sel[3]/vf->magnitude);
    rx = g_strdup_printf("%.*f", vf->precision, res.sel[0]/vf->magnitude);
    ry = g_strdup_printf("%.*f", vf->precision, res.sel[1]/vf->magnitude);
    uni = g_strdup(vf->units);


    vf = gwy_data_field_get_value_format_z(data_field,
                                           GWY_SI_UNIT_FORMAT_PLAIN, vf);
    avg = fmt_val(avg);
    min = fmt_val(min);
    max = fmt_val(max);
    median = fmt_val(median);
    ra = fmt_val(ra);
    rms = fmt_val(rms);

    skew = g_strdup_printf("%2.3g", res.skew);
    kurtosis = g_strdup_printf("%2.3g", res.kurtosis);

    siunitxy = gwy_data_field_get_si_unit_xy(data_field);
    siunitarea = gwy_si_unit_power(siunitxy, 2, NULL);
    xreal = gwy_data_field_get_xreal(data_field);
    yreal = gwy_data_field_get_xreal(data_field);
    q = xreal/gwy_data_field_get_xres(data_field)
        *yreal/gwy_data_field_get_yres(data_field);
    vf = gwy_si_unit_get_format_with_resolution(siunitarea,
                                                GWY_SI_UNIT_FORMAT_PLAIN,
                                                xreal*yreal, q, vf);
    g_object_unref(siunitarea);

    area = tool->same_units ? fmt_val(area) : g_strdup(_("N.A."));
    projarea = fmt_val(projarea);

    gwy_si_unit_value_format_free(vf);
    vf = tool->angle_format;

    theta = ((tool->same_units && !mask_in_use)
             ? fmt_val(theta) : g_strdup(_("N.A.")));
    phi = ((tool->same_units && !mask_in_use)
           ? fmt_val(phi) : g_strdup(_("N.A.")));

    fprintf(fh,
            _("Selected area:     %s × %s at (%s, %s) px\n"
              "                   %s × %s at (%s, %s) %s\n"
              "Mask in use:       %s\n"
              "\n"
              "Average value:     %s\n"
              "Minimum:           %s\n"
              "Maximum:           %s\n"
              "Median:            %s\n"
              "Ra:                %s\n"
              "Rms:               %s\n"
              "Skew:              %s\n"
              "Kurtosis:          %s\n"
              "Surface area:      %s\n"
              "Projected area:    %s\n"
              "Inclination theta: %s\n"
              "Inclination phi:   %s\n"),
            iw, ih, ix, iy,
            rw, rh, rx, ry, uni,
            muse,
            avg, min, max, median, ra, rms, skew, kurtosis,
            area, projarea, theta, phi);

    fclose(fh);

    g_free(ix);
    g_free(iy);
    g_free(iw);
    g_free(ih);
    g_free(rx);
    g_free(ry);
    g_free(rw);
    g_free(rh);
    g_free(avg);
    g_free(min);
    g_free(max);
    g_free(median);
    g_free(ra);
    g_free(rms);
    g_free(skew);
    g_free(kurtosis);
    g_free(area);
    g_free(projarea);
    g_free(theta);
    g_free(phi);
}*/

/*******************************************************************************
 * Update
 ******************************************************************************/

static gboolean
emit_row_changed(GtkTreeModel *model,
                 GtkTreePath *path,
                 GtkTreeIter *iter,
                 G_GNUC_UNUSED gpointer user_data)
{
    gtk_tree_model_row_changed(model, path, iter);
    return FALSE;
}

void
gwy_tool_roughness_update(GwyToolRoughness *tool)
{
    GwyPlainTool *plain_tool;
    gdouble line[4];
    gint xl1, yl1, xl2, yl2;
    gint n, lineres;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (!plain_tool->selection
        || !(n = gwy_selection_get_data(plain_tool->selection, NULL)))
    {
        gwy_graph_model_remove_all_curves(tool->graphmodel);
        gwy_graph_model_remove_all_curves(tool->graphmodel_profile);
        return;
    }


    g_return_if_fail(plain_tool->selection);
    g_return_if_fail(gwy_selection_get_object(plain_tool->selection, 0, line));

    xl1 = (gint)gwy_data_field_rtoj(plain_tool->data_field, line[0]);
    yl1 = (gint)gwy_data_field_rtoi(plain_tool->data_field, line[1]);
    xl2 = (gint)gwy_data_field_rtoj(plain_tool->data_field, line[2]);
    yl2 = (gint)gwy_data_field_rtoi(plain_tool->data_field, line[3]);

    lineres = ROUND(hypot(xl1 - xl2, yl1 - yl2));
    lineres = MAX(lineres, 10);

    tool->dataline = gwy_data_field_get_profile(plain_tool->data_field,
                                                tool->dataline,
                                                xl1, yl1, xl2, yl2,
                                                lineres,
                                                tool->args.thickness,
                                                tool->args.interpolation);

    tool->profiles.texture = gwy_data_line_new_alike(tool->dataline, FALSE);
    tool->profiles.roughness = gwy_data_line_new_alike(tool->dataline, FALSE);
    tool->profiles.waviness = gwy_data_line_new_alike(tool->dataline, FALSE);
    gwy_data_line_copy(tool->dataline, tool->profiles.texture);
    gwy_data_line_copy(tool->dataline, tool->profiles.roughness);
    gwy_data_line_copy(tool->dataline, tool->profiles.waviness);

    gwy_tool_roughness_set_data_from_profile(tool->profiles,
                                             tool->dataline,
                                             tool->args.cutoff,
                                             tool->args.interpolation);

    gwy_tool_roughness_update_graphs(tool);
    gwy_tool_roughness_update_parameters(tool);
    gtk_tree_model_foreach(GTK_TREE_MODEL(tool->store), emit_row_changed, NULL);
}

static void
gwy_tool_roughness_update_parameters(GwyToolRoughness *tool)
{
    tool->params[PARAM_RA] = gwy_tool_roughness_Xa(tool->profiles.roughness);
    tool->params[PARAM_RQ] = gwy_tool_roughness_Xq(tool->profiles.roughness);
    tool->params[PARAM_RV] = gwy_tool_roughness_Xvm(tool->profiles.roughness,
                                                    1, 1);
    tool->params[PARAM_RP] = gwy_tool_roughness_Xpm(tool->profiles.roughness,
                                                    1, 1);
    tool->params[PARAM_RT] = tool->params[PARAM_RP] + tool->params[PARAM_RV];
    tool->params[PARAM_RVM] = gwy_tool_roughness_Xvm(tool->profiles.roughness,
                                                     5, 1);
    tool->params[PARAM_RPM] = gwy_tool_roughness_Xpm(tool->profiles.roughness,
                                                     5, 1);
    tool->params[PARAM_RTM] = tool->params[PARAM_RPM] + tool->params[PARAM_RVM];
    tool->params[PARAM_R3Z] = gwy_tool_roughness_Xtm(tool->profiles.roughness,
                                                     1, 3);
    tool->params[PARAM_R3Z_ISO] = gwy_tool_roughness_Xtm(tool->profiles.roughness,
                                                        5, 3);
    tool->params[PARAM_RZ] = gwy_tool_roughness_Xz(tool->profiles.roughness);
    tool->params[PARAM_RZ_ISO] = tool->params[PARAM_RTM];
    tool->params[PARAM_RSK] = gwy_tool_roughness_Xsk(tool->profiles.roughness);
    tool->params[PARAM_RKU] = gwy_tool_roughness_Xku(tool->profiles.roughness);
    tool->params[PARAM_WA] = gwy_tool_roughness_Xa(tool->profiles.waviness);
    tool->params[PARAM_WQ] = gwy_tool_roughness_Xq(tool->profiles.waviness);
    tool->params[PARAM_WY] = gwy_tool_roughness_Xtm(tool->profiles.waviness,
                                                    1, 1);
    tool->params[PARAM_PT] = gwy_tool_roughness_Xtm(tool->profiles.texture,
                                                    1, 1);

    tool->params[PARAM_DA] = gwy_tool_roughness_Da(tool->profiles.roughness);
    tool->params[PARAM_DQ] = gwy_tool_roughness_Dq(tool->profiles.roughness);
    tool->params[PARAM_L0] = gwy_tool_roughness_l0(tool->profiles.roughness);
    tool->params[PARAM_L] = tool->profiles.roughness->real;
    tool->params[PARAM_LR] = gwy_tool_roughness_lr(tool->profiles.texture);

    tool->profiles.adf = gwy_data_line_new_resampled(tool->profiles.roughness,
                                                     101,
                                                     GWY_INTERPOLATION_LINEAR);
    gwy_tool_roughness_graph_adf(tool->profiles);

    tool->profiles.brc = gwy_data_line_new_resampled(tool->profiles.roughness,
                                                     101,
                                                     GWY_INTERPOLATION_LINEAR);
    gwy_tool_roughness_graph_brc(tool->profiles);

    tool->profiles.pc = gwy_data_line_new(101, tool->params[PARAM_RT], TRUE);
    gwy_data_line_set_si_unit_x(tool->profiles.pc,
                                gwy_data_line_get_si_unit_y(tool->profiles.roughness));
    gwy_tool_roughness_graph_pc(tool->profiles);
}

static void
gwy_tool_roughness_update_graphs(GwyToolRoughness *tool)
{
    GwyGraphCurveModel *gcmodel;
    GwyRGBA preset_color;

    /* graph */
    gwy_graph_model_remove_all_curves(tool->graphmodel);
    gwy_graph_model_remove_all_curves(tool->graphmodel_profile);

    /* profile */
    g_object_set(tool->graphmodel_profile,
                 "title", _("Surface Profiles"),
                 NULL);
    gwy_graph_model_set_units_from_data_line(tool->graphmodel_profile,
                                             tool->dataline);
    preset_color = *gwy_graph_get_preset_color(0);

    gcmodel = gwy_graph_curve_model_new();
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "color", &preset_color,
                 "description", _("Texture"),
                 NULL);

    gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                 tool->profiles.texture,
                                                 0, 0);
    gwy_graph_model_add_curve(tool->graphmodel_profile, gcmodel);
    g_object_unref(gcmodel);

    preset_color = *gwy_graph_get_preset_color(1);
    gcmodel = gwy_graph_curve_model_new();
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "color", &preset_color,
                 "description", _("Roughness"),
                 NULL);

    gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                 tool->profiles.roughness,
                                                 0, 0);
    gwy_graph_model_add_curve(tool->graphmodel_profile, gcmodel);
    g_object_unref(gcmodel);


    preset_color = *gwy_graph_get_preset_color(2);
    gcmodel = gwy_graph_curve_model_new();
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "color", &preset_color,
                 "description", _("Waviness"),
                 NULL);
    gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                 tool->profiles.waviness,
                                                 0, 0);
    gwy_graph_model_add_curve(tool->graphmodel_profile, gcmodel);
    g_object_unref(gcmodel);


    /* Function */
    preset_color = *gwy_graph_get_preset_color(0);

    gcmodel = gwy_graph_curve_model_new();
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "color", &preset_color,
                 NULL);

    if (tool->graph_type == GWY_ROUGHNESS_GRAPH_TEXTURE) {
        g_object_set(tool->graphmodel,
                     "title", _("Texture"),
                     "si-unit-x", gwy_data_line_get_si_unit_x(tool->profiles.texture),
                     "si-unit-y", gwy_data_line_get_si_unit_y(tool->profiles.texture),
                     NULL);
        g_object_set(gcmodel, "description", _("Texture"), NULL);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                     tool->profiles.texture,
                                                     0, 0);
    }

    if (tool->graph_type == GWY_ROUGHNESS_GRAPH_WAVINESS) {
        g_object_set(tool->graphmodel,
                     "title", _("Waviness"),
                     "si-unit-x", gwy_data_line_get_si_unit_x(tool->profiles.waviness),
                     "si-unit-y", gwy_data_line_get_si_unit_y(tool->profiles.waviness),
                     NULL);
        g_object_set(gcmodel, "description", _("Waviness"), NULL);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                     tool->profiles.waviness,
                                                     0, 0);
    }

    if (tool->graph_type == GWY_ROUGHNESS_GRAPH_ROUGHNESS)
    {
        g_object_set(tool->graphmodel,
                     "title", _("Roughness"),
                     "si-unit-x", gwy_data_line_get_si_unit_x(tool->profiles.roughness),
                     "si-unit-y", gwy_data_line_get_si_unit_y(tool->profiles.roughness),
                     NULL);
        g_object_set(gcmodel, "description", _("Roughness"), NULL);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                     tool->profiles.roughness,
                                                     0, 0);
    }

    if (tool->graph_type == GWY_ROUGHNESS_GRAPH_ADF)
    {
        g_object_set(tool->graphmodel,
                     "title", _("The Amplitude Distribution Function"),
                     "si-unit-x", gwy_data_line_get_si_unit_x(tool->profiles.adf),
                     "si-unit-y", gwy_data_line_get_si_unit_y(tool->profiles.adf),
                     NULL);
        g_object_set(gcmodel,
                     "description", _("The Amplitude Distribution Function"),
                     NULL);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                     tool->profiles.adf,
                                                     0, 0);
    }

    if (tool->graph_type == GWY_ROUGHNESS_GRAPH_BRC)
    {
        g_object_set(tool->graphmodel,
                     "title", _("The Bearing Ratio Curve"),
                     "si-unit-x", gwy_data_line_get_si_unit_x(tool->profiles.brc),
                     "si-unit-y", gwy_data_line_get_si_unit_y(tool->profiles.brc),
                     NULL);
        g_object_set(gcmodel,
                     "description", _("The Bearing Ratio Curve"),
                     NULL);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                     tool->profiles.brc,
                                                     0, 0);
    }

    if (tool->graph_type == GWY_ROUGHNESS_GRAPH_PC)
    {
        g_object_set(tool->graphmodel,
                     "title", _("Peak Count"),
                     "si-unit-x", gwy_data_line_get_si_unit_x(tool->profiles.pc),
                     "si-unit-y", gwy_data_line_get_si_unit_y(tool->profiles.pc),
                     NULL);
        g_object_set(gcmodel,
                     "description", _("Peak Count"),
                     NULL);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                     tool->profiles.pc,
                                                     0, 0);
    }

    gwy_graph_model_add_curve(tool->graphmodel, gcmodel);
    g_object_unref(gcmodel);
}

static void
gwy_data_line_data_discrete(gdouble *x, gdouble *y, gint a_res,
                            GwyDataLine *data_line,
                            GwyInterpolationType interpolation)
{
    gdouble val, ratio, real;
    gint res;
    gdouble xl1, xl2, yl1, yl2;
    gint i, j;

    a_res--;
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(a_res>0);

    res = data_line->res-1;
    real = data_line->real;

    ratio = real/res;

    data_line->data[0]=y[0];
    data_line->data[res]=y[a_res];
    for (i = 1; i < res; i++)
    {
        val = i*ratio;
        j = 0;
        do {
            j++;
        } while (x[j] < val && j <= a_res);

        xl1 = x[j-1];
        xl2 = x[j];
        yl1 = y[j-1];
        yl2 = y[j];

        //data_line->data[i] = gwy_interpolation_get_dval(val, xl1, yl1, xl2, yl2,
        //                                                interpolation);
        data_line->data[i]=yl1+(val-xl1)*(yl2-yl1)/(xl2-xl1);
    }

    return;
}


static void
gwy_math_quicksort(gdouble *array, gint *index, gint low, gint high)
{
    gdouble pivot, temp;
    gint l, r, pos;

    pivot = array[low];
    l = low + 1;
    r = high;

    while(l < r)
    {
        if (array[l] <= pivot) l++;
        else
        {
            r--;

            temp = array[l];
            array[l] = array[r];
            array[r] = temp;

            pos = index[l];
            index[l] = index[r];
            index[r] = pos;
        }
    }

    l--;

    temp = array[low];
    array[low] = array[l];
    array[l] = temp;

    pos = index[low];
    index[low] = index[l];
    index[l] = pos;


    if (l-low > 1) gwy_math_quicksort(array, index, low, l);
    if (high-r > 1) gwy_math_quicksort(array, index, r, high);
}

static void
gwy_data_line_line_rotate2(GwyDataLine *a,
                           gdouble angle,
                           GwyInterpolationType interpolation)
{
    gint i, res;
    gdouble ratio, as, radius;
    gdouble *dx, *dy, *dy_sort;
    gint *index;
    gdouble min = 0;

    g_return_if_fail(GWY_IS_DATA_LINE(a));

    if (angle == 0)
        return;

    res = gwy_data_line_get_res(a)-1;
    dx = g_new(gdouble, a->res);
    dy_sort = g_new(gdouble, a->res);
    index = g_new(gint, a->res);
    dy = g_new(gdouble, a->res);

    ratio = a->real/res;

    /*as = asin(1); // 90°
      radius = a->data[0]*a->data[0];
      skip = radius*cos(as + angle);
      dy[0] = radius*sin(as + angle);
      dx[0] = 0;*/

    for (i = 0; i <= res; i++) {
        as = atan2(a->data[i], i*ratio);
        radius = hypot(i*ratio, a->data[i]);
        dx[i] = radius*cos(as + angle);
        dy[i] = radius*sin(as + angle);
        if (dx[i] < min)
            min = dx[i];
    }

    for (i = 0; i <= res; i++) {
        dx[i] = dx[i] - min;
        index[i] = i;
    }

    gwy_math_quicksort(dx, index, 0, a->res-1);

    for (i = 0; i <= res; i++)
    {
        as = dy[i];
        dy_sort[i] = dy[index[i]];
    }

    gwy_data_line_set_real(a, dx[res]);
    gwy_data_line_data_discrete(dx, dy_sort, res+1, a, interpolation);

    g_free(dx);
    g_free(dy);
    g_free(dy_sort);
    g_free(index);
}

static void
gwy_tool_roughness_set_data_from_profile(GwyRoughnessProfiles profiles,
                                         GwyDataLine *a,
                                         gint cutoff,
                                         gint interpolation)
{
    gint i, newres, cut;
    gdouble av, bv;
    GwyDataLine *b, *rin, *iin, *rout, *iout;
    GwyDataLine *w_rin, *w_iin, *r_rin, *r_iin;
    gdouble *re_out, *im_out;

    b = gwy_data_line_new_alike(a, FALSE);
    gwy_data_line_copy(a,b);

    newres = (gint)pow(2, ROUND(log(b->res)/G_LN2));
    gwy_data_line_resample(profiles.texture, newres, interpolation);
    gwy_data_line_resample(profiles.roughness, newres, interpolation);
    gwy_data_line_resample(profiles.waviness, newres, interpolation);
    gwy_data_line_resample(b, newres, interpolation);

    rin = gwy_data_line_new(newres, b->real, TRUE);
    iin = gwy_data_line_new(newres, b->real, TRUE);
    re_out = g_new(gdouble, newres);
    im_out = g_new(gdouble, newres);
    rout = gwy_data_line_new(newres, b->real, TRUE);
    iout = gwy_data_line_new(newres, b->real, TRUE);
    w_rin = gwy_data_line_new(newres, b->real, TRUE);
    w_iin = gwy_data_line_new(newres, b->real, TRUE);
    r_rin = gwy_data_line_new(newres, b->real, TRUE);
    r_iin = gwy_data_line_new(newres, b->real, TRUE);


    cut = (guint)(newres*cutoff/100.0);

    if (cut>=4)
    {
        gwy_data_line_fft(b,iin,rout,iout,
                          GWY_WINDOWING_NONE,
                          GWY_TRANSFORM_DIRECTION_FORWARD,
                          interpolation,FALSE,0);

        gwy_data_line_copy(rout,w_rin);
        gwy_data_line_copy(iout,w_iin);

        gwy_data_line_copy(rout,r_rin);
        gwy_data_line_copy(iout,r_iin);

        /*waviness*/
        for (i = cut; i < b->res; i++)
        {
            gwy_data_line_set_val(w_rin,i,0.0);
            gwy_data_line_set_val(w_iin,i,0.0);
        }

        gwy_data_line_clear(rout);
        gwy_data_line_clear(iout);

        gwy_data_line_fft(w_rin,w_iin,rout,iout,
                          GWY_WINDOWING_NONE,
                          GWY_TRANSFORM_DIRECTION_BACKWARD,
                          interpolation,FALSE,0);

        for (i = 0; i < newres; i++)
        {
            //profiles.waviness->data[i] = hypot(rout->data[i], iout->data[i]);
            profiles.waviness->data[i] = rout->data[i];
        }


        /*rougness*/
        for (i = 0; i < cut; i++)
        {
            gwy_data_line_set_val(r_rin,i,0.0);
            gwy_data_line_set_val(r_iin,i,0.0);
        }

        gwy_data_line_clear(rout);
        gwy_data_line_clear(iout);
        gwy_data_line_fft(r_rin,r_iin,rout,iout,
                          GWY_WINDOWING_NONE,
                          GWY_TRANSFORM_DIRECTION_BACKWARD,
                          interpolation,FALSE,0);

        for (i = 0; i < newres; i++)
        {
            //profiles.roughness->data[i] = hypot(rout->data[i], iout->data[i]);
            profiles.roughness->data[i] = rout->data[i];
        }
    }
    else
    {
        for (i = 0; i < newres; i++)
        {
            profiles.roughness->data[i] = b->data[i];
            profiles.waviness->data[i] = 0;
        }
    }

    gwy_data_line_get_line_coeffs(profiles.texture, &av, &bv);
    bv = bv/(gwy_data_line_get_real(profiles.texture)/
             (gwy_data_line_get_res(profiles.texture)-1));
    gwy_data_line_add(profiles.texture,(-1.0)*av);
    gwy_data_line_line_rotate2(profiles.texture, (-1)*bv, GWY_INTERPOLATION_LINEAR);

    gwy_data_line_get_line_coeffs(profiles.roughness, &av, &bv);
    bv = bv/(gwy_data_line_get_real(profiles.roughness)/
             (gwy_data_line_get_res(profiles.roughness)-1));
    gwy_data_line_add(profiles.roughness,(-1.0)*av);
    gwy_data_line_line_rotate2(profiles.roughness, (-1)*bv, GWY_INTERPOLATION_LINEAR);

    gwy_data_line_get_line_coeffs(profiles.waviness, &av, &bv);
    bv = bv/(gwy_data_line_get_real(profiles.waviness)/
             (gwy_data_line_get_res(profiles.waviness)-1));
    gwy_data_line_add(profiles.waviness,(-1.0)*av);
    gwy_data_line_line_rotate2(profiles.waviness, (-1)*bv, GWY_INTERPOLATION_LINEAR);

    g_object_unref(b);
    g_object_unref(rin);
    g_object_unref(rout);
    g_object_unref(iin);
    g_object_unref(iout);
    g_object_unref(r_iin);
    g_object_unref(r_rin);
    g_object_unref(w_iin);
    g_object_unref(w_rin);
}

/**
 * TODO: symmmetry
 */
static gint
gwy_tool_roughness_peaks(GwyDataLine *data_line, gdouble *peaks,
                         gint from, gint to, gdouble threshold, gint k,
                         gboolean symmetrical)
{
    gint i, res, c=-1;
    gdouble val, val_prev;
    gdouble *p;
    gboolean under=FALSE;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0);

    res = data_line->res-1;

    if (from<1) from = 1;
    if (to>(res+1)) to = res+1;

    val_prev=data_line->data[from-1];
    if (val_prev > threshold) c++;

    for (i = from; i<to; i++)
    {
        val = data_line->data[i];
        if ((val > threshold) && (val_prev < threshold)) c++;
        if (symmetrical == TRUE)
        {
            if ((val < (-1.0)*threshold) && (val_prev > (-1.0)*threshold))
            {
                under = TRUE;
            }
        }
        val_prev = val;
    }

    p = g_new(gdouble, c+1);

    c = -1;
    val_prev=data_line->data[from-1];
    if (val_prev > threshold) {c++; p[c] = val_prev;}
    for (i = from; i<to; i++)
    {
        val = data_line->data[i];
        if (val > threshold)
        {
            if (val_prev < threshold)
            {
                c++;
                p[c] = val;
            }
            else
            {
                if ((c>=0) && val > p[c]) p[c] = val;
            }
        }

        val_prev = val;
    }

    gwy_math_sort(c+1, p);

    if (k < 0) k = c;

    for (i=0; i<k; i++)
    {
        if (i<=c) {peaks[i] = p[c-i];} else {peaks[i] = 0;};
    }

    g_free(p);

    return c+1;
}

/**
 * Roughness Average - Ra, Pa, Wa
 * OK
 */
static gdouble
gwy_tool_roughness_Xa(GwyDataLine *data_line)
{
    gdouble ratio, Xa = 0.0;
    gint i, res;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Xa);

    res = data_line->res-1;
    ratio = data_line->real/res;

    for (i = 0; i <= res; i++)
    {
        Xa += fabs(data_line->data[i]);
    }

    return Xa/(res+1);
}

/**
 * Root Mean Square (RMS) Roughness - Rq, Pq, Wq
 * OK
 */
static gdouble
gwy_tool_roughness_Xq(GwyDataLine *data_line)
{
    gdouble ratio, Xq = 0.0;
    gint i, res;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Xq);

    res = data_line->res-1;
    ratio = data_line->real/res;

    for (i = 0; i <= res; i++)
    {
        Xq += pow(data_line->data[i],2);
    }

    return sqrt(Xq/(res+1));
}

/**
 * Average Maximum Profile Peak Height - Rpm
 * Maximum Profile Peak Height - Rp, Pp, Wp = Rpm for m=1
 * OK
 */
static gdouble
gwy_tool_roughness_Xpm(GwyDataLine *data_line, gint m, gint k)
{
    gdouble Xpm = 0.0;
    GwyDataLine *dl;
    gint i, samp;
    gdouble *peaks;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Xpm);
    g_return_val_if_fail(m >= 1, Xpm);
    g_return_val_if_fail(k >= 1, Xpm);

    dl = gwy_data_line_new_alike(data_line, FALSE);
    gwy_data_line_copy(data_line, dl);

    if (m > 1)
    {
        samp = floor(dl->res/m);
        gwy_data_line_resample(dl, m*samp, GWY_INTERPOLATION_LINEAR);
    }
    else
    {
        samp = dl->res;
    }

    for (i = 1; i <= m; i++)
    {
        peaks = g_new0(gdouble, k);
        gwy_tool_roughness_peaks(dl, peaks, (i-1)*samp+1, i*samp, 0, k, FALSE);
        Xpm += peaks[k-1];
        g_free(peaks);
    }

    g_object_unref(dl);

    return Xpm/m;
}

/**
 * OK
 */
static gdouble
gwy_tool_roughness_Xvm(GwyDataLine *data_line, gint m, gint k)
{
    gdouble Xvm = 0.0;
    GwyDataLine *dl;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Xvm);

    dl = gwy_data_line_new_alike(data_line, FALSE);
    gwy_data_line_copy(data_line, dl);
    gwy_data_line_multiply(dl, -1.0);

    Xvm = gwy_tool_roughness_Xpm(dl, m, k);

    g_object_unref(dl);
    return Xvm;
}

/**
 * OK
 */
static gdouble
gwy_tool_roughness_Xtm(GwyDataLine *data_line, gint m, gint k)
{
    return gwy_tool_roughness_Xpm(data_line, m, k)
        + gwy_tool_roughness_Xvm(data_line, m, k);
}

static gdouble
gwy_tool_roughness_Xz(GwyDataLine *data_line)
{
    gdouble Xz = 0.0;
    GwyDataLine *dl;
    gint i, samp;
    gdouble *peaks;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Xz);

    dl = gwy_data_line_new_alike(data_line, FALSE);
    gwy_data_line_copy(data_line, dl);

    samp = dl->res;

    peaks = g_new0(gdouble, 5);
    gwy_tool_roughness_peaks(data_line, peaks, 1, samp, 0, 5, FALSE);
    for (i = 0; i < 5; i++)
    {
        Xz += peaks[i];
    }
    g_free(peaks);

    gwy_data_line_multiply(dl, -1.0);
    peaks = g_new0(gdouble, 5);
    gwy_tool_roughness_peaks(data_line, peaks, 1, samp, 0, 5, FALSE);
    for (i = 0; i < 5; i++)
    {
        Xz += peaks[i];
    }
    g_free(peaks);

    g_object_unref(dl);

    return Xz/5;
}

static gdouble
gwy_tool_roughness_Xsk (GwyDataLine *data_line)
{
    gdouble Xsk = 0.0;
    gint i, res;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Xsk);

    res = data_line->res-1;

    for (i = 0; i <= res; i++)
    {
        Xsk += pow(data_line->data[i],3);
    }

    return Xsk/((res+1)*pow(gwy_tool_roughness_Xq(data_line),3));
}

static gdouble
gwy_tool_roughness_Xku (GwyDataLine *data_line)
{
    gdouble Xku = 0.0;
    gint i, res;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Xku);

    res = data_line->res-1;

    for (i = 0; i <= res; i++)
    {
        Xku += pow(data_line->data[i],4);
    }

    return Xku/((res+1)*pow(gwy_tool_roughness_Xq(data_line),4));
}

/*******************************************************************************
 * Spatial
 ******************************************************************************/
static gdouble
gwy_tool_roughness_Pc (GwyDataLine *data_line, gdouble threshold)
{
    gint Pc = 0;
    gint res;
    gdouble *peaks;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Pc);

    res = data_line->res;
    peaks = g_new0(gdouble, 1);
    Pc = gwy_tool_roughness_peaks(data_line, peaks, 1, res, threshold, 1,  FALSE);
    g_free(peaks);

    return Pc;
}

/*
static gdouble
gwy_tool_roughness_HSC (GwyDataLine *data_line)
{
    gdouble HSC=0;

}
*/

/*******************************************************************************
 * Hybrid
 ******************************************************************************/

static gdouble
gwy_tool_roughness_Da(GwyDataLine *data_line)
{
    gdouble Da = 0;
    gint i, res;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Da);

    res = data_line->res-1;

    for (i = 0; i < res; i++)
    {
        Da += fabs(data_line->data[i+1]-data_line->data[i]);
    }

    return Da/(res+1);
}


static gdouble
gwy_tool_roughness_Dq(GwyDataLine *data_line)
{
    gdouble Dq = 0.0;
    gint i, res;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Dq);

    res = data_line->res-1;

    for (i = 0; i < res; i++)
    {
        Dq += pow((data_line->data[i+1]-data_line->data[i]),2);
    }

    return sqrt(Dq/(res+1));
}

static gdouble
gwy_tool_roughness_l0(GwyDataLine *data_line)
{
    gdouble ratio, l0 = 0.0;
    gint i, res;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), l0);

    res = data_line->res-1;
    ratio = data_line->real/res;

    for (i = 0; i < res; i++)
    {
        l0+=hypot((i+1)*ratio-i*ratio,data_line->data[i+1]-data_line->data[i]);
    }

    return l0;
}

static gdouble
gwy_tool_roughness_lr(GwyDataLine *data_line)
{
    return gwy_tool_roughness_l0(data_line)/data_line->real;
}


/*******************************************************************************
 * Graphs
 ******************************************************************************/
/*void
  gwy_tool_roughness_distribution(GwyDataLine *data_line, GwyDataLine *distr,
  gboolean cum)
  {
  gint dz_count;
  gdouble zmin, zmax, dz, dd;
  gdouble val, val_prev, max=1, threshold;
  gint i, j, k, res;
  FILE *fp;

  zmin = (-1.0)*gwy_tool_roughness_Xpm(data_line, 1, 1);
  zmax = gwy_tool_roughness_Xvm(data_line, 1, 1);
  dz_count = distr->res;
  dz = (zmax-zmin)/dz_count;

  res = data_line->res-1;

//gwy_data_line_resample(distr, dz_count, GWY_INTERPOLATION_LINEAR);
gwy_data_line_clear(distr);

fp = fopen("distr.txt", "w");
fprintf(fp, "%15e\n", dz);

val_prev = data_line->data[0]-zmax;
for (i = 1; i < res; i++)
{
val = data_line->data[i]-zmax;
j = floor((-1.0)*val/dz);
threshold = (-1.0)*(j+1)*dz;

if (val >= threshold)
{
if (val_prev > threshold)
{
dd = 1.0;
}
else
{
dd = gwy_interpolation_get_dval(threshold, val_prev, 0, val, 1,
GWY_INTERPOLATION_LINEAR);
dd = 1.0 - dd;
}
}

fprintf(fp, "%3d %15e %15e %15e %15e\n", j, val_prev, val, threshold, dd);
if (cum == TRUE)
{
distr->data[j] = distr->data[j] + (dz_count - (j + 1));
}
else
{
for (k = j; k<dz_count; k++)
{
if (val > distr->data[k])
{
distr->data[k] = distr->data[k] + dd;
}
}
}

val_prev = val;
}

fclose(fp);

max = gwy_tool_roughness_Xpm(distr, 1, 1);
if (max != 0.0) gwy_data_line_multiply(distr, 100/max);
gwy_data_line_set_real(distr, zmax-zmin);
//gwy_data_line_set_si_unit_x(distr, gwy_data_line_get_si_unit_y(data_line));
//gwy_data_line_set_si_unit_y(distr, gwy_si_unit_new(NULL));

return;
}*/

static void
gwy_tool_roughness_distribution(GwyDataLine *data_line, GwyDataLine *distr)
{
    gint dz_count;
    gdouble zmin, zmax, dz, dd;
    gdouble val, val_prev, max=1, top, bottom;
    gint i, j, res;

    zmin = (-1.0)*gwy_tool_roughness_Xpm(data_line, 1, 1);
    zmax = gwy_tool_roughness_Xvm(data_line, 1, 1);
    dz_count = distr->res;
    dz = (zmax-zmin)/dz_count;

    res = data_line->res-1;

    gwy_data_line_clear(distr);

    for (i = 1; i <= dz_count; i++)
    {
        top = i*dz+zmin;
        bottom = (i-1)*dz+zmin;

        val_prev = data_line->data[0];
        for (j = 1; j <= res; j++)
        {
            val = data_line->data[j];

            if ((val >= bottom) && (val < top))
            {
                /*if ((val_prev >= bottom) && (val_prev < top))
                  {
                  dd = 1.0;
                  }
                  else if (val_prev >= top)
                  {
                  dd = gwy_interpolation_get_dval(top, val_prev, 0, val, 1,
                  GWY_INTERPOLATION_LINEAR);
                  dd = 1.0 - dd;
                  }
                  else if (val_prev < bottom)
                  {
                  dd = gwy_interpolation_get_dval(bottom, val_prev, 0, val, 1,
                  GWY_INTERPOLATION_LINEAR);
                  dd = 1.0 - dd;
                  }*/
                dd = 1.0;

                distr->data[dz_count - i] = distr->data[dz_count - i] + dd;
            }

            val_prev = val;
        }
    }

    max = gwy_tool_roughness_Xpm(distr, 1, 1);
    if (max != 0.0) gwy_data_line_multiply(distr, 100/max);
    gwy_data_line_set_real(distr, zmax-zmin);
    //gwy_data_line_set_si_unit_x(distr, gwy_data_line_get_si_unit_y(data_line));
    //gwy_data_line_set_si_unit_y(distr, gwy_si_unit_new(NULL));

    return;
}

static void
gwy_tool_roughness_graph_adf(GwyRoughnessProfiles profiles)
{
    gwy_tool_roughness_distribution(profiles.roughness, profiles.adf);
    return;
}

static void
gwy_tool_roughness_graph_brc(GwyRoughnessProfiles profiles)
{
    gwy_tool_roughness_distribution(profiles.roughness, profiles.brc);
    gwy_data_line_cumulate(profiles.brc);
    //gwy_data_line_line_rotate2(profiles.brc, asin(1), GWY_INTERPOLATION_LINEAR);
    return;
}

static void
gwy_tool_roughness_graph_pc(GwyRoughnessProfiles profiles)
{
    gint samples;
    gdouble ymax, dy, threshold;
    gint i;

    ymax = gwy_tool_roughness_Xpm(profiles.roughness, 1, 1);
    samples = profiles.pc->res;
    dy = ymax/samples;

    for (i = 0; i < samples; i++)
    {
        threshold = dy*i;
        profiles.pc->data[i] = gwy_tool_roughness_Pc(profiles.roughness, threshold);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
