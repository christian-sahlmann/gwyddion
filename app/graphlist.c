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

#include <string.h>
#include <stdlib.h>

#include <libgwyddion/gwyddion.h>
#include <libgwydgets/gwydgets.h>
#include "gwyapp.h"
#include "gwyappinternal.h"

#ifdef I_WANT_A_BROKEN_GWY_GRAPH_MODEL

enum {
    GRAPHLIST_GMODEL,
    GRAPHLIST_EDITABLE,
    GRAPHLIST_VISIBLE,
    GRAPHLIST_TITLE,
    GRAPHLIST_NCURVES,
    GRAPHLIST_ID,
    GRAPHLIST_LAST
};

typedef struct {
    GtkWidget *delete;
    GtkWidget *delete_all;
    GtkWidget *show_all;
    GtkWidget *hide_all;
    GtkWidget *list;
} Controls;

static GtkWidget* gwy_app_graph_list_construct       (GwyContainer *data,
                                                      Controls *controls);
static void       gwy_app_graph_list_cell_renderer   (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *cell,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *piter,
                                                      gpointer userdata);
static void       gwy_app_graph_list_row_inserted    (GtkTreeModel *store,
                                                      GtkTreePath *path,
                                                      GtkTreeIter *iter,
                                                      Controls *controls);
static void       gwy_app_graph_list_row_deleted     (GtkTreeModel *store,
                                                      GtkTreePath *path,
                                                      Controls *controls);
static void       gwy_app_graph_list_selection_changed(GtkTreeSelection *selection,
                                                      Controls *controls);
static void       gwy_app_graph_list_toggled         (GtkCellRendererToggle *toggle,
                                                      gchar *pathstring,
                                                      GtkWidget *list);
