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
#include <gtk/gtkwindow.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertext.h>

#include "gwymodulebrowser.h"

static void      gwy_module_browser_cell_renderer (GtkTreeViewColumn *column,
                                                   GtkCellRenderer *cell,
                                                   GtkTreeModel *model,
                                                   GtkTreeIter *piter,
                                                   gpointer data);
static GtkWidget* gwy_module_browser_construct    (void);
static void       gwy_hash_table_to_slist_cb      (gpointer key,
                                                   gpointer value,
                                                   gpointer user_data);
static gint       module_name_compare_cb          (_GwyModuleInfoInternal *a,
                                                   _GwyModuleInfoInternal *b);

enum {
    MODULE_MOD_INFO,
    MODULE_NAME,
    MODULE_LOADED,
    MODULE_FILENAME,
    MODULE_AUTHOR,
    MODULE_VERSION,
    MODULE_COPYRIGHT,
    MODULE_DATE,
    MODULE_LAST
};


/**
 * gwy_module_browser:
 *
 * Shows a simple module browser.
 **/
void
gwy_module_browser(void)
{
    GtkWidget *window, *browser;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Gwyddion Module Browser");
    gtk_window_set_wmclass(GTK_WINDOW(window), "browser_module",
                           g_get_application_name());
    browser = gwy_module_browser_construct();
    gtk_container_add(GTK_CONTAINER(window), browser);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show_all(window);
}

static GtkWidget*
gwy_module_browser_construct(void)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { "Module", MODULE_NAME },
        { "Loaded?", MODULE_LOADED },
        { "File", MODULE_FILENAME },
        { "Author", MODULE_AUTHOR },
        { "Version", MODULE_VERSION },
        { "Copyright", MODULE_COPYRIGHT },
        { "Date", MODULE_DATE },
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
                               G_TYPE_STRING,  /* file */
                               G_TYPE_STRING,  /* author */
                               G_TYPE_STRING,  /* version */
                               G_TYPE_STRING,  /* copyright */
                               G_TYPE_STRING   /* date */
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
    gtk_tree_selection_set_mode(select, GTK_SELECTION_NONE);

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
    gchar *s;
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

        case MODULE_COPYRIGHT:
        g_object_set(cell, "text", mod_info->copyright, NULL);
        break;

        case MODULE_DATE:
        g_object_set(cell, "text", mod_info->date, NULL);
        break;

        case MODULE_LOADED:
        g_object_set(cell, "text", iinfo->loaded ? "Yes" : "No", NULL);
        break;

        case MODULE_FILENAME:
        s = g_path_get_basename(iinfo->file);
        g_object_set(cell, "text", s, NULL);
        g_free(s);
        break;

        default:
        g_assert_not_reached();
        break;
    }
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
