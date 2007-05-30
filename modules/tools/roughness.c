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

#include <string.h>
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

enum {
    RESPONSE_SAVE = 1024
};

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
   ROUGHNESS_SET_FUNCTIONAL,
   ROUGHNESS_NSETS
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
    GwyDataLine *texture;
    GwyDataLine *roughness;
    GwyDataLine *waviness;

    GwyDataLine *adf;
    GwyDataLine *brc;
    GwyDataLine *pc;
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
    PARAM_H,
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
    gdouble cutoff;
    GwyInterpolationType interpolation;
    guint expanded;
} ToolArgs;

typedef struct _GwyToolRoughness      GwyToolRoughness;
typedef struct _GwyToolRoughnessClass GwyToolRoughnessClass;

struct _GwyToolRoughness {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkTreeStore *store;
    gdouble *params;

    gboolean same_units;
    GwySIUnit *slope_unit;

    /* data */
    gboolean have_data;
    GwyDataLine *dataline;
    GwyRoughnessProfiles profiles;
    GwyRoughnessGraph graph_type;

    /* graph */
    GwyGraphModel *graphmodel;
    GtkWidget *graph;

    GwyGraphModel *graphmodel_profile;
    GtkWidget *graph_profile;

    GtkWidget *graph_out;

    GtkObject *thickness;
    GtkObject *cutoff;
    GtkWidget *interpolation;

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
static void     gwy_tool_roughness_update_units      (GwyToolRoughness *tool);
static void     gwy_tool_roughness_update_parameters (GwyToolRoughness *tool);
static void     gwy_tool_roughness_update_graphs     (GwyToolRoughness *tool);

static void     gwy_tool_roughness_selection_changed (GwyPlainTool *plain_tool,
                                                      gint hint);
static void     gwy_tool_roughness_interpolation_changed
                                                     (GtkComboBox *combo,
                                                      GwyToolRoughness *tool);
static void     gwy_tool_roughness_thickness_changed (GtkAdjustment *adj,
                                                      GwyToolRoughness *tool);
static void     gwy_tool_roughness_cutoff_changed    (GtkAdjustment *adj,
                                                      GwyToolRoughness *tool);
static void     gwy_tool_roughness_graph_changed     (GtkWidget *combo,
                                                      GwyToolRoughness *tool);
static void     gwy_tool_roughness_apply             (GwyToolRoughness *tool);

static void     gwy_data_line_data_discrete          (gdouble *x,
                                                      gdouble *y,
                                                      gint res,
                                                      GwyDataLine *dline,
                                                      GwyInterpolationType interpolation);
static void     gwy_data_line_rotate2                (GwyDataLine *dline,
                                                      gdouble angle,
                                                      GwyInterpolationType interpolation);
static void     gwy_data_line_balance                (GwyDataLine *dline);
static void     gwy_math_quicksort                   (gdouble *array, gint *ind, gint low, gint high);
static gint     gwy_data_line_extend                 (GwyDataLine *dline,
                                                      GwyDataLine *extline);
static void     gwy_tool_roughness_set_data_from_fft (GwyDataLine *dline,
                                                      GwyDataLine *rin, GwyDataLine *iin);
static void     gwy_tool_roughness_set_data_from_profile (GwyRoughnessProfiles *profiles,
                                                      GwyDataLine *dline,
                                                      gdouble cutoff,
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
static gdouble  gwy_tool_roughness_HSC               (GwyDataLine *data_line);
static gdouble  gwy_tool_roughness_Da                (GwyDataLine *data_line);
static gdouble  gwy_tool_roughness_Dq                (GwyDataLine *data_line);
static gdouble  gwy_tool_roughness_l0                (GwyDataLine *data_line);
static gdouble  gwy_tool_roughness_lr                (GwyDataLine *data_line);

static void     gwy_tool_roughness_distribution   (GwyDataLine *data_line, GwyDataLine *distr);
static void     gwy_tool_roughness_graph_adf         (GwyRoughnessProfiles *profiles);
static void     gwy_tool_roughness_graph_brc         (GwyRoughnessProfiles *profiles);
static void     gwy_tool_roughness_graph_pc          (GwyRoughnessProfiles *profiles);

static const GwyRoughnessParameterInfo parameters[] = {
    {
        -1,
        ROUGHNESS_SET_AMPLITUDE,
        NULL,
        N_("Amplitude"),
        0,
        FALSE,
    },
    {
        PARAM_RA,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>a</sub>",
        N_("Roughness average"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_RQ,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>q</sub>",
        N_("Root mean square roughness"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_RT,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>t</sub>",
        N_("Maximum height of the roughness"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_RV,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>v</sub>",
        N_("Maximum roughness valley depth"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_RP,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>p</sub>",
        N_("Maximum roughness peak height"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_RTM,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>tm</sub>",
        N_("Average maximum height of the roughness"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_RVM,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>vm</sub>",
        N_("Average maximum roughness valley depth"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_RPM,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>pm</sub>",
        N_("Average maximum roughness peak height"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_R3Z,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>3z</sub>",
        N_("Average third highest peak to third lowest valley height"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_R3Z_ISO,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>3z ISO</sub>",
        N_("Average third highest peak to third lowest valley height"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_RZ,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>z</sub>",
        N_("Average maximum height of the profile"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_RZ_ISO,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>z ISO</sub>",
        N_("Average maximum height of the roughness"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_RSK,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>sk</sub>",
        N_("Skewness"),
        UNITS_NONE,
        FALSE,
    },
    {
        PARAM_RKU,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>R</i><sub>ku</sub>",
        N_("Kurtosis"),
        UNITS_NONE,
        FALSE,
    },
    {
        PARAM_WA,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>W</i><sub>a</sub>",
        N_("Waviness average"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_WQ,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>W</i><sub>q</sub>",
        N_("Root mean square waviness"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_WY,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>W</i><sub>y</sub>=<i>W</i><sub>max</sub>",
        N_("Waviness maximum height"),
        UNITS_VALUE,
        FALSE,
    },
    {
        PARAM_PT,
        ROUGHNESS_SET_AMPLITUDE,
        "<i>P</i><sub>t</sub>",
        N_("Maximum height of the profile"),
        UNITS_VALUE,
        FALSE,
    },
    {
        -1,
        ROUGHNESS_SET_SPATIAL,
        NULL,
        N_("Spatial"),
        0,
        FALSE,
    },
    /*
    {
        PARAM_S,
        ROUGHNESS_SET_SPATIAL,
        "<i>S</i>",
        N_("Mean spacing of local peaks of the profile"),
        UNITS_COORDS,
        FALSE,
    },
    {
        PARAM_SM,
        ROUGHNESS_SET_SPATIAL,
        "<i>S</i><sub>m</sub>",
        N_("Mean spacing of profile irregularities"),
        UNITS_COORDS,
        FALSE,
    },
    {
        PARAM_D,
        ROUGHNESS_SET_SPATIAL,
        "<i>D</i>",
        N_("Profile peak density"),
        UNITS_COORDS,
        FALSE,
    },
    {
        PARAM_PC,
        ROUGHNESS_SET_SPATIAL,
        "<i>P</i><sub>c</sub>",
        N_("Peak count (peak density)"),
        UNITS_NONE,
        FALSE,
    },
    {
        PARAM_HSC,
        ROUGHNESS_SET_SPATIAL,
        "HSC",
        N_("Hight spot count"),
        UNITS_NONE,
        FALSE,
    },
    */
    {
        PARAM_LA,
        ROUGHNESS_SET_SPATIAL,
        "λ<sub>a</sub>",
        N_("Average wavelength of the profile"),
        UNITS_COORDS,
        FALSE,
    },
    {
        PARAM_LQ,
        ROUGHNESS_SET_SPATIAL,
        "λ<sub>q</sub>",
        N_("Root mean square (RMS) wavelength of the profile"),
        UNITS_COORDS,
        FALSE,
    },
    {
        -1,
        ROUGHNESS_SET_HYBRID,
        NULL,
        N_("Hybrid"),
        0,
        FALSE,
    },
    {
        PARAM_DA,
        ROUGHNESS_SET_HYBRID,
        "Δ<sub>a</sub>",
        N_("Average absolute slope"),
        UNITS_SLOPE,
        FALSE,
    },
    {
        PARAM_DQ,
        ROUGHNESS_SET_HYBRID,
        "Δ<sub>q</sub>",
        N_("Root mean square (RMS) slope"),
        UNITS_SLOPE,
        FALSE,
    },
    {
        PARAM_L,
        ROUGHNESS_SET_HYBRID,
        "<i>L</i>",
        N_("Length"),
        UNITS_COORDS,
        TRUE,
    },
    {
        PARAM_L0,
        ROUGHNESS_SET_HYBRID,
        "L<sub>0</sub>",
        N_("Developed profile length"),
        UNITS_COORDS,
        TRUE,
    },
    {
        PARAM_LR,
        ROUGHNESS_SET_HYBRID,
        "<i>l</i><sub>r</sub>",
        N_("Profile length ratio"),
        UNITS_NONE,
        TRUE,
    },
    /*
    {
        -1,
        ROUGHNESS_SET_FUNCTIONAL,
        NULL,
        N_("Functional"),
        0,
        FALSE,
    },
    {
        PARAM_H,
        ROUGHNESS_SET_FUNCTIONAL,
        "<i>H</i>",
        N_("Swedish height"),
        UNITS_COORDS,
        FALSE,
    },
    {
        PARAM_HTP,
        ROUGHNESS_SET_FUNCTIONAL,
        "<i>H</i><sub>tp</sub>",
        N_("Profile section height difference"),
        UNITS_COORDS,
        FALSE,
    },
    {
        PARAM_RK,
        ROUGHNESS_SET_FUNCTIONAL,
        "<i>R</i><sub>k</sub>",
        N_("Core roughness depth"),
        UNITS_COORDS,
        FALSE,
    },
    {
        PARAM_RKP,
        ROUGHNESS_SET_FUNCTIONAL,
        "<i>R</i><sub>pk</sub>",
        N_("Reduced peak height"),
        UNITS_COORDS,
        FALSE,
    },
    {
        PARAM_RVK,
        ROUGHNESS_SET_FUNCTIONAL,
        "<i>R</i><sub>vk</sub>",
        N_("Reduced valley depth"),
        UNITS_COORDS,
        FALSE,
    },
    {
        PARAM_MR1,
        ROUGHNESS_SET_FUNCTIONAL,
        "<i>M</i><sub>r1</sub>",
        N_("Material portion "),
        UNITS_COORDS,
        FALSE,
    },
    {
        PARAM_MR2,
        ROUGHNESS_SET_FUNCTIONAL,
        "<i>M</i><sub>r2</sub>",
        N_("Material portion "),
        UNITS_COORDS,
        FALSE,
    },
    */
};

static const ToolArgs default_args = {
    1,
    0.9,
    GWY_INTERPOLATION_LINEAR,
    0,
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
static const gchar expanded_key[]      = "/module/roughness/expanded";

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
    gwy_container_set_int32_by_name(settings, thickness_key,
                                    tool->args.thickness);
    gwy_container_set_double_by_name(settings, cutoff_key,
                                     tool->args.cutoff);
    gwy_container_set_enum_by_name(settings, interpolation_key,
                                   tool->args.interpolation);
    gwy_container_set_int32_by_name(settings, expanded_key,
                                    tool->args.expanded);

    g_free(tool->params);
    gwy_object_unref(tool->store);
    gwy_object_unref(tool->dataline);
    gwy_object_unref(tool->slope_unit);
    gwy_si_unit_value_format_free(tool->none_format);
    /* TODO: Free various graph models, here or elsewhere */

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
    gwy_container_gis_double_by_name(settings, cutoff_key,
                                     &tool->args.cutoff);
    gwy_container_gis_enum_by_name(settings, interpolation_key,
                                   &tool->args.interpolation);
    gwy_container_gis_int32_by_name(settings, expanded_key,
                                    &tool->args.expanded);

    tool->slope_unit = gwy_si_unit_new(NULL);
    tool->none_format = g_new0(GwySIValueFormat, 1);
    tool->none_format->magnitude = 1.0;
    tool->none_format->precision = 3;
    gwy_si_unit_value_format_set_units(tool->none_format, "");

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
    guint i, j;

    tool->store = gtk_tree_store_new(1, G_TYPE_POINTER);
    tool->params = g_new0(gdouble, ROUGHNESS_NPARAMS);

    for (i = j = 0; i < G_N_ELEMENTS(parameters); i++) {
        pinfo = parameters + i;
        if (pinfo->param == -1) {
            if (!i)
                gtk_tree_store_insert_after(tool->store, &siter, NULL, NULL);
            else
                gtk_tree_store_insert_after(tool->store, &siter, NULL, &siter);
            gtk_tree_store_set(tool->store, &siter, 0, pinfo, -1);
            j = 0;
        }
        else {
            if (!j)
                gtk_tree_store_insert_after(tool->store, &iter, &siter, NULL);
            else
                gtk_tree_store_insert_after(tool->store, &iter, &siter, &iter);
            gtk_tree_store_set(tool->store, &iter, 0, pinfo, -1);
            j++;
        }
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

    GtkDialog *dialog;
    GtkSizeGroup *sizegroup;
    GtkWidget *dialog_vbox, *hbox, *vbox_left, *vbox_right, *table;
    GtkWidget *scwin, *treeview;
    GwyAxis *axis;
    gint row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    dialog_vbox = GTK_DIALOG(dialog)->vbox;

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox, TRUE, TRUE, 0);

    vbox_left = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox_left, TRUE, TRUE, 0);

    vbox_right = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox_right, TRUE, TRUE, 0);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox_left), scwin, TRUE, TRUE, 0);

    treeview = gwy_tool_roughness_param_view_new(tool);
    gtk_container_add(GTK_CONTAINER(scwin), treeview);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox_left), table, FALSE, FALSE, 0);
    row = 0;

    tool->graph_out
        = gwy_enum_combo_box_new(graph_types, G_N_ELEMENTS(graph_types),
                                 G_CALLBACK(gwy_tool_roughness_graph_changed),
                                 tool, tool->graph_type, TRUE);
    gwy_table_attach_hscale(table, row, _("_Graph:"), NULL,
                            GTK_OBJECT(tool->graph_out), GWY_HSCALE_WIDGET);
    row++;

    /* cut-off */
    tool->cutoff = gtk_adjustment_new(tool->args.cutoff,
                                      0.0, 0.2, 0.01, 0.05, 0);
    gwy_table_attach_hscale(table, row, _("C_ut-off:"), NULL,
                            tool->cutoff, GWY_HSCALE_DEFAULT);
    g_signal_connect(tool->cutoff, "value-changed",
                     G_CALLBACK(gwy_tool_roughness_cutoff_changed), tool);
    row++;

    tool->thickness = gtk_adjustment_new(tool->args.thickness,
                                         1, 128, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Thickness:"), "px",
                            tool->thickness, GWY_HSCALE_DEFAULT);
    g_signal_connect(tool->thickness, "value-changed",
                     G_CALLBACK(gwy_tool_roughness_thickness_changed), tool);
    row++;

    tool->interpolation = gwy_enum_combo_box_new
                           (gwy_interpolation_type_get_enum(), -1,
                            G_CALLBACK(gwy_tool_roughness_interpolation_changed),
                            tool, tool->args.interpolation, TRUE);
    gwy_table_attach_hscale(table, row, _("_Interpolation type:"), NULL,
                            GTK_OBJECT(tool->interpolation), GWY_HSCALE_WIDGET);
    row++;

    tool->graphmodel_profile = gwy_graph_model_new();
    tool->graph_profile = gwy_graph_new(tool->graphmodel_profile);
    gtk_widget_set_size_request(tool->graph_profile, 300, 250);
    gwy_graph_enable_user_input(GWY_GRAPH(tool->graph_profile), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox_right), tool->graph_profile, TRUE, TRUE, 0);

    tool->graphmodel = gwy_graph_model_new();
    tool->graph = gwy_graph_new(tool->graphmodel);
    gtk_widget_set_size_request(tool->graph, 300, 250);
    gwy_graph_enable_user_input(GWY_GRAPH(tool->graph), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox_right), tool->graph, TRUE, TRUE, 0);

    sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    axis = gwy_graph_get_axis(GWY_GRAPH(tool->graph_profile), GTK_POS_LEFT);
    gtk_size_group_add_widget(sizegroup, GTK_WIDGET(axis));
    axis = gwy_graph_get_axis(GWY_GRAPH(tool->graph), GTK_POS_LEFT);
    gtk_size_group_add_widget(sizegroup, GTK_WIDGET(axis));
    g_object_unref(sizegroup);

    gtk_dialog_add_button(dialog, GTK_STOCK_SAVE, RESPONSE_SAVE);
    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    gtk_dialog_add_button(dialog, GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);
    gtk_dialog_set_response_sensitive(dialog, RESPONSE_SAVE, FALSE);

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
    GwySIValueFormat *tvf = NULL;
    const GwySIValueFormat *vf;
    gdouble value;
    gchar buf[64];

    gtk_tree_model_get(model, iter, 0, &pinfo, -1);
    if (pinfo->param == -1 || !tool->have_data) {
        g_object_set(renderer, "text", "", NULL);
        return;
    }
    if (pinfo->same_units && !tool->same_units) {
        g_object_set(renderer, "text", _("N.A."), NULL);
        return;
    }

    value = tool->params[pinfo->param];
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
        tvf = gwy_si_unit_get_format_with_digits(tool->slope_unit,
                                                 GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                 value, 3, NULL);
        vf = tvf;
        break;

        default:
        g_return_if_reached();
        break;
    }
    g_snprintf(buf, sizeof(buf), "%.*f%s%s",
               vf->precision, value/vf->magnitude,
               *vf->units ? " " : "", vf->units);
    g_object_set(renderer, "markup", buf, NULL);

    if (tvf)
        gwy_si_unit_value_format_free(tvf);
}

static void
param_row_expanded_collapsed(GtkTreeView *treeview,
                             GtkTreeIter *iter,
                             GtkTreePath *path,
                             GwyToolRoughness *tool)
{
    const GwyRoughnessParameterInfo *pinfo;

    gtk_tree_model_get(gtk_tree_view_get_model(treeview), iter, 0, &pinfo, -1);
    if (gtk_tree_view_row_expanded(treeview, path))
        tool->args.expanded |= 1 << pinfo->set;
    else
        tool->args.expanded &= ~(1 << pinfo->set);
}

static GtkWidget*
gwy_tool_roughness_param_view_new(GwyToolRoughness *tool)
{
    GtkWidget *treeview;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeIter iter;

    model = GTK_TREE_MODEL(tool->store);
    treeview = gtk_tree_view_new_with_model(model);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

    column = gtk_tree_view_column_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_symbol, tool, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
                 "weight-set", TRUE,
                 "ellipsize-set", TRUE,
                 NULL);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_name, tool, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_value, tool, NULL);

    /* Restore set visibility state */
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            const GwyRoughnessParameterInfo *pinfo;

            gtk_tree_model_get(model, &iter, 0, &pinfo, -1);
            if (pinfo->param == -1
                && (tool->args.expanded & (1 << pinfo->set))) {
                GtkTreePath *path;

                path = gtk_tree_model_get_path(model, &iter);
                gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), path, TRUE);
                gtk_tree_path_free(path);
            }
        } while (gtk_tree_model_iter_next(model, &iter));
    }
    g_signal_connect(treeview, "row-collapsed",
                     G_CALLBACK(param_row_expanded_collapsed), tool);
    g_signal_connect(treeview, "row-expanded",
                     G_CALLBACK(param_row_expanded_collapsed), tool);

    return treeview;
}

static void
gwy_tool_roughness_response(GwyTool *tool,
                            gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_roughness_parent_class)->response(tool,
                                                              response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_roughness_apply(GWY_TOOL_ROUGHNESS(tool));
    /*
    if (response_id == RESPONSE_SAVE)
        gwy_tool_roughness_save(GWY_TOOL_ROUGHNESS(tool));
        */
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
        gwy_tool_roughness_update_units(tool);
    }

