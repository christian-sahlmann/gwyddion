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
    guint radius;
    gboolean instant_apply;
    gboolean allow_undo;
    gboolean set_zero;
} ToolArgs;

struct _GwyToolLevel3 {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkTreeView *treeview;
    GtkTreeModel *model;
    GtkObject *radius;
    GtkWidget *instant_apply;
    GtkWidget *set_zero;
    GtkWidget *apply;

    /* potential class data */
    GType layer_type_point;
};

struct _GwyToolLevel3Class {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType  gwy_tool_level3_get_type             (void) G_GNUC_CONST;
static void   gwy_tool_level3_finalize             (GObject *object);
static void   gwy_tool_level3_init_dialog          (GwyToolLevel3 *tool);
static void   gwy_tool_level3_data_switched        (GwyTool *gwytool,
                                                    GwyDataView *data_view);
static void   gwy_tool_level3_data_changed         (GwyPlainTool *plain_tool);
static void   gwy_tool_level3_response             (GwyTool *tool,
                                                    gint response_id);
static void   gwy_tool_level3_selection_changed    (GwyPlainTool *plain_tool,
                                                    gint hint);
static void   gyw_tool_level3_selection_finished   (GwyPlainTool *plain_tool);
static void   gwy_tool_level3_radius_changed       (GwyToolLevel3 *tool);
static void   gwy_tool_level3_instant_apply_changed(GtkToggleButton *check,
                                                    GwyToolLevel3 *tool);
static void   gwy_tool_level3_set_zero_changed     (GtkToggleButton *check,
                                                    GwyToolLevel3 *tool);
static void   gwy_tool_level3_update_headers       (GwyToolLevel3 *tool);
static void   gwy_tool_level3_render_cell          (GtkCellLayout *layout,
                                                    GtkCellRenderer *renderer,
                                                    GtkTreeModel *model,
                                                    GtkTreeIter *iter,
                                                    gpointer user_data);
static void   gwy_tool_level3_apply                (GwyToolLevel3 *tool);

static const gchar radius_key[]        = "/module/level3/radius";
static const gchar instant_apply_key[] = "/module/level3/instant_apply";
static const gchar set_zero_key[]      = "/module/level3/set_zero";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Three-point level tool, levels data by subtracting a plane fitted "
       "through three selected points."),
    "Yeti <yeti@gwyddion.net>",
    "2.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static const ToolArgs default_args = {
    1,
    FALSE,
    TRUE,
    FALSE,
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
    tool_class->title = _("Three Point Level");
    tool_class->tooltip = _("Level data by fitting a plane through three "
                            "points");
    tool_class->prefix = "/module/level3";
    tool_class->data_switched = gwy_tool_level3_data_switched;
    tool_class->response = gwy_tool_level3_response;

    ptool_class->data_changed = gwy_tool_level3_data_changed;
    ptool_class->selection_changed = gwy_tool_level3_selection_changed;
    ptool_class->selection_finished = gyw_tool_level3_selection_finished;
}

static void
gwy_tool_level3_finalize(GObject *object)
{
    GwyToolLevel3 *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_LEVEL3(object);

    if (tool->model) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        gwy_object_unref(tool->model);
    }

    settings = gwy_app_settings_get();
    gwy_container_set_int32_by_name(settings, radius_key, tool->args.radius);
    gwy_container_set_boolean_by_name(settings, instant_apply_key,
                                      tool->args.instant_apply);
    gwy_container_set_boolean_by_name(settings, set_zero_key,
                                      tool->args.set_zero);

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
    gwy_container_gis_boolean_by_name(settings, instant_apply_key,
                                      &tool->args.instant_apply);
    gwy_container_gis_boolean_by_name(settings, set_zero_key,
                                      &tool->args.set_zero);

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_point,
                                     "point");

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

    table = gtk_table_new(3, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), table, TRUE, TRUE, 0);

    tool->radius = gtk_adjustment_new(tool->args.radius, 1, 16, 1, 5, 0);
    gwy_table_attach_spinbutton(table, 1, _("_Averaging radius:"), "px",
                                tool->radius);
    g_signal_connect_swapped(tool->radius, "value-changed",
                             G_CALLBACK(gwy_tool_level3_radius_changed), tool);
    gtk_table_set_row_spacing(GTK_TABLE(table), 1, 8);

    tool->instant_apply
            = gtk_check_button_new_with_mnemonic(_("_Instant apply"));
    gtk_table_attach(GTK_TABLE(table), tool->instant_apply, 0, 3, 2, 3,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->instant_apply),
                                 tool->args.instant_apply);
    g_signal_connect(tool->instant_apply, "toggled",
                     G_CALLBACK(gwy_tool_level3_instant_apply_changed), tool);

    tool->set_zero
            = gtk_check_button_new_with_mnemonic(_("Set plane to _zero"));
    gtk_table_attach(GTK_TABLE(table), tool->set_zero, 0, 3, 3, 4,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->set_zero),
                                 tool->args.set_zero);
    g_signal_connect(tool->set_zero, "toggled",
                     G_CALLBACK(gwy_tool_level3_set_zero_changed), tool);

    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gwy_tool_level3_update_headers(tool);

    gtk_widget_set_sensitive(tool->apply, !tool->args.instant_apply);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_level3_data_switched(GwyTool *gwytool,
                              GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolLevel3 *tool;
    gboolean ignore;

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    ignore = (data_view == plain_tool->data_view);

    GWY_TOOL_CLASS(gwy_tool_level3_parent_class)->data_switched(gwytool,
                                                                data_view);

    if (ignore || plain_tool->init_failed)
        return;

    if (data_view) {
        tool = GWY_TOOL_LEVEL3(gwytool);
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_point,
                                "marker-radius", tool->args.radius - 1,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 3);
        tool->args.allow_undo = TRUE;
    }

    gwy_tool_level3_update_headers(GWY_TOOL_LEVEL3(gwytool));
}

