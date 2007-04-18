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
    GWY_ROUGHNESS_GRAPH_TEXTURE   = 0,
    GWY_ROUGHNESS_GRAPH_WAVINESS  = 1,
    GWY_ROUGHNESS_GRAPH_ROUGHNESS = 2,
    GWY_ROUGHNESS_GRAPH_ADF       = 3,
    GWY_ROUGHNESS_GRAPH_BRC       = 4,
    GWY_ROUGHNESS_GRAPH_PC        = 5
} GwyRoughnessGraph;

typedef enum {
   ROUGHNESS_SET_AMPLITUDE  = 0,
   ROUGHNESS_SET_SPATIAL    = 1,
   ROUGHNESS_SET_HYBRID     = 2,
   ROUGHNESS_SET_FUNCTIONAL = 3
} GwyRoughnessSet;

typedef struct {
    GwyDataLine *texture;   /**/
    GwyDataLine *roughness;   /**/
    GwyDataLine *waviness;   /**/

    GwyDataLine *adf; /* */
    GwyDataLine *brc;
    GwyDataLine *pc;  /* Peak count */
} GwyRoughnessProfiles;

typedef struct {
    /*Amplitude*/
    gdouble Ra, Rq, Rt, Rv, Rp, Rtm, Rvm, Rpm, R3z, R3zISO, Rz, RzISO, Rsk, Rku;
    gdouble Pt;
    gdouble Wa, Wq, Wy;

    /* Spacing */
    gdouble Pc, S, Sm, La, Lq, HSC, D;

    /*Hybrid*/
    gdouble Da, Dq, L0, L, Lr;

    /*BRC*/
    gdouble Htp, Rk, Rpk, Rvk, Mr1, Mr2;
} GwyRoughnessParameters;

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
    GWY_INTERPOLATION_BILINEAR,
};

typedef struct _GwyToolRoughness      GwyToolRoughness;
typedef struct _GwyToolRoughnessClass GwyToolRoughnessClass;

struct _GwyToolRoughness {
    GwyPlainTool parent_instance;

    ToolArgs args;

    /* data */
    GwyDataLine *dataline;
    GwyRoughnessProfiles profiles;
    GwyRoughnessParameters parameters;
    GwyRoughnessGraph graph_type;

    /* graph */
    GwyGraphModel *graphmodel;
    GtkWidget *graph;

    GwyGraphModel *graphmodel_profile;
    GtkWidget *graph_profile;


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

    /* Parameters */
    /*Amplitude*/
    GtkWidget *Ra, *Rq, *Rt, *Rv, *Rp, *Rtm, *Rvm, *Rpm, *R3z, *R3zISO,
              *Rz, *RzISO, *Rsk, *Rku;
    GtkWidget *Pt;
    GtkWidget *Wa, *Wq, *Wy;

    /* Spacing */
    GtkWidget *Pc, *S, *Sm, *La, *Lq, *HSC, *D;

    /*Hybrid*/
    GtkWidget *Da, *Dq, *L0, *L, *Lr;

    /*BRC*/
    GtkWidget *Htp, *Rk, *Rpk, *Rvk, *Mr1, *Mr2;

    /* potential class data */
    GwySIValueFormat *pixel_format;
    GType layer_type_line;
};

struct _GwyToolRoughnessClass {
    GwyPlainToolClass parent_class;
};

static const gchar interpolation_key[] = "/module/roughness/interpolation";
static const gchar cutoff_key[]        = "/module/roughness/cutoff";
static const gchar thickness_key[]     = "/module/roughness/thickness";
static const gchar pm_key[]            = "/module/roughness/pm";
static const gchar vm_key[]            = "/module/roughness/vm";
static const gchar tm_key[]            = "/module/roughness/tm";
static const gchar pc_threshold_key[]  = "/module/roughness/pc_threshold";


static gboolean module_register                      (void);
static GType    gwy_tool_roughness_get_type          (void) G_GNUC_CONST;
static void     gwy_tool_roughness_finalize          (GObject *object);
static void     gwy_tool_roughness_init_dialog       (GwyToolRoughness *tool);
static void     gwy_tool_roughness_data_switched     (GwyTool *gwytool,
                                                      GwyDataView *data_view);
