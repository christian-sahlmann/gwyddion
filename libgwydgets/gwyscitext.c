/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyentities.h>
#include "gwyscitext.h"

#define GWY_SCI_TEXT_TYPE_NAME "GwySciText"

#define GWY_SCI_TEXT_BOLD      1
#define GWY_SCI_TEXT_ITALIC    2 
#define GWY_SCI_TEXT_SUBSCRIPT 3
#define GWY_SCI_TEXT_SUPERSCRIPT 4



/* Forward declarations - widget related*/
static void     gwy_sci_text_class_init           (GwySciTextClass *klass);
static void     gwy_sci_text_init                 (GwySciText *sci_text);
static void     gwy_sci_text_finalize             (GObject *object);

static void     gwy_sci_text_realize              (GtkWidget *widget);
static void     gwy_sci_text_unrealize            (GtkWidget *widget);
static void     gwy_sci_text_size_request         (GtkWidget *widget,
                                                      GtkRequisition *requisition);
static void     gwy_sci_text_size_allocate        (GtkWidget *widget,
                                                      GtkAllocation *allocation);
static gboolean gwy_sci_text_expose               (GtkWidget *widget,
                                                      GdkEventExpose *event);

/* Forward declarations - sci_text related*/
static void     gwy_sci_text_edited               (GtkEntry *entry);
static void     gwy_sci_text_entity_selected      (GtkEntry *entry);
static void     gwy_sci_text_button_bold_pressed  (GtkButton *button);
static void     gwy_sci_text_button_italic_pressed(GtkButton *button);
static void     gwy_sci_text_button_upper_pressed (GtkButton *button);
static void     gwy_sci_text_button_lower_pressed (GtkButton *button);
static void     gwy_sci_text_button_some_pressed  (GtkButton *button, gint i);
static void     stupid_put_entities               (GList *items);
static void     stupid_put_entity                 (GList *items, gint i);

/* Local data */
const gchar* SOME_ENTITIES[] = {
    "alpha",
    "beta",
    "gamma",
    "delta",
    "epsilon",
    "omega",
    "infin",
    "nabla",
    "rceil",
    "rfloor",
    "lceil",
    "lfloor",
    "int",
};
    
static GtkWidgetClass *parent_class = NULL;


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
        gwy_debug("%s", __FUNCTION__);
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

    gwy_debug("%s", __FUNCTION__);

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_sci_text_finalize;

    widget_class->realize = gwy_sci_text_realize;
/*    widget_class->expose_event = gwy_sci_text_expose;*/
/*    widget_class->size_request = gwy_sci_text_size_request;*/
    widget_class->unrealize = gwy_sci_text_unrealize;
    widget_class->size_allocate = gwy_sci_text_size_allocate;

}

