/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <stdlib.h>

#include <libgwyddion/gwyddion.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include "gwyappinternal.h"

/* Set on GwyContainer to remember its graph list */
#define CONTROLS_KEY "graph-list-controls"
/* Set on GwyGraphModel to remember its key in container */
#define GMODEL_ID_KEY "graph-list-model-id"
/* Set on GwyGraphWindow to remember its (persistent!) iter in list */
#define ITER_KEY "graph-list-iter"
/* Temporary set on GwyGraphModel to graph window showing it */
#define GMODEL_GRAPH_KEY "graph-list-graph-pointer"

enum {
    GRAPHLIST_GMODEL,
    GRAPHLIST_GRAPH,
    GRAPHLIST_TITLE,
    GRAPHLIST_NCURVES,
    GRAPHLIST_LAST
};

typedef struct {
    GtkWidget *window;
    GtkWidget *delete;
    GtkWidget *delete_all;
    GtkWidget *show_all;
    GtkWidget *hide_all;
    GtkWidget *list;
    GwyContainer *data;
} Controls;

static GtkWidget* gwy_app_graph_list_construct       (Controls *controls);
static void       gwy_app_graph_list_cell_renderer   (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *cell,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *piter,
                                                      gpointer userdata);
static void       gwy_app_graph_list_row_inserted    (GtkTreeModel *model,
                                                      GtkTreePath *path,
                                                      GtkTreeIter *iter,
                                                      Controls *controls);
static void       gwy_app_graph_list_row_deleted     (GtkTreeModel *model,
                                                      GtkTreePath *path,
                                                      Controls *controls);
static void       gwy_app_graph_list_selection_changed(GtkTreeSelection *selection,
                                                      Controls *controls);
static void       gwy_app_graph_list_toggled         (GtkCellRendererToggle *toggle,
                                                      gchar *pathstring,
                                                      Controls *controls);
static gboolean   gwy_app_graph_list_hide_graph      (GtkTreeModel *model,
                                                      GtkTreePath *path,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static gboolean   gwy_app_graph_list_show_graph      (GtkTreeModel *model,
                                                      GtkTreePath *path,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static void       gwy_app_graph_list_delete_graph    (GtkTreeModel *model,
                                                      GtkTreeIter *iter,
                                                      Controls *controls);
static void       gwy_app_graph_list_hide_all        (Controls *controls);
static void       gwy_app_graph_list_show_all        (Controls *controls);
static void       gwy_app_graph_list_delete_all      (Controls *controls);
static void       gwy_app_graph_list_delete_one      (Controls *controls);
static void       gwy_app_graph_list_add_line        (gpointer hkey,
                                                      GValue *value,
                                                      Controls *controls);
static gint       gwy_app_graph_list_sort_func       (GtkTreeModel *model,
                                                      GtkTreeIter *a,
                                                      GtkTreeIter *b,
                                                      gpointer user_data);
static void       gwy_app_graph_list_graph_destroy   (Controls *controls,
                                                      GwyGraphWindow *window);
static void       gwy_app_graph_list_destroy         (Controls *controls);

void
gwy_app_graph_list_add(GwyContainer *data,
                       GwyGraphModel *gmodel,
                       GwyGraphWindow *graph)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    Controls *controls;
    gint32 lastid = 0, id;

    gwy_debug("have graph window: %d", graph != NULL);
    g_return_if_fail(GWY_IS_CONTAINER(data));
    g_return_if_fail(GWY_IS_GRAPH_MODEL(gmodel));
    g_return_if_fail(!graph || GWY_IS_GRAPH_WINDOW(graph));
    /* TODO: paranoid check whether graph's model is gmodel */

    /* We can be called from gwy_app_graph_window_create() when graph is
     * re-created for already known model.   Do nothing then. */
    id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gmodel), GMODEL_ID_KEY));
    if (!id) {
        gchar key[32];

        /* Compute new id and new id list */
        if (gwy_container_gis_int32_by_name(data, "/0/graph/lastid", &lastid)
            && lastid <= 0)
            g_warning("Broken last graph id");
        lastid = MAX(0, lastid) + 1;
        gwy_debug("New graph, got id: %d", lastid);

        g_snprintf(key, sizeof(key), "/0/graph/graph/%d", lastid);
        g_object_set_data(G_OBJECT(gmodel), GMODEL_ID_KEY,
                          GINT_TO_POINTER(lastid));
        gwy_container_set_int32_by_name(data, "/0/graph/lastid", lastid);
        gwy_container_set_object_by_name(data, key, gmodel);
        g_object_unref(gmodel);
    }

    /* Until the graph list is first time invoked, don't bother with its
     * actual construction */
    if (!(controls = g_object_get_data(G_OBJECT(data), CONTROLS_KEY))
        || !controls->list)
        return;

    /* If the graph model is new, add it to the store */
    if (!id) {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->list));
        gtk_list_store_append(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           GRAPHLIST_GMODEL, gmodel,
                           GRAPHLIST_GRAPH, graph,
                           -1);
    }

    /* If there is a graph window, watch it for desctruction and toggle */
    if (graph)
        g_signal_connect_swapped(graph, "destroy",
                                 G_CALLBACK(gwy_app_graph_list_graph_destroy),
                                 controls);

    if (!id && graph)
        g_object_set_data(G_OBJECT(graph), ITER_KEY, gtk_tree_iter_copy(&iter));
}

