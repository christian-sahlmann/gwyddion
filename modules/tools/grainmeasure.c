/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <stdarg.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/grains.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <app/gwyapp.h>

enum {
    /* Integers are 32bit */
    MAX_STATS = 33,
    /* Must not be a bit position */
    GRAIN_QUANTITY_ID = MAX_STATS-1
};

#define GWY_TYPE_TOOL_GRAIN_MEASURE            (gwy_tool_grain_measure_get_type())
#define GWY_TOOL_GRAIN_MEASURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_GRAIN_MEASURE, GwyToolGrainMeasure))
#define GWY_IS_TOOL_GRAIN_MEASURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_GRAIN_MEASURE))
#define GWY_TOOL_GRAIN_MEASURE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_GRAIN_MEASURE, GwyToolGrainMeasureClass))

typedef struct _GwyToolGrainMeasure      GwyToolGrainMeasure;
typedef struct _GwyToolGrainMeasureClass GwyToolGrainMeasureClass;

typedef enum {
    UNITS_COORDS,
    UNITS_VALUE,
    UNITS_ANGLE,
    UNITS_AREA,
    UNITS_VOLUME
} UnitsType;

typedef enum {
   GRAIN_QUANTITY_SET_ID,
   GRAIN_QUANTITY_SET_POSITION,
   GRAIN_QUANTITY_SET_VALUE,
   GRAIN_QUANTITY_SET_AREA,
   GRAIN_QUANTITY_SET_BOUNDARY,
   GRAIN_QUANTITY_SET_VOLUME,
   GRAIN_QUANTITY_SET_SLOPE,
   GRAIN_QUANTITY_NSETS
} GrainQuantitySet;

typedef struct {
    GwyGrainQuantity quantity;
    GrainQuantitySet set;
    const gchar *name;
    UnitsType units;
    gboolean same_units;
} QuantityInfo;

typedef struct {
    guint expanded;
} ToolArgs;

struct _GwyToolGrainMeasure {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkTreeStore *store;
    gint ngrains;
    gint *grains;
    gint gno;
    GArray *values[MAX_STATS];

    gboolean same_units;
    GwySIUnit *area_unit;
    GwySIUnit *volume_unit;

    GtkWidget *gno_label;
    GtkWidget *value_labels[MAX_STATS];

    /* potential class data */
    GwySIValueFormat *angle_format;
    GType layer_type_point;
    gint map[MAX_STATS];   /* GwyGrainQuantity -> index in quantities + 1 */
};

struct _GwyToolGrainMeasureClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_grain_measure_get_type        (void) G_GNUC_CONST;
static void gwy_tool_grain_measure_finalize         (GObject *object);
static void gwy_tool_grain_measure_init_quantities  (GwyToolGrainMeasure *tool);
static void gwy_tool_grain_measure_init_dialog      (GwyToolGrainMeasure *tool);
static GtkWidget* gwy_tool_grain_measure_param_view_new(GwyToolGrainMeasure *tool);
static void gwy_tool_grain_measure_data_switched    (GwyTool *gwytool,
                                                     GwyDataView *data_view);
static void gwy_tool_grain_measure_data_changed     (GwyPlainTool *plain_tool);
static void gwy_tool_grain_measure_mask_changed     (GwyPlainTool *plain_tool);
static void gwy_tool_grain_measure_selection_changed(GwyPlainTool *plain_tool,
                                                     gint hint);
static void gwy_tool_grain_measure_invalidate       (GwyToolGrainMeasure *tool);
static void gwy_tool_grain_measure_update_units     (GwyToolGrainMeasure *tool);
static void gwy_tool_grain_measure_recalculate      (GwyToolGrainMeasure *tool);