static void     gwy_tool_roughness_response          (GwyTool *tool,
                                                      gint response_id);
static void     gwy_tool_roughness_data_changed      (GwyPlainTool *plain_tool);
static void     gwy_tool_roughness_update            (GwyToolRoughness *tool);
static void     gwy_tool_roughness_update_parameters (GwyToolRoughness *tool);
static void     gwy_tool_roughness_update_graphs     (GwyToolRoughness *tool);
static void     gwy_tool_roughness_update_label      (GwySIValueFormat *units,
                                                      GtkWidget *label,
                                                      gdouble value);
static void     gwy_tool_roughness_update_labels     (GwyToolRoughness *tool);

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

void     gwy_tool_roughness_distribution   (GwyDataLine *data_line, GwyDataLine *distr);
static void     gwy_tool_roughness_graph_adf         (GwyRoughnessProfiles profiles);
static void     gwy_tool_roughness_graph_brc         (GwyRoughnessProfiles profiles);
static void     gwy_tool_roughness_graph_pc          (GwyRoughnessProfiles profiles);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculate surface profile parameters."),
    "Martin Hasoň <hasonm@physics.muni.cz>",
    "1.0",
    "Martin Hasoň",
    "2006",
};

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

    gwy_object_unref(tool->dataline);

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

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;
    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_int32_by_name(settings, thickness_key,
                                    &tool->args.thickness);
    gwy_container_gis_int32_by_name(settings, cutoff_key,
                                    &tool->args.cutoff);
    gwy_container_gis_enum_by_name(settings, interpolation_key,
                                   &tool->args.interpolation);

    tool->pixel_format = g_new0(GwySIValueFormat, 1);
    tool->pixel_format->magnitude = 1.0;
    tool->pixel_format->precision = 0;
    gwy_si_unit_value_format_set_units(tool->pixel_format, "px");

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_line,
                                     "line");

    gwy_tool_roughness_init_dialog(tool);
}

