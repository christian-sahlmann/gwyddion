/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <glib.h>
#include <gtk/gtk.h>
#include <glib-object.h>
#include <stdio.h>
#include <gdk/gdk.h>
#include <pango/pango.h>
#include <gdk/gdkpango.h>
#include <libgwyddion/gwymacros.h>
#include "gwyaxiser.h"

#define GWY_AXISER_TYPE_NAME "GwyAxiser"

enum {
    LABEL_UPDATED,
    LAST_SIGNAL
};

/* Forward declarations - widget related*/
static void     gwy_axiser_class_init           (GwyAxiserClass *klass);
static void     gwy_axiser_init                 (GwyAxiser *axiser);
static void     gwy_axiser_finalize             (GObject *object);

static void     gwy_axiser_realize              (GtkWidget *widget);
static void     gwy_axiser_unrealize            (GtkWidget *widget);
static void     gwy_axiser_size_request         (GtkWidget *widget,
                                               GtkRequisition *requisition);
static void     gwy_axiser_size_allocate        (GtkWidget *widget,
                                               GtkAllocation *allocation);
static gboolean gwy_axiser_expose               (GtkWidget *widget,
                                               GdkEventExpose *event);
static gboolean gwy_axiser_button_press         (GtkWidget *widget,
                                               GdkEventButton *event);
static gboolean gwy_axiser_button_release       (GtkWidget *widget,
                                               GdkEventButton *event);

/* Forward declarations - axiser related*/
static gdouble  gwy_axiser_dbl_raise            (gdouble x, gint y);
static gdouble  gwy_axiser_quantize_normal_tics (gdouble arg, gint guide);
static gint     gwy_axiser_normalscale          (GwyAxiser *a);
static gint     gwy_axiser_logscale             (GwyAxiser *a);
static gint     gwy_axiser_scale                (GwyAxiser *a);
static gint     gwy_axiser_formatticks          (GwyAxiser *a);
static gint     gwy_axiser_precompute           (GwyAxiser *a,
                                               gint scrmin,
                                               gint scrmax);
static void     gwy_axiser_draw_axiser            (GtkWidget *widget);
static void     gwy_axiser_draw_ticks           (GtkWidget *widget);
static void     gwy_axiser_draw_tlabels         (GtkWidget *widget);
static void     gwy_axiser_draw_label           (GtkWidget *widget);
static void     gwy_axiser_autoset              (GwyAxiser *axiser,
                                               gint width,
                                               gint height);
static void     gwy_axiser_adjust               (GwyAxiser *axiser,
                                               gint width,
                                               gint height);
static void     gwy_axiser_entry                (GwyAxisDialog *dialog,
                                               gint arg1,
                                               gpointer user_data);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

static guint axiser_signals[LAST_SIGNAL] = { 0 };

