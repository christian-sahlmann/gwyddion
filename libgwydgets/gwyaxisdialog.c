/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "gwyaxisdialog.h"
#include <libgwyddion/gwymacros.h>

#define GWY_AXIS_DIALOG_TYPE_NAME "GwyAxisDialog"

static void     gwy_axis_dialog_class_init           (GwyAxisDialogClass *klass);
static void     gwy_axis_dialog_init                 (GwyAxisDialog *dialog);
static void     gwy_axis_dialog_finalize             (GObject *object);
static void     gwy_axis_dialog_size_request         (GtkWidget *widget,
						GtkRequisition *requisition);
static void     gwy_axis_dialog_size_allocate        (GtkWidget *widget,
                                                GtkAllocation *allocation);
static void     gwy_axis_dialog_delete               (GtkWidget *widget,
						      GdkEvent *event, 
						      gpointer user_data);

static GtkWidgetClass *parent_class = NULL;

GType
gwy_axis_dialog_get_type(void)
{
    static GType gwy_axis_dialog_type = 0;
    if (!gwy_axis_dialog_type) {
	static const GTypeInfo gwy_axis_dialog_info = {
	 sizeof(GwyAxisDialogClass),
	 NULL,
	 NULL,
	 (GClassInitFunc)gwy_axis_dialog_class_init,
	 NULL,
	 NULL,
	 sizeof(GwyAxisDialog),
	 0,
	 (GInstanceInitFunc)gwy_axis_dialog_init,
	 NULL,
         };
	gwy_debug("%s", __FUNCTION__);
	gwy_axis_dialog_type = g_type_register_static (GTK_TYPE_DIALOG, 
						 GWY_AXIS_DIALOG_TYPE_NAME, 
						 &gwy_axis_dialog_info, 
						 0);
		
    }
	
    return gwy_axis_dialog_type;
}

static void
gwy_axis_dialog_class_init(GwyAxisDialogClass *klass)
{
    GtkWidgetClass *widget_class;
    
    gwy_debug("%s", __FUNCTION__);
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    widget_class->delete_event = gwy_axis_dialog_delete;
}

static void
gwy_axis_dialog_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    gwy_debug("%s", __FUNCTION__);
    gtk_widget_hide(widget);
}

static void
gwy_axis_dialog_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
    GTK_WIDGET_CLASS(parent_class)->size_request(widget, requisition); 
    requisition->width = 200;
    requisition->height = 400;
    
}

static void
gwy_axis_dialog_init(GwyAxisDialog *dialog)
{
    gwy_debug("%s", __FUNCTION__);

    dialog->sci_text = gwy_sci_text_new();
    gtk_dialog_add_button(dialog, GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);
    gtk_dialog_add_button(dialog, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), dialog->sci_text);
    
}

GtkWidget *
gwy_axis_dialog_new()
{
    gwy_debug("%s", __FUNCTION__);
    return GTK_WIDGET (g_object_new (gwy_axis_dialog_get_type (), NULL));
}

static void     
gwy_axis_dialog_finalize(GObject *object)
{
    gwy_debug("%s", __FUNCTION__);

}