static void
gwy_sci_text_init(GwySciText *sci_text)
{
    GtkWidget *lab1, *lab2;
    GList *items = NULL;
    GtkFrame *frame;
    GtkButton *lower, *upper, *bold, *italic;
    GtkHBox *hbox;
    gwy_debug("%s", __FUNCTION__);

    lab1 = gtk_label_new("Entry hypertext:");
    lab2 = gtk_label_new(" ");
    frame = gtk_frame_new("Resulting label:");
    sci_text->entry = GTK_ENTRY(gtk_entry_new());
    sci_text->label = gtk_label_new(" ");
    sci_text->entities = gtk_combo_new(); 
    lower = gtk_button_new_with_label("x_y");
    upper = gtk_button_new_with_label("x^y");
    bold = gtk_button_new_with_label("B");
    italic = gtk_button_new_with_label("I");
    hbox = gtk_hbox_new(0,0);

    items = g_list_append (items,"choose symbol"); 
    stupid_put_entities(items);
    gtk_combo_set_popdown_strings (GTK_COMBO (sci_text->entities), items);
    
    gtk_editable_set_editable(GTK_EDITABLE((sci_text->entities)->entry), 0);
     
    gtk_widget_show(lab1);
    gtk_widget_show(frame);
    gtk_widget_show(upper);
    gtk_widget_show(lower);
    gtk_widget_show(bold);
    gtk_widget_show(italic);
    
    gtk_widget_show(GTK_WIDGET(sci_text->entry));
    gtk_widget_show(GTK_WIDGET(sci_text->label));
    gtk_widget_show(GTK_WIDGET(sci_text->entities));
    
    gtk_box_pack_start(GTK_VBOX(sci_text), GTK_WIDGET(lab1), 0, 0, 0);
    gtk_box_pack_start(GTK_VBOX(sci_text), GTK_WIDGET(sci_text->entry), 0, 0, 0);
    gtk_box_pack_start(GTK_VBOX(sci_text), GTK_WIDGET(hbox), 0, 0, 0);
    gtk_box_pack_start(GTK_VBOX(sci_text), GTK_WIDGET(lab2), 0, 0, 0);
    gtk_box_pack_start(GTK_VBOX(sci_text), GTK_WIDGET(frame), 1, 0, 0);
    gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(sci_text->label));

    gtk_box_pack_start(GTK_HBOX(hbox), GTK_WIDGET(sci_text->entities), 0, 0, 0);
    gtk_box_pack_start(GTK_HBOX(hbox), GTK_WIDGET(bold), 0, 0, 0);
    gtk_box_pack_start(GTK_HBOX(hbox), GTK_WIDGET(italic), 0, 0, 0);
    gtk_box_pack_start(GTK_HBOX(hbox), GTK_WIDGET(lower), 0, 0, 0);
    gtk_box_pack_start(GTK_HBOX(hbox), GTK_WIDGET(upper), 0, 0, 0);

    gtk_widget_set_events (sci_text->entry, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_events (sci_text->entities->list, GDK_BUTTON_PRESS_MASK);
    
    g_signal_connect(sci_text->entry, "key_release_event", G_CALLBACK(gwy_sci_text_edited), NULL);
    g_signal_connect(sci_text->entities->entry, "changed", G_CALLBACK(gwy_sci_text_entity_selected), NULL);
    g_signal_connect(bold, "button_press_event", G_CALLBACK(gwy_sci_text_button_bold_pressed), NULL);
    g_signal_connect(italic, "button_press_event", G_CALLBACK(gwy_sci_text_button_italic_pressed), NULL);
    g_signal_connect(upper, "button_press_event", G_CALLBACK(gwy_sci_text_button_upper_pressed), NULL);
    g_signal_connect(lower, "button_press_event", G_CALLBACK(gwy_sci_text_button_lower_pressed), NULL);
    
}

GtkWidget*
gwy_sci_text_new()
{
    GwySciText *sci_text;

    gwy_debug("%s", __FUNCTION__);

    sci_text = gtk_object_new (gwy_sci_text_get_type (), NULL);

    sci_text->par.label_font = pango_font_description_new();
    pango_font_description_set_family(sci_text->par.label_font, "Helvetica");
    pango_font_description_set_style(sci_text->par.label_font, PANGO_STYLE_NORMAL);
    pango_font_description_set_variant(sci_text->par.label_font, PANGO_VARIANT_NORMAL);
    pango_font_description_set_weight(sci_text->par.label_font, PANGO_WEIGHT_NORMAL);
    pango_font_description_set_size(sci_text->par.label_font, 12*PANGO_SCALE);

    return GTK_WIDGET(sci_text);
}

static void
gwy_sci_text_finalize(GObject *object)
{
    GwySciText *sci_text;

    gwy_debug("finalizing a GwySciText (refcount = %u)", object->ref_count);

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_SCI_TEXT(object));

    sci_text = GWY_SCI_TEXT(object);

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

static void
gwy_sci_text_size_request(GtkWidget *widget,
                             GtkRequisition *requisition)
{
    GwySciText *sci_text;
    gwy_debug("%s", __FUNCTION__);

    sci_text = GWY_SCI_TEXT(widget);

    requisition->width = 80;
    requisition->height = 100;
    

}

static void
gwy_sci_text_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GwySciText *sci_text;

    gwy_debug("%s", __FUNCTION__);

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_SCI_TEXT(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;
    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);

    sci_text = GWY_SCI_TEXT(widget);
    if (GTK_WIDGET_REALIZED(widget)) {

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
    }
    

}

static void
gwy_sci_text_edited(GtkEntry *entry)
{
    GwySciText *sci_text;
    GError *err = NULL;
    PangoAttrList *attr_list = NULL;
    gchar *text = NULL;
                    
    gwy_debug("%s", __FUNCTION__);

    sci_text = gtk_widget_get_parent(entry);

    if (pango_parse_markup(gwy_entities_text_to_utf8(gtk_entry_get_text(entry)), -1, 0,
                           &attr_list, &text, NULL, &err))
    gtk_label_set_markup(sci_text->label, gwy_entities_text_to_utf8(gtk_entry_get_text(entry)));
}

