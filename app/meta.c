/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydatawindow.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertext.h>

/* XXX: This is completely static.  Ideally the browser would allow changing
 * the metadata.  However, now it can't even react properly to external
 * changes. */

static void       gwy_meta_browser_cell_renderer (GtkTreeViewColumn *column,
                                                  GtkCellRenderer *cell,
                                                  GtkTreeModel *model,
                                                  GtkTreeIter *piter,
                                                  gpointer data);
static void       gwy_meta_browser_add_line      (gpointer hkey,
                                                  GValue *value,
                                                  GtkListStore *store);
static GtkWidget* gwy_meta_browser_construct     (GwyContainer *data);
static void       gwy_meta_destroy               (GtkWidget *window,
                                                  GtkWidget *browser);

enum {
    META_KEY,
    META_VALUE,
    META_LAST
};


/**
 * gwy_meta_browser:
 * @data: A data window to show metadata of.
 *
 * Shows a simple metadata browser.
 **/
void
gwy_app_metadata_browser(GwyDataWindow *data_window)
{
    GtkWidget *window, *browser;
    GwyContainer *data;
    gchar *filename, *title;
    const gchar *fnm;

    data = gwy_data_window_get_data(data_window);
    g_return_if_fail(GWY_IS_CONTAINER(data));

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    /* FIXME: this duplicates code from GwyDataWindow */
    if (gwy_container_contains_by_name(data, "/filename")) {
        fnm = gwy_container_get_string_by_name(data, "/filename");
        filename = g_path_get_basename(fnm);
    }
    else {
        fnm = gwy_container_get_string_by_name(data, "/filename/untitled");
        filename = g_strdup(fnm);
    }

    title = g_strdup_printf("%s Metadata %s", g_get_application_name(),
                            filename);
    gtk_window_set_title(GTK_WINDOW(window), title);
    g_free(title);
    g_free(filename);
    gtk_window_set_wmclass(GTK_WINDOW(window), "toolbox",
                           g_get_application_name());
    browser = gwy_meta_browser_construct(data);
    gtk_container_add(GTK_CONTAINER(window), browser);
    g_signal_connect(window, "destroy", G_CALLBACK(gwy_meta_destroy), browser);
    gtk_widget_show_all(window);
}

static GtkWidget*
gwy_meta_browser_construct(GwyContainer *data)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { "Key", META_KEY },
        { "Value", META_VALUE },
    };

    GtkWidget *tree;
    GtkListStore *store;
    GtkTreeSelection *select;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    gsize i;

    store = gtk_list_store_new(META_LAST,
                               G_TYPE_STRING,  /* key */
                               G_TYPE_STRING   /* value */
                              );

    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    g_object_set_data(G_OBJECT(store), "container", data);
    gwy_container_foreach(data, "/meta",
                          (GHFunc)(gwy_meta_browser_add_line), store);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(columns[i].title,
                                                          renderer,
                                                          "text", columns[i].id,
                                                          NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                gwy_meta_browser_cell_renderer,
                                                GUINT_TO_POINTER(columns[i].id),
                                                NULL);  /* destroy notify */
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    }

    select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    gtk_tree_selection_set_mode(select, GTK_SELECTION_NONE);

    return tree;
}

static void
gwy_meta_browser_cell_renderer(G_GNUC_UNUSED GtkTreeViewColumn *column,
                               GtkCellRenderer *cell,
                               GtkTreeModel *model,
                               GtkTreeIter *piter,
                               gpointer userdata)
{
    const gchar *text;
    gulong id;

    id = GPOINTER_TO_UINT(userdata);
    g_assert(id >= META_KEY && id < META_LAST);
    gtk_tree_model_get(model, piter, id, &text, -1);
    g_return_if_fail(text);
    g_object_set(cell, "text", text, NULL);
}

static void
gwy_meta_browser_add_line(gpointer hkey,
                          GValue *value,
                          GtkListStore *store)
{
    GQuark quark;
    GtkTreeIter iter;
    const gchar *key;

    g_return_if_fail(G_VALUE_HOLDS_STRING(value));
    quark = GPOINTER_TO_INT(hkey);
    key = g_quark_to_string(quark);
    g_return_if_fail(key);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       META_KEY, key + sizeof("/meta"),
                       META_VALUE, g_value_get_string(value),
                       -1);
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

