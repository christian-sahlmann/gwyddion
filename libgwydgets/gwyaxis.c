/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <pango/pango.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwydgetenums.h>
#include <libgwydgets/gwyscitext.h>
#include <libgwydgets/gwyaxisdialog.h>
#include <libgwydgets/gwyaxis.h>

enum {
    RESCALED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_LABEL,
    PROP_OUTER_BORDER_WIDTH,
    /* XXX: Where this stuff *really* belongs to? To the model? */
    PROP_AUTO,
    PROP_MAJOR_LENGTH,
    PROP_MAJOR_THICKNESS,
    PROP_MAJOR_MAXTICKS,
    PROP_MINOR_LENGTH,
    PROP_MINOR_THICKNESS,
    PROP_MINOR_DIVISION,
    PROP_LINE_THICKNESS,
    PROP_LAST
};

typedef struct {
    gint xmin;         /*x offset of the active area with respect to drawable left border*/
    gint ymin;         /*y offset of the active area with respect to drawable top border*/
    gint height;       /*active area height*/
    gint width;        /*active area width*/
} GwyAxisActiveAreaSpecs;

typedef struct {
    gdouble value;      /*tick value*/
    gint scrpos;        /*precomputed tick screen position*/
} GwyAxisTick;

typedef struct {
    GwyAxisTick t;
    GString *ttext;
} GwyAxisLabeledTick;

static void     gwy_axis_finalize      (GObject *object);
static void     gwy_axis_set_property  (GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec);
static void     gwy_axis_get_property  (GObject*object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec);
static void     gwy_axis_notify        (GObject *object,
                                        GParamSpec *pspec);
static void     gwy_axis_destroy       (GtkObject *object);
static void     gwy_axis_realize       (GtkWidget *widget);
static void     gwy_axis_unrealize     (GtkWidget *widget);
static void     gwy_axis_size_request  (GtkWidget *widget,
                                        GtkRequisition *requisition);
static void     gwy_axis_size_allocate (GtkWidget *widget,
                                        GtkAllocation *allocation);
static gboolean gwy_axis_expose        (GtkWidget *widget,
                                        GdkEventExpose *event);
static gboolean gwy_axis_button_press  (GtkWidget *widget,
                                        GdkEventButton *event);

static gdouble gwy_axis_dbl_raise           (gdouble x,
                                             gint y);
static gdouble gwy_axis_quantize_normal_tics(gdouble arg,
                                             gint guide);
static gboolean gwy_axis_normalscale         (GwyAxis *a);
static gboolean gwy_axis_logscale           (GwyAxis *a);
static gint    gwy_axis_scale               (GwyAxis *a);
static gint    gwy_axis_formatticks         (GwyAxis *a);
static GwySIValueFormat* gwy_axis_calculate_format(GwyAxis *axis,
                                                   GwySIValueFormat *format);
static gint    gwy_axis_precompute          (GwyAxis *a,
                                             gint scrmin,
                                             gint scrmax);
static void    gwy_axis_draw_axis           (GdkDrawable *drawable,
                                             GdkGC *gc,
                                             GwyAxisActiveAreaSpecs *specs,
                                             GwyAxis *axis);
static void    gwy_axis_draw_ticks          (GdkDrawable *drawable,
                                             GdkGC *gc,
                                             GwyAxisActiveAreaSpecs *specs,
                                             GwyAxis *axis);
static void    gwy_axis_draw_tlabels        (GdkDrawable *drawable,
                                             GdkGC *gc,
                                             GwyAxisActiveAreaSpecs *specs,
                                             GwyAxis *axis);
static void    gwy_axis_format_label        (GwyAxis *axis,
                                             PangoLayout *layout);
static void    gwy_axis_draw_label          (GdkDrawable *drawable,
                                             GdkGC *gc,
                                             GwyAxisActiveAreaSpecs *specs,
                                             GwyAxis *axis);
static void    gwy_axis_autoset             (GwyAxis *axis,
                                             gint width,
                                             gint height);
static void    gwy_axis_adjust              (GwyAxis *axis,
                                             gint width,
                                             gint height);
static void    gwy_axis_entry               (GwySciText *sci_text,
                                             GwyAxis *axis);

static guint axis_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GwyAxis, gwy_axis, GTK_TYPE_WIDGET)

