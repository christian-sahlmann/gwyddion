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

#include <math.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "gwygraphwindowasciidialog.h"
#include <libgwyddion/gwymacros.h>

#define GWY_GRAPH_WINDOW_ASCII_DIALOG_TYPE_NAME "GwyGraphWindowAsciiDialog"

GwyEnum style_type[] = {
    {N_("Plain text"),             GWY_GRAPH_MODEL_EXPORT_ASCII_PLAIN   },
    {N_("Gnuplot friendly"),       GWY_GRAPH_MODEL_EXPORT_ASCII_GNUPLOT },
    {N_("Comma separated values"), GWY_GRAPH_MODEL_EXPORT_ASCII_CSV     },
    {N_("Origin friendly"),        GWY_GRAPH_MODEL_EXPORT_ASCII_ORIGIN  },
};
               

static void     gwy_graph_window_ascii_dialog_class_init       (GwyGraphWindowAsciiDialogClass *klass);
static void     gwy_graph_window_ascii_dialog_init             (GwyGraphWindowAsciiDialog *dialog);
static void     gwy_graph_window_ascii_dialog_finalize         (GObject *object);
static gboolean gwy_graph_window_ascii_dialog_delete           (GtkWidget *widget,
                                                  GdkEventAny *event);

static void     units_changed_cb                                 (GwyGraphWindowAsciiDialog *dialog);
static void     metadata_changed_cb                              (GwyGraphWindowAsciiDialog *dialog);
static void     labels_changed_cb                                (GwyGraphWindowAsciiDialog *dialog);
static void     style_cb                                         (GtkWidget *item, 
                                                                  GwyGraphWindowAsciiDialog *dialog);



static GtkDialogClass *parent_class = NULL;

GType
gwy_graph_window_ascii_dialog_get_type(void)
{
    static GType gwy_graph_window_ascii_dialog_type = 0;

    if (!gwy_graph_window_ascii_dialog_type) {
        static const GTypeInfo gwy_graph_window_ascii_dialog_info = {
            sizeof(GwyGraphWindowAsciiDialogClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_window_ascii_dialog_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphWindowAsciiDialog),
            0,
            (GInstanceInitFunc)gwy_graph_window_ascii_dialog_init,
            NULL,
        };
        gwy_debug("");
        gwy_graph_window_ascii_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
                                                      GWY_GRAPH_WINDOW_ASCII_DIALOG_TYPE_NAME,
                                                      &gwy_graph_window_ascii_dialog_info,
                                                      0);

    }

    return gwy_graph_window_ascii_dialog_type;
}

static void
gwy_graph_window_ascii_dialog_class_init(GwyGraphWindowAsciiDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_graph_window_ascii_dialog_finalize;
    widget_class->delete_event = gwy_graph_window_ascii_dialog_delete;
}

static gboolean
gwy_graph_window_ascii_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_graph_window_ascii_dialog_init(GwyGraphWindowAsciiDialog *dialog)
{
    GtkWidget *label;

    dialog->style = GWY_GRAPH_MODEL_EXPORT_ASCII_PLAIN;
    dialog->preference =  gwy_option_menu_create(style_type,
                          G_N_ELEMENTS(style_type), "style",
                          G_CALLBACK(style_cb), dialog,
                          dialog->style);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                                    dialog->preference);
    
   
    dialog->units = TRUE;
    dialog->metadata = TRUE;
    dialog->labels = TRUE;
    
    dialog->check_labels = gtk_check_button_new_with_mnemonic(_("Export _labels"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->check_labels), dialog->labels);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                                            dialog->check_labels);
    g_signal_connect_swapped(dialog->check_labels, "clicked",
                             G_CALLBACK(labels_changed_cb), dialog);
    
    dialog->check_units = gtk_check_button_new_with_mnemonic(_("Export _units"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->check_units), dialog->units);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                                            dialog->check_units);
    g_signal_connect_swapped(dialog->check_units, "clicked",
                             G_CALLBACK(units_changed_cb), dialog);
     
    dialog->check_metadata = gtk_check_button_new_with_mnemonic(_("Export _metadata"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->check_metadata), dialog->metadata);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                                            dialog->check_metadata);
    g_signal_connect_swapped(dialog->check_metadata, "clicked",
                             G_CALLBACK(metadata_changed_cb), dialog);
       
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

    gtk_widget_show_all(dialog->preference);
    gtk_widget_show(dialog->check_units);
    gtk_widget_show(dialog->check_labels);
    gtk_widget_show(dialog->check_metadata);
}

GtkWidget *
gwy_graph_window_ascii_dialog_new()
{
    gwy_debug("");
    return GTK_WIDGET (g_object_new (gwy_graph_window_ascii_dialog_get_type (), NULL));
}

static void
gwy_graph_window_ascii_dialog_finalize(GObject *object)
{
    gwy_debug("");

    g_return_if_fail(GWY_IS_GRAPH_WINDOW_ASCII_DIALOG(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}


static void     
units_changed_cb(GwyGraphWindowAsciiDialog *dialog)
{
    dialog->units = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->check_units));
}

static void     
labels_changed_cb(GwyGraphWindowAsciiDialog *dialog)
{
    dialog->labels = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->check_labels));
}

static void     
metadata_changed_cb(GwyGraphWindowAsciiDialog *dialog)
{
    dialog->metadata = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->check_metadata));
}

static void     
style_cb(GtkWidget *item, GwyGraphWindowAsciiDialog *dialog)
{
    dialog->style = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "style"));
}

void  gwy_graph_window_ascii_dialog_get_data(GwyGraphWindowAsciiDialog *dialog,
                                             GwyGraphModelExportStyle *style,
                                             gboolean* units,
                                             gboolean* labels,
                                             gboolean* metadata)
{
    g_return_if_fail(GWY_IS_GRAPH_WINDOW_ASCII_DIALOG(dialog));
    *style = dialog->style;
    *units = dialog->units;
    *labels = dialog->labels;
    *metadata = dialog->metadata;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
