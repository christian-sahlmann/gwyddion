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
#include <libgwydgets/gwydgetutils.h>

#define GWY_SCI_TEXT_TYPE_NAME "GwySciText"

enum {
    GWY_SCI_TEXT_BOLD        = 1,
    GWY_SCI_TEXT_ITALIC      = 2,
    GWY_SCI_TEXT_SUBSCRIPT   = 3,
    GWY_SCI_TEXT_SUPERSCRIPT = 4
};

enum {
    EDITED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_HAS_PREVIEW
};

static void       gwy_sci_text_set_property       (GObject *object,
                                                   guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void       gwy_sci_text_get_property       (GObject*object,
                                                   guint prop_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static void       gwy_sci_text_edited             (GwySciText *sci_text);
static void       gwy_sci_text_entity_selected    (GwySciText *sci_text);
static void       gwy_sci_text_button_some_pressed(GwySciText *sci_text,
                                                   GtkWidget *button);

static guint sci_text_signals[LAST_SIGNAL] = { 0 };

static struct {
    gint tag;
    const gchar *stock_id;
    const gchar *start;
    const gchar *end;
    gsize len;
    const gchar *label;
}
const tags[] = {
    {
        GWY_SCI_TEXT_BOLD,
        GWY_STOCK_BOLD,
        "<b>", "</b>", sizeof("<b>")-1,
        N_("_Bold"),
    },
    {
        GWY_SCI_TEXT_ITALIC,
        GWY_STOCK_ITALIC,
        "<i>", "</i>", sizeof("<i>")-1,
        N_("_Italic"),
    },
    {
        GWY_SCI_TEXT_SUBSCRIPT,
        GWY_STOCK_SUBSCRIPT,
        "<sub>", "</sub>", sizeof("<sub>")-1,
        N_("_Subscript"),
    },
    {
        GWY_SCI_TEXT_SUPERSCRIPT,
        GWY_STOCK_SUPERSCRIPT,
        "<sup>", "</sup>", sizeof("<sup>")-1,
        N_("Su_perscript"),
    },
};

G_DEFINE_TYPE(GwySciText, gwy_sci_text, GTK_TYPE_VBOX)

static void
gwy_sci_text_class_init(GwySciTextClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = gwy_sci_text_set_property;
    gobject_class->get_property = gwy_sci_text_get_property;

    /**
     * GwySciText:has-preview:
     *
     * The :has-preview property controls whether a #GwySciText has a preview.
     */
    g_object_class_install_property
        (gobject_class,
         PROP_HAS_PREVIEW,
         g_param_spec_boolean("has-preview",
                              "Has preview",
                              "Whether sci text has a preview",
                              TRUE, G_PARAM_READWRITE));

    /**
     * GwySciText::edited:
     * @gwyscitext: The #GwySciText which received the signal.
     *
     * The ::edited signal is emitted when the text in its entry changes
     * to a valid markup.  If you need to react to all changes in entry
     * contents, you can use gwy_sci_text_get_entry() to get the entry and
     * connect to its signal.
     */
    sci_text_signals[EDITED]
        = g_signal_new("edited",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwySciTextClass, edited),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_sci_text_init(GwySciText *sci_text)
{
    sci_text->has_preview = TRUE;
}

static void
gwy_sci_text_set_property(GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    GwySciText *sci_text = GWY_SCI_TEXT(object);

    switch (prop_id) {
        case PROP_HAS_PREVIEW:
        gwy_sci_text_set_has_preview(sci_text, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_sci_text_get_property(GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    GwySciText *sci_text = GWY_SCI_TEXT(object);

    switch (prop_id) {
        case PROP_HAS_PREVIEW:
        g_value_set_boolean(value, sci_text->preview != NULL);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

GtkWidget*
gwy_sci_text_new(void)
{
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
    g_signal_connect_swapped(sci_text->entry, "changed",
                             G_CALLBACK(gwy_sci_text_edited), sci_text);
    gtk_widget_show(sci_text->entry);

    /* Symbols */
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sci_text), hbox, FALSE, FALSE, 2);

    model = GTK_TREE_MODEL(gwy_inventory_store_new(gwy_entities()));
    sci_text->symbols = gtk_combo_box_new();
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(sci_text->symbols), 10);
    gtk_combo_box_set_model(GTK_COMBO_BOX(sci_text->symbols), model);
    g_object_unref(model);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sci_text->symbols), 0);
    gtk_box_pack_start(GTK_BOX(hbox), sci_text->symbols, FALSE, FALSE, 0);

    /*  a compact cell layout for the popup (in table mode)  */
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

    button = gtk_button_new_with_mnemonic("A_dd symbol");
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_sci_text_entity_selected),
                             sci_text);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 4);

    /* Tag buttons */
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sci_text), hbox, FALSE, FALSE, 2);

    for (i = 0; i < G_N_ELEMENTS(tags); i++) {
        button = gwy_stock_like_button_new(_(tags[i].label),
                                           tags[i].stock_id);
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(gwy_sci_text_button_some_pressed),
                                 sci_text);
        g_object_set_data(G_OBJECT(button), "type",
                          GINT_TO_POINTER(tags[i].tag));
    }
    gtk_widget_show_all(hbox);

    /* Preview */
    sci_text->frame = gtk_frame_new(_("Preview"));
    gtk_box_pack_start(GTK_BOX(sci_text), sci_text->frame, FALSE, FALSE, 0);
    if (sci_text->has_preview) {
        sci_text->has_preview = FALSE;
        gwy_sci_text_set_has_preview(sci_text, TRUE);
    }

    return (GtkWidget*)sci_text;
}

