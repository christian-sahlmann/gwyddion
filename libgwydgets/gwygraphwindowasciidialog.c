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

#define GWY_GRAPHER_WINDOW_ASCII_DIALOG_TYPE_NAME "GwyGrapherWindowAsciiDialog"

static void     gwy_grapher_window_ascii_dialog_class_init       (GwyGrapherWindowAsciiDialogClass *klass);
static void     gwy_grapher_window_ascii_dialog_init             (GwyGrapherWindowAsciiDialog *dialog);
static void     gwy_grapher_window_ascii_dialog_finalize         (GObject *object);
static gboolean gwy_grapher_window_ascii_dialog_delete           (GtkWidget *widget,
                                                  GdkEventAny *event);

static GtkDialogClass *parent_class = NULL;

GType
gwy_grapher_window_ascii_dialog_get_type(void)
{
    static GType gwy_grapher_window_ascii_dialog_type = 0;

    if (!gwy_grapher_window_ascii_dialog_type) {
        static const GTypeInfo gwy_grapher_window_ascii_dialog_info = {
            sizeof(GwyGrapherWindowAsciiDialogClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_grapher_window_ascii_dialog_class_init,
            NULL,
            NULL,
            sizeof(GwyGrapherWindowAsciiDialog),
            0,
            (GInstanceInitFunc)gwy_grapher_window_ascii_dialog_init,
            NULL,
        };
        gwy_debug("");
        gwy_grapher_window_ascii_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
                                                      GWY_GRAPHER_WINDOW_ASCII_DIALOG_TYPE_NAME,
                                                      &gwy_grapher_window_ascii_dialog_info,
                                                      0);

    }

    return gwy_grapher_window_ascii_dialog_type;
}

static void
gwy_grapher_window_ascii_dialog_class_init(GwyGrapherWindowAsciiDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_grapher_window_ascii_dialog_finalize;
    widget_class->delete_event = gwy_grapher_window_ascii_dialog_delete;
}

static gboolean
gwy_grapher_window_ascii_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_grapher_window_ascii_dialog_init(GwyGrapherWindowAsciiDialog *dialog)
{
    GtkWidget *label;
    gwy_debug("");

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

        
}

GtkWidget *
gwy_grapher_window_ascii_dialog_new()
{
    gwy_debug("");
    return GTK_WIDGET (g_object_new (gwy_grapher_window_ascii_dialog_get_type (), NULL));
}

static void
gwy_grapher_window_ascii_dialog_finalize(GObject *object)
{
    gwy_debug("");

    g_return_if_fail(GWY_IS_GRAPHER_WINDOW_ASCII_DIALOG(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