static void
gwy_tool_roughness_init_dialog(GwyToolRoughness *tool)
{
    static const GwyEnum set[] = {
        { N_("Amplitude"),  ROUGHNESS_SET_AMPLITUDE,  },
        { N_("Spatial"),    ROUGHNESS_SET_SPATIAL,    },
        { N_("Hybrid"),     ROUGHNESS_SET_HYBRID,     },
        { N_("Functional"), ROUGHNESS_SET_FUNCTIONAL, },
    };

    static struct
    {
        const gint set;
        const gchar *parameter;
        const gchar *name;
        gsize offset;
    }
    const parameters[] = {
        { 0, N_("R<sub>a</sub>"), N_("Roughness Average"),
          G_STRUCT_OFFSET(GwyToolRoughness, Ra) },
        { 0, N_("R<sub>q</sub>"), N_("Root Mean Square Roughness"),
          G_STRUCT_OFFSET(GwyToolRoughness, Rq) },
        { 0, N_("R<sub>t</sub>"), N_("Maximum Height of the Roughness"),
          G_STRUCT_OFFSET(GwyToolRoughness, Rt) },
        { 0, N_("R<sub>v</sub>"), N_("Maximum Roughness Valley Depth"),
          G_STRUCT_OFFSET(GwyToolRoughness, Rv) },
        { 0, N_("R<sub>p</sub>"), N_("Maximum Roughness Peak Height"),
          G_STRUCT_OFFSET(GwyToolRoughness, Rp) },
        { 0, N_("R<sub>tm</sub>"), N_("Average Maximum Height of the Roughness"),
          G_STRUCT_OFFSET(GwyToolRoughness, Rtm) },
        { 0, N_("R<sub>vm</sub>"), N_("Average Maximum Roughness Valley Depth"),
          G_STRUCT_OFFSET(GwyToolRoughness, Rvm) },
        { 0, N_("R<sub>pm</sub>"), N_("Average Maximum Roughness Peak Height"),
          G_STRUCT_OFFSET(GwyToolRoughness, Rpm) },
        { 0, N_("R<sub>3z</sub>"), N_("Average Third Highest Peak to Third Lowest Valley Height"),
          G_STRUCT_OFFSET(GwyToolRoughness, R3z) },
        { 0, N_("R<sub>3z ISO</sub>"), N_("Average Third Highest Peak to Third Lowest Valley Height"),
          G_STRUCT_OFFSET(GwyToolRoughness, R3zISO) },
        { 0, N_("R<sub>z</sub>"), N_("Average Maximum Height of the Profile"),
          G_STRUCT_OFFSET(GwyToolRoughness, Rz) },
        { 0, N_("R<sub>z ISO</sub>"), N_("Average Maximum Height of the Roughness"),
          G_STRUCT_OFFSET(GwyToolRoughness, RzISO) },
        { 0, N_("R<sub>sk</sub>"), N_("Skewness"),
          G_STRUCT_OFFSET(GwyToolRoughness, Rsk) },
        { 0, N_("R<sub>ku</sub>"), N_("Kurtosis"),
          G_STRUCT_OFFSET(GwyToolRoughness, Rku) },
        { 0, N_("W<sub>a</sub>"), N_("Waviness Average"),
          G_STRUCT_OFFSET(GwyToolRoughness, Wa) },
        { 0, N_("W<sub>q</sub>"), N_("Root Mean Square Waviness"),
          G_STRUCT_OFFSET(GwyToolRoughness, Wq) },
        { 0, N_("W<sub>y</sub>=W<sub>max</sub>"), N_("Waviness Maximum Height"),
          G_STRUCT_OFFSET(GwyToolRoughness, Wy) },
        { 0, N_("P<sub>t</sub>"), N_("Maximum Height of the Profile"),
          G_STRUCT_OFFSET(GwyToolRoughness, Pt) },

        { 1, N_("S"), N_("Mean Spacing of Local Peaks of the Profile"),
          G_STRUCT_OFFSET(GwyToolRoughness, S) },
        { 1, N_("S<sub>m</sub>"), N_("Mean Spacing of Profile Irregularities"),
          G_STRUCT_OFFSET(GwyToolRoughness, Sm) },
        { 1, N_("D"), N_("Profile Peak Density"),
          G_STRUCT_OFFSET(GwyToolRoughness, D) },
        { 1, N_("P<sub>c</sub>"), N_("Peak Count (Peak Density)"),
          G_STRUCT_OFFSET(GwyToolRoughness, Pc) },
        { 1, N_("HSC"), N_("Hight Spot Count"),
          G_STRUCT_OFFSET(GwyToolRoughness, HSC) },
        { 1, N_("lambda<sub>a</sub>"), N_("Average Wavelength of the Profile"),
          G_STRUCT_OFFSET(GwyToolRoughness, La) },
        { 1, N_("lambda<sub>q</sub>"),
          N_("Root Mean Square (RMS) Wavelength of the Profile"),
          G_STRUCT_OFFSET(GwyToolRoughness, Lq) },

        { 2, N_("Delta<sub>a</sub>"), N_("Average Absolute Slope"),
          G_STRUCT_OFFSET(GwyToolRoughness, Da) },
        { 2, N_("Delta<sub>q</sub>"), N_("Root Mean Square (RMS) Slope"),
          G_STRUCT_OFFSET(GwyToolRoughness, Dq) },
        { 2, N_("L<sub>0</sub>"), N_("Developed Profile Length"),
          G_STRUCT_OFFSET(GwyToolRoughness, L0) },
        { 2, N_("l<sub>r</sub>"), N_("Profile Length Ratio"),
          G_STRUCT_OFFSET(GwyToolRoughness, Lr) },
        { 2, N_("Length"), N_("L"),
          G_STRUCT_OFFSET(GwyToolRoughness, L) },

        { 3, N_("Delta<sub>a</sub>"), N_("Average Absolute Slope"),
          G_STRUCT_OFFSET(GwyToolRoughness, Da) },
        { 3, N_("Delta<sub>q</sub>"), N_("Root Mean Square (RMS) Slope"),
          G_STRUCT_OFFSET(GwyToolRoughness, Dq) },
        { 3, N_("L<sub>0</sub>"), N_("Developed Profile Length"),
          G_STRUCT_OFFSET(GwyToolRoughness, L0) },
        { 3, N_("l<sub>r</sub>"), N_("Profile Length Ratio"),
          G_STRUCT_OFFSET(GwyToolRoughness, Lr) },
        { 3, N_("Length"), N_("L"),
          G_STRUCT_OFFSET(GwyToolRoughness, L) },

    };

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
    GtkWidget *dialog_vbox, *notebook, *hbox, *vbox_left, *vbox_right, *table;
    GtkWidget *scrolled, *viewport;
    GtkWidget *label, **plabel;
    gint i, j, rows;

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

    notebook = gtk_notebook_new();
    for (i = 0; i < G_N_ELEMENTS(set); i++) {
        rows = 1;
        scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                       GTK_POLICY_NEVER,
                                       GTK_POLICY_ALWAYS);

        viewport = gtk_viewport_new (NULL, NULL);

        table = gtk_table_new(rows, 3, FALSE);
        gtk_table_set_col_spacings(GTK_TABLE(table), 6);
        gtk_container_set_border_width(GTK_CONTAINER(table), 4);

        for (j = 0; j < G_N_ELEMENTS(parameters); j++)
        {
           if (_(parameters[j].set)!=i) continue;
           rows++;
           gtk_table_resize(GTK_TABLE(table), rows, 4);

           label = gtk_label_new(NULL);
           gtk_label_set_markup(GTK_LABEL(label), _(parameters[j].parameter));
           gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
           gtk_table_attach(GTK_TABLE(table), label, 0, 1, j+1, j+2,
                           GTK_EXPAND | GTK_FILL, 0, 2, 2);

           label = gtk_label_new(_(parameters[j].name));
           gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
           gtk_table_attach(GTK_TABLE(table), label, 1, 2, j+1, j+2,
                           GTK_EXPAND | GTK_FILL, 0, 2, 2);

           plabel = (GtkWidget**)G_STRUCT_MEMBER_P(tool, parameters[j].offset);
           *plabel = gtk_label_new(NULL);
           gtk_misc_set_alignment(GTK_MISC(*plabel), 1.0, 0.5);
           gtk_label_set_single_line_mode(GTK_LABEL(*plabel), TRUE);
           gtk_label_set_selectable(GTK_LABEL(*plabel), TRUE);
           gtk_table_attach(GTK_TABLE(table), *plabel, 2, 3, j+1, j+2,
                            GTK_EXPAND | GTK_FILL, 0, 2, 2);
        }

        label = gtk_label_new(_(set[i].name));
        gtk_container_add(GTK_CONTAINER(viewport), table);
        gtk_container_add(GTK_CONTAINER(scrolled), viewport);
        gtk_notebook_insert_page(GTK_NOTEBOOK(notebook), scrolled, label, -1);
    }

    gtk_box_pack_start(GTK_BOX(vbox_left), notebook, TRUE, TRUE, 0);

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
    GwyRoughnessParameters parameters;
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
    gwy_tool_roughness_update_labels(tool);
}