static gboolean   gwy_app_graph_list_hide_graph      (GtkTreeModel *store,
                                                      GtkTreePath *path,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static gboolean   gwy_app_graph_list_show_graph      (GtkTreeModel *store,
                                                      GtkTreePath *path,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static gboolean   gwy_app_graph_list_delete_graph    (GtkTreeModel *store,
                                                      GtkTreePath *path,
                                                      GtkTreeIter *iter,
                                                      gpointer userdata);
static void       gwy_app_graph_list_hide_all        (GtkWidget *list);
static void       gwy_app_graph_list_show_all        (GtkWidget *list);
static void       gwy_app_graph_list_delete_all      (GtkWidget *list);
static void       gwy_app_graph_list_delete_one      (GtkWidget *list);
static void       gwy_app_graph_list_add_line        (gpointer hkey,
                                                      GValue *value,
                                                      GtkListStore *store);
static gint       gwy_app_graph_list_sort_func       (GtkTreeModel *model,
                                                      GtkTreeIter *a,
                                                      GtkTreeIter *b,
                                                      gpointer user_data);
static void       gwy_app_graph_list_orphaned        (GtkWidget *graph_view);

void
gwy_app_graph_list_add(GwyDataWindow *data_window,
                       GwyGraph *graph)
{
    GwyContainer *data;
    GtkListStore *store;
    GtkTreeIter iter;
    GObject *gmodel;
    GtkWidget *graph_view, *list;
    gint32 lastid;
    gchar key[32];

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    g_return_if_fail(GWY_IS_GRAPH(graph));

    data = gwy_data_window_get_data(data_window);
    gmodel = gwy_graph_model_new(graph);
    g_object_set_data(G_OBJECT(graph), "graph-model", gmodel);

    /* compute new id and new id list */
    if (gwy_container_gis_int32_by_name(data, "/0/graph/lastid", &lastid)) {
        if (lastid <= 0)
            g_warning("Broken last graph id");
        lastid = MAX(0, lastid) + 1;
    }
    else
        lastid = 1;

    g_snprintf(key, sizeof(key), "/0/graph/graph/%d", lastid);
    g_object_set_data(gmodel, "gwy-app-graph-list-id", GINT_TO_POINTER(lastid));
    gwy_container_set_int32_by_name(data, "/0/graph/lastid", lastid);
    gwy_container_set_object_by_name(data, key, gmodel);
    g_object_unref(gmodel);
    if (!(graph_view = g_object_get_data(G_OBJECT(data_window),
                                         "gwy-app-graph-list-window")))
        return;

    list = g_object_get_data(G_OBJECT(graph_view), "gwy-app-graph-list-view");
    g_assert(list);
    /* XXX: redraw assures the toggles get into a consistent state.  A more
     * fine-grained method should be probably used... */
    g_signal_connect_swapped(graph, "destroy",
                             G_CALLBACK(gtk_widget_queue_draw), list);
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       GRAPHLIST_GMODEL, gmodel,
                       GRAPHLIST_EDITABLE, TRUE,
                       -1);
}

GtkWidget*
gwy_app_graph_list_new(GwyDataWindow *data_window)
{
    GtkWidget *window, *vbox, *buttonbox, *list, *scroll;
    Controls *controls;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Graph list for FIXME");
    gtk_window_set_default_size(GTK_WINDOW(window), -1, 180);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    controls = g_new(Controls, 1);
    list = gwy_app_graph_list_construct(gwy_data_window_get_data(data_window),
                                        controls);
    g_signal_connect_swapped(list, "destroy", G_CALLBACK(g_free), controls);
    gtk_container_add(GTK_CONTAINER(scroll), list);

    buttonbox = gtk_hbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(buttonbox), 2);
    gtk_box_pack_start(GTK_BOX(vbox), buttonbox, FALSE, FALSE, 0);

    controls->delete = gtk_button_new_with_mnemonic(_("_Delete"));
    gtk_box_pack_start(GTK_BOX(buttonbox), controls->delete, TRUE, TRUE, 0);
    g_signal_connect_swapped(controls->delete, "clicked",
                             G_CALLBACK(gwy_app_graph_list_delete_one), list);

    controls->delete_all = gtk_button_new_with_mnemonic(_("Delete _All"));
    gtk_box_pack_start(GTK_BOX(buttonbox), controls->delete_all, TRUE, TRUE, 0);
    g_signal_connect_swapped(controls->delete_all, "clicked",
                             G_CALLBACK(gwy_app_graph_list_delete_all), list);

    controls->show_all = gtk_button_new_with_mnemonic(_("_Show All"));
    gtk_box_pack_start(GTK_BOX(buttonbox), controls->show_all, TRUE, TRUE, 0);
    g_signal_connect_swapped(controls->show_all, "clicked",
                             G_CALLBACK(gwy_app_graph_list_show_all), list);

    controls->hide_all = gtk_button_new_with_mnemonic(_("_Hide All"));
    gtk_box_pack_start(GTK_BOX(buttonbox), controls->hide_all, TRUE, TRUE, 0);
    g_signal_connect_swapped(controls->hide_all, "clicked",
                             G_CALLBACK(gwy_app_graph_list_hide_all), list);

    g_object_set_data(G_OBJECT(data_window), "gwy-app-graph-list-window",
                      window);
    g_object_set_data(G_OBJECT(window), "gwy-app-graph-list-view", list);

    g_signal_connect_swapped(data_window, "destroy",
                             G_CALLBACK(gwy_app_graph_list_orphaned), window);
    gtk_widget_show_all(vbox);

    return window;
}

static GtkWidget*
gwy_app_graph_list_construct(GwyContainer *data,
                             Controls *controls)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { "Vis.", GRAPHLIST_VISIBLE },
        { "Title", GRAPHLIST_TITLE },
        { "Curves", GRAPHLIST_NCURVES },
        { "Id", GRAPHLIST_ID },   /* FIXME: debug only */
    };

    GtkWidget *list;
    GtkListStore *store;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    gsize i;

    /* use an `editable' boolean column which is alwas true */
    store = gtk_list_store_new(2, G_TYPE_OBJECT, G_TYPE_BOOLEAN);

    gwy_container_foreach(data, "/0/graph/graph",
                          (GHFunc)(gwy_app_graph_list_add_line), store);

    list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    controls->list = list;
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(list), TRUE);
    g_object_unref(store);
    g_object_set_data(G_OBJECT(store), "container", data);

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
                                    0, gwy_app_graph_list_sort_func,
                                    NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), 0,
                                         GTK_SORT_ASCENDING);

    /* first column (toggle) is special, set up it separately */
    renderer = gtk_cell_renderer_toggle_new();
    g_signal_connect(renderer, "toggled",
                     G_CALLBACK(gwy_app_graph_list_toggled), list);
    column = gtk_tree_view_column_new_with_attributes(_(columns[0].title),
                                                      renderer,
                                                      "activatable",
                                                      GRAPHLIST_EDITABLE,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_graph_list_cell_renderer,
         GUINT_TO_POINTER(columns[0].id),
         NULL);  /* destroy notify */
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

    /* other columns */
    for (i = 1; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_(columns[i].title),
                                                          renderer,
                                                          NULL);
        gtk_tree_view_column_set_cell_data_func
            (column, renderer,
             gwy_app_graph_list_cell_renderer,
             GUINT_TO_POINTER(columns[i].id),
             NULL);  /* destroy notify */
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
    }

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
                           GtkWidget *list)
{
    GtkTreeModel *store;
    GtkTreePath *path;
    GtkTreeIter iter;
    gboolean active;

    active = gtk_cell_renderer_toggle_get_active(toggle);
    path = gtk_tree_path_new_from_string(pathstring);
    store = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    gtk_tree_model_get_iter(store, &iter, path);
    if (active)
        gwy_app_graph_list_hide_graph(store, path, &iter, list);
    else
        gwy_app_graph_list_show_graph(store, path, &iter, list);
    gtk_tree_path_free(path);
}

