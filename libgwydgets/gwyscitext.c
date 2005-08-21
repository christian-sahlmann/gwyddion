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
#include <string.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyentities.h>
#include <libgwyddion/gwyenum.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwydgets/gwyscitext.h>

#define GWY_SCI_TEXT_TYPE_NAME "GwySciText"

enum {
    GWY_SCI_TEXT_BOLD        = 1,
    GWY_SCI_TEXT_ITALIC      = 2,
    GWY_SCI_TEXT_SUBSCRIPT   = 3,
    GWY_SCI_TEXT_SUPERSCRIPT = 4
};

static GtkWidget* gwy_image_button_new_from_stock (const gchar *stock_id);
static void       gwy_sci_text_edited             (GwySciText *sci_text);
static void       gwy_sci_text_entity_selected    (GwySciText *sci_text);
static void       gwy_sci_text_button_some_pressed(GwySciText *sci_text,
                                                   GtkWidget *button);

G_DEFINE_TYPE(GwySciText, gwy_sci_text, GTK_TYPE_VBOX)

static void
gwy_sci_text_class_init(G_GNUC_UNUSED GwySciTextClass *klass)
{
}

static void
gwy_sci_text_init(G_GNUC_UNUSED GwySciText *sci_text)
{
}

GtkWidget*
gwy_sci_text_new(void)
{
    static const GwyEnum buttons[] = {
        { GWY_STOCK_BOLD,        GWY_SCI_TEXT_BOLD,        },
        { GWY_STOCK_ITALIC,      GWY_SCI_TEXT_ITALIC,      },
        { GWY_STOCK_SUBSCRIPT,   GWY_SCI_TEXT_SUBSCRIPT,   },
        { GWY_STOCK_SUPERSCRIPT, GWY_SCI_TEXT_SUPERSCRIPT, },
    };
    GtkWidget *label, *button, *hbox;
    GtkTreeModel *model;
    GtkCellLayout *layout;
    GtkCellRenderer *cell;
    GwySciText *sci_text;
    gint i;

    sci_text = (GwySciText*)g_object_new(GWY_TYPE_SCI_TEXT, NULL);

    /* Entry */
    label = gtk_label_new_with_mnemonic("Hyper_text");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(sci_text), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    sci_text->entry = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), sci_text->entry);
    gtk_box_pack_start(GTK_BOX(sci_text), sci_text->entry, FALSE, FALSE, 0);
    /*gtk_widget_add_events(GTK_WIDGET(sci_text->entry), GDK_KEY_RELEASE_MASK);*/
    g_signal_connect_swapped(sci_text->entry, "changed",
                             G_CALLBACK(gwy_sci_text_edited), sci_text);
    gtk_widget_show(sci_text->entry);

    /* Controls */
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sci_text), hbox, FALSE, FALSE, 4);

    /* Symbols */
    model = GTK_TREE_MODEL(gwy_inventory_store_new(gwy_entities()));
    sci_text->symbols = gtk_combo_box_new_with_model(model);
    g_object_unref(model);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sci_text->symbols), 0);
    gtk_box_pack_start(GTK_BOX(hbox), sci_text->symbols, FALSE, FALSE, 0);

    /*  a compact cell layout for the popup (in table mode)  */
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(sci_text->symbols), 10);
    layout = GTK_CELL_LAYOUT(sci_text->symbols);

    i = gwy_inventory_store_get_column_by_name(GWY_INVENTORY_STORE(model),
                                               "utf8");
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(layout, cell, FALSE);
    gtk_cell_layout_set_attributes(layout, cell, "text", i, NULL);

    /*  use a more descriptive cell layout for the box itself  */
    layout = GTK_CELL_LAYOUT(GTK_BIN(sci_text->symbols)->child);
    gtk_cell_layout_clear(layout);

    i = gwy_inventory_store_get_column_by_name(GWY_INVENTORY_STORE(model),
                                               "utf8");
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(layout, cell, FALSE);
    gtk_cell_layout_set_attributes(layout, cell, "text", i, NULL);

    i = gwy_inventory_store_get_column_by_name(GWY_INVENTORY_STORE(model),
                                               "entity");
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(layout, cell, TRUE);
    gtk_cell_layout_set_attributes(layout, cell, "text", i, NULL);

    /* Add */
    button = gtk_button_new_with_mnemonic("A_dd symbol");
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_sci_text_entity_selected),
                             sci_text);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 8);

    /* Other buttons */
    for (i = 0; i < G_N_ELEMENTS(buttons); i++) {
        button = gwy_image_button_new_from_stock(buttons[i].name);
        gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(gwy_sci_text_button_some_pressed),
                                 sci_text);
        g_object_set_data(G_OBJECT(button), "type",
                          GINT_TO_POINTER(buttons[i].value));
    }
    gtk_widget_show_all(hbox);

    /* Preview */
    label = gtk_label_new_with_mnemonic("Preview");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(sci_text), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    sci_text->preview = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(sci_text), sci_text->preview, FALSE, FALSE, 4);
    gtk_widget_show(label);

    return (GtkWidget*)sci_text;
}

