/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/* Color picker button for GNOME
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Backported to Gtk+-2.2 and GLib-2.2 by Yeti in Feb 2004.
 *
 * _GtkColorButtonPrivate made a normal structure member and moved to the
 * header file as there's no support for private in GLib-2.2.
 * Renamed to GwyColorButton to avoid name clash with Gtk+-2.4.
 *
 * In May 2004 all the dialog functionality was removed as we need much
 * more control.  DnD did not fit then and was removed too.
 */

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkdrawingarea.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwymath.h>
#include "gwycolorbutton.h"

/* Size of checks and gray levels for alpha compositing checkerboard */
#define CHECK_SIZE  4
#define CHECK_DARK  (1.0/3.0)
#define CHECK_LIGHT (2.0/3.0)

#define P_(x) x

/* Properties */
enum
{
    PROP_0,
    PROP_USE_ALPHA,
    PROP_COLOR,
};

static void gwy_color_button_class_init    (GwyColorButtonClass *klass);
static void gwy_color_button_init          (GwyColorButton      *color_button);

/* gobject signals */
static void gwy_color_button_finalize      (GObject             *object);
static void gwy_color_button_set_property  (GObject        *object,
                                            guint           param_id,
                                            const GValue   *value,
                                            GParamSpec     *pspec);
static void gwy_color_button_get_property  (GObject        *object,
                                            guint           param_id,
                                            GValue         *value,
                                            GParamSpec     *pspec);

/* gtkwidget signals */
static void gwy_color_button_realize       (GtkWidget *widget);
static void gwy_color_button_state_changed (GtkWidget           *widget,
                                            GtkStateType         previous_state);
static void gwy_color_button_style_set     (GtkWidget *widget,
                                            GtkStyle  *previous_style);

static gpointer parent_class = NULL;

GType
gwy_color_button_get_type (void)
{
    static GType color_button_type = 0;

    if (!color_button_type) {
        static const GTypeInfo color_button_info = {
            sizeof(GwyColorButtonClass),
            NULL,           /* base_init */
            NULL,           /* base_finalize */
            (GClassInitFunc)gwy_color_button_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof(GwyColorButton),
            0,              /* n_preallocs */
            (GInstanceInitFunc)gwy_color_button_init,
            NULL,
        };

        color_button_type
            = g_type_register_static(GTK_TYPE_BUTTON, "GwyColorButton",
                                     &color_button_info, 0);
    }

    return color_button_type;
}

static void
gwy_color_button_class_init(GwyColorButtonClass *klass)
{
    GObjectClass *gobject_class;
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;
    GtkButtonClass *button_class;

    gobject_class = G_OBJECT_CLASS(klass);
    object_class = GTK_OBJECT_CLASS(klass);
    widget_class = GTK_WIDGET_CLASS(klass);
    button_class = GTK_BUTTON_CLASS(klass);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->get_property = gwy_color_button_get_property;
    gobject_class->set_property = gwy_color_button_set_property;
    gobject_class->finalize = gwy_color_button_finalize;
    widget_class->state_changed = gwy_color_button_state_changed;
    widget_class->realize = gwy_color_button_realize;
    widget_class->style_set = gwy_color_button_style_set;

    /**
     * GwyColorButton:use-alpha:
     *
     * If this property is set to %TRUE, the color swatch on the button is
     * rendered against a checkerboard background to show its opacity and
     * the opacity slider is displayed in the color selection dialog.
     */
    g_object_class_install_property
        (gobject_class,
         PROP_USE_ALPHA,
         g_param_spec_boolean("use_alpha", P_("Use alpha"),
                              P_("Whether or not to give the color "
                                 "an alpha value"),
                              FALSE,
                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));


    /**
     * GwyColorButton:color:
     *
     * The selected color.
     */
    g_object_class_install_property
        (gobject_class,
         PROP_COLOR,
         g_param_spec_boxed("color",
                            P_("Current Color"),
                            P_("The selected color"),
                            GWY_TYPE_RGBA,
                            G_PARAM_READABLE | G_PARAM_WRITABLE));
}

