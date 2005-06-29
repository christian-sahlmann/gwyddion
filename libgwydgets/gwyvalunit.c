/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#include <libgwyddion/gwymacros.h>

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>

#include <libgwyddion/gwysiunit.h>
#include "gwyoptionmenus.h"
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
static void     gwy_val_unit_value_changed        (GtkSpinButton *spinbutton,
                                                   GwyValUnit *val_unit);
static void     gwy_val_unit_unit_changed         (GObject *item,
                                                   GwyValUnit *val_unit);

/* Local data */
static GtkWidgetClass *parent_class = NULL;

enum {
    VALUE_CHANGED_SIGNAL,
    LAST_SIGNAL
};

static guint gwyvalunit_signals[LAST_SIGNAL] = { 0 };

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
        gwy_debug(" ");
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

    gwy_debug(" ");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_val_unit_finalize;

    widget_class->realize = gwy_val_unit_realize;
    widget_class->unrealize = gwy_val_unit_unrealize;
    widget_class->size_allocate = gwy_val_unit_size_allocate;

    gwyvalunit_signals[VALUE_CHANGED_SIGNAL]
        = g_signal_new("value-changed",
                       G_TYPE_FROM_CLASS(klass),
                       G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET(GwyValUnitClass, value_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}


static void
gwy_val_unit_init(GwyValUnit *val_unit)
{
    val_unit->dival = 0.0;
    val_unit->unit = 0;

    gwy_debug(" ");
}


/**
 * gwy_val_unit_new:
 * @label_text: label to appear on the left side
 * @si_unit: base unit to appear on the right side
 *
 * Creates label, adjustment and selection to
 * set value with unit.
 *
 * Returns: new widget.
 **/
GtkWidget*
gwy_val_unit_new(gchar *label_text, GwySIUnit *si_unit)
{
    GwyValUnit *val_unit;

    gwy_debug(" ");

    val_unit = (GwyValUnit*)g_object_new(gwy_val_unit_get_type(),
                                         "spacing", 2,
                                         NULL);

    val_unit->label = gtk_label_new_with_mnemonic(label_text);
    gtk_box_pack_start(GTK_BOX(val_unit), val_unit->label, FALSE, FALSE, 2);

    val_unit->adjustment = gtk_adjustment_new(val_unit->dival,
                                              -1e6, 1e6, 1, 10, 0);
    val_unit->spin = gtk_spin_button_new(GTK_ADJUSTMENT(val_unit->adjustment),
                                         1, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(val_unit->spin), 3);
    gtk_box_pack_start(GTK_BOX(val_unit), val_unit->spin, FALSE, FALSE, 2);

    val_unit->selection
        = gwy_option_menu_metric_unit(G_CALLBACK(gwy_val_unit_unit_changed),
                                      val_unit,
                                      -12, 6, si_unit,
                                      val_unit->unit);
    gtk_box_pack_start(GTK_BOX(val_unit), val_unit->selection, FALSE, FALSE, 2);

    g_signal_connect(val_unit->spin, "value-changed",
                     G_CALLBACK(gwy_val_unit_value_changed), val_unit);


    val_unit->base_si_unit = gwy_si_unit_duplicate(si_unit);

    return GTK_WIDGET(val_unit);
}

static void
gwy_val_unit_finalize(GObject *object)
{
    gwy_debug("finalizing a GwyValUnit %d (refcount = %u)",
              (gint*)object, object->ref_count);

    g_return_if_fail(GWY_IS_VAL_UNIT(object));
    g_object_unref(GWY_VAL_UNIT(object)->base_si_unit);

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
    gwy_debug(" ");

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_VAL_UNIT(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;
    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);
}

static void
gwy_val_unit_value_changed(GtkSpinButton *spinbutton, GwyValUnit *val_unit)
{
    val_unit->dival = gtk_spin_button_get_value(spinbutton);
    gwy_val_unit_signal_value_changed(val_unit);
}

static void
gwy_val_unit_unit_changed(GObject *item, GwyValUnit *val_unit)
{
    val_unit->unit = GPOINTER_TO_INT(g_object_get_data(item,
                                                      "metric-unit"));
    gwy_val_unit_signal_value_changed(val_unit);
}

/**
 * gwy_val_unit_set_value:
 * @val_unit: GwyValUnit widget
 * @value: value to be set
 *
 * sets value and automatically chooses its prefix to appear
 * in selection.
 **/
void
gwy_val_unit_set_value(GwyValUnit *val_unit, gdouble value)
{
    GwySIValueFormat *format;
    format = gwy_si_unit_get_format(val_unit->base_si_unit,
                                    GWY_SI_UNIT_FORMAT_VFMARKUP, value, NULL);

    val_unit->unit = floor(log10(format->magnitude)); /* /3 */
    val_unit->dival = value/pow10(val_unit->unit);  /*1000*/

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(val_unit->spin), val_unit->dival);

    gtk_option_menu_set_history(GTK_OPTION_MENU(val_unit->selection),
                                floor(val_unit->unit/3) + 4);
}

/**
 * gwy_val_unit_get_value:
 * @val_unit: GwyValUnit widget
 *
 * Computes actual value of adjustment and unit prefix.
 *
 * Returns: actual value
 **/
gdouble
gwy_val_unit_get_value(GwyValUnit *val_unit)
{
    val_unit->dival
        = gtk_spin_button_get_value(GTK_SPIN_BUTTON(val_unit->spin));

    return val_unit->dival * pow10(val_unit->unit);
}

void
gwy_val_unit_signal_value_changed(GwyValUnit *val_unit)
{
    g_signal_emit(G_OBJECT(val_unit), gwyvalunit_signals[VALUE_CHANGED_SIGNAL],
                  0);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
