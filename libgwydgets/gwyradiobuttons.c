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
#include "gwyradiobuttons.h"

static GQuark gwyrb_quark = 0;

static void
setup_quark(void)
{
    if (!gwyrb_quark)
        gwyrb_quark = g_quark_from_static_string("gwy-radiobuttons-key");
}

/**
 * gwy_radio_buttons_create:
 * @entries: Radio button group items.
 * @nentries: The number of items.
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            no callback).
 * @cbdata: User data passed to the callback.
 * @current: Value to be shown as currently selected (-1 to use what happens
 *           to be first).
 *
 * Creates a radio button group for an enum.
 *
 * Try to avoid -1 as an enum value.
 *
 * Returns: The newly created radio button group (a #GSList).  Iterate over
 *          the list and pack the widgets (the order is the same as in
 *          @entries).  The group is owned by the buttons and must not be
 *          freed.
 **/
GSList*
gwy_radio_buttons_create(const GwyEnum *entries,
                         gint nentries,
                         GCallback callback,
                         gpointer cbdata,
                         gint current)
{
    GtkWidget *button, *curbutton;
    GSList *group;
    gint i;

    setup_quark();
    button = curbutton = NULL;
    /* FIXME: this relies on undocumented GtkRadioButton behaviour;
     * we assume it puts the items into the group in reverse order */
    for (i = nentries-1; i >= 0; i--) {
        button = gtk_radio_button_new_with_mnemonic_from_widget
                               (GTK_RADIO_BUTTON(button), _(entries[i].name));
        g_object_set_qdata(G_OBJECT(button), gwyrb_quark,
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
 * gwy_radio_buttons_attach_to_table:
 * @group: A radio button group.  Not necessarily created by
 *         gwy_radio_buttons_create().
 * @table: A table.
 * @colspan: The number of columns the radio buttons should span across.
 * @row: Table row to start attaching at.
 *
 * Attaches a group of radio buttons to table rows.
 *
 * Returns: The row after the last attached radio button.
 *
 * Since: 2.1
 **/
gint
gwy_radio_buttons_attach_to_table(GSList *group,
                                  GtkTable *table,
                                  gint colspan,
                                  gint row)
{
    g_return_val_if_fail(GTK_IS_TABLE(table), row);

    while (group) {
        gtk_table_attach(table, GTK_WIDGET(group->data),
                         0, colspan, row, row + 1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;
        group = g_slist_next(group);
    }

    return row;
}

/**
 * gwy_radio_buttons_set_current:
 * @group: A radio button group created by gwy_radio_buttons_create().
 * @current: Value to be shown as currently selected.
 *
 * Sets currently selected radio button in @group based on integer item object
 * data (as set by gwy_radio_buttons_create()).
 *
 * Returns: %TRUE if current button was set, %FALSE if @current was not found.
 **/
gboolean
gwy_radio_buttons_set_current(GSList *group,
                              gint current)
{
    GtkWidget *button;

    if (!(button = gwy_radio_buttons_find(group, current)))
        return FALSE;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    return TRUE;
}

/**
 * gwy_radio_buttons_get_current:
 * @group: A radio button group created by gwy_radio_buttons_create().
 *
 * Gets the integer enum value corresponding to currently selected item.
 *
 * Returns: The enum value corresponding to currently selected item.  In
 *          case of failure -1 is returned.
 **/
gint
gwy_radio_buttons_get_current(GSList *group)
{
    while (group) {
        g_return_val_if_fail(GTK_IS_RADIO_BUTTON(group->data), -1);
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(group->data)))
            return GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(group->data),
                                                      gwyrb_quark));
        group = g_slist_next(group);
    }

    return -1;
}

/**
 * gwy_radio_buttons_find:
 * @group: A radio button group created by gwy_radio_buttons_create().
 * @value: The value associated with the button to find.
 *
 * Finds a radio button by its associated integer value.
 *
 * Returns: The radio button corresponding to @value, or %NULL on failure.
 **/
GtkWidget*
gwy_radio_buttons_find(GSList *group,
                       gint value)
{
    g_return_val_if_fail(group, NULL);
    while (group) {
        g_return_val_if_fail(GTK_IS_RADIO_BUTTON(group->data), FALSE);
        if (GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(group->data),
                                               gwyrb_quark)) == value)
            return GTK_WIDGET(group->data);
        group = g_slist_next(group);
    }

    return NULL;
}

/**
 * gwy_radio_button_get_value:
 * @button: A radio button belonging to a group created by
 *          gwy_radio_buttons_create().
 *
 * Gets the integer value associated with a radio button.
 *
 * Returns: The integer value corresponding to @button.
 **/
gint
gwy_radio_button_get_value(GtkWidget *button)
{
    g_return_val_if_fail(GTK_IS_RADIO_BUTTON(button), -1);
    return GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(button), gwyrb_quark));
}

/**
 * gwy_radio_button_set_value:
 * @button: A radio button to set associated value of.
 * @value: Value to associate.
 *
 * Sets the integer value associated with a radio button.
 *
 * This function allow to change associated radio button values after creation
 * or even construct a radio button group with associated integers without the
 * help of gwy_radio_buttons_create().
 **/
void
gwy_radio_button_set_value(GtkWidget *button,
                           gint value)
{
    g_return_if_fail(GTK_IS_RADIO_BUTTON(button));
    setup_quark();
    g_object_set_qdata(G_OBJECT(button), gwyrb_quark, GINT_TO_POINTER(value));
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyradiobuttons
 * @title: gwyradiobuttons
 * @short_description: Radio button constructors for enums
 * @see_also: <link linkend="libgwydget-gwycombobox">gwycombobox</link>
 *            -- combo box constructors
 *
 * Groups of button associated with some integers can be easily constructed
 * from #GwyEnum's with gwy_radio_buttons_create().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