#define C(x) (gint)floor(255.99999999*(x))
#define X(x,v,a) C((x) + ((v) - (x))*(a))

static void
render(GwyColorButton *color_button)
{
    gint dark_r, dark_g, dark_b;
    gint light_r, light_g, light_b;
    gint i, j, rowstride;
    gint width, height;
    gint c1[3], c2[3];
    guchar *pixels;
    guint8 insensitive_r = 0;
    guint8 insensitive_g = 0;
    guint8 insensitive_b = 0;

    width = color_button->drawing_area->allocation.width;
    height = color_button->drawing_area->allocation.height;
    if (color_button->pixbuf == NULL
        || gdk_pixbuf_get_width(color_button->pixbuf) != width
        || gdk_pixbuf_get_height(color_button->pixbuf) != height) {
        if (color_button->pixbuf != NULL)
            g_object_unref(color_button->pixbuf);
        color_button->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                              width, height);
        gwy_debug_objects_creation(G_OBJECT(color_button->pixbuf));
    }


    /* Compute dark and light check colors */

    insensitive_r
        = GTK_WIDGET(color_button)->style->bg[GTK_STATE_INSENSITIVE].red >> 8;
    insensitive_g
        = GTK_WIDGET(color_button)->style->bg[GTK_STATE_INSENSITIVE].green >> 8;
    insensitive_b
        = GTK_WIDGET(color_button)->style->bg[GTK_STATE_INSENSITIVE].blue >> 8;

    if (color_button->use_alpha) {
        dark_r = X(CHECK_DARK, color_button->color.r, color_button->color.a);
        dark_g = X(CHECK_DARK, color_button->color.g, color_button->color.a);
        dark_b = X(CHECK_DARK, color_button->color.b, color_button->color.a);

        light_r = X(CHECK_LIGHT, color_button->color.r, color_button->color.a);
        light_g = X(CHECK_LIGHT, color_button->color.g, color_button->color.a);
        light_b = X(CHECK_LIGHT, color_button->color.b, color_button->color.a);
    }
    else {
        dark_r = light_r = C(color_button->color.r);
        dark_g = light_g = C(color_button->color.g);
        dark_b = light_b = C(color_button->color.b);
    }

    /* Fill image buffer */
    pixels = gdk_pixbuf_get_pixels(color_button->pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(color_button->pixbuf);
    for (j = 0; j < height; j++) {
        if ((j / CHECK_SIZE) & 1) {
            c1[0] = dark_r;
            c1[1] = dark_g;
            c1[2] = dark_b;

            c2[0] = light_r;
            c2[1] = light_g;
            c2[2] = light_b;
        }
        else {
            c1[0] = light_r;
            c1[1] = light_g;
            c1[2] = light_b;

            c2[0] = dark_r;
            c2[1] = dark_g;
            c2[2] = dark_b;
        }

        for (i = 0; i < width; i++) {
            if (!GTK_WIDGET_SENSITIVE(GTK_WIDGET(color_button)) && (i + j)%2) {
                *(pixels + j * rowstride + i * 3) = insensitive_r;
                *(pixels + j * rowstride + i * 3 + 1) = insensitive_g;
                *(pixels + j * rowstride + i * 3 + 2) = insensitive_b;
            }
            else if ((i / CHECK_SIZE) & 1) {
                *(pixels + j * rowstride + i * 3)     = c1[0];
                *(pixels + j * rowstride + i * 3 + 1) = c1[1];
                *(pixels + j * rowstride + i * 3 + 2) = c1[2];
            }
            else {
                *(pixels + j * rowstride + i * 3)     = c2[0];
                *(pixels + j * rowstride + i * 3 + 1) = c2[1];
                *(pixels + j * rowstride + i * 3 + 2) = c2[2];
            }
        }
    }
}