    gwy_tool_roughness_update(tool);
}

static void
gwy_tool_roughness_data_changed(GwyPlainTool *plain_tool)
{
    GwyToolRoughness *tool;

    tool = GWY_TOOL_ROUGHNESS(plain_tool);
    gwy_tool_roughness_update(tool);
}

static void
gwy_tool_roughness_selection_changed(GwyPlainTool *plain_tool,
                                     gint hint)
{
    GwyToolRoughness *tool;
    GtkDialog *dialog;
    gint n = 0;

    tool = GWY_TOOL_ROUGHNESS(plain_tool);
    g_return_if_fail(hint <= 0);

    if (plain_tool->selection) {
        n = gwy_selection_get_data(plain_tool->selection, NULL);
        g_return_if_fail(n == 0 || n == 1);
    }

    gwy_tool_roughness_update(tool);
    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);
    gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_APPLY, n > 0);
    gtk_dialog_set_response_sensitive(dialog, RESPONSE_SAVE, n > 0);
}

static void
gwy_tool_roughness_interpolation_changed(GtkComboBox *combo,
                                         GwyToolRoughness *tool)
{
    tool->args.interpolation = gwy_enum_combo_box_get_active(combo);
    gwy_tool_roughness_update(tool);
}

static void
gwy_tool_roughness_thickness_changed(GtkAdjustment *adj,
                                     GwyToolRoughness *tool)
{
    tool->args.thickness = gwy_adjustment_get_int(adj);
    gwy_tool_roughness_update(tool);
}

