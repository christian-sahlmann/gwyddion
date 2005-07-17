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
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyentities.h>
#include "gwystock.h"
#include "gwyscitext.h"

#define GWY_SCI_TEXT_TYPE_NAME "GwySciText"

enum {
    GWY_SCI_TEXT_BOLD        = 1,
    GWY_SCI_TEXT_ITALIC      = 2,
    GWY_SCI_TEXT_SUBSCRIPT   = 3,
    GWY_SCI_TEXT_SUPERSCRIPT = 4
};

/* Forward declarations - widget related*/
static void     gwy_sci_text_class_init           (GwySciTextClass *klass);
static void     gwy_sci_text_init                 (GwySciText *sci_text);
static void     gwy_sci_text_finalize             (GObject *object);

static void     gwy_sci_text_realize              (GtkWidget *widget);
static void     gwy_sci_text_unrealize            (GtkWidget *widget);
/*
static void     gwy_sci_text_size_request         (GtkWidget *widget,
                                                   GtkRequisition *requisition);
*/
static void     gwy_sci_text_size_allocate        (GtkWidget *widget,
                                                   GtkAllocation *allocation);

/* Forward declarations - sci_text related*/
static void     gwy_sci_text_edited               (GtkEntry *entry);
static void     gwy_sci_text_entity_selected      (GwySciText *sci_text);
static void     gwy_sci_text_button_some_pressed  (GtkButton *button,
                                                   gpointer p);
static GList*   stupid_put_entities               (GList *items);
static GList*   stupid_put_entity                 (GList *items, gsize i);

/* Local data */
static GtkWidgetClass *parent_class = NULL;

const GwyTextEntity *ENTITIES = NULL;


GType
gwy_sci_text_get_type(void)
{
    static GType gwy_sci_text_type = 0;

    if (!gwy_sci_text_type) {
        static const GTypeInfo gwy_sci_text_info = {
            sizeof(GwySciTextClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_sci_text_class_init,
            NULL,
            NULL,
            sizeof(GwySciText),
            0,
            (GInstanceInitFunc)gwy_sci_text_init,
            NULL,
        };
        gwy_debug("");
        gwy_sci_text_type = g_type_register_static(GTK_TYPE_VBOX,
                                                      GWY_SCI_TEXT_TYPE_NAME,
                                                      &gwy_sci_text_info,
                                                      0);
    }

    return gwy_sci_text_type;
}

static void
gwy_sci_text_class_init(GwySciTextClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_sci_text_finalize;

    widget_class->realize = gwy_sci_text_realize;
    widget_class->unrealize = gwy_sci_text_unrealize;
    widget_class->size_allocate = gwy_sci_text_size_allocate;

    ENTITIES = gwy_entities_get_entities();
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
gwy_sci_text_init(GwySciText *sci_text)
{
    GtkWidget *lab1, *frame, *lower, *upper, *bold, *italic, *add, *hbox;
    GList *items = NULL;

    gwy_debug("");

    lab1 = gtk_label_new_with_mnemonic("Enter hyper_text");
    gtk_misc_set_alignment(GTK_MISC(lab1), 0.0, 0.5);
    frame = gtk_frame_new("Preview");
    gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
    sci_text->entry = GTK_ENTRY(gtk_entry_new());
    gtk_label_set_mnemonic_widget(GTK_LABEL(lab1), GTK_WIDGET(sci_text->entry));
    sci_text->label = GTK_LABEL(gtk_label_new(" "));
    sci_text->entities = GTK_COMBO(gtk_combo_new());
    lower = gwy_image_button_new_from_stock(GWY_STOCK_SUBSCRIPT);
    upper = gwy_image_button_new_from_stock(GWY_STOCK_SUPERSCRIPT);
    bold = gwy_image_button_new_from_stock(GWY_STOCK_BOLD);
    italic = gwy_image_button_new_from_stock(GWY_STOCK_ITALIC);
    add = gtk_button_new_with_mnemonic("A_dd symbol");
    hbox = gtk_hbox_new(FALSE, 0);

    items = stupid_put_entities(NULL);
    gtk_combo_set_popdown_strings(GTK_COMBO(sci_text->entities), items);

    gtk_editable_set_editable(GTK_EDITABLE(sci_text->entities->entry), FALSE);

    gtk_widget_show(lab1);
    gtk_widget_show(frame);
    gtk_widget_show(add);
    gtk_widget_show(upper);
    gtk_widget_show(lower);
    gtk_widget_show(bold);
    gtk_widget_show(italic);

    gtk_widget_show(GTK_WIDGET(sci_text->entry));
    gtk_widget_show(GTK_WIDGET(sci_text->label));
    gtk_widget_show(GTK_WIDGET(sci_text->entities));

    gtk_box_pack_start(GTK_BOX(sci_text), lab1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sci_text), GTK_WIDGET(sci_text->entry),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sci_text), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sci_text), frame, TRUE, FALSE, 6);
    gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(sci_text->label));

    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(sci_text->entities),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), add, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), bold, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), italic, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), lower, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), upper, FALSE, FALSE, 0);

    gtk_widget_set_events(GTK_WIDGET(sci_text->entry), GDK_KEY_RELEASE_MASK);
    gtk_widget_set_events(sci_text->entities->list, GDK_BUTTON_PRESS_MASK);

    g_signal_connect(sci_text->entry, "changed",
                     G_CALLBACK(gwy_sci_text_edited), NULL);
    g_signal_connect_swapped(add, "clicked",
                             G_CALLBACK(gwy_sci_text_entity_selected),
                             sci_text);
    g_signal_connect(bold, "clicked",
                     G_CALLBACK(gwy_sci_text_button_some_pressed),
                     GINT_TO_POINTER(GWY_SCI_TEXT_BOLD));
    g_signal_connect(italic, "clicked",
                     G_CALLBACK(gwy_sci_text_button_some_pressed),
                     GINT_TO_POINTER(GWY_SCI_TEXT_ITALIC));
    g_signal_connect(upper, "clicked",
                     G_CALLBACK(gwy_sci_text_button_some_pressed),
                     GINT_TO_POINTER(GWY_SCI_TEXT_SUPERSCRIPT));
    g_signal_connect(lower, "clicked",
                     G_CALLBACK(gwy_sci_text_button_some_pressed),
                     GINT_TO_POINTER(GWY_SCI_TEXT_SUBSCRIPT));

}

