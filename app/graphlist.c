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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <libgwyddion/gwyddion.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include "gwyapp.h"
#include "gwyappinternal.h"

void
gwy_app_graph_list_add(GwyDataWindow *data_window,
                       GwyGraph *graph)
{
    GwyContainer *data;
    GObject *gmodel;
    GtkWidget *graph_view;
    const guchar *list, *last;
    gchar *newlist;
    guint newid;
    gchar key[24];

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    g_return_if_fail(GWY_IS_GRAPH(graph));

    data = gwy_data_window_get_data(data_window);
    gmodel = gwy_graph_model_new(graph);

    /* compute new id and new id list */
    if (gwy_container_gis_string_by_name(data, "/0/graph/list", &list)) {
        if ((last = strrchr(list, '\n')))
            last++;
        else
            last = list;

        newid = atol(last);
        if (!newid)
            g_warning("Broken graph id list");
        newid++;
        newlist = g_strdup_printf("%s\n%u", list, newid);
    }
    else {
        newid = 1;
        newlist = g_strdup_printf("%u", newid);
    }

    g_snprintf(key, sizeof(key), "/0/graph/%u", newid);
    gwy_container_set_object_by_name(data, key, gmodel);
    gwy_container_set_string_by_name(data, "/0/graph/list", newlist);

    if ((graph_view = g_object_get_data(G_OBJECT(data_window),
                                        "gwy-app-graph-list-view"))) {
        /* TODO: actualize the view */
    }
}

#if 0
static GtkWidget*
gwy_graph_model_list_construct(GwyContainer *data)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { "Shown", META_VISIBLE },
        { "Title", META_TITLE },
        { "Curves", META_NCURVES },
    };

    GtkWidget *tree;
    GtkListStore *store;
    GtkTreeSelection *select;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeIter iter;
    gsize i;

    store = gtk_list_store_new(META_LAST,
                               G_TYPE_STRING,  /* key */
                               G_TYPE_STRING   /* value */
                              );

    gwy_container_foreach(data, "/meta",
                          (GHFunc)(gwy_graph_model_list_add_line), store);
    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
        g_object_unref(store);
        return NULL;
    }

    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
    g_object_unref(store);
    g_object_set_data(G_OBJECT(store), "container", data);

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
                                    0, gwy_meta_sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), 0,
                                         GTK_SORT_ASCENDING);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(columns[i].title,
                                                          renderer,
                                                          "text", columns[i].id,
                                                          NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                gwy_graph_model_list_cell_renderer,
                                                GUINT_TO_POINTER(columns[i].id),
                                                NULL);  /* destroy notify */
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    }

    select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    gtk_tree_selection_set_mode(select, GTK_SELECTION_NONE);

    return tree;
}

static void
gwy_graph_model_list_cell_renderer(G_GNUC_UNUSED GtkTreeViewColumn *column,
                               GtkCellRenderer *cell,
                               GtkTreeModel *model,
                               GtkTreeIter *piter,
                               gpointer userdata)
{
    const gchar *text;
    gulong id;

    id = GPOINTER_TO_UINT(userdata);
    /*g_assert(id >= META_KEY && id < META_LAST);*/
    g_assert(id < META_LAST);
    gtk_tree_model_get(model, piter, id, &text, -1);
    g_return_if_fail(text);
    g_object_set(cell, "text", text, NULL);
}

static void
gwy_graph_model_list_add_line(gpointer hkey,
                          GValue *value,
                          GtkListStore *store)
{
    GQuark quark;
    GtkTreeIter iter;
    const gchar *key, *val;
    gchar *s;

    g_return_if_fail(G_VALUE_HOLDS_STRING(value));
    val = g_value_get_string(value);
    if (g_utf8_validate(val, -1 , NULL))
        s = NULL;
    else {
        if (!(s = g_locale_to_utf8(val, -1, NULL, NULL, NULL)))
            s = g_strdup("???");
    }
    quark = GPOINTER_TO_INT(hkey);
    key = g_quark_to_string(quark);
    g_return_if_fail(key);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       META_KEY, key + sizeof("/meta"),
                       META_VALUE, s ? s : val,
                       -1);
    g_free(s);
}

static void
gwy_meta_destroy(GtkWidget *window,
                 GtkWidget *browser)
{
    GtkTreeModel *model;

    gwy_debug("");
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(browser));
    gtk_widget_destroy(window);
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
