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

#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include "gwydgets.h"


/************************** Table attaching ****************************/

/**
 * gwy_table_attach_spinbutton:
 * @table: A #GtkTable.
 * @row: Table row to attach to.
 * @name: The label before @adj.
 * @units: The label after @adj.
 * @adj: An adjustment to create spinbutton from.
 *
 * Attaches a spinbutton with two labels to a table.
 *
 * Returns: The spinbutton as a #GtkWidget.
 **/
GtkWidget*
gwy_table_attach_spinbutton(GtkWidget *table,
                            gint row,
                            const gchar *name,
                            const gchar *units,
                            GtkObject *adj)
{
    GtkWidget *spin;

    g_return_val_if_fail(GTK_IS_TABLE(table), NULL);
    if (adj)
        g_return_val_if_fail(GTK_IS_ADJUSTMENT(adj), NULL);
    else
        adj = gtk_adjustment_new(0, 0, 0, 0, 0, 0);

    spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gwy_table_attach_row(table, row, name, units, spin);

    return spin;
}

/**
 * gwy_table_attach_row:
 * @table: A #GtkTable.
 * @row: Table row to attach to.
 * @name: The label before @middle_widget.
 * @units: The label after @adj.
 * @middle_widget: A widget.
 *
 * Attaches a widget with two labels to a table.
 **/
void
gwy_table_attach_row(GtkWidget *table,
                     gint row,
                     const gchar *name,
                     const gchar *units,
                     GtkWidget *middle_widget)
{
    GtkWidget *label;

    g_return_if_fail(GTK_IS_TABLE(table));
    g_return_if_fail(GTK_IS_WIDGET(middle_widget));

    label = gtk_label_new(units);
    gtk_table_attach(GTK_TABLE(table), label,
                     2, 3, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    label = gtk_label_new_with_mnemonic(name);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    gtk_table_attach(GTK_TABLE(table), middle_widget,
                     1, 2, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), middle_widget);
}


/************************** Utils ****************************/

/**
 * gwy_dialog_prevent_delete_cb:
 *
 * Returns %TRUE.
 *
 * The purpose of this function is to be used as a callback connected to the
 * "delete_event" of non-modal dialogs so that they can hide instead of
 * being destroyed.  This is achieved by returning %TRUE from the
 * "delete_event" callback.
 *
 * See #GtkDialog source code for the gory details...
 *
 * Returns: %TRUE.
 **/
gboolean
gwy_dialog_prevent_delete_cb(void)
{
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