static void
gwy_tool_roughness_cutoff_changed(GtkAdjustment *adj,
                                  GwyToolRoughness *tool)
{
    tool->args.cutoff = gtk_adjustment_get_value(adj);
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
    g_object_set(graphmodel, "title", s, NULL);
    g_free(s);
    gwy_app_data_browser_add_graph_model(graphmodel, plain_tool->container,
                                         TRUE);
    g_object_unref(graphmodel);
}

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
        || !(n = gwy_selection_get_data(plain_tool->selection, NULL))) {
        tool->have_data = FALSE;
        gwy_tool_roughness_update_graphs(tool);
        gtk_tree_model_foreach(GTK_TREE_MODEL(tool->store),
                               emit_row_changed, NULL);
        return;
    }

    g_return_if_fail(plain_tool->selection);
    g_return_if_fail(gwy_selection_get_object(plain_tool->selection, 0, line));

    tool->have_data = TRUE;
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

    gwy_tool_roughness_set_data_from_profile(&tool->profiles,
                                             tool->dataline,
                                             tool->args.cutoff,
                                             tool->args.interpolation);

    gwy_tool_roughness_update_graphs(tool);
    gwy_tool_roughness_update_parameters(tool);
    gtk_tree_model_foreach(GTK_TREE_MODEL(tool->store), emit_row_changed, NULL);
}

