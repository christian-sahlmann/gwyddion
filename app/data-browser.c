/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek, Chris Anderson
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, sidewinderasu@gmail.com.
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

//#define DEBUG 1

//#include <string.h>
//#include <libgwyddion/gwymacros.h>
//#include <libgwyddion/gwyutils.h>
//#include <libgwyddion/gwycontainer.h>
//#include <libprocess/datafield.h>
//#include <libgwydgets/gwydatawindow.h>
//#include <libgwydgets/gwydgetutils.h>
#include "config.h"
#include <gtk/gtk.h>
#include "data-browser.h"

typedef struct {
    GwyContainer *container;
    GtkListStore *channel_store;
} DataBrowser;

enum {
    VIS_COLUMN,
    TITLE_COLUMN,
    N_COLUMNS
};

static GtkWidget* gwy_browser_construct_channels(DataBrowser *browser);

void   gwy_browser_channel_toggled(GtkCellRendererToggle *cell_renderer,
                                   gchar *path_str,
                                   DataBrowser *browser);

/**
 * gwy_app_data_browser:
 * @data: A data container to be browsed.
 *
 * Creates and displays a data browser window. All data channels, graphs,
 * etc. within @data will be displayed.
 **/
void
gwy_app_data_browser(GwyContainer *data)
{
    DataBrowser *browser;
    GtkWidget *window, *notebook;
    GtkWidget *channels;
    GtkWidget *box_page;
    GtkWidget *label;
    const guchar *filename = NULL;
    gchar *base_name;
    gchar *window_title;

    g_return_if_fail(GWY_IS_CONTAINER(data));
    //g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    /* Setup browser structure */
    browser = g_new0(DataBrowser, 1);
    browser->container = data;

    /* Setup the window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 300);
    window_title = g_strdup("Data Browser");
    if (gwy_container_gis_string_by_name(data, "/filename", &filename)) {
        base_name = g_path_get_basename(filename);
        window_title = g_strconcat(window_title, ": ", base_name, NULL);
        g_free(base_name);
    }
    gtk_window_set_title(GTK_WINDOW(window), window_title);
    g_free(window_title);

    /* Create the notebook */
    notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(GTK_WINDOW(window)), notebook);

    /* Create the notebook tabs */
    channels = gwy_browser_construct_channels(browser);
    box_page = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_page), channels, FALSE, FALSE, 0);
    label = gtk_label_new("Data Channels");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_page, label);

    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new("Graphs");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_page, label);

    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new("Masks");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_page, label);

    /* Connect signals */
    //g_signal_connect(data_window, "destroy",
    //                 G_CALLBACK(gwy_app_data_window_remove), NULL);

    gtk_widget_show_all(window);
    gtk_window_present(GTK_WINDOW(window));
    //return window;
}

static GtkWidget* gwy_browser_construct_channels(DataBrowser *browser)
{
    GtkListStore *store;
    GtkTreeIter iter;
    GtkWidget *tree;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    gchar *channel_title = NULL;
    gint data_count;
    gint i;

    /* Create a list store to hold the channel data */
    store = gtk_list_store_new(N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING);
    browser->channel_store = store;

    /* Add channels to list store */
    data_count = gwy_browser_get_num_channels(browser->container);
    for (i=0; i<data_count; i++) {
        channel_title = gwy_browser_get_channel_title(browser->container, i);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, VIS_COLUMN, TRUE,
                                         TITLE_COLUMN, channel_title, -1);
        g_free(channel_title);
    }

    /* Construct the GtkTreeView that will display data channels */
    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);

    /* Add the "Visible" column */
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(G_OBJECT(renderer), "activatable", TRUE, NULL);
    g_signal_connect(renderer, "toggled",
                     G_CALLBACK(gwy_browser_channel_toggled),
                     browser);
    column = gtk_tree_view_column_new_with_attributes("Visible", renderer,
                                                      "active", VIS_COLUMN,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    /* Add the "Title" column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Title", renderer,
                                                      "text", TITLE_COLUMN,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    /* connect signals */
    //g_signal_connect(browser->container, "item-changed",
    //                 G_CALLBACK(gwy_browser_item_changed), browser);

    return tree;
}

void
gwy_browser_channel_toggled(GtkCellRendererToggle *cell_renderer,
                            gchar *path_str,
                            DataBrowser *browser)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model;
    gboolean enabled;

    path = gtk_tree_path_new_from_string(path_str);
    model = GTK_TREE_MODEL(browser->channel_store);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, VIS_COLUMN, &enabled, -1);
    enabled = !enabled;

    /*TODO: implement show/hide of windows here */

    gtk_list_store_set(browser->channel_store, &iter, VIS_COLUMN, enabled, -1);

    gtk_tree_path_free(path);
}



/**
 * gwy_browser_get_num_channels:
 * @data: A data container.
 *
 * Used to get the number of data channels stored within the @data
 * container.
 *
 * This value should be stored under the key "/data_count". If it can't be
 * found, a count of 1 will be returned.
 *
 * Returns: the number of channels as a #gint.
 **/
gint
gwy_browser_get_num_channels(GwyContainer *data)
{
    gint data_count;

    if (gwy_container_gis_int32_by_name(data,
                                        "/data_count",
                                        &data_count))
        return data_count;
    else
        return 1;
}

/**
 * gwy_browser_get_channel_title:
 * @data: A data container.
 * @channel: the data channel.
 *
 * Used to get the title of the given data channel stored within @data. If the
 * title can't be found, "Unknown Channel" will be returned.
 *
 * Returns: a new string containing the title (free it after use).
 **/
gchar*
gwy_browser_get_channel_title(GwyContainer *data, guint channel)
{
    gchar* channel_key;
    const guchar* channel_title = NULL;

    channel_key = g_strdup_printf("/%i/data/title", channel);
    gwy_container_gis_string_by_name(data, channel_key, &channel_title);

    /* Need to support "old" files (1.x) */
    if (!channel_title)
        gwy_container_gis_string_by_name(data, "/filename/title",
                                         &channel_title);

    if (channel_title)
        return g_strdup(channel_title);
    else
        return g_strdup("Unknown Channel");
}

/**
 * gwy_browser_get_channel_key:
 * @channel: the data channel.
 *
 * Used to automatically generate the appropriate container key for a given
 * data channel. (ie. channel=0 returns "/0/data", channel=1 returns "/1/data")
 *
 * Returns: a new string containing the key (free it after use).
 **/
gchar*
gwy_browser_get_channel_key(guint channel)
{
    gchar* channel_key;

    channel_key = g_strdup_printf("/%i/data", channel);

    return channel_key;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