static void
gwy_axis_class_init(GwyAxisClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    gobject_class->finalize = gwy_axis_finalize;
    gobject_class->set_property = gwy_axis_set_property;
    gobject_class->get_property = gwy_axis_get_property;
    gobject_class->notify = gwy_axis_notify;

    object_class->destroy = gwy_axis_destroy;

    widget_class->realize = gwy_axis_realize;
    widget_class->expose_event = gwy_axis_expose;
    widget_class->size_request = gwy_axis_size_request;
    widget_class->unrealize = gwy_axis_unrealize;
    widget_class->size_allocate = gwy_axis_size_allocate;
    widget_class->button_press_event = gwy_axis_button_press;

    axis_signals[RESCALED] =
        g_signal_new("rescaled",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyAxisClass, rescaled),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    g_object_class_install_property
        (gobject_class,
         PROP_LABEL,
         g_param_spec_string("label",
                             "Label",
                             "Axis label (without units).",
                             "",
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_OUTER_BORDER_WIDTH,
         g_param_spec_int("outer-border-width",
                          "Outer border width",
                          "The extra amount of space left on the outer side "
                          "of an axis.  This space is also retained when "
                          "axis is set non-visible.",
                          0, G_MAXINT, 5,
                          G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_AUTO,
         g_param_spec_boolean("auto",
                              "Autoscale",
                              "Autoscale ticks with changing content",
                              TRUE,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_MAJOR_LENGTH,
         g_param_spec_int("major-length",
                          "Major ticks length",
                          "Major ticks length",
                          0,
                          20,
                          5,
                          G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_MAJOR_THICKNESS,
         g_param_spec_int("major-thickness",
                          "Major ticks thickness",
                          "Major ticks thickness",
                          0,
                          20,
                          5,
                          G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_MAJOR_MAXTICKS,
         g_param_spec_int("major-maxticks",
                          "Major ticks maximum number",
                          "Major ticks maximum number",
                          0,
                          50,
                          5,
                          G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_MINOR_LENGTH,
         g_param_spec_int("minor-length",
                          "Minor ticks length",
                          "Minor ticks length",
                          0,
                          20,
                          5,
                          G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_MINOR_THICKNESS,
         g_param_spec_int("minor-thickness",
                          "Minor ticks thickness",
                          "Minor ticks thickness",
                          0,
                          20,
                          5,
                          G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_MINOR_DIVISION,
         g_param_spec_int("minor-division",
                          "Minor ticks division",
                          "Minor ticks division",
                          0,
                          20,
                          5,
                          G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_LINE_THICKNESS,
         g_param_spec_int("line-thickness",
                          "Line thickness",
                          "Axis main line thickness",
                          0,
                          20,
                          5,
                          G_PARAM_READWRITE));
}

static void
gwy_axis_init(GwyAxis *axis)
{
    PangoContext *context;
    PangoFontDescription *description;
    gint size;

    axis->orientation = GTK_POS_BOTTOM;

    axis->is_visible = TRUE;
    axis->is_auto = TRUE;
    axis->par.major_printmode = GWY_AXIS_SCALE_FORMAT_AUTO;

    axis->par.major_length = 10;
    axis->par.major_thickness = 1;
    axis->par.major_maxticks = 20;

    axis->par.minor_length = 5;
    axis->par.minor_thickness = 1;
    axis->par.minor_division = 10;
    axis->par.line_thickness = 1;

    axis->reqmax = 1.0;
    axis->reqmin = 0.0;

    axis->enable_label_edit = TRUE;

    axis->unit = gwy_si_unit_new(NULL);
    axis->magnification = 1;

    axis->outer_border_width = 5;

    axis->mjticks = g_array_new(FALSE, FALSE, sizeof(GwyAxisLabeledTick));
    axis->miticks = g_array_new(FALSE, FALSE, sizeof(GwyAxisTick));
    axis->label_text = g_string_new(NULL);

    context = gtk_widget_get_pango_context(GTK_WIDGET(axis));
    description = pango_context_get_font_description(context);

    /* Make major font a bit smaller */
    axis->par.major_font = pango_font_description_copy(description);
    size = pango_font_description_get_size(axis->par.major_font);
    size = MAX(1, size*10/11);
    pango_font_description_set_size(axis->par.major_font, size);

    /* Keep label font to default. */
    axis->par.label_font = pango_font_description_copy(description);
}

/**
 * gwy_axis_new:
 * @orientation:  axis orientation
 *
 * Creates a new axis.
 *
 * Returns: New axis as a #GtkWidget.
 **/
GtkWidget*
gwy_axis_new(gint orientation)
{
    GwyAxis *axis;

    gwy_debug("");

    axis = (GwyAxis*)g_object_new(GWY_TYPE_AXIS, NULL);
    axis->orientation = orientation;

    return GTK_WIDGET(axis);
}

static void
gwy_axis_finalize(GObject *object)
{
    GwyAxis *axis;

    g_return_if_fail(GWY_IS_AXIS(object));

    axis = GWY_AXIS(object);

    g_string_free(axis->label_text, TRUE);
    g_array_free(axis->mjticks, TRUE);
    g_array_free(axis->miticks, TRUE);
    if (axis->mjpubticks)
        g_array_free(axis->mjpubticks, TRUE);

    gwy_object_unref(axis->unit);
    gwy_object_unref(axis->gc);

    if (axis->par.major_font)
        pango_font_description_free(axis->par.major_font);
    if (axis->par.label_font)
        pango_font_description_free(axis->par.label_font);

    G_OBJECT_CLASS(gwy_axis_parent_class)->finalize(object);
}

static void
gwy_axis_destroy(GtkObject *object)
{
    GwyAxis *axis;

    axis = GWY_AXIS(object);
    if (axis->dialog) {
        gtk_widget_destroy(axis->dialog);
        axis->dialog = NULL;
    }

    GTK_OBJECT_CLASS(gwy_axis_parent_class)->destroy(object);
}

static void
gwy_axis_set_property(GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
    GwyAxis *axis = GWY_AXIS(object);

    switch (prop_id) {
        case PROP_LABEL:
        gwy_axis_set_label(axis, g_value_get_string(value));
        break;

        case PROP_AUTO:
        axis->is_auto = g_value_get_boolean(value);
        break;

        case PROP_OUTER_BORDER_WIDTH:
        axis->outer_border_width = g_value_get_int(value);
        gtk_widget_queue_resize(GTK_WIDGET(axis));
        break;

        case PROP_MAJOR_LENGTH:
        axis->par.major_length = g_value_get_int(value);
        break;

        case PROP_MAJOR_THICKNESS:
        axis->par.major_thickness = g_value_get_int(value);
        break;

        case PROP_MAJOR_MAXTICKS:
        axis->par.major_maxticks = g_value_get_int(value);
        break;

        case PROP_MINOR_LENGTH:
        axis->par.minor_length = g_value_get_int(value);
        break;

        case PROP_MINOR_THICKNESS:
        axis->par.minor_thickness = g_value_get_int(value);
        break;

        case PROP_MINOR_DIVISION:
        axis->par.minor_division = g_value_get_int(value);
        break;

        case PROP_LINE_THICKNESS:
        axis->par.line_thickness = g_value_get_int(value);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_axis_get_property(GObject*object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    GwyAxis *axis = GWY_AXIS(object);

    switch (prop_id) {
        case PROP_LABEL:
        g_value_set_string(value, axis->label_text->str);
        break;

        case PROP_AUTO:
        g_value_set_boolean(value, axis->is_auto);
        break;

        case PROP_OUTER_BORDER_WIDTH:
        g_value_set_int(value, axis->outer_border_width);
        break;

        case PROP_MAJOR_LENGTH:
        g_value_set_int(value, axis->par.major_length);
        break;

        case PROP_MAJOR_THICKNESS:
        g_value_set_int(value, axis->par.major_thickness);
        break;

        case PROP_MAJOR_MAXTICKS:
        g_value_set_int(value, axis->par.major_maxticks);
        break;

        case PROP_MINOR_LENGTH:
        g_value_set_int(value, axis->par.minor_length);
        break;

        case PROP_MINOR_THICKNESS:
        g_value_set_int(value, axis->par.minor_thickness);
        break;

        case PROP_MINOR_DIVISION:
        g_value_set_int(value, axis->par.minor_division);
        break;

        case PROP_LINE_THICKNESS:
        g_value_set_int(value, axis->par.line_thickness);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_axis_unrealize(GtkWidget *widget)
{
    GwyAxis *axis;

    axis = GWY_AXIS(widget);

    if (GTK_WIDGET_CLASS(gwy_axis_parent_class)->unrealize)
        GTK_WIDGET_CLASS(gwy_axis_parent_class)->unrealize(widget);
}

static void
gwy_axis_realize(GtkWidget *widget)
{
    GwyAxis *axis;
    GdkWindowAttr attributes;
    gint i, attributes_mask;
    GtkStyle *style;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_AXIS(widget));

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    axis = GWY_AXIS(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget)
                            | GDK_EXPOSURE_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_POINTER_MOTION_MASK
                            | GDK_POINTER_MOTION_HINT_MASK;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
                                    &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);

    /* set background to white forever */
    style = gtk_style_copy(widget->style);
    for (i = 0; i < 5; i++) {
        style->bg_gc[i] = widget->style->white_gc;
        style->bg[i] = widget->style->white;
    }
    gtk_style_set_background(style, widget->window, GTK_STATE_NORMAL);
    g_object_unref(style);

    axis->gc = gdk_gc_new(widget->window);

    /* compute ticks */
    gwy_axis_adjust(axis, widget->allocation.width, widget->allocation.height);
}

static void
gwy_axis_size_request(GtkWidget *widget,
                      GtkRequisition *requisition)
{
    PangoRectangle rect_label, rect;
    PangoLayout *layout;
    const GwyAxisLabeledTick *pmjt;
    GwyAxis *axis;
    guint i;
    gint sep;

    axis = GWY_AXIS(widget);
    gwy_debug("%p(%d)", axis, axis->orientation);

    requisition->height = requisition->width = 0;

    switch (axis->orientation) {
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
        requisition->width += axis->outer_border_width;
        requisition->width += axis->par.line_thickness;
        break;

        case GTK_POS_BOTTOM:
        case GTK_POS_TOP:
        requisition->height += axis->outer_border_width;
        requisition->height += axis->par.line_thickness;
        break;

        default:
        g_assert_not_reached();
        break;
    }

    if (!axis->is_visible)
        return;

    layout = gtk_widget_create_pango_layout(widget, "");
    gwy_axis_format_label(axis, layout);
    pango_layout_get_pixel_extents(layout, NULL, &rect_label);

    sep = 3;

    switch (axis->orientation) {
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
        requisition->height += rect_label.width + 2*sep;

        requisition->width += rect_label.height;
        requisition->width += axis->par.major_length;
        requisition->width += 2*sep;

        /* FIXME: This does not work, at this point we do not know the real
         * tick labels yet */
        rect_label.width = 0;
        pango_layout_set_font_description(layout, axis->par.major_font);
        for (i = 0; i < axis->mjticks->len; i++) {
            pmjt = &g_array_index(axis->mjticks, GwyAxisLabeledTick, i);
            pango_layout_set_markup(layout, pmjt->ttext->str, pmjt->ttext->len);
            pango_layout_get_pixel_extents(layout, NULL, &rect);
            rect_label.width = MAX(rect_label.width, rect.width);
        }
        if (!axis->mjticks->len) {
            axis->rerequest_size++;
            if (G_UNLIKELY(axis->rerequest_size > 3)) {
                g_warning("Axis size rerequest repeated 3 times, giving up");
                axis->rerequest_size = 0;
            }
        }
        gwy_debug("%p must rerequest: %d", axis, axis->rerequest_size);
        requisition->width += rect_label.width;
        break;

        case GTK_POS_BOTTOM:
        case GTK_POS_TOP:
        requisition->width += rect_label.width + 2*sep;

        requisition->height += rect_label.height;
        requisition->height += axis->par.major_length;
        requisition->height += 2*sep;

        pango_layout_set_text(layout, "0.9", -1);
        pango_layout_get_pixel_extents(layout, NULL, &rect_label);
        requisition->height += rect_label.height;
        break;
    }

    g_object_unref(layout);
}

static void
gwy_axis_size_allocate(GtkWidget *widget,
                       GtkAllocation *allocation)
{
    GwyAxis *axis;

    gwy_debug("");

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_AXIS(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;

    axis = GWY_AXIS(widget);

    if (GTK_WIDGET_REALIZED(widget))
        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
    gwy_axis_adjust(axis, allocation->width, allocation->height);
}

static void
gwy_axis_adjust(GwyAxis *axis, gint width, gint height)
{
    gint scaleres, iterations;

    gwy_debug("%p(%d)", axis, axis->orientation);

    if (width == -1 && GTK_WIDGET_REALIZED(axis))
        width = GTK_WIDGET(axis)->allocation.width;
    if (height == -1 && GTK_WIDGET_REALIZED(axis))
        height = GTK_WIDGET(axis)->allocation.height;

    if (width == -1 || height == -1)
        return;

    if (axis->is_auto)
        gwy_axis_autoset(axis, width, height);
    iterations = 0;
    if (axis->is_logarithmic)
        gwy_axis_scale(axis);
    else {
        do {
            scaleres = gwy_axis_scale(axis);
            /*printf("scale: %d   iterations: %d\n", scaleres, iterations);*/
            if (scaleres > 0)
                axis->par.major_maxticks = (gint)(0.5*axis->par.major_maxticks);

            if (scaleres < 0)
                axis->par.major_maxticks = (gint)(2.0*axis->par.major_maxticks);

            iterations++;
        } while (scaleres != 0 && iterations < 10);
    }
    if (axis->orientation == GTK_POS_TOP
        || axis->orientation == GTK_POS_BOTTOM)
        gwy_axis_precompute(axis, 0, width);
    else
        gwy_axis_precompute(axis, 0, height);

    g_signal_emit(axis, axis_signals[RESCALED], 0);

    if (axis->rerequest_size) {
        gwy_debug("%p issuing rerequest", axis);
        axis->rerequest_size = 0;
        gtk_widget_queue_resize(GTK_WIDGET(axis));
    }
    if (GTK_WIDGET_DRAWABLE(axis))
        gtk_widget_queue_draw(GTK_WIDGET(axis));
}

static void
gwy_axis_autoset(GwyAxis *axis, gint width, gint height)
{
    if (axis->orientation == GTK_POS_TOP
        || axis->orientation == GTK_POS_BOTTOM) {

        axis->par.major_maxticks = width/50; /*empirical equation*/
        if (width < 300)
            axis->par.minor_division = 5;
        else
            axis->par.minor_division = 10;
    }
    if (axis->orientation == GTK_POS_RIGHT
        || axis->orientation == GTK_POS_LEFT) {

        axis->par.major_maxticks = height/40; /*empirical equation*/
        if (height < 150)
            axis->par.minor_division = 5;
        else
            axis->par.minor_division = 10;
    }
}

/* FIXME: ugly. */
static void
gwy_axis_notify(GObject *object,
                GParamSpec *pspec)
{
    void (*method)(GObject*, GParamSpec*);

    method = G_OBJECT_CLASS(gwy_axis_parent_class)->notify;
    if (method)
        method(object, pspec);

    if (g_type_is_a(pspec->owner_type, GWY_TYPE_AXIS))
        gwy_axis_adjust(GWY_AXIS(object), -1, -1);
}

/**
 * gwy_axis_set_logarithmic:
 * @axis: graph axis
 * @is_logarithmic: logarithmic mode
 *
 * Sets logarithmic mode.
 **/
void
gwy_axis_set_logarithmic(GwyAxis *axis,
                         gboolean is_logarithmic)
{
    g_return_if_fail(GWY_IS_AXIS(axis));

    is_logarithmic = !!is_logarithmic;
    if (axis->is_logarithmic == is_logarithmic)
        return;

    axis->is_logarithmic = is_logarithmic;
    gwy_axis_adjust(axis, -1, -1);
}

static gboolean
gwy_axis_expose(GtkWidget *widget,
                GdkEventExpose *event)
{
    GwyAxis *axis;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_AXIS(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    axis = GWY_AXIS(widget);

    gdk_window_clear_area(widget->window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);

    gwy_axis_draw_on_drawable(axis, widget->window, axis->gc,
                              0, 0,
                              widget->allocation.width,
                              widget->allocation.height);

    return FALSE;
}

/**
 * gwy_axis_draw_on_drawable:
 * @axis: An axis.
 * @drawable: Drawable to draw on.
 * @gc: Graphics context.
 *      It is modified by this function unpredictably.
 * @xmin: The minimum x-axis value.
 * @ymin: The minimum y-axis value.
 * @width: The width of the x-axis.
 * @height: The height of the y-axis.
 *
 * Draws the x and y-axis on a drawable
 **/
void
gwy_axis_draw_on_drawable(GwyAxis *axis,
                          GdkDrawable *drawable,
                          GdkGC *gc,
                          gint xmin, gint ymin,
                          gint width, gint height)
{
    GwyAxisActiveAreaSpecs specs;
    specs.xmin = xmin;
    specs.ymin = ymin;
    specs.width = width;
    specs.height = height;

    if (axis->is_standalone && axis->is_visible)
        gwy_axis_draw_axis(drawable, gc, &specs, axis);
    if (axis->is_visible) {
        gwy_axis_draw_ticks(drawable, gc, &specs, axis);
        gwy_axis_draw_tlabels(drawable, gc, &specs, axis);
        gwy_axis_draw_label(drawable, gc, &specs, axis);
    }
}

static void
gwy_axis_draw_axis(GdkDrawable *drawable,
                   GdkGC *gc,
                   GwyAxisActiveAreaSpecs *specs,
                   GwyAxis *axis)
{
    gdk_gc_set_line_attributes(gc, axis->par.line_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
    switch (axis->orientation) {
        case GTK_POS_BOTTOM:
        gdk_draw_line(drawable, gc,
                      specs->xmin, specs->ymin,
                      specs->xmin + specs->width-1, specs->ymin);
        break;

        case GTK_POS_TOP:
        gdk_draw_line(drawable, gc,
                      specs->xmin, specs->ymin + specs->height-1,
                      specs->xmin + specs->width-1, specs->ymin + specs->height-1);
        break;

        case GTK_POS_RIGHT:
        gdk_draw_line(drawable, gc,
                      specs->xmin, specs->ymin,
                      specs->xmin, specs->ymin + specs->height-1);
        break;

        case GTK_POS_LEFT:
        gdk_draw_line(drawable, gc,
                      specs->xmin + specs->width-1, specs->ymin,
                      specs->xmin + specs->width-1, specs->ymin + specs->height-1);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
gwy_axis_draw_ticks(GdkDrawable *drawable,
                    GdkGC *gc,
                    GwyAxisActiveAreaSpecs *specs,
                    GwyAxis *axis)
{
    guint i;
    GwyAxisTick *pmit;
    const GwyAxisLabeledTick *pmjt;

    gdk_gc_set_line_attributes(gc, axis->par.major_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    for (i = 0; i < axis->mjticks->len; i++) {
        pmjt = &g_array_index (axis->mjticks, GwyAxisLabeledTick, i);

        switch (axis->orientation) {
            case GTK_POS_BOTTOM:
            gdk_draw_line(drawable, gc,
                          specs->xmin + pmjt->t.scrpos,
                          specs->ymin,
                          specs->xmin + pmjt->t.scrpos,
                          specs->ymin + axis->par.major_length);
            break;

            case GTK_POS_TOP:
            gdk_draw_line(drawable, gc,
                          specs->xmin + pmjt->t.scrpos,
                          specs->ymin + specs->height-1,
                          specs->xmin + pmjt->t.scrpos,
                          specs->ymin + specs->height-1 - axis->par.major_length);
            break;

            case GTK_POS_RIGHT:
            gdk_draw_line(drawable, gc,
                          specs->xmin,
                          specs->ymin + specs->height-1 - pmjt->t.scrpos,
                          specs->xmin + axis->par.major_length,
                          specs->ymin + specs->height-1 - pmjt->t.scrpos);
            break;

            case GTK_POS_LEFT:
            gdk_draw_line(drawable, gc,
                          specs->xmin + specs->width-1,
                          specs->ymin + specs->height-1 - pmjt->t.scrpos,
                          specs->xmin + specs->width-1 - axis->par.major_length,
                          specs->ymin + specs->height-1 - pmjt->t.scrpos);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }

    gdk_gc_set_line_attributes(gc, axis->par.minor_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    for (i = 0; i < axis->miticks->len; i++) {
        pmit = &g_array_index (axis->miticks, GwyAxisTick, i);

        switch (axis->orientation) {
            case GTK_POS_BOTTOM:
            gdk_draw_line(drawable, gc,
                          specs->xmin + pmit->scrpos,
                          specs->ymin,
                          specs->xmin + pmit->scrpos,
                          specs->ymin + axis->par.minor_length);
            break;

            case GTK_POS_TOP:
            gdk_draw_line(drawable, gc,
                          specs->xmin + pmit->scrpos,
                          specs->ymin + specs->height-1,
                          specs->xmin + pmit->scrpos,
                          specs->ymin + specs->height-1 - axis->par.minor_length);
            break;

            case GTK_POS_RIGHT:
            gdk_draw_line(drawable, gc,
                          specs->xmin,
                          specs->ymin + specs->height-1 - pmit->scrpos,
                          specs->xmin + axis->par.minor_length,
                          specs->ymin + specs->height-1 - pmit->scrpos);
            break;

            case GTK_POS_LEFT:
            gdk_draw_line(drawable, gc,
                          specs->xmin + specs->width-1,
                          specs->ymin + specs->height-1 - pmit->scrpos,
                          specs->xmin + specs->width-1 - axis->par.minor_length,
                          specs->ymin + specs->height-1 - pmit->scrpos);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }
}

static void
gwy_axis_draw_tlabels(GdkDrawable *drawable,
                      GdkGC *gc,
                      GwyAxisActiveAreaSpecs *specs,
                      GwyAxis *axis)
{
    const GwyAxisLabeledTick *pmjt;
    PangoLayout *layout;
    PangoRectangle rect;
    gint sep, xpos = 0, ypos = 0;
    guint i;

    layout = gtk_widget_create_pango_layout(GTK_WIDGET(axis), "");
    pango_layout_set_font_description(layout, axis->par.major_font);

    sep = 3;

    for (i = 0; i < axis->mjticks->len; i++) {
        pmjt = &g_array_index(axis->mjticks, GwyAxisLabeledTick, i);
        pango_layout_set_markup(layout, pmjt->ttext->str, pmjt->ttext->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);

        switch (axis->orientation) {
            case GTK_POS_BOTTOM:
            xpos = specs->xmin + pmjt->t.scrpos - rect.width/2;
            ypos = specs->ymin + axis->par.major_length + sep;
            break;

            case GTK_POS_TOP:
            xpos = specs->xmin + pmjt->t.scrpos - rect.width/2;
            ypos = specs->ymin + specs->height-1
                   - axis->par.major_length - rect.height;
            break;

            case GTK_POS_RIGHT:
            xpos = specs->xmin + axis->par.major_length + sep;
            ypos = specs->ymin + specs->height-1 - pmjt->t.scrpos - rect.height/2;
            break;

            case GTK_POS_LEFT:
            xpos = specs->xmin + specs->width-1
                   - axis->par.major_length - sep - rect.width;
            ypos = specs->ymin + specs->height-1
                   - pmjt->t.scrpos - rect.height/2;
            break;

            default:
            g_assert_not_reached();
            break;
        }
        if ((specs->xmin + specs->width-1 - xpos) < rect.width)
            xpos = specs->xmin + specs->width-1 - rect.width;
        else if (xpos < 0)
            xpos = specs->xmin;

        if ((specs->ymin + specs->height-1 - ypos) < rect.height)
            ypos = specs->ymin + specs->height-1 - rect.height;
        else if (ypos < 0)
            ypos = specs->ymin;

        gdk_draw_layout(drawable, gc, xpos, ypos, layout);
    }

    g_object_unref(layout);
}

static void
gwy_axis_format_label(GwyAxis *axis,
                      PangoLayout *layout)
{
    gchar *units;
    GString *plotlabel;

    plotlabel = g_string_new(axis->label_text->str);
    units = gwy_si_unit_get_string(axis->unit, GWY_SI_UNIT_FORMAT_MARKUP);
    if ((axis->magnification_string && axis->magnification_string->len > 0)
        || *units) {
        g_string_append(plotlabel, " [");
        if (axis->magnification_string)
            g_string_append(plotlabel, axis->magnification_string->str);
        else
            g_string_append(plotlabel, units);
        g_string_append(plotlabel, "]");
    }
    g_free(units);

    pango_layout_set_font_description(layout, axis->par.major_font);
    pango_layout_set_markup(layout, plotlabel->str, plotlabel->len);
    g_string_free(plotlabel, TRUE);
}

static void
gwy_axis_draw_label(GdkDrawable *drawable,
                    GdkGC *gc,
                    GwyAxisActiveAreaSpecs *specs,
                    GwyAxis *axis)
{
    PangoLayout *layout;
    PangoContext *context;
    PangoRectangle rect;
    PangoMatrix matrix = PANGO_MATRIX_INIT;
    gint width, height;

    context = gtk_widget_create_pango_context(GTK_WIDGET(axis));
    layout = pango_layout_new(context);

    gwy_axis_format_label(axis, layout);
    pango_layout_get_pixel_extents(layout, NULL, &rect);

    switch (axis->orientation) {
        case GTK_POS_BOTTOM:
        gdk_draw_layout(drawable, gc,
                        specs->xmin + specs->width/2 - rect.width/2,
                        specs->ymin + specs->height - axis->outer_border_width
                        - rect.height,
                        layout);
        break;

        case GTK_POS_TOP:
        gdk_draw_layout(drawable, gc,
                        specs->xmin + specs->width/2 - rect.width/2,
                        specs->ymin + axis->outer_border_width,
                        layout);
        break;

        case GTK_POS_RIGHT:
        pango_matrix_rotate(&matrix, 90);
        pango_context_set_matrix(context, &matrix);
        pango_layout_context_changed(layout);
        pango_layout_get_size(layout, &width, &height);
        gdk_draw_layout(drawable, gc,
                        specs->width - axis->outer_border_width - rect.width,
                        specs->ymin + specs->height/2 - rect.width/2,
                        layout);
        break;

        case GTK_POS_LEFT:
        pango_matrix_rotate(&matrix, 90);
        pango_context_set_matrix(context, &matrix);
        pango_layout_context_changed(layout);
        pango_layout_get_size(layout, &width, &height);
        gdk_draw_layout(drawable, gc,
                        specs->xmin + axis->outer_border_width,
                        specs->ymin + specs->height/2 - rect.width/2,
                        layout);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    g_object_unref(layout);
    g_object_unref(context);
}

static gboolean
gwy_axis_button_press(GtkWidget *widget,
                      GdkEventButton *event)
{
    GwyAxis *axis;
    GtkWidget *scitext;

    gwy_debug("");

    axis = GWY_AXIS(widget);

    if (event->button != 1)
        return FALSE;

    if (axis->enable_label_edit) {
        if (!axis->dialog) {
            axis->dialog = _gwy_axis_dialog_new(axis);
            scitext = _gwy_axis_dialog_get_sci_text(axis->dialog);
            g_signal_connect(scitext, "edited",
                             G_CALLBACK(gwy_axis_entry), axis);
            g_signal_connect(axis->dialog, "response",
                             G_CALLBACK(gtk_widget_hide), NULL);
            gwy_sci_text_set_text(GWY_SCI_TEXT(scitext), axis->label_text->str);
        }
        gtk_widget_show_all(axis->dialog);
        gtk_window_present(GTK_WINDOW(axis->dialog));
    }

    return FALSE;
}

static void
gwy_axis_entry(GwySciText *sci_text, GwyAxis *axis)
{
    gchar *text;

    gwy_debug("");
    g_assert(GWY_IS_AXIS(axis));

    text = gwy_sci_text_get_text(sci_text);
    if (gwy_strequal(text, axis->label_text->str)) {
        g_free(text);
        return;
    }
    g_string_assign(axis->label_text, text);
    g_free(text);
    g_object_notify(G_OBJECT(axis), "label");
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
    else if (posns > 0.1)
        tics = 5;               /*0, 5, 10, 15*/
    else
        tics = ceil(xnorm);

    return (tics * power);
}

static gboolean
gwy_axis_normalscale(GwyAxis *a)
{
    gint i;
    GwyAxisTick mit;
    GwyAxisLabeledTick mjt;
    gdouble reqmin, reqmax;
    gdouble range, tickstep, majorbase, minortickstep, minorbase;

    /* Do something reasonable even with silly requests, they can easily occur
     * when graph model properties are updated sequentially */
    reqmin = MIN(a->reqmin, a->reqmax);
    reqmax = MAX(a->reqmax, a->reqmin);
    if (reqmax == reqmin) {
        if (reqmax == 0.0) {
            reqmin = 0.0;
            reqmax = 1.0;
        }
        else {
            range = reqmax;
            reqmax += range/2.0;
            reqmin -= range/2.0;
        }
    }
    gwy_debug("%p: reqmin: %g, reqmax: %g", a, reqmin, reqmax);

    range = fabs(reqmax - reqmin); /* total range of the field */

    if (range > 1e40 || range < -1e40) {
        g_warning("Axis with extreme range (>1e40)!");
        return TRUE;
    }

    /*step*/
    tickstep = gwy_axis_quantize_normal_tics(range, a->par.major_maxticks);
    majorbase = ceil(reqmin/tickstep)*tickstep; /*starting value*/
    minortickstep = tickstep/(gdouble)a->par.minor_division;
    minorbase = ceil(reqmin/minortickstep)*minortickstep;
    gwy_debug("majorbase: %g, tickstep: %g, minorbase: %g: minortickstep %g",
              majorbase, tickstep, minorbase, minortickstep);
    if (majorbase > reqmin) {
        majorbase -= tickstep;
        minorbase = majorbase;
        a->min = majorbase;
    }
    else
        a->min = reqmin;

    /*major tics*/
    i = 0;
    do {
        mjt.t.value = majorbase;
        mjt.ttext = g_string_new(NULL);
        a->mjticks = g_array_append_val(a->mjticks, mjt);
        majorbase += tickstep;
        i++;
    } while ((majorbase - tickstep) < reqmax && i < 2*a->par.major_maxticks);
    a->max = majorbase - tickstep;


    i = 0;
    /*minor tics*/
    do {
        mit.value = minorbase;
        a->miticks = g_array_append_val(a->miticks, mit);
        minorbase += minortickstep;
        i++;
    } while (minorbase <= a->max && i < 20*a->par.major_maxticks);

    gwy_debug("min: %g, max: %g", a->min, a->max);

    return TRUE;
}

static gboolean
gwy_axis_logscale(GwyAxis *a)
{
    gint i;
    gdouble max, min, logmax, logmin, tickstep, base;
    GwyAxisLabeledTick mjt;
    GwyAxisTick mit;

    max = MAX(a->reqmax, a->reqmin);
    min = MIN(a->reqmin, a->reqmax);

    if (min < 0.0 || (min == max && max == 0.0))
        return FALSE;

    /* No negative values are allowed, do anything just don't crash... */
    logmax = log10(max);
    if (min > 0.0)
        logmin = log10(min);
    else
        logmin = logmax - 1.0;

    /* Ticks will be linearly distributed again */
    /* Major ticks - will be equally ditributed in the log domain 1,10,100 */
    tickstep = (ceil(logmax) - floor(logmin))/MAX(a->par.major_maxticks - 1, 1);
    tickstep = ceil(tickstep);
    base = (ceil(logmin/tickstep) - 1)*tickstep; /* starting value */
    logmin = base;
    i = 0;
    do {
        mjt.t.value = base;
        mjt.ttext = g_string_new(NULL);
        g_array_append_val(a->mjticks, mjt);
        base += tickstep;
        i++;
    } while (i < 2 || ((base - tickstep) < logmax
                       && i < a->par.major_maxticks));
    logmax = base - tickstep;
    min = gwy_axis_dbl_raise(10.0, logmin);
    max = gwy_axis_dbl_raise(10.0, logmax);

    /* Minor ticks - will be equally distributed in the normal domain 1,2,3...
     * if the major tick step is only one order, otherwise distribute them in
     * the log domain too */
    if (tickstep == 1) {
        tickstep = min;
        base = tickstep;
        i = 0;
        do {
            /* Here, tickstep must be adapted do scale */
            tickstep = gwy_axis_dbl_raise(10.0, (gint)floor(log10(base*1.001)));
            mit.value = log10(base);
            g_array_append_val(a->miticks, mit);
            base += tickstep;
            i++;
        } while (base <= max && i < a->par.major_maxticks*20);
    }
    else {
        i = 0;
        tickstep = 1;
        base = logmin;
        do {
            mit.value = base;
            g_array_append_val(a->miticks, mit);
            base += tickstep;
            i++;
        } while ((base - tickstep) < logmax && i < a->par.major_maxticks*20);
    }

    a->max = max;
    a->min = min;

    return TRUE;
}


/* returns
 * 0 if everything went OK,
 * < 0 if there are not enough major ticks,
 * > 0 if there area too many ticks */
static gint
gwy_axis_scale(GwyAxis *a)
{
    gsize i;
    gint ret;
    GwyAxisLabeledTick *mjt;

    /*never use logarithmic mode for negative numbers*/
    if (a->reqmin < 0 && a->is_logarithmic)
        return 1; /*this is an error*/

    /*remove old ticks*/
    for (i = 0; i < a->mjticks->len; i++) {
        mjt = &g_array_index(a->mjticks, GwyAxisLabeledTick, i);
        g_string_free(mjt->ttext, TRUE);
    }
    g_array_set_size(a->mjticks, 0);
    g_array_set_size(a->miticks, 0);

    /*find tick positions*/
    if (a->is_logarithmic)
        gwy_axis_logscale(a);
    else
        gwy_axis_normalscale(a);
    /*label ticks*/
    ret = gwy_axis_formatticks(a);

    return ret;
}

/* precompute screen coordinates of ticks
 * (must be done after each geometry change) */
static gint
gwy_axis_precompute(GwyAxis *a, gint scrmin, gint scrmax)
{
    guint i;
    gdouble dist, range, dr;
    GwyAxisLabeledTick *pmjt;
    GwyAxisTick *pmit;

    dist = scrmax - scrmin - 1;
    if (a->is_logarithmic)
        range = log10(a->max/a->min);
    else
        range = a->max - a->min;

    dr = dist/range;
    for (i = 0; i < a->mjticks->len; i++) {
        pmjt = &g_array_index(a->mjticks, GwyAxisLabeledTick, i);
        if (a->is_logarithmic)
            pmjt->t.scrpos = GWY_ROUND(scrmin
                                       + (pmjt->t.value - log10(a->min))*dr);
        else
            pmjt->t.scrpos = GWY_ROUND(scrmin + (pmjt->t.value - a->min)*dr);
    }

    for (i = 0; i < a->miticks->len; i++) {
        pmit = &g_array_index(a->miticks, GwyAxisTick, i);
        if (a->is_logarithmic)
            pmit->scrpos = GWY_ROUND(scrmin + (pmit->value - log10(a->min))*dr);
        else
            pmit->scrpos = GWY_ROUND(scrmin + (pmit->value - a->min)*dr);
    }

    return 0;
}

static gint
gwy_axis_formatticks(GwyAxis *a)
{
    gdouble value;
    PangoLayout *layout;
    PangoContext *context;
    PangoRectangle rect;
    gint totalwidth = 0, totalheight = 0;
    GwySIValueFormat *format = NULL;
    GwyAxisLabeledTick *pmjt;
    gboolean human_fmt = TRUE;
    guint i;

    /* Determine range */
    if (a->mjticks->len == 0) {
        g_warning("No ticks found");
        return 1;
    }

    /* move exponents to axis label */
    if (a->par.major_printmode == GWY_AXIS_SCALE_FORMAT_AUTO) {
        format = gwy_axis_calculate_format(a, format);
        if (a->magnification_string)
            g_string_assign(a->magnification_string, format->units);
        else
            a->magnification_string = g_string_new(format->units);
        a->magnification = format->magnitude;
    }
    else {
        if (a->magnification_string)
            g_string_free(a->magnification_string, TRUE);
        a->magnification_string = NULL;
        a->magnification = 1;
    }

    context = gdk_pango_context_get_for_screen(gdk_screen_get_default());
    layout = pango_layout_new(context);
    pango_layout_set_font_description(layout, a->par.major_font);

    if (a->is_logarithmic) {
        pmjt = &g_array_index(a->mjticks, GwyAxisLabeledTick, 0);
        if (pmjt->t.value < -4 || pmjt->t.value > 3)
            human_fmt = FALSE;
        pmjt = &g_array_index(a->mjticks, GwyAxisLabeledTick,
                              a->mjticks->len-1);
        if (pmjt->t.value < -4 || pmjt->t.value > 3)
            human_fmt = FALSE;
    }

    for (i = 0; i < a->mjticks->len; i++) {
        /* Find the value we want to put in string */
        pmjt = &g_array_index(a->mjticks, GwyAxisLabeledTick, i);
        value = pmjt->t.value;
        if (format)
            value /= format->magnitude;

        /* Fill tick labels dependent to mode */
        if (a->is_logarithmic) {
            if (human_fmt)
                g_string_printf(pmjt->ttext, "%g", pow10(value));
            else
                g_string_printf(pmjt->ttext, "10<sup>%d</sup>",
                                GWY_ROUND(value));
        }
        else {
            if (a->par.major_printmode == GWY_AXIS_SCALE_FORMAT_AUTO) {
                g_string_printf(pmjt->ttext, "%.*f", format->precision, value);
            }
            else if (a->par.major_printmode == GWY_AXIS_SCALE_FORMAT_EXP) {
                if (value == 0)
                    g_string_printf(pmjt->ttext, "0");
                else
                    g_string_printf(pmjt->ttext, "%.1e", value);
            }
            else if (a->par.major_printmode == GWY_AXIS_SCALE_FORMAT_INT) {
                g_string_printf(pmjt->ttext, "%d", (int)(value+0.5));
            }
        }

        pango_layout_set_markup(layout, pmjt->ttext->str, pmjt->ttext->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);
        totalwidth += rect.width;
        totalheight += rect.height;
    }

    if (format)
        gwy_si_unit_value_format_free(format);
    g_object_unref(layout);
    g_object_unref(context);

    /* Guess whether we dont have too many or not enough ticks */
    if (a->orientation == GTK_POS_RIGHT
        || a->orientation == GTK_POS_LEFT) {
        if (totalheight > 200)
            return 1;
        else if (a->mjticks->len < 3)
            return -1;
    }
    else {
        if (totalwidth > 200)
            return 1;
        else if (a->mjticks->len < 3)
            return -1;
    }

    return 0;
}

static GwySIValueFormat*
gwy_axis_calculate_format(GwyAxis *axis,
                          GwySIValueFormat *format)
{
    GwyAxisLabeledTick *pmjt, *mji, *mjx;
    gdouble average, step;
    GString *u1, *u2;
    gboolean ok;
    guint i;

    /* FIXME: Does anyone really care? */
    if (axis->is_logarithmic)
        return gwy_si_unit_get_format(axis->unit, GWY_SI_UNIT_FORMAT_MARKUP,
                                      0, format);

    mji = &g_array_index(axis->mjticks, GwyAxisLabeledTick, 0);
    mjx = &g_array_index(axis->mjticks, GwyAxisLabeledTick,
                         axis->mjticks->len - 1);
    average = fabs(mjx->t.value + mji->t.value)/2;
    step = fabs(mjx->t.value - mji->t.value);
    average = MAX(average, step);
    step /= MAX(axis->mjticks->len - 1, 1);

    format = gwy_si_unit_get_format_with_resolution
                                        (axis->unit, GWY_SI_UNIT_FORMAT_MARKUP,
                                         average, step, format);

    /* Ensure the format is not too precise */
    format->precision++;
    u1 = g_string_new(NULL);
    u2 = g_string_new(NULL);
    ok = TRUE;
    while (ok && format->precision > 0 && axis->mjticks->len > 1) {
        format->precision--;
        ok = TRUE;
        pmjt = &g_array_index(axis->mjticks, GwyAxisLabeledTick, 0);
        g_string_printf(u1, "%.*f",
                        format->precision, pmjt->t.value/format->magnitude);
        for (i = 1; i < MIN(axis->mjticks->len, 6); i++) {
            pmjt = &g_array_index(axis->mjticks, GwyAxisLabeledTick, i);
            g_string_printf(u2, "%.*f",
                            format->precision, pmjt->t.value/format->magnitude);
            if (gwy_strequal(u2->str, u1->str)) {
                format->precision++;
                ok = FALSE;
                break;
            }
            GWY_SWAP(GString*, u2, u1);
        }
    }

    g_string_free(u1, TRUE);
    g_string_free(u2, TRUE);

    return format;
}

/**
 * gwy_axis_set_visible:
 * @axis: An axis.
 * @is_visible: visibility
 *
 * Sets the visibility of an axis.
 **/
void
gwy_axis_set_visible(GwyAxis *axis, gboolean is_visible)
{
    axis->is_visible = is_visible;
    gwy_axis_adjust(axis, -1, -1);
    //gtk_widget_queue_resize(GTK_WIDGET(axis));
}

/**
 * gwy_axis_set_auto:
 * @axis: An axis.
 * @is_auto: %TRUE to enable automatic tick size and distribution adjustment,
 *           %FALSE to disable it.
 *
 * Enables or disables automatic axis adjustmet.
 **/
void
gwy_axis_set_auto(GwyAxis *axis, gboolean is_auto)
{
    axis->is_auto = is_auto;
    gwy_axis_adjust(axis, -1, -1);
}

/**
 * gwy_axis_request_range:
 * @axis: An axis.
 * @min: Minimum requisition (min boundary value).
 * @max: Maximum requisition (max boundary value).
 *
 * Sets the requisition of axis boundaries.
 *
 * The axis will adjust the boundaries to satisfy requisition but still have
 * reasonable tick values and spacing.  Use gwy_axis_get_range() to obtain the
 * boundaries the axis actually decided to use.
 **/
void
gwy_axis_request_range(GwyAxis *axis, gdouble min, gdouble max)
{
    g_return_if_fail(GWY_IS_AXIS(axis));

    if (axis->reqmin == min && axis->reqmax == max)
        return;

    axis->reqmin = min;
    axis->reqmax = max;

    gwy_axis_adjust(axis, -1, -1);
    if (GTK_WIDGET_REALIZED(axis))
        gtk_widget_queue_resize(GTK_WIDGET(axis));
}

/**
 * gwy_axis_get_range:
 * @axis: An axis.
 * @min: Location to store actual axis minimum, or %NULL.
 * @max: Location to store actual axis maximum, or %NULL.
 *
 * Gets the actual boundaries of an axis.
 **/
void
gwy_axis_get_range(GwyAxis *axis,
                   gdouble *min,
                   gdouble *max)
{
    g_return_if_fail(GWY_IS_AXIS(axis));

    if (min)
        *min = axis->min;
    if (max)
        *max = axis->max;
}

/**
 * gwy_axis_get_requested_range:
 * @axis: An axis.
 * @min: Location to store requested axis minimum, or %NULL.
 * @max: Location to store requested axis maximum, or %NULL.
 *
 * Gets the requested boundaries of an axis.
 **/
void
gwy_axis_get_requested_range(GwyAxis *axis,
                             gdouble *min,
                             gdouble *max)
{
    g_return_if_fail(GWY_IS_AXIS(axis));

    if (min)
        *min = axis->reqmin;
    if (max)
        *max = axis->reqmax;
}

/**
 * gwy_axis_set_label:
 * @axis: An axis.
 * @label: The new label text (it can be %NULL for an empty label).
 *
 * Sets the label text of an axis.
 **/
void
gwy_axis_set_label(GwyAxis *axis,
                   const gchar *label)
{
    if (!label)
        label = "";

    gwy_debug("label_text = <%s>", label);
    if (gwy_strequal(label, axis->label_text->str))
        return;

    g_string_assign(axis->label_text, label);
    if (axis->dialog) {
        GwyAxisDialog *dialog = GWY_AXIS_DIALOG(axis->dialog);

        gwy_sci_text_set_text(GWY_SCI_TEXT(dialog->sci_text), label);
    }
    g_object_notify(G_OBJECT(axis), "label");
}

/**
 * gwy_axis_get_label:
 * @axis: An axis.
 *
 * Gets the label of an axis.
 *
 * Returns: Axis label as a string owned by @axis.
 **/
const gchar*
gwy_axis_get_label(GwyAxis *axis)
{
    return axis->label_text->str;
}
/**
 * gwy_axis_set_si_unit:
 * @axis: An axis.
 * @unit: axis unit
 *
 * Sets the axis unit. This will be added automatically
 * to the label. @unit is duplicated.
 **/
void
gwy_axis_set_si_unit(GwyAxis *axis, GwySIUnit *unit)
{
    if (axis->unit && gwy_si_unit_equal(axis->unit, unit))
        return;

    gwy_serializable_clone(G_OBJECT(unit), G_OBJECT(axis->unit));
    if (GTK_WIDGET_DRAWABLE(axis))
        gtk_widget_queue_draw(GTK_WIDGET(axis));
}

/**
 * gwy_axis_enable_label_edit:
 * @axis: Axis widget
 * @enable: enable/disable user to change axis label
 *
 * Enables/disables user to change axis label by clicking on axis widget.
 **/
void
gwy_axis_enable_label_edit(GwyAxis *axis, gboolean enable)
{
    axis->enable_label_edit = enable;
}

/**
 * gwy_axis_get_magnification:
 * @axis: Axis widget
 *
 * Returns: Magnification value of the axis
 **/
gdouble
gwy_axis_get_magnification(GwyAxis *axis)
{
    return axis->magnification;
}

/**
 * gwy_axis_get_magnification_string:
 * @axis: An axis.
 *
 * Gets the magnification string of an axis.
 *
 * Returns: Magnification string of the axis, owned by the axis.
 **/
const gchar*
gwy_axis_get_magnification_string(GwyAxis *axis)
{
    if (axis->magnification_string)
        return axis->magnification_string->str;
    else
        return "";
}

/**
 * gwy_axis_export_vector:
 * @axis: An axis.
 * @xmin:
 * @ymin:
 * @width: width of the x-axis
 * @height: hieght of the y-axis
 * @fontsize:
 *
 *
 **/
GString*
gwy_axis_export_vector(GwyAxis *axis, gint xmin, gint ymin,
                       gint width, gint height, gint fontsize)
{
    GString *out;
    gdouble mult;
    GwyAxisLabeledTick *pmjt;
    GwyAxisTick *pmit;
    GString *plotlabel;
    gchar *units;
    gint i;
    gint linewidth = 2;
    gint ticklinewidth = 1;

    if (axis->orientation == GTK_POS_TOP || axis->orientation == GTK_POS_BOTTOM)
        mult = (gdouble)width/(axis->max - axis->min);
    else
        mult = (gdouble)height/(axis->max - axis->min);

    out = g_string_new("%%Axis\n");

    g_string_append_printf(out, "/Times-Roman findfont\n");
    g_string_append_printf(out, "%d scalefont\n setfont\n", fontsize);
    g_string_append_printf(out, "%d setlinewidth\n", linewidth);

    /*draw axis*/
    switch (axis->orientation) {
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
    g_string_append_printf(out, "%d setlinewidth\n", ticklinewidth);
    for (i = 0; i < axis->mjticks->len; i++) {
        pmjt = &g_array_index(axis->mjticks, GwyAxisLabeledTick, i);

        switch (axis->orientation) {
            case GTK_POS_TOP:
            g_string_append_printf(out, "%d %d M\n",
                                   (gint)(xmin + pmjt->t.value*mult
                                          - axis->min*mult + 0.5),
                                   ymin);
            g_string_append_printf(out, "%d %d L\n",
                                   (gint)(xmin + pmjt->t.value*mult
                                          - axis->min*mult + 0.5),
                                   ymin+axis->par.major_length);
            g_string_append_printf(out, "%d %d R\n",
                                   -(gint)(pmjt->ttext->len*(fontsize/4)),
                                   5);
            g_string_append_printf(out, "(%s) show\n", pmjt->ttext->str);
            break;

            case GTK_POS_BOTTOM:
            g_string_append_printf(out, "%d %d M\n",
                                   (gint)(xmin + pmjt->t.value*mult
                                          - axis->min*mult + 0.5),
                                   ymin + height);
            g_string_append_printf(out, "%d %d L\n",
                                   (gint)(xmin + pmjt->t.value*mult
                                          - axis->min*mult + 0.5),
                                   ymin + height - axis->par.major_length);
            g_string_append_printf(out, "%d %d R\n",
                                   -(gint)(pmjt->ttext->len*(fontsize/4)),
                                   -fontsize);
            g_string_append_printf(out, "(%s) show\n", pmjt->ttext->str);
            break;

            case GTK_POS_LEFT:
            g_string_append_printf(out, "%d %d M\n",
                                   xmin + width,
                                   (gint)(ymin + pmjt->t.value*mult
                                          - axis->min*mult + 0.5));
            g_string_append_printf(out, "%d %d L\n",
                                   xmin + width - axis->par.major_length,
                                   (gint)(ymin + pmjt->t.value*mult
                                          - axis->min*mult + 0.5));
            g_string_append_printf(out, "%d %d R\n",
                                   -(gint)(pmjt->ttext->len*(fontsize/2)) - 5,
                                   -(gint)(fontsize/2.5));
            g_string_append_printf(out, "(%s) show\n", pmjt->ttext->str);
            break;

            case GTK_POS_RIGHT:
            g_string_append_printf(out, "%d %d M\n",
                                   xmin,
                                   (gint)(ymin + pmjt->t.value*mult
                                          - axis->min*mult + 0.5));
            g_string_append_printf(out, "%d %d L\n",
                                   xmin + axis->par.major_length,
                                   (gint)(ymin + pmjt->t.value*mult
                                          - axis->min*mult + 0.5));
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
    for (i = 0; i < axis->miticks->len; i++) {
        pmit = &g_array_index (axis->miticks, GwyAxisTick, i);

        switch (axis->orientation) {
            case GTK_POS_TOP:
            g_string_append_printf(out, "%d %d M\n",
                                   (gint)(xmin + pmit->value*mult
                                          - axis->min*mult + 0.5),
                                   ymin);
            g_string_append_printf(out, "%d %d L\n",
                                   (gint)(xmin + pmit->value*mult
                                          - axis->min*mult + 0.5),
                                   ymin+axis->par.minor_length);
            break;

            case GTK_POS_BOTTOM:
            g_string_append_printf(out, "%d %d M\n",
                                   (gint)(xmin + pmit->value*mult
                                          - axis->min*mult + 0.5),
                                   ymin + height);
            g_string_append_printf(out, "%d %d L\n",
                                   (gint)(xmin + pmit->value*mult
                                          - axis->min*mult + 0.5),
                                   ymin + height - axis->par.minor_length);
            break;

            case GTK_POS_LEFT:
            g_string_append_printf(out, "%d %d M\n",
                                   xmin + width,
                                   (gint)(ymin + pmit->value*mult
                                          - axis->min*mult + 0.5));
            g_string_append_printf(out, "%d %d L\n",
                                   xmin + width - axis->par.minor_length,
                                   (gint)(ymin + pmit->value*mult
                                          - axis->min*mult + 0.5));
            break;

            case GTK_POS_RIGHT:
            g_string_append_printf(out, "%d %d M\n",
                                   xmin,
                                   (gint)(ymin + pmit->value*mult
                                          - axis->min*mult + 0.5));
            g_string_append_printf(out, "%d %d L\n",
                                   xmin +  axis->par.minor_length,
                                   (gint)(ymin + pmit->value*mult
                                          - axis->min*mult + 0.5));
            break;

            default:
            g_assert_not_reached();
            break;
        }
        g_string_append_printf(out, "stroke\n");
  }

    g_string_append_printf(out, "%%AxisLabel\n");

    plotlabel = g_string_new(axis->label_text->str);
    units = gwy_si_unit_get_string(axis->unit, GWY_SI_UNIT_FORMAT_MARKUP);
    if (axis->magnification_string->len > 0 || *units) {
        g_string_append(plotlabel, " [");
        if (axis->magnification_string)
            g_string_append(plotlabel, axis->magnification_string->str);
        else
            g_string_append(plotlabel, units);
        g_string_append(plotlabel, "]");
    }
    g_free(units);

    switch (axis->orientation) {
        case GTK_POS_TOP:
        g_string_append_printf(out, "%d %d M\n",
                               (gint)(xmin + width/2
                                      - plotlabel->len*fontsize/4),
                               ymin + height - 20);
        g_string_append_printf(out, "(%s) show\n", plotlabel->str);
        break;

        case GTK_POS_BOTTOM:
        g_string_append_printf(out, "%d %d M\n",
                               (gint)(xmin + width/2
                                      - plotlabel->len*fontsize/4),
                               ymin + 20);
        g_string_append_printf(out, "(%s) show\n", plotlabel->str);
        break;

        case GTK_POS_LEFT:
        g_string_append_printf(out, "%d %d M\n",
                               (gint)(xmin + fontsize/2 + 8),
                               (gint)(ymin + height/2
                                      - plotlabel->len*fontsize/4));
        g_string_append_printf(out, "gsave\n");
        g_string_append_printf(out, "90 rotate\n");
        g_string_append_printf(out, "(%s) show\n", plotlabel->str);
        g_string_append_printf(out, "grestore\n");
        break;

        case GTK_POS_RIGHT:
        g_string_append_printf(out, "%d %d M\n",
                               (gint)(xmin + width - fontsize/2 - 8),
                               (gint)(ymin + height/2
                                      + plotlabel->len*fontsize/4));
        g_string_append_printf(out, "gsave\n");
        g_string_append_printf(out, "270 rotate\n");
        g_string_append_printf(out, "(%s) show\n", plotlabel->str);
        g_string_append_printf(out, "grestore\n");
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return out;
}

/**
 * gwy_axis_get_major_ticks:
 * @axis: An axis.
 * @nticks: Location to store the number of returned ticks.
 *
 * Gets the positions of major ticks of an axis.
 *
 * Returns: The positions of axis major ticks (as real values, not pixels).
 *          The returned array is owned by the axis.
 **/
const gdouble*
gwy_axis_get_major_ticks(GwyAxis *axis,
                         guint *nticks)
{
    GwyAxisLabeledTick *pmji;
    gdouble *pvalue;
    guint i;

    if (!axis->mjpubticks)
        axis->mjpubticks = g_array_sized_new(FALSE, FALSE, sizeof(gdouble),
                                             axis->mjticks->len);
    else
        g_array_set_size(axis->mjpubticks, axis->mjticks->len);

    for (i = 0; i < axis->mjticks->len; i++) {
        pmji = &g_array_index(axis->mjticks, GwyAxisLabeledTick, i);
        pvalue = &g_array_index(axis->mjpubticks, gdouble, i);
        if (!axis->is_logarithmic)
            *pvalue = pmji->t.value;
        else
            *pvalue = gwy_axis_dbl_raise(10.0, pmji->t.value);
    }

    if (nticks)
        *nticks = axis->mjpubticks->len;

    return (const gdouble*)axis->mjpubticks->data;
}

/**
 * gwy_axis_is_visible:
 * @axis: An axis.
 *
 * Determines whether axis is set to be visible.
 *
 * Return: %TRUE if @axis is set to be visible.
 **/
gboolean
gwy_axis_is_visible(GwyAxis *axis)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), FALSE);

    return axis->is_visible;
}

/**
 * gwy_axis_is_logarithmic:
 * @axis: An axis.
 *
 * Determines whether axis is set to be locarithmic.
 *
 * Returns: %TRUE if @axis is logarithmic.
 **/
gboolean
gwy_axis_is_logarithmic(GwyAxis *axis)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), FALSE);

    return axis->is_logarithmic;
}

/**
 * gwy_axis_get_orientation:
 * @axis: An axis.
 *
 * Gets the orientation of an axis.
 *
 * Returns: The orientation.
 **/
GtkPositionType
gwy_axis_get_orientation(GwyAxis *axis)
{
    g_return_val_if_fail(GWY_IS_AXIS(axis), 0);

    return axis->orientation;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyaxis
 * @title: GwyAxis
 * @short_description: Axis with ticks and labels
 * @see_also: #GwyColorAxis -- Axis with a false color scale,
 *            #GwyRuler -- Horizontal and vertical rulers
 *
 * #GwyAxis is used for drawing axis. It is namely used within #GwyGraph
 * widget, but it can be also used standalone. It plots a horizontal or
 * vertical axis with major and minor ticks, with ranges in the requested
 * interval.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