static const QuantityInfo quantities[] = {
    {
        -1,
        GRAIN_QUANTITY_SET_ID,
        N_("Id"),
        0,
        FALSE,
    },
    {
        GRAIN_QUANTITY_ID,
        GRAIN_QUANTITY_SET_ID,
        N_("Grain number"),
        0,
        FALSE,
    },
    {
        -1,
        GRAIN_QUANTITY_SET_POSITION,
        N_("Position"),
        0,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_CENTER_X,
        GRAIN_QUANTITY_SET_POSITION,
        N_("Center x:"),
        UNITS_COORDS,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_CENTER_Y,
        GRAIN_QUANTITY_SET_POSITION,
        N_("Center y:"),
        UNITS_COORDS,
        FALSE,
    },
    {
        -1,
        GRAIN_QUANTITY_SET_VALUE,
        N_("Value"),
        0,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_MINIMUM,
        GRAIN_QUANTITY_SET_VALUE,
        N_("Minimum:"),
        UNITS_VALUE,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_MAXIMUM,
        GRAIN_QUANTITY_SET_VALUE,
        N_("Maximum:"),
        UNITS_VALUE,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_MEAN,
        GRAIN_QUANTITY_SET_VALUE,
        N_("Mean:"),
        UNITS_VALUE,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_MEDIAN,
        GRAIN_QUANTITY_SET_VALUE,
        N_("Median:"),
        UNITS_VALUE,
        FALSE,
    },
    {
        -1,
        GRAIN_QUANTITY_SET_AREA,
        N_("Area"),
        0,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_PROJECTED_AREA,
        GRAIN_QUANTITY_SET_AREA,
        N_("Projected area:"),
        UNITS_AREA,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_SURFACE_AREA,
        GRAIN_QUANTITY_SET_AREA,
        N_("Surface area:"),
        UNITS_AREA,
        TRUE,
    },
    {
        GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE,
        GRAIN_QUANTITY_SET_AREA,
        N_("Equivalent square side:"),
        UNITS_COORDS,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS,
        GRAIN_QUANTITY_SET_AREA,
        N_("Equivalent disc radius:"),
        UNITS_COORDS,
        FALSE,
    },
    {
        -1,
        GRAIN_QUANTITY_SET_VOLUME,
        N_("Volume"),
        0,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_VOLUME_0,
        GRAIN_QUANTITY_SET_VOLUME,
        N_("Zero basis:"),
        UNITS_VOLUME,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_VOLUME_MIN,
        GRAIN_QUANTITY_SET_VOLUME,
        N_("Grain minimum basis:"),
        UNITS_VOLUME,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_VOLUME_LAPLACE,
        GRAIN_QUANTITY_SET_VOLUME,
        N_("Laplacian background basis:"),
        UNITS_VOLUME,
        FALSE,
    },
    {
        -1,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Boundary"),
        0,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Projected boundary length:"),
        UNITS_COORDS,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Minimum bounding size:"),
        UNITS_COORDS,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Minimum bounding direction:"),
        UNITS_ANGLE,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Maximum bounding size:"),
        UNITS_COORDS,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Maximum bounding direction:"),
        UNITS_ANGLE,
        FALSE,
    },
    {
        -1,
        GRAIN_QUANTITY_SET_SLOPE,
        N_("Slope"),
        0,
        FALSE,
    },
    {
        GWY_GRAIN_VALUE_SLOPE_THETA,
        GRAIN_QUANTITY_SET_SLOPE,
        N_("Inclination θ:"),
        UNITS_ANGLE,
        TRUE,
    },
    {
        GWY_GRAIN_VALUE_SLOPE_PHI,
        GRAIN_QUANTITY_SET_SLOPE,
        N_("Inclination φ:"),
        UNITS_ANGLE,
        TRUE,
    },
};

static const ToolArgs default_args = {
    0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Grain measurement tool, calculates characteristics of selected "
       "countinous parts of mask."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2007",
};

static const gchar expanded_key[] = "/module/grainmeasure/expanded";

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolGrainMeasure, gwy_tool_grain_measure, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_GRAIN_MEASURE);

    return TRUE;
}

