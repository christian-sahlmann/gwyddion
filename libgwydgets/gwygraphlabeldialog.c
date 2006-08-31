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
#include <glib-object.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygraphlabeldialog.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwydgetutils.h>

static void     gwy_graph_label_dialog_finalize(GObject *object);
static gboolean gwy_graph_label_dialog_delete  (GtkWidget *widget,
                                                GdkEventAny *event);
static void     linesize_changed_cb            (GtkObject *adj,
                                                GwyGraphLabelDialog *dialog);
static void     refresh                        (GwyGraphLabelDialog *dialog);
static void     reverse_changed_cb             (GtkToggleButton *button,
                                                GwyGraphLabelDialog *dialog);

G_DEFINE_TYPE(GwyGraphLabelDialog, gwy_graph_label_dialog, GTK_TYPE_DIALOG)

static void
gwy_graph_label_dialog_class_init(GwyGraphLabelDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;

    gobject_class->finalize = gwy_graph_label_dialog_finalize;
    widget_class->delete_event = gwy_graph_label_dialog_delete;
}

static gboolean
gwy_graph_label_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_graph_label_dialog_init(GwyGraphLabelDialog *dialog)
{
    GtkWidget *label, *table;
    gint row = 0;
    gwy_debug("");

    table = gtk_table_new(2, 8, FALSE);

    dialog->linesize = gtk_adjustment_new(1, 0, 6, 1, 5, 0);
    gwy_table_attach_spinbutton(table, row, _("Frame thickness:"), NULL,
                                dialog->linesize);
    g_signal_connect(dialog->linesize, "value-changed",
                     G_CALLBACK(linesize_changed_cb), dialog);
    row++;

    label = gtk_label_new("Layout:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    dialog->reversed = gtk_check_button_new_with_mnemonic(_("_reversed"));
    g_signal_connect(dialog->reversed, "toggled",
                     G_CALLBACK(reverse_changed_cb), dialog);
    gtk_table_attach(GTK_TABLE(table), dialog->reversed, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                      table);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);

    gtk_window_set_title(GTK_WINDOW(dialog), "Label properties");
    dialog->graph_model = NULL;
}

GtkWidget*
_gwy_graph_label_dialog_new()
{
    gwy_debug("");
    return GTK_WIDGET(g_object_new(GWY_TYPE_GRAPH_LABEL_DIALOG, NULL));
}

static void
gwy_graph_label_dialog_finalize(GObject *object)
{
    gwy_debug("");
    G_OBJECT_CLASS(gwy_graph_label_dialog_parent_class)->finalize(object);
}

static void
refresh(GwyGraphLabelDialog *dialog)
{
    GwyGraphModel *model;

    if (dialog->graph_model == NULL)
        return;

    model = GWY_GRAPH_MODEL(dialog->graph_model);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(dialog->linesize),
                             model->label_frame_thickness);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->reversed),
                                 model->label_reverse);
}

void
_gwy_graph_label_dialog_set_graph_data(GtkWidget *dialog, GObject *model)
{
    GwyGraphLabelDialog *gadialog = GWY_GRAPH_LABEL_DIALOG(dialog);

    gadialog->graph_model = model;
    refresh(GWY_GRAPH_LABEL_DIALOG(dialog));
}

static void
linesize_changed_cb(GtkObject *adj, GwyGraphLabelDialog *dialog)
{
    GwyGraphModel *model;

    if (dialog->graph_model == NULL)
        return;

    model = GWY_GRAPH_MODEL(dialog->graph_model);
    gwy_graph_model_set_label_frame_thickness(model,
                                              gwy_adjustment_get_int(adj));
}

static void
reverse_changed_cb(GtkToggleButton *button, GwyGraphLabelDialog *dialog)
{
    GwyGraphModel *model;

    if (dialog->graph_model == NULL)
        return;

    model = GWY_GRAPH_MODEL(dialog->graph_model);
    gwy_graph_model_set_label_reverse(model,
                                      gtk_toggle_button_get_active(button));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
