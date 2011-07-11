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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodulebrowser.h>
#include "gwymoduleinternal.h"

static GtkWidget* gwy_module_browser_construct    (GtkWidget *parent);
static GtkWidget* gwy_module_browser_info_table   (GtkWidget *parent);
static GtkWidget* gwy_module_browser_failure_table(GtkWidget *parent);
static void       attach_info_line                (GtkWidget *table,
                                                   gint row,
                                                   const gchar *name,
                                                   GtkWidget *parent,
                                                   const gchar *key);
static void       update_module_info_cb           (GtkWidget *tree,
                                                   GtkWidget *parent);

enum {
    MODEL_NAME,
    MODEL_INFO,
    MODEL_FAILED,
    MODEL_LAST
};

static GtkWidget* window = NULL;

/**
 * gwy_module_browser:
 *
 * Shows a simple module browser.
 **/
void
gwy_module_browser(void)
{
    GtkWidget *browser, *scroll, *info, *vbox;

    if (window) {
        gtk_window_present(GTK_WINDOW(window));
        return;
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 480, 480);
    gtk_window_set_title(GTK_WINDOW(window), _("Module Browser"));
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    browser = gwy_module_browser_construct(window);
    gtk_container_add(GTK_CONTAINER(scroll), browser);

    info = gwy_module_browser_info_table(window);
    g_object_set_data(G_OBJECT(window), "mod-info", info);
    gtk_box_pack_start(GTK_BOX(vbox), info, FALSE, FALSE, 0);

    info = gwy_module_browser_failure_table(window);
    gtk_widget_set_no_show_all(info, TRUE);
    gtk_widget_hide(info);
    g_object_set_data(G_OBJECT(window), "fail-info", info);
    gtk_box_pack_start(GTK_BOX(vbox), info, FALSE, FALSE, 0);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_widget_destroy), NULL);
    g_signal_connect_swapped(window, "destroy",
                             G_CALLBACK(g_nullify_pointer), &window);
    gtk_widget_show_all(window);
}

static void
gwy_module_browser_store_module(const gchar *name,
                                GwyModuleInfo *mod_info,
                                GtkListStore *store)
{
    GtkTreeIter iter;
    gtk_list_store_insert_with_values(store, &iter, G_MAXINT,
                                      MODEL_NAME, name,
                                      MODEL_INFO, mod_info,
                                      MODEL_FAILED, FALSE,
                                      -1);
}

static void
gwy_module_browser_store_failure(G_GNUC_UNUSED const gchar *filename,
                                 _GwyModuleFailureInfo *fail_info,
                                 GtkListStore *store)
{
    GtkTreeIter iter;

    gtk_list_store_insert_with_values(store, &iter, G_MAXINT,
                                      MODEL_NAME, fail_info->modname,
                                      MODEL_INFO, fail_info,
                                      MODEL_FAILED, TRUE,
                                      -1);
}

static void
gwy_module_browser_render(G_GNUC_UNUSED GtkTreeViewColumn *column,
                          GtkCellRenderer *renderer,
                          GtkTreeModel *model,
                          GtkTreeIter *iter,
                          gpointer userdata)
{
    const gchar *name;
    const GwyModuleInfo *mod_info;
    const _GwyModuleFailureInfo *fail_info;
    gboolean failed;
    const gchar *text;

    gtk_tree_model_get(model, iter,
                       MODEL_NAME, &name,
                       /* We do not actually access the other one */
                       MODEL_INFO, &mod_info,
                       MODEL_INFO, &fail_info,
                       MODEL_FAILED, &failed,
                       -1);

    switch (GPOINTER_TO_UINT(userdata)) {
        case 0:
        text = name;
        break;

        case 1:
        text = failed ? "" : mod_info->version;
        break;

        case 2:
        text = failed ? "" : mod_info->author;
        break;

        default:
        g_return_if_reached();
        break;
    }
    g_object_set(renderer,
                 "text", text,
                 "foreground", failed ? "red" : "black",
                 "foreground-set", failed,
                 NULL);
}

static gint
compare_modules(GtkTreeModel *model,
                GtkTreeIter *a,
                GtkTreeIter *b,
                G_GNUC_UNUSED gpointer user_data)
{
    const gchar *namea, *nameb;
    gboolean faila, failb;

    gtk_tree_model_get(model, a, MODEL_NAME, &namea, MODEL_FAILED, &faila, -1);
    gtk_tree_model_get(model, b, MODEL_NAME, &nameb, MODEL_FAILED, &failb, -1);
    if (faila && !failb)
        return -1;
    if (failb && !faila)
        return 1;

    return strcmp(namea, nameb);
}