static void
gwy_tool_roughness_update_units(GwyToolRoughness *tool)
{
    GwyPlainTool *plain_tool;
    GwySIUnit *siunitxy, *siunitz;

    plain_tool = GWY_PLAIN_TOOL(tool);
    siunitxy = gwy_data_field_get_si_unit_xy(plain_tool->data_field);
    siunitz = gwy_data_field_get_si_unit_z(plain_tool->data_field);
    tool->same_units = gwy_si_unit_equal(siunitxy, siunitz);
    gwy_si_unit_divide(siunitz, siunitxy, tool->slope_unit);
}

static void
gwy_tool_roughness_update_parameters(GwyToolRoughness *tool)
{
    GwyDataLine *roughness, *waviness, *texture;
    gdouble *params;

    roughness = tool->profiles.roughness;
    waviness = tool->profiles.waviness;
    texture = tool->profiles.texture;
    params = tool->params;

    params[PARAM_RA]      = gwy_tool_roughness_Xa(roughness);
    params[PARAM_RQ]      = gwy_tool_roughness_Xq(roughness);
    params[PARAM_RV]      = gwy_tool_roughness_Xvm(roughness, 1, 1);
    params[PARAM_RP]      = gwy_tool_roughness_Xpm(roughness, 1, 1);
    params[PARAM_RT]      = params[PARAM_RP] + params[PARAM_RV];
    params[PARAM_RVM]     = gwy_tool_roughness_Xvm(roughness, 5, 1);
    params[PARAM_RPM]     = gwy_tool_roughness_Xpm(roughness, 5, 1);
    params[PARAM_RTM]     = params[PARAM_RPM] + params[PARAM_RVM];
    params[PARAM_R3Z]     = gwy_tool_roughness_Xtm(roughness, 1, 3);
    params[PARAM_R3Z_ISO] = gwy_tool_roughness_Xtm(roughness, 5, 3);
    params[PARAM_RZ]      = gwy_tool_roughness_Xz(roughness);
    params[PARAM_RZ_ISO]  = params[PARAM_RTM];
    params[PARAM_RSK]     = gwy_tool_roughness_Xsk(roughness);
    params[PARAM_RKU]     = gwy_tool_roughness_Xku(roughness);
    params[PARAM_WA]      = gwy_tool_roughness_Xa(waviness);
    params[PARAM_WQ]      = gwy_tool_roughness_Xq(waviness);
    params[PARAM_WY]      = gwy_tool_roughness_Xtm(waviness, 1, 1);
    params[PARAM_PT]      = gwy_tool_roughness_Xtm(texture, 1, 1);
    params[PARAM_DA]      = gwy_tool_roughness_Da(roughness);
    params[PARAM_DQ]      = gwy_tool_roughness_Dq(roughness);
    params[PARAM_LA]      = 2*G_PI*params[PARAM_RA]/params[PARAM_DA];
    params[PARAM_LQ]      = 2*G_PI*params[PARAM_RQ]/params[PARAM_DQ];
    params[PARAM_L0]      = gwy_tool_roughness_l0(roughness);
    params[PARAM_L]       = gwy_data_line_get_real(roughness);
    params[PARAM_LR]      = gwy_tool_roughness_lr(texture);

    gwy_tool_roughness_graph_adf(&tool->profiles);
    gwy_tool_roughness_graph_brc(&tool->profiles);
    gwy_tool_roughness_graph_pc(&tool->profiles);
}