static void
gwy_tool_level3_data_changed(GwyPlainTool *plain_tool)
{
    GwyToolLevel3 *tool;
    GwyNullStore *store;
    guint i, n;

    tool = GWY_TOOL_LEVEL3(plain_tool);
    gwy_tool_level3_update_headers(tool);
    n = gwy_selection_get_data(plain_tool->selection, NULL);
    store = GWY_NULL_STORE(tool->model);
    for (i = 0; i < n; i++)
        gwy_null_store_row_changed(store, i);
}

static void
gwy_tool_level3_response(GwyTool *tool,
                         gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_level3_parent_class)->response(tool, response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_level3_apply(GWY_TOOL_LEVEL3(tool));
}

static void
gwy_tool_level3_selection_changed(GwyPlainTool *plain_tool,
                                  gint hint)
{
    GwyToolLevel3 *tool;
    GwyNullStore *store;
    guint n, i;

    tool = GWY_TOOL_LEVEL3(plain_tool);
    store = GWY_NULL_STORE(tool->model);
    g_return_if_fail(hint <= 3);

    if (plain_tool->selection)
        n = gwy_selection_get_data(plain_tool->selection, NULL);
    else
        n = 0;

    if (hint < 0) {
        for (i = 0; i < n; i++)
            gwy_null_store_row_changed(store, i);
    }
    else
        gwy_null_store_row_changed(store, hint);

    gtk_widget_set_sensitive(tool->apply, n == 3 && !tool->args.instant_apply);

    if (n == 3 && tool->args.instant_apply) {
        gwy_tool_level3_apply(tool);
        tool->args.allow_undo = FALSE;
    }
    else
        tool->args.allow_undo = TRUE;
}

static void
gyw_tool_level3_selection_finished(GwyPlainTool *plain_tool)
{
    GwyToolLevel3 *tool = GWY_TOOL_LEVEL3(plain_tool);
    gint n = 0;

    if (plain_tool->selection)
        n = gwy_selection_get_data(plain_tool->selection, NULL);

    if (n == 3 && tool->args.instant_apply)
        tool->args.allow_undo = TRUE;
}

static void
gwy_tool_level3_radius_changed(GwyToolLevel3 *tool)
{
    GwyPlainTool *plain_tool;

    tool->args.radius = gwy_adjustment_get_int(tool->radius);

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_object_set(plain_tool->layer, "marker-radius", tool->args.radius-1, NULL);
    if (plain_tool->selection)
        gwy_tool_level3_selection_changed(plain_tool, -1);
}

static void
gwy_tool_level3_instant_apply_changed(GtkToggleButton *check,
                                      GwyToolLevel3 *tool)
{
    GwyPlainTool *plain_tool = GWY_PLAIN_TOOL(tool);
    gint n = 0;

    tool->args.instant_apply = gtk_toggle_button_get_active(check);

    if (plain_tool->selection)
        n = gwy_selection_get_data(plain_tool->selection, NULL);
    gtk_widget_set_sensitive(tool->apply, n == 3 && !tool->args.instant_apply);

    if (tool->args.instant_apply)
        gwy_tool_level3_apply(tool);
}

static void
gwy_tool_level3_set_zero_changed(GtkToggleButton *check,
                                 GwyToolLevel3 *tool)
{
    tool->args.set_zero = gtk_toggle_button_get_active(check);
    if (tool->args.instant_apply)
        gwy_tool_level3_apply(tool);
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
                                  "n", NULL);
    gwy_tool_level3_update_header(tool, COLUMN_X, str,
                                  "x", plain_tool->coord_format);
    gwy_tool_level3_update_header(tool, COLUMN_Y, str,
                                  "y", plain_tool->coord_format);
    gwy_tool_level3_update_header(tool, COLUMN_Z, str,
                                  _("Value"), plain_tool->value_format);

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
    if (!plain_tool->selection
        || !gwy_selection_get_object(plain_tool->selection, idx, point)) {
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
    gdouble points[9], z[3], coeffs[3];
    gint xres, yres, i;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->id >= 0 && plain_tool->data_field != NULL);

    if (gwy_selection_get_data(plain_tool->selection, points) < 3) {
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
    if (!tool->args.set_zero)
        coeffs[2] = -0.5*(coeffs[0]*xres + coeffs[1]*yres);
    if (tool->args.allow_undo)
        gwy_app_undo_qcheckpoint(plain_tool->container,
                                 gwy_app_get_data_key_for_id(plain_tool->id),
                                 0);
    gwy_data_field_plane_level(plain_tool->data_field,
                               coeffs[2], coeffs[0], coeffs[1]);

    gwy_data_field_data_changed(plain_tool->data_field);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