static void
gwy_tool_grain_measure_class_init(GwyToolGrainMeasureClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_grain_measure_finalize;

    tool_class->stock_id = GWY_STOCK_GRAINS_MEASURE;
    tool_class->title = _("Grain Measure");
    tool_class->tooltip = _("Measure individual grains "
                            "(continuous parts of mask)");
    tool_class->prefix = "/module/grainmeasure";
    tool_class->data_switched = gwy_tool_grain_measure_data_switched;
    tool_class->default_width = 240;
    tool_class->default_height = 400;

    ptool_class->data_changed = gwy_tool_grain_measure_data_changed;
    ptool_class->mask_changed = gwy_tool_grain_measure_mask_changed;
    ptool_class->selection_changed = gwy_tool_grain_measure_selection_changed;
}

static void
gwy_tool_grain_measure_finalize(GObject *object)
{
    GwyToolGrainMeasure *tool;
    GwyContainer *settings;
    guint i;

    tool = GWY_TOOL_GRAIN_MEASURE(object);

    settings = gwy_app_settings_get();
    gwy_container_set_int32_by_name(settings, expanded_key,
                                    tool->args.expanded);

    g_free(tool->grains);
    gwy_object_unref(tool->store);
    g_object_unref(tool->area_unit);
    g_object_unref(tool->volume_unit);
    for (i = 0; i < MAX_STATS; i++) {
        if (tool->values[i]) {
            g_array_free(tool->values[i], TRUE);
            tool->values[i] = NULL;
        }
    }
    if (tool->angle_format)
        gwy_si_unit_value_format_free(tool->angle_format);

    G_OBJECT_CLASS(gwy_tool_grain_measure_parent_class)->finalize(object);
}

static void
gwy_tool_grain_measure_init(GwyToolGrainMeasure *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;
    guint i;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_point = gwy_plain_tool_check_layer_type(plain_tool,
                                                             "GwyLayerPoint");
    if (!tool->layer_type_point)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_VFMARKUP;
    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_int32_by_name(settings, expanded_key,
                                    &tool->args.expanded);

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_point,
                                     "pointer");

    tool->area_unit = gwy_si_unit_new(NULL);
    tool->volume_unit = gwy_si_unit_new(NULL);

    tool->angle_format = g_new0(GwySIValueFormat, 1);
    tool->angle_format->magnitude = 1.0;
    tool->angle_format->precision = 1;
    gwy_si_unit_value_format_set_units(tool->angle_format, "deg");

    for (i = 0; i < G_N_ELEMENTS(quantities); i++) {
        if (quantities[i].quantity != -1)
            tool->map[quantities[i].quantity] = i+1;
    }

    gwy_tool_grain_measure_init_quantities(tool);
    gwy_tool_grain_measure_init_dialog(tool);
}

static void
gwy_tool_grain_measure_init_quantities(GwyToolGrainMeasure *tool)
{
    const QuantityInfo *qinfo;
    GtkTreeIter siter, iter;
    guint i, j;

    tool->store = gtk_tree_store_new(1, G_TYPE_POINTER);

    for (i = j = 0; i < G_N_ELEMENTS(quantities); i++) {
        qinfo = quantities + i;
        if (qinfo->quantity == -1) {
            if (!i)
                gtk_tree_store_insert_after(tool->store, &siter, NULL, NULL);
            else
                gtk_tree_store_insert_after(tool->store, &siter, NULL, &siter);
            gtk_tree_store_set(tool->store, &siter, 0, qinfo, -1);
            j = 0;
        }
        else {
            if (!j)
                gtk_tree_store_insert_after(tool->store, &iter, &siter, NULL);
            else
                gtk_tree_store_insert_after(tool->store, &iter, &siter, &iter);
            gtk_tree_store_set(tool->store, &iter, 0, qinfo, -1);
            j++;
        }
    }
}