static void
gwy_tool_roughness_update_graphs(GwyToolRoughness *tool)
{
    GwyGraphCurveModel *gcmodel;
    GwyRGBA preset_color;
    gint i;

    typedef struct
    {
        const gchar *title;
        const gchar *description;
        GwyDataLine *dataline;
    } Graphs;

    Graphs graph_profile[] =  {
        { N_("Texture"),   "",  tool->profiles.texture,   },
        { N_("Waviness"),  "",  tool->profiles.waviness,  },
        { N_("Roughness"), "",  tool->profiles.roughness, },
    };

    Graphs graph_all[] =  {
        { N_("Texture"),   "",  tool->profiles.texture,   },
        { N_("Waviness"),  "",  tool->profiles.waviness,  },
        { N_("Roughness"), "",  tool->profiles.roughness, },
        { N_("The Amplitude Distribution Function"),  N_("The Amplitude Distribution Function"),  tool->profiles.adf,  },
        { N_("The Bearing Ratio Curve"),  N_("The Bearing Ratio Curve"),  tool->profiles.brc,  },
        { N_("Peak Count"),  N_("Peak Count"),  tool->profiles.pc,  },

    };

    gwy_graph_model_remove_all_curves(tool->graphmodel);
    gwy_graph_model_remove_all_curves(tool->graphmodel_profile);
    if (!tool->have_data)
        return;

    g_object_set(tool->graphmodel_profile,
                 "title", _("Surface Profiles"),
                 NULL);
    gwy_graph_model_set_units_from_data_line(tool->graphmodel_profile,
                                             tool->dataline);
    for (i = 0; i < G_N_ELEMENTS(graph_profile); i++) {
        preset_color = *gwy_graph_get_preset_color(i);
        gcmodel = gwy_graph_curve_model_new();
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", &preset_color,
                     "description", graph_profile[i].title,
                     NULL);

        gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                     graph_profile[i].dataline,
                                                     0, 0);
        gwy_graph_model_add_curve(tool->graphmodel_profile, gcmodel);
        g_object_unref(gcmodel);
    }

    preset_color = *gwy_graph_get_preset_color(0);
    gcmodel = gwy_graph_curve_model_new();
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "color", &preset_color,
                 NULL);
    g_object_set(tool->graphmodel,
                 "title", graph_all[tool->graph_type].title,
                 "si-unit-x", gwy_data_line_get_si_unit_x(graph_all[tool->graph_type].dataline),
                 "si-unit-y", gwy_data_line_get_si_unit_y(graph_all[tool->graph_type].dataline),
                 NULL);
    g_object_set(gcmodel, "description",  graph_all[tool->graph_type].description, NULL);
    gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                 graph_all[tool->graph_type].dataline,
                                                 0, 0);
    gwy_graph_model_add_curve(tool->graphmodel, gcmodel);
    g_object_unref(gcmodel);
}

