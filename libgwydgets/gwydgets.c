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

#include <string.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libdraw/gwydraw.h>
#include "gwydgets.h"

static guint types_initialized = 0;

/**
 * gwy_widgets_type_init:
 *
 * Initializes libgwywidgets data types, making their deserialization safe.
 *
 * Eventually calls gwy_draw_type_init().
 *
 * Since: 1.4.
 **/
void
gwy_widgets_type_init(void)
{
    if (types_initialized)
        return;

    gwy_draw_type_init();

    types_initialized += gwy_sphere_coords_get_type();
    types_initialized += gwy_graph_curve_model_get_type();
    types_initialized += gwy_graph_model_get_type();
    types_initialized |= 1;
}

/************************** Table attaching ****************************/

/**
 * gwy_table_attach_spinbutton:
 * @table: A #GtkTable.
 * @row: Table row to attach to.
 * @name: The label before @adj.
 * @units: The label after @adj.
 * @adj: An adjustment to create spinbutton from.
 *
 * Attaches a spinbutton with two labels to a table.
 *
 * Returns: The spinbutton as a #GtkWidget.
 **/
GtkWidget*
gwy_table_attach_spinbutton(GtkWidget *table,
                            gint row,
                            const gchar *name,
                            const gchar *units,
                            GtkObject *adj)
{
    GtkWidget *spin;

    g_return_val_if_fail(GTK_IS_TABLE(table), NULL);
    if (adj)
        g_return_val_if_fail(GTK_IS_ADJUSTMENT(adj), NULL);
    else
        adj = gtk_adjustment_new(0, 0, 0, 0, 0, 0);

    spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gwy_table_attach_row(table, row, name, units, spin);

    return spin;
}

/**
 * gwy_table_attach_row:
 * @table: A #GtkTable.
 * @row: Table row to attach to.
 * @name: The label before @middle_widget.
 * @units: The label after @adj.
 * @middle_widget: A widget.
 *
 * Attaches a widget with two labels to a table.
 **/
void
gwy_table_attach_row(GtkWidget *table,
                     gint row,
                     const gchar *name,
                     const gchar *units,
                     GtkWidget *middle_widget)
{
    GtkWidget *label;

    g_return_if_fail(GTK_IS_TABLE(table));
    g_return_if_fail(GTK_IS_WIDGET(middle_widget));

    label = gtk_label_new(units);
    gtk_table_attach(GTK_TABLE(table), label,
                     2, 3, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    label = gtk_label_new_with_mnemonic(name);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    gtk_table_attach(GTK_TABLE(table), middle_widget,
                     1, 2, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), middle_widget);
}

/************************** Mask colors ****************************/
typedef struct {
    GwyDataView *data_view;
    GwyColorButton *color_button;
    GwyContainer *container;
    gchar *prefix;
} MaskColorSelectorData;

static void
mask_color_updated_cb(GtkWidget *sel, MaskColorSelectorData *mcsdata)
{
    GdkColor gdkcolor;
    guint16 gdkalpha;
    GwyRGBA rgba;

    gwy_debug("mcsdata = %p", mcsdata);
    if (gtk_color_selection_is_adjusting(GTK_COLOR_SELECTION(sel)))
        return;

    gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(sel), &gdkcolor);
    gdkalpha = gtk_color_selection_get_current_alpha(GTK_COLOR_SELECTION(sel));

    gwy_rgba_from_gdk_color_and_alpha(&rgba, &gdkcolor, gdkalpha);
    gwy_rgba_store_to_container(&rgba, mcsdata->container, mcsdata->prefix);

    if (mcsdata->color_button)
        gwy_color_button_set_color(mcsdata->color_button, &rgba);
    if (mcsdata->data_view)
        gwy_data_view_update(mcsdata->data_view);
}

/**
 * gwy_color_selector_for_mask:
 * @dialog_title: Title of the color selection dialog (%NULL to use default).
 * @data_view: Data view to update on color change (%NULL to not update
 *             any data view).
 * @color_button: Color button to update on color change (or %NULL).
 * @container: Container to initialize the color from and save it to, may be
 *             %NULL to use @data_view's one if that is not %NULL.
 * @prefix: Prefix in @container (normally "/0/mask").
 *
 * Creates and runs a color selector dialog for a mask.
 *
 * Note this function does not return anything, it runs the dialog modally
 * and returns when it finishes.
 *
 * Since: 1.3.
 **/
void
gwy_color_selector_for_mask(const gchar *dialog_title,
                            GwyDataView *data_view,
                            GwyColorButton *color_button,
                            GwyContainer *container,
                            const gchar *prefix)
{
    GtkWidget *selector, *dialog;
    MaskColorSelectorData *mcsdata;
    GdkColor gdkcolor;
    guint16 gdkalpha;
    GwyRGBA rgba;
    gint response;

    g_return_if_fail(prefix && *prefix == '/');
    if (!container) {
        g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
        container = gwy_data_view_get_data(data_view);
    }

    mcsdata = g_new(MaskColorSelectorData, 1);
    mcsdata->data_view = data_view;
    mcsdata->color_button = color_button;
    mcsdata->container = container;
    mcsdata->prefix = g_strdup(prefix);

    gwy_rgba_get_from_container(&rgba, container, mcsdata->prefix);
    gwy_rgba_to_gdk_color(&rgba, &gdkcolor);
    gdkalpha = gwy_rgba_to_gdk_alpha(&rgba);

    dialog = gtk_color_selection_dialog_new(dialog_title
                                            ? dialog_title
                                            : _("Change Mask Color"));
    selector = GTK_COLOR_SELECTION_DIALOG(dialog)->colorsel;
    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(selector),
                                          &gdkcolor);
    gtk_color_selection_set_current_alpha(GTK_COLOR_SELECTION(selector),
                                          gdkalpha);
    gtk_color_selection_set_has_palette(GTK_COLOR_SELECTION(selector), FALSE);
    gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(selector),
                                                TRUE);
    g_signal_connect(selector, "color-changed",
                     G_CALLBACK(mask_color_updated_cb), mcsdata);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (response != GTK_RESPONSE_OK) {
        gwy_rgba_store_to_container(&rgba, container, mcsdata->prefix);
        if (mcsdata->color_button)
            gwy_color_button_set_color(mcsdata->color_button, &rgba);
        if (data_view)
            gwy_data_view_update(data_view);
    }
    g_free(mcsdata->prefix);
    g_free(mcsdata);
}


/************************** Utils ****************************/

/**
 * gwy_dialog_prevent_delete_cb:
 *
 * Returns %TRUE.
 *
 * The purpose of this function is to be used as a callback connected to the
 * "delete_event" of non-modal dialogs so that they can hide instead of
 * being destroyed.  This is achieved by returning %TRUE from the
 * "delete_event" callback.
 *
 * See #GtkDialog source code for the gory details...
 *
 * Returns: %TRUE.
 **/
gboolean
gwy_dialog_prevent_delete_cb(void)
{
    return TRUE;
}

/**
 * gwy_stock_like_button_new:
 * @label_text: Button label text.
 * @stock_id: Button icon stock id.
 *
 * Creates a button that looks like a stock button, but can have different
 * label text.
 *
 * Returns: The newly created button as #GtkWidget.
 *
 * Since: 1.5.
 **/
GtkWidget*
gwy_stock_like_button_new(const gchar *label_text,
                          const gchar *stock_id)
{
    GtkWidget *button, *alignment, *hbox, *label, *image;

    button = gtk_button_new();

    alignment = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_container_add(GTK_CONTAINER(button), alignment);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), hbox);

    image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(label_text);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

    return button;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
