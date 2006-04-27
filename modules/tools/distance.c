/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 Nenad Ocelic, David Necas (Yeti), Petr Klapetek.
 *  E-mail: ocelic@biochem.mpg.de, yeti@gwyddion.net, klapetek@gwyddion.net.
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
#include <libprocess/datafield.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_DISTANCE            (gwy_tool_distance_get_type())
#define GWY_TOOL_DISTANCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_DISTANCE, GwyToolDistance))
#define GWY_IS_TOOL_DISTANCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_DISTANCE))
#define GWY_TOOL_DISTANCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_DISTANCE, GwyToolDistanceClass))

enum {
    NLINES = 12
};

enum {
    COLUMN_I, COLUMN_DX, COLUMN_DY, COLUMN_PHI, COLUMN_R, COLUMN_DZ, NCOLUMNS
};

typedef struct _GwyToolDistance      GwyToolDistance;
typedef struct _GwyToolDistanceClass GwyToolDistanceClass;

struct _GwyToolDistance {
    GwyPlainTool parent_instance;

    GtkTreeView *treeview;
    GtkTreeModel *model;
    GtkWidget *clear;

    gulong selection_id;

    /* potential class data */
    GwySIValueFormat *angle_format;
    GType layer_type_line;
};

struct _GwyToolDistanceClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register                  (void);

static GType  gwy_tool_distance_get_type         (void) G_GNUC_CONST;
static void   gwy_tool_distance_finalize         (GObject *object);
static void   gwy_tool_distance_data_switched    (GwyTool *gwytool,
                                                  GwyDataView *data_view);
static void   gwy_tool_distance_data_changed     (GwyPlainTool *plain_tool);
static void   gwy_tool_distance_response         (GwyTool *tool,
                                                  gint response_id);
static void   gwy_tool_distance_selection_changed(GwySelection *selection,
                                                  gint hint,
                                                  GwyToolDistance *tool);
static void   gwy_tool_distance_update_headers   (GwyToolDistance *tool);
static void   gwy_tool_distance_render_cell      (GtkCellLayout *layout,
                                                  GtkCellRenderer *renderer,
                                                  GtkTreeModel *model,
                                                  GtkTreeIter *iter,
                                                  gpointer user_data);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Distance measurement tool, measures distances and angles."),
    "Nenad Ocelic <ocelic@biochem.mpg.de>",
    "2.0",
    "Nenad Ocelic & David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolDistance, gwy_tool_distance, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_DISTANCE);

    return TRUE;
}

static void
gwy_tool_distance_class_init(GwyToolDistanceClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_distance_finalize;

    tool_class->stock_id = GWY_STOCK_DISTANCE;
    tool_class->title = _("Distance");
    tool_class->tooltip = _("Measure distances and directions between points");
    tool_class->prefix = "/module/distance";
    tool_class->default_height = 200;
    tool_class->data_switched = gwy_tool_distance_data_switched;
    tool_class->response = gwy_tool_distance_response;

    ptool_class->data_changed = gwy_tool_distance_data_changed;
}

static void
gwy_tool_distance_finalize(GObject *object)
{
    GwyToolDistance *tool;
    GwyPlainTool *plain_tool;
    GwySelection *selection;

    plain_tool = GWY_PLAIN_TOOL(object);
    tool = GWY_TOOL_DISTANCE(object);

    if (plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_signal_handler_disconnect(selection, tool->selection_id);
    }

    if (tool->model) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        gwy_object_unref(tool->model);
    }
    if (tool->angle_format)
        gwy_si_unit_value_format_free(tool->angle_format);

    G_OBJECT_CLASS(gwy_tool_distance_parent_class)->finalize(object);
}

static void
gwy_tool_distance_init(GwyToolDistance *tool)
{
    GwyPlainTool *plain_tool;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkDialog *dialog;
    GtkWidget *scwin, *label;
    GwyNullStore *store;
    guint i;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_line = gwy_plain_tool_check_layer_type(plain_tool,
                                                            "GwyLayerLine");
    if (!tool->layer_type_line)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;

    tool->angle_format = g_new0(GwySIValueFormat, 1);
    tool->angle_format->magnitude = 1.0;
    tool->angle_format->precision = 0.1;
    gwy_si_unit_value_format_set_units(tool->angle_format, "deg");

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    store = gwy_null_store_new(0);
    tool->model = GTK_TREE_MODEL(store);
    tool->treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(tool->model));

    for (i = 0; i < NCOLUMNS; i++) {
        column = gtk_tree_view_column_new();
        g_object_set_data(G_OBJECT(column), "id", GUINT_TO_POINTER(i));
        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "xalign", 1.0, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                           gwy_tool_distance_render_cell, tool,
                                           NULL);
        label = gtk_label_new(NULL);
        gtk_tree_view_column_set_widget(column, label);
        gtk_widget_show(label);
        gtk_tree_view_append_column(tool->treeview, column);
    }

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scwin), GTK_WIDGET(tool->treeview));
    gtk_box_pack_start(GTK_BOX(dialog->vbox), scwin, TRUE, TRUE, 0);

    tool->clear = gtk_dialog_add_button(dialog, GTK_STOCK_CLEAR,
                                        GWY_TOOL_RESPONSE_CLEAR);
    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);
    gwy_tool_distance_update_headers(tool);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_distance_data_switched(GwyTool *gwytool,
                                GwyDataView *data_view)
{
    GwyToolDistance *tool;
    GwySelection *selection;
    GwyPlainTool *plain_tool;

    GWY_TOOL_CLASS(gwy_tool_distance_parent_class)->data_switched(gwytool,
                                                                  data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_DISTANCE(gwytool);
    if (plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_signal_handler_disconnect(selection, tool->selection_id);
    }
    if (!data_view) {
        gtk_widget_set_sensitive(tool->clear, FALSE);
        gwy_null_store_set_n_rows(GWY_NULL_STORE(tool->model), 0);
        gwy_tool_distance_update_headers(tool);
        return;
    }

    gwy_plain_tool_assure_layer(plain_tool, tool->layer_type_line);
    gwy_plain_tool_set_selection_key(plain_tool, "line");
    g_object_set(plain_tool->layer, "line-numbers", TRUE, NULL);
    selection = gwy_vector_layer_get_selection(plain_tool->layer);
    gwy_selection_set_max_objects(selection, NLINES);
    tool->selection_id
        = g_signal_connect(selection, "changed",
                           G_CALLBACK(gwy_tool_distance_selection_changed),
                           tool);

    gwy_tool_distance_data_changed(plain_tool);
}

static void
gwy_tool_distance_data_changed(GwyPlainTool *plain_tool)
{
    GwySelection *selection;
    GwyToolDistance *tool;

    tool = GWY_TOOL_DISTANCE(plain_tool);
    selection = gwy_vector_layer_get_selection(plain_tool->layer);

    gwy_tool_distance_update_headers(tool);
    gwy_tool_distance_selection_changed(selection, -1, tool);
}

static void
gwy_tool_distance_response(GwyTool *tool,
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

        default:
        g_return_if_reached();
        break;
    }
}

