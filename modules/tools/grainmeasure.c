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
#include <libgwydgets/gwygrainvaluemenu.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_GRAIN_MEASURE            (gwy_tool_grain_measure_get_type())
#define GWY_TOOL_GRAIN_MEASURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_GRAIN_MEASURE, GwyToolGrainMeasure))
#define GWY_IS_TOOL_GRAIN_MEASURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_GRAIN_MEASURE))
#define GWY_TOOL_GRAIN_MEASURE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_GRAIN_MEASURE, GwyToolGrainMeasureClass))

typedef struct _GwyToolGrainMeasure      GwyToolGrainMeasure;
typedef struct _GwyToolGrainMeasureClass GwyToolGrainMeasureClass;

typedef struct {
    guint expanded;
} ToolArgs;

struct _GwyToolGrainMeasure {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkTreeView *treeview;
    GPtrArray *values;
    gint value_col;
    gint ngrains;
    gint *grains;
    gint gno;

    gboolean same_units;
    GwySIUnit *siunit;
    GwySIValueFormat *vf;

    /* potential class data */
    GType layer_type_point;
};

struct _GwyToolGrainMeasureClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_grain_measure_get_type        (void) G_GNUC_CONST;
static void gwy_tool_grain_measure_finalize         (GObject *object);
static void gwy_tool_grain_measure_init_dialog      (GwyToolGrainMeasure *tool);
static void gwy_tool_grain_measure_data_switched    (GwyTool *gwytool,
                                                     GwyDataView *data_view);
static void gwy_tool_grain_measure_data_changed     (GwyPlainTool *plain_tool);
static void gwy_tool_grain_measure_mask_changed     (GwyPlainTool *plain_tool);
static void gwy_tool_grain_measure_selection_changed(GwyPlainTool *plain_tool,
                                                     gint hint);
static void gwy_tool_grain_measure_invalidate       (GwyToolGrainMeasure *tool);
static void gwy_tool_grain_measure_update_units     (GwyToolGrainMeasure *tool);
static void gwy_tool_grain_measure_recalculate      (GwyToolGrainMeasure *tool);

static const ToolArgs default_args = {
    0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Grain measurement tool, calculates characteristics of selected "
       "countinous parts of mask."),
    "Yeti <yeti@gwyddion.net>",
    "1.4",
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
    tool->args.expanded
        = gwy_grain_value_tree_view_get_expanded_groups(tool->treeview);
    gwy_container_set_int32_by_name(settings, expanded_key,
                                    tool->args.expanded);

    g_free(tool->grains);
    gwy_object_unref(tool->siunit);
    if (tool->values) {
        for (i = 0; i < tool->values->len; i++)
            g_free(tool->values->pdata[i]);
        g_ptr_array_free(tool->values, TRUE);
    }
    if (tool->vf)
        gwy_si_unit_value_format_free(tool->vf);

    G_OBJECT_CLASS(gwy_tool_grain_measure_parent_class)->finalize(object);
}

static void
gwy_tool_grain_measure_init(GwyToolGrainMeasure *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_point = gwy_plain_tool_check_layer_type(plain_tool,
                                                             "GwyLayerPoint");
    if (!tool->layer_type_point)
        return;

    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_int32_by_name(settings, expanded_key,
                                    &tool->args.expanded);

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_point,
                                     "pointer");

    gwy_tool_grain_measure_init_dialog(tool);
}

static void
render_value(G_GNUC_UNUSED GtkTreeViewColumn *column,
             GtkCellRenderer *renderer,
             GtkTreeModel *model,
             GtkTreeIter *iter,
             gpointer user_data)
{
    GwyToolGrainMeasure *tool = (GwyToolGrainMeasure*)user_data;
    GwyGrainValue *gvalue;
    gdouble value;
    const gdouble *values;
    gchar buf[64];
    const gchar *name;
    gint i;

    gtk_tree_model_get(model, iter,
                       GWY_GRAIN_VALUE_STORE_COLUMN_ITEM, &gvalue,
                       -1);
    if (tool->gno <= 0 || !gvalue) {
        g_object_set(renderer, "text", "", NULL);
        return;
    }

    g_object_unref(gvalue);
    if (!tool->same_units
        && (gwy_grain_value_get_flags(gvalue) & GWY_GRAIN_VALUE_SAME_UNITS)) {
        g_object_set(renderer, "text", _("N.A."), NULL);
        return;
    }

    /* FIXME: Magic number, see top of gwygrainvalue.c */
    if ((gint)gwy_grain_value_get_quantity(gvalue) > 31) {
        g_snprintf(buf, sizeof(buf), "%d", tool->gno);
        g_object_set(renderer, "text", buf, NULL);
        return;
    }

    name = gwy_resource_get_name(GWY_RESOURCE(gvalue));
    i = gwy_inventory_get_item_position(gwy_grain_values(), name);
    if (i < 0) {
        g_warning("Grain value not present in inventory.");
        g_object_set(renderer, "text", "", NULL);
        return;
    }

    values = g_ptr_array_index(tool->values, i);
    value = values[tool->gno];

    if (gwy_grain_value_get_flags(gvalue) & GWY_GRAIN_VALUE_IS_ANGLE) {
        g_snprintf(buf, sizeof(buf), "%.1f deg", 180.0/G_PI*value);
        g_object_set(renderer, "text", buf, NULL);
    }
    else {
        GwySIUnit *siunitxy, *siunitz;
        GwyDataField *dfield;
        GwySIUnitFormatStyle style = GWY_SI_UNIT_FORMAT_VFMARKUP;

        dfield = GWY_PLAIN_TOOL(tool)->data_field;
        siunitxy = gwy_data_field_get_si_unit_xy(dfield);
        siunitz = gwy_data_field_get_si_unit_z(dfield);
        tool->siunit
            = gwy_si_unit_power_multiply(siunitxy,
                                         gwy_grain_value_get_power_xy(gvalue),
                                         siunitz,
                                         gwy_grain_value_get_power_z(gvalue),
                                         tool->siunit);
        tool->vf = gwy_si_unit_get_format_with_digits(tool->siunit, style,
                                                      value, 3, tool->vf);
        g_snprintf(buf, sizeof(buf), "%.*f%s%s",
                   tool->vf->precision, value/tool->vf->magnitude,
                   *tool->vf->units ? " " : "", tool->vf->units);
        g_object_set(renderer, "markup", buf, NULL);
    }
}

