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

#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include "app.h"
#include "menu-windowlist.h"

typedef struct {
    gulong hook_id;
    gchar *none_label;
} MenuInfo;

enum {
    THUMBNAIL_SIZE = 16
};

static GQuark omenu_data_window_key = 0;
static GQuark omenu_data_window_id_key = 0;
static GQuark omenu_data_window_info_key = 0;

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
 * It sets object data "data-window" to data window pointer for each menu
 * item.
 *
 * Note the menu is currently only safe to use in modal dialogs only because
 * it is static and does NOT react to creation or closing of data windows.
 * However, it probably will react to it in the future, so make no
 * assupmtions.
 *
 * Returns: The newly created option menu as a #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_data_window(GCallback callback,
                            gpointer cbdata,
                            const gchar *none_label,
                            GtkWidget *current)
{
    GtkWidget *omenu, *menu, *item;
    MenuInfo *info;
    gulong id;
    GList *c;

    if (!omenu_data_window_key)
        omenu_data_window_key = g_quark_from_static_string("data-window");
    if (!omenu_data_window_id_key)
        omenu_data_window_id_key
            = g_quark_from_static_string("gwy-option-menu-data-window");
    if (!omenu_data_window_info_key)
        omenu_data_window_info_key
            = g_quark_from_static_string("gwy-option-menu-data-window-info");

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
    info = g_new(MenuInfo, 1);
    info->hook_id = id;
    info->none_label = g_strdup(none_label);
    g_object_set_qdata(G_OBJECT(omenu), omenu_data_window_info_key, info);
    g_signal_connect(omenu, "destroy",
                     G_CALLBACK(gwy_option_menu_data_window_destroy), NULL);

    return omenu;
}

static void
gwy_option_menu_data_window_append(GwyDataWindow *data_window,
                                   GtkWidget *menu)
{
    GtkWidget *item, *data_view, *image, *label, *hbox;
    GdkPixbuf *pixbuf;
    gchar *filename;

    gwy_debug("adding %p to %p", data_window, menu);
    data_view = gwy_data_window_get_data_view(data_window);
    filename = gwy_data_window_get_base_name(data_window);

    pixbuf = gwy_data_view_get_thumbnail(GWY_DATA_VIEW(data_view),
                                         THUMBNAIL_SIZE);
    image = gtk_image_new_from_pixbuf(pixbuf);
    gwy_object_unref(pixbuf);
    hbox = gtk_hbox_new(FALSE, 6);
    item = gtk_menu_item_new();
    label = gtk_label_new(filename);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(item), hbox);
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
gwy_option_menu_data_window_update(G_GNUC_UNUSED GtkWidget *omenu)
{
    /*
    MenuInfo *info;
    GtkWidget *menu, *item;
    */

    gwy_debug("would update option menu %p, but don't know how", omenu);
    /*
    info = g_object_get_qdata(G_OBJECT(omenu), omenu_data_window_info_key);
    g_assert(info);

    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    item = gtk_menu_item_new_with_label("Foobar");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    item = gtk_menu_item_new_with_label("Quux");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gwy_debug("%d", g_list_length(GTK_MENU_SHELL(menu)->children));
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                   NULL, NULL, 0, gtk_get_current_event_time());
    */
}

static void
gwy_option_menu_data_window_destroy(GtkWidget *omenu)
{
    MenuInfo *info;

    gwy_debug("destroying: %p", omenu);

    info = (MenuInfo*)g_object_get_qdata(G_OBJECT(omenu),
                                         omenu_data_window_info_key);
    g_assert(info->hook_id);
    gwy_app_data_window_list_remove_hook(GPOINTER_TO_UINT(info->hook_id));
    g_free(info->none_label);
    g_free(info);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
