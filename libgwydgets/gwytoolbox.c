/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <glib.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include "gwytoolbox.h"

#define GWY_TOOLBOX_TYPE_NAME "GwyToolbox"

/* Forward declarations */

static void     gwy_toolbox_class_init     (GwyToolboxClass *klass);
static void     gwy_toolbox_destroy        (GtkObject *object);
static void     gwy_toolbox_init           (GwyToolbox *toolbox);


/* Local data */

static GtkTableClass *parent_class = NULL;

GType
gwy_toolbox_get_type(void)
{
    static GType gwy_toolbox_type = 0;

    if (!gwy_toolbox_type) {
        static const GTypeInfo gwy_toolbox_info = {
            sizeof(GwyToolboxClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_toolbox_class_init,
            NULL,
            NULL,
            sizeof(GwyToolbox),
            0,
            (GInstanceInitFunc)gwy_toolbox_init,
            NULL,
        };
        gwy_debug("");
        gwy_toolbox_type = g_type_register_static(GTK_TYPE_TABLE,
                                                  GWY_TOOLBOX_TYPE_NAME,
                                                  &gwy_toolbox_info,
                                                  0);
    }

    return gwy_toolbox_type;
}

static void
gwy_toolbox_class_init(GwyToolboxClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);
    object_class->destroy = gwy_toolbox_destroy;
}

static void
gwy_toolbox_init(GwyToolbox *toolbox)
{
    gwy_debug("");

    toolbox->tooltips = NULL;
    toolbox->stuff = NULL;
    toolbox->nstuff = 0;
    toolbox->max_width = -1;
}

/**
 * gwy_toolbox_new:
 * @max_width: The maximum width of the toolbox (in item count).
 *
 * Creates a new #GwyToolbox.
 *
 * Returns: The new toolbox as a #GtkWidget.
 **/
GtkWidget*
gwy_toolbox_new(gint max_width)
{
    GwyToolbox *toolbox;

    gwy_debug("");

    toolbox = (GwyToolbox*)g_object_new(GWY_TYPE_TOOLBOX, NULL);

    toolbox->max_width = max_width;
    gtk_table_resize(GTK_TABLE(toolbox), 1, max_width > 0 ? max_width : 1);
    gtk_table_set_homogeneous(GTK_TABLE(toolbox), TRUE);
    gtk_table_set_row_spacings(GTK_TABLE(toolbox), 0);
    gtk_table_set_col_spacings(GTK_TABLE(toolbox), 0);
    gtk_container_set_border_width(GTK_CONTAINER(toolbox), 0);

    toolbox->tooltips = gtk_tooltips_new();
    g_object_ref(toolbox->tooltips);
    gtk_object_sink(GTK_OBJECT(toolbox->tooltips));

    return GTK_WIDGET(toolbox);
}

static void
gwy_toolbox_destroy(GtkObject *object)
{
    GwyToolbox *toolbox;

    g_return_if_fail(GWY_IS_TOOLBOX(object));

    toolbox = GWY_TOOLBOX(object);
    gwy_object_unref(toolbox->tooltips);
    g_list_free(toolbox->stuff);

    GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

GtkWidget*
gwy_toolbox_append(GwyToolbox *toolbox,
                   GType type,
                   GtkWidget *widget,
                   const char *tooltip_text,
                   const char *tooltip_private_text,
                   const gchar *stock_id,
                   GCallback callback,
                   gpointer user_data)
{
    const gchar *signame;
    GtkWidget *icon, *child;
    gint row, col;

    g_return_val_if_fail(GWY_IS_TOOLBOX(toolbox), NULL);

    icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
    g_return_val_if_fail(GTK_IS_IMAGE(icon), NULL);

    if (type == GTK_TYPE_BUTTON) {
        child = gtk_button_new();
        signame = "clicked";
    }
    else if (type == GTK_TYPE_RADIO_BUTTON) {
        if (widget)
            child = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(widget));
        else
            child = gtk_radio_button_new(NULL);
        gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(child), FALSE);
        /* XXX: tools need to know being clicked on even when already active */
        signame = "clicked";
    }
    else {
        g_critical("Type %s not supported (yet)", g_type_name(type));
        return NULL;
    }
    gtk_container_add(GTK_CONTAINER(child), icon);

    if (toolbox->max_width > 0) {
        row = toolbox->nstuff/toolbox->max_width;
        col = toolbox->nstuff % toolbox->max_width;
        if (toolbox->nstuff > 0 && !col)
            gtk_table_resize(GTK_TABLE(toolbox), row, toolbox->max_width);
    }
    else {
        row = 0;
        col = toolbox->nstuff;
        gtk_table_resize(GTK_TABLE(toolbox), 1, toolbox->nstuff);
    }
    toolbox->nstuff++;

    gtk_tooltips_set_tip(toolbox->tooltips, child,
                         tooltip_text, tooltip_private_text);
    gtk_table_attach(GTK_TABLE(toolbox), child, col, col+1, row, row+1,
                     0, 0, 0, 0);
    g_signal_connect_swapped(child, signame, callback, user_data);

    return child;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