static void
gwy_tool_grain_measure_init_dialog(GwyToolGrainMeasure *tool)
{
    GtkDialog *dialog;
    GtkWidget *treeview, *scwin;
    GtkTreeSelection *selection;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), scwin, TRUE, TRUE, 0);

    treeview = gwy_grain_value_tree_view_new(TRUE,
                                             "name", "symbol_markup", NULL);
    tool->treeview = GTK_TREE_VIEW(treeview);
    gtk_tree_view_set_headers_visible(tool->treeview, FALSE);
    gtk_container_add(GTK_CONTAINER(scwin), treeview);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(tool->treeview, column);
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 1.0, NULL);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_value, tool, NULL);

    selection = gtk_tree_view_get_selection(tool->treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

    gwy_grain_value_tree_view_set_expanded_groups(tool->treeview,
                                                  tool->args.expanded);

    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_grain_measure_data_switched(GwyTool *gwytool,
                                     GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolGrainMeasure *tool;
    gboolean ignore;

    tool = GWY_TOOL_GRAIN_MEASURE(gwytool);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    ignore = (data_view == plain_tool->data_view);

    if (!ignore)
        gwy_tool_grain_measure_invalidate(tool);

    GWY_TOOL_CLASS(gwy_tool_grain_measure_parent_class)->data_switched(gwytool,
                                                                    data_view);

    if (ignore || plain_tool->init_failed)
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
        row = floor(gwy_data_field_rtoi(plain_tool->mask_field, point[1]));
        col = floor(gwy_data_field_rtoj(plain_tool->mask_field, point[0]));
        if (gwy_data_field_get_val(plain_tool->mask_field, col, row)) {
            if (!tool->grains)
                gwy_tool_grain_measure_recalculate(tool);

            xres = gwy_data_field_get_xres(plain_tool->mask_field);
            tool->gno = tool->grains[row*xres + col];
        }
        else
            tool->gno = 0;
    }

    if (tool->gno != oldgno) {
        GtkTreeModel *model;

        model = gtk_tree_view_get_model(tool->treeview);
        gtk_tree_model_foreach(model, emit_row_changed, NULL);
    }
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
    GwyDataField *dfield;
    GwySIUnit *siunitxy, *siunitz;

    dfield = GWY_PLAIN_TOOL(tool)->data_field;
    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    siunitz = gwy_data_field_get_si_unit_z(dfield);
    tool->same_units = gwy_si_unit_equal(siunitxy, siunitz);
}

static void
gwy_tool_grain_measure_recalculate(GwyToolGrainMeasure *tool)
{
    GwyPlainTool *plain_tool;
    GwyDataField *dfield, *mask;
    GwyInventory *inventory;
    GwyGrainValue **gvalues;
    guint n, i;

    plain_tool = GWY_PLAIN_TOOL(tool);
    dfield = plain_tool->data_field;
    mask = plain_tool->mask_field;

    if (!tool->grains) {
        tool->grains = g_new0(gint,
                              gwy_data_field_get_xres(dfield)
                              *gwy_data_field_get_yres(dfield));
        tool->ngrains = gwy_data_field_number_grains(mask, tool->grains);
    }

    inventory = gwy_grain_values();
    n = gwy_inventory_get_n_items(inventory);

    if (!tool->values) {
        tool->values = g_ptr_array_new();
        g_ptr_array_set_size(tool->values, n);
    }

    gvalues = g_new(GwyGrainValue*, n);
    for (i = 0; i < n; i++) {
        gvalues[i] = gwy_inventory_get_nth_item(inventory, i);
        g_ptr_array_index(tool->values, i)
             = g_renew(gdouble, g_ptr_array_index(tool->values, i),
                       tool->ngrains+1);
    }

    gwy_grain_values_calculate(n, gvalues, (gdouble**)tool->values->pdata,
                               dfield, tool->ngrains, tool->grains);
    g_free(gvalues);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
