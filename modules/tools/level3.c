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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/level.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_LEVEL3            (gwy_tool_level3_get_type())
#define GWY_TOOL_LEVEL3(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_LEVEL3, GwyToolLevel3))
#define GWY_IS_TOOL_LEVEL3(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_LEVEL3))
#define GWY_TOOL_LEVEL3_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_LEVEL3, GwyToolLevel3Class))

enum {
    COLUMN_I, COLUMN_X, COLUMN_Y, COLUMN_Z, NCOLUMNS
};

typedef struct _GwyToolLevel3      GwyToolLevel3;
typedef struct _GwyToolLevel3Class GwyToolLevel3Class;

typedef struct {
    gint radius;
} ToolArgs;

struct _GwyToolLevel3 {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkTreeView *treeview;
    GtkTreeModel *model;
    GtkObject *radius;
    GtkWidget *clear;
    GtkWidget *apply;

    gulong selection_id;

    /* potential class data */
    GType layer_type_point;
};

struct _GwyToolLevel3Class {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType  gwy_tool_level3_get_type         (void) G_GNUC_CONST;
static void   gwy_tool_level3_finalize         (GObject *object);
static void   gwy_tool_level3_init_dialog      (GwyToolLevel3 *tool);
static void   gwy_tool_level3_data_switched    (GwyTool *gwytool,
                                                GwyDataView *data_view);
static void   gwy_tool_level3_data_changed     (GwyPlainTool *plain_tool);
static void   gwy_tool_level3_response         (GwyTool *tool,
                                                gint response_id);
static void   gwy_tool_level3_selection_changed(GwySelection *selection,
                                                gint hint,
                                                GwyToolLevel3 *tool);
static void   gwy_tool_level3_radius_changed   (GwyToolLevel3 *tool);
static void   gwy_tool_level3_update_headers   (GwyToolLevel3 *tool);
static void   gwy_tool_level3_render_cell      (GtkCellLayout *layout,
                                                GtkCellRenderer *renderer,
                                                GtkTreeModel *model,
                                                GtkTreeIter *iter,
                                                gpointer user_data);
static void   gwy_tool_level3_apply            (GwyToolLevel3 *tool);

static const gchar radius_key[] = "/module/level3/radius";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Three-point level tool, levels data by subtracting a plane fitted "
       "through three selected points."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static const ToolArgs default_args = {
    1,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolLevel3, gwy_tool_level3, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_LEVEL3);

    return TRUE;
}

static void
gwy_tool_level3_class_init(GwyToolLevel3Class *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_level3_finalize;

    tool_class->stock_id = GWY_STOCK_LEVEL_TRIANGLE;
    tool_class->title = _("Level3");
    tool_class->tooltip = _("Level data by fitting a plane through three "
                            "points");
    tool_class->prefix = "/module/level3";
    tool_class->data_switched = gwy_tool_level3_data_switched;
    tool_class->response = gwy_tool_level3_response;

    ptool_class->data_changed = gwy_tool_level3_data_changed;
}

static void
gwy_tool_level3_finalize(GObject *object)
{
    GwyToolLevel3 *tool;
    GwyPlainTool *plain_tool;
    GwySelection *selection;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(object);
    tool = GWY_TOOL_LEVEL3(object);

    if (plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_signal_handler_disconnect(selection, tool->selection_id);
    }

    if (tool->model) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        gwy_object_unref(tool->model);
    }

    settings = gwy_app_settings_get();
    gwy_container_set_int32_by_name(settings, radius_key, tool->args.radius);

    G_OBJECT_CLASS(gwy_tool_level3_parent_class)->finalize(object);
}

static void
gwy_tool_level3_init(GwyToolLevel3 *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_point = gwy_plain_tool_check_layer_type(plain_tool,
                                                             "GwyLayerPoint");
    if (!tool->layer_type_point)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_int32_by_name(settings, radius_key, &tool->args.radius);

    gwy_tool_level3_init_dialog(tool);
}

