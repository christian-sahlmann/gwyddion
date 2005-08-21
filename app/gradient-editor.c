/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwyddion.h>
#include <libdraw/gwygradient.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <app/settings.h>
#include <app/gradient-editor.h>

/* For late objectzation... */
typedef struct {
    GtkWidget *window;
    GtkWidget *treeview;
    GtkWidget *button_edit;
    GtkWidget *button_new;
    GtkWidget *button_delete;
    GtkWidget *button_default;
    GString *active;
} GwyGradientEditor;

static void
gwy_gradient_editor_changed(GtkTreeSelection *selection,
                            GwyGradientEditor *editor)
{
    GwyResource *resource;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean is_modifiable;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_widget_set_sensitive(editor->button_edit, FALSE);
        gtk_widget_set_sensitive(editor->button_delete, FALSE);
        gtk_widget_set_sensitive(editor->button_default, FALSE);
        return;
    }

    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    g_string_assign(editor->active, gwy_resource_get_name(resource));

    gtk_widget_set_sensitive(editor->button_default, TRUE);
    is_modifiable = gwy_resource_get_is_modifiable(resource);
    gtk_widget_set_sensitive(editor->button_edit, is_modifiable);
    gtk_widget_set_sensitive(editor->button_delete, is_modifiable);
}

static void
gwy_gradient_editor_destroy(GwyGradientEditor *editor)
{
    GString *s = editor->active;

    memset(editor, 0, sizeof(GwyGradientEditor));
    editor->active = s;
}

static void
gwy_gradient_editor_set_default(GwyGradientEditor *editor)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GwyResource *resource;
    GwyInventory *inventory;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(editor->treeview));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        g_warning("Something should be selected for `Set Default'");
        return;
    }

    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));
    gwy_inventory_set_default_item_name(inventory,
                                        gwy_resource_get_name(resource));
}

void
gwy_app_gradient_editor(void)
{
    static GwyGradientEditor *editor = NULL;
    GtkWidget *treeview, *scwin, *vbox, *toolbox, *button;

    if (!editor) {
        editor = g_new0(GwyGradientEditor, 1);
        editor->active = g_string_new("");
    }
    else if (editor->window) {
        gtk_window_present(GTK_WINDOW(editor->window));
        return;
    }

    /* Pop up */
    editor->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(editor->window), _("Gradient Editor"));
    gtk_window_set_default_size(GTK_WINDOW(editor->window), -1, 420);
    g_signal_connect_swapped(editor->window, "destroy",
                             G_CALLBACK(gwy_gradient_editor_destroy), editor);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(editor->window), vbox);

    /* Gradients */
    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), scwin, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    treeview
        = gwy_gradient_tree_view_new(G_CALLBACK(gwy_gradient_editor_changed),
                                     editor, editor->active->str);
    editor->treeview = treeview;
    gtk_container_add(GTK_CONTAINER(scwin), treeview);

    /* Controls */
    toolbox = gtk_hbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(toolbox), 2);
    gtk_box_pack_start(GTK_BOX(vbox), toolbox, FALSE, FALSE, 0);

    button = gtk_button_new_from_stock(GTK_STOCK_EDIT);
    editor->button_edit = button;
    gtk_box_pack_start(GTK_BOX(toolbox), button, TRUE, TRUE, 0);

    button = gtk_button_new_from_stock(GTK_STOCK_NEW);
    editor->button_new = button;
    gtk_box_pack_start(GTK_BOX(toolbox), button, TRUE, TRUE, 0);

    button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
    editor->button_delete = button;
    gtk_box_pack_start(GTK_BOX(toolbox), button, TRUE, TRUE, 0);

    button = gtk_button_new_with_mnemonic(_("Set De_fault"));
    editor->button_default = button;
    gtk_box_pack_start(GTK_BOX(toolbox), button, TRUE, TRUE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_gradient_editor_set_default),
                             editor);

    gtk_widget_show_all(vbox);
    gtk_window_present(GTK_WINDOW(editor->window));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

