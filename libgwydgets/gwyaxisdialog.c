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
#include <math.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "gwyaxisdialog.h"
#include <libgwyddion/gwymacros.h>
#include "gwydgets.h"

#include <stdio.h>

#define GWY_AXIS_DIALOG_TYPE_NAME "GwyAxisDialog"

static void     gwy_axis_dialog_class_init       (GwyAxisDialogClass *klass);
static void     gwy_axis_dialog_init             (GwyAxisDialog *dialog);
static gboolean gwy_axis_dialog_delete           (GtkWidget *widget,
                                                  GdkEventAny *event);
static void     major_length_changed_cb          (GObject *adjustment, 
                                                  GObject *axis);
static void     major_thickness_changed_cb       (GObject *adjustment, 
                                                  GObject *axis);
static void     major_maxticks_changed_cb        (GObject *adjustment, 
                                                  GObject *axis);
static void     minor_length_changed_cb          (GObject *adjustment, 
                                                  GObject *axis);
static void     minor_thickness_changed_cb       (GObject *adjustment, 
                                                  GObject *axis);
static void     minor_division_changed_cb        (GObject *adjustment, 
                                                  GObject *axis);
static void     line_thickness_changed_cb        (GObject *adjustment, 
                                                  GObject *axis);
static void     autoscale_changed_cb             (GtkToggleButton *button,
                                                  GwyAxisDialog *dialog);

static GtkDialogClass *parent_class = NULL;

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
        gwy_debug("");
        gwy_axis_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
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

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    widget_class->delete_event = gwy_axis_dialog_delete;
}

static gboolean
gwy_axis_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_axis_dialog_init(GwyAxisDialog *dialog)
{
    gwy_debug("");
}

/**
 * gwy_axis_dialog_new:
 *
 * Creates a new axis dialog.
 *
 * Returns: A new axis dialog as a #GtkWidget.
 **/
GtkWidget*
gwy_axis_dialog_new(GtkWidget *axis)
{
    GwyAxisDialog *dialog = GWY_AXIS_DIALOG(g_object_new(gwy_axis_dialog_get_type(), NULL));
    GtkWidget *entry, *label, *table;
    gint row = 0;
    GValue val, bval;
    
    gwy_debug("");
    dialog->axis = axis;

    if (dialog->axis)
        gtk_window_set_title(GTK_WINDOW(dialog), "Axis properties");
    else
        gtk_window_set_title(GTK_WINDOW(dialog), "Label properties");
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);

   
    if (dialog->axis != NULL)
    {
        g_value_init(&val, G_TYPE_INT);
        g_value_init(&bval, G_TYPE_BOOLEAN);
        
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), _("<b>Axis settings:</b>"));
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;

        g_object_get_property(dialog->axis, "auto", &bval);
        dialog->is_auto
                    = gtk_check_button_new_with_mnemonic(_("_Autoscale"));
        gtk_table_attach(GTK_TABLE(table), dialog->is_auto,
                           0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        
        row++;
        
        g_object_get_property(dialog->axis, "major-maxticks", &val);
        dialog->major_division = gtk_adjustment_new(g_value_get_int(&val), 
                                                    1, 50, 1, 5, 0);
        dialog->major_division_spin = gwy_table_attach_hscale(table, row, _("Major division:"), NULL,
                                dialog->major_division, GWY_HSCALE_DEFAULT);
        g_signal_connect(dialog->major_division, "value-changed",
                     G_CALLBACK(major_maxticks_changed_cb), dialog->axis);
        row++;

        g_object_get_property(dialog->axis, "major-thickness", &val);
        dialog->major_thickness = gtk_adjustment_new(g_value_get_int(&val),
                                                     1, 10, 1, 5, 0);
        gwy_table_attach_spinbutton(table, row, _("Major thickness:"), NULL,
                                dialog->major_thickness);
        g_signal_connect(dialog->major_thickness, "value-changed",
                     G_CALLBACK(major_thickness_changed_cb), dialog->axis);
        row++;

        g_object_get_property(dialog->axis, "major-length", &val);
        dialog->major_length = gtk_adjustment_new(g_value_get_int(&val),
                                                  1, 20, 1, 5, 0);
        gwy_table_attach_spinbutton(table, row, _("Major length:"), NULL,
                                dialog->major_length);
        g_signal_connect(dialog->major_length, "value-changed",
                     G_CALLBACK(major_length_changed_cb), dialog->axis);
        row++;

        g_object_get_property(dialog->axis, "minor-division", &val);
        dialog->minor_division = gtk_adjustment_new(g_value_get_int(&val),
                                                    1, 20, 1, 5, 0);
        dialog->minor_division_spin = gwy_table_attach_spinbutton(table, row, _("Minor division:"), NULL,
                                dialog->minor_division);
        g_signal_connect(dialog->minor_division, "value-changed",
                     G_CALLBACK(minor_division_changed_cb), dialog->axis);
        row++;

        g_object_get_property(dialog->axis, "minor-thickness", &val);
        dialog->minor_thickness = gtk_adjustment_new(g_value_get_int(&val),
                                                     1, 10, 1, 5, 0);
        gwy_table_attach_spinbutton(table, row, _("Minor thickness:"), NULL,
                                dialog->minor_thickness);
        g_signal_connect(dialog->minor_thickness, "value-changed",
                     G_CALLBACK(minor_thickness_changed_cb), dialog->axis);
        row++;

        g_object_get_property(dialog->axis, "minor-length", &val);
        dialog->minor_length = gtk_adjustment_new(g_value_get_int(&val),
                                                  1, 20, 1, 5, 0);
        gwy_table_attach_spinbutton(table, row, _("Minor length:"), NULL,
                                dialog->minor_length);
        g_signal_connect(dialog->minor_length, "value-changed",
                     G_CALLBACK(minor_length_changed_cb), dialog->axis);
        row++;

        /*
        g_object_get_property(dialog->axis, "line-thickness", &val);
        dialog->line_thickness = gtk_adjustment_new(g_value_get_int(&val),
                                                    1, 10, 1, 5, 0);
        gwy_table_attach_spinbutton(table, row, _("Line thickness:"), NULL,
                                dialog->line_thickness);
        g_signal_connect(dialog->line_thickness, "value-changed",
                     G_CALLBACK(line_thickness_changed_cb), dialog->axis);
        row++;
        */
        g_signal_connect(dialog->is_auto, "toggled",
                                  G_CALLBACK(autoscale_changed_cb), dialog);
         gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->is_auto),
                                     g_value_get_boolean(&bval));
      }
    
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Label text:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                                                         table);

    dialog->sci_text = gwy_sci_text_new();
    gtk_container_set_border_width(GTK_CONTAINER(dialog->sci_text), 4);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    entry = gwy_sci_text_get_entry(GWY_SCI_TEXT(dialog->sci_text));

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                      dialog->sci_text);
    gtk_widget_show_all(dialog->sci_text);
 
    return GTK_WIDGET(dialog);
}