static void
gwy_tool_level3_init_dialog(GwyToolLevel3 *tool)
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkDialog *dialog;
    GtkWidget *label, *table;
    GwyNullStore *store;
    guint i;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    store = gwy_null_store_new(3);
    tool->model = GTK_TREE_MODEL(store);
    tool->treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(tool->model));

    for (i = 0; i < NCOLUMNS; i++) {
        column = gtk_tree_view_column_new();
        g_object_set_data(G_OBJECT(column), "id", GUINT_TO_POINTER(i));
        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "xalign", 1.0, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                           gwy_tool_level3_render_cell, tool,
                                           NULL);
        label = gtk_label_new(NULL);
        gtk_tree_view_column_set_widget(column, label);
        gtk_widget_show(label);
        gtk_tree_view_append_column(tool->treeview, column);
    }

    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(tool->treeview),
                       TRUE, TRUE, 0);

    table = gtk_table_new(1, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), table, TRUE, TRUE, 0);

    tool->radius = gtk_adjustment_new(tool->args.radius, 1, 16, 1, 5, 16);
    gwy_table_attach_spinbutton(table, 9, _("_Averaging radius:"), "px",
                                tool->radius);
    g_signal_connect_swapped(tool->radius, "value-changed",
                             G_CALLBACK(gwy_tool_level3_radius_changed), tool);

    tool->clear = gtk_dialog_add_button(dialog, GTK_STOCK_CLEAR,
                                        GWY_TOOL_RESPONSE_CLEAR);
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gwy_tool_level3_update_headers(tool);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_level3_data_switched(GwyTool *gwytool,
                              GwyDataView *data_view)
{
    GwyToolLevel3 *tool;
    GwySelection *selection;
    GwyPlainTool *plain_tool;

    GWY_TOOL_CLASS(gwy_tool_level3_parent_class)->data_switched(gwytool,
                                                                  data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_LEVEL3(gwytool);
    if (plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_signal_handler_disconnect(selection, tool->selection_id);
    }
    if (!data_view) {
        gtk_widget_set_sensitive(tool->clear, FALSE);
        gtk_widget_set_sensitive(tool->apply, FALSE);
        gwy_tool_level3_update_headers(tool);
        return;
    }

    gwy_plain_tool_assure_layer(plain_tool, tool->layer_type_point);
    gwy_plain_tool_set_selection_key(plain_tool, "point");
    g_object_set(plain_tool->layer, "draw-marker", TRUE, NULL);
    selection = gwy_vector_layer_get_selection(plain_tool->layer);
    gwy_selection_set_max_objects(selection, 3);
    tool->selection_id
        = g_signal_connect(selection, "changed",
                           G_CALLBACK(gwy_tool_level3_selection_changed), tool);

    gwy_tool_level3_data_changed(plain_tool);
}

static void
gwy_tool_level3_data_changed(GwyPlainTool *plain_tool)
{
    GwySelection *selection;
    GwyToolLevel3 *tool;

    tool = GWY_TOOL_LEVEL3(plain_tool);
    selection = gwy_vector_layer_get_selection(plain_tool->layer);

    gwy_tool_level3_update_headers(tool);
    gwy_tool_level3_selection_changed(selection, -1, tool);
}

static void
gwy_tool_level3_response(GwyTool *tool,
                         gint response_id)
{
    GwyPlainTool *plain_tool;
    GwySelection *selection;

    switch (response_id) {
        case GWY_TOOL_RESPONSE_CLEAR:
        plain_tool = GWY_PLAIN_TOOL(tool);
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_selection_clear(selection);
        break;

        case GTK_RESPONSE_APPLY:
        gwy_tool_level3_apply(GWY_TOOL_LEVEL3(tool));
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
gwy_tool_level3_selection_changed(GwySelection *selection,
                                  gint hint,
                                  GwyToolLevel3 *tool)
{
    GwyNullStore *store;
    gint n;

    store = GWY_NULL_STORE(tool->model);
    g_return_if_fail(hint <= 3);

    if (hint < 0) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        gtk_tree_view_set_model(tool->treeview, tool->model);
    }
    else
        gwy_null_store_row_changed(store, hint);

    n = gwy_selection_get_data(selection, NULL);
    gtk_widget_set_sensitive(tool->clear, n == 3);
    gtk_widget_set_sensitive(tool->apply, n == 3);
}

static void
gwy_tool_level3_radius_changed(GwyToolLevel3 *tool)
{
    GwyPlainTool *plain_tool;
    GwySelection *selection;

    tool->args.radius = gwy_adjustment_get_int(tool->radius);

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_tool_level3_selection_changed(selection, -1, tool);
    }
}

static void
gwy_tool_level3_update_header(GwyToolLevel3 *tool,
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
gwy_tool_level3_update_headers(GwyToolLevel3 *tool)
{
    GwyPlainTool *plain_tool;
    GString *str;

    plain_tool = GWY_PLAIN_TOOL(tool);
    str = g_string_new("");

    gwy_tool_level3_update_header(tool, COLUMN_I, str,
                                  "<b>n</b>", NULL);
    gwy_tool_level3_update_header(tool, COLUMN_X, str,
                                  "<b>x</b>", plain_tool->coord_format);
    gwy_tool_level3_update_header(tool, COLUMN_Y, str,
                                  "<b>y</b>", plain_tool->coord_format);
    gwy_tool_level3_update_header(tool, COLUMN_Z, str,
                                  _("<b>Value</b>"), plain_tool->value_format);

    g_string_free(str, TRUE);
}

static void
gwy_tool_level3_render_cell(GtkCellLayout *layout,
                            GtkCellRenderer *renderer,
                            GtkTreeModel *model,
                            GtkTreeIter *iter,
                            gpointer user_data)
{
    GwyToolLevel3 *tool = (GwyToolLevel3*)user_data;
    GwyPlainTool *plain_tool;
    GwySelection *selection;
    const GwySIValueFormat *vf;
    gchar buf[32];
    gdouble point[2];
    gdouble val;
    guint idx, id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(layout), "id"));
    gtk_tree_model_get(model, iter, 0, &idx, -1);
    if (id == COLUMN_I) {
        g_snprintf(buf, sizeof(buf), "%d", idx + 1);
        g_object_set(renderer, "text", buf, NULL);
        return;
    }

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (!plain_tool->layer) {
        g_object_set(renderer, "text", "", NULL);
        return;
    }
    selection = gwy_vector_layer_get_selection(plain_tool->layer);
    if (!gwy_selection_get_object(selection, idx, point)) {
        g_object_set(renderer, "text", "", NULL);
        return;
    }

    switch (id) {
        case COLUMN_X:
        vf = plain_tool->coord_format;
        val = point[0];
        break;

        case COLUMN_Y:
        vf = plain_tool->coord_format;
        val = point[1];
        break;

        case COLUMN_Z:
        vf = plain_tool->value_format;
        val = gwy_plain_tool_get_z_average(plain_tool->data_field, point,
                                           tool->args.radius);
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
gwy_tool_level3_apply(GwyToolLevel3 *tool)
{
    GwyPlainTool *plain_tool;
    GwySelection *selection;
    gdouble points[9], z[3], coeffs[3];
    gint xres, yres, i;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->id >= 0 && plain_tool->data_field != NULL);

    selection = gwy_vector_layer_get_selection(plain_tool->layer);
    if (gwy_selection_get_data(selection, points) < 3) {
        g_warning("Apply invoked with less than 3 points");
        return;
    }

    /* find the plane levelling coeffs so that values in the three points
     * will be all zeroes
     *
     *  /       \   /  \     /  \
     * | x1 y1 1 | | bx |   | z1 |
     * | x2 y2 1 | | by | = | z2 |
     * | x3 y3 1 | | c  |   | z3 |
     *  \       /   \  /     \  /
     *
     */
    for (i = 0; i < 3; i++)
        z[i] = gwy_plain_tool_get_z_average(plain_tool->data_field,
                                            points + 2*i, tool->args.radius);
    points[7] = points[5];
    points[6] = points[4];
    points[4] = points[3];
    points[3] = points[2];
    points[2] = points[5] = points[8] = 1.0;
    gwy_math_lin_solve_rewrite(3, points, z, coeffs);
    /* to keep mean value intact, the mean value of the plane we add to the
     * data has to be zero, i.e., in the center of the data the value must
     * be zero */
    coeffs[0] = gwy_data_field_jtor(plain_tool->data_field, coeffs[0]);
    coeffs[1] = gwy_data_field_itor(plain_tool->data_field, coeffs[1]);
    xres = gwy_data_field_get_xres(plain_tool->data_field);
    yres = gwy_data_field_get_yres(plain_tool->data_field);
    coeffs[2] = -0.5*(coeffs[0]*xres + coeffs[1]*yres);
    gwy_app_undo_qcheckpoint(plain_tool->container,
                             gwy_app_get_data_key_for_id(plain_tool->id), 0);
    gwy_data_field_plane_level(plain_tool->data_field,
                               coeffs[2], coeffs[0], coeffs[1]);

    gwy_data_field_data_changed(plain_tool->data_field);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

