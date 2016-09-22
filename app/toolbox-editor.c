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
    GwyToolboxSpec *spec;
    GtkWidget *dialogue;
    GtkTreeStore *toolbox_model;
    GtkTreeView *toolbox_view;
    GtkWidget *add_item;
    GtkWidget *add_group;
    GtkWidget *remove;
    GtkWidget *edit;
    GtkWidget *move_up;
    GtkWidget *move_down;
    GtkAdjustment *width;
    GtkTreeStore *function_model;
    GtkTreeView *function_view;
    GtkListStore *icon_model_gwy;
    GtkListStore *icon_model_gtk;
    GtkIconView *icon_view;
} GwyToolboxEditor;

typedef struct {
    GwyToolboxSpec *spec;
    guint i;
    GwyToolboxGroupSpec gspec;
    GtkWidget *dialogue;
    GtkWidget *title;
    GtkWidget *id;
    GtkWidget *message;
} GwyToolboxGroupEditor;

static void          fill_toolbox_treestore          (GtkTreeStore *store,
                                                      GwyToolboxSpec *spec);
static void          free_toolbox_treestore          (GtkTreeStore *store);
static void          fill_function_list_treestore    (GtkTreeStore *store);
static void          create_toolbox_tree_view        (GwyToolboxEditor *editor);
static void          create_function_tree_view       (GwyToolboxEditor *editor);
static void          create_icon_icon_view           (GwyToolboxEditor *editor);
static void          toolbox_cell_renderer_icon      (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *cell,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static void          toolbox_cell_renderer_icon_flags(GtkTreeViewColumn *column,
                                                      GtkCellRenderer *renderer,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static void          toolbox_cell_renderer_name      (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *cell,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static void          toolbox_cell_renderer_mode      (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *cell,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static void          function_cell_renderer_icon     (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *renderer,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static void          function_cell_renderer_name     (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *renderer,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static gboolean      edit_group_dialogue             (GwyToolboxEditor *editor,
                                                      GwyToolboxGroupSpec *gspec,
                                                      guint i);
static void          group_title_changed             (GwyToolboxGroupEditor *geditor,
                                                      GtkEntry *entry);
static void          group_id_changed                (GwyToolboxGroupEditor *geditor,
                                                      GtkEntry *entry);
static void          suggest_group_id                (GwyToolboxGroupEditor *geditor);
static void          group_title_transl_changed      (GwyToolboxGroupEditor *geditor,
                                                      GtkToggleButton *toggle);
static void          toolbox_selection_changed       (GtkTreeSelection *selection,
                                                      GwyToolboxEditor *editor);
static void          add_toolbox_item                (GwyToolboxEditor *editor);
static void          add_toolbox_group               (GwyToolboxEditor *editor);
static void          edit_item_or_group              (GwyToolboxEditor *editor);
static void          remove_from_toolbox             (GwyToolboxEditor *editor);
static void          move_up_in_toolbox              (GwyToolboxEditor *editor);
static void          move_down_in_toolbox            (GwyToolboxEditor *editor);
static void          width_changed                   (GwyToolboxEditor *editor);
static void          apply_toolbox_spec              (GwyToolboxEditor *editor);
static const gchar*  action_get_nice_name            (GwyAppActionType type,
                                                      const gchar *name);
static const gchar*  find_default_stock_id           (GwyAppActionType type,
                                                      const gchar *name);
static GtkTreeModel* create_gtk_icon_list            (GtkWidget *widget);
static GtkTreeModel* create_gwy_icon_list            (GtkWidget *widget);

void
gwy_toolbox_editor(void)
{
    GwyToolboxSpec *spec;
    GtkWindow *toolbox;
    GtkBox *vbox, *hbox, *hbox2;
    GtkWidget *scwin, *label, *spin;
    GwyToolboxEditor editor;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    gint response;

    gwy_clear(&editor, 1);
    toolbox = GTK_WINDOW(gwy_app_main_window_get());
    spec = g_object_get_data(G_OBJECT(toolbox), "gwy-app-toolbox-spec");
    g_return_if_fail(spec);
    editor.spec = gwy_toolbox_spec_duplicate(spec);

    editor.dialogue = gtk_dialog_new_with_buttons(_("Toolbox Editor"),
                                                  toolbox,
                                                  GTK_DIALOG_MODAL,
                                                  GTK_STOCK_APPLY,
                                                  GTK_RESPONSE_APPLY,
                                                  GTK_STOCK_CLOSE,
                                                  GTK_RESPONSE_CLOSE,
                                                  NULL);
    gtk_window_set_default_size(GTK_WINDOW(editor.dialogue), 480, 480);

    hbox = GTK_BOX(gtk_hbox_new(FALSE, 6));
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(editor.dialogue)->vbox),
                       GTK_WIDGET(hbox), TRUE, TRUE, 0);

    editor.toolbox_model = gtk_tree_store_new(1, G_TYPE_POINTER);
    fill_toolbox_treestore(editor.toolbox_model, editor.spec);
    create_toolbox_tree_view(&editor);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scwin), GTK_WIDGET(editor.toolbox_view));
    gtk_box_pack_start(hbox, scwin, TRUE, TRUE, 0);

    selection = gtk_tree_view_get_selection(editor.toolbox_view);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(editor.toolbox_model),
                                      &iter))
        gtk_tree_selection_select_iter(selection, &iter);
    g_signal_connect(selection, "changed",
                     G_CALLBACK(toolbox_selection_changed), &editor);

    vbox = GTK_BOX(gtk_vbox_new(FALSE, 2));
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
    gtk_box_pack_start(hbox, GTK_WIDGET(vbox), FALSE, FALSE, 0);

    editor.add_item = gwy_stock_like_button_new(_("_New Item"), GTK_STOCK_NEW);
    gtk_box_pack_start(vbox, editor.add_item, FALSE, FALSE, 0);
    g_signal_connect_swapped(editor.add_item, "clicked",
                             G_CALLBACK(add_toolbox_item), &editor);

    editor.add_group = gwy_stock_like_button_new(_("_New Group"),
                                                 GTK_STOCK_NEW);
    gtk_box_pack_start(vbox, editor.add_group, FALSE, FALSE, 0);
    g_signal_connect_swapped(editor.add_group, "clicked",
                             G_CALLBACK(add_toolbox_group), &editor);

    editor.edit = gwy_stock_like_button_new(_("_Edit"), GTK_STOCK_EDIT);
    gtk_box_pack_start(vbox, editor.edit, FALSE, FALSE, 0);
    g_signal_connect_swapped(editor.edit, "clicked",
                             G_CALLBACK(edit_item_or_group), &editor);

    editor.remove = gwy_stock_like_button_new(_("_Remove"), GTK_STOCK_REMOVE);
    gtk_box_pack_start(vbox, editor.remove, FALSE, FALSE, 0);
    g_signal_connect_swapped(editor.remove, "clicked",
                             G_CALLBACK(remove_from_toolbox), &editor);

    editor.move_up = gwy_stock_like_button_new(_("Move _Up"), GTK_STOCK_GO_UP);
    gtk_box_pack_start(vbox, editor.move_up, FALSE, FALSE, 0);
    g_signal_connect_swapped(editor.move_up, "clicked",
                             G_CALLBACK(move_up_in_toolbox), &editor);

    editor.move_down = gwy_stock_like_button_new(_("Move _Down"),
                                                 GTK_STOCK_GO_DOWN);
    gtk_box_pack_start(vbox, editor.move_down, FALSE, FALSE, 0);
    g_signal_connect_swapped(editor.move_down, "clicked",
                             G_CALLBACK(move_down_in_toolbox), &editor);

    hbox2 = GTK_BOX(gtk_hbox_new(FALSE, 6));
    gtk_box_pack_start(vbox, GTK_WIDGET(hbox2), FALSE, FALSE, 8);

    label = gtk_label_new_with_mnemonic(_("_Width:"));
    gtk_box_pack_start(hbox2, label, FALSE, FALSE, 0);

    editor.width = GTK_ADJUSTMENT(gtk_adjustment_new(editor.spec->width,
                                                     1, 20, 1, 2, 0));
    g_signal_connect_swapped(editor.width, "value-changed",
                             G_CALLBACK(width_changed), &editor);
    spin = gtk_spin_button_new(editor.width, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_box_pack_start(hbox2, spin, FALSE, FALSE, 0);

    gtk_widget_show_all(editor.dialogue);
    toolbox_selection_changed(selection, &editor);

    do {
       response = gtk_dialog_run(GTK_DIALOG(editor.dialogue));

       if (response == GTK_RESPONSE_APPLY)
           apply_toolbox_spec(&editor);

    } while (response != GTK_RESPONSE_CLOSE
             && response != GTK_RESPONSE_DELETE_EVENT);

    gtk_widget_destroy(editor.dialogue);

    free_toolbox_treestore(editor.toolbox_model);
    gwy_object_unref(editor.function_model);
    gwy_object_unref(editor.icon_model_gwy);
    gwy_object_unref(editor.icon_model_gtk);
    gwy_toolbox_spec_free(editor.spec);
}

#if 0
    /* XXX: This should go to item add/edit, not the main window. */
    editor.function_model = gtk_tree_store_new(2, G_TYPE_UINT, G_TYPE_STRING);
    fill_function_list_treestore(editor.function_model);
    create_function_tree_view(&editor);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scwin), GTK_WIDGET(editor.function_view));
    gtk_box_pack_start(hbox, scwin, TRUE, TRUE, 0);


    /* XXX: This should go to icon selector, not the main window. */
    create_icon_icon_view(&editor);
    model = create_gwy_icon_list(editor.dialogue);
    gtk_icon_view_set_model(editor.icon_view, model);
    g_object_unref(model);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scwin), GTK_WIDGET(editor.icon_view));
    gtk_box_pack_start(hbox, scwin, TRUE, TRUE, 0);


