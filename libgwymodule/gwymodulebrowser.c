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
#include <gtk/gtk.h>

#include "gwymoduleinternal.h"
#include "gwymodulebrowser.h"

static void      gwy_module_browser_cell_renderer (GtkTreeViewColumn *column,
                                                   GtkCellRenderer *cell,
                                                   GtkTreeModel *model,
                                                   GtkTreeIter *piter,
                                                   gpointer data);
static GtkWidget* gwy_module_browser_construct    (GtkWidget *parent);
static GtkWidget* gwy_module_browser_info_table   (GtkWidget *parent);
static void       attach_info_line                (GtkWidget *table,
                                                   gint row,
                                                   const gchar *name,
                                                   GtkWidget *parent,
                                                   const gchar *key);
static void       update_module_info_cb           (GtkWidget *tree,
                                                   GtkWidget *parent);
static void       gwy_hash_table_to_slist_cb      (gpointer key,
                                                   gpointer value,
                                                   gpointer user_data);
static gint       module_name_compare_cb          (_GwyModuleInfoInternal *a,
                                                   _GwyModuleInfoInternal *b);

enum {
    MODULE_MOD_INFO,
    MODULE_NAME,
    MODULE_LOADED,
    MODULE_VERSION,
    MODULE_AUTHOR,
    MODULE_LAST
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
    GtkWidget *browser, *scroll, *vbox, *info;

    if (window) {
        gtk_window_present(GTK_WINDOW(window));
        return;
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 480, 360);
    gtk_window_set_title(GTK_WINDOW(window), "Gwyddion Module Browser");
    gtk_window_set_wmclass(GTK_WINDOW(window), "browser_module",
                           g_get_application_name());
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    browser = gwy_module_browser_construct(window);
    gtk_container_add(GTK_CONTAINER(scroll), browser);
    info = gwy_module_browser_info_table(window);
    gtk_box_pack_start(GTK_BOX(vbox), info, FALSE, FALSE, 0);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_widget_destroy), NULL);
    g_signal_connect_swapped(window, "destroy",
                             G_CALLBACK(g_nullify_pointer), &window);
    gtk_widget_show_all(window);
}

static GtkWidget*
gwy_module_browser_construct(GtkWidget *parent)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { "Module", MODULE_NAME },
        { "Loaded?", MODULE_LOADED },
        { "Version", MODULE_VERSION },
        { "Author", MODULE_AUTHOR },
    };

    GtkWidget *tree;
    GtkListStore *store;
    GtkTreeSelection *select;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GSList *l, *list = NULL;
    GtkTreeIter iter;
    gsize i;

    store = gtk_list_store_new(MODULE_LAST,
                               G_TYPE_POINTER, /* module info itself */
                               G_TYPE_STRING,  /* name */
                               G_TYPE_STRING,  /* loaded? */
                               G_TYPE_STRING,  /* version */
                               G_TYPE_STRING   /* author */
                              );

    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gwy_module_foreach((GHFunc)gwy_hash_table_to_slist_cb, &list);
    list = g_slist_sort(list, (GCompareFunc)module_name_compare_cb);
    for (l = list; l; l = g_slist_next(l)) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, MODULE_MOD_INFO, l->data, -1);
    }
    g_slist_free(list);
    g_object_unref(store);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(columns[i].title,
                                                          renderer,
                                                          "text", columns[i].id,
                                                          NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                gwy_module_browser_cell_renderer,
                                                GUINT_TO_POINTER(columns[i].id),
                                                NULL);  /* destroy notify */
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    }

    select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

    g_signal_connect(G_OBJECT(tree), "cursor-changed",
                     G_CALLBACK(update_module_info_cb), parent);

    return tree;
}

static void
gwy_module_browser_cell_renderer(GtkTreeViewColumn *column,
                                 GtkCellRenderer *cell,
                                 GtkTreeModel *model,
                                 GtkTreeIter *piter,
                                 gpointer data)
{
    _GwyModuleInfoInternal *iinfo;
    GwyModuleInfo *mod_info;
    gulong id;

    id = GPOINTER_TO_UINT(data);
    g_assert(id > MODULE_MOD_INFO && id < MODULE_LAST);
    gtk_tree_model_get(model, piter, MODULE_MOD_INFO, &iinfo, -1);
    mod_info = iinfo->mod_info;
    switch (id) {
        case MODULE_NAME:
        g_object_set(cell, "text", mod_info->name, NULL);
        break;

        case MODULE_AUTHOR:
        g_object_set(cell, "text", mod_info->author, NULL);
        break;

        case MODULE_VERSION:
        g_object_set(cell, "text", mod_info->version, NULL);
        break;

        case MODULE_LOADED:
        g_object_set(cell, "text", iinfo->loaded ? "Yes" : "No", NULL);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static GtkWidget*
gwy_module_browser_info_table(GtkWidget *parent)
{
    GtkWidget *table, *align;
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

    align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
    gtk_container_add(GTK_CONTAINER(align), table);

    return align;
}

static void
update_module_info_cb(GtkWidget *tree,
                      GtkWidget *parent)
{
    GtkLabel *label;
    GtkTreeModel *store;
    GtkTreeSelection *select;
    _GwyModuleInfoInternal *iinfo;
    GtkTreeIter iter;
    GSList *l;
    gchar *s;
    gsize n;

    select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    g_return_if_fail(select);
    if (!gtk_tree_selection_get_selected(select, &store, &iter))
        return;

    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, MODULE_MOD_INFO, &iinfo,
                       -1);
    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "name-version"));
    s = g_strconcat(iinfo->mod_info->name, "-", iinfo->mod_info->version, NULL);
    gtk_label_set_text(label, s);
    g_free(s);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "file"));
    gtk_label_set_text(label, iinfo->file);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "author"));
    gtk_label_set_text(label, iinfo->mod_info->author);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "copy"));
    gtk_label_set_text(label, iinfo->mod_info->copyright);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "date"));
    gtk_label_set_text(label, iinfo->mod_info->date);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "desc"));
    gtk_label_set_text(label, iinfo->mod_info->blurb);

    label = GTK_LABEL(g_object_get_data(G_OBJECT(parent), "funcs"));
    n = 0;
    for (l = iinfo->funcs; l; l = g_slist_next(l))
        n += strlen((gchar*)l->data) + 1;
    if (!n)
        gtk_label_set_text(label, "");
    else {
        gchar *p;

        s = g_new(gchar, n);
        for (l = iinfo->funcs, p = s; l; l = g_slist_next(l)) {
            p = g_stpcpy(p, (gchar*)l->data);
            *(p++) = ' ';
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

    multiline = (strcmp(key, "desc") == 0) || (strcmp(key, "funcs") == 0);
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

static void
gwy_hash_table_to_slist_cb(gpointer key,
                           gpointer value,
                           gpointer user_data)
{
    GSList **list = (GSList**)user_data;

    *list = g_slist_prepend(*list, value);
}

static gint
module_name_compare_cb(_GwyModuleInfoInternal *a,
                       _GwyModuleInfoInternal *b)
{
    return strcmp(a->mod_info->name, b->mod_info->name);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