static gboolean
gwy_app_graph_list_hide_graph(GtkTreeModel *store,
                              G_GNUC_UNUSED GtkTreePath *path,
                              GtkTreeIter *iter,
                              G_GNUC_UNUSED gpointer userdata)
{
    GtkWidget *graph;
    GObject *gmodel;

    gtk_tree_model_get(store, iter, GRAPHLIST_GMODEL, &gmodel, -1);
    graph = GTK_WIDGET(GWY_GRAPH_MODEL(gmodel)->graph);
    if (graph)
        gtk_widget_destroy(gtk_widget_get_toplevel(graph));
    g_object_unref(gmodel);

    return FALSE;
}

static gboolean
gwy_app_graph_list_show_graph(GtkTreeModel *store,
                              G_GNUC_UNUSED GtkTreePath *path,
                              GtkTreeIter *iter,
                              gpointer userdata)
{
    GtkWidget *graph;
    GObject *gmodel;
    GtkWidget *list;

    list = GTK_WIDGET(userdata);
    gtk_tree_model_get(store, iter, GRAPHLIST_GMODEL, &gmodel, -1);
    if (GWY_GRAPH_MODEL(gmodel)->graph) {
        g_object_unref(gmodel);
        return FALSE;
    }

    graph = gwy_graph_new_from_model(GWY_GRAPH_MODEL(gmodel));
    g_object_set_data(G_OBJECT(graph), "graph-model", gmodel);
    /* XXX: redraw assures the toggles get into a consistent state.  A more
     * fine-grained method should be probably used... */
    g_signal_connect_swapped(graph, "destroy",
                             G_CALLBACK(gtk_widget_queue_draw), list);
    gwy_app_graph_window_create(graph);
    g_object_unref(gmodel);

    return FALSE;
}

static gboolean
gwy_app_graph_list_delete_graph(GtkTreeModel *store,
                                G_GNUC_UNUSED GtkTreePath *path,
                                GtkTreeIter *iter,
                                G_GNUC_UNUSED gpointer userdata)
{
    GtkWidget *graph;
    GObject *gmodel;
    GwyContainer *data;
    gint id;
    gchar key[32];

    gtk_tree_model_get(store, iter, GRAPHLIST_GMODEL, &gmodel, -1);
    graph = GTK_WIDGET(GWY_GRAPH_MODEL(gmodel)->graph);
    if (graph)
        gtk_widget_destroy(gtk_widget_get_toplevel(graph));

    gtk_list_store_remove(GTK_LIST_STORE(store), iter);
    id = GPOINTER_TO_INT(g_object_get_data(gmodel, "gwy-app-graph-list-id"));
    g_assert(id);
    g_snprintf(key, sizeof(key), "/0/graph/graph/%d", id);
    data = (GwyContainer*)g_object_get_data(G_OBJECT(store), "container");
    g_assert(GWY_IS_CONTAINER(data));
    gwy_container_remove_by_name(data, key);
    g_object_unref(gmodel);

    return FALSE;
}

static void
gwy_app_graph_list_hide_all(GtkWidget *list)
{
    GtkTreeModel *store;

    store = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    gtk_tree_model_foreach(store, gwy_app_graph_list_hide_graph, list);
    gtk_widget_queue_draw(list);
}

static void
gwy_app_graph_list_show_all(GtkWidget *list)
{
    GtkTreeModel *store;

    store = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    gtk_tree_model_foreach(store, gwy_app_graph_list_show_graph, list);
    gtk_widget_queue_draw(list);
}