GtkWidget*
gwy_app_graph_list_new(GwyDataWindow *data_window)
{
    GtkWidget *window, *vbox, *buttonbox, *list, *scroll;
    GwyContainer *data;
    Controls *controls;
    gchar *t, *title;

    gwy_debug("data_window: %p", data_window);
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);

    data = gwy_data_window_get_data(data_window);
    g_object_ref(data);
    controls = g_object_get_data(G_OBJECT(data), CONTROLS_KEY);
    g_return_val_if_fail(!controls, controls->window);

    t = gwy_data_window_get_base_name(data_window);
    title = g_strdup_printf(_("Graph List for %s"), t);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), title);
    gtk_window_set_default_size(GTK_WINDOW(window), -1, 180);
    g_free(title);
    g_free(t);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    controls = g_new(Controls, 1);
    g_object_ref(data);
    controls->data = data;
    g_object_set_data(G_OBJECT(data), CONTROLS_KEY, controls);

    list = gwy_app_graph_list_construct(controls);
    g_signal_connect_swapped(list, "destroy",
                             G_CALLBACK(gwy_app_graph_list_destroy), controls);
    gtk_container_add(GTK_CONTAINER(scroll), list);

    buttonbox = gtk_hbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(buttonbox), 2);
    gtk_box_pack_start(GTK_BOX(vbox), buttonbox, FALSE, FALSE, 0);

    controls->delete = gtk_button_new_with_mnemonic(_("_Delete"));
    gtk_box_pack_start(GTK_BOX(buttonbox), controls->delete, TRUE, TRUE, 0);
    g_signal_connect_swapped(controls->delete, "clicked",
                             G_CALLBACK(gwy_app_graph_list_delete_one),
                             controls);

    controls->delete_all = gtk_button_new_with_mnemonic(_("Delete _All"));
    gtk_box_pack_start(GTK_BOX(buttonbox), controls->delete_all, TRUE, TRUE, 0);
    g_signal_connect_swapped(controls->delete_all, "clicked",
                             G_CALLBACK(gwy_app_graph_list_delete_all),
                             controls);

    controls->show_all = gtk_button_new_with_mnemonic(_("_Show All"));
    gtk_box_pack_start(GTK_BOX(buttonbox), controls->show_all, TRUE, TRUE, 0);
    g_signal_connect_swapped(controls->show_all, "clicked",
                             G_CALLBACK(gwy_app_graph_list_show_all),
                             controls);

    controls->hide_all = gtk_button_new_with_mnemonic(_("_Hide All"));
    gtk_box_pack_start(GTK_BOX(buttonbox), controls->hide_all, TRUE, TRUE, 0);
    g_signal_connect_swapped(controls->hide_all, "clicked",
                             G_CALLBACK(gwy_app_graph_list_hide_all),
                             controls);

    g_signal_connect_swapped(data_window, "destroy",
                             G_CALLBACK(gtk_widget_destroy), window);
    gtk_widget_show_all(vbox);

    return window;
}

