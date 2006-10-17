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

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/data-browser.h>

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
    GwyContainer *meta;
    gulong destroy_id;
    GtkWidget *window;
    GtkWidget *treeview;
    GtkWidget *new;
    GtkWidget *delete;
    GtkWidget *close;
} MetadataBrowser;

static void       gwy_meta_browser_construct    (MetadataBrowser *browser);
static MetadataBrowser* gwy_meta_switch_data    (MetadataBrowser *browser,
                                                 GwyContainer *data,
                                                 gint id);
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
                                                 GQuark quark,
                                                 MetadataBrowser *browser);
static void       gwy_meta_new_item             (MetadataBrowser *browser);
static void       gwy_meta_delete_item          (MetadataBrowser *browser);
static void       gwy_meta_destroy              (MetadataBrowser *browser);
static void       gwy_meta_data_finalized       (MetadataBrowser *browser);
static void       gwy_meta_focus_iter           (MetadataBrowser *browser,
                                                 GtkTreeIter *iter);
static gboolean   gwy_meta_find_key             (MetadataBrowser *browser,
                                                 GQuark quark,
                                                 GtkTreeIter *iter);

/**
 * gwy_app_metadata_browser:
 * @data: A data window to show metadata of.
 *
 * Shows a simple metadata browser.
 **/
void
gwy_app_metadata_browser(GwyContainer *data,
                         gint id)
{
    MetadataBrowser *browser;
    GtkWidget *scroll, *vbox, *hbox;
    GtkRequisition request;

    g_return_if_fail(GWY_IS_CONTAINER(data));
    if ((browser = gwy_meta_switch_data(NULL, data, id))) {
        gtk_window_present(GTK_WINDOW(browser->window));
        return;
    }

    browser = g_new0(MetadataBrowser, 1);
    gwy_meta_browser_construct(browser);
    browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_widget_size_request(browser->treeview, &request);
    request.width = MAX(request.width, 120);
    request.height = MAX(request.height, 200);
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
    browser->destroy_id
        = g_signal_connect_swapped(browser->window, "destroy",
                                   G_CALLBACK(gwy_meta_destroy), browser);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    browser->new = gwy_stock_like_button_new(_("_New"), GTK_STOCK_NEW);
    gtk_box_pack_start(GTK_BOX(hbox), browser->new, TRUE, TRUE, 0);
    g_signal_connect_swapped(browser->new, "clicked",
                             G_CALLBACK(gwy_meta_new_item), browser);

    browser->delete = gwy_stock_like_button_new(_("_Delete"), GTK_STOCK_DELETE);
    gtk_box_pack_start(GTK_BOX(hbox), browser->delete, TRUE, TRUE, 0);
    g_signal_connect_swapped(browser->delete, "clicked",
                             G_CALLBACK(gwy_meta_delete_item), browser);

    browser->close = gwy_stock_like_button_new(_("_Close"), GTK_STOCK_CLOSE);
    gtk_box_pack_start(GTK_BOX(hbox), browser->close, TRUE, TRUE, 0);
    g_signal_connect_swapped(browser->close, "clicked",
                             G_CALLBACK(gtk_widget_destroy), browser->window);

    gwy_meta_switch_data(browser, data, id);
    gtk_widget_show_all(browser->window);
}

static gint
gwy_meta_sort_func(GtkTreeModel *model,
                   GtkTreeIter *a,
                   GtkTreeIter *b,
                   G_GNUC_UNUSED gpointer userdata)
{
    GQuark qa, qb;

    gtk_tree_model_get(model, a, META_KEY, &qa, -1);
    gtk_tree_model_get(model, b, META_KEY, &qb, -1);
    return g_utf8_collate(g_quark_to_string(qa), g_quark_to_string(qb));
}

static void
gwy_meta_update_title(MetadataBrowser *browser,
                      GwyContainer *data,
                      gint id)
{
    gchar *title, *dataname;

    dataname = gwy_app_get_data_field_title(data, id);
    title = g_strdup_printf(_("Metadata of %s (%s)"),
                            dataname, g_get_application_name());
    gtk_window_set_title(GTK_WINDOW(browser->window), title);
    g_free(title);
    g_free(dataname);
}