static void
gwy_sci_text_edited(GwySciText *sci_text)
{
    GError *err = NULL;
    PangoAttrList *attr_list = NULL;
    gchar *text = NULL;
    gchar *utf8;
    const gchar *input;
    gboolean emit_edited = FALSE;

    gwy_debug(" ");

    input = gtk_entry_get_text(GTK_ENTRY(sci_text->entry));
    utf8 = gwy_entities_text_to_utf8(input);
    if (pango_parse_markup(utf8, -1, 0, &attr_list, &text, NULL, &err)) {
        if (sci_text->has_preview)
            gtk_label_set_markup(GTK_LABEL(sci_text->preview), utf8);
        emit_edited = TRUE;
    }
    g_free(utf8);
    g_free(text);
    if (attr_list)
        pango_attr_list_unref(attr_list);
    g_clear_error(&err);

    if (emit_edited)
        g_signal_emit(sci_text, sci_text_signals[EDITED], 0);
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
}

static void
gwy_sci_text_button_some_pressed(GwySciText *sci_text,
                                 GtkWidget *button)
{
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
        if (tags[j].tag == i) {
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

gboolean
gwy_sci_text_get_has_preview(GwySciText *sci_text)
{
    g_return_val_if_fail(GWY_IS_SCI_TEXT(sci_text), FALSE);
    return sci_text->has_preview;
}

void
gwy_sci_text_set_has_preview(GwySciText *sci_text,
                             gboolean has_preview)
{
    GtkWidget *hbox;

    g_return_if_fail(GWY_IS_SCI_TEXT(sci_text));

    if (!sci_text->frame) {
        sci_text->has_preview = has_preview;
        return;
    }

    if (sci_text->has_preview && !has_preview) {
        gtk_widget_hide(sci_text->frame);
        hbox = gtk_bin_get_child(GTK_BIN(sci_text->frame));
        sci_text->preview = NULL;
        gtk_widget_destroy(hbox);
        sci_text->has_preview = FALSE;
    }
    else if (!sci_text->has_preview && has_preview) {
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(sci_text->frame), hbox);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);

        sci_text->preview = gtk_label_new(NULL);
        gtk_box_pack_start(GTK_BOX(hbox), sci_text->preview, TRUE, TRUE, 0);
        sci_text->has_preview = TRUE;

        gwy_sci_text_edited(sci_text);
        gtk_widget_show_all(sci_text->frame);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