static void
gwy_data_line_data_discrete(gdouble *x, gdouble *y, gint res,
                            GwyDataLine *dline,
                            GwyInterpolationType interpolation)
{
    gdouble val, ratio;
    gint i, j, n;

    g_return_if_fail(GWY_IS_DATA_LINE(dline));
    g_return_if_fail(res > 1);

    n = gwy_data_line_get_res(dline);
    ratio = gwy_data_line_get_real(dline)/(n - 1);

    gwy_data_line_set_val(dline, 0, y[0]);
    //gwy_data_line_set_val(dline, n - 1, y[res - 1]);
    j = 0;
    for (i = 1; i < n; i++) {
        val = i*ratio;
        while (x[j] < val && j < res)
          j++;
        //data_line->data[i] = gwy_interpolation_get_dval(val, xl1, yl1, xl2, yl2, interpolation);
        gwy_data_line_set_val(dline, i, y[j-1]+(val-x[j-1])*(y[j]-y[j-1])/(x[j]-x[j-1]));
    }

    return;
}


static void
gwy_math_quicksort(gdouble *array, gint *ind, gint low, gint high)
{
    gdouble pivot, temp;
    gint l, r, pos;

    pivot = array[low];
    l = low + 1;
    r = high;

    while(l < r)
    {
        if (array[l] <= pivot)
            l++;
        else
        {
            r--;

            temp = array[l];
            array[l] = array[r];
            array[r] = temp;

            pos = ind[l];
            ind[l] = ind[r];
            ind[r] = pos;
        }
    }

    l--;

    temp = array[low];
    array[low] = array[l];
    array[l] = temp;

    pos = ind[low];
    ind[low] = ind[l];
    ind[l] = pos;


    if (l-low > 1) gwy_math_quicksort(array, ind, low, l);
    if (high-r > 1) gwy_math_quicksort(array, ind, r, high);
}

static void
gwy_data_line_rotate2     (GwyDataLine *dline,
                           gdouble angle,
                           GwyInterpolationType interpolation)
{
    gint i, j, k, n;
    gdouble ratio, as, radius, val;
    gdouble *dx, *dy, *dx_sort, *dy_sort;
    gint *ind;
    gdouble min = 0;

    g_return_if_fail(GWY_IS_DATA_LINE(dline));

    if (angle == 0)
        return;

    n = gwy_data_line_get_res(dline);
    dx = g_new(gdouble, n);
    dy_sort = g_new(gdouble, n);
    dx_sort = g_new(gdouble, n);
    ind = g_new(gint, n);
    dy = g_new(gdouble, n);

    ratio = dline->real/(n-1);

    for (i = 0; i < n; i++)
    {
        as = atan2(gwy_data_line_get_val(dline, i), i*ratio);
        radius = hypot(i*ratio, gwy_data_line_get_val(dline, i));
        dx[i] = radius*cos(as + angle);
        dy[i] = radius*sin(as + angle);
        ind[i] = i;
        if (dx[i] < min)
            min = dx[i];
    }

    for (i = 0; i < n; i++)
        dx[i] = dx[i] - min;
    gwy_math_quicksort(dx, ind, 0, n-1);

    dx_sort[0] = dx[0];
    j = ind[0];
    dy_sort[0] = dy[j];
    k = 1;
    for (i = 1; i < n; i++)
        if (ind[i] > j)
        {
        	  dx_sort[k] = dx[i];
        	  j = ind[i];
        	  dy_sort[k] = dy[j];
        	  k++;
        }

    gwy_data_line_set_offset(dline, min);
    gwy_data_line_set_real(dline, dx[n-1]);
    gwy_data_line_data_discrete(dx_sort, dy_sort, n, dline, interpolation);

    g_free(dx);
    g_free(dy);
    g_free(dy_sort);
    g_free(ind);
}


