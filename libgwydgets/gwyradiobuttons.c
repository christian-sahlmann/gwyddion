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

/**
 * gwy_radio_buttons_create:
 * @entries: Radio button group items.
 * @nentries: The number of items.
 * @key: Value object data key.
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            no callback).
 * @cbdata: User data passed to the callback.
 * @current: Value to be shown as currently selected (-1 to use what happens
 *           to be first).
 *
 * Creates a radio button group for an enum.
 *
 * It sets object data identified by @key for each menu item to its value.
 * Try to avoid -1 as an enum value.
 *
 * Returns: The newly created radio button group (a #GSList).
 **/
GSList*
gwy_radio_buttons_create(const GwyEnum *entries,
                         gint nentries,
                         const gchar *key,
                         GCallback callback,
                         gpointer cbdata,
                         gint current)
{
    GtkWidget *button, *curbutton;
    GSList *group;
    GQuark quark;
    gint i;

    quark = g_quark_from_string(key);

    button = curbutton = NULL;
    for (i = 0; i < nentries; i++) {
        button = gtk_radio_button_new_with_mnemonic_from_widget
                               (GTK_RADIO_BUTTON(button), _(entries[i].name));
        g_object_set_qdata(G_OBJECT(button), quark,
                           GINT_TO_POINTER(entries[i].value));
        if (entries[i].value == current)
            curbutton = button;
    }
    gwy_debug("current: %p", curbutton);

    if (curbutton)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(curbutton), TRUE);

    if (callback) {
        for (group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
             group;
             group = g_slist_next(group))
            g_signal_connect(group->data, "clicked", callback, cbdata);
    }

    return gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
}

/**
 * gwy_radio_buttons_set_current:
 * @group: A radio button group created by gwy_radio_buttons_create().
 * @key: Value object data key (specified as @key when called
 *       gwy_radio_buttons_create()).
 * @current: Value to be shown as currently selected.
 *
 * Sets currently selected radio button in @group based on integer item object
 * data (as set by gwy_radio_buttons_create()).
 *
 * Returns: %TRUE if current button was set, %FALSE if @current was not found.
 **/
gboolean
gwy_radio_buttons_set_current(GSList *group,
                              const gchar *key,
                              gint current)
{
    GQuark quark;

    g_return_val_if_fail(group, FALSE);
    quark = g_quark_from_string(key);
    while (group) {
        g_return_val_if_fail(GTK_IS_RADIO_BUTTON(group->data), FALSE);
        if (GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(group->data), quark))
            == current) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(group->data), TRUE);
            return TRUE;
        }
        group = g_slist_next(group);
    }

    return FALSE;
}

/**
 * gwy_radio_buttons_set_current_from_widget:
 * @widget: A member of a radio button group created by
 *          gwy_radio_buttons_create().
 * @key: Value object data key (specified as @key when called
 *       gwy_radio_buttons_create()).
 * @current: Value to be shown as currently selected.
 *
 * Sets currently selected radio button in @group based on integer item object
 * data (as set by gwy_radio_buttons_create()).
 *
 * Returns: %TRUE if current button was set, %FALSE if @current was not found.
 **/
gboolean
gwy_radio_buttons_set_current_from_widget(GtkWidget *widget,
                                          const gchar *key,
                                          gint current)
{
    return gwy_radio_buttons_set_current
               (gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget)),
                key, current);
}

/**
 * gwy_radio_buttons_get_current:
 * @group: A radio button group created by gwy_radio_buttons_create().
 * @key: Value object data key (specified as @key when called
 *       gwy_radio_buttons_create()).
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
    GQuark quark;

    quark = g_quark_from_string(key);

    while (group) {
        g_return_val_if_fail(GTK_IS_RADIO_BUTTON(group->data), -1);
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(group->data)))
            return GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(group->data),
                                                      quark));
        group = g_slist_next(group);
    }

    return -1;
}

/**
 * gwy_radio_buttons_get_current_from_widget:
 * @widget: A member of a radio button group created by
 *          gwy_radio_buttons_create().
 * @key: Value object data key (specified as @key when called
 *       gwy_radio_buttons_create()).
 *
 * Gets the integer enum value corresponding to currently selected item.
 *
 * Returns: The enum value corresponding to currently selected item.  In
 *          case of failure -1 is returned.
 **/
gint
gwy_radio_buttons_get_current_from_widget(GtkWidget *widget,
                                          const gchar *key)
{
    return gwy_radio_buttons_get_current
               (gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget)), key);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