#endif

static void
fill_toolbox_row_for_group(GwyToolboxEditorRow *row,
                           const GwyToolboxGroupSpec *gspec)
{
    row->type = GWY_APP_ACTION_TYPE_GROUP;
    row->name = gspec->name;
    row->gid = g_quark_to_string(gspec->id);
}

static void
fill_toolbox_row_for_item(GwyToolboxEditorRow *row,
                          const GwyToolboxItemSpec *ispec)
{
    row->type = ispec->type;
    row->func = ispec->function ? g_quark_to_string(ispec->function) : NULL;
    row->id = ispec->icon ? g_quark_to_string(ispec->icon) : NULL;
    row->mode = ispec->mode;
}

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
        fill_toolbox_row_for_group(row, gspec);
        gtk_tree_store_insert_with_values(store, &giter, NULL, i, 0, row, -1);
        item = gspec->item;
        for (j = 0; j < item->len; j++) {
            ispec = &g_array_index(item, GwyToolboxItemSpec, j);
            row = g_slice_new0(GwyToolboxEditorRow);
            fill_toolbox_row_for_item(row, ispec);
            gtk_tree_store_insert_with_values(store, &iiter, &giter, j,
                                              0, row, -1);
        }
    }
}

static gboolean
free_toolbox_treestore_row(GtkTreeModel *model,
                           G_GNUC_UNUSED GtkTreePath *path,
                           GtkTreeIter *iter,
                           G_GNUC_UNUSED gpointer user_data)
{
    GwyToolboxEditorRow *row;
    gtk_tree_model_get(model, iter, 0, &row, -1);
    g_slice_free(GwyToolboxEditorRow, row);
    return FALSE;
}

static void
free_toolbox_treestore(GtkTreeStore *store)
{
    gtk_tree_model_foreach(GTK_TREE_MODEL(store),
                           &free_toolbox_treestore_row, NULL);
    g_object_unref(store);
}

static void
add_function_name(const gchar *name, GPtrArray *func_names)
{
    g_ptr_array_add(func_names, (gpointer)name);
}

static gint
compare_func_names(gconstpointer pa, gconstpointer pb)
{
    const gchar *a = *(const gchar**)pa;
    const gchar *b = *(const gchar**)pb;
    return strcmp(a, b);
}

