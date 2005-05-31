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
#include <libgwyddion/gwydebugobjects.h>
#include <libdraw/gwydraw.h>
#include <pango/pangoft2.h>
#include "gwydgetutils.h"

enum {
    GWY_HSCALE_WIDTH = 96
};

static void  gwy_hscale_update_log  (GtkAdjustment *adj,
                                     GtkAdjustment *slave);
static void  gwy_hscale_update_exp  (GtkAdjustment *adj,
                                     GtkAdjustment *slave);
static void  gwy_hscale_update_sqrt (GtkAdjustment *adj,
                                     GtkAdjustment *slave);
static void  gwy_hscale_update_sq   (GtkAdjustment *adj,
                                     GtkAdjustment *slave);

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

    if (units) {
        label = gtk_label_new(units);
        gtk_table_attach(GTK_TABLE(table), label,
                        2, 3, row, row+1, GTK_FILL, 0, 2, 2);
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    }

    label = gtk_label_new_with_mnemonic(name);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    gtk_table_attach(GTK_TABLE(table), middle_widget,
                     1, 2, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), middle_widget);
}

/**
 * gwy_table_get_child_widget:
 * @table: A #GtkTable.
 * @row: Row in @table.
 * @col: Column in @table.
 *
 * Finds a widget in #GtkTable by its coordinates.
 *
 * By widget at (@col, @row) is meant a widget that either contains this
 * corner or is attached by its left side, top side, or top left cornder to
 * it.
 *
 * If there are multiple matches due to overlapping widgets, an arbitrary of
 * them is returned.
 *
 * Returns: The widget at (@col, @row) or %NULL if there is no such widget.
 **/
GtkWidget*
gwy_table_get_child_widget(GtkWidget *table,
                           gint row,
                           gint col)
{
    GList *l;

    g_return_val_if_fail(GTK_IS_TABLE(table), NULL);
    for (l = GTK_TABLE(table)->children; l; l = g_list_next(l)) {
        GtkTableChild *child = (GtkTableChild*)l->data;

        if (child->left_attach <= col && child->right_attach > col
            && child->top_attach <= row && child->bottom_attach > row)
            return child->widget;
    }
    return NULL;
}

/************************** Scale attaching ****************************/

static void
gwy_hscale_update_log(GtkAdjustment *adj, GtkAdjustment *slave)
{
    gulong id;

    id = g_signal_handler_find(slave,
                               G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                               0, 0, 0, gwy_hscale_update_exp, adj);
    g_signal_handler_block(slave, id);
    gtk_adjustment_set_value(slave, log(adj->value));
    g_signal_handler_unblock(slave, id);
}

static void
gwy_hscale_update_exp(GtkAdjustment *adj, GtkAdjustment *slave)
{
    gulong id;

    id = g_signal_handler_find(slave,
                               G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                               0, 0, 0, gwy_hscale_update_log, adj);
    g_signal_handler_block(slave, id);
    gtk_adjustment_set_value(slave, exp(adj->value));
    g_signal_handler_unblock(slave, id);
}

static void
gwy_hscale_update_sqrt(GtkAdjustment *adj, GtkAdjustment *slave)
{
    gulong id;

    id = g_signal_handler_find(slave,
                               G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                               0, 0, 0, gwy_hscale_update_sq, adj);
    g_signal_handler_block(slave, id);
    gtk_adjustment_set_value(slave, sqrt(adj->value));
    g_signal_handler_unblock(slave, id);
}

static void
gwy_hscale_update_sq(GtkAdjustment *adj, GtkAdjustment *slave)
{
    gulong id;

    id = g_signal_handler_find(slave,
                               G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                               0, 0, 0, gwy_hscale_update_sqrt, adj);
    g_signal_handler_block(slave, id);
    gtk_adjustment_set_value(slave, adj->value*adj->value);
    g_signal_handler_unblock(slave, id);
}

