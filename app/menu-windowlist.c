/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include "app.h"
#include "menu-windowlist.h"

enum {
    THUMBNAIL_SIZE = 16
};

static GQuark omenu_data_window_key = 0;
static GQuark omenu_data_window_id_key = 0;
static GQuark omenu_data_window_hook_id_key = 0;

static void       gwy_option_menu_data_window_append(GwyDataWindow *data_window,
                                                     GtkWidget *menu);
static void       gwy_option_menu_data_window_update(GtkWidget *omenu);
static void       gwy_option_menu_data_window_destroy(GtkWidget *omenu);

/**
 * gwy_option_menu_data_window:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            no callback).
 * @cbdata: User data passed to the callback.
 * @none_label: Label to use for `none' menu item.  If it is %NULL, no `none'
 *              item is created, if it is empty, a default label is used.
 * @current: Data window to be shown as currently selected.
 *
 * Creates an option menu of existing data windows, with thumbnails.
 *
 * It sets object data "data-window" to data window for each menu item.
 * Note the menu is static and does NOT react to creation or closing of
 * data windows.
 *
 * Returns: The newly created option menu as a #GtkWidget.
 *
 * Since: 1.2.
 **/
GtkWidget*
gwy_option_menu_data_window(GCallback callback,
                            gpointer cbdata,
                            const gchar *none_label,
                            GtkWidget *current)
{
    GtkWidget *omenu, *menu, *item;
    gulong id;
    GList *c;

    if (!omenu_data_window_key)
        omenu_data_window_key = g_quark_from_static_string("data-window");
    if (!omenu_data_window_id_key)
        omenu_data_window_id_key
            = g_quark_from_static_string("gwy-option-menu-data-window");
    if (!omenu_data_window_hook_id_key)
        omenu_data_window_hook_id_key
            = g_quark_from_static_string("gwy-option-menu-data-window-hook-id");

    omenu = gtk_option_menu_new();
    g_object_set_qdata(G_OBJECT(omenu), omenu_data_window_id_key,
                       GINT_TO_POINTER(TRUE));
    menu = gtk_menu_new();
    gwy_app_data_window_foreach((GFunc)gwy_option_menu_data_window_append,
                                menu);
    if (none_label) {
        if (!*none_label)
            none_label = _("(none)");
        item = gtk_menu_item_new_with_label(none_label);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (current || none_label)
        gwy_option_menu_data_window_set_history(omenu, current);

    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    id = gwy_app_data_window_list_add_hook(gwy_option_menu_data_window_update,
                                           omenu);
    g_assert(id);
    g_object_set_qdata(G_OBJECT(omenu), omenu_data_window_hook_id_key,
                       GUINT_TO_POINTER(id));
    g_signal_connect(omenu, "destroy",
                     G_CALLBACK(gwy_option_menu_data_window_destroy), NULL);

    return omenu;
}

static void
gwy_option_menu_data_window_append(GwyDataWindow *data_window,
                                   GtkWidget *menu)
{
    GtkWidget *item, *data_view, *image;
    GdkPixbuf *pixbuf;
    gchar *filename;

    data_view = gwy_data_window_get_data_view(data_window);
    filename = gwy_data_window_get_base_name(data_window);

    pixbuf = gwy_data_view_get_thumbnail(GWY_DATA_VIEW(data_view),
                                         THUMBNAIL_SIZE);
    image = gtk_image_new_from_pixbuf(pixbuf);
    gwy_object_unref(pixbuf);
    item = gtk_image_menu_item_new_with_label(filename);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_qdata(G_OBJECT(item), omenu_data_window_key, data_window);
    g_free(filename);
}

/**
 * gwy_option_menu_data_window_set_history:
 * @option_menu: An option menu created by gwy_option_menu_data_window().
 * @current: Data window to be shown as currently selected.
 *
 * Sets data window option menu history to a specific data window.
 *
 * Returns: %TRUE if the history was set, %FALSE if @current was not found.
 *
 * Since: 1.2.
 **/
gboolean
gwy_option_menu_data_window_set_history(GtkWidget *option_menu,
                                        GtkWidget *current)
{
    GtkWidget *menu;
    GtkOptionMenu *omenu;
    GList *c;
    gint i;

    g_return_val_if_fail(GTK_IS_OPTION_MENU(option_menu), FALSE);
    g_return_val_if_fail(g_object_get_qdata(G_OBJECT(option_menu),
                                            omenu_data_window_id_key), FALSE);

    omenu = GTK_OPTION_MENU(option_menu);
    g_assert(omenu);
    menu = gtk_option_menu_get_menu(omenu);
    i = 0;
    for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c)) {
        if (g_object_get_qdata(G_OBJECT(c->data), omenu_data_window_key)
            == current) {
            gtk_option_menu_set_history(omenu, i);
            return TRUE;
        }
        i++;
    }

    return FALSE;
}

/**
 * gwy_option_menu_data_window_get_history:
 * @option_menu: An option menu created by gwy_option_menu_data_window().
 *
 * Gets the currently selected data window in a data window option menu.
 *
 * Returns: The currently selected data window (may be %NULL if `none' is
 *          selected).
 *
 * Since: 1.2.
 **/
GtkWidget*
gwy_option_menu_data_window_get_history(GtkWidget *option_menu)
{
    GList *c;
    GtkWidget *menu, *item;
    GtkOptionMenu *omenu;
    gint idx;

    g_return_val_if_fail(GTK_IS_OPTION_MENU(option_menu), NULL);
    g_return_val_if_fail(g_object_get_qdata(G_OBJECT(option_menu),
                                            omenu_data_window_id_key), NULL);

    omenu = GTK_OPTION_MENU(option_menu);
    g_assert(omenu);
    idx = gtk_option_menu_get_history(omenu);
    if (idx < 0)
        return NULL;
    menu = gtk_option_menu_get_menu(omenu);
    c = g_list_nth(GTK_MENU_SHELL(menu)->children, (guint)idx);
    g_return_val_if_fail(c, NULL);
    item = GTK_WIDGET(c->data);

    return (GtkWidget*)g_object_get_qdata(G_OBJECT(item),
                                          omenu_data_window_key);
}

static void
gwy_option_menu_data_window_update(GtkWidget *omenu)
{
    gwy_debug("would update: %p", omenu);
}

static void
gwy_option_menu_data_window_destroy(GtkWidget *omenu)
{
    gpointer id;

    gwy_debug("destroying: %p", omenu);

    id = g_object_get_qdata(G_OBJECT(omenu), omenu_data_window_hook_id_key);
    g_assert(id);
    gwy_app_data_window_list_remove_hook(GPOINTER_TO_UINT(id));
}