static void
fill_function_list_treestore(GtkTreeStore *store)
{
    const GwyToolboxBuiltinSpec* spec;
    GtkTreeIter giter, iiter;
    GPtrArray *func_names;
    guint i, n;

    func_names = g_ptr_array_new();

    /* Built-in */
    gtk_tree_store_insert_with_values(store, &giter, NULL, G_MAXINT,
                                      0, GWY_APP_ACTION_TYPE_GROUP,
                                      1, _("Builtin"),
                                      -1);
    spec = gwy_toolbox_get_builtins(&n);
    for (i = 0; i < n; i++) {
        gtk_tree_store_insert_with_values(store, &iiter, &giter, G_MAXINT,
                                          0, GWY_APP_ACTION_TYPE_BUILTIN,
                                          1, spec[i].name,
                                          -1);
    }

    /* Data Process */
    g_ptr_array_set_size(func_names, 0);
    gtk_tree_store_insert_with_values(store, &giter, NULL, G_MAXINT,
                                      0, GWY_APP_ACTION_TYPE_GROUP,
                                      1, _("Data Process"),
                                      -1);
    gwy_process_func_foreach((GFunc)add_function_name, func_names);
    g_ptr_array_sort(func_names, compare_func_names);
    for (i = 0; i < func_names->len; i++) {
        gtk_tree_store_insert_with_values(store, &iiter, &giter, G_MAXINT,
                                          0, GWY_APP_ACTION_TYPE_PROC,
                                          1, g_ptr_array_index(func_names, i),
                                          -1);
    }

    /* Graph */
    g_ptr_array_set_size(func_names, 0);
    gtk_tree_store_insert_with_values(store, &giter, NULL, G_MAXINT,
                                      0, GWY_APP_ACTION_TYPE_GROUP,
                                      1, _("Graph"),
                                      -1);
    gwy_graph_func_foreach((GFunc)add_function_name, func_names);
    g_ptr_array_sort(func_names, compare_func_names);
    for (i = 0; i < func_names->len; i++) {
        gtk_tree_store_insert_with_values(store, &iiter, &giter, G_MAXINT,
                                          0, GWY_APP_ACTION_TYPE_GRAPH,
                                          1, g_ptr_array_index(func_names, i),
                                          -1);
    }

    /* Volume Data */
    g_ptr_array_set_size(func_names, 0);
    gtk_tree_store_insert_with_values(store, &giter, NULL, G_MAXINT,
                                      0, GWY_APP_ACTION_TYPE_GROUP,
                                      1, _("Volume Data"),
                                      -1);
    gwy_volume_func_foreach((GFunc)add_function_name, func_names);
    g_ptr_array_sort(func_names, compare_func_names);
    for (i = 0; i < func_names->len; i++) {
        gtk_tree_store_insert_with_values(store, &iiter, &giter, G_MAXINT,
                                          0, GWY_APP_ACTION_TYPE_VOLUME,
                                          1, g_ptr_array_index(func_names, i),
                                          -1);
    }

    /* XYZ Data */
    g_ptr_array_set_size(func_names, 0);
    gtk_tree_store_insert_with_values(store, &giter, NULL, G_MAXINT,
                                      0, GWY_APP_ACTION_TYPE_GROUP,
                                      1, _("XYZ Data"),
                                      -1);
    gwy_xyz_func_foreach((GFunc)add_function_name, func_names);
    g_ptr_array_sort(func_names, compare_func_names);
    for (i = 0; i < func_names->len; i++) {
        gtk_tree_store_insert_with_values(store, &iiter, &giter, G_MAXINT,
                                          0, GWY_APP_ACTION_TYPE_XYZ,
                                          1, g_ptr_array_index(func_names, i),
                                          -1);
    }

    /* Tool */
    g_ptr_array_set_size(func_names, 0);
    gtk_tree_store_insert_with_values(store, &giter, NULL, G_MAXINT,
                                      0, GWY_APP_ACTION_TYPE_GROUP,
                                      1, _("Tools"),
                                      -1);
    gwy_tool_func_foreach((GFunc)add_function_name, func_names);
    g_ptr_array_sort(func_names, compare_func_names);
    for (i = 0; i < func_names->len; i++) {
        gtk_tree_store_insert_with_values(store, &iiter, &giter, G_MAXINT,
                                          0, GWY_APP_ACTION_TYPE_TOOL,
                                          1, g_ptr_array_index(func_names, i),
                                          -1);
    }

    g_ptr_array_free(func_names, TRUE);
}

static void
create_toolbox_tree_view(GwyToolboxEditor *editor)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    gint width, height;
    GtkTreeModel *model;

    model = GTK_TREE_MODEL(editor->toolbox_model);
    editor->toolbox_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
    gtk_tree_view_set_headers_visible(editor->toolbox_view, FALSE);
    gtk_icon_size_lookup(GTK_ICON_SIZE_LARGE_TOOLBAR, &width, &height);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(column, FALSE);
    gtk_tree_view_append_column(editor->toolbox_view, column);

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
    gtk_tree_view_append_column(editor->toolbox_view, column);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_cell_renderer_set_fixed_size(renderer, -1, height);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            toolbox_cell_renderer_name,
                                            NULL, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_append_column(editor->toolbox_view, column);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_cell_renderer_set_fixed_size(renderer, -1, height);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            toolbox_cell_renderer_mode,
                                            NULL, NULL);

    gtk_tree_view_expand_all(editor->toolbox_view);
}

static void
create_function_tree_view(GwyToolboxEditor *editor)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    gint width, height;
    GtkTreeModel *model;

    model = GTK_TREE_MODEL(editor->function_model);
    editor->function_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
    gtk_tree_view_set_headers_visible(editor->function_view, FALSE);
    gtk_icon_size_lookup(GTK_ICON_SIZE_LARGE_TOOLBAR, &width, &height);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(column, FALSE);
    gtk_tree_view_append_column(editor->function_view, column);

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_cell_renderer_set_fixed_size(renderer, width, height);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            function_cell_renderer_icon,
                                            NULL, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_cell_renderer_set_fixed_size(renderer, -1, height);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            function_cell_renderer_name,
                                            NULL, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(editor->function_view, column);

    gtk_tree_view_expand_all(editor->function_view);
}