static GtkWidget*
gwy_app_graph_list_construct(Controls *controls)
{
    GtkWidget *list;
    GtkListStore *store;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    /* Graph model (permanent), Graph (if visible) */
    store = gtk_list_store_new(2, G_TYPE_OBJECT, G_TYPE_POINTER);
    g_assert(gtk_tree_model_get_flags(GTK_TREE_MODEL(store))
             & GTK_TREE_MODEL_ITERS_PERSIST);
    gwy_debug_objects_creation(G_OBJECT(store));
    list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    controls->list = list;
    g_object_unref(store);
    gwy_container_foreach(controls->data, "/0/graph/graph",
                          (GHFunc)(gwy_app_graph_list_add_line), controls);

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
                                    0, gwy_app_graph_list_sort_func,
                                    NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), 0,
                                         GTK_SORT_ASCENDING);

    /* Graph (toggle) column */
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(G_OBJECT(renderer), "activatable", TRUE, NULL);
    g_signal_connect(renderer, "toggled",
                     G_CALLBACK(gwy_app_graph_list_toggled), controls);
    column = gtk_tree_view_column_new_with_attributes(_("Vis."),
                                                      renderer, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            gwy_app_graph_list_cell_renderer,
                                            GUINT_TO_POINTER(GRAPHLIST_GRAPH),
                                            NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

    /* Title column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Title"),
                                                      renderer, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            gwy_app_graph_list_cell_renderer,
                                            GUINT_TO_POINTER(GRAPHLIST_TITLE),
                                            NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

    /* N curves column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes
                                              (gwy_sgettext("graphlist|Curves"),
                                               renderer, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            gwy_app_graph_list_cell_renderer,
                                            GUINT_TO_POINTER(GRAPHLIST_NCURVES),
                                            NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_app_graph_list_selection_changed),
                     controls);
    g_signal_connect(store, "row-deleted",
                     G_CALLBACK(gwy_app_graph_list_row_deleted), controls);
    g_signal_connect(store, "row-inserted",
                     G_CALLBACK(gwy_app_graph_list_row_inserted), controls);

    return list;
}

static void
gwy_app_graph_list_row_inserted(G_GNUC_UNUSED GtkTreeModel *store,
                                G_GNUC_UNUSED GtkTreePath *path,
                                G_GNUC_UNUSED GtkTreeIter *iter,
                                Controls *controls)
{
    GtkTreeSelection *selection;

    gtk_widget_set_sensitive(controls->delete_all, TRUE);
    gtk_widget_set_sensitive(controls->show_all, TRUE);
    gtk_widget_set_sensitive(controls->hide_all, TRUE);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->list));
    gtk_widget_set_sensitive(controls->delete,
                             gtk_tree_selection_get_selected(selection,
                                                             NULL, NULL));
}

static void
gwy_app_graph_list_row_deleted(GtkTreeModel *store,
                               G_GNUC_UNUSED GtkTreePath *path,
                               Controls *controls)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    gboolean has_rows;

    has_rows = gtk_tree_model_get_iter_first(store, &iter);
    gtk_widget_set_sensitive(controls->delete_all, has_rows);
    gtk_widget_set_sensitive(controls->show_all, has_rows);
    gtk_widget_set_sensitive(controls->hide_all, has_rows);
    if (has_rows) {
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->list));
        gtk_widget_set_sensitive(controls->delete,
                                 gtk_tree_selection_get_selected(selection,
                                                                 NULL, NULL));
    }
    else
        gtk_widget_set_sensitive(controls->hide_all, has_rows);
}

static void
gwy_app_graph_list_selection_changed(GtkTreeSelection *selection,
                                     Controls *controls)
{
    gtk_widget_set_sensitive(controls->delete,
                             gtk_tree_selection_get_selected(selection,
                                                             NULL, NULL));
}

static void
gwy_app_graph_list_toggled(GtkCellRendererToggle *toggle,
                           gchar *pathstring,
                           Controls *controls)
{
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    gboolean active;

    active = gtk_cell_renderer_toggle_get_active(toggle);
    gwy_debug("toggle: %d", active);
    path = gtk_tree_path_new_from_string(pathstring);
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->list));
    gtk_tree_model_get_iter(model, &iter, path);
    if (active)
        gwy_app_graph_list_hide_graph(model, path, &iter, controls);
    else
        gwy_app_graph_list_show_graph(model, path, &iter, controls);
    gtk_tree_path_free(path);
}

static gboolean
gwy_app_graph_list_hide_graph(GtkTreeModel *store,
                              G_GNUC_UNUSED GtkTreePath *path,
                              GtkTreeIter *iter,
                              G_GNUC_UNUSED gpointer userdata)
{
    GtkWidget *graph;

    gtk_tree_model_get(store, iter, GRAPHLIST_GRAPH, &graph, -1);
    if (!graph)
        return FALSE;
    gtk_widget_destroy(graph);

    /* To be usable as gtk_tree_model_foreach() callback */
    return FALSE;
}