static gint
gwy_data_line_extend(GwyDataLine *dline,
                     GwyDataLine *extline)
{
    enum { SMEAR = 6 };
    gint n, next, k, i;
    gdouble *data, *edata;
    gdouble der0, der1;

    n = gwy_data_line_get_res(dline);
    next = gwy_fft_find_nice_size(4*n/3);
    g_return_val_if_fail(next < 3*n, n);

    gwy_data_line_resample(extline, next, GWY_INTERPOLATION_NONE);
    gwy_data_line_set_real(extline, next*gwy_data_line_get_real(dline)/n);
    data = gwy_data_line_get_data(dline);
    edata = gwy_data_line_get_data(extline);

    memcpy(edata, data, n*sizeof(gdouble));
    /* 0 and 1 in extension data coordinates, not primary data */
    der0 = (2*data[n-1] - data[n-2] - data[n-3])/3;
    der1 = (2*data[0] - data[1] - data[2])/3;
    k = next - n;
    for (i = 0; i < k; i++) {
        gdouble x, y, ww, w;

        y = w = 0.0;
        if (i < SMEAR) {
            ww = 2.0*(SMEAR-1 - i)/SMEAR;
            y += ww*(data[n-1] + der0*(i + 1));
            w += ww;
        }
        if (k-1 - i < SMEAR) {
            ww = 2.0*(i + SMEAR-1 - (k-1))/SMEAR;
            y += ww*(data[0] + der1*(k - i));
            w += ww;
        }
        if (i < n) {
            x = 1.0 - i/(k - 1.0);
            ww = x*x;
            y += ww*data[n-1 - i];
            w += ww;
        }
        if (k-1 - i < n) {
            x = 1.0 - (k-1 - i)/(k - 1.0);
            ww = x*x;
            y += ww*data[k-1 - i];
            w += ww;
        }
        edata[n + i] = y/w;
    }

    return next;
}

static void
gwy_data_line_balance(GwyDataLine *dline)
{
	  gdouble av, bv;

	  gwy_data_line_get_line_coeffs(dline, &av, &bv);
    bv = bv/(gwy_data_line_get_real(dline)/(gwy_data_line_get_res(dline)-1));
    gwy_data_line_add(dline, -av);
    gwy_data_line_rotate2(dline, -bv, GWY_INTERPOLATION_LINEAR);
}

static void
gwy_tool_roughness_set_data_from_fft(GwyDataLine *dline,
                                     GwyDataLine *rin, GwyDataLine *iin)
{
    gint i, n;
    GwyDataLine *rout, *iout;
    const gdouble *reals;
    gdouble *data;

    n = gwy_data_line_get_res(dline);
    rout = gwy_data_line_new_alike(rin, FALSE);
    iout = gwy_data_line_new_alike(rin, FALSE);
    gwy_data_line_fft_raw(rin, iin, rout, iout, GWY_TRANSFORM_DIRECTION_BACKWARD);
    reals = gwy_data_line_get_data_const(rout);
    data = gwy_data_line_get_data(dline);
    for (i = 0; i < n; i++)
        data[i] = reals[i];

    g_object_unref(rout);
    g_object_unref(iout);
}

static void
gwy_tool_roughness_set_data_from_profile(GwyRoughnessProfiles *profiles,
                                         GwyDataLine *dline,
                                         gdouble cutoff,
                                         gint interpolation)
{
    gint i, n, cut, next;
    gdouble f;
    GwyDataLine *rin, *iin, *rout, *iout, *rrin, *riin, *wrin, *wiin;

    n = gwy_fft_find_nice_size(gwy_data_line_get_res(dline));
    gwy_data_line_resample(dline, n, GWY_INTERPOLATION_LINEAR);
    profiles->texture = gwy_data_line_duplicate(dline);
    profiles->waviness = gwy_data_line_new_alike(dline, FALSE);
    profiles->roughness = gwy_data_line_new_alike(dline, FALSE);

    rin = gwy_data_line_duplicate(dline);
    //next = gwy_data_line_extend(dline, rin);
    iin = gwy_data_line_new_alike(rin, TRUE);
    rout = gwy_data_line_new_alike(rin, TRUE);
    iout = gwy_data_line_new_alike(rin, TRUE);
    rrin = gwy_data_line_new_alike(rin, TRUE);
    riin = gwy_data_line_new_alike(rin, TRUE);
    wrin = gwy_data_line_new_alike(rin, TRUE);
    wiin = gwy_data_line_new_alike(rin, TRUE);

    gwy_data_line_fft_raw(rin, iin, rout, iout, GWY_TRANSFORM_DIRECTION_FORWARD);
    gwy_data_line_copy(rout, rrin);
    gwy_data_line_copy(iout, riin);
    gwy_data_line_copy(rout, wrin);
    gwy_data_line_copy(iout, wiin);

    cut = ceil(n*cutoff);
    gwy_data_line_part_clear(wrin, cut, n);
    gwy_data_line_part_clear(wiin, cut, n);
    gwy_data_line_part_clear(rrin, 0, cut);
    gwy_data_line_part_clear(riin, 0, cut);
/*
    for (i = 0; i < next; i++)
    {
        f = 2.0*MIN(i, next-i)/next;
        if (f > cutoff)
        {
						gwy_data_line_set_val(wrin, i, 0);
						gwy_data_line_set_val(wiin, i, 0);
        }
        else
        {
        	  gwy_data_line_set_val(rrin, i, 0);
					  gwy_data_line_set_val(riin, i, 0);
        }
    }
*/
    gwy_tool_roughness_set_data_from_fft(profiles->waviness, wrin, wiin);
    gwy_tool_roughness_set_data_from_fft(profiles->roughness, rrin, riin);

    gwy_data_line_balance(profiles->waviness);
    gwy_data_line_balance(profiles->roughness);
    gwy_data_line_balance(profiles->texture);

    g_object_unref(rin);
    g_object_unref(rout);
    g_object_unref(iin);
    g_object_unref(iout);
    g_object_unref(rrin);
    g_object_unref(riin);
    g_object_unref(wrin);
    g_object_unref(wiin);
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

    if (from < 1) from = 1;
    if (to>(res+1)) to = res+1;

    val_prev=data_line->data[from-1];
    if (val_prev > threshold) c++;

    for (i = from; i<to; i++)
    {
        val = data_line->data[i];
        if ((val > threshold) && (val_prev < threshold)) c++;
        if (symmetrical == TRUE)
            if ((val < (-1.0)*threshold) && (val_prev > (-1.0)*threshold))
                under = TRUE;
        val_prev = val;
    }

    p = g_new(gdouble, c+1);

    c = -1;
    val_prev=data_line->data[from-1];
    if (val_prev > threshold) {
        c++;
        p[c] = val_prev;
    }
    for (i = from; i<to; i++)
    {
        val = data_line->data[i];
        if (val > threshold) {
            if (val_prev < threshold) {
                c++;
                p[c] = val;
            }
            else {
                if (c >= 0 && val > p[c])
                    p[c] = val;
            }
        }

        val_prev = val;
    }

    gwy_math_sort(c+1, p);

    if (k < 0) k = c;

    for (i=0; i<k; i++)
        if (i <= c)
            peaks[i] = p[c-i];
        else
            peaks[i] = 0;

    g_free(p);

    return c+1;
}