static void
create_icon_icon_view(GwyToolboxEditor *editor)
{
    GtkCellRenderer *renderer;
    GtkCellLayout *layout;
    gint width, height;

    editor->icon_view = GTK_ICON_VIEW(gtk_icon_view_new());
    layout = GTK_CELL_LAYOUT(editor->icon_view);
    gtk_icon_size_lookup(GTK_ICON_SIZE_LARGE_TOOLBAR, &width, &height);

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(layout, renderer, FALSE);
    gtk_cell_renderer_set_fixed_size(renderer, width, height);
    gtk_cell_layout_add_attribute(layout, renderer, "pixbuf", 1);
    gtk_icon_view_set_selection_mode(editor->icon_view, GTK_SELECTION_BROWSE);
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

    if (!id)
        id = find_default_stock_id(row->type, row->func);

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
        const gchar *s = action_get_nice_name(row->type, row->func);
        g_object_set(renderer,
                     "text", s ? s : row->func,
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

static void
function_cell_renderer_icon(GtkTreeViewColumn *column,
                            GtkCellRenderer *renderer,
                            GtkTreeModel *model,
                            GtkTreeIter *iter,
                            G_GNUC_UNUSED gpointer userdata)
{
    GtkWidget *widget;
    GdkPixbuf *pixbuf;
    GtkIconSet *iconset;
    GwyAppActionType type;
    gchar *name;
    const gchar *id;

    gtk_tree_model_get(model, iter, 0, &type, 1, &name, -1);

    id = find_default_stock_id(type, name);
    g_free(name);

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
function_cell_renderer_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
                            GtkCellRenderer *renderer,
                            GtkTreeModel *model,
                            GtkTreeIter *iter,
                            G_GNUC_UNUSED gpointer userdata)
{
    GwyAppActionType type;
    gchar *name;

    gtk_tree_model_get(model, iter, 0, &type, 1, &name, -1);
    if (type == GWY_APP_ACTION_TYPE_GROUP) {
        g_object_set(renderer,
                     "text", name,
                     "weight", PANGO_WEIGHT_BOLD,
                     "weight-set", TRUE,
                     NULL);
    }
    else {
        const gchar *s = action_get_nice_name(type, name);
        g_object_set(renderer,
                     "text", s,
                     "weight-set", FALSE,
                     "style-set", FALSE,
                     NULL);
    }
    g_free(name);
}

static gboolean
edit_group_dialogue(GwyToolboxEditor *editor,
                    GwyToolboxGroupSpec *gspec, guint i)
{
    GwyToolboxGroupEditor geditor;
    GtkWidget *label, *button;
    GtkTable *table;
    gint response, row;
    const gchar *id;

    geditor.spec = editor->spec;
    geditor.i = i;
    geditor.gspec = *gspec;
    geditor.gspec.item = NULL;
    geditor.gspec.name = g_strdup(gspec->name);
    id = gspec->id ? g_quark_to_string(gspec->id) : "";

    geditor.dialogue = gtk_dialog_new_with_buttons(_("Toolbox Editor"),
                                                   GTK_WINDOW(editor->dialogue),
                                                   GTK_DIALOG_MODAL,
                                                   GTK_STOCK_CANCEL,
                                                   GTK_RESPONSE_CANCEL,
                                                   GTK_STOCK_OK,
                                                   GTK_RESPONSE_OK,
                                                   NULL);

    table = GTK_TABLE(gtk_table_new(4, 3, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(geditor.dialogue)->vbox),
                       GTK_WIDGET(table), TRUE, TRUE, 0);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_Title:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    geditor.title = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(geditor.title), 24);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), geditor.title);
    gtk_entry_set_text(GTK_ENTRY(geditor.title), geditor.gspec.name);
    gtk_table_attach(table, geditor.title, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(geditor.title, "changed",
                             G_CALLBACK(group_title_changed), &geditor);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Id:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    geditor.id = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(geditor.id), 24);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), geditor.id);
    gtk_entry_set_text(GTK_ENTRY(geditor.id), id);
    gtk_table_attach(table, geditor.id, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(geditor.id, "changed",
                             G_CALLBACK(group_id_changed), &geditor);

    button = gtk_button_new_with_mnemonic(_("_Suggest"));
    gtk_table_attach(table, button, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(suggest_group_id), &geditor);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Trans_latable title"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 geditor.gspec.translatable);
    gtk_table_attach(table, button, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(group_title_transl_changed), &geditor);
    row++;

    gtk_table_set_row_spacing(table, row-1, 8);
    geditor.message = gtk_label_new(NULL);
    gtk_table_attach(table, geditor.message,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    group_id_changed(&geditor, GTK_ENTRY(geditor.id));
    gtk_widget_show_all(geditor.dialogue);
    response = gtk_dialog_run(GTK_DIALOG(geditor.dialogue));
    if (response == GTK_RESPONSE_OK) {
        /* XXX: The caller must ensure the corresponding GwyToolboxEditorRow
         * is updated! */
        GWY_SWAP(gchar*, gspec->name, geditor.gspec.name);
        gspec->translatable = geditor.gspec.translatable;
        id = gtk_entry_get_text(GTK_ENTRY(geditor.id));
        gspec->id = g_quark_from_string(id);
    }
    g_free(geditor.gspec.name);

    gtk_widget_destroy(geditor.dialogue);

    return (response == GTK_RESPONSE_OK);
}

static void
group_title_changed(GwyToolboxGroupEditor *geditor, GtkEntry *entry)
{
    g_free(geditor->gspec.name);
    geditor->gspec.name = g_strdup(gtk_entry_get_text(entry));
}

static void
group_id_changed(GwyToolboxGroupEditor *geditor, GtkEntry *entry)
{
    const GwyToolboxGroupSpec *gspec;
    const gchar *newid;
    GArray *group;
    const gchar *errmessage = NULL;
    guint i;

    newid = gtk_entry_get_text(entry);
    if (!gwy_strisident(newid, "_-", NULL))
        errmessage = _("Group id is not a valid identifier");

    if (!errmessage) {
        group = geditor->spec->group;
        for (i = 0; i < group->len; i++) {
            gspec = &g_array_index(group, GwyToolboxGroupSpec, i);
            if (i != geditor->i && gwy_strequal(g_quark_to_string(gspec->id),
                                                newid)) {
                errmessage = _("Duplicate group id");
                break;
            }
        }
    }

    if (errmessage) {
        GdkColor gdkcolor = { 0, 51118, 0, 0 };

        gtk_label_set_text(GTK_LABEL(geditor->message), errmessage);
        gtk_widget_modify_fg(geditor->message, GTK_STATE_NORMAL, &gdkcolor);
    }
    else {
        gtk_label_set_text(GTK_LABEL(geditor->message), "");
        gtk_widget_modify_fg(geditor->message, GTK_STATE_NORMAL, NULL);
    }
    gtk_dialog_set_response_sensitive(GTK_DIALOG(geditor->dialogue),
                                      GTK_RESPONSE_OK, !errmessage);
}

static void
suggest_group_id(GwyToolboxGroupEditor *geditor)
{
    GString *suggestion;
    gunichar *name_chars;
    glong name_len, i;

    name_chars = g_utf8_to_ucs4(geditor->gspec.name, -1, NULL, &name_len, NULL);
    if (!name_chars)
        return;

    suggestion = g_string_new(NULL);
#if GLIB_CHECK_VERSION(2, 30, 0)
    {
        guint decomp_len, j;
        gunichar buf[G_UNICHAR_MAX_DECOMPOSITION_LENGTH];

        for (i = 0; i < name_len; i++) {
            decomp_len = g_unichar_fully_decompose(name_chars[i], TRUE,
                                                   buf, G_N_ELEMENTS(buf));
            for (j = 0; j < decomp_len; j++) {
                if (buf[j] <= 0x20 || buf[j] >= 0x80)
                    continue;

                if (g_ascii_isalpha(buf[j])
                    || (suggestion->len && (buf[j] == '_'
                                            || buf[j] == '-'
                                            || g_ascii_isdigit(buf[j]))))
                    g_string_append_c(suggestion, g_ascii_tolower(buf[j]));
            }
        }
    }
#else
    for (i = 0; i < name_len; i++) {
        if (name_chars[i] <= 0x20 || name_chars[j] >= 0x80)
            continue;

        if (g_ascii_isalpha(name_chars[i])
            || (suggestion->len && (name_chars[i] == '_'
                                    || name_chars[i] == '-'
                                    || g_ascii_isdigit(name_chars[i]))))
            g_string_append_c(suggestion, g_ascii_tolower(name_chars[i]));
    }
#endif

    gtk_entry_set_text(GTK_ENTRY(geditor->id), suggestion->str);
    g_string_free(suggestion, TRUE);
    g_free(name_chars);
}

static void
group_title_transl_changed(GwyToolboxGroupEditor *geditor,
                           GtkToggleButton *toggle)
{
    geditor->gspec.translatable = gtk_toggle_button_get_active(toggle);
}

static guint
toolbox_model_iter_indices(GtkTreeModel *model, GtkTreeIter *iter,
                           guint *i, guint *j)
{
    GtkTreePath *path;
    gint depth;
    gint *indices;

    path = gtk_tree_model_get_path(model, iter);
    depth = gtk_tree_path_get_depth(path);
    indices = gtk_tree_path_get_indices(path);
    if (depth == 1) {
        *i = indices[0];
        *j = G_MAXUINT;
    }
    else if (depth == 2) {
        *i = indices[0];
        *j = indices[1];
    }
    else {
        g_return_val_if_reached(0);
    }

    gtk_tree_path_free(path);

    return depth;
}

static gboolean
tree_model_iter_is_first(GtkTreeModel *model, GtkTreeIter *iter)
{
    GtkTreePath *path;
    gint depth;
    gint *indices;
    gboolean is_first;

    path = gtk_tree_model_get_path(model, iter);
    depth = gtk_tree_path_get_depth(path);
    g_return_val_if_fail(depth, TRUE);
    indices = gtk_tree_path_get_indices(path);
    is_first = (indices[depth-1] == 0);
    gtk_tree_path_free(path);

    return is_first;
}

static gboolean
tree_model_iter_is_last(GtkTreeModel *model, GtkTreeIter *iter)
{
    GtkTreeIter myiter = *iter;

    return !gtk_tree_model_iter_next(model, &myiter);
}

/* TODO: Must update sensitivity also whenever we add/remove/move items! */
static void
toolbox_selection_changed(GtkTreeSelection *selection,
                          GwyToolboxEditor *editor)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_widget_set_sensitive(editor->remove, FALSE);
        gtk_widget_set_sensitive(editor->edit, FALSE);
        gtk_widget_set_sensitive(editor->move_up, FALSE);
        gtk_widget_set_sensitive(editor->move_down, FALSE);
        return;
    }

    gtk_widget_set_sensitive(editor->remove, TRUE);
    gtk_widget_set_sensitive(editor->edit, TRUE);
    gtk_widget_set_sensitive(editor->move_up,
                             !tree_model_iter_is_first(model, &iter));
    gtk_widget_set_sensitive(editor->move_down,
                             !tree_model_iter_is_last(model, &iter));
}