static gboolean
gwy_app_graph_list_show_graph(GtkTreeModel *model,
                              G_GNUC_UNUSED GtkTreePath *path,
                              GtkTreeIter *iter,
                              gpointer userdata)
{
    Controls *controls = (Controls*)userdata;
    GtkWidget *graph, *window;
    GwyGraphModel *gmodel;

    gtk_tree_model_get(model, iter,
                       GRAPHLIST_GMODEL, &gmodel,
                       GRAPHLIST_GRAPH, &graph,
                       -1);
    if (graph)
        return FALSE;
    graph = gwy_graph_new(gmodel);
    g_object_unref(gmodel);
    window = gwy_app_graph_window_create(GWY_GRAPH(graph), controls->data);
    gtk_list_store_set(GTK_LIST_STORE(model), iter,
                       GRAPHLIST_GRAPH, window,
                       -1);
    g_object_set_data(G_OBJECT(window), ITER_KEY, gtk_tree_iter_copy(iter));
    g_signal_connect_swapped(window, "destroy",
                             G_CALLBACK(gwy_app_graph_list_graph_destroy),
                             controls);

    /* To be usable as gtk_tree_model_foreach() callback */
    return FALSE;
}

static void
gwy_app_graph_list_delete_graph(GtkTreeModel *store,
                                GtkTreeIter *iter,
                                Controls *controls)
{
    GwyGraphModel *gmodel;
    GtkWidget *graph;
    gint id;
    gchar key[32];

    gtk_tree_model_get(store, iter,
                       GRAPHLIST_GMODEL, &gmodel,
                       GRAPHLIST_GRAPH, &graph,
                       -1);
    if (graph)
        gtk_widget_destroy(graph);
    gtk_list_store_remove(GTK_LIST_STORE(store), iter);
    id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gmodel), GMODEL_ID_KEY));
    g_assert(id);
    g_snprintf(key, sizeof(key), "/0/graph/graph/%d", id);
    gwy_container_remove_by_name(controls->data, key);
    g_object_unref(gmodel);
}

static void
gwy_app_graph_list_hide_all(Controls *controls)
{
    GtkTreeModel *model;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->list));
    gtk_tree_model_foreach(model, gwy_app_graph_list_hide_graph, controls);
}

static void
gwy_app_graph_list_show_all(Controls *controls)
{
    GtkTreeModel *model;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->list));
    gtk_tree_model_foreach(model, gwy_app_graph_list_show_graph, controls);
}

static void
gwy_app_graph_list_delete_all(Controls *controls)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->list));
    while (gtk_tree_model_get_iter_first(model, &iter))
        gwy_app_graph_list_delete_graph(model, &iter, controls);
}

static void
gwy_app_graph_list_delete_one(Controls *controls)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->list));
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
        gwy_app_graph_list_delete_graph(model, &iter, controls);
}

