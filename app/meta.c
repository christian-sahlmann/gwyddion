/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydatawindow.h>

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
                                                  GwyContainer *data);
static void       gwy_meta_data_finalized        (GtkWidget *window,
                                                  GwyContainer *data);

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
    GtkWidget *window, *browser, *scroll;
    GtkRequisition request;
    GwyContainer *data;
    gchar *filename, *title;

    data = gwy_data_window_get_data(data_window);
    g_return_if_fail(GWY_IS_CONTAINER(data));
    if ((window = GTK_WIDGET(g_object_get_data(G_OBJECT(data),
                                               "metadata-browser")))) {
        gtk_window_present(GTK_WINDOW(window));
        return;
    }
    filename = gwy_data_window_get_base_name(data_window);

    browser = gwy_meta_browser_construct(data);
    if (!browser) {
        window = gtk_message_dialog_new(NULL, 0,
                                        GTK_MESSAGE_INFO,
                                        GTK_BUTTONS_OK,
                                        N_("There is no metadata in %s."),
                                        filename);
        g_signal_connect(window, "delete_event",
                         G_CALLBACK(gtk_widget_destroy), NULL);
        g_signal_connect(window, "response",
                         G_CALLBACK(gtk_widget_destroy), NULL);
        gtk_widget_show_all(window);

        return;
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    title = g_strdup_printf(_("Metadata of %s (%s)"),
                            filename, g_get_application_name());
    gtk_window_set_title(GTK_WINDOW(window), title);
    g_free(title);
    g_free(filename);

    gtk_widget_size_request(browser, &request);
    gtk_window_set_default_size(GTK_WINDOW(window),
                                MIN(request.width + 24, 2*gdk_screen_width()/3),
                                MIN(request.height + 24,
                                    2*gdk_screen_height()/3));
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), browser);
    gtk_container_add(GTK_CONTAINER(window), scroll);
    g_object_set_data(G_OBJECT(data), "metadata-browser", window);
    g_signal_connect(window, "destroy", G_CALLBACK(gwy_meta_destroy), data);
    g_object_weak_ref(G_OBJECT(data), (GWeakNotify)&gwy_meta_data_finalized,
                      window);
    gtk_widget_show_all(window);
}

static gint
gwy_meta_sort_func(GtkTreeModel *model,
                   GtkTreeIter *a,
                   GtkTreeIter *b,
                   G_GNUC_UNUSED gpointer user_data)
{
    gchar *ka, *kb;
    gint result;

    gtk_tree_model_get(model, a, 0, &ka, -1);
    gtk_tree_model_get(model, b, 0, &kb, -1);
    result = strcmp(ka, kb);
    g_free(ka);
    g_free(kb);

    return result;
}

static GtkWidget*
gwy_meta_browser_construct(GwyContainer *data)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { N_("metadata|Key"),   META_KEY },
        { N_("Value"), META_VALUE },
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
                          (GHFunc)(gwy_meta_browser_add_line), store);
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
        column = gtk_tree_view_column_new_with_attributes
                                      (gwy_sgettext(columns[i].title),
                                       renderer, "text", columns[i].id, NULL);
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
    gchar *text;
    gulong id;

    id = GPOINTER_TO_UINT(userdata);
    /*g_assert(id >= META_KEY && id < META_LAST);*/
    g_assert(id < META_LAST);
    gtk_tree_model_get(model, piter, id, &text, -1);
    g_return_if_fail(text);
    g_object_set(cell, "markup", text, NULL);
    g_free(text);
}

static void
gwy_meta_browser_add_line(gpointer hkey,
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
                 GwyContainer *data)
{
    g_object_weak_unref(G_OBJECT(data), (GWeakNotify)&gwy_meta_data_finalized,
                        window);
    g_object_set_data(G_OBJECT(data), "metadata-browser", NULL);
}

static void
gwy_meta_data_finalized(GtkWidget *window,
                        GwyContainer *data)
{
    g_signal_handlers_disconnect_by_func(window, &gwy_meta_destroy, data);
    gtk_widget_destroy(window);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