static void
add_toolbox_item(GwyToolboxEditor *editor)
{
    g_warning("Implement me!");
}

static void
add_toolbox_group(GwyToolboxEditor *editor)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    guint i, j;
    GwyToolboxGroupSpec gspec;
    GwyToolboxEditorRow *row;

    selection = gtk_tree_view_get_selection(editor->toolbox_view);
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        toolbox_model_iter_indices(model, &iter, &i, &j);
        i++;
    }
    else
        i = 0;

    gspec.name = g_strdup("");
    gspec.id = 0;
    gspec.translatable = FALSE;
    if (!edit_group_dialogue(editor, &gspec, G_MAXUINT)) {
        g_free(gspec.name);
        return;
    }

    gwy_toolbox_spec_add_group(editor->spec, &gspec, i);
    row = g_slice_new0(GwyToolboxEditorRow);
    fill_toolbox_row_for_group(row, &gspec);
    gtk_tree_store_insert_with_values(editor->toolbox_model,
                                      &iter, NULL, i, 0, row, -1);
}

static void
edit_item_or_group(GwyToolboxEditor *editor)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreePath *path;
    guint depth, i, j;
    GwyToolboxGroupSpec *gspec;
    GwyToolboxEditorRow *row;

    selection = gtk_tree_view_get_selection(editor->toolbox_view);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    depth = toolbox_model_iter_indices(model, &iter, &i, &j);
    gtk_tree_model_get(model, &iter, 0, &row, -1);
    if (depth == 1) {
        gspec = &g_array_index(editor->spec->group, GwyToolboxGroupSpec, i);
        if (edit_group_dialogue(editor, gspec, i)) {
            fill_toolbox_row_for_group(row, gspec);
            path = gtk_tree_model_get_path(model, &iter);
            gtk_tree_model_row_changed(model, path, &iter);
            gtk_tree_path_free(path);
        }
    }
    else if (depth == 2) {
        g_warning("Implement me!");
    }
}

static void
remove_from_toolbox(GwyToolboxEditor *editor)
{
    GwyToolboxEditorRow *row;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    guint i, j, depth;

    selection = gtk_tree_view_get_selection(editor->toolbox_view);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    depth = toolbox_model_iter_indices(model, &iter, &i, &j);
    if (depth == 1)
        gwy_toolbox_spec_remove_group(editor->spec, i);
    else if (depth == 2)
        gwy_toolbox_spec_remove_item(editor->spec, i, j);
    else
        g_return_if_reached();

    gtk_tree_model_get(model, &iter, 0, &row, -1);
    gtk_tree_store_remove(editor->toolbox_model, &iter);
    g_slice_free(GwyToolboxEditorRow, row);
}

static void
move_up_in_toolbox(GwyToolboxEditor *editor)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter, otheriter, parent;
    guint i, j, depth;

    selection = gtk_tree_view_get_selection(editor->toolbox_view);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    depth = toolbox_model_iter_indices(model, &iter, &i, &j);
    if (depth == 1) {
        gwy_toolbox_spec_move_group(editor->spec, i, TRUE);
        gtk_tree_model_iter_nth_child(model, &otheriter, NULL, i-1);
        gtk_tree_store_swap(editor->toolbox_model, &iter, &otheriter);
    }
    else if (depth == 2) {
        gwy_toolbox_spec_move_item(editor->spec, i, j, TRUE);
        gtk_tree_model_iter_parent(model, &parent, &iter);
        gtk_tree_model_iter_nth_child(model, &otheriter, &parent, j-1);
        gtk_tree_store_swap(editor->toolbox_model, &iter, &otheriter);
    }
    else
        g_return_if_reached();
}