static GtkWidget*
gwy_image_button_new_from_stock(const gchar *stock_id)
{
    GtkWidget *image, *button;

    image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
    button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button), image);

    return button;
}

static void
gwy_sci_text_edited(GwySciText *sci_text)
{
    GError *err = NULL;
    PangoAttrList *attr_list = NULL;
    gchar *text = NULL;
    gchar *utf8;
    const gchar *input;

    gwy_debug(" ");

    input = gtk_entry_get_text(GTK_ENTRY(sci_text->entry));
    utf8 = gwy_entities_text_to_utf8(input);
    if (pango_parse_markup(utf8, -1, 0, &attr_list, &text, NULL, &err))
        gtk_label_set_markup(GTK_LABEL(sci_text->preview), utf8);
    g_free(utf8);
    g_free(text);
    if (attr_list)
        pango_attr_list_unref(attr_list);
    g_clear_error(&err);
}

static void
gwy_sci_text_entity_selected(GwySciText *sci_text)
{
    GtkTreeModel *model;
    GtkEditable *editable;
    GtkTreeIter iter;
    GwyTextEntity *entity;
    gchar *p;
    gint pos;

    gwy_debug("");

    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(sci_text->symbols), &iter))
        return;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(sci_text->symbols));
    gtk_tree_model_get(model, &iter, 0, &entity, -1);

    p = g_strconcat("&", entity->entity, ";", NULL);

    editable = GTK_EDITABLE(sci_text->entry);
    pos = gtk_editable_get_position(editable);
    gtk_editable_insert_text(editable, p, strlen(p), &pos);
    gtk_editable_set_position(editable, pos);
    g_free(p);

    gwy_sci_text_edited(sci_text);
}


static void
gwy_sci_text_button_some_pressed(GwySciText *sci_text,
                                 GtkWidget *button)
{
    static struct {
        gint markup;
        const gchar *start;
        const gchar *end;
        gsize len;
    }
    const tags[] = {
        { GWY_SCI_TEXT_BOLD, "<b>", "</b>", sizeof("<b>")-1 },
        { GWY_SCI_TEXT_ITALIC, "<i>", "</i>", sizeof("<i>")-1 },
        { GWY_SCI_TEXT_SUBSCRIPT, "<sub>", "</sub>", sizeof("<sub>")-1 },
        { GWY_SCI_TEXT_SUPERSCRIPT, "<sup>", "</sup>", sizeof("<sup>")-1 },
    };
    GtkEditable *editable;
    gboolean selected;
    gint i, start, end;
    gsize j;

    i = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "type"));
    editable = GTK_EDITABLE(sci_text->entry);
    selected = gtk_editable_get_selection_bounds(editable, &start, &end);
    if (!selected) {
        start = gtk_editable_get_position(editable);
        end = start;
    }
    for (j = 0; j < G_N_ELEMENTS(tags); j++) {
        if (tags[j].markup == i) {
            gtk_editable_insert_text(editable,
                                     tags[j].start, tags[j].len, &start);
            end += tags[j].len;
            gtk_editable_insert_text(editable,
                                     tags[j].end, tags[j].len+1, &end);
            gtk_widget_grab_focus(GTK_WIDGET(sci_text->entry));
            if (selected)
                gtk_editable_select_region(editable, start, end-tags[j].len-1);
            else
                gtk_editable_set_position(editable, start);
            break;
        }
    }
    gwy_sci_text_edited(sci_text);
}

/**
 * gwy_sci_text_get_text:
 * @sci_text: A science text widget.
 *
 * Returns the text.
 *
 * The text is already in UTF-8 with all entities converted.
 *
 * Returns: The text as a newly allocated string. It should be freed when no
 *          longer used.
 **/
gchar*
gwy_sci_text_get_text(GwySciText *sci_text)
{
    gchar *text, *utf8;

    text = gtk_editable_get_chars(GTK_EDITABLE(sci_text->entry), 0, -1);
    utf8 = gwy_entities_text_to_utf8(text);
    g_free(text);

    return utf8;
}

/**
 * gwy_sci_text_set_text:
 * @sci_text: A science text widget.
 * @new_text: The text to display.
 *
 * Sets the text a science text widget displays.
 *
 * It can contain both UTF-8 and entities, but attempt to convert UTF-8
 * `back' to entities is made.
 **/
void
gwy_sci_text_set_text(GwySciText *sci_text, const gchar *new_text)
{
    g_return_if_fail(GWY_IS_SCI_TEXT(sci_text));
    gtk_entry_set_text(GTK_ENTRY(sci_text->entry), new_text);

}

GtkWidget*
gwy_sci_text_get_entry(GwySciText *sci_text)
{
    g_return_val_if_fail(GWY_IS_SCI_TEXT(sci_text), NULL);
    return sci_text->entry;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