static MetadataBrowser*
gwy_meta_switch_data(MetadataBrowser *browser,
                     GwyContainer *data,
                     gint id)
{
    GwyContainer *meta;
    GtkListStore *store;
    GtkTreeIter iter;
    GSList *fixlist, *l;
    gchar key[24];

    g_snprintf(key, sizeof(key), "/%d/meta", id);
    if (gwy_container_gis_object_by_name(data, key, &meta)) {
        if (!browser)
            browser = g_object_get_data(G_OBJECT(meta), "metadata-browser");
        if (!browser || browser->meta == meta)
            return browser;

        fixlist = NULL;
        store = gtk_list_store_new(1, G_TYPE_UINT);
        gwy_container_foreach(meta, NULL,
                              (GHFunc)(gwy_meta_browser_add_line), &fixlist);
        /* Commit UTF-8 fixes found by gwy_meta_browser_add_line() */
        for (l = fixlist; l; l = g_slist_next(l)) {
            FixupData *fd = (FixupData*)l->data;

            if (fd->isok || fd->value) {
                if (!fd->isok)
                    gwy_container_set_string(meta, fd->quark, fd->value);
                gtk_list_store_insert_with_values(store, &iter, G_MAXINT,
                                                  META_KEY, fd->quark,
                                                  -1);
            }
            else
                gwy_container_remove(meta, fd->quark);
            g_free(fd);
        }
        g_slist_free(fixlist);
    }
    else {
        if (!browser)
            return NULL;

        meta = gwy_container_new();
        gwy_container_set_object_by_name(data, key, meta);
        g_object_unref(meta);
        store = gtk_list_store_new(1, G_TYPE_UINT);
    }

    if (browser->meta) {
        g_object_set_data(G_OBJECT(browser->meta), "metadata-browser", NULL);
        g_object_weak_unref(G_OBJECT(browser->meta),
                            (GWeakNotify)&gwy_meta_data_finalized,
                            browser);
    }
    browser->meta = meta;
    g_object_set_data(G_OBJECT(browser->meta), "metadata-browser", browser);
    g_object_weak_ref(G_OBJECT(browser->meta),
                      (GWeakNotify)&gwy_meta_data_finalized,
                      browser);

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), 0,
                                    gwy_meta_sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), META_KEY,
                                         GTK_SORT_ASCENDING);

    g_signal_connect(meta, "item-changed",
                     G_CALLBACK(gwy_meta_item_changed), browser);

    gtk_tree_view_set_model(GTK_TREE_VIEW(browser->treeview),
                            GTK_TREE_MODEL(store));
    g_object_unref(store);

    gwy_meta_update_title(browser, data, id);

    return browser;
}

static void
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

    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    gsize i;

    browser->treeview = gtk_tree_view_new();
    treeview = GTK_TREE_VIEW(browser->treeview);
    gtk_tree_view_set_rules_hint(treeview, TRUE);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, NULL);
        column = gtk_tree_view_column_new_with_attributes(_(columns[i].title),
                                                          renderer, NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                gwy_meta_browser_cell_renderer,
                                                browser, NULL);
        gtk_tree_view_append_column(treeview, column);
        g_object_set_data(G_OBJECT(renderer), "column",
                          GUINT_TO_POINTER(columns[i].id));
        g_signal_connect(renderer, "edited",
                         G_CALLBACK(gwy_meta_cell_edited), browser);
    }

    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    browser->treeview = GTK_WIDGET(treeview);
}

static gboolean
gwy_meta_validate_key(const gchar *key)
{
    if (!key || !*key)
        return FALSE;

    while (*key) {
        gchar c = *key;

        if (c < ' ' || c == '/' || c == '<' || c == '>' || c == '&' || c == 127)
            return FALSE;
        key++;
    }
    return TRUE;
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
    GQuark oldkey, quark;
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
        if (gwy_meta_validate_key(text)) {
            quark = g_quark_from_string(text);
            gwy_container_rename(browser->meta, oldkey, quark, FALSE);
        }
        break;

        case META_VALUE:
        if (pango_parse_markup(text, -1, 0, NULL, NULL, NULL, NULL))
            gwy_container_set_string(browser->meta, oldkey,
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
    MetadataBrowser *browser = (MetadataBrowser*)userdata;
    const gchar *s;
    GQuark quark;
    gulong id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(renderer), "column"));
    gtk_tree_model_get(model, iter, META_KEY, &quark, -1);
    switch (id) {
        case META_KEY:
        g_object_set(renderer, "markup", g_quark_to_string(quark), NULL);
        break;

        case META_VALUE:
        s = gwy_container_get_string(browser->meta, quark);
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
    if (g_utf8_validate(val, -1, NULL)) {
        fd->value = (gchar*)val;
        fd->isok = TRUE;
    }
    else if ((s = g_locale_to_utf8(val, -1, NULL, NULL, NULL)))
        fd->value = s;

    /* The same applies to markup validity.  Fix invalid markup by taking it
     * literally. */
    if (!pango_parse_markup(fd->value, -1, 0, NULL, NULL, NULL, NULL)) {
        s = g_markup_escape_text(fd->value, -1);
        if (!fd->isok)
            g_free(fd->value);
        fd->value = s;
        fd->isok = FALSE;
    }

    *slist = g_slist_prepend(*slist, fd);
}

