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
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwydgets.h>
#include "gwyvalunit.h"

#define GWY_VAL_UNIT_TYPE_NAME "GwyValUnit"


/* Forward declarations - widget related*/
static void     gwy_val_unit_class_init           (GwyValUnitClass *klass);
static void     gwy_val_unit_init                 (GwyValUnit *val_unit);
static void     gwy_val_unit_finalize             (GObject *object);

static void     gwy_val_unit_realize              (GtkWidget *widget);
static void     gwy_val_unit_unrealize            (GtkWidget *widget);
static void     gwy_val_unit_size_allocate        (GtkWidget *widget,
                                                   GtkAllocation *allocation);

/* Local data */
static GtkWidgetClass *parent_class = NULL;


GType
gwy_val_unit_get_type(void)
{
    static GType gwy_val_unit_type = 0;

    if (!gwy_val_unit_type) {
        static const GTypeInfo gwy_val_unit_info = {
            sizeof(GwyValUnitClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_val_unit_class_init,
            NULL,
            NULL,
            sizeof(GwyValUnit),
            0,
            (GInstanceInitFunc)gwy_val_unit_init,
            NULL,
        };
        gwy_debug("");
        gwy_val_unit_type = g_type_register_static(GTK_TYPE_HBOX,
                                                      GWY_VAL_UNIT_TYPE_NAME,
                                                      &gwy_val_unit_info,
                                                      0);
    }

    return gwy_val_unit_type;
}

static void
gwy_val_unit_class_init(GwyValUnitClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_val_unit_finalize;

    widget_class->realize = gwy_val_unit_realize;
    widget_class->unrealize = gwy_val_unit_unrealize;
    widget_class->size_allocate = gwy_val_unit_size_allocate;
}


static void
gwy_val_unit_init(GwyValUnit *val_unit)
{
   
    gwy_debug("");

  
    /*
    frame = gtk_frame_new("Preview");
    gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
    val_unit->entry = GTK_ENTRY(gtk_entry_new());
    gtk_label_set_mnemonic_widget(GTK_LABEL(lab1), GTK_WIDGET(val_unit->entry));
    val_unit->label = GTK_LABEL(gtk_label_new(" "));
    val_unit->entities = GTK_COMBO(gtk_combo_new());
    lower = gwy_image_button_new_from_stock(GWY_STOCK_SUBSCRIPT);
    upper = gwy_image_button_new_from_stock(GWY_STOCK_SUPERSCRIPT);
    bold = gwy_image_button_new_from_stock(GWY_STOCK_BOLD);
    italic = gwy_image_button_new_from_stock(GWY_STOCK_ITALIC);
    add = gtk_button_new_with_mnemonic("A_dd symbol");
    hbox = gtk_hbox_new(FALSE, 0);

    items = stupid_put_entities(NULL);
    gtk_combo_set_popdown_strings(GTK_COMBO(val_unit->entities), items);

    gtk_editable_set_editable(GTK_EDITABLE(val_unit->entities->entry), FALSE);

    gtk_widget_show(lab1);
    gtk_widget_show(frame);
    gtk_widget_show(add);
    gtk_widget_show(upper);
    gtk_widget_show(lower);
    gtk_widget_show(bold);
    gtk_widget_show(italic);

    gtk_widget_show(GTK_WIDGET(val_unit->entry));
    gtk_widget_show(GTK_WIDGET(val_unit->label));
    gtk_widget_show(GTK_WIDGET(val_unit->entities));

    gtk_box_pack_start(GTK_BOX(val_unit), lab1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(val_unit), GTK_WIDGET(val_unit->entry),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(val_unit), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(val_unit), frame, TRUE, FALSE, 6);
    gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(val_unit->label));

    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(val_unit->entities),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), add, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), bold, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), italic, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), lower, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), upper, FALSE, FALSE, 0);

    gtk_widget_set_events(GTK_WIDGET(val_unit->entry), GDK_KEY_RELEASE_MASK);
    gtk_widget_set_events(val_unit->entities->list, GDK_BUTTON_PRESS_MASK);

    g_signal_connect(val_unit->entry, "changed",
                     G_CALLBACK(gwy_val_unit_edited), NULL);
    g_signal_connect_swapped(add, "clicked",
                             G_CALLBACK(gwy_val_unit_entity_selected),
                             val_unit);
    g_signal_connect(bold, "clicked",
                     G_CALLBACK(gwy_val_unit_button_some_pressed),
                     GINT_TO_POINTER(GWY_VAL_UNIT_BOLD));
    g_signal_connect(italic, "clicked",
                     G_CALLBACK(gwy_val_unit_button_some_pressed),
                     GINT_TO_POINTER(GWY_VAL_UNIT_ITALIC));
    g_signal_connect(upper, "clicked",
                     G_CALLBACK(gwy_val_unit_button_some_pressed),
                     GINT_TO_POINTER(GWY_VAL_UNIT_SUPERSCRIPT));
    g_signal_connect(lower, "clicked",
                     G_CALLBACK(gwy_val_unit_button_some_pressed),
                     GINT_TO_POINTER(GWY_VAL_UNIT_SUBSCRIPT));
*/
}

GtkWidget*
gwy_val_unit_new(gchar *label_text)
{
    GwyValUnit *val_unit;

    gwy_debug("");

    val_unit = (GwyValUnit*)gtk_object_new(gwy_val_unit_get_type (), NULL);

    val_unit->label = gtk_label_new(NULL);
    gtk_label_set_markup(val_unit->label, label_text);
    gtk_box_pack_start(GTK_BOX(val_unit), val_unit->label, FALSE, FALSE, 0);

    val_unit->adjustment = gtk_adjustment_new(val_unit->dival, 
                                    0, 10000, 1, 10, 0);
    val_unit->spin = gtk_spin_button_new(GTK_ADJUSTMENT(val_unit->adjustment), 1, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(val_unit->spin), 3);
    gtk_box_pack_start(GTK_BOX(val_unit), val_unit->spin, FALSE, FALSE, 0);
   
    val_unit->selection = gwy_option_menu_metric_unit(NULL, NULL,
                                                      -12, 3, "m",
                                                      val_unit->unit);
    gtk_box_pack_start(GTK_BOX(val_unit), val_unit->selection, FALSE, FALSE, 0);
 
    return GTK_WIDGET(val_unit);
}

static void
gwy_val_unit_finalize(GObject *object)
{
    gwy_debug("finalizing a GwyValUnit %d (refcount = %u)",
              (gint*)object, object->ref_count);

    g_return_if_fail(GWY_IS_VAL_UNIT(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_val_unit_unrealize(GtkWidget *widget)
{
    GwyValUnit *val_unit;

    val_unit = GWY_VAL_UNIT(widget);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static void
gwy_val_unit_realize(GtkWidget *widget)
{

    gwy_debug("realizing a GwyValUnit (%ux%u)",
              widget->allocation.x, widget->allocation.height);

    if (GTK_WIDGET_CLASS(parent_class)->realize)
    GTK_WIDGET_CLASS(parent_class)->realize(widget);

}


static void
gwy_val_unit_size_allocate(GtkWidget *widget,
                           GtkAllocation *allocation)
{
    gwy_debug("");

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_VAL_UNIT(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;
    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);

}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