static void
gwy_tool_roughness_update_parameters(GwyToolRoughness *tool)
{
    tool->parameters.Ra = gwy_tool_roughness_Xa(tool->profiles.roughness);
    tool->parameters.Rq = gwy_tool_roughness_Xq(tool->profiles.roughness);
    tool->parameters.Rv = gwy_tool_roughness_Xvm(tool->profiles.roughness, 1, 1);
    tool->parameters.Rp = gwy_tool_roughness_Xpm(tool->profiles.roughness, 1, 1);
    tool->parameters.Rt = tool->parameters.Rp + tool->parameters.Rv;
    tool->parameters.Rvm = gwy_tool_roughness_Xvm(tool->profiles.roughness, 5, 1);
    tool->parameters.Rpm = gwy_tool_roughness_Xpm(tool->profiles.roughness, 5, 1);
    tool->parameters.Rtm = tool->parameters.Rpm + tool->parameters.Rvm;
    tool->parameters.R3z = gwy_tool_roughness_Xtm(tool->profiles.roughness, 1, 3);
    tool->parameters.R3zISO = gwy_tool_roughness_Xtm(tool->profiles.roughness, 5, 3);
    tool->parameters.Rz = gwy_tool_roughness_Xz(tool->profiles.roughness);
    tool->parameters.RzISO = tool->parameters.Rtm;
    tool->parameters.Rsk = gwy_tool_roughness_Xsk(tool->profiles.roughness);
    tool->parameters.Rku = gwy_tool_roughness_Xku(tool->profiles.roughness);
    tool->parameters.Wa = gwy_tool_roughness_Xa(tool->profiles.waviness);
    tool->parameters.Wq = gwy_tool_roughness_Xq(tool->profiles.waviness);
    tool->parameters.Wy = gwy_tool_roughness_Xtm(tool->profiles.waviness, 1, 1);
    tool->parameters.Pt = gwy_tool_roughness_Xtm(tool->profiles.texture, 1, 1);

    tool->parameters.Da = gwy_tool_roughness_Da(tool->profiles.roughness);
    tool->parameters.Dq = gwy_tool_roughness_Dq(tool->profiles.roughness);
    tool->parameters.L0 = gwy_tool_roughness_l0(tool->profiles.roughness);
    tool->parameters.L = tool->profiles.roughness->real;
    tool->parameters.Lr = gwy_tool_roughness_lr(tool->profiles.texture);

    tool->profiles.adf = gwy_data_line_new_resampled(tool->profiles.roughness,
                                                     101,
                                                     GWY_INTERPOLATION_BILINEAR);
    gwy_tool_roughness_graph_adf(tool->profiles);

    tool->profiles.brc = gwy_data_line_new_resampled(tool->profiles.roughness,
                                                     101,
                                                     GWY_INTERPOLATION_BILINEAR);
    gwy_tool_roughness_graph_brc(tool->profiles);

    tool->profiles.pc = gwy_data_line_new(101, tool->parameters.Rt, TRUE);
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
                 "si-unit-x", gwy_data_line_get_si_unit_x(tool->dataline),
                 "si-unit-y", gwy_data_line_get_si_unit_y(tool->dataline),
                 NULL);
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

    if (tool->graph_type == GWY_ROUGHNESS_GRAPH_TEXTURE)
    {
        g_object_set(tool->graphmodel,
                     "title", _("Texture"),
                     "si-unit-x", gwy_data_line_get_si_unit_x(tool->profiles.texture),
                     "si-unit-y", gwy_data_line_get_si_unit_y(tool->profiles.texture),
                     NULL);
        g_object_set(gcmodel,
                     "description", _("Texture"),
                     NULL);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                     tool->profiles.texture,
                                                     0, 0);
    }

    if (tool->graph_type == GWY_ROUGHNESS_GRAPH_WAVINESS)
    {
        g_object_set(tool->graphmodel,
                     "title", _("Waviness"),
                     "si-unit-x", gwy_data_line_get_si_unit_x(tool->profiles.waviness),
                     "si-unit-y", gwy_data_line_get_si_unit_y(tool->profiles.waviness),
                     NULL);
        g_object_set(gcmodel,
                     "description", _("Waviness"),
                     NULL);
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
        g_object_set(gcmodel,
                     "description", _("Roughness"),
                     NULL);
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
gwy_tool_roughness_update_label(GwySIValueFormat *units,
                                GtkWidget *label,
                                gdouble value)
{
    static gchar buffer[64];

    g_return_if_fail(units);
    g_return_if_fail(GTK_IS_LABEL(label));

    g_snprintf(buffer, sizeof(buffer), "%.*f%s%s",
               units->precision, value/units->magnitude,
               *units->units ? " " : "", units->units);
    gtk_label_set_markup(GTK_LABEL(label), buffer);
}


static void
gwy_tool_roughness_update_labels(GwyToolRoughness *tool)
{
    GwyPlainTool *plain_tool;
    gint n;

    plain_tool = GWY_PLAIN_TOOL(tool);

    if (!plain_tool->selection
        || !(n = gwy_selection_get_data(plain_tool->selection, NULL))) {
        gtk_label_set_text(GTK_LABEL(tool->Ra), "");
        gtk_label_set_text(GTK_LABEL(tool->Rq), "");
        gtk_label_set_text(GTK_LABEL(tool->Rt), "");
        gtk_label_set_text(GTK_LABEL(tool->Rv), "");
        gtk_label_set_text(GTK_LABEL(tool->Rp), "");
        gtk_label_set_text(GTK_LABEL(tool->Rtm), "");
        gtk_label_set_text(GTK_LABEL(tool->Rvm), "");
        gtk_label_set_text(GTK_LABEL(tool->Rpm), "");
        gtk_label_set_text(GTK_LABEL(tool->Rz), "");
        gtk_label_set_text(GTK_LABEL(tool->RzISO), "");
        gtk_label_set_text(GTK_LABEL(tool->R3z), "");
        gtk_label_set_text(GTK_LABEL(tool->R3zISO), "");
        gtk_label_set_text(GTK_LABEL(tool->Rsk), "");
        gtk_label_set_text(GTK_LABEL(tool->Rku), "");
        gtk_label_set_text(GTK_LABEL(tool->Pt), "");
        gtk_label_set_text(GTK_LABEL(tool->Wa), "");
        gtk_label_set_text(GTK_LABEL(tool->Wq), "");
        gtk_label_set_text(GTK_LABEL(tool->Wy), "");

        gtk_label_set_text(GTK_LABEL(tool->Da), "");
        gtk_label_set_text(GTK_LABEL(tool->Dq), "");
        gtk_label_set_text(GTK_LABEL(tool->L0), "");
        gtk_label_set_text(GTK_LABEL(tool->L), "");
        gtk_label_set_text(GTK_LABEL(tool->Lr), "");

        gtk_label_set_text(GTK_LABEL(tool->S), "");
        gtk_label_set_text(GTK_LABEL(tool->Sm), "");
        gtk_label_set_text(GTK_LABEL(tool->D), "");
        gtk_label_set_text(GTK_LABEL(tool->Pc), "");
        gtk_label_set_text(GTK_LABEL(tool->HSC), "");
        gtk_label_set_text(GTK_LABEL(tool->La), "");
        gtk_label_set_text(GTK_LABEL(tool->Lq), "");
        return;
    }

    /*if (!tool->area_format)
      gwy_tool_stats_update_units(tool);
     */

    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Ra, tool->parameters.Ra);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Rq, tool->parameters.Rq);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Rt, tool->parameters.Rt);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Rv, tool->parameters.Rv);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Rp, tool->parameters.Rp);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Rtm, tool->parameters.Rtm);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Rvm, tool->parameters.Rvm);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Rpm, tool->parameters.Rpm);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->R3z, tool->parameters.R3z);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->R3zISO, tool->parameters.R3zISO);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Rz, tool->parameters.Rz);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->RzISO, tool->parameters.RzISO);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Rsk, tool->parameters.Rsk);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Rku, tool->parameters.Rku);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Pt, tool->parameters.Pt);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Wa, tool->parameters.Wa);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Wq, tool->parameters.Wq);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Wy, tool->parameters.Wy);

    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Da, tool->parameters.Da);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Dq, tool->parameters.Dq);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->L0, tool->parameters.L0);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->L, tool->parameters.L);
    gwy_tool_roughness_update_label(plain_tool->value_format, tool->Lr, tool->parameters.Lr);

    /*g_snprintf(buffer, sizeof(buffer), "%2.3g", tool->results.kurtosis);
      gtk_label_set_text(GTK_LABEL(tool->kurtosis), buffer);*/
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

//gwy_data_line_resample(distr, dz_count, GWY_INTERPOLATION_BILINEAR);
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

void
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
    //gwy_data_line_line_rotate2(profiles.brc, asin(1), GWY_INTERPOLATION_BILINEAR);
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