static void
gwy_app_graph_list_cell_renderer(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                 GtkCellRenderer *cell,
                                 GtkTreeModel *model,
                                 GtkTreeIter *piter,
                                 gpointer userdata)
{
    GwyGraphModel *gmodel;
    GwyGraph *graph;
    gulong id;
    gchar s[16];

    id = GPOINTER_TO_UINT(userdata);
    g_assert(id > GRAPHLIST_GMODEL && id < GRAPHLIST_LAST);
    switch (id) {
        case GRAPHLIST_GRAPH:
        gtk_tree_model_get(model, piter, GRAPHLIST_GRAPH, &graph, -1);
        g_object_set(cell, "active", graph != NULL, NULL);
        break;

        case GRAPHLIST_TITLE:
        gtk_tree_model_get(model, piter, GRAPHLIST_GMODEL, &gmodel, -1);
        g_object_set(cell, "text", gwy_graph_model_get_title(gmodel), NULL);
        g_object_unref(gmodel);
        break;

        case GRAPHLIST_NCURVES:
        gtk_tree_model_get(model, piter, GRAPHLIST_GMODEL, &gmodel, -1);
        g_snprintf(s, sizeof(s), "%d", gwy_graph_model_get_n_curves(gmodel));
        g_object_set(cell, "text", s, NULL);
        g_object_unref(gmodel);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
gwy_app_graph_list_check_graph(GwyGraphWindow *graph,
                               GwyGraphModel *gmodel)
{
    if (gwy_graph_get_model(GWY_GRAPH(gwy_graph_window_get_graph(graph)))
        == gmodel)
        g_object_set_data(G_OBJECT(gmodel), GMODEL_GRAPH_KEY, graph);
}

static void
gwy_app_graph_list_add_line(gpointer hkey,
                            GValue *value,
                            Controls *controls)
{
    GObject *gmodel;
    GtkTreeIter iter;
    GtkListStore *store;
    GwyGraphWindow *graph;

    g_return_if_fail(G_VALUE_HOLDS_OBJECT(value));
    gmodel = g_value_get_object(value);
    g_return_if_fail(GWY_IS_GRAPH_MODEL(gmodel));

    if (!g_object_get_data(gmodel, GMODEL_ID_KEY)) {
        GQuark quark;
        const gchar *key;
        gint32 id;

        quark = GPOINTER_TO_INT(hkey);
        key = g_quark_to_string(quark);
        g_return_if_fail(key);
        key = strrchr(key, '/');
        g_return_if_fail(key);
        key++;
        id = atoi(key);
        g_return_if_fail(id);
        g_object_set_data(gmodel, GMODEL_ID_KEY, GINT_TO_POINTER(id));
    }

    /* FIXME: The need to scan graph list means we screwed up ownership logic */
    gwy_app_graph_window_foreach((GFunc)gwy_app_graph_list_check_graph,
                                 GWY_GRAPH_MODEL(gmodel));
    graph = g_object_get_data(gmodel, GMODEL_GRAPH_KEY);
    gwy_debug("graph found: %p", graph);
    g_object_set_data(gmodel, GMODEL_GRAPH_KEY, NULL);
    store = GTK_LIST_STORE(gtk_tree_view_get_model
                                              (GTK_TREE_VIEW(controls->list)));
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       GRAPHLIST_GMODEL, gmodel,
                       GRAPHLIST_GRAPH, graph,
                       -1);
    if (graph) {
        g_signal_connect_swapped(graph, "destroy",
                                 G_CALLBACK(gwy_app_graph_list_graph_destroy),
                                 controls);
        g_object_set_data(G_OBJECT(graph), ITER_KEY, gtk_tree_iter_copy(&iter));
    }
}

static gint
gwy_app_graph_list_sort_func(GtkTreeModel *model,
                             GtkTreeIter *a,
                             GtkTreeIter *b,
                             G_GNUC_UNUSED gpointer user_data)
{
    GObject *p, *q;
    guint x, y;

    gtk_tree_model_get(model, a, 0, &p, -1);
    gtk_tree_model_get(model, b, 0, &q, -1);

    x = GPOINTER_TO_INT(g_object_get_data(p, GMODEL_ID_KEY));
    y = GPOINTER_TO_INT(g_object_get_data(q, GMODEL_ID_KEY));

    gwy_debug("x = %d, y = %d", x, y);
    g_object_unref(p);
    g_object_unref(q);

    if (y > x)
        return -1;
    else if (x > y)
        return 1;
    else
        return 0;
}

static void
gwy_app_graph_list_graph_destroy(Controls *controls,
                                 GwyGraphWindow *window)
{
    GtkTreeIter *iter;
    GtkTreeModel *model;

    iter = g_object_get_data(G_OBJECT(window), ITER_KEY);
    gwy_debug("iter: %p", iter);
    if (!iter)
        return;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->list));
    gtk_list_store_set(GTK_LIST_STORE(model), iter,
                       GRAPHLIST_GRAPH, NULL,
                       -1);
    gtk_tree_iter_free(iter);
    g_object_set_data(G_OBJECT(window), ITER_KEY, NULL);
}

static void
gwy_app_graph_list_destroy(Controls *controls)
{
    GtkTreeModel *model;
    GtkTreeIter iter, *piter;

    /* Free iters attached to graph windows */
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->list));
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            GObject *graph;

            gtk_tree_model_get(model, &iter,
                               GRAPHLIST_GRAPH, &graph,
                               -1);
            if (graph && (piter = g_object_get_data(graph, ITER_KEY)))
                gtk_tree_iter_free(piter);
            if (graph)
                g_signal_handlers_disconnect_by_func
                                             (graph,
                                              gwy_app_graph_list_graph_destroy,
                                              controls);
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    g_object_set_data(G_OBJECT(controls->data), CONTROLS_KEY, NULL);
    g_object_unref(controls->data);
    g_free(controls);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