GtkWidget*
gwy_sci_text_new()
{
    GwySciText *sci_text;

    gwy_debug("");

    sci_text = (GwySciText*)gtk_object_new(gwy_sci_text_get_type (), NULL);

    sci_text->par.label_font = pango_font_description_new();
    pango_font_description_set_family(sci_text->par.label_font,
                                      "Helvetica");
    pango_font_description_set_style(sci_text->par.label_font,
                                     PANGO_STYLE_NORMAL);
    pango_font_description_set_variant(sci_text->par.label_font,
                                       PANGO_VARIANT_NORMAL);
    pango_font_description_set_weight(sci_text->par.label_font,
                                      PANGO_WEIGHT_NORMAL);
    pango_font_description_set_size(sci_text->par.label_font,
                                    12*PANGO_SCALE);

    return GTK_WIDGET(sci_text);
}

static void
gwy_sci_text_finalize(GObject *object)
{
    gwy_debug("finalizing a GwySciText %d (refcount = %u)",
              (gint*)object, object->ref_count);

    g_return_if_fail(GWY_IS_SCI_TEXT(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_sci_text_unrealize(GtkWidget *widget)
{
    GwySciText *sci_text;

    sci_text = GWY_SCI_TEXT(widget);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static void
gwy_sci_text_realize(GtkWidget *widget)
{

    gwy_debug("realizing a GwySciText (%ux%u)",
              widget->allocation.x, widget->allocation.height);

    if (GTK_WIDGET_CLASS(parent_class)->realize)
    GTK_WIDGET_CLASS(parent_class)->realize(widget);

}

/*
static void
gwy_sci_text_size_request(GtkWidget *widget,
                          GtkRequisition *requisition)
{
    GwySciText *sci_text;
    gwy_debug("");

    sci_text = GWY_SCI_TEXT(widget);

    requisition->width = 80;
    requisition->height = 100;


}
*/

static void
gwy_sci_text_size_allocate(GtkWidget *widget,
                           GtkAllocation *allocation)
{
    gwy_debug("");

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_SCI_TEXT(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;
    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);

}

static void
gwy_sci_text_edited(GtkEntry *entry)
{
    GwySciText *sci_text;
    GError *err = NULL;
    PangoAttrList *attr_list = NULL;
    gchar *text = NULL;
    gchar *utf8;

    gwy_debug("");

    sci_text = GWY_SCI_TEXT(gtk_widget_get_ancestor(GTK_WIDGET(entry),
                                                    GWY_TYPE_SCI_TEXT));

    utf8 = gwy_entities_text_to_utf8(gtk_entry_get_text(entry));
    if (pango_parse_markup(utf8, -1, 0, &attr_list, &text, NULL, &err))
        gtk_label_set_markup(sci_text->label, utf8);
    g_free(utf8);
    g_free(text);
    if (attr_list)
        pango_attr_list_unref(attr_list);
    g_clear_error(&err);
}

static void
gwy_sci_text_entity_selected(GwySciText *sci_text)
{
    GtkEditable *editable;
    gint pos;
    gchar *text, *p;
    const gchar *utf8;

    gwy_debug("");

    editable = GTK_EDITABLE(sci_text->entry);
    text = gtk_editable_get_chars(GTK_EDITABLE(sci_text->entities->entry),
                                  0, -1);
    p = strrchr(text, ' ');
    g_assert(p);
    p++;
    utf8 = gwy_entities_entity_to_utf8(p);
    p = g_strconcat("&", p, ";", NULL);
    pos = gtk_editable_get_position(editable);
    gtk_editable_insert_text(editable, p, strlen(p), &pos);
    gtk_editable_set_position(editable, pos);
    g_free(text);
    g_free(p);
    gwy_sci_text_edited(sci_text->entry);
}


static void
gwy_sci_text_button_some_pressed(GtkButton *button, gpointer p)
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
    GwySciText *sci_text;
    GtkEditable *editable;
    gboolean selected;
    gint i, start, end;
    gsize j;

    gwy_debug("%p", p);
    i = GPOINTER_TO_INT(p);
    sci_text = GWY_SCI_TEXT(gtk_widget_get_ancestor(GTK_WIDGET(button),
                                                    GWY_TYPE_SCI_TEXT));
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
    gwy_sci_text_edited(sci_text->entry);
}


static GList*
stupid_put_entity(GList *items, gsize i)
{
    GString *text, *entity;
    text = g_string_new("");
    entity = g_string_new("");

    g_string_assign(text, ENTITIES[i].entity);
    g_string_assign(entity, ENTITIES[i].utf8);
    g_string_prepend(text, "  ");
    g_string_prepend(text, entity->str);
    items = g_list_append(items, text->str);

    g_string_free(entity, 1);
    return items;
}


static GList*
stupid_put_entities(GList *items)
{
    gsize i;

    for (i = 0; ENTITIES[i].entity; i++)
        items = stupid_put_entity(items, i);

    return items;
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