/**
 * gwy_table_hscale_set_sensitive:
 * @pivot: The same object that was passed to gwy_table_attach_hscale() as
 *         @pivot.
 * @sensitive: %TRUE to make the row sensitive, %FALSE to insensitive.
 *
 * Sets sensitivity of a group of controls create by gwy_table_attach_hscale().
 *
 * Do not use with %GWY_HSCALE_CHECK, simply set state of the check button
 * in such a case.
 **/
void
gwy_table_hscale_set_sensitive(GtkObject *pivot,
                               gboolean sensitive)
{
    GtkWidget *widget;
    GObject *object;

    object = G_OBJECT(pivot);

    if ((widget = g_object_get_data(object, "scale")))
        gtk_widget_set_sensitive(widget, sensitive);
    if ((widget = g_object_get_data(object, "middle_widget")))
        gtk_widget_set_sensitive(widget, sensitive);
    if ((widget = g_object_get_data(object, "label")))
        gtk_widget_set_sensitive(widget, sensitive);
    if ((widget = g_object_get_data(object, "units")))
        gtk_widget_set_sensitive(widget, sensitive);
}

static void
gwy_hscale_checkbutton_cb(GtkToggleButton *check,
                          GtkObject *pivot)
{
    gwy_table_hscale_set_sensitive(pivot, gtk_toggle_button_get_active(check));
}

/**
 * gwy_table_attach_hscale:
 * @table: A #GtkTable.
 * @row: Row in @table to attach stuff to.
 * @name: The label before @pivot widget.
 * @units: The label after @pivot widget.
 * @pivot: Either a #GtkAdjustment, or a widget to use instead of the spin
 *         button and scale widgets (if @style is %GWY_HSCALE_WIDGET).
 * @style: A mix of options an flags determining what and how will be attached
 *         to the table.
 *
 * Attaches a spinbutton with a scale and labels, or something else to a table
 * row.
 *
 * Following object data are set on @pivot to various components:
 * "scale", "check", "label", "units", "middle_widget"
 * (some may be %NULL if not present).
 *
 * FIXME: What exactly happens with various @style values is quite convoluted.
 *
 * Returns: The middle widget.  If a spinbutton is attached, then this
 *          spinbutton is returned.  Otherwise (in %GWY_HSCALE_WIDGET case)
 *          @pivot itself.
 **/
