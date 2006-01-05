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
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwydgetutils.h>

enum {
    META_KEY,
    META_VALUE
};

typedef struct {
    GQuark quark;
    gchar *value;
    gboolean isok;
} FixupData;

typedef struct {
    GwyContainer *container;
    gulong container_id;
    gulong item_id;
    GtkWidget *window;
    GtkWidget *treeview;
    GtkWidget *new;
    GtkWidget *delete;
    GtkWidget *close;
} MetadataBrowser;

static GtkWidget* gwy_meta_browser_construct    (MetadataBrowser *browser);
static void       gwy_meta_cell_edited          (GtkCellRendererText *renderer,
                                                 const gchar *strpath,
                                                 const gchar *text,
                                                 MetadataBrowser *browser);
static void       gwy_meta_browser_cell_renderer(GtkTreeViewColumn *column,
                                                 GtkCellRenderer *cell,
                                                 GtkTreeModel *model,
                                                 GtkTreeIter *piter,
                                                 gpointer data);
static void       gwy_meta_browser_add_line     (gpointer hkey,
                                                 GValue *value,
                                                 GSList **slist);
static void       gwy_meta_item_changed         (GwyContainer *container,
                                                 const gchar *key,
                                                 MetadataBrowser *browser);
static void       gwy_meta_delete               (MetadataBrowser *browser);
static void       gwy_meta_destroy              (MetadataBrowser *browser);
static void       gwy_meta_data_finalized       (MetadataBrowser *browser);

/**
 * gwy_meta_browser:
 * @data: A data window to show metadata of.
 *
 * Shows a simple metadata browser.
 **/
void
gwy_app_metadata_browser(GwyDataWindow *data_window)
{
    MetadataBrowser *browser;
    GtkWidget *scroll, *vbox, *hbox;
    GtkRequisition request;
    GwyContainer *data;
    gchar *filename, *title;

    data = gwy_data_window_get_data(data_window);
    g_return_if_fail(GWY_IS_CONTAINER(data));
    if ((browser = g_object_get_data(G_OBJECT(data), "metadata-browser"))) {
        gtk_window_present(GTK_WINDOW(browser->window));
        return;
    }
    filename = gwy_data_window_get_base_name(data_window);

    browser = g_new0(MetadataBrowser, 1);
    browser->container = data;
    browser->treeview = gwy_meta_browser_construct(browser);
    browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    title = g_strdup_printf(_("Metadata of %s (%s)"),
                            filename, g_get_application_name());
    gtk_window_set_title(GTK_WINDOW(browser->window), title);
    g_free(title);
    g_free(filename);

    gtk_widget_size_request(browser->treeview, &request);
    gtk_window_set_default_size(GTK_WINDOW(browser->window),
                                MIN(request.width + 24, 2*gdk_screen_width()/3),
                                MIN(request.height + 32,
                                    2*gdk_screen_height()/3));

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(browser->window), vbox);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(scroll), browser->treeview);
    g_object_set_data(G_OBJECT(data), "metadata-browser", browser);
    browser->container_id
        = g_signal_connect_swapped(browser->window, "destroy",
                                   G_CALLBACK(gwy_meta_destroy), browser);
    g_object_weak_ref(G_OBJECT(data), (GWeakNotify)&gwy_meta_data_finalized,
                      browser);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    browser->new = gwy_stock_like_button_new(_("_New"), GTK_STOCK_NEW);
    gtk_box_pack_start(GTK_BOX(hbox), browser->new, TRUE, TRUE, 0);

    browser->delete = gwy_stock_like_button_new(_("_Delete"), GTK_STOCK_DELETE);
    gtk_box_pack_start(GTK_BOX(hbox), browser->delete, TRUE, TRUE, 0);
    g_signal_connect_swapped(browser->delete, "clicked",
                             G_CALLBACK(gwy_meta_delete), browser);

    browser->close = gwy_stock_like_button_new(_("_Close"), GTK_STOCK_CLOSE);
    gtk_box_pack_start(GTK_BOX(hbox), browser->close, TRUE, TRUE, 0);
    g_signal_connect_swapped(browser->close, "clicked",
                             G_CALLBACK(gtk_widget_destroy), browser->window);

    gtk_widget_show_all(browser->window);
}

static gint
gwy_meta_sort_func(GtkTreeModel *model,
                   GtkTreeIter *a,
                   GtkTreeIter *b,
                   gpointer userdata)
{
    GwyContainer *container = (GwyContainer*)userdata;
    GQuark qa, qb;

    gtk_tree_model_get(model, a, META_KEY, &qa, -1);
    gtk_tree_model_get(model, b, META_KEY, &qb, -1);
    return g_utf8_collate(gwy_container_get_string(container, qa),
                          gwy_container_get_string(container, qb));
}

