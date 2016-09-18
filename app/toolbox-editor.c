/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#include "gwyappinternal.h"
#include "gwyddion.h"
#include "toolbox.h"

typedef struct {
    GwyAppActionType type;
    const gchar *name;
    const gchar *func;
    const gchar *id;
    const gchar *gid;
    GwyRunType mode;
} GwyToolboxEditorRow;

typedef struct {
    GtkWidget *dialogue;
    GtkTreeStore *toolbox_model;
    GtkTreeView *toolbox_view;
} GwyToolboxEditor;

static void toolbox_cell_renderer_icon      (GtkTreeViewColumn *column,
                                             GtkCellRenderer *cell,
                                             GtkTreeModel *model,
                                             GtkTreeIter *iter,
                                             gpointer userdata);
static void toolbox_cell_renderer_icon_flags(GtkTreeViewColumn *column,
                                             GtkCellRenderer *renderer,
                                             GtkTreeModel *model,
                                             GtkTreeIter *iter,
                                             gpointer userdata);
static void toolbox_cell_renderer_name      (GtkTreeViewColumn *column,
                                             GtkCellRenderer *cell,
                                             GtkTreeModel *model,
                                             GtkTreeIter *iter,
                                             gpointer userdata);
static void toolbox_cell_renderer_mode      (GtkTreeViewColumn *column,
                                             GtkCellRenderer *cell,
                                             GtkTreeModel *model,
                                             GtkTreeIter *iter,
                                             gpointer userdata);

/* XXX: Work on a copy of the toolbox spec? */
static void
fill_toolbox_treestore(GtkTreeStore *store, GwyToolboxSpec *spec)
{
    const GwyToolboxGroupSpec *gspec;
    const GwyToolboxItemSpec *ispec;
    GArray *group, *item;
    GtkTreeIter giter, iiter;
    GwyToolboxEditorRow *row;
    guint i, j;

    group = spec->group;
    for (i = 0; i < group->len; i++) {
        gspec = &g_array_index(group, GwyToolboxGroupSpec, i);
        row = g_slice_new0(GwyToolboxEditorRow);
        row->type = GWY_APP_ACTION_TYPE_GROUP;
        row->name = gspec->name;
        row->gid = g_quark_to_string(gspec->id);
        gtk_tree_store_insert_with_values(store, &giter, NULL, i,
                                          0, row, -1);
        item = gspec->item;
        for (j = 0; j < item->len; j++) {
            ispec = &g_array_index(item, GwyToolboxItemSpec, j);
            row = g_slice_new0(GwyToolboxEditorRow);
            row->type = ispec->type;
            row->func = (ispec->function
                         ? g_quark_to_string(ispec->function)
                         : NULL);
            row->id = ispec->icon ? g_quark_to_string(ispec->icon) : NULL;
            row->mode = ispec->mode;
            gtk_tree_store_insert_with_values(store, &iiter, &giter, j,
                                              0, row, -1);
        }
    }
}

void
gwy_toolbox_editor(void)
{
    GtkWindow *toolbox;
    GtkBox *vbox;
    GtkWidget *scwin;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeModel *model;
    GwyToolboxEditor editor;
    gint response, width, height;
    GwyToolboxSpec *spec;

    gwy_clear(&editor, 1);
    toolbox = GTK_WINDOW(gwy_app_main_window_get());
    spec = g_object_get_data(G_OBJECT(toolbox), "gwy-app-toolbox-spec");
    g_return_if_fail(spec);
    gtk_icon_size_lookup(GTK_ICON_SIZE_LARGE_TOOLBAR, &width, &height);

    editor.dialogue = gtk_dialog_new_with_buttons(_("Toolbox Editor"),
                                                  toolbox,
                                                  GTK_DIALOG_MODAL,
                                                  GTK_STOCK_CLOSE,
                                                  GTK_RESPONSE_CLOSE,
                                                  GTK_STOCK_APPLY,
                                                  GTK_RESPONSE_APPLY,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_OK,
                                                  NULL);
    gtk_window_set_default_size(GTK_WINDOW(editor.dialogue), -1, 480);
    vbox = GTK_BOX(GTK_DIALOG(editor.dialogue)->vbox);

    editor.toolbox_model = gtk_tree_store_new(1, G_TYPE_POINTER);
    fill_toolbox_treestore(editor.toolbox_model, spec);
    model = GTK_TREE_MODEL(editor.toolbox_model);

    editor.toolbox_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
    gtk_tree_view_set_headers_visible(editor.toolbox_view, FALSE);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(column, FALSE);
    gtk_tree_view_append_column(editor.toolbox_view, column);

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_cell_renderer_set_fixed_size(renderer, width, height);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            toolbox_cell_renderer_icon,
                                            NULL, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_cell_renderer_set_fixed_size(renderer, -1, height);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            toolbox_cell_renderer_icon_flags,
                                            NULL, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(editor.toolbox_view, column);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_cell_renderer_set_fixed_size(renderer, -1, height);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            toolbox_cell_renderer_name,
                                            NULL, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_append_column(editor.toolbox_view, column);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_cell_renderer_set_fixed_size(renderer, -1, height);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            toolbox_cell_renderer_mode,
                                            NULL, NULL);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scwin), GTK_WIDGET(editor.toolbox_view));
    gtk_box_pack_start(vbox, scwin, TRUE, TRUE, 0);

    gtk_tree_view_expand_all(editor.toolbox_view);
    gtk_widget_show_all(editor.dialogue);

    do {
       response = gtk_dialog_run(GTK_DIALOG(editor.dialogue));
    } while (response != GTK_RESPONSE_OK
             && response != GTK_RESPONSE_CLOSE
             && response != GTK_RESPONSE_DELETE_EVENT);

    gtk_widget_destroy(editor.dialogue);
    g_object_unref(editor.toolbox_model);
}