static void
gwy_tool_grain_measure_init_dialog(GwyToolGrainMeasure *tool)
{
    GtkWidget *scwin, *treeview;
    GtkDialog *dialog;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), scwin, TRUE, TRUE, 0);

    treeview = gwy_tool_grain_measure_param_view_new(tool);
    gtk_container_add(GTK_CONTAINER(scwin), treeview);

    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

    gtk_widget_show_all(dialog->vbox);
}

static void
render_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer user_data)
{
    const QuantityInfo *qinfo;
    gboolean header;

    gtk_tree_model_get(model, iter, 0, &qinfo, -1);
    header = (qinfo->quantity == -1);
    g_object_set(renderer,
                 "ellipsize", header ? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_END ,
                 "weight", header ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
                 "text", qinfo->name,
                 NULL);
}

static void
render_value(G_GNUC_UNUSED GtkTreeViewColumn *column,
             GtkCellRenderer *renderer,
             GtkTreeModel *model,
             GtkTreeIter *iter,
             gpointer user_data)
{
    GwyToolGrainMeasure *tool = (GwyToolGrainMeasure*)user_data;
    const QuantityInfo *qinfo;
    GwySIValueFormat *tvf = NULL;
    const GwySIValueFormat *vf;
    gdouble value;
    gchar buf[64];

    gtk_tree_model_get(model, iter, 0, &qinfo, -1);
    if (qinfo->quantity == -1 || tool->gno <= 0) {
        g_object_set(renderer, "text", "", NULL);
        return;
    }
    if (qinfo->same_units && !tool->same_units) {
        g_object_set(renderer, "text", _("N.A."), NULL);
        return;
    }
    if (qinfo->quantity == GRAIN_QUANTITY_ID) {
        g_snprintf(buf, sizeof(buf), "%d", tool->gno);
        g_object_set(renderer, "text", buf, NULL);
        return;
    }

    value = g_array_index(tool->values[qinfo->quantity], gdouble, tool->gno);
    switch (qinfo->units) {
        case UNITS_COORDS:
        vf = GWY_PLAIN_TOOL(tool)->coord_format;
        break;

        case UNITS_VALUE:
        vf = GWY_PLAIN_TOOL(tool)->value_format;
        break;

        case UNITS_ANGLE:
        vf = tool->angle_format;
        break;

        case UNITS_AREA:
        tvf = gwy_si_unit_get_format_with_digits(tool->area_unit,
                                                 GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                 value, 3, NULL);
        vf = tvf;
        break;

        case UNITS_VOLUME:
        tvf = gwy_si_unit_get_format_with_digits(tool->volume_unit,
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
                             GwyToolGrainMeasure *tool)
{
    const QuantityInfo *qinfo;

    gtk_tree_model_get(gtk_tree_view_get_model(treeview), iter, 0, &qinfo, -1);
    if (gtk_tree_view_row_expanded(treeview, path))
        tool->args.expanded |= 1 << qinfo->set;
    else
        tool->args.expanded &= ~(1 << qinfo->set);
}

static GtkWidget*
gwy_tool_grain_measure_param_view_new(GwyToolGrainMeasure *tool)
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
    g_object_set(renderer, "xalign", 1.0, NULL);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_value, tool, NULL);

    /* Restore set visibility state */
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            const QuantityInfo *qinfo;

            gtk_tree_model_get(model, &iter, 0, &qinfo, -1);
            if (qinfo->quantity == -1
                && (tool->args.expanded & (1 << qinfo->set))) {
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
gwy_tool_grain_measure_data_switched(GwyTool *gwytool,
                                     GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolGrainMeasure *tool;

    tool = GWY_TOOL_GRAIN_MEASURE(gwytool);
    gwy_tool_grain_measure_invalidate(tool);

    GWY_TOOL_CLASS(gwy_tool_grain_measure_parent_class)->data_switched(gwytool,
                                                                    data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_point,
                                "draw-marker", TRUE,
                                "marker-radius", 0,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
        gwy_tool_grain_measure_update_units(tool);
    }
}

static void
gwy_tool_grain_measure_data_changed(GwyPlainTool *plain_tool)
{
    gwy_tool_grain_measure_invalidate(GWY_TOOL_GRAIN_MEASURE(plain_tool));
    gwy_tool_grain_measure_selection_changed(plain_tool, -1);
}

static void
gwy_tool_grain_measure_mask_changed(GwyPlainTool *plain_tool)
{
    gwy_tool_grain_measure_invalidate(GWY_TOOL_GRAIN_MEASURE(plain_tool));
    gwy_tool_grain_measure_selection_changed(plain_tool, -1);
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

static void
gwy_tool_grain_measure_selection_changed(GwyPlainTool *plain_tool,
                                         gint hint)
{
    GwyToolGrainMeasure *tool;
    gdouble point[2];
    gint col, row, xres, oldgno;

    g_return_if_fail(hint <= 0);

    tool = GWY_TOOL_GRAIN_MEASURE(plain_tool);
    oldgno = tool->gno;

    if (!plain_tool->mask_field
        || !gwy_selection_get_object(plain_tool->selection, 0, point))
        tool->gno = 0;
    else {
        row = gwy_data_field_rtoi(plain_tool->mask_field, point[1]);
        col = gwy_data_field_rtoj(plain_tool->mask_field, point[0]);
        if (gwy_data_field_get_val(plain_tool->mask_field, col, row)) {
            if (!tool->grains)
                gwy_tool_grain_measure_recalculate(tool);

            xres = gwy_data_field_get_xres(plain_tool->mask_field);
            tool->gno = tool->grains[row*xres + col];
        }
        else
            tool->gno = 0;
    }

    if (tool->gno != oldgno)
        gtk_tree_model_foreach(GTK_TREE_MODEL(tool->store),
                               emit_row_changed, NULL);
}

static void
gwy_tool_grain_measure_invalidate(GwyToolGrainMeasure *tool)
{
    g_free(tool->grains);
    tool->grains = NULL;
    tool->ngrains = 0;
    tool->gno = -1;
}

static void
gwy_tool_grain_measure_update_units(GwyToolGrainMeasure *tool)
{
    GwyPlainTool *plain_tool;
    GwySIUnit *siunitxy, *siunitz;

    plain_tool = GWY_PLAIN_TOOL(tool);
    siunitxy = gwy_data_field_get_si_unit_xy(plain_tool->data_field);
    siunitz = gwy_data_field_get_si_unit_z(plain_tool->data_field);
    tool->same_units = gwy_si_unit_equal(siunitxy, siunitz);
    gwy_si_unit_power(siunitxy, 2, tool->area_unit);
    gwy_si_unit_multiply(tool->area_unit, siunitz, tool->volume_unit);
}

static void
gwy_tool_grain_measure_recalculate(GwyToolGrainMeasure *tool)
{
    GwyPlainTool *plain_tool;
    GwyDataField *dfield, *mask;
    guint i, j;

    plain_tool = GWY_PLAIN_TOOL(tool);
    dfield = plain_tool->data_field;
    mask = plain_tool->mask_field;

    if (!tool->grains) {
        tool->grains = g_new0(gint,
                              gwy_data_field_get_xres(dfield)
                              *gwy_data_field_get_yres(dfield));
        tool->ngrains = gwy_data_field_number_grains(mask, tool->grains);
    }

    for (i = 0; i < MAX_STATS; i++) {
        if (!tool->map[i] || i == GRAIN_QUANTITY_ID)
            continue;

        if (!tool->values[i])
            tool->values[i] = g_array_new(FALSE, FALSE, sizeof(gdouble));
        g_array_set_size(tool->values[i], tool->ngrains + 1);
        gwy_data_field_grains_get_values(dfield,
                                         (gdouble*)tool->values[i]->data,
                                         tool->ngrains, tool->grains, i);
        if (quantities[tool->map[i]-1].units == UNITS_ANGLE) {
            for (j = 1; j < tool->ngrains; j++)
                g_array_index(tool->values[i], gdouble, j) *= 180.0/G_PI;
        }
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