static void
gwy_app_graph_list_delete_all(GtkWidget *list)
{
    GtkTreeModel *store;
    GtkTreeIter iter;

    store = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    while (gtk_tree_model_get_iter_first(store, &iter))
        gwy_app_graph_list_delete_graph(store, NULL, &iter, list);
    gtk_widget_queue_draw(list);
}

static void
gwy_app_graph_list_delete_one(GtkWidget *list)
{
    GtkTreeSelection *selection;
    GtkTreeModel *store;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    if (gtk_tree_selection_get_selected(selection, &store, &iter))
        gwy_app_graph_list_delete_graph(store, NULL, &iter, list);
    gtk_widget_queue_draw(list);
}

static void
gwy_app_graph_list_cell_renderer(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                 GtkCellRenderer *cell,
                                 GtkTreeModel *model,
                                 GtkTreeIter *piter,
                                 gpointer userdata)
{
    GwyGraphModel *gmodel;
    gulong id;
    gchar s[16];

    id = GPOINTER_TO_UINT(userdata);
    g_assert(id > GRAPHLIST_GMODEL && id < GRAPHLIST_LAST);
    gtk_tree_model_get(model, piter, GRAPHLIST_GMODEL, &gmodel, -1);
    g_return_if_fail(GWY_IS_GRAPH_MODEL(gmodel));
    switch (id) {
        case GRAPHLIST_VISIBLE:
        g_object_set(cell, "active", gmodel->graph != NULL, NULL);
        break;

        case GRAPHLIST_TITLE:
        g_object_set(cell, "text", gmodel->title->str, NULL);
        break;

        case GRAPHLIST_NCURVES:
        g_snprintf(s, sizeof(s), "%d", gwy_graph_model_get_n_curves(gmodel));
        g_object_set(cell, "text", s, NULL);
        break;

        case GRAPHLIST_ID:
        g_snprintf(s, sizeof(s), "%d",
                   GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gmodel),
                                                     "gwy-app-graph-list-id")));
        g_object_set(cell, "text", s, NULL);
        break;

        default:
        g_assert_not_reached();
        break;
    }
    g_object_unref(gmodel);
}

static void
gwy_app_graph_list_add_line(gpointer hkey,
                            GValue *value,
                            GtkListStore *store)
{
    GObject *gmodel;
    GtkTreeIter iter;

    g_return_if_fail(G_VALUE_HOLDS_OBJECT(value));
    gmodel = g_value_get_object(value);
    g_return_if_fail(GWY_IS_GRAPH_MODEL(gmodel));

    if (!g_object_get_data(gmodel, "gwy-app-graph-list-id")) {
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
        g_object_set_data(gmodel, "gwy-app-graph-list-id",
                          GINT_TO_POINTER(id));
    }

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       GRAPHLIST_GMODEL, gmodel,
                       GRAPHLIST_EDITABLE, TRUE,
                       -1);
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

    x = GPOINTER_TO_INT(g_object_get_data(p, "gwy-app-graph-list-id"));
    y = GPOINTER_TO_INT(g_object_get_data(q, "gwy-app-graph-list-id"));

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

static gboolean
gwy_app_graph_list_release_gmodel(GtkTreeModel *store,
                                  G_GNUC_UNUSED GtkTreePath *path,
                                  GtkTreeIter *iter,
                                  gpointer list)
{
    GObject *gmodel;
    GwyGraph *graph;

    gtk_tree_model_get(store, iter, GRAPHLIST_GMODEL, &gmodel, -1);
    graph = GWY_GRAPH_MODEL(gmodel)->graph;
    if (graph)
        g_signal_handlers_disconnect_matched(graph,
                                             G_SIGNAL_MATCH_FUNC
                                             | G_SIGNAL_MATCH_DATA,
                                             0, 0, 0,
                                             gtk_widget_queue_draw,
                                             list);
    g_object_unref(gmodel);

    return FALSE;
}

/* What to do when our beloved data window is treacheously murdered:
 * commit a suicide. */
static void
gwy_app_graph_list_orphaned(GtkWidget *graph_view)
{
    GtkWidget *list;
    GtkTreeModel *store;

    list = g_object_get_data(G_OBJECT(graph_view), "gwy-app-graph-list-view");
    g_assert(list);
    store = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    gtk_tree_model_foreach(store, gwy_app_graph_list_release_gmodel, list);
    gtk_widget_destroy(graph_view);
}

#endif  /* I_WANT_A_BROKEN_GWY_GRAPH_MODEL */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