GtkWidget*
gwy_axis_dialog_get_sci_text(GtkWidget* dialog)
{
    return GWY_AXIS_DIALOG(dialog)->sci_text;
}

static void
major_length_changed_cb(GObject *adjustment, GObject *axis)
{
    GValue val = {0,};

    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment)));
    g_object_set_property (G_OBJECT (axis), "major-length", &val);
}

static void
major_maxticks_changed_cb(GObject *adjustment, GObject *axis)
{
    GValue val = {0,};

    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment)));
    g_object_set_property (G_OBJECT (axis), "major-maxticks", &val);
}

static void
major_thickness_changed_cb(GObject *adjustment, GObject *axis)
{
    GValue val = {0,};

    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment)));
    g_object_set_property (G_OBJECT (axis), "major-thickness", &val);
}

static void
minor_length_changed_cb(GObject *adjustment, GObject *axis)
{
    GValue val = {0,};

    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment)));
    g_object_set_property (G_OBJECT (axis), "minor-length", &val);
}

static void
minor_thickness_changed_cb(GObject *adjustment, GObject *axis)
{
    GValue val = {0,};

    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment)));
    g_object_set_property (G_OBJECT (axis), "minor-thickness", &val);
}

static void
minor_division_changed_cb(GObject *adjustment, GObject *axis)
{
    GValue val = {0,};

    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment)));
    g_object_set_property (G_OBJECT (axis), "minor-division", &val);
}

static void
line_thickness_changed_cb(GObject *adjustment, GObject *axis)
{
    GValue val = {0,};

    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment)));
    g_object_set_property (G_OBJECT (axis), "line-thickness", &val);
}

static void    
autoscale_changed_cb(GtkToggleButton *button, GwyAxisDialog *dialog)
{
    gboolean value;
    GValue val = {FALSE,};
    GValue ival = {0,};

    value = gtk_toggle_button_get_active(button);
    
    gtk_widget_set_sensitive(GTK_WIDGET(dialog->minor_division_spin), !value);
    gtk_widget_set_sensitive(GTK_WIDGET(dialog->major_division_spin), !value);

    g_value_init (&val, G_TYPE_BOOLEAN);
    g_value_set_boolean (&val, value);
    g_object_set_property (G_OBJECT (dialog->axis), "auto", &val);

    g_value_init (&ival, G_TYPE_INT);
    g_value_set_int (&ival, gtk_adjustment_get_value(GTK_ADJUSTMENT(dialog->major_division)));
    g_object_set_property (G_OBJECT (dialog->axis), "major-maxticks", &ival);
    g_value_set_int (&ival, gtk_adjustment_get_value(GTK_ADJUSTMENT(dialog->minor_division)));
    g_object_set_property (G_OBJECT (dialog->axis), "minor-division", &ival);


    
}


/**
 * SECTION:gwyaxisdialog
 * @title: GwyAxisDialog
 * @short_description: Axis properties dialog
 *
 * #GwyAxisDialog is used for setting the text properties
 * of the axis. It is used namely with #GwyAxis.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