static GtkWidget*
gwy_meta_browser_construct(MetadataBrowser *browser)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { N_("Name"),  META_KEY,   },
        { N_("Value"), META_VALUE, },
    };

    GtkWidget *tree;
    GtkListStore *store;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeIter iter;
    GSList *fixlist, *l;
    gsize i;

    store = gtk_list_store_new(1, G_TYPE_UINT);
    fixlist = NULL;
    gwy_container_foreach(browser->container, "/meta",
                          (GHFunc)(gwy_meta_browser_add_line), &fixlist);
    /* Commit UTF-8 fixes found by gwy_meta_browser_add_line() */
    for (l = fixlist; l; l = g_slist_next(l)) {
        FixupData *fd = (FixupData*)l->data;

        if (fd->isok || fd->value) {
            if (!fd->isok) {
                gwy_container_set_string(browser->container,
                                         fd->quark, fd->value);
            }
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter, META_KEY, fd->quark, -1);
        }
        else
            gwy_container_remove(browser->container, fd->quark);
        g_free(fd);
    }
    g_slist_free(fixlist);

    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
    g_object_unref(store);

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
                                    0, gwy_meta_sort_func, browser->container,
                                    NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), 0,
                                         GTK_SORT_ASCENDING);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        g_object_set(G_OBJECT(renderer),
                     "editable", TRUE,
                     "editable-set", TRUE,
                     NULL);
        column = gtk_tree_view_column_new_with_attributes(_(columns[i].title),
                                                          renderer, NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                gwy_meta_browser_cell_renderer,
                                                browser->container, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
        g_object_set_data(G_OBJECT(renderer), "column",
                          GUINT_TO_POINTER(columns[i].id));
        g_signal_connect(renderer, "edited",
                         G_CALLBACK(gwy_meta_cell_edited), browser);
    }

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    g_signal_connect(browser->container, "item-changed",
                     G_CALLBACK(gwy_meta_item_changed), browser);

    return tree;
}

static void
gwy_meta_cell_edited(GtkCellRendererText *renderer,
                     const gchar *strpath,
                     const gchar *text,
                     MetadataBrowser *browser)
{
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    GQuark oldkey;
    guint col;

    col = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(renderer), "column"));
    gwy_debug("Column %d edited to <%s> (path %s)", col, text, strpath);

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(browser->treeview));
    path = gtk_tree_path_new_from_string(strpath);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    gtk_tree_model_get(model, &iter, META_KEY, &oldkey, -1);
    switch (col) {
        case META_KEY:
        break;

        case META_VALUE:
        if (pango_parse_markup(text, -1, 0, NULL, NULL, NULL, NULL))
            gwy_container_set_string(browser->container, oldkey,
                                     g_strdup(text));
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
gwy_meta_browser_cell_renderer(G_GNUC_UNUSED GtkTreeViewColumn *column,
                               GtkCellRenderer *renderer,
                               GtkTreeModel *model,
                               GtkTreeIter *iter,
                               gpointer userdata)
{
    GwyContainer *container = (GwyContainer*)userdata;
    const gchar *s;
    GQuark quark;
    gulong id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(renderer), "column"));
    gtk_tree_model_get(model, iter, META_KEY, &quark, -1);
    switch (id) {
        case META_KEY:
        g_object_set(renderer, "markup", g_quark_to_string(quark) + 6, NULL);
        break;

        case META_VALUE:
        s = gwy_container_get_string(container, quark);
        g_object_set(renderer, "markup", s, NULL);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
gwy_meta_browser_add_line(gpointer hkey,
                          GValue *value,
                          GSList **slist)
{
    FixupData *fd;
    GQuark quark;
    const gchar *val;
    gchar *s;

    g_return_if_fail(G_VALUE_HOLDS_STRING(value));
    quark = GPOINTER_TO_UINT(hkey);
    val = g_value_get_string(value);

    /* Theoretically, modules should assure metadata are in UTF-8 when it's
     * stored to container.  But in practice we cannot rely on it. */
    fd = g_new0(FixupData, 1);
    fd->quark = quark;
    if (g_utf8_validate(val, -1 , NULL)) {
        fd->value = (gchar*)val;
        fd->isok = TRUE;
    }
    else if ((s = g_locale_to_utf8(val, -1, NULL, NULL, NULL)))
        fd->value = s;
    *slist = g_slist_prepend(*slist, fd);
}

static void
gwy_meta_item_changed(GwyContainer *container,
                      const gchar *key,
                      MetadataBrowser *browser)
{
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    GQuark quark, q = 0;

    if (!g_str_has_prefix(key, "/meta/"))
        return;

    gwy_debug("Meta item <%s> changed", key);
    quark = g_quark_try_string(key);
    g_return_if_fail(quark);
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(browser->treeview));

    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gtk_tree_model_get(model, &iter, META_KEY, &q, -1);
            if (q == quark) {
                if (gwy_container_contains(container, quark)) {
                    path = gtk_tree_model_get_path(model, &iter);
                    gtk_tree_model_row_changed(model, path, &iter);
                    gtk_tree_path_free(path);
                }
                else
                    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
                return;
            }
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, META_KEY, quark, -1);
}

static void
gwy_meta_delete(MetadataBrowser *browser)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GQuark quark;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->treeview));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, META_KEY, &quark, -1);
    gwy_container_remove(browser->container, quark);
}

static void
gwy_meta_destroy(MetadataBrowser *browser)
{
    g_object_weak_unref(G_OBJECT(browser->container),
                        (GWeakNotify)&gwy_meta_data_finalized,
                        browser);
    g_object_set_data(G_OBJECT(browser->container), "metadata-browser", NULL);
    g_free(browser);
}

static void
gwy_meta_data_finalized(MetadataBrowser *browser)
{
    g_signal_handler_disconnect(browser->window, browser->container_id);
    gtk_widget_destroy(browser->window);
    g_free(browser);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