GType
gwy_axiser_get_type(void)
{
    static GType gwy_axiser_type = 0;

    if (!gwy_axiser_type) {
        static const GTypeInfo gwy_axiser_info = {
            sizeof(GwyAxiserClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_axiser_class_init,
            NULL,
            NULL,
            sizeof(GwyAxiser),
            0,
            (GInstanceInitFunc)gwy_axiser_init,
            NULL,
        };
        gwy_debug("");
        gwy_axiser_type = g_type_register_static(GTK_TYPE_WIDGET,
                                                      GWY_AXISER_TYPE_NAME,
                                                      &gwy_axiser_info,
                                                      0);
    }

    return gwy_axiser_type;
}

static void
gwy_axiser_class_init(GwyAxiserClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_axiser_finalize;

    widget_class->realize = gwy_axiser_realize;
    widget_class->expose_event = gwy_axiser_expose;
    widget_class->size_request = gwy_axiser_size_request;
    widget_class->unrealize = gwy_axiser_unrealize;
    widget_class->size_allocate = gwy_axiser_size_allocate;
    widget_class->button_press_event = gwy_axiser_button_press;
    widget_class->button_release_event = gwy_axiser_button_release;

    klass->label_updated = NULL;

    axiser_signals[LABEL_UPDATED] =
        g_signal_new("label_updated",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyAxiserClass, label_updated),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void
gwy_axiser_init(GwyAxiser *axiser)
{
    gwy_debug("");

    axiser->is_visible = 1;
    axiser->is_logarithmic = 0;
    axiser->is_auto = 1;
    axiser->is_standalone = 0;
    /*axiser->orientation = GWY_AXISER_NORTH;*/
    axiser->max = 0;
    axiser->min = 0;
    /*axiser->mjticks = NULL;*/
    /*axiser->miticks = NULL;*/
    axiser->label_x_pos = 0;
    axiser->label_y_pos = 0;
    /*axiser->label_text = NULL;*/
    axiser->par.major_printmode = GWY_AXISER_AUTO;

    axiser->par.major_length = 10;
    axiser->par.major_thickness = 1;
    axiser->par.major_maxticks = 20;

    axiser->par.minor_length = 5;
    axiser->par.minor_thickness = 1;
    axiser->par.minor_division = 10;
    axiser->par.line_thickness = 1;

    /*axiser->dialog = NULL;*/
    axiser->reqmax = 100;
    axiser->reqmin = 0;

    axiser->enable_label_edit = TRUE;

    axiser->has_unit = 0;
    /*axiser->unit = NULL;*/
}

/**
 * gwy_axiser_new:
 * @orientation:  axiser orientation
 * @min: minimum value
 * @max: maximum value
 * @label: axiser label
 *
 * Creates new axiser.
 *
 * Returns: new axiser
 **/
GtkWidget*
gwy_axiser_new(gint orientation, gdouble min, gdouble max, const gchar *label)
{
    GwyAxiser *axiser;

    gwy_debug("");

    axiser = GWY_AXISER(g_object_new(GWY_TYPE_AXISER, NULL));
    axiser->reqmin = min;
    axiser->reqmax = max;
    axiser->orientation = orientation;

    axiser->label_text = g_string_new(label);
    axiser->mjticks = g_array_new(FALSE, FALSE, sizeof(GwyAxiserLabeledTick));
    axiser->miticks = g_array_new(FALSE, FALSE, sizeof(GwyAxiserTick));

    axiser->label_x_pos = 20;
    axiser->label_y_pos = 20;

    axiser->par.major_font = pango_font_description_new();
    pango_font_description_set_family(axiser->par.major_font, "Helvetica");
    pango_font_description_set_style(axiser->par.major_font, PANGO_STYLE_NORMAL);
    pango_font_description_set_variant(axiser->par.major_font, PANGO_VARIANT_NORMAL);
    pango_font_description_set_weight(axiser->par.major_font, PANGO_WEIGHT_NORMAL);
    pango_font_description_set_size(axiser->par.major_font, 10*PANGO_SCALE);

    axiser->par.label_font = pango_font_description_new();
    pango_font_description_set_family(axiser->par.label_font, "Helvetica");
    pango_font_description_set_style(axiser->par.label_font, PANGO_STYLE_NORMAL);
    pango_font_description_set_variant(axiser->par.label_font, PANGO_VARIANT_NORMAL);
    pango_font_description_set_weight(axiser->par.label_font, PANGO_WEIGHT_NORMAL);
    pango_font_description_set_size(axiser->par.label_font, 12*PANGO_SCALE);

    axiser->dialog = gwy_axis_dialog_new();

    /* FIXME: emits a spurious label_updated? */
    g_signal_connect(axiser->dialog, "response",
                     G_CALLBACK(gwy_axiser_entry), axiser);
    gwy_sci_text_set_text(GWY_SCI_TEXT(GWY_AXIS_DIALOG(axiser->dialog)->sci_text),
                          label);

    return GTK_WIDGET(axiser);
}

static void
gwy_axiser_finalize(GObject *object)
{
    GwyAxiser *axiser;

    gwy_debug("finalizing a GwyAxiser (refcount = %u)", object->ref_count);

    g_return_if_fail(GWY_IS_AXISER(object));

    axiser = GWY_AXISER(object);

    g_string_free(axiser->label_text, TRUE);
    g_array_free(axiser->mjticks, FALSE);
    g_array_free(axiser->miticks, FALSE);

    gtk_widget_destroy(axiser->dialog);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_axiser_unrealize(GtkWidget *widget)
{
    GwyAxiser *axiser;

    axiser = GWY_AXISER(widget);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static void
gwy_axiser_realize(GtkWidget *widget)
{
    GwyAxiser *axiser;
    GdkWindowAttr attributes;
    gint attributes_mask;
    GtkStyle *s;

    gwy_debug("realizing a GwyAxiser (%ux%u)",
              widget->allocation.x, widget->allocation.height);

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_AXISER(widget));

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    axiser = GWY_AXISER(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget)
                            | GDK_EXPOSURE_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_POINTER_MOTION_MASK
                            | GDK_POINTER_MOTION_HINT_MASK;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
                                    &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);

    /*set backgroun for white forever*/
    s = gtk_style_copy(widget->style);
    s->bg_gc[0] =
        s->bg_gc[1] =
        s->bg_gc[2] =
        s->bg_gc[3] =
        s->bg_gc[4] = widget->style->white_gc;
    s->bg[0] =
        s->bg[1] =
        s->bg[2] =
        s->bg[3] =
        s->bg[4] = widget->style->white;

    gtk_style_set_background (s, widget->window, GTK_STATE_NORMAL);

    /*compute ticks*/
    gwy_axiser_adjust(axiser, widget->allocation.width, widget->allocation.height);
}

static void
gwy_axiser_size_request(GtkWidget *widget,
                             GtkRequisition *requisition)
{
    GwyAxiser *axiser;
    gwy_debug("");

    axiser = GWY_AXISER(widget);

    if (axiser->orientation == GWY_AXISER_EAST 
        || axiser->orientation == GWY_AXISER_WEST) {
        requisition->width = 80;
        requisition->height = 100;
    }
    else {
        requisition->width = 100;
        requisition->height = 80;
    }

}

static void
gwy_axiser_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GwyAxiser *axiser;

    gwy_debug("");

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_AXISER(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;

    axiser = GWY_AXISER(widget);
    if (GTK_WIDGET_REALIZED(widget)) {

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
    }
    gwy_axiser_adjust(axiser, allocation->width, allocation->height);

}

static void
gwy_axiser_adjust(GwyAxiser *axiser, gint width, gint height)
{

    if (axiser->orientation == GWY_AXISER_NORTH
        || axiser->orientation == GWY_AXISER_SOUTH) {
        axiser->label_x_pos = width/2;
        if (axiser->orientation == GWY_AXISER_NORTH)
            axiser->label_y_pos = 40;
        else
            axiser->label_y_pos = height - 50;
    }
    if (axiser->orientation == GWY_AXISER_EAST
        || axiser->orientation == GWY_AXISER_WEST) {
        axiser->label_y_pos = height/2;
        if (axiser->orientation == GWY_AXISER_EAST)
            axiser->label_x_pos = 40;
        else
            axiser->label_x_pos = width - 40;
    }


    if (axiser->is_auto)
        gwy_axiser_autoset(axiser, width, height);
    gwy_axiser_scale(axiser);

    if (axiser->orientation == GWY_AXISER_NORTH
        || axiser->orientation == GWY_AXISER_SOUTH)
        gwy_axiser_precompute(axiser, 0, width);
    else
        gwy_axiser_precompute(axiser, 0, height);


}

static void
gwy_axiser_autoset(GwyAxiser *axiser, gint width, gint height)
{
    if (axiser->orientation == GWY_AXISER_NORTH
        || axiser->orientation == GWY_AXISER_SOUTH) {

        if (width < 150) {
            axiser->par.major_thickness = 1;
            axiser->par.major_maxticks = 5;
            axiser->par.minor_division = 5;
        }
        else if (width < 600) {
            axiser->par.major_thickness = 1;
            axiser->par.major_maxticks = 7;
            axiser->par.minor_division = 10;
        }
        else {
            axiser->par.major_thickness = 1;
            axiser->par.major_maxticks = 18;
            axiser->par.minor_division = 10;
        }
    }
    if (axiser->orientation == GWY_AXISER_EAST
        || axiser->orientation == GWY_AXISER_WEST) {

        if (height < 150) {
            axiser->par.major_thickness = 1;
            axiser->par.major_maxticks = 10;
            axiser->par.minor_division = 5;
        }
        else if (height < 600) {
            axiser->par.major_thickness = 1;
            axiser->par.major_maxticks = 20;
            axiser->par.minor_division = 10;
        }
        else {
            axiser->par.major_thickness = 1;
            axiser->par.major_maxticks = 25;
            axiser->par.minor_division = 10;
        }
    }


}

/**
 * gwy_axiser_set_logarithmic:
 * @axiser: axiser 
 * @is_logarithmic: logarithimc mode
 *
 * Sets logarithmic mode. Untested.
 **/
void
gwy_axiser_set_logarithmic(GwyAxiser *axiser,
                         gboolean is_logarithmic)
{
    axiser->is_logarithmic = is_logarithmic;
}

static gboolean
gwy_axiser_expose(GtkWidget *widget,
                GdkEventExpose *event)
{
    GwyAxiser *axiser;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_AXISER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    axiser = GWY_AXISER(widget);

    gdk_window_clear_area(widget->window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);

    if (axiser->is_standalone && axiser->is_visible) 
        gwy_axiser_draw_axiser(widget);
    if (axiser->is_visible) gwy_axiser_draw_ticks(widget);
    if (axiser->is_visible) gwy_axiser_draw_tlabels(widget);
    if (axiser->is_visible) gwy_axiser_draw_label(widget);

    return FALSE;
}

static void
gwy_axiser_draw_axiser(GtkWidget *widget)
{
    GwyAxiser *axiser;
    GdkGC *mygc;

    axiser = GWY_AXISER(widget);
    mygc = gdk_gc_new(widget->window);
    gdk_gc_set_line_attributes (mygc, axiser->par.line_thickness,
                                GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    switch (axiser->orientation) {
        case GWY_AXISER_NORTH:
        gdk_draw_line(widget->window, mygc,
                      0, 0,
                      widget->allocation.width-1, 0);
        break;

        case GWY_AXISER_SOUTH:
        gdk_draw_line(widget->window, mygc,
                      0, widget->allocation.height-1,
                      widget->allocation.width-1, widget->allocation.height-1);
        break;

        case GWY_AXISER_EAST:
        gdk_draw_line(widget->window, mygc,
                      0, 0,
                      0, widget->allocation.height-1);
        break;

        case GWY_AXISER_WEST:
        gdk_draw_line(widget->window, mygc,
                      widget->allocation.width-1, 0,
                      widget->allocation.width-1, widget->allocation.height-1);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    g_object_unref(mygc);
}


static void
gwy_axiser_draw_ticks(GtkWidget *widget)
{
    guint i;
    GwyAxiser *axiser;
    GwyAxiserTick *pmit;
    GdkGC *mygc;
    GwyAxiserLabeledTick *pmjt;

    axiser = GWY_AXISER(widget);

    mygc = gdk_gc_new(widget->window);

    gdk_gc_set_line_attributes (mygc, axiser->par.major_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    for (i = 0; i < axiser->mjticks->len; i++) {
        pmjt = &g_array_index (axiser->mjticks, GwyAxiserLabeledTick, i);

        switch (axiser->orientation) {
            case GWY_AXISER_NORTH:
            gdk_draw_line(widget->window, mygc,
                          pmjt->t.scrpos,
                          0,
                          pmjt->t.scrpos,
                          axiser->par.major_length);
            break;

            case GWY_AXISER_SOUTH:
            gdk_draw_line(widget->window, mygc,
                          pmjt->t.scrpos,
                          widget->allocation.height-1,
                          pmjt->t.scrpos,
                          widget->allocation.height-1 - axiser->par.major_length);
            break;

            case GWY_AXISER_EAST:
            gdk_draw_line(widget->window, mygc,
                          0,
                          widget->allocation.height-1 - pmjt->t.scrpos,
                          axiser->par.major_length,
                          widget->allocation.height-1 - pmjt->t.scrpos);
            break;

            case GWY_AXISER_WEST:
            gdk_draw_line(widget->window, mygc,
                          widget->allocation.width-1,
                          widget->allocation.height-1 - pmjt->t.scrpos,
                          widget->allocation.width-1 - axiser->par.major_length,
                          widget->allocation.height-1 - pmjt->t.scrpos);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }

    gdk_gc_set_line_attributes(mygc, axiser->par.minor_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    for (i = 0; i < axiser->miticks->len; i++) {
        pmit = &g_array_index (axiser->miticks, GwyAxiserTick, i);

        switch (axiser->orientation) {
            case GWY_AXISER_NORTH:
            gdk_draw_line(widget->window, mygc,
                          pmit->scrpos,
                          0,
                          pmit->scrpos,
                          axiser->par.minor_length);
            break;

            case GWY_AXISER_SOUTH:
            gdk_draw_line(widget->window, mygc,
                          pmit->scrpos,
                          widget->allocation.height-1,
                          pmit->scrpos,
                          widget->allocation.height-1 - axiser->par.minor_length);
            break;

            case GWY_AXISER_EAST:
            gdk_draw_line(widget->window, mygc,
                          0,
                          widget->allocation.height-1 - pmit->scrpos,
                          axiser->par.minor_length,
                          widget->allocation.height-1 - pmit->scrpos);
            break;

            case GWY_AXISER_WEST:
            gdk_draw_line(widget->window, mygc,
                          widget->allocation.width-1,
                          widget->allocation.height-1 - pmit->scrpos,
                          widget->allocation.width-1 - axiser->par.minor_length,
                          widget->allocation.height-1 - pmit->scrpos);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }
    g_object_unref(mygc);
}

static void
gwy_axiser_draw_tlabels(GtkWidget *widget)
{
    guint i;
    GwyAxiser *axiser;
    GwyAxiserLabeledTick *pmjt;
    PangoLayout *layout;
    PangoRectangle rect;
    GdkGC *mygc;
    gint sep, xpos = 0, ypos = 0;

    mygc = gdk_gc_new(widget->window);

    axiser = GWY_AXISER(widget);
    layout = gtk_widget_create_pango_layout(widget, "");
    pango_layout_set_font_description(layout, axiser->par.major_font);

    sep = 5;

    for (i = 0; i < axiser->mjticks->len; i++) {
        pmjt = &g_array_index(axiser->mjticks, GwyAxiserLabeledTick, i);
        pango_layout_set_text(layout,  pmjt->ttext->str, pmjt->ttext->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);

        switch (axiser->orientation) {
            case GWY_AXISER_NORTH:
            xpos = pmjt->t.scrpos - rect.width/2;
            ypos = axiser->par.major_length + sep;
            break;

            case GWY_AXISER_SOUTH:
            xpos = pmjt->t.scrpos - rect.width/2;
            ypos = widget->allocation.height-1
                   - axiser->par.major_length - sep - rect.height;
            break;

            case GWY_AXISER_EAST:
            xpos = axiser->par.major_length + sep;
            ypos = widget->allocation.height-1 - pmjt->t.scrpos - rect.height/2;
            break;

            case GWY_AXISER_WEST:
            xpos = widget->allocation.width-1
                   - axiser->par.major_length - sep - rect.width;
            ypos = widget->allocation.height-1
                   - pmjt->t.scrpos - rect.height/2;
            break;

            default:
            g_assert_not_reached();
            break;
        }
        if ((widget->allocation.width-1 - xpos) < rect.width)
            xpos = widget->allocation.width-1 - rect.width;
        else if (xpos < 0)
            xpos = 0;

        if ((widget->allocation.height-1 - ypos) < rect.height)
            ypos = widget->allocation.height-1 - rect.height;
        else if (ypos < 0)
            ypos = 0;

        gdk_draw_layout(widget->window, mygc, xpos, ypos, layout);
    }

    g_object_unref(mygc);
}

static void
gwy_axiser_draw_label(GtkWidget *widget)
{
    GwyAxiser *axiser;
    PangoLayout *layout;
    GdkGC *mygc;
    PangoRectangle rect;
    /*PangoContext *context;
    PangoMatrix matrix = PANGO_MATRIX_INIT;
    */
    GString *plotlabel;

    mygc = gdk_gc_new(widget->window);

    axiser = GWY_AXISER(widget);
    layout = gtk_widget_create_pango_layout(widget, "");
    pango_layout_set_font_description(layout, axiser->par.major_font);

    plotlabel = g_string_new(axiser->label_text->str);

    if (axiser->has_unit) {
        g_string_append(plotlabel, " [");
        g_string_append(plotlabel, axiser->unit);
        g_string_append(plotlabel, "]");
    }

    pango_layout_set_markup(layout,  plotlabel->str, plotlabel->len);
    pango_layout_get_pixel_extents(layout, NULL, &rect);
    /*context = gtk_widget_create_pango_context (widget);
    */
    switch (axiser->orientation) {
        case GWY_AXISER_NORTH:
        gdk_draw_layout(widget->window, mygc,
                        axiser->label_x_pos - rect.width/2, axiser->label_y_pos,
                        layout);
        break;

        case GWY_AXISER_SOUTH:
        gdk_draw_layout(widget->window, mygc,
                        axiser->label_x_pos - rect.width/2,
                        axiser->label_y_pos,
                        layout);
        break;

        case GWY_AXISER_EAST:
        /*pango_matrix_rotate (&matrix, 90);
        pango_context_set_matrix (context, &matrix);
        pango_layout_context_changed (layout);
        pango_layout_get_size (layout, &width, &height);*/
        gdk_draw_layout(widget->window, mygc,
                        axiser->label_x_pos,
                        axiser->label_y_pos,
                        layout);
        break;

        case GWY_AXISER_WEST:
        gdk_draw_layout(widget->window, mygc,
                        axiser->label_x_pos - rect.width,
                        axiser->label_y_pos,
                        layout);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    g_string_free(plotlabel, TRUE);
    g_object_unref(mygc);
}



static gboolean
gwy_axiser_button_press(GtkWidget *widget,
                      GdkEventButton *event)
{
    GwyAxiser *axiser;

    gwy_debug("");
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_AXISER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    axiser = GWY_AXISER(widget);

    if (axiser->enable_label_edit)
        gtk_widget_show_all(axiser->dialog);

    return FALSE;
}

static gboolean
gwy_axiser_button_release(GtkWidget *widget,
                        GdkEventButton *event)
{
    GwyAxiser *axiser;

    gwy_debug("");
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_AXISER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    axiser = GWY_AXISER(widget);

    return FALSE;
}

static void
gwy_axiser_entry(GwyAxisDialog *dialog, gint arg1, gpointer user_data)
{
    GwyAxiser *axiser;
    GdkRectangle rec;

    gwy_debug("");

    axiser = GWY_AXISER(user_data);
    g_assert(GWY_IS_AXISER(axiser));

    rec.x = GTK_WIDGET(axiser)->allocation.x;
    rec.y = GTK_WIDGET(axiser)->allocation.y;
    rec.width = GTK_WIDGET(axiser)->allocation.width;
    rec.height = GTK_WIDGET(axiser)->allocation.height;

    if (arg1 == GTK_RESPONSE_APPLY) {
        gchar *text;

        text = gwy_sci_text_get_text(GWY_SCI_TEXT(dialog->sci_text));
        g_string_assign(axiser->label_text, text);
        g_free(text);
        g_signal_emit(axiser, axiser_signals[LABEL_UPDATED], 0);
        gtk_widget_queue_draw(GTK_WIDGET(axiser));
    }
    else if (arg1 == GTK_RESPONSE_CLOSE) {
        gtk_widget_hide(GTK_WIDGET(dialog));
    }
}


static gdouble
gwy_axiser_dbl_raise(gdouble x, gint y)
{
    gint i = (int)fabs(y);
    gdouble val = 1.0;

    while (--i >= 0)
        val *= x;

    if (y < 0)
        return (1.0 / val);
    return (val);
}

static gdouble
gwy_axiser_quantize_normal_tics(gdouble arg, gint guide)
{
    gdouble power = gwy_axiser_dbl_raise(10.0, (gint)floor(log10(arg)));
    gdouble xnorm = arg / power;        /* approx number of decades */
    gdouble posns = guide / xnorm; /* approx number of tic posns per decade */
    gdouble tics;
    if (posns > 40)
        tics = 0.05;            /* eg 0, .05, .10, ... */
    else if (posns > 20)
        tics = 0.1;             /* eg 0, .1, .2, ... */
    else if (posns > 10)
        tics = 0.2;             /* eg 0,0.2,0.4,... */
    else if (posns > 4)
        tics = 0.5;             /* 0,0.5,1, */
    else if (posns > 1)
        tics = 1;               /* 0,1,2,.... */
    else if (posns > 0.5)
        tics = 2;               /* 0, 2, 4, 6 */
    else
        tics = ceil(xnorm);


    return (tics * power);
}


static gint
gwy_axiser_normalscale(GwyAxiser *a)
{
    gint i;
    GwyAxiserTick mit;
    GwyAxiserLabeledTick mjt;
    gdouble range, tickstep, majorbase, minortickstep, minorbase;

    if (a->reqmax == a->reqmin) {g_warning("Axiser with zero range!"); a->reqmax = a->reqmin+1;}
        
    /*printf("reqmin=%f, reqmax=%f\n", a->reqmin, a->reqmax);*/
    range = fabs(a->reqmax - a->reqmin); /*total range of the field*/

    if (range > 1e40 || range < -1e40)
    {
        g_warning("Axiser with extreme range (>1e40)!");
        a->reqmax = 100; a->reqmin = 0;
    }
    
    tickstep = gwy_axiser_quantize_normal_tics(range, a->par.major_maxticks); /*step*/
    majorbase = ceil(a->reqmin/tickstep)*tickstep; /*starting value*/
    minortickstep = tickstep/(gdouble)a->par.minor_division;
    minorbase = ceil(a->reqmin/minortickstep)*minortickstep;

    /*printf("rng=%f, tst=%f, mjb=%f, mnts=%f, mnb=%f\n",
       range, tickstep, majorbase, minortickstep, minorbase);*/

    if (majorbase > a->reqmin) {
        majorbase -= tickstep;
        minorbase = majorbase;
        a->min = majorbase;
    }
    else
        a->min = a->reqmin;

    /*printf("majorbase = %f, reqmin=%f\n", majorbase, a->reqmin);*/

    /*major tics*/
    i = 0;
    do {
        mjt.t.value = majorbase;
        mjt.ttext = g_string_new(" ");
        g_array_append_val(a->mjticks, mjt);
        majorbase += tickstep;
        i++;
    } while ((majorbase - tickstep) < a->reqmax /*&& i< a->par.major_maxticks*/);
/*printf("majorbase=%f, tickstep=%f, reqmax=%f\n", majorbase, tickstep, a->reqmax);*/
    a->max = majorbase - tickstep;

    i = 0;
    /*minor tics*/
    do {
        mit.value = minorbase;
        /*printf("gwyaxiser.c:893: appending %f (%dth)\n", (gdouble)mit.value, i);*/
        g_array_append_val(a->miticks, mit);
        minorbase += minortickstep;
        i++;
    } while (minorbase <= a->max);

    return 0;
}


static gint
gwy_axiser_logscale(GwyAxiser *a)
{
    gint i;
    gdouble max, min, _min, tickstep, base;
    GwyAxiserLabeledTick mjt;
    GwyAxiserTick mit;

    max = a->max;
    min = a->min;
    _min = min+0.1;

    /*no negative values are allowed*/
    if (min > 0)
        min = log10(min);
    else if (min == 0)
        min = log10(_min);
    else
        return 1;

    if (max > 0)
        max = log10(max);
    else
        return 1;

    /*ticks will be linearly distributed again*/

    /*major ticks - will be equally ditributed in the log domain 1,10,100*/
    tickstep = 1; /*step*/
    base = ceil(min/tickstep)*tickstep; /*starting value*/

    i = 0;
    do {
        mjt.t.value = base;
        mjt.ttext = g_string_new(" ");
        g_array_append_val(a->mjticks, mjt);
        base += tickstep;
        i++;
    } while (base<=max && i<a->par.major_maxticks);

    /*minor ticks - will be equally distributed in the normal domain 1,2,3...*/
    tickstep = gwy_axiser_dbl_raise(10.0, (gint)floor(min));
    base = ceil(pow(10, min)/tickstep)*tickstep;
    max = a->max;
    i = 0;
    do {
         /*here, tickstep must be adapted do scale*/
         tickstep = gwy_axiser_dbl_raise(10.0, (gint)floor(log10(base*1.01)));
             mit.value = log10(base);
         g_array_append_val(a->miticks, mit);
         base += tickstep;
         i++;
    } while (base<=max && i<(a->par.major_maxticks*10));

    return 0;
}


/* FIXME: return TRUE for success, not 0 */
static gint
gwy_axiser_scale(GwyAxiser *a)
{
    gsize i;
    GwyAxiserLabeledTick *mjt;

    
    /*never use logarithmic mode for negative numbers*/
    if (a->min < 0 && a->is_logarithmic == TRUE)
        return 1; /*this is an error*/

    /*remove old ticks*/
    for (i = 0; i < a->mjticks->len; i++) {
        mjt = &g_array_index(a->mjticks, GwyAxiserLabeledTick, i);
        g_string_free(mjt->ttext, TRUE);
    }
    g_array_free(a->mjticks, FALSE);
    g_array_free(a->miticks, FALSE);

    a->mjticks = g_array_new(FALSE, FALSE, sizeof(GwyAxiserLabeledTick));
    a->miticks = g_array_new(FALSE, FALSE, sizeof(GwyAxiserTick));

    /*find tick positions*/
    if (!a->is_logarithmic)
        gwy_axiser_normalscale(a);
    else
        gwy_axiser_logscale(a);
    /*label ticks*/
    gwy_axiser_formatticks(a);
    /*precompute screen coordinates of ticks (must be done after each geometry change)*/

    return 0;
}

static gint
gwy_axiser_precompute(GwyAxiser *a, gint scrmin, gint scrmax)
{
    guint i;
    gdouble dist, range;
    GwyAxiserLabeledTick *pmjt;
    GwyAxiserTick *pmit;

    dist = (gdouble)scrmax-scrmin-1;
    range = a->max - a->min;
    if (a->is_logarithmic) range = log10(a->max)-log10(a->min);

    for (i = 0; i < a->mjticks->len; i++) {
        pmjt = &g_array_index (a->mjticks, GwyAxiserLabeledTick, i);
        if (!a->is_logarithmic)
            pmjt->t.scrpos = (gint)(0.5 + scrmin
                                    + (pmjt->t.value - a->min)/range*dist);
        else
            pmjt->t.scrpos = (gint)(0.5 + scrmin
                                    + (pmjt->t.value - log10(a->min))/range*dist);
    }

    for (i = 0; i < a->miticks->len; i++) {
        pmit = &g_array_index (a->miticks, GwyAxiserTick, i);
        if (!a->is_logarithmic)
            pmit->scrpos = (gint)(0.5 + scrmin
                                  + (pmit->value - a->min)/range*dist);
        else
            pmit->scrpos = (gint)(0.5 + scrmin
                                  + (pmit->value - log10(a->min))/range*dist);
    }
    return 0;
}

/* XXX: return TRUE for success, not 0 */
static gint
gwy_axiser_formatticks(GwyAxiser *a)
{
    guint i;
    gdouble value;
    gdouble range; /*only for automode and precision*/
    GwyAxiserLabeledTick mji, mjx, *pmjt;
    /*determine range*/
    if (a->mjticks->len == 0) {
        g_warning("No ticks found");
        return 1;
    }
    mji = g_array_index(a->mjticks, GwyAxiserLabeledTick, 0);
    mjx = g_array_index(a->mjticks, GwyAxiserLabeledTick, a->mjticks->len - 1);
    if (!a->is_logarithmic)
        range = fabs(mjx.t.value - mji.t.value);
    else
        range = fabs(pow(10, mjx.t.value) - pow(10, mji.t.value));

    for (i = 0; i< a->mjticks->len; i++)
    {
        /*find the value we want to put in string*/
        pmjt = &g_array_index(a->mjticks, GwyAxiserLabeledTick, i);
        if (!a->is_logarithmic)
            value = pmjt->t.value;
        else
            value = pow(10, pmjt->t.value);

        /*fill dependent to mode*/
        if (a->par.major_printmode == GWY_AXISER_FLOAT
            || (a->par.major_printmode == GWY_AXISER_AUTO
                && (fabs(value) <= 10000 && fabs(value) >= 0.001))) {
            if (range < 0.01 && range >= 0.001) {
                g_string_printf(pmjt->ttext, "%.4f", value);
            }
            else if (range < 0.1) {
                g_string_printf(pmjt->ttext, "%.3f", value);
            }
            else if (range < 1) {
                g_string_printf(pmjt->ttext, "%.2f", value);
            }
            else if (range < 100) {
                g_string_printf(pmjt->ttext, "%.1f", value);
            }
            else if (range >= 100) {
                g_string_printf(pmjt->ttext, "%.0f", value);
            }
            else {
                g_string_printf(pmjt->ttext, "%f", value);
            }
            if (value==0) g_string_printf(pmjt->ttext, "0");
        }
        else if (a->par.major_printmode == GWY_AXISER_EXP
                 || (a->par.major_printmode == GWY_AXISER_AUTO
                     && (fabs(value) > 10000 || fabs(value) < 0.001))) {
            g_string_printf(pmjt->ttext,"%.1E", value);
            if (value == 0)
                g_string_printf(pmjt->ttext,"0");
        }
        else if (a->par.major_printmode == GWY_AXISER_INT) {
            g_string_printf(pmjt->ttext,"%d", (int)(value+0.5));
        }
    }
    return 0;
}



/**
 * gwy_axiser_set_visible:
 * @axiser: axiser widget 
 * @is_visible: visibility
 *
 * Sets visibility of axiser.
 **/
void
gwy_axiser_set_visible(GwyAxiser *axiser, gboolean is_visible)
{
    axiser->is_visible = is_visible;
}

/**
 * gwy_axiser_set_auto:
 * @axiser: axiser widget 
 * @is_auto: auto preperty
 *
 * Sets the auto property. If TRUE, axiser changes fonts
 * and ticks sizes to produce reasonable output at different
 * widget sizes.
 **/
void
gwy_axiser_set_auto(GwyAxiser *axiser, gboolean is_auto)
{
    axiser->is_auto = is_auto;
}

/**
 * gwy_axiser_set_req:
 * @axiser: axiser widget 
 * @min: minimum requisistion
 * @max: maximum requisition
 *
 * Set requisition of axiser boundaries. Axiser will fix the boundaries
 * to satisfy requisition but still have reasonable tick values and spacing.
 **/
void
gwy_axiser_set_req(GwyAxiser *axiser, gdouble min, gdouble max)
{
    axiser->reqmin = min;
    axiser->reqmax = max;
    
    /*prevent axiser to allow null range. It has no sense*/
    if (min==max) axiser->reqmax += 10.0;
   
    gwy_axiser_adjust(axiser,
                    (GTK_WIDGET(axiser))->allocation.width,
                    (GTK_WIDGET(axiser))->allocation.height);
}

/**
 * gwy_axiser_set_style:
 * @axiser: axiser widget 
 * @style: axiser style
 *
 * Set axiser style. The style affects used tick sizes, fonts etc.
 **/
void
gwy_axiser_set_style(GwyAxiser *axiser, GwyAxiserParams style)
{
    axiser->par = style;
    gwy_axiser_adjust(axiser,
                    (GTK_WIDGET(axiser))->allocation.width,
                    (GTK_WIDGET(axiser))->allocation.height);
}

/**
 * gwy_axiser_get_maximum:
 * @axiser: axiser widget 
 *
 * 
 *
 * Returns: real maximum of axiser
 **/
gdouble
gwy_axiser_get_maximum(GwyAxiser *axiser)
{
    return axiser->max;
}

/**
 * gwy_axiser_get_minimum:
 * @axiser: axiser widget 
 *
 * 
 *
 * Returns: real minimum of axiser
 **/
gdouble
gwy_axiser_get_minimum(GwyAxiser *axiser)
{
    return axiser->min;
}

/**
 * gwy_axiser_get_reqmaximum:
 * @axiser: axiser widget 
 *
 * 
 *
 * Returns: axiser requisition maximum
 **/
gdouble
gwy_axiser_get_reqmaximum(GwyAxiser *axiser)
{
    return axiser->reqmax;
}

/**
 * gwy_axiser_get_reqminimum:
 * @axiser: axiser widget 
 *
 * 
 *
 * Returns: axiser requisition minimum
 **/
gdouble
gwy_axiser_get_reqminimum(GwyAxiser *axiser)
{
    return axiser->reqmin;
}

/**
 * gwy_axiser_set_label:
 * @axiser: axiser widget 
 * @label_text: label to be set
 *
 * sets the label text
 **/
void
gwy_axiser_set_label(GwyAxiser *axiser, GString *label_text)
{
    gwy_debug("label_text = <%s>", label_text->str);
    g_string_assign(axiser->label_text, label_text->str);
    gwy_sci_text_set_text(GWY_SCI_TEXT(GWY_AXIS_DIALOG(axiser->dialog)->sci_text),
                          label_text->str);
    g_signal_emit(axiser, axiser_signals[LABEL_UPDATED], 0);
    gtk_widget_queue_draw(GTK_WIDGET(axiser));
}

/**
 * gwy_axiser_get_label:
 * @axiser: axiser widget 
 *
 * 
 *
 * Returns: axiser label string
 **/
GString*
gwy_axiser_get_label(GwyAxiser *axiser)
{
    return axiser->label_text;
}

/* XXX: Fuck! There's NO way how the units could be unset! */
/* XXX: DoubleFuck! The thing GOBBLES the passed string! */
/**
 * gwy_axiser_set_unit:
 * @axiser: axiser widget 
 * @unit: label unit
 *
 * Sets the label unit. This will be added automatically
 * to the label.
 **/
void
gwy_axiser_set_unit(GwyAxiser *axiser, char *unit)
{
    axiser->unit = unit;
    axiser->has_unit = 1;
}


/**
 * gwy_axiser_enable_label_edit:
 * @axiser: Axiser widget 
 * @enable: enable/disable user to change axiser label 
 *
 * Enables/disables user to change axiser label by clicking on axiser widget.
 *
 * Since: 1.3.
 **/
void
gwy_axiser_enable_label_edit(GwyAxiser *axiser, gboolean enable)
{
    axiser->enable_label_edit = enable;
}

/************************** Documentation ****************************/

/**
 * GwyAxiserScaleFormat:
 * @GWY_AXISER_FLOAT: Floating point format.
 * @GWY_AXISER_EXP: Exponential (`scienfitic') format.
 * @GWY_AXISER_INT: Integer format.
 * @GWY_AXISER_AUTO: Automatical format.
 *
 * Labeled axiser tick mark format.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