static GtkWidget*
gwy_module_browser_construct(GtkWidget *parent)
{
    static const gchar *columns[] = {
        N_("Module"), N_("Version"), N_("Author"),
    };

    GtkWidget *tree;
    GtkListStore *store;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    gsize i;

    store = gtk_list_store_new(MODEL_LAST,
                               G_TYPE_POINTER,
                               G_TYPE_POINTER,
                               G_TYPE_BOOLEAN);
    gwy_module_foreach((GHFunc)gwy_module_browser_store_module, store);
    _gwy_module_failure_foreach((GHFunc)gwy_module_browser_store_failure,
                                store);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
                                    MODEL_NAME, compare_modules, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), MODEL_NAME,
                                         GTK_SORT_ASCENDING);

    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
    g_object_unref(store);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_(columns[i]),
                                                          renderer, NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                gwy_module_browser_render,
                                                GUINT_TO_POINTER(i), NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    }

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    g_signal_connect(tree, "cursor-changed",
                     G_CALLBACK(update_module_info_cb), parent);

    return tree;
}

static GtkWidget*
gwy_module_browser_info_table(GtkWidget *parent)
{
    GtkWidget *table;
    gint i;

    table = gtk_table_new(7, 1, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 8);
    i = 0;
    attach_info_line(table, i++, _("Name-Version:"), parent, "name-version");
    attach_info_line(table, i++, _("File:"), parent, "file");
    attach_info_line(table, i++, _("Registered functions:"), parent, "funcs");
    attach_info_line(table, i++, _("Authors:"), parent, "author");
    attach_info_line(table, i++, _("Copyright:"), parent, "copy");
    attach_info_line(table, i++, _("Date:"), parent, "date");
    attach_info_line(table, i++, _("Description:"), parent, "desc");
    gtk_widget_show_all(table);

    return table;
}

static GtkWidget*
gwy_module_browser_failure_table(GtkWidget *parent)
{
    GtkWidget *table;
    gint i;

    table = gtk_table_new(3, 1, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 8);
    i = 0;
    attach_info_line(table, i++, _("Name:"), parent, "fail-name");
    attach_info_line(table, i++, _("File:"), parent, "fail-file");
    attach_info_line(table, i++, _("Failure:"), parent, "failure");
    gtk_widget_show_all(table);

    return table;
}

static void
update_module_info_cb(GtkWidget *tree,
                      GtkWidget *parent)
{
    GtkWidget *failed_widget, *module_widget;
    GtkLabel *label;
    GtkTreeModel *store;
    GtkTreeSelection *selection;
    const GwyModuleInfo *mod_info;
    const _GwyModuleFailureInfo *fail_info;
    const gchar *name;
    gboolean failed;
    GtkTreeIter iter;
    GSList *l;
    gchar *s;
    gsize n;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    g_return_if_fail(selection);
    if (!gtk_tree_selection_get_selected(selection, &store, &iter))
        return;

    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                       MODEL_NAME, &name,
                       /* We do not actually access the other one */
                       MODEL_INFO, &mod_info,
                       MODEL_INFO, &fail_info,
                       MODEL_FAILED, &failed,
                       -1);

    module_widget = g_object_get_data(G_OBJECT(parent), "mod-info");
    failed_widget = g_object_get_data(G_OBJECT(parent), "fail-info");
    if (failed) {
        gtk_widget_hide(module_widget);
        gtk_widget_show(failed_widget);

        label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "fail-name"));
        gtk_label_set_text(label, name);

        label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "fail-file"));
        gtk_label_set_text(label, fail_info->filename);

        label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "failure"));
        gtk_label_set_text(label, fail_info->message);

        return;
    }

    gtk_widget_hide(failed_widget);
    gtk_widget_show(module_widget);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "name-version"));
    s = g_strconcat(name, "-", mod_info->version, NULL);
    gtk_label_set_text(label, s);
    g_free(s);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "file"));
    gtk_label_set_text(label, gwy_module_get_filename(name));

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "author"));
    gtk_label_set_text(label, mod_info->author);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "copy"));
    gtk_label_set_text(label, mod_info->copyright);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "date"));
    gtk_label_set_text(label, mod_info->date);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "desc"));
    gtk_label_set_text(label, _(mod_info->blurb));

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "funcs"));
    n = 0;
    for (l = gwy_module_get_functions(name); l; l = g_slist_next(l))
        n += strlen((gchar*)l->data) + 1;
    if (!n)
        gtk_label_set_text(label, "");
    else {
        gchar *p;

        p = s = g_new(gchar, n);
        for (l = gwy_module_get_functions(name); l; l = g_slist_next(l)) {
            p = g_stpcpy(p, (gchar*)l->data);
            *(p++) = '\n';
        }
        *(--p) = '\0';
        gtk_label_set_text(label, s);
        g_free(s);
    }
}

static void
attach_info_line(GtkWidget *table,
                 gint row,
                 const gchar *name,
                 GtkWidget *parent,
                 const gchar *key)
{
    GtkWidget *label;
    gboolean multiline;

    multiline = (gwy_strequal(key, "desc")
                 || gwy_strequal(key, "funcs")
                 || gwy_strequal(key, "failure"));
    label = gtk_label_new(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, multiline ? 0.0 : 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, row, row+1);

    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, row, row+1);
    if (multiline)
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

    g_object_set_data(G_OBJECT(parent), key, label);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymodulebrowser
 * @title: gwymodulebrowser
 * @short_description: Gwyddion module browser
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