GtkWidget*
gwy_table_attach_hscale(GtkWidget *table,
                        gint row,
                        const gchar *name,
                        const gchar *units,
                        GtkObject *pivot,
                        GwyHScaleStyle style)
{
    GtkWidget *spin, *scale, *label, *check, *middle_widget, *align;
    GtkAdjustment *scale_adj = NULL, *adj = NULL;
    GwyHScaleStyle base_style;
    GtkTable *tab;
    gdouble u, l;

    g_return_val_if_fail(GTK_IS_TABLE(table), NULL);
    tab = GTK_TABLE(table);

    base_style = style & ~GWY_HSCALE_CHECK;
    switch (base_style) {
        case GWY_HSCALE_DEFAULT:
        case GWY_HSCALE_NO_SCALE:
        case GWY_HSCALE_LOG:
        case GWY_HSCALE_SQRT:
        if (pivot) {
            g_return_val_if_fail(GTK_IS_ADJUSTMENT(pivot), NULL);
            adj = GTK_ADJUSTMENT(pivot);
        }
        else {
            if (base_style == GWY_HSCALE_LOG || base_style == GWY_HSCALE_SQRT)
                g_warning("Nonlinear scale doesn't work with implicit adj.");
            adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0));
        }
        break;

        case GWY_HSCALE_WIDGET:
        case GWY_HSCALE_WIDGET_NO_EXPAND:
        g_return_val_if_fail(GTK_IS_WIDGET(pivot), NULL);
        break;

        default:
        g_return_val_if_reached(NULL);
        break;
    }

    if (base_style != GWY_HSCALE_WIDGET
        && base_style != GWY_HSCALE_WIDGET_NO_EXPAND) {
        spin = gtk_spin_button_new(adj, 1, 0);
        u = adj->value;
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
        gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
        gtk_table_attach(tab, spin, 2, 3, row, row+1, GTK_FILL, 0, 2, 2);
        gtk_adjustment_set_value(adj, u);
        middle_widget = spin;

        if (base_style == GWY_HSCALE_LOG) {
            u = log(adj->upper);
            l = log(adj->lower);
            scale_adj
                = GTK_ADJUSTMENT(gtk_adjustment_new(log(adj->value),
                                                    l, u,
                                                    (u - l)/GWY_HSCALE_WIDTH,
                                                    10*(u - l)/GWY_HSCALE_WIDTH,
                                                    0));
            g_signal_connect(adj, "value_changed",
                             G_CALLBACK(gwy_hscale_update_log), scale_adj);
            g_signal_connect(scale_adj, "value_changed",
                             G_CALLBACK(gwy_hscale_update_exp), adj);
        }
        else if (base_style == GWY_HSCALE_SQRT) {
            u = sqrt(adj->upper);
            l = sqrt(adj->lower);
            scale_adj
                = GTK_ADJUSTMENT(gtk_adjustment_new(sqrt(adj->value),
                                                    l, u,
                                                    (u - l)/GWY_HSCALE_WIDTH,
                                                    10*(u - l)/GWY_HSCALE_WIDTH,
                                                    0));
            g_signal_connect(adj, "value_changed",
                             G_CALLBACK(gwy_hscale_update_sqrt), scale_adj);
            g_signal_connect(scale_adj, "value_changed",
                             G_CALLBACK(gwy_hscale_update_sq), adj);
        }
        else
            scale_adj = adj;
    }
    else {
        align = middle_widget = GTK_WIDGET(pivot);
        if (base_style == GWY_HSCALE_WIDGET_NO_EXPAND) {
            if (GTK_IS_MISC(middle_widget))
                gtk_misc_set_alignment(GTK_MISC(middle_widget), 0.0, 0.5);
            else {
                align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
                gtk_container_add(GTK_CONTAINER(align), middle_widget);
            }
        }
        gtk_table_attach(GTK_TABLE(table), align, 1, 3, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
    }
    g_object_set_data(G_OBJECT(pivot), "middle_widget", middle_widget);

    if (base_style == GWY_HSCALE_DEFAULT
        || base_style == GWY_HSCALE_LOG
        || base_style == GWY_HSCALE_SQRT) {
        scale = gtk_hscale_new(scale_adj);
        gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
        gtk_widget_set_size_request(scale, GWY_HSCALE_WIDTH, -1);
        gtk_table_attach(tab, scale, 1, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
        g_object_set_data(G_OBJECT(pivot), "scale", scale);
    }


    if (style & GWY_HSCALE_CHECK) {
        check = gtk_check_button_new_with_mnemonic(name);
        gtk_table_attach(tab, check, 0, 1, row, row+1, GTK_FILL, 0, 2, 2);
        g_signal_connect(check, "toggled",
                         G_CALLBACK(gwy_hscale_checkbutton_cb), pivot);
        g_object_set_data(G_OBJECT(pivot), "check", check);
    }
    else {
        label = gtk_label_new_with_mnemonic(name);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        gtk_table_attach(tab, label, 0, 1, row, row+1, GTK_FILL, 0, 2, 2);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), middle_widget);
        g_object_set_data(G_OBJECT(pivot), "label", label);
    }

    if (units) {
        label = gtk_label_new(units);
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        gtk_table_attach(tab, label, 3, 4, row, row+1, GTK_FILL, 0, 2, 2);
        g_object_set_data(G_OBJECT(pivot), "units", label);
    }

    return middle_widget;
}

/************************** Mask colors ****************************/

typedef struct {
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
}

