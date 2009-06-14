/*
 *  @(#) $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/grains.h>
#include <libprocess/fractals.h>
#include <libprocess/correct.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_SELECTION_MANAGER            (gwy_tool_selection_manager_get_type())
#define GWY_TOOL_SELECTION_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_SELECTION_MANAGER, GwyToolSelectionManager))
#define GWY_IS_TOOL_SELECTION_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_SELECTION_MANAGER))
#define GWY_TOOL_SELECTION_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_SELECTION_MANAGER, GwyToolSelectionManagerClass))

enum {
    MODEL_ID,
    MODEL_OBJECT,
    MODEL_WIDGET,
    MODEL_N_COLUMNS
};

typedef struct _GwyToolSelectionManager      GwyToolSelectionManager;
typedef struct _GwyToolSelectionManagerClass GwyToolSelectionManagerClass;

typedef struct {
    gint msvc60_does_not_permit_empty_structs;
} ToolArgs;

struct _GwyToolSelectionManager {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkListStore *model;
    GtkWidget *treeview;

    /* potential class data */
    GType layer_type_point;
};

struct _GwyToolSelectionManagerClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_selection_manager_get_type        (void) G_GNUC_CONST;
static void gwy_tool_selection_manager_finalize         (GObject *object);
static void gwy_tool_selection_manager_init_dialog      (GwyToolSelectionManager *tool);
static void gwy_tool_selection_manager_data_switched    (GwyTool *gwytool,
                                                     GwyDataView *data_view);

//static const gchar mode_key[]   = "/module/grainremover/mode";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Grain removal tool, removes continuous parts of mask and/or "
       "underlying data."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2009",
};

static const ToolArgs default_args = {
    0,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolSelectionManager, gwy_tool_selection_manager, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_SELECTION_MANAGER);

    return TRUE;
}

static void
gwy_tool_selection_manager_class_init(GwyToolSelectionManagerClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_selection_manager_finalize;

    tool_class->stock_id = GWY_SELECTIONS;
    tool_class->title = _("Selection Manager");
    tool_class->tooltip = _("Display and copy selections");
    tool_class->prefix = "/module/selmanager";
    tool_class->data_switched = gwy_tool_selection_manager_data_switched;
}

static void
gwy_tool_selection_manager_finalize(GObject *object)
{
    GwyToolSelectionManager *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_SELECTION_MANAGER(object);

    settings = gwy_app_settings_get();
    /*
    gwy_container_set_int32_by_name(settings,
                                    mode_key, tool->args.mode);
                                    */
    g_object_unref(tool->model);

    G_OBJECT_CLASS(gwy_tool_selection_manager_parent_class)->finalize(object);
}

static void
gwy_tool_selection_manager_init(GwyToolSelectionManager *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_point = gwy_plain_tool_check_layer_type(plain_tool,
                                                             "GwyLayerPoint");
    if (!tool->layer_type_point)
        return;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    /*
    gwy_container_gis_enum_by_name(settings, mode_key, &tool->args.mode);
    */

    tool->model = gtk_list_store_new(MODEL_N_COLUMNS,
                                     G_TYPE_INT, G_TYPE_OBJECT, G_TYPE_OBJECT);

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_point,
                                     "pointer");

    gwy_tool_selection_manager_init_dialog(tool);
}

static void
render_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer user_data)
{
    GQuark quark;
    const gchar *s;

    gtk_tree_model_get(model, iter, MODEL_ID, &quark, -1);
    s = g_quark_to_string(quark);
    g_return_if_fail(s);
    s = strrchr(s, GWY_CONTAINER_PATHSEP);
    g_return_if_fail(s);
    g_object_set(renderer, "text", s+1, NULL);
}

static void
render_type(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer user_data)
{
    GwySelection *sel;

    gtk_tree_model_get(model, iter, MODEL_OBJECT, &sel, -1);
    g_return_if_fail(GWY_IS_SELECTION(sel));
    g_object_set(renderer, "text", G_OBJECT_TYPE_NAME(sel), NULL);
    g_object_unref(sel);
}

static void
render_objects(G_GNUC_UNUSED GtkTreeViewColumn *column,
               GtkCellRenderer *renderer,
               GtkTreeModel *model,
               GtkTreeIter *iter,
               G_GNUC_UNUSED gpointer user_data)
{
    gchar buffer[16];
    GwySelection *sel;

    gtk_tree_model_get(model, iter, MODEL_OBJECT, &sel, -1);
    g_return_if_fail(GWY_IS_SELECTION(sel));
    g_snprintf(buffer, sizeof(buffer), "%d", gwy_selection_get_data(sel, NULL));
    g_object_set(renderer, "text", buffer, NULL);
    g_object_unref(sel);
}

static void
gwy_tool_selection_manager_init_dialog(GwyToolSelectionManager *tool)
{
    GtkDialog *dialog;
    GtkWidget *scwin;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), scwin, TRUE, TRUE, 0);

    tool->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tool->model));
    gtk_container_add(GTK_CONTAINER(scwin), tool->treeview);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Selection"));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tool->treeview), column);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_name, tool, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "Type");
    gtk_tree_view_append_column(GTK_TREE_VIEW(tool->treeview), column);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_type, tool, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "Objects");
    gtk_tree_view_append_column(GTK_TREE_VIEW(tool->treeview), column);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_objects, tool, NULL);

    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

    gtk_widget_show_all(dialog->vbox);
}

static void
add_selection(gpointer hkey, gpointer hvalue, gpointer data)
{
    GQuark quark = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    GwyToolSelectionManager *tool = (GwyToolSelectionManager*)data;
    GwySelection *sel = g_value_get_object(value);
    GtkTreeIter iter;

    gtk_list_store_insert_with_values(tool->model, &iter, G_MAXINT,
                                      MODEL_ID, quark,  /* FIXME */
                                      MODEL_OBJECT, sel,
                                      MODEL_WIDGET, NULL, /* FIXME */
                                      -1);
}

static void
gwy_tool_selection_manager_data_switched(GwyTool *gwytool,
                                         GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolSelectionManager *tool;
    gboolean ignore;

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    ignore = (data_view == plain_tool->data_view);

    GWY_TOOL_CLASS(gwy_tool_selection_manager_parent_class)->data_switched(gwytool,
                                                                     data_view);

    if (ignore || plain_tool->init_failed)
        return;

    tool = GWY_TOOL_SELECTION_MANAGER(gwytool);

    if (data_view) {
        gtk_list_store_clear(tool->model);
        gwy_container_foreach(plain_tool->container,
                              g_strdup_printf("/%d/select", plain_tool->id),
                              (GHFunc)&add_selection,
                              tool);
        /*
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_point,
                                "draw-marker", FALSE,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
        */
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
