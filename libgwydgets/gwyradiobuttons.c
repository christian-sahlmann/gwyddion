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
#include "gwyradiobuttons.h"

/*XXX
 * gwy_option_menu_create:
 * @entries: Option menu entries.
 * @nentries: The number of entries.
 * @key: Value object data key.
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            no callback).
 * @cbdata: User data passed to the callback.
 * @current: Value to be shown as currently selected (-1 to use what happens
 *           to be first).
 *
 * Creates an option menu for an enum.
 *
 * It sets object data identified by @key for each menu item to its value.
 * Try to avoid -1 as an enum value.
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GSList*
gwy_radio_buttons_create(const GwyEnum *entries,
                         gint nentries,
                         const gchar *key,
                         GCallback callback,
                         gpointer cbdata,
                         gint current)
{
#if 0
    GtkWidget *omenu, *menu, *item;
    GList *c;
    GQuark quark;
    gint i, idx;

    quark = g_quark_from_string(key);
    omenu = gtk_option_menu_new();
    g_object_set_data(G_OBJECT(omenu), "gwy-option-menu",
                      GINT_TO_POINTER(TRUE));
    menu = gtk_menu_new();

    idx = -1;
    for (i = 0; i < nentries; i++) {
        item = gtk_menu_item_new_with_label(_(entries[i].name));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_qdata(G_OBJECT(item), quark,
                           GINT_TO_POINTER(entries[i].value));
        if (entries[i].value == current)
            idx = i;
    }
    gwy_debug("current: %d", idx);

    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (idx != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), idx);

    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    return omenu;
#endif
    return NULL;
}

/*XXX
 * gwy_option_menu_set_history:
 * @option_menu: An option menu created by gwy_option_menu_create().
 * @key: Value object data key.  Either the key you specified when called
 *       gwy_option_menu_create(), or the key listed in description of
 *       particular option menu constructor.
 * @current: Value to be shown as currently selected.
 *
 * Sets option menu history based on integer item object data (as set by
 * gwy_option_menu_create()).
 *
 * Returns: %TRUE if the history was set, %FALSE if @current was not found.
 **/
gboolean
gwy_radio_buttons_set_current(GSList *group,
                              const gchar *key,
                              gint current)
{
#if 0
    GQuark quark;
    GtkWidget *menu;
    GList *c;
    gint i;

    g_return_val_if_fail(GTK_IS_OPTION_MENU(option_menu), FALSE);
    g_return_val_if_fail(g_object_get_data(G_OBJECT(option_menu),
                                           "gwy-option-menu"), FALSE);
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
    quark = g_quark_from_string(key);
    i = 0;
    for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c)) {
        if (GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(c->data), quark))
            == current) {
            gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), i);
            return TRUE;
        }
        i++;
    }
#endif
    return FALSE;
}

/*XXX
 * gwy_option_menu_get_history:
 * @option_menu: An option menu created by gwy_option_menu_create().
 * @key: Value object data key.  Either the key you specified when called
 *       gwy_option_menu_create(), or the key listed in description of
 *       particular option menu constructor.
 *
 * Gets the integer enum value corresponding to currently selected item.
 *
 * Returns: The enum value corresponding to currently selected item.  In
 *          case of failure -1 is returned.
 **/
gint
gwy_radio_buttons_get_current(GSList *group,
                              const gchar *key)
{
#if 0
    GList *c;
    GQuark quark;
    GtkWidget *menu, *item;
    gint idx;

    g_return_val_if_fail(GTK_IS_OPTION_MENU(option_menu), -1);
    g_return_val_if_fail(g_object_get_data(G_OBJECT(option_menu),
                                           "gwy-option-menu"), -1);

    idx = gtk_option_menu_get_history(GTK_OPTION_MENU(option_menu));
    if (idx < 0)
        return -1;
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
    quark = g_quark_from_string(key);
    c = g_list_nth(GTK_MENU_SHELL(menu)->children, (guint)idx);
    g_return_val_if_fail(c, FALSE);
    item = GTK_WIDGET(c->data);

    return GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(item), quark));
#endif
    return -1;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