/**
 * gwy_color_selector_for_mask:
 * @dialog_title: Title of the color selection dialog (%NULL to use default).
 * @color_button: Color button to update on color change (or %NULL).
 * @container: Container to initialize the color from and save it to.
 * @prefix: Prefix in @container (normally "/0/mask").
 *
 * Creates and runs a color selector dialog for a mask.
 *
 * Note this function does not return anything, it runs the dialog modally
 * and returns when it finishes.
 **/
void
gwy_color_selector_for_mask(const gchar *dialog_title,
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

    mcsdata = g_new(MaskColorSelectorData, 1);
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

/* TODO: may use "icon" property in Gtk+ 2.6 */
/**
 * gwy_stock_like_button_new:
 * @label_text: Button label text.
 * @stock_id: Button icon stock id.
 *
 * Creates a button that looks like a stock button, but can have different
 * label text.
 *
 * Returns: The newly created button as #GtkWidget.
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

    return button;
}

/**
 * gwy_get_pango_ft2_font_map:
 * @unref: If %TRUE, function removes the font map reference and return %NULL
 *         %NULL.
 *
 * Returns global Pango FT2 font map, eventually creating it.
 *
 * Returns: Pango FT2 font map.  Add your own reference if you want it to
 *          never go away.
 **/
PangoFontMap*
gwy_get_pango_ft2_font_map(gboolean unref)
{
    static PangoFontMap *ft2_font_map = NULL;

    if (unref) {
        gwy_object_unref(ft2_font_map);
        return NULL;
    }

    if (ft2_font_map)
        return ft2_font_map;

    ft2_font_map = pango_ft2_font_map_new();
    gwy_debug_objects_creation(G_OBJECT(ft2_font_map));

    return ft2_font_map;
}

/**
 * gwy_gdk_cursor_new_or_ref:
 * @cursor: A Gdk cursor, or %NULL.
 * @type: Cursor type to eventually create.
 *
 * Increments reference count of a given Gdk cursor or creates a new one
 * (if @cursor is NULL) of type @cursor_type.
 *
 * This function is intended for layer implementation.
 **/
void
gwy_gdk_cursor_new_or_ref(GdkCursor **cursor,
                          GdkCursorType type)
{
    g_return_if_fail(cursor);

    if (*cursor)
        gdk_cursor_ref(*cursor);
    else
        *cursor = gdk_cursor_new(type);
}

/**
 * gwy_gdk_cursor_free_or_unref:
 * @cursor: A Gdk cursor.
 *
 * Decrements reference count of a Gdk cursor, possibly freeing it.
 *
 * This function is intended for layer implementation.
 **/
void
gwy_gdk_cursor_free_or_unref(GdkCursor **cursor)
{
    int refcount;

    g_return_if_fail(cursor);
    g_return_if_fail(*cursor);

    refcount = (*cursor)->ref_count - 1;
    gdk_cursor_unref(*cursor);
    if (!refcount)
        *cursor = NULL;
}

/************************** Documentation ****************************/

/**
 * gwy_adjustment_get_int:
 * @adj: A #GtkAdjustment to get value of.
 *
 * Gets a properly rounded integer value from an adjustment.
 **/

/**
 * GwyHScaleStyle:
 * @GWY_HSCALE_DEFAULT: Default label, hscale, spinbutton, and units widget
 *                      row.
 * @GWY_HSCALE_LOG: Hscale is logarithmic.
 * @GWY_HSCALE_SQRT: Hscale is square root.
 * @GWY_HSCALE_NO_SCALE: There is no hscale.
 * @GWY_HSCALE_WIDGET: An user-specified widget is used in place of hscale and
 *                     spinbutton.
 * @GWY_HSCALE_WIDGET_NO_EXPAND: An user-specified widget is used in place of
 *                               hscale and spinbutton, and it is left-aligned
 *                               instead of taking all the alloted space.
 * @GWY_HSCALE_CHECK: The label is actually a check button that controls
 *                    sensitivity of the row.
 *
 * Options controlling gwy_table_attach_hscale() behaviour.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