static void
move_down_in_toolbox(GwyToolboxEditor *editor)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter, otheriter;
    guint i, j, depth;

    selection = gtk_tree_view_get_selection(editor->toolbox_view);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    depth = toolbox_model_iter_indices(model, &iter, &i, &j);
    if (depth == 1)
        gwy_toolbox_spec_move_group(editor->spec, i, FALSE);
    else if (depth == 2)
        gwy_toolbox_spec_move_item(editor->spec, i, j, FALSE);
    else
        g_return_if_reached();

    otheriter = iter;
    gtk_tree_model_iter_next(model, &otheriter);
    gtk_tree_store_swap(editor->toolbox_model, &iter, &otheriter);
}

static void
width_changed(GwyToolboxEditor *editor)
{
    editor->spec->width = gwy_adjustment_get_int(editor->width);
}

static void
apply_toolbox_spec(GwyToolboxEditor *editor)
{
    GwyToolboxSpec *spec;

    /* We work on a copy.  But invoking gwy_toolbox_rebuild_to_spec() also
     * makes the passed spec the actual primary spec.  So pass a copy. */
    spec = gwy_toolbox_spec_duplicate(editor->spec);
    gwy_toolbox_rebuild_to_spec(spec);
}

/* Copied from menu.c */
static void
gwy_app_menu_canonicalize_label(gchar *label)
{
    guint i, j;

    for (i = j = 0; label[i]; i++) {
        label[j] = label[i];
        if (label[i] != '_' || label[i+1] == '_')
            j++;
    }
    /* If the label *ends* with an underscore, just kill it */
    label[j] = '\0';
    if (j >= 3 && label[j-3] == '.' && label[j-2] == '.' && label[j-1] == '.')
        label[j-3] = '\0';
}

static const gchar*
action_get_nice_name(GwyAppActionType type, const gchar *name)
{
    static GString *label = NULL;

    const gchar *menupath = NULL;

    if (type == GWY_APP_ACTION_TYPE_TOOL) {
        GType gtype = g_type_from_name(name);
        GwyToolClass *tool_class = g_type_class_peek(gtype);
        return gwy_tool_class_get_title(tool_class);
    }
    if (type == GWY_APP_ACTION_TYPE_BUILTIN) {
        const GwyToolboxBuiltinSpec* spec;

        if ((spec = gwy_toolbox_find_builtin_spec(name)))
            return spec->nice_name;
    }

    if (type == GWY_APP_ACTION_TYPE_PROC)
        menupath = gwy_process_func_get_menu_path(name);
    else if (type == GWY_APP_ACTION_TYPE_GRAPH)
        menupath = gwy_graph_func_get_menu_path(name);
    else if (type == GWY_APP_ACTION_TYPE_VOLUME)
        menupath = gwy_volume_func_get_menu_path(name);
    else if (type == GWY_APP_ACTION_TYPE_XYZ)
        menupath = gwy_xyz_func_get_menu_path(name);

    if (menupath) {
        const gchar *p;
        gchar *s = g_strdup(menupath);

        if (!label)
            label = g_string_new(NULL);

        gwy_app_menu_canonicalize_label(s);
        p = strrchr(s, '/');
        g_string_assign(label, p ? p+1 : s);
        g_free(s);

        return label->str;
    }

    return NULL;
}

static const gchar*
find_default_stock_id(GwyAppActionType type, const gchar *name)
{
    if (type == GWY_APP_ACTION_TYPE_PROC)
        return gwy_process_func_get_stock_id(name);
    if (type == GWY_APP_ACTION_TYPE_GRAPH)
        return gwy_graph_func_get_stock_id(name);
    if (type == GWY_APP_ACTION_TYPE_VOLUME)
        return gwy_volume_func_get_stock_id(name);
    if (type == GWY_APP_ACTION_TYPE_XYZ)
        return gwy_xyz_func_get_stock_id(name);
    if (type == GWY_APP_ACTION_TYPE_TOOL && name) {
        GType gtype = g_type_from_name(name);
        GwyToolClass *tool_class = g_type_class_peek(gtype);
        return gwy_tool_class_get_stock_id(tool_class);
    }
    if (type == GWY_APP_ACTION_TYPE_BUILTIN) {
        const GwyToolboxBuiltinSpec* spec;

        if ((spec = gwy_toolbox_find_builtin_spec(name)))
            return spec->stock_id;
    }
    return NULL;
}

static GtkTreeModel*
create_icon_list(GtkWidget *widget, const gchar **stock_ids, guint nicons)
{
    GtkListStore *store;
    GdkPixbuf *pixbuf;
    GtkIconSet *iconset;
    GtkTreeIter iter;
    guint i;

    store = gtk_list_store_new(2, G_TYPE_UINT, GDK_TYPE_PIXBUF);
    for (i = 0; i < nicons; i++) {
        iconset = gtk_icon_factory_lookup_default(stock_ids[i]);
        if (!iconset)
            continue;

        pixbuf = gtk_icon_set_render_icon(iconset, widget->style,
                                          GTK_TEXT_DIR_NONE,
                                          GTK_STATE_NORMAL,
                                          GTK_ICON_SIZE_LARGE_TOOLBAR,
                                          widget, NULL);
        if (!pixbuf)
            continue;

        gtk_list_store_insert_with_values(store, &iter, G_MAXINT,
                                          0, g_quark_from_string(stock_ids[i]),
                                          1, pixbuf,
                                          -1);
    }

    return GTK_TREE_MODEL(store);
}