static void
gwy_tool_distance_selection_changed(GwySelection *selection,
                                    gint hint,
                                    GwyToolDistance *tool)
{
    GwyNullStore *store;
    gint n;

    store = GWY_NULL_STORE(tool->model);
    n = gwy_null_store_get_n_rows(store);
    g_return_if_fail(hint <= n);

    if (hint < 0) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        n = gwy_selection_get_data(selection, NULL);
        gwy_null_store_set_n_rows(store, n);
        gtk_tree_view_set_model(tool->treeview, tool->model);
        gtk_widget_set_sensitive(tool->clear, n > 0);
    }
    else {
        if (hint < n)
            gwy_null_store_row_changed(store, hint);
        else {
            gwy_null_store_set_n_rows(store, n+1);
            gtk_widget_set_sensitive(tool->clear, TRUE);
        }
    }
}

static void
gwy_tool_distance_update_header(GwyToolDistance *tool,
                                guint col,
                                GString *str,
                                const gchar *title,
                                GwySIValueFormat *vf)
{
    GtkTreeViewColumn *column;
    GtkLabel *label;

    column = gtk_tree_view_get_column(tool->treeview, col);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));

    g_string_assign(str, title);
    if (vf)
        g_string_append_printf(str, " [%s]", vf->units);
    gtk_label_set_markup(label, str->str);
}

static void
gwy_tool_distance_update_headers(GwyToolDistance *tool)
{
    GwyPlainTool *plain_tool;
    GString *str;

    plain_tool = GWY_PLAIN_TOOL(tool);
    str = g_string_new("");

    gwy_tool_distance_update_header(tool, COLUMN_I, str,
                                    "n", NULL);
    gwy_tool_distance_update_header(tool, COLUMN_DX, str,
                                    "Δx", plain_tool->coord_format);
    gwy_tool_distance_update_header(tool, COLUMN_DY, str,
                                    "Δy", plain_tool->coord_format);
    gwy_tool_distance_update_header(tool, COLUMN_PHI, str,
                                    "φ", tool->angle_format);
    gwy_tool_distance_update_header(tool, COLUMN_R, str,
                                    "R", plain_tool->coord_format);
    gwy_tool_distance_update_header(tool, COLUMN_DZ, str,
                                    "Δz", plain_tool->value_format);

    g_string_free(str, TRUE);
}

static void
gwy_tool_distance_render_cell(GtkCellLayout *layout,
                              GtkCellRenderer *renderer,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
    GwyToolDistance *tool = (GwyToolDistance*)user_data;
    GwyPlainTool *plain_tool;
    GwySelection *selection;
    const GwySIValueFormat *vf;
    gchar buf[32];
    gdouble line[4];
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
    selection = gwy_vector_layer_get_selection(plain_tool->layer);
    gwy_selection_get_object(selection, idx, line);

    switch (id) {
        case COLUMN_DX:
        vf = plain_tool->coord_format;
        val = line[2] - line[0];
        break;

        case COLUMN_DY:
        vf = plain_tool->coord_format;
        val = line[3] - line[1];
        break;

        case COLUMN_R:
        vf = plain_tool->coord_format;
        val = hypot(line[2] - line[0], line[3] - line[1]);
        break;

        case COLUMN_PHI:
        vf = tool->angle_format;
        val = atan2(line[3] - line[1], line[2] - line[0]) * 180.0/G_PI;
        break;

        case COLUMN_DZ:
        {
            gint x, y;

            x = gwy_data_field_rtoj(plain_tool->data_field, line[2]);
            y = gwy_data_field_rtoi(plain_tool->data_field, line[3]);
            val = gwy_data_field_get_val(plain_tool->data_field, x, y);
            x = gwy_data_field_rtoj(plain_tool->data_field, line[0]);
            y = gwy_data_field_rtoi(plain_tool->data_field, line[1]);
            val -= gwy_data_field_get_val(plain_tool->data_field, x, y);
            vf = plain_tool->value_format;
        }
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
