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

#include <stdio.h>
#include "gwyoptionmenus.h"
#include "gwygrapher.h"
#include "gwygrapherareadialog.h"
#include <libgwyddion/gwymacros.h>

#define GWY_GRAPHER_AREA_DIALOG_TYPE_NAME "GwyGrapherAreaDialog"
#define BITS_PER_SAMPLE 8
#define POINT_SAMPLE_HEIGHT 20
#define POINT_SAMPLE_WIDTH 20

static void     gwy_grapher_area_dialog_class_init       (GwyGrapherAreaDialogClass *klass);
static void     gwy_grapher_area_dialog_init             (GwyGrapherAreaDialog *dialog);
static void     gwy_grapher_area_dialog_finalize         (GObject *object);
static gboolean gwy_grapher_area_dialog_delete           (GtkWidget *widget,
                                                          GdkEventAny *event);

static GtkWidget* gwy_point_menu_create                  (const GwyGrapherPointType current, 
                                                          gint *current_idx);
GtkWidget*      gwy_option_menu_point                    (GCallback callback,
                                                          gpointer cbdata,
                                                          const GwyGrapherPointType current);
static GtkWidget* gwy_sample_point_to_gtkimage           (GwyGrapherPointType type);
static void     pointtype_cb                             (GObject *item, 
                                                          GwyGrapherAreaDialog *dialog);

static GtkDialogClass *parent_class = NULL;

GType
gwy_grapher_area_dialog_get_type(void)
{
    static GType gwy_grapher_area_dialog_type = 0;

    if (!gwy_grapher_area_dialog_type) {
        static const GTypeInfo gwy_grapher_area_dialog_info = {
            sizeof(GwyGrapherAreaDialogClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_grapher_area_dialog_class_init,
            NULL,
            NULL,
            sizeof(GwyGrapherAreaDialog),
            0,
            (GInstanceInitFunc)gwy_grapher_area_dialog_init,
            NULL,
        };
        gwy_debug("");
        gwy_grapher_area_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
                                                      GWY_GRAPHER_AREA_DIALOG_TYPE_NAME,
                                                      &gwy_grapher_area_dialog_info,
                                                      0);

    }

    return gwy_grapher_area_dialog_type;
}

static void
gwy_grapher_area_dialog_class_init(GwyGrapherAreaDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_grapher_area_dialog_finalize;
    widget_class->delete_event = gwy_grapher_area_dialog_delete;
}

static gboolean
gwy_grapher_area_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_grapher_area_dialog_init(GwyGrapherAreaDialog *dialog)
{
    GtkWidget *label, *hbox;
    gwy_debug("");

    dialog->pointtype_menu = gwy_option_menu_point(G_CALLBACK(pointtype_cb), 
                                                   dialog, dialog->ptype);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                      dialog->pointtype_menu);

}

static void
pointtype_cb(GObject *item, GwyGrapherAreaDialog *dialog)
{
    printf("ble\n");
}

GtkWidget *
gwy_grapher_area_dialog_new()
{
    gwy_debug("");
    return GTK_WIDGET (g_object_new (gwy_grapher_area_dialog_get_type (), NULL));
}

static void
gwy_grapher_area_dialog_finalize(GObject *object)
{
    gwy_debug("");

    g_return_if_fail(GWY_IS_GRAPHER_AREA_DIALOG(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}



static GtkWidget*
gwy_point_menu_create(const GwyGrapherPointType current,
                      gint *current_idx)
{
    GtkWidget *menu, *image, *item, *hbox;
    guint l; 
    gint idx;
                                                                                                                                                                 menu = gtk_menu_new();
                                                                                                                                                                 idx = -1;
                                                                                                  
    for (l = 0; l<=GWY_GRAPHER_POINT_DIAMOND; l++) {
        image = gwy_sample_point_to_gtkimage(l);
        item = gtk_menu_item_new();
        hbox = gtk_hbox_new(FALSE, 6);
        /*label = gtk_label_new(name);*/
        /*gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);*/
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
        /*gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);*/
        gtk_container_add(GTK_CONTAINER(item), hbox);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data(G_OBJECT(item), "point-type", (gpointer)l);
        if (current && (current == l))
            idx = l;
    }

    if (current_idx && idx != -1)
        *current_idx = idx;
    return menu;        
}

GtkWidget*
gwy_option_menu_point(GCallback callback,
                        gpointer cbdata,
                        const GwyGrapherPointType current)
{
    GtkWidget *omenu, *menu;
    GList *c;
    gint idx;

    idx = -1;
    omenu = gtk_option_menu_new();
    g_object_set_data(G_OBJECT(omenu), "gwy-option-menu",
                      GINT_TO_POINTER(TRUE));
    menu = gwy_point_menu_create(current, &idx);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (idx != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), idx);
    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }
    return omenu;
}

static GtkWidget*
gwy_sample_point_to_gtkimage(GwyGrapherPointType type)
{
    static guchar *samples = NULL;
    GdkPixbuf *pixbuf;
    GtkWidget *image;
    guint rowstride;
    guchar *data;
    gint i;


    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            POINT_SAMPLE_WIDTH, POINT_SAMPLE_HEIGHT);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);

       
    image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    return image;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