static void
gwy_meta_item_changed(GwyContainer *container,
                      GQuark quark,
                      MetadataBrowser *browser)
{
    GtkListStore *store;
    GtkTreeView *treeview;
    GtkTreeIter iter;
    const gchar *key;

    key = g_quark_to_string(quark);
    gwy_debug("Meta item <%s> changed", key);
    g_return_if_fail(quark);
    treeview = GTK_TREE_VIEW(browser->treeview);
    store = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));

    if (gwy_meta_find_key(browser, quark, &iter)) {
        if (gwy_container_contains(container, quark))
            gwy_list_store_row_changed(store, &iter, NULL, -1);
        else
            gtk_list_store_remove(store, &iter);
        return;
    }

    gtk_list_store_insert_with_values(store, &iter, G_MAXINT, META_KEY, quark,
                                      -1);
    gwy_meta_focus_iter(browser, &iter);
}

static void
gwy_meta_new_item(MetadataBrowser *browser)
{
    static const gchar *whatever[] = {
        "angary", "bistere", "couchant", "dolerite", "envoy", "figwort",
        "gudgeon", "hidalgo", "ictus", "jibbah", "kenosis", "logie",
        "maser", "nephology", "ozalid", "parallax", "reduit", "savate",
        "thyristor", "urate", "versicle", "wapentake", "xystus",
        "yogh", "zeugma",
    };
    GtkTreeIter iter;
    static GQuark quark = 0;
    gchar *s;

    if (!quark) {
        s = g_strdup(_("New item"));
        quark = g_quark_from_string(s);
        g_free(s);
    }

    if (gwy_container_contains(browser->meta, quark)) {
        if (gwy_meta_find_key(browser, quark, &iter))
            gwy_meta_focus_iter(browser, &iter);
    }
    else {
        if (g_random_int() % 4 == 0)
            s = g_strdup(whatever[g_random_int() % G_N_ELEMENTS(whatever)]);
        else
            s = g_strdup("");
        gwy_container_set_string(browser->meta, quark, s);
    }
}

static void
gwy_meta_delete_item(MetadataBrowser *browser)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GQuark quark;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->treeview));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, META_KEY, &quark, -1);
    gwy_container_remove(browser->meta, quark);
}

static void
gwy_meta_destroy(MetadataBrowser *browser)
{
    g_object_set_data(G_OBJECT(browser->meta), "metadata-browser", NULL);
    g_object_weak_unref(G_OBJECT(browser->meta),
                        (GWeakNotify)&gwy_meta_data_finalized,
                        browser);
    g_free(browser);
}

static void
gwy_meta_data_finalized(MetadataBrowser *browser)
{
    g_signal_handler_disconnect(browser->window, browser->destroy_id);
    gtk_widget_destroy(browser->window);
    g_free(browser);
}

static void
gwy_meta_focus_iter(MetadataBrowser *browser,
                    GtkTreeIter *iter)
{
    GtkTreeView *treeview;
    GtkTreeViewColumn *column;
    GtkTreePath *path;

    treeview = GTK_TREE_VIEW(browser->treeview);
    path = gtk_tree_model_get_path(gtk_tree_view_get_model(treeview), iter);
    gtk_tree_view_scroll_to_cell(treeview, path, NULL, FALSE, 0.0, 0.0);
    column = gtk_tree_view_get_column(treeview, META_KEY);
    gtk_tree_view_set_cursor(treeview, path, column, FALSE);
    gtk_widget_grab_focus(browser->treeview);
    gtk_tree_path_free(path);
}

static gboolean
gwy_meta_find_key(MetadataBrowser *browser,
                  GQuark quark,
                  GtkTreeIter *iter)
{
    GtkTreeModel *model;
    GQuark q;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(browser->treeview));
    if (gtk_tree_model_get_iter_first(model, iter)) {
        do {
            gtk_tree_model_get(model, iter, META_KEY, &q, -1);
            if (q == quark)
                return TRUE;
        } while (gtk_tree_model_iter_next(model, iter));
    }

    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