/* Handle exposure events for the color picker's drawing area */
static gint
expose_event(GtkWidget      *widget,
             GdkEventExpose *event,
             gpointer        data)
{
    GwyColorButton *color_button = GWY_COLOR_BUTTON (data);

    gint width = color_button->drawing_area->allocation.width;
    gint height = color_button->drawing_area->allocation.height;

    if (color_button->pixbuf == NULL ||
        width != gdk_pixbuf_get_width(color_button->pixbuf) ||
        height != gdk_pixbuf_get_height(color_button->pixbuf))
        render (color_button);

    gdk_draw_pixbuf(widget->window,
                    color_button->gc,
                    color_button->pixbuf,
                    event->area.x,
                    event->area.y,
                    event->area.x,
                    event->area.y,
                    event->area.width,
                    event->area.height,
                    GDK_RGB_DITHER_MAX,
                    event->area.x,
                    event->area.y);
    return FALSE;
}

static void
gwy_color_button_realize (GtkWidget *widget)
{
    GwyColorButton *color_button = GWY_COLOR_BUTTON(widget);

    GTK_WIDGET_CLASS(parent_class)->realize(widget);

    if (color_button->gc == NULL)
        color_button->gc = gdk_gc_new(widget->window);

    render(color_button);
}

static void
gwy_color_button_style_set(GtkWidget *widget,
                           GtkStyle  *previous_style)
{
    GwyColorButton *color_button = GWY_COLOR_BUTTON (widget);

    GTK_WIDGET_CLASS(parent_class)->style_set(widget, previous_style);

    if (GTK_WIDGET_REALIZED(widget))
        gwy_object_unref(color_button->pixbuf);
}

static void
gwy_color_button_state_changed(GtkWidget   *widget,
                               GtkStateType previous_state)
{
    GwyColorButton *color_button = GWY_COLOR_BUTTON (widget);

    if (widget->state == GTK_STATE_INSENSITIVE
        || previous_state == GTK_STATE_INSENSITIVE)
        gwy_object_unref(color_button->pixbuf);
}

static void
gwy_color_button_init(GwyColorButton *color_button)
{
    GtkWidget *alignment;
    PangoLayout *layout;
    PangoRectangle rect;

    gtk_widget_push_composite_child();

    alignment = gtk_alignment_new(0.5, 0.5, 1.0, 1.0);
    gtk_container_set_border_width(GTK_CONTAINER (alignment), 1);
    gtk_container_add(GTK_CONTAINER(color_button), alignment);
    gtk_widget_show(alignment);

    color_button->drawing_area = gtk_drawing_area_new();

    layout = gtk_widget_create_pango_layout(GTK_WIDGET(color_button), "Black");
    pango_layout_get_pixel_extents(layout, NULL, &rect);
    gtk_widget_set_size_request(color_button->drawing_area,
                                rect.width - 2, rect.height - 2);
    g_signal_connect(color_button->drawing_area, "expose_event",
                     G_CALLBACK (expose_event), color_button);
    gtk_container_add(GTK_CONTAINER(alignment), color_button->drawing_area);
    gtk_widget_show(color_button->drawing_area);

    /* Create the buffer for the image so that we can create an image.
     * Also create the picker's pixmap.
     */
    color_button->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                          rect.width, rect.height);
    gwy_debug_objects_creation(G_OBJECT(color_button->pixbuf));

    color_button->gc = NULL;

    /* Start with opaque black, alpha disabled */

    color_button->color.r = 0.0;
    color_button->color.g = 0.0;
    color_button->color.b = 0.0;
    color_button->color.a = 1.0;
    color_button->use_alpha = FALSE;

    gtk_widget_pop_composite_child();
}

static void
gwy_color_button_finalize(GObject *object)
{
    GwyColorButton *color_button = GWY_COLOR_BUTTON (object);

    gwy_object_unref(color_button->gc);
    gwy_object_unref(color_button->pixbuf);

    G_OBJECT_CLASS(parent_class)->finalize (object);
}


/**
 * gwy_color_button_new:
 *
 * Creates a new color button. This returns a widget in the form of
 * a small button containing a swatch representing the current selected
 * color. When the button is clicked, a color-selection dialog will open,
 * allowing the user to select a color. The swatch will be updated to reflect
 * the new color when the user finishes.
 *
 * Returns: a new color button.
 */