static GtkTreeModel*
create_gtk_icon_list(GtkWidget *widget)
{
    /* We need to use the names directly, not symbols, because the list
     * includes icons that may not be present in Gtk+ we compile Gwyddion
     * with. */
    static const gchar *stock_icon_names[] = { /* {{{ */
        "gtk-about",
        "gtk-add",
        "gtk-apply",
        "gtk-bold",
        "gtk-cancel",
        "gtk-caps-lock-warning",
        "gtk-cdrom",
        "gtk-clear",
        "gtk-close",
        "gtk-color-picker",
        "gtk-connect",
        "gtk-convert",
        "gtk-copy",
        "gtk-cut",
        "gtk-delete",
        "gtk-dialog-authentication",
        "gtk-dialog-info",
        "gtk-dialog-warning",
        "gtk-dialog-error",
        "gtk-dialog-question",
        "gtk-directory",
        "gtk-discard",
        "gtk-disconnect",
        "gtk-dnd",
        "gtk-dnd-multiple",
        "gtk-edit",
        "gtk-execute",
        "gtk-file",
        "gtk-find",
        "gtk-find-and-replace",
        "gtk-floppy",
        "gtk-fullscreen",
        "gtk-goto-bottom",
        //"gtk-goto-first",
        //"gtk-goto-last",
        "gtk-goto-top",
        //"gtk-go-back",
        "gtk-go-down",
        //"gtk-go-forward",
        "gtk-go-up",
        "gtk-harddisk",
        "gtk-help",
        "gtk-home",
        "gtk-index",
        //"gtk-indent",
        "gtk-info",
        "gtk-italic",
        //"gtk-jump-to",
        "gtk-justify-center",
        "gtk-justify-fill",
        "gtk-justify-left",
        "gtk-justify-right",
        "gtk-leave-fullscreen",
        "gtk-missing-image",
        //"gtk-media-forward",
        //"gtk-media-next",
        //"gtk-media-pause",
        //"gtk-media-play",
        //"gtk-media-previous",
        //"gtk-media-record",
        //"gtk-media-rewind",
        //"gtk-media-stop",
        "gtk-network",
        "gtk-new",
        "gtk-no",
        "gtk-ok",
        "gtk-open",
        "gtk-orientation-portrait",
        "gtk-orientation-landscape",
        "gtk-orientation-reverse-landscape",
        "gtk-orientation-reverse-portrait",
        "gtk-page-setup",
        "gtk-paste",
        "gtk-preferences",
        "gtk-print",
        "gtk-print-error",
        "gtk-print-paused",
        "gtk-print-preview",
        "gtk-print-report",
        "gtk-print-warning",
        "gtk-properties",
        "gtk-quit",
        //"gtk-redo",
        "gtk-refresh",
        "gtk-remove",
        //"gtk-revert-to-saved",
        "gtk-save",
        "gtk-save-as",
        "gtk-select-all",
        "gtk-select-color",
        "gtk-select-font",
        "gtk-sort-ascending",
        "gtk-sort-descending",
        "gtk-spell-check",
        "gtk-stop",
        "gtk-strikethrough",
        //"gtk-undelete",
        "gtk-underline",
        //"gtk-undo",
        //"gtk-unindent",
        "gtk-yes",
        "gtk-zoom-100",
        "gtk-zoom-fit",
        "gtk-zoom-in",
        "gtk-zoom-out",
    }; /* }}} */

    return create_icon_list(widget,
                            stock_icon_names, G_N_ELEMENTS(stock_icon_names));
}