static void
gwy_sci_text_entity_selected(GtkEntry *entry)
{
    GwySciText *sci_text;
    GString *entity;
    gint i, n, pos;
    gwy_debug("%s", __FUNCTION__);

    sci_text = gtk_widget_get_parent(gtk_widget_get_parent(gtk_widget_get_parent(entry)));
    g_assert(GWY_IS_SCI_TEXT(sci_text));
    
    entity = g_string_new("");

    /*put entity into text entry*/
    n = G_N_ELEMENTS(SOME_ENTITIES) - 1;
    for (i=0; i<n; i++)
    {
        if (strstr(gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1), SOME_ENTITIES[i]))
        {
            pos = gtk_editable_get_position(GTK_EDITABLE(sci_text->entry));
            g_string_assign(entity, SOME_ENTITIES[i]);
            g_string_append(entity, ";");
            g_string_prepend(entity, "&");
            gtk_editable_insert_text(GTK_EDITABLE(sci_text->entry), entity->str, entity->len, &pos);
        }
    }
    gwy_sci_text_edited(sci_text->entry); 
}


static void     
gwy_sci_text_button_some_pressed(GtkButton *button, gint i)
{
    GwySciText *sci_text;
    gwy_debug("%s", __FUNCTION__);
    gint start, end;

    sci_text = gtk_widget_get_parent(gtk_widget_get_parent(button));
    if (!gtk_editable_get_selection_bounds(GTK_EDITABLE(sci_text->entry),
                                          &start, &end))
    {
        start = gtk_editable_get_position(GTK_EDITABLE(sci_text->entry));
        end = start;
    }
    if (i == GWY_SCI_TEXT_BOLD)
    {
        gtk_editable_insert_text(GTK_EDITABLE(sci_text->entry), "<b>", 3, &start);
        end += 3;
        gtk_editable_insert_text(GTK_EDITABLE(sci_text->entry), "</b>", 4, &end);
    }
    else if (i == GWY_SCI_TEXT_ITALIC)
    {
        gtk_editable_insert_text(GTK_EDITABLE(sci_text->entry), "<i>", 3, &start);
        end += 3;
        gtk_editable_insert_text(GTK_EDITABLE(sci_text->entry), "</i>", 4, &end);
    }
    else if (i == GWY_SCI_TEXT_SUBSCRIPT)
    {
        gtk_editable_insert_text(GTK_EDITABLE(sci_text->entry), "<sub>", 5, &start);
        end += 5;
        gtk_editable_insert_text(GTK_EDITABLE(sci_text->entry), "</sub>", 6, &end);
    }
    else if (i == GWY_SCI_TEXT_SUPERSCRIPT)
    {
        gtk_editable_insert_text(GTK_EDITABLE(sci_text->entry), "<sup>", 5, &start);
        end += 5;
        gtk_editable_insert_text(GTK_EDITABLE(sci_text->entry), "</sup>", 6, &end);
    }
    gwy_sci_text_edited(sci_text->entry);
}


static void     
gwy_sci_text_button_bold_pressed(GtkButton *button)
{
    gwy_sci_text_button_some_pressed(button, GWY_SCI_TEXT_BOLD);
}

static void     
gwy_sci_text_button_italic_pressed(GtkButton *button)
{
    gwy_sci_text_button_some_pressed(button, GWY_SCI_TEXT_ITALIC);
}

static void     
gwy_sci_text_button_lower_pressed(GtkButton *button)
{
    gwy_sci_text_button_some_pressed(button, GWY_SCI_TEXT_SUBSCRIPT);
}

static void     
gwy_sci_text_button_upper_pressed(GtkButton *button)
{
    gwy_sci_text_button_some_pressed(button, GWY_SCI_TEXT_SUPERSCRIPT);
}


static void
stupid_put_entity(GList *items, gint i)
{
    GString *text, *entity;
    text = g_string_new("");
    entity = g_string_new("");

    g_string_assign(text, SOME_ENTITIES[i]);  
    g_string_assign(entity, gwy_entities_entity_to_utf8(text->str));
    g_string_prepend(text, "  ");
    g_string_prepend(text, entity->str);
    items = g_list_append (items, text->str);

    g_string_free(entity, 1);
}
    

static void
stupid_put_entities(GList *items)
{
    gint i, n;
   
    n = G_N_ELEMENTS(SOME_ENTITIES) - 1;
    for (i=0; i<n; i++) stupid_put_entity(items, i);
}

gchar * 
gwy_sci_text_get_text(GwySciText *sci_text)
{
    return gtk_editable_get_chars(GTK_EDITABLE(sci_text->entry), 0, -1);
}

static void
gwy_sci_text_set_text(GwySciText *sci_text, gchar *new_text)
{
    gint pos=0;
    GString *text;
    text = g_string_new(new_text);
    
    gtk_editable_delete_text(GTK_EDITABLE(sci_text->entry), 0, -1);
    gtk_editable_insert_text(GTK_EDITABLE(sci_text->entry), text->str, text->len, &pos);

}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