static void
toolbox_cell_renderer_icon(GtkTreeViewColumn *column,
                           GtkCellRenderer *renderer,
                           GtkTreeModel *model,
                           GtkTreeIter *iter,
                           G_GNUC_UNUSED gpointer userdata)
{
    GwyToolboxEditorRow *row;
    GtkWidget *widget;
    GdkPixbuf *pixbuf;
    GtkIconSet *iconset;
    const gchar *id = NULL;

    gtk_tree_model_get(model, iter, 0, &row, -1);
    id = row->id;

    if (!id && row->type == GWY_APP_ACTION_TYPE_PROC)
        id = gwy_process_func_get_stock_id(row->func);
    else if (!id && row->type == GWY_APP_ACTION_TYPE_GRAPH)
        id = gwy_graph_func_get_stock_id(row->func);
    else if (!id && row->type == GWY_APP_ACTION_TYPE_VOLUME)
        id = gwy_volume_func_get_stock_id(row->func);
    else if (!id && row->type == GWY_APP_ACTION_TYPE_XYZ)
        id = gwy_xyz_func_get_stock_id(row->func);
    else if (!id && row->type == GWY_APP_ACTION_TYPE_TOOL && row->func) {
        GType gtype = g_type_from_name(row->func);
        GwyToolClass *tool_class = g_type_class_peek(gtype);
        id = gwy_tool_class_get_stock_id(tool_class);
    }
    else if (row->type == GWY_APP_ACTION_TYPE_BUILTIN) {
        /* TODO */
    }

    if (!id) {
        g_object_set(renderer, "pixbuf", NULL, NULL);
        return;
    }

    iconset = gtk_icon_factory_lookup_default(id);
    widget = GTK_WIDGET(gtk_tree_view_column_get_tree_view(column));
    pixbuf = gtk_icon_set_render_icon(iconset, widget->style, GTK_TEXT_DIR_NONE,
                                      GTK_STATE_NORMAL,
                                      GTK_ICON_SIZE_LARGE_TOOLBAR,
                                      widget, NULL);
    g_object_set(renderer, "pixbuf", pixbuf, NULL);
    g_object_unref(pixbuf);
}

static void
toolbox_cell_renderer_icon_flags(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                 GtkCellRenderer *renderer,
                                 GtkTreeModel *model,
                                 GtkTreeIter *iter,
                                 G_GNUC_UNUSED gpointer userdata)
{
    GwyToolboxEditorRow *row;

    gtk_tree_model_get(model, iter, 0, &row, -1);
    if (row->type <= 0 || !row->id) {
        g_object_set(renderer,
                     "text", NULL,
                     "weight-set", FALSE,
                     "scale-set", FALSE,
                     NULL);
    }
    else {
        g_object_set(renderer,
                     "markup", "<b>!</b>",
                     "weight", PANGO_WEIGHT_BOLD,
                     "weight-set", TRUE,
                     "scale", 1.2,
                     "scale-set", TRUE,
                     NULL);
    }
}

static void
toolbox_cell_renderer_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
                           GtkCellRenderer *renderer,
                           GtkTreeModel *model,
                           GtkTreeIter *iter,
                           G_GNUC_UNUSED gpointer userdata)
{
    GwyToolboxEditorRow *row;

    gtk_tree_model_get(model, iter, 0, &row, -1);
    if (row->type == GWY_APP_ACTION_TYPE_GROUP) {
        g_object_set(renderer,
                     "text", row->name,
                     "weight", PANGO_WEIGHT_BOLD,
                     "weight-set", TRUE,
                     "style-set", FALSE,
                     NULL);
    }
    else if (row->type == GWY_APP_ACTION_TYPE_TOOL && !row->func) {
        g_object_set(renderer,
                     "text", _("remaining tools"),
                     "style", PANGO_STYLE_ITALIC,
                     "style-set", TRUE,
                     "weight-set", FALSE,
                     NULL);
    }
    else if (row->type == GWY_APP_ACTION_TYPE_PLACEHOLDER) {
        g_object_set(renderer,
                     "text", _("placeholder"),
                     "style", PANGO_STYLE_ITALIC,
                     "style-set", TRUE,
                     "weight-set", FALSE,
                     NULL);
    }
    else {
        g_object_set(renderer,
                     "text", row->func,
                     "weight-set", FALSE,
                     "style-set", FALSE,
                     NULL);
    }
}

static void
toolbox_cell_renderer_mode(G_GNUC_UNUSED GtkTreeViewColumn *column,
                           GtkCellRenderer *renderer,
                           GtkTreeModel *model,
                           GtkTreeIter *iter,
                           G_GNUC_UNUSED gpointer userdata)
{
    GwyToolboxEditorRow *row;

    gtk_tree_model_get(model, iter, 0, &row, -1);
    if (row->type == GWY_APP_ACTION_TYPE_GROUP
        || row->type == GWY_APP_ACTION_TYPE_TOOL
        || row->type == GWY_APP_ACTION_TYPE_PLACEHOLDER) {
        g_object_set(renderer, "text", NULL, NULL);
    }
    else {
        const gchar *mode = gwy_enum_to_string(row->mode,
                                               gwy_toolbox_mode_types, -1);
        g_object_set(renderer, "text", mode, NULL);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