/**
 * Roughness Average - Ra, Pa, Wa
 *
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
        Xa += fabs(data_line->data[i]);

    return Xa/(res+1);
}

/**
 * Root Mean Square (RMS) Roughness - Rq, Pq, Wq
 *
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
        Xq += pow(data_line->data[i],2);

    return sqrt(Xq/(res+1));
}

/**
 * Average Maximum Profile Peak Height - Rpm
 * Maximum Profile Peak Height - Rp, Pp, Wp = Rpm for m=1
 *
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
        samp = dl->res;

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
        Xz += peaks[i];
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
        Xku += pow(data_line->data[i],4);

    return Xku/((res+1)*pow(gwy_tool_roughness_Xq(data_line),4));
}

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

static gdouble
gwy_tool_roughness_HSC (GwyDataLine *data_line)
{
    gdouble HSC=0;

}


static gdouble
gwy_tool_roughness_Da(GwyDataLine *data_line)
{
    gdouble Da = 0;
    gint i, res;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Da);

    res = data_line->res-1;

    for (i = 0; i < res; i++)
        Da += fabs(data_line->data[i+1]-data_line->data[i]);

    return Da/gwy_data_line_get_real(data_line);
}


static gdouble
gwy_tool_roughness_Dq(GwyDataLine *data_line)
{
    gdouble Dq = 0.0;
    gint i, res;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), Dq);

    res = data_line->res-1;

    for (i = 0; i < res; i++)
        Dq += pow((data_line->data[i+1]-data_line->data[i]), 2);

    return sqrt(Dq/gwy_data_line_get_real(data_line));
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
    gwy_data_line_set_offset(distr, zmin);
    gwy_data_line_set_si_unit_x(distr, gwy_data_line_get_si_unit_y(data_line));
    gwy_data_line_set_si_unit_y(distr, gwy_si_unit_new("%"));
    return;
}

static void
gwy_tool_roughness_graph_adf(GwyRoughnessProfiles *profiles)
{
	  profiles->adf = gwy_data_line_new_resampled(profiles->roughness, 101,
                                                GWY_INTERPOLATION_LINEAR);
    gwy_tool_roughness_distribution(profiles->roughness, profiles->adf);
    return;
}

static void
gwy_tool_roughness_graph_brc(GwyRoughnessProfiles *profiles)
{
    gdouble max;

    profiles->brc = gwy_data_line_new_resampled(profiles->roughness, 101,
                                                GWY_INTERPOLATION_LINEAR);
    gwy_tool_roughness_distribution(profiles->roughness, profiles->brc);
    gwy_data_line_cumulate(profiles->brc);
    max = gwy_tool_roughness_Xpm(profiles->brc, 1, 1);
    if (max != 0.0) gwy_data_line_multiply(profiles->brc, 100/max);
    //gwy_data_line_rotate2(profiles->brc, asin(1), GWY_INTERPOLATION_LINEAR);
    return;
}

static void
gwy_tool_roughness_graph_pc(GwyRoughnessProfiles *profiles)
{
    gint samples;
    gdouble ymax, dy, threshold;
    gint i;

    ymax = gwy_tool_roughness_Xpm(profiles->roughness, 1, 1);
    profiles->pc = gwy_data_line_new(101, ymax, TRUE);
    samples = gwy_data_line_get_res(profiles->pc);
    dy = ymax/samples;

    gwy_data_line_set_si_unit_x(profiles->pc,
                                gwy_data_line_get_si_unit_y(profiles->roughness));


    for (i = 0; i < samples; i++)
    {
        threshold = dy*i;
        gwy_data_line_set_val(profiles->pc, i, gwy_tool_roughness_Pc(profiles->roughness, threshold));
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
