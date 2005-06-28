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
#include <pango/pango-context.h>
#include <gdk/gdkpango.h>
#include <libgwydgets/gwydgetenums.h>
#include <libgwyddion/gwymacros.h>
#include "gwyaxis.h"

#define GWY_AXIS_TYPE_NAME "GwyAxis"

enum {
    LABEL_UPDATED,
    RESCALED,
    LAST_SIGNAL
};

/* Forward declarations - widget related*/
static void     gwy_axis_class_init           (GwyAxisClass *klass);
static void     gwy_axis_init                 (GwyAxis *axiser);
static void     gwy_axis_finalize             (GObject *object);

static void     gwy_axis_realize              (GtkWidget *widget);
static void     gwy_axis_unrealize            (GtkWidget *widget);
static void     gwy_axis_size_request         (GtkWidget *widget,
                                               GtkRequisition *requisition);
static void     gwy_axis_size_allocate        (GtkWidget *widget,
                                               GtkAllocation *allocation);
static gboolean gwy_axis_expose               (GtkWidget *widget,
                                               GdkEventExpose *event);
static gboolean gwy_axis_button_press         (GtkWidget *widget,
                                               GdkEventButton *event);
static gboolean gwy_axis_button_release       (GtkWidget *widget,
                                               GdkEventButton *event);

/* Forward declarations - axiser related*/
static gdouble  gwy_axis_dbl_raise            (gdouble x, gint y);
static gdouble  gwy_axis_quantize_normal_tics (gdouble arg, gint guide);
static gint     gwy_axis_normalscale          (GwyAxis *a);
static gint     gwy_axis_logscale             (GwyAxis *a);
static gint     gwy_axis_scale                (GwyAxis *a);
static gint     gwy_axis_formatticks          (GwyAxis *a);
static gint     gwy_axis_precompute           (GwyAxis *a,
                                               gint scrmin,
                                               gint scrmax);
static void     gwy_axis_draw_axiser          (GdkDrawable *drawable, GdkGC *gc, GwyAxisActiveAreaSpecs *specs, GwyAxis *axiser);
static void     gwy_axis_draw_ticks           (GdkDrawable *drawable, GdkGC *gc, GwyAxisActiveAreaSpecs *specs, GwyAxis *axiser);
static void     gwy_axis_draw_tlabels         (GdkDrawable *drawable, GdkGC *gc, GwyAxisActiveAreaSpecs *specs, GwyAxis *axiser);
static void     gwy_axis_draw_label           (GdkDrawable *drawable, GdkGC *gc, GwyAxisActiveAreaSpecs *specs, GwyAxis *axiser);
static void     gwy_axis_autoset              (GwyAxis *axiser,
                                               gint width,
                                               gint height);
static void     gwy_axis_adjust               (GwyAxis *axiser,
                                               gint width,
                                               gint height);
static void     gwy_axis_entry                (GwyAxisDialog *dialog,
                                               gint arg1,
                                               gpointer user_data);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

static guint axiser_signals[LAST_SIGNAL] = { 0 };