static GtkTreeModel*
create_gwy_icon_list(GtkWidget *widget)
{
    /* The following generated part is updated by running utils/stockgen.py */
    static const gchar *stock_icon_names[] = { /* {{{ */
/* @@@ GENERATED STOCK LIST BEGIN @@@ */
    GWY_STOCK_3D_BASE,
    GWY_STOCK_ARITHMETIC,
    GWY_STOCK_BOLD,
    GWY_STOCK_CANTILEVER,
    GWY_STOCK_COLOR_RANGE,
    GWY_STOCK_COLOR_RANGE_ADAPTIVE,
    GWY_STOCK_COLOR_RANGE_AUTO,
    GWY_STOCK_COLOR_RANGE_FIXED,
    GWY_STOCK_COLOR_RANGE_FULL,
    GWY_STOCK_CONVOLUTION,
    GWY_STOCK_CORRECT_AFFINE,
    GWY_STOCK_CROP,
    GWY_STOCK_CWT,
    GWY_STOCK_DATA_MEASURE,
    GWY_STOCK_DISTANCE,
    GWY_STOCK_DISTANCE_TRANSFORM,
    GWY_STOCK_DISTRIBUTION_ANGLE,
    GWY_STOCK_DISTRIBUTION_SLOPE,
    GWY_STOCK_DRIFT,
    GWY_STOCK_DWT,
    GWY_STOCK_EDGE,
    GWY_STOCK_ENFORCE_DISTRIBUTION,
    GWY_STOCK_ENTROPY,
    GWY_STOCK_EXTEND,
    GWY_STOCK_EXTRACT_PATH,
    GWY_STOCK_FACET_LEVEL,
    GWY_STOCK_FAVOURITE,
    GWY_STOCK_FFT,
    GWY_STOCK_FFT_2D,
    GWY_STOCK_FFT_FILTER_2D,
    GWY_STOCK_FILTER,
    GWY_STOCK_FIND_PEAKS,
    GWY_STOCK_FIX_ZERO,
    GWY_STOCK_FLIP_HORIZONTALLY,
    GWY_STOCK_FLIP_VERTICALLY,
    GWY_STOCK_FRACTAL,
    GWY_STOCK_GL_MATERIAL,
    GWY_STOCK_GRADIENT_HORIZONTAL,
    GWY_STOCK_GRADIENT_VERTICAL,
    GWY_STOCK_GRAINS,
    GWY_STOCK_GRAINS_EDGE,
    GWY_STOCK_GRAINS_EDGE_REMOVE,
    GWY_STOCK_GRAINS_GRAPH,
    GWY_STOCK_GRAINS_MEASURE,
    GWY_STOCK_GRAINS_REMOVE,
    GWY_STOCK_GRAINS_WATER,
    GWY_STOCK_GRAIN_CORRELATION,
    GWY_STOCK_GRAIN_EXSCRIBED_CIRCLE,
    GWY_STOCK_GRAIN_INSCRIBED_CIRCLE,
    GWY_STOCK_GRAPH,
    GWY_STOCK_GRAPH_ALIGN,
    GWY_STOCK_GRAPH_CUT,
    GWY_STOCK_GRAPH_DOS,
    GWY_STOCK_GRAPH_EXPORT_ASCII,
    GWY_STOCK_GRAPH_EXPORT_PNG,
    GWY_STOCK_GRAPH_EXPORT_VECTOR,
    GWY_STOCK_GRAPH_FD,
    GWY_STOCK_GRAPH_FILTER,
    GWY_STOCK_GRAPH_FUNCTION,
    GWY_STOCK_GRAPH_HALFGAUSS,
    GWY_STOCK_GRAPH_LEVEL,
    GWY_STOCK_GRAPH_MEASURE,
    GWY_STOCK_GRAPH_PALETTE,
    GWY_STOCK_GRAPH_POINTER,
    GWY_STOCK_GRAPH_RULER,
    GWY_STOCK_GRAPH_VERTICAL,
    GWY_STOCK_GRAPH_ZOOM_FIT,
    GWY_STOCK_GRAPH_ZOOM_IN,
    GWY_STOCK_GRAPH_ZOOM_OUT,
    GWY_STOCK_GWYDDION,
    GWY_STOCK_HOUGH,
    GWY_STOCK_IMMERSE,
    GWY_STOCK_ISO_ROUGHNESS,
    GWY_STOCK_ITALIC,
    GWY_STOCK_LESS,
    GWY_STOCK_LEVEL,
    GWY_STOCK_LEVEL_FLATTEN_BASE,
    GWY_STOCK_LEVEL_MEDIAN,
    GWY_STOCK_LEVEL_TRIANGLE,
    GWY_STOCK_LIGHT_ROTATE,
    GWY_STOCK_LINE_LEVEL,
    GWY_STOCK_LOAD_DEBUG,
    GWY_STOCK_LOAD_INFO,
    GWY_STOCK_LOAD_WARNING,
    GWY_STOCK_LOCAL_SLOPE,
    GWY_STOCK_LOGSCALE_HORIZONTAL,
    GWY_STOCK_LOGSCALE_VERTICAL,
    GWY_STOCK_MARK_WITH,
    GWY_STOCK_MASK,
    GWY_STOCK_MASK_ADD,
    GWY_STOCK_MASK_CIRCLE,
    GWY_STOCK_MASK_CIRCLE_EXCLUSIVE,
    GWY_STOCK_MASK_CIRCLE_INCLUSIVE,
    GWY_STOCK_MASK_DISTRIBUTE,
    GWY_STOCK_MASK_EDITOR,
    GWY_STOCK_MASK_EXCLUDE,
    GWY_STOCK_MASK_EXCLUDE_CIRCLE,
    GWY_STOCK_MASK_EXTRACT,
    GWY_STOCK_MASK_FILL_DRAW,
    GWY_STOCK_MASK_FILL_ERASE,
    GWY_STOCK_MASK_GROW,
    GWY_STOCK_MASK_INTERSECT,
    GWY_STOCK_MASK_INVERT,
    GWY_STOCK_MASK_LINE,
    GWY_STOCK_MASK_MORPH,
    GWY_STOCK_MASK_PAINT_DRAW,
    GWY_STOCK_MASK_PAINT_ERASE,
    GWY_STOCK_MASK_RECT_EXCLUSIVE,
    GWY_STOCK_MASK_RECT_INCLUSIVE,
    GWY_STOCK_MASK_REMOVE,
    GWY_STOCK_MASK_SHRINK,
    GWY_STOCK_MASK_SUBTRACT,
    GWY_STOCK_MASK_THIN,
    GWY_STOCK_MEASURE_LATTICE,
    GWY_STOCK_MERGE,
    GWY_STOCK_MORE,
    GWY_STOCK_MUTUAL_CROP,
    GWY_STOCK_NEURAL_APPLY,
    GWY_STOCK_NEURAL_TRAIN,
    GWY_STOCK_PALETTES,
    GWY_STOCK_PATH_LEVEL,
    GWY_STOCK_POINTER_MEASURE,
    GWY_STOCK_POLYNOM,
    GWY_STOCK_POLYNOM_LEVEL,
    GWY_STOCK_POLY_DISTORT,
    GWY_STOCK_PROFILE,
    GWY_STOCK_PSDF_LOG_PHI,
    GWY_STOCK_PSDF_SECTION,
    GWY_STOCK_PYGWY,
    GWY_STOCK_REMOVE_UNDER_MASK,
    GWY_STOCK_ROTATE,
    GWY_STOCK_ROTATE_180,
    GWY_STOCK_ROTATE_90_CCW,
    GWY_STOCK_ROTATE_90_CW,
    GWY_STOCK_SCALE,
    GWY_STOCK_SCALE_HORIZONTALLY,
    GWY_STOCK_SCALE_VERTICALLY,
    GWY_STOCK_SCARS,
    GWY_STOCK_SELECTIONS,
    GWY_STOCK_SHADER,
    GWY_STOCK_SPECTRUM,
    GWY_STOCK_SPOT_REMOVE,
    GWY_STOCK_STAT_QUANTITIES,
    GWY_STOCK_STRAIGHTEN_PATH,
    GWY_STOCK_SUBSCRIPT,
    GWY_STOCK_SUPERSCRIPT,
    GWY_STOCK_SYNTHETIC_BALLISTIC_DEPOSITION,
    GWY_STOCK_SYNTHETIC_BROWNIAN_MOTION,
    GWY_STOCK_SYNTHETIC_COLUMNAR,
    GWY_STOCK_SYNTHETIC_DIFFUSION,
    GWY_STOCK_SYNTHETIC_DOMAINS,
    GWY_STOCK_SYNTHETIC_LATTICE,
    GWY_STOCK_SYNTHETIC_LINE_NOISE,
    GWY_STOCK_SYNTHETIC_NOISE,
    GWY_STOCK_SYNTHETIC_OBJECTS,
    GWY_STOCK_SYNTHETIC_PARTICLES,
    GWY_STOCK_SYNTHETIC_PATTERN,
    GWY_STOCK_SYNTHETIC_SPECTRAL,
    GWY_STOCK_SYNTHETIC_WAVES,
    GWY_STOCK_TILT,
    GWY_STOCK_TIP_DILATION,
    GWY_STOCK_TIP_EROSION,
    GWY_STOCK_TIP_ESTIMATION,
    GWY_STOCK_TIP_INDENT_ANALYZE,
    GWY_STOCK_TIP_LATERAL_FORCE,
    GWY_STOCK_TIP_MAP,
    GWY_STOCK_TIP_MODEL,
    GWY_STOCK_TIP_PID,
    GWY_STOCK_UNROTATE,
    GWY_STOCK_VALUE_INVERT,
    GWY_STOCK_VOLUME,
    GWY_STOCK_VOLUME_CALIBRATE,
    GWY_STOCK_VOLUME_DIMENSIONS,
    GWY_STOCK_VOLUME_FD,
    GWY_STOCK_VOLUME_INVERT,
    GWY_STOCK_VOLUME_KMEANS,
    GWY_STOCK_VOLUME_KMEDIANS,
    GWY_STOCK_VOLUME_SLICE,
    GWY_STOCK_VOLUMIZE,
    GWY_STOCK_VOLUMIZE_LAYERS,
    GWY_STOCK_ZERO_MEAN,
    GWY_STOCK_ZOOM_1_1,
    GWY_STOCK_ZOOM_FIT,
    GWY_STOCK_ZOOM_IN,
    GWY_STOCK_ZOOM_OUT,
/* @@@ GENERATED STOCK LIST END @@@ */
    }; /* }}} */

    return create_icon_list(widget,
                            stock_icon_names, G_N_ELEMENTS(stock_icon_names));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