GtkWidget *
gwy_color_button_new (void)
{
    return g_object_new(GWY_TYPE_COLOR_BUTTON, NULL);
}

/**
 * gwy_color_button_new_with_color:
 * @color: A #GwyRGBA to set the current color with.
 *
 * Creates a new color button.
 *
 * Returns: a new color button.
 */
GtkWidget *
gwy_color_button_new_with_color(GwyRGBA *color)
{
    return g_object_new(GWY_TYPE_COLOR_BUTTON, "color", color, NULL);
}

/**
 * gwy_color_button_set_color:
 * @color_button: a #GwyColorButton.
 * @color: A #GwyRGBA to set the current color with.
 *
 * Sets the current color to be @color.
 **/
void
gwy_color_button_set_color (GwyColorButton *color_button,
                            GwyRGBA        *color)
{
    g_return_if_fail(GWY_IS_COLOR_BUTTON (color_button));

    color_button->color.r = color->r;
    color_button->color.g = color->g;
    color_button->color.b = color->b;
    color_button->color.a = color->a;
    gwy_object_unref(color_button->pixbuf);
    gtk_widget_queue_draw(color_button->drawing_area);

    g_object_notify(G_OBJECT(color_button), "color");
}


/**
 * gwy_color_button_get_color:
 * @color_button: a #GwyColorButton.
 * @color: a #GwyRGBA to fill in with the current color.
 *
 * Sets @color to be the current color in the #GwyColorButton widget.
 **/
void
gwy_color_button_get_color(GwyColorButton *color_button,
                           GwyRGBA       *color)
{
    g_return_if_fail (GWY_IS_COLOR_BUTTON (color_button));

    color->r = color_button->color.r;
    color->g = color_button->color.g;
    color->b = color_button->color.b;
    color->a = color_button->color.a;
}

/**
 * gwy_color_button_set_use_alpha:
 * @color_button: a #GwyColorButton.
 * @use_alpha: %TRUE if color button should use alpha channel, %FALSE if not.
 *
 * Sets whether or not the color button should use the alpha channel.
 */
void
gwy_color_button_set_use_alpha(GwyColorButton *color_button,
                               gboolean        use_alpha)
{
    g_return_if_fail (GWY_IS_COLOR_BUTTON (color_button));

    use_alpha = (use_alpha != FALSE);
    if (color_button->use_alpha != use_alpha) {
        color_button->use_alpha = use_alpha;
        color_button->color.a = 1.0;
        render(color_button);
        gtk_widget_queue_draw(color_button->drawing_area);

        g_object_notify (G_OBJECT (color_button), "use_alpha");
    }
}

/**
 * gwy_color_button_get_use_alpha:
 * @color_button: a #GwyColorButton.
 *
 * Does the color selection dialog use the alpha channel?
 *
 * Returns: %TRUE if the color sample uses alpha channel, %FALSE if not.
 */
gboolean
gwy_color_button_get_use_alpha(GwyColorButton *color_button)
{
    g_return_val_if_fail(GWY_IS_COLOR_BUTTON(color_button), FALSE);

    return color_button->use_alpha;
}


static void
gwy_color_button_set_property(GObject      *object,
                              guint         param_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
    GwyColorButton *color_button = GWY_COLOR_BUTTON(object);

    switch (param_id) {
        case PROP_USE_ALPHA:
        gwy_color_button_set_use_alpha(color_button,
                                       g_value_get_boolean(value));
        break;
        case PROP_COLOR:
        gwy_color_button_set_color(color_button, g_value_get_boxed(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
        break;
    }
}

static void
gwy_color_button_get_property(GObject    *object,
                              guint       param_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    GwyColorButton *color_button = GWY_COLOR_BUTTON(object);
    GwyRGBA color;

    switch (param_id)
    {
        case PROP_USE_ALPHA:
        g_value_set_boolean(value,
                            gwy_color_button_get_use_alpha(color_button));
        break;
        case PROP_COLOR:
        gwy_color_button_get_color(color_button, &color);
        g_value_set_boxed(value, &color);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
        break;
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