GType
gwy_axis_get_type(void)
{
    static GType gwy_axis_type = 0;

    if (!gwy_axis_type) {
        static const GTypeInfo gwy_axis_info = {
            sizeof(GwyAxisClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_axis_class_init,
            NULL,
            NULL,
            sizeof(GwyAxis),
            0,
            (GInstanceInitFunc)gwy_axis_init,
            NULL,
        };
        gwy_debug("");
        gwy_axis_type = g_type_register_static(GTK_TYPE_WIDGET,
                                                      GWY_AXIS_TYPE_NAME,
                                                      &gwy_axis_info,
                                                      0);
    }

    return gwy_axis_type;
}

static void
gwy_axis_class_init(GwyAxisClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_axis_finalize;

    widget_class->realize = gwy_axis_realize;
    widget_class->expose_event = gwy_axis_expose;
    widget_class->size_request = gwy_axis_size_request;
    widget_class->unrealize = gwy_axis_unrealize;
    widget_class->size_allocate = gwy_axis_size_allocate;
    widget_class->button_press_event = gwy_axis_button_press;
    widget_class->button_release_event = gwy_axis_button_release;

    klass->label_updated = NULL;
    klass->rescaled = NULL;

    axiser_signals[LABEL_UPDATED] =
        g_signal_new("label_updated",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyAxisClass, label_updated),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
    axiser_signals[RESCALED] =
        g_signal_new("rescaled",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyAxisClass, rescaled),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void
gwy_axis_init(GwyAxis *axiser)
{
    gwy_debug("");

    axiser->gc = NULL;
    axiser->is_visible = 1;
    axiser->is_logarithmic = 0;
    axiser->is_auto = 1;
    axiser->is_standalone = 0;
    /*axiser->orientation = GTK_POS_TOP;*/
    axiser->max = 0;
    axiser->min = 0;
    /*axiser->mjticks = NULL;*/
    /*axiser->miticks = NULL;*/
    axiser->label_x_pos = 0;
    axiser->label_y_pos = 0;
    /*axiser->label_text = NULL;*/
    axiser->par.major_printmode = GWY_AXIS_SCALE_FORMAT_AUTO;

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
    axiser->unit = NULL;
    axiser->magnification_string = NULL;
    axiser->magnification = 1;
}

/**
 * gwy_axis_new:
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
gwy_axis_new(gint orientation, gdouble min, gdouble max, const gchar *label)
{
    GwyAxis *axiser;

    gwy_debug("");

    axiser = GWY_AXIS(g_object_new(GWY_TYPE_AXISER, NULL));
    axiser->reqmin = min;
    axiser->reqmax = max;
    axiser->orientation = orientation;

    axiser->label_text = g_string_new(label);
    axiser->mjticks = g_array_new(FALSE, FALSE, sizeof(GwyAxisLabeledTick));
    axiser->miticks = g_array_new(FALSE, FALSE, sizeof(GwyAxisTick));

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
                     G_CALLBACK(gwy_axis_entry), axiser);
    gwy_sci_text_set_text(GWY_SCI_TEXT(GWY_AXIS_DIALOG(axiser->dialog)->sci_text),
                          label);

    return GTK_WIDGET(axiser);
}

static void
gwy_axis_finalize(GObject *object)
{
    GwyAxis *axiser;

    gwy_debug("finalizing a GwyAxis (refcount = %u)", object->ref_count);

    g_return_if_fail(GWY_IS_AXISER(object));

    axiser = GWY_AXIS(object);

    g_string_free(axiser->label_text, TRUE);
    g_array_free(axiser->mjticks, FALSE);
    g_array_free(axiser->miticks, FALSE);

    gtk_widget_destroy(axiser->dialog);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_axis_unrealize(GtkWidget *widget)
{
    GwyAxis *axiser;

    axiser = GWY_AXIS(widget);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static void
gwy_axis_realize(GtkWidget *widget)
{
    GwyAxis *axiser;
    GdkWindowAttr attributes;
    gint attributes_mask;
    GtkStyle *s;

    gwy_debug("realizing a GwyAxis (%ux%u)",
              widget->allocation.x, widget->allocation.height);

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_AXISER(widget));

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    axiser = GWY_AXIS(widget);

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

    axiser->gc = gdk_gc_new(widget->window);
    
    /*compute ticks*/
    gwy_axis_adjust(axiser, widget->allocation.width, widget->allocation.height);
}

static void
gwy_axis_size_request(GtkWidget *widget,
                             GtkRequisition *requisition)
{
    GwyAxis *axiser;
    gwy_debug("");

    axiser = GWY_AXIS(widget);

    if (axiser->is_visible) {
        if (axiser->orientation == GTK_POS_LEFT 
            || axiser->orientation == GTK_POS_RIGHT) {
            requisition->width = 80;
            requisition->height = 100;
        }
        else {
            requisition->width = 100;
            requisition->height = 80;
        }
    }
    else {
        requisition->width = 5;
        requisition->height = 5;
    }

}

static void
gwy_axis_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GwyAxis *axiser;

    gwy_debug("");

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_AXISER(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;

    axiser = GWY_AXIS(widget);
    if (GTK_WIDGET_REALIZED(widget)) {

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
    }
    gwy_axis_adjust(axiser, allocation->width, allocation->height);

}

static void
gwy_axis_adjust(GwyAxis *axiser, gint width, gint height)
{

    if (axiser->orientation == GTK_POS_TOP
        || axiser->orientation == GTK_POS_BOTTOM) {
        axiser->label_x_pos = width/2;
        if (axiser->orientation == GTK_POS_TOP)
            axiser->label_y_pos = 40;
        else
            axiser->label_y_pos = height - 50;
    }
    if (axiser->orientation == GTK_POS_LEFT
        || axiser->orientation == GTK_POS_RIGHT) {
        axiser->label_y_pos = height/2;
        if (axiser->orientation == GTK_POS_LEFT)
            axiser->label_x_pos = 40;
        else
            axiser->label_x_pos = width - 40;
    }


    if (axiser->is_auto)
        gwy_axis_autoset(axiser, width, height);
    gwy_axis_scale(axiser);

    if (axiser->orientation == GTK_POS_TOP
        || axiser->orientation == GTK_POS_BOTTOM)
        gwy_axis_precompute(axiser, 0, width);
    else
        gwy_axis_precompute(axiser, 0, height);


    gtk_widget_queue_draw(axiser);
}

static void
gwy_axis_autoset(GwyAxis *axiser, gint width, gint height)
{
    if (axiser->orientation == GTK_POS_TOP
        || axiser->orientation == GTK_POS_BOTTOM) {

        axiser->par.major_maxticks = width/50; /*empirical equation*/
        if (width < 300) 
            axiser->par.minor_division = 5;
        else 
            axiser->par.minor_division = 10;
    }
    if (axiser->orientation == GTK_POS_LEFT
        || axiser->orientation == GTK_POS_RIGHT) {

        axiser->par.major_maxticks = height/40; /*empirical equation*/
        if (height < 150)
            axiser->par.minor_division = 5;
        else 
            axiser->par.minor_division = 10;
       
    }


}

/**
 * gwy_axis_set_logarithmic:
 * @axiser: axiser 
 * @is_logarithmic: logarithimc mode
 *
 * Sets logarithmic mode. Untested.
 **/
void
gwy_axis_set_logarithmic(GwyAxis *axiser,
                         gboolean is_logarithmic)
{
    axiser->is_logarithmic = is_logarithmic;
}

static gboolean
gwy_axis_expose(GtkWidget *widget,
                GdkEventExpose *event)
{
    GwyAxis *axiser;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_AXISER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    axiser = GWY_AXIS(widget);

    gdk_window_clear_area(widget->window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);


    gwy_axis_draw_on_drawable(widget->window,
                                axiser->gc,
                                0, 0,
                                widget->allocation.width,
                                widget->allocation.height,
                                GWY_AXIS(widget));
    return FALSE;
}

void 
gwy_axis_draw_on_drawable(GdkDrawable *drawable, GdkGC *gc, gint xmin, gint ymin, gint width, gint height,
                    GwyAxis *axiser)
{
    GwyAxisActiveAreaSpecs specs;
    specs.xmin = xmin;
    specs.ymin = ymin;
    specs.width = width;
    specs.height = height;
     
    if (axiser->is_standalone && axiser->is_visible) 
        gwy_axis_draw_axiser(drawable, gc, &specs, axiser);
    if (axiser->is_visible) gwy_axis_draw_ticks(drawable, gc, &specs, axiser);
    if (axiser->is_visible) gwy_axis_draw_tlabels(drawable, gc, &specs, axiser);
    if (axiser->is_visible) gwy_axis_draw_label(drawable, gc, &specs, axiser);
}

static void
gwy_axis_draw_axiser(GdkDrawable *drawable, GdkGC *gc, GwyAxisActiveAreaSpecs *specs, GwyAxis *axiser)
{
    gdk_gc_set_line_attributes (gc, axiser->par.line_thickness,
                                GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    switch (axiser->orientation) {
        case GTK_POS_BOTTOM:
        gdk_draw_line(drawable, gc,
                      0, 0,
                      specs->width-1, 0);
        break;

        case GTK_POS_TOP:
        gdk_draw_line(drawable, gc,
                      0, specs->height-1,
                      specs->width-1, specs->height-1);
        break;

        case GTK_POS_LEFT:
        gdk_draw_line(drawable, gc,
                      0, 0,
                      0, specs->height-1);
        break;

        case GTK_POS_RIGHT:
        gdk_draw_line(drawable, gc,
                      specs->width-1, 0,
                      specs->width-1, specs->height-1);
        break;

        default:
        g_assert_not_reached();
        break;
    }

}


static void
gwy_axis_draw_ticks(GdkDrawable *drawable, GdkGC *gc, GwyAxisActiveAreaSpecs *specs, GwyAxis *axiser)
{
    guint i;
    GwyAxisTick *pmit;
    GwyAxisLabeledTick *pmjt;


    gdk_gc_set_line_attributes (gc, axiser->par.major_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    for (i = 0; i < axiser->mjticks->len; i++) {
        pmjt = &g_array_index (axiser->mjticks, GwyAxisLabeledTick, i);

        switch (axiser->orientation) {
            case GTK_POS_BOTTOM:
            gdk_draw_line(drawable, gc,
                          pmjt->t.scrpos,
                          0,
                          pmjt->t.scrpos,
                          axiser->par.major_length);
            break;

            case GTK_POS_TOP:
            gdk_draw_line(drawable, gc,
                          pmjt->t.scrpos,
                          specs->height-1,
                          pmjt->t.scrpos,
                          specs->height-1 - axiser->par.major_length);
            break;

            case GTK_POS_LEFT:
            gdk_draw_line(drawable, gc,
                          0,
                          specs->height-1 - pmjt->t.scrpos,
                          axiser->par.major_length,
                          specs->height-1 - pmjt->t.scrpos);
            break;

            case GTK_POS_RIGHT:
            gdk_draw_line(drawable, gc,
                          specs->width-1,
                          specs->height-1 - pmjt->t.scrpos,
                          specs->width-1 - axiser->par.major_length,
                          specs->height-1 - pmjt->t.scrpos);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }

    gdk_gc_set_line_attributes(gc, axiser->par.minor_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    for (i = 0; i < axiser->miticks->len; i++) {
        pmit = &g_array_index (axiser->miticks, GwyAxisTick, i);

        switch (axiser->orientation) {
            case GTK_POS_BOTTOM:
            gdk_draw_line(drawable, gc,
                          pmit->scrpos,
                          0,
                          pmit->scrpos,
                          axiser->par.minor_length);
            break;

            case GTK_POS_TOP:
            gdk_draw_line(drawable, gc,
                          pmit->scrpos,
                          specs->height-1,
                          pmit->scrpos,
                          specs->height-1 - axiser->par.minor_length);
            break;

            case GTK_POS_LEFT:
            gdk_draw_line(drawable, gc,
                          0,
                          specs->height-1 - pmit->scrpos,
                          axiser->par.minor_length,
                          specs->height-1 - pmit->scrpos);
            break;

            case GTK_POS_RIGHT:
            gdk_draw_line(drawable, gc,
                          specs->width-1,
                          specs->height-1 - pmit->scrpos,
                          specs->width-1 - axiser->par.minor_length,
                          specs->height-1 - pmit->scrpos);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }
}

static void
gwy_axis_draw_tlabels(GdkDrawable *drawable, GdkGC *gc, GwyAxisActiveAreaSpecs *specs, GwyAxis *axiser)
{
    guint i;
    GwyAxisLabeledTick *pmjt;
    PangoLayout *layout;
    PangoContext *context;
    PangoRectangle rect;
    gint sep, xpos = 0, ypos = 0;


    context = gdk_pango_context_get_for_screen(gdk_screen_get_default());
    layout = pango_layout_new(context);
    pango_layout_set_font_description(layout, axiser->par.major_font);

    sep = 3;

    for (i = 0; i < axiser->mjticks->len; i++) {
        pmjt = &g_array_index(axiser->mjticks, GwyAxisLabeledTick, i);
        pango_layout_set_text(layout,  pmjt->ttext->str, pmjt->ttext->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);

        switch (axiser->orientation) {
            case GTK_POS_BOTTOM:
            xpos = pmjt->t.scrpos - rect.width/2;
            ypos = axiser->par.major_length + sep;
            break;

            case GTK_POS_TOP:
            xpos = pmjt->t.scrpos - rect.width/2;
            ypos = specs->height-1
                   - axiser->par.major_length - rect.height;
            break;

            case GTK_POS_LEFT:
            xpos = axiser->par.major_length + sep;
            ypos = specs->height-1 - pmjt->t.scrpos - rect.height/2;
            break;

            case GTK_POS_RIGHT:
            xpos = specs->width-1
                   - axiser->par.major_length - sep - rect.width;
            ypos = specs->height-1
                   - pmjt->t.scrpos - rect.height/2;
            break;

            default:
            g_assert_not_reached();
            break;
        }
        if ((specs->width-1 - xpos) < rect.width)
            xpos = specs->width-1 - rect.width;
        else if (xpos < 0)
            xpos = 0;

        if ((specs->height-1 - ypos) < rect.height)
            ypos = specs->height-1 - rect.height;
        else if (ypos < 0)
            ypos = 0;

        gdk_draw_layout(drawable, gc, xpos, ypos, layout);
    }

}

static void
gwy_axis_draw_label(GdkDrawable *drawable, GdkGC *gc, GwyAxisActiveAreaSpecs *specs, GwyAxis *axiser)
{
    PangoLayout *layout;
    PangoContext *context;
    PangoRectangle rect;
    /*PangoContext *context;
    PangoMatrix matrix = PANGO_MATRIX_INIT;
    */
    GString *plotlabel;

   context = gdk_pango_context_get_for_screen(gdk_screen_get_default());
    layout = pango_layout_new(context);
    pango_layout_set_font_description(layout, axiser->par.major_font);

    plotlabel = g_string_new(axiser->label_text->str);

    if (axiser->has_unit) {
        g_string_append(plotlabel, " [");
        if (axiser->magnification_string) g_string_append(plotlabel, axiser->magnification_string->str);
        else g_string_append(plotlabel, gwy_si_unit_get_unit_string(axiser->unit));
        g_string_append(plotlabel, "]");
    }

    pango_layout_set_markup(layout,  plotlabel->str, plotlabel->len);
    pango_layout_get_pixel_extents(layout, NULL, &rect);
    /*context = gtk_widget_create_pango_context (widget);
    */
    switch (axiser->orientation) {
        case GTK_POS_BOTTOM:
        gdk_draw_layout(drawable, gc,
                        axiser->label_x_pos - rect.width/2, axiser->label_y_pos,
                        layout);
        break;

        case GTK_POS_TOP:
        gdk_draw_layout(drawable, gc,
                        axiser->label_x_pos - rect.width/2,
                        axiser->label_y_pos,
                        layout);
        break;

        case GTK_POS_LEFT:
        /*pango_matrix_rotate (&matrix, 90);
        pango_context_set_matrix (context, &matrix);
        pango_layout_context_changed (layout);
        pango_layout_get_size (layout, &width, &height);*/
        gdk_draw_layout(drawable, gc,
                        axiser->label_x_pos,
                        axiser->label_y_pos,
                        layout);
        break;

        case GTK_POS_RIGHT:
        gdk_draw_layout(drawable, gc,
                        axiser->label_x_pos - rect.width,
                        axiser->label_y_pos,
                        layout);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    g_string_free(plotlabel, TRUE);
}



static gboolean
gwy_axis_button_press(GtkWidget *widget,
                      GdkEventButton *event)
{
    GwyAxis *axiser;

    gwy_debug("");
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_AXISER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    axiser = GWY_AXIS(widget);

    if (axiser->enable_label_edit)
        gtk_widget_show_all(axiser->dialog);

    return FALSE;
}

static gboolean
gwy_axis_button_release(GtkWidget *widget,
                        GdkEventButton *event)
{
    GwyAxis *axiser;

    gwy_debug("");
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_AXISER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    axiser = GWY_AXIS(widget);

    return FALSE;
}

static void
gwy_axis_entry(GwyAxisDialog *dialog, gint arg1, gpointer user_data)
{
    GwyAxis *axiser;
    GdkRectangle rec;

    gwy_debug("");

    axiser = GWY_AXIS(user_data);
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


void        
gwy_axis_signal_rescaled(GwyAxis *axiser)
{
    g_signal_emit(axiser, axiser_signals[RESCALED], 0);
}

static gdouble
gwy_axis_dbl_raise(gdouble x, gint y)
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
gwy_axis_quantize_normal_tics(gdouble arg, gint guide)
{
    gdouble power = gwy_axis_dbl_raise(10.0, (gint)floor(log10(arg)));
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
gwy_axis_normalscale(GwyAxis *a)
{
    gint i;
    GwyAxisTick mit;
    GwyAxisLabeledTick mjt;
    gdouble range, tickstep, majorbase, minortickstep, minorbase;

    if (a->reqmax == a->reqmin) {g_warning("Axiser with zero range!"); a->reqmax = a->reqmin+1;}
        
    /*printf("reqmin=%f, reqmax=%f\n", a->reqmin, a->reqmax);*/
    range = fabs(a->reqmax - a->reqmin); /*total range of the field*/

    if (range > 1e40 || range < -1e40)
    {
        g_warning("Axiser with extreme range (>1e40)!");
        a->reqmax = 100; a->reqmin = 0;
    }
    
    tickstep = gwy_axis_quantize_normal_tics(range, a->par.major_maxticks); /*step*/
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
    a->max = majorbase - tickstep;
    
    
    i = 0;
    /*minor tics*/
    do {
        mit.value = minorbase;
        /*printf("gwyaxis.c:893: appending %f (%dth)\n", (gdouble)mit.value, i);*/
        g_array_append_val(a->miticks, mit);
        minorbase += minortickstep;
        i++;
    } while (minorbase <= a->max);

    return 0;
}

static gint
gwy_axis_logscale(GwyAxis *a)
{
    gint i;
    gdouble max, min, _min, tickstep, base;
    GwyAxisLabeledTick mjt;
    GwyAxisTick mit;

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
    tickstep = gwy_axis_dbl_raise(10.0, (gint)floor(min));
    base = ceil(pow(10, min)/tickstep)*tickstep;
    max = a->max;
    i = 0;
    do {
         /*here, tickstep must be adapted do scale*/
         tickstep = gwy_axis_dbl_raise(10.0, (gint)floor(log10(base*1.01)));
             mit.value = log10(base);
         g_array_append_val(a->miticks, mit);
         base += tickstep;
         i++;
    } while (base<=max && i<(a->par.major_maxticks*10));

    return 0;
}


/* FIXME: return TRUE for success, not 0 */
static gint
gwy_axis_scale(GwyAxis *a)
{
    gsize i;
    GwyAxisLabeledTick *mjt;

    
    /*never use logarithmic mode for negative numbers*/
    if (a->min < 0 && a->is_logarithmic == TRUE)
        return 1; /*this is an error*/

    /*remove old ticks*/
    for (i = 0; i < a->mjticks->len; i++) {
        mjt = &g_array_index(a->mjticks, GwyAxisLabeledTick, i);
        g_string_free(mjt->ttext, TRUE);
    }
    g_array_free(a->mjticks, FALSE);
    g_array_free(a->miticks, FALSE);

    a->mjticks = g_array_new(FALSE, FALSE, sizeof(GwyAxisLabeledTick));
    a->miticks = g_array_new(FALSE, FALSE, sizeof(GwyAxisTick));

    /*find tick positions*/
    if (!a->is_logarithmic)
        gwy_axis_normalscale(a);
    else
        gwy_axis_logscale(a);
    /*label ticks*/
    gwy_axis_formatticks(a);
    /*precompute screen coordinates of ticks (must be done after each geometry change)*/

    gwy_axis_signal_rescaled(a);
    return 0;
}

static gint
gwy_axis_precompute(GwyAxis *a, gint scrmin, gint scrmax)
{
    guint i;
    gdouble dist, range;
    GwyAxisLabeledTick *pmjt;
    GwyAxisTick *pmit;

    dist = (gdouble)scrmax-scrmin-1;
    range = a->max - a->min;
    if (a->is_logarithmic) range = log10(a->max)-log10(a->min);

    for (i = 0; i < a->mjticks->len; i++) {
        pmjt = &g_array_index (a->mjticks, GwyAxisLabeledTick, i);
        if (!a->is_logarithmic)
            pmjt->t.scrpos = (gint)(0.5 + scrmin
                                    + (pmjt->t.value - a->min)/range*dist);
        else
            pmjt->t.scrpos = (gint)(0.5 + scrmin
                                    + (pmjt->t.value - log10(a->min))/range*dist);
    }

    for (i = 0; i < a->miticks->len; i++) {
        pmit = &g_array_index (a->miticks, GwyAxisTick, i);
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
gwy_axis_formatticks(GwyAxis *a)
{
    guint i;
    gdouble value;
    gdouble range, crange; /*only for automode and precision*/
    gdouble average;
    GwySIValueFormat *format = NULL;
    GwyAxisLabeledTick mji, mjx, *pmjt;
    /*determine range*/
    if (a->mjticks->len == 0) {
        g_warning("No ticks found");
        return 1;
    }
    mji = g_array_index(a->mjticks, GwyAxisLabeledTick, 0);
    mjx = g_array_index(a->mjticks, GwyAxisLabeledTick, a->mjticks->len - 1);
    if (!a->is_logarithmic)
    {
        average = fabs(mjx.t.value + mji.t.value)/2;
        range = fabs(mjx.t.value - mji.t.value);
    }
    else
    {
        average = 0;
        range = fabs(pow(10, mjx.t.value) - pow(10, mji.t.value));
    }

    
    /*move exponents to axis label*/
    if (a->par.major_printmode == GWY_AXIS_SCALE_FORMAT_AUTO
        && (range > 1000 || average > 1000 || range < 0.001 || average < 0.001))
    {
        format = gwy_si_unit_get_format(a->unit, GWY_SI_UNIT_FORMAT_MARKUP,
                                        MAX(average, range), format);
        if (a->magnification_string) g_string_free(a->magnification_string, TRUE);
        a->magnification_string = g_string_new(format->units);
        a->magnification = format->magnitude;
        range /= format->magnitude;
    } 
    else
    {
        if (a->magnification_string) g_string_free(a->magnification_string, TRUE);
        a->magnification_string = NULL;
        a->magnification = 1;
    }


    for (i = 0; i< a->mjticks->len; i++)
    {
        /*find the value we want to put in string*/
        pmjt = &g_array_index(a->mjticks, GwyAxisLabeledTick, i);
        if (!a->is_logarithmic)
            value = pmjt->t.value;
        else
            value = pow(10, pmjt->t.value);

        if (format) 
            value /= format->magnitude;


        /*fill dependent to mode*/
        if (a->par.major_printmode == GWY_AXIS_SCALE_FORMAT_FLOAT
            || (a->par.major_printmode == GWY_AXIS_SCALE_FORMAT_AUTO
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
        else if (a->par.major_printmode == GWY_AXIS_SCALE_FORMAT_EXP
                 || (a->par.major_printmode == GWY_AXIS_SCALE_FORMAT_AUTO
                     && (fabs(value) > 10000 || fabs(value) < 0.001))) {
            g_string_printf(pmjt->ttext,"%.1E", value);
            if (value == 0)
                g_string_printf(pmjt->ttext,"0");
        }
        else if (a->par.major_printmode == GWY_AXIS_SCALE_FORMAT_INT) {
            g_string_printf(pmjt->ttext,"%d", (int)(value+0.5));
        }
    }
    
    if (format) g_free(format->units);
    
    return 0;
}



/**
 * gwy_axis_set_visible:
 * @axiser: axiser widget 
 * @is_visible: visibility
 *
 * Sets visibility of axiser.
 **/
void
gwy_axis_set_visible(GwyAxis *axiser, gboolean is_visible)
{
    axiser->is_visible = is_visible;
}

/**
 * gwy_axis_set_auto:
 * @axiser: axiser widget 
 * @is_auto: auto preperty
 *
 * Sets the auto property. If TRUE, axiser changes fonts
 * and ticks sizes to produce reasonable output at different
 * widget sizes.
 **/
void
gwy_axis_set_auto(GwyAxis *axiser, gboolean is_auto)
{
    axiser->is_auto = is_auto;
}

/**
 * gwy_axis_set_req:
 * @axiser: axiser widget 
 * @min: minimum requisistion
 * @max: maximum requisition
 *
 * Set requisition of axiser boundaries. Axiser will fix the boundaries
 * to satisfy requisition but still have reasonable tick values and spacing.
 **/
void
gwy_axis_set_req(GwyAxis *axiser, gdouble min, gdouble max)
{
    axiser->reqmin = min;
    axiser->reqmax = max;
    
    /*prevent axiser to allow null range. It has no sense*/
    if (min==max) axiser->reqmax += 10.0;
  
    gwy_axis_adjust(axiser,
                    (GTK_WIDGET(axiser))->allocation.width,
                    (GTK_WIDGET(axiser))->allocation.height);
}

/**
 * gwy_axis_set_style:
 * @axiser: axiser widget 
 * @style: axiser style
 *
 * Set axiser style. The style affects used tick sizes, fonts etc.
 **/
void
gwy_axis_set_style(GwyAxis *axiser, GwyAxisParams style)
{
    axiser->par = style;
    gwy_axis_adjust(axiser,
                    (GTK_WIDGET(axiser))->allocation.width,
                    (GTK_WIDGET(axiser))->allocation.height);
}

/**
 * gwy_axis_get_maximum:
 * @axiser: axiser widget 
 *
 * 
 *
 * Returns: real maximum of axiser
 **/
gdouble
gwy_axis_get_maximum(GwyAxis *axiser)
{
    return axiser->max;
}

/**
 * gwy_axis_get_minimum:
 * @axiser: axiser widget 
 *
 * 
 *
 * Returns: real minimum of axiser
 **/
gdouble
gwy_axis_get_minimum(GwyAxis *axiser)
{
    return axiser->min;
}

/**
 * gwy_axis_get_reqmaximum:
 * @axiser: axiser widget 
 *
 * 
 *
 * Returns: axiser requisition maximum
 **/
gdouble
gwy_axis_get_reqmaximum(GwyAxis *axiser)
{
    return axiser->reqmax;
}

/**
 * gwy_axis_get_reqminimum:
 * @axiser: axiser widget 
 *
 * 
 *
 * Returns: axiser requisition minimum
 **/
gdouble
gwy_axis_get_reqminimum(GwyAxis *axiser)
{
    return axiser->reqmin;
}

/**
 * gwy_axis_set_label:
 * @axiser: axiser widget 
 * @label_text: label to be set
 *
 * sets the label text
 **/
void
gwy_axis_set_label(GwyAxis *axiser, GString *label_text)
{
    gwy_debug("label_text = <%s>", label_text->str);
    g_string_assign(axiser->label_text, label_text->str);
    gwy_sci_text_set_text(GWY_SCI_TEXT(GWY_AXIS_DIALOG(axiser->dialog)->sci_text),
                          label_text->str);
    g_signal_emit(axiser, axiser_signals[LABEL_UPDATED], 0);
    gtk_widget_queue_draw(GTK_WIDGET(axiser));
}

/**
 * gwy_axis_get_label:
 * @axiser: axiser widget 
 *
 * 
 *
 * Returns: axiser label string
 **/
GString*
gwy_axis_get_label(GwyAxis *axiser)
{
    return axiser->label_text;
}

/**
 * gwy_axis_set_unit:
 * @axiser: axiser widget 
 * @unit: axiser unit
 *
 * Sets the axiser unit. This will be added automatically
 * to the label.
 **/
void
gwy_axis_set_unit(GwyAxis *axiser, GwySIUnit *unit)
{
    axiser->unit = GWY_SI_UNIT(gwy_serializable_duplicate(G_OBJECT(unit)));
    axiser->has_unit = 1;
}


/**
 * gwy_axis_enable_label_edit:
 * @axiser: Axiser widget 
 * @enable: enable/disable user to change axiser label 
 *
 * Enables/disables user to change axiser label by clicking on axiser widget.
 *
 * Since: 1.3.
 **/
void
gwy_axis_enable_label_edit(GwyAxis *axiser, gboolean enable)
{
    axiser->enable_label_edit = enable;
}

gdouble     
gwy_axis_get_magnification (GwyAxis *axiser)
{
    return axiser->magnification;
}

GString*    
gwy_axis_get_magnification_string(GwyAxis *axiser)
{
    if (axiser->magnification_string != NULL)
        return g_string_new(axiser->magnification_string->str);
    else return g_string_new("");
}


GString*    
gwy_axis_export_vector (GwyAxis *axiser, gint xmin, gint ymin, gint width, gint height)
{
    GString *out;
    gdouble mult;
    GwyAxisLabeledTick *pmjt;
    GwyAxisTick *pmit;
    gint i;
    gint fontsize = 15;
    gint linewidth = 2;

    if (axiser->orientation == GTK_POS_TOP || axiser->orientation == GTK_POS_BOTTOM)
        mult = width/(axiser->max - axiser->min);
    else
        mult = height/(axiser->max - axiser->min);

    out = g_string_new("%%Axis\n");
   
    g_string_append_printf(out, "/Times-Roman findfont\n");
    g_string_append_printf(out, "%d scalefont\n setfont\n", fontsize);
    g_string_append_printf(out, "%d setlinewidth\n", linewidth);

    /*draw axis*/
    switch (axiser->orientation) {
        /*note that the eps geometry starts in the bottom left point*/
            case GTK_POS_TOP:
            g_string_append_printf(out, "%d %d M\n", xmin, ymin);
            g_string_append_printf(out, "%d %d L\n", xmin + width, ymin);
            break;

            case GTK_POS_BOTTOM:
            g_string_append_printf(out, "%d %d M\n", xmin, ymin + height);
            g_string_append_printf(out, "%d %d L\n", xmin + width, ymin + height);
            break;

            case GTK_POS_RIGHT:
            g_string_append_printf(out, "%d %d M\n", xmin, ymin);
            g_string_append_printf(out, "%d %d L\n", xmin, ymin + height);
            break;

            case GTK_POS_LEFT:
            g_string_append_printf(out, "%d %d M\n", xmin + width, ymin);
            g_string_append_printf(out, "%d %d L\n", xmin + width, ymin + height);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    g_string_append_printf(out, "stroke\n");

    /*draw ticks*/
    g_string_append_printf(out, "%%MajorTicks\n");
    for (i = 0; i < axiser->mjticks->len; i++) {
        pmjt = &g_array_index (axiser->mjticks, GwyAxisLabeledTick, i);

        switch (axiser->orientation) {
            case GTK_POS_TOP:
            g_string_append_printf(out, "%d %d M\n", (gint)(xmin + pmjt->t.value*mult), ymin);
            g_string_append_printf(out, "%d %d L\n", (gint)(xmin + pmjt->t.value*mult), ymin+axiser->par.major_length);
            g_string_append_printf(out, "%d %d R\n", -(gint)(pmjt->ttext->len*(fontsize/4)), 5);
            g_string_append_printf(out, "(%s) show\n", pmjt->ttext->str);
            break;

            case GTK_POS_BOTTOM:
            g_string_append_printf(out, "%d %d M\n", (gint)(xmin + pmjt->t.value*mult), ymin + height);
            g_string_append_printf(out, "%d %d L\n", (gint)(xmin + pmjt->t.value*mult), ymin + height - axiser->par.major_length);
            g_string_append_printf(out, "%d %d R\n", -(gint)(pmjt->ttext->len*(fontsize/4)), -fontsize);
            g_string_append_printf(out, "(%s) show\n", pmjt->ttext->str);
            break;

            case GTK_POS_LEFT:
            g_string_append_printf(out, "%d %d M\n", xmin + width, (gint)(ymin + pmjt->t.value*mult));
            g_string_append_printf(out, "%d %d L\n", xmin + width - axiser->par.major_length, (gint)(ymin + pmjt->t.value*mult));
            g_string_append_printf(out, "%d %d R\n", -(gint)(pmjt->ttext->len*(fontsize/2)) - 5, -(gint)(fontsize/2.5));
            g_string_append_printf(out, "(%s) show\n", pmjt->ttext->str);
            break;

            case GTK_POS_RIGHT:
            g_string_append_printf(out, "%d %d M\n", xmin, (gint)(ymin + pmjt->t.value*mult));
            g_string_append_printf(out, "%d %d L\n", xmin + axiser->par.major_length, (gint)(ymin + pmjt->t.value*mult));
            g_string_append_printf(out, "%d %d R\n", 5, -(gint)(fontsize/2.5));
            g_string_append_printf(out, "(%s) show\n", pmjt->ttext->str);
            break;

            default:
            g_assert_not_reached();
            break;
        }
        g_string_append_printf(out, "stroke\n");

  }
    g_string_append_printf(out, "%%MinorTicks\n");
    for (i = 0; i < axiser->miticks->len; i++) {
        pmit = &g_array_index (axiser->miticks, GwyAxisTick, i);

        switch (axiser->orientation) {
            case GTK_POS_TOP:
            g_string_append_printf(out, "%d %d M\n", (gint)(xmin + pmit->value*mult), ymin);
            g_string_append_printf(out, "%d %d L\n", (gint)(xmin + pmit->value*mult), ymin+axiser->par.minor_length);
            break;

            case GTK_POS_BOTTOM:
            g_string_append_printf(out, "%d %d M\n", (gint)(xmin + pmit->value*mult), ymin + height);
            g_string_append_printf(out, "%d %d L\n", (gint)(xmin + pmit->value*mult), ymin + height - axiser->par.minor_length);
            break;

            case GTK_POS_LEFT:
            g_string_append_printf(out, "%d %d M\n", xmin + width, (gint)(ymin + pmit->value*mult));
            g_string_append_printf(out, "%d %d L\n", xmin + width - axiser->par.minor_length, (gint)(ymin + pmit->value*mult));
            break;

            case GTK_POS_RIGHT:
            g_string_append_printf(out, "%d %d M\n", xmin, (gint)(ymin + pmit->value*mult));
            g_string_append_printf(out, "%d %d L\n", xmin +  axiser->par.minor_length, (gint)(ymin + pmit->value*mult));
            break;

            default:
            g_assert_not_reached();
            break;
        }
        g_string_append_printf(out, "stroke\n");
  }


    return out;
}



/************************** Documentation ****************************/

/**
 * GwyAxisScaleFormat:
 * @GWY_AXIS_FLOAT: Floating point format.
 * @GWY_AXIS_EXP: Exponential (`scienfitic') format.
 * @GWY_AXIS_INT: Integer format.
 * @GWY_AXIS_AUTO: Automatical format.
 *
 * Labeled axiser tick mark format.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
