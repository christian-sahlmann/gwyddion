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
#include <libgwyddion/gwymacros.h>
#include "gwyaxis.h"

#define GWY_AXIS_TYPE_NAME "GwyAxis"

enum {
    LABEL_UPDATED,
    LAST_SIGNAL
};

/* Forward declarations - widget related*/
static void     gwy_axis_class_init           (GwyAxisClass *klass);
static void     gwy_axis_init                 (GwyAxis *axis);
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

/* Forward declarations - axis related*/
static gdouble  gwy_axis_dbl_raise            (gdouble x, gint y);
static gdouble  gwy_axis_quantize_normal_tics (gdouble arg, gint guide);
static gint     gwy_axis_normalscale          (GwyAxis *a);
static gint     gwy_axis_logscale             (GwyAxis *a);
static gint     gwy_axis_scale                (GwyAxis *a);
static gint     gwy_axis_formatticks          (GwyAxis *a);
static gint     gwy_axis_precompute           (GwyAxis *a,
                                               gint scrmin,
                                               gint scrmax);
static void     gwy_axis_draw_axis            (GtkWidget *widget);
static void     gwy_axis_draw_ticks           (GtkWidget *widget);
static void     gwy_axis_draw_tlabels         (GtkWidget *widget);
static void     gwy_axis_draw_label           (GtkWidget *widget);
static void     gwy_axis_autoset              (GwyAxis *axis,
                                               gint width,
                                               gint height);
static void     gwy_axis_adjust               (GwyAxis *axis,
                                               gint width,
                                               gint height);
static void     gwy_axis_entry                (GwyAxisDialog *dialog,
                                               gint arg1,
                                               gpointer user_data);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

static guint axis_signals[LAST_SIGNAL] = { 0 };

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

    axis_signals[LABEL_UPDATED] =
        g_signal_new("label_updated",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyAxisClass, label_updated),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void
gwy_axis_init(GwyAxis *axis)
{
    gwy_debug("");

    axis->is_visible = 1;
    axis->is_logarithmic = 0;
    axis->is_auto = 1;
    axis->is_standalone = 0;
    /*axis->orientation = GTK_POS_TOP;*/
    axis->max = 0;
    axis->min = 0;
    /*axis->mjticks = NULL;*/
    /*axis->miticks = NULL;*/
    axis->label_x_pos = 0;
    axis->label_y_pos = 0;
    /*axis->label_text = NULL;*/
    axis->par.major_printmode = GWY_AXIS_SCALE_FORMAT_AUTO;

    axis->par.major_length = 10;
    axis->par.major_thickness = 1;
    axis->par.major_maxticks = 20;

    axis->par.minor_length = 5;
    axis->par.minor_thickness = 1;
    axis->par.minor_division = 10;
    axis->par.line_thickness = 1;

    /*axis->dialog = NULL;*/
    axis->reqmax = 100;
    axis->reqmin = 0;

    axis->enable_label_edit = TRUE;

    axis->has_unit = 0;
    /*axis->unit = NULL;*/
}

/**
 * gwy_axis_new:
 * @orientation:  axis orientation
 * @min: minimum value
 * @max: maximum value
 * @label: axis label
 *
 * Creates new axis.
 *
 * Returns: new axis
 **/
GtkWidget*
gwy_axis_new(gint orientation, gdouble min, gdouble max, const gchar *label)
{
    GwyAxis *axis;

    gwy_debug("");

    axis = GWY_AXIS(g_object_new(GWY_TYPE_AXIS, NULL));
    axis->reqmin = min;
    axis->reqmax = max;
    axis->orientation = orientation;

    axis->label_text = g_string_new(label);
    axis->mjticks = g_array_new(FALSE, FALSE, sizeof(GwyLabeledTick));
    axis->miticks = g_array_new(FALSE, FALSE, sizeof(GwyTick));

    axis->label_x_pos = 20;
    axis->label_y_pos = 20;

    axis->par.major_font = pango_font_description_new();
    pango_font_description_set_family(axis->par.major_font, "Helvetica");
    pango_font_description_set_style(axis->par.major_font, PANGO_STYLE_NORMAL);
    pango_font_description_set_variant(axis->par.major_font, PANGO_VARIANT_NORMAL);
    pango_font_description_set_weight(axis->par.major_font, PANGO_WEIGHT_NORMAL);
    pango_font_description_set_size(axis->par.major_font, 10*PANGO_SCALE);

    axis->par.label_font = pango_font_description_new();
    pango_font_description_set_family(axis->par.label_font, "Helvetica");
    pango_font_description_set_style(axis->par.label_font, PANGO_STYLE_NORMAL);
    pango_font_description_set_variant(axis->par.label_font, PANGO_VARIANT_NORMAL);
    pango_font_description_set_weight(axis->par.label_font, PANGO_WEIGHT_NORMAL);
    pango_font_description_set_size(axis->par.label_font, 12*PANGO_SCALE);

    axis->dialog = gwy_axis_dialog_new();

    /* FIXME: emits a spurious label_updated? */
    g_signal_connect(axis->dialog, "response",
                     G_CALLBACK(gwy_axis_entry), axis);
    gwy_sci_text_set_text(GWY_SCI_TEXT(GWY_AXIS_DIALOG(axis->dialog)->sci_text),
                          label);

    return GTK_WIDGET(axis);
}

static void
gwy_axis_finalize(GObject *object)
{
    GwyAxis *axis;

    gwy_debug("finalizing a GwyAxis (refcount = %u)", object->ref_count);

    g_return_if_fail(GWY_IS_AXIS(object));

    axis = GWY_AXIS(object);

    g_string_free(axis->label_text, TRUE);
    g_array_free(axis->mjticks, FALSE);
    g_array_free(axis->miticks, FALSE);

    gtk_widget_destroy(axis->dialog);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_axis_unrealize(GtkWidget *widget)
{
    GwyAxis *axis;

    axis = GWY_AXIS(widget);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static void
gwy_axis_realize(GtkWidget *widget)
{
    GwyAxis *axis;
    GdkWindowAttr attributes;
    gint attributes_mask;
    GtkStyle *s;

    gwy_debug("realizing a GwyAxis (%ux%u)",
              widget->allocation.x, widget->allocation.height);

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
    gwy_axis_adjust(axis, widget->allocation.width, widget->allocation.height);
}

static void
gwy_axis_size_request(GtkWidget *widget,
                             GtkRequisition *requisition)
{
    GwyAxis *axis;
    gwy_debug("");

    axis = GWY_AXIS(widget);

    if (axis->orientation == GTK_POS_LEFT
        || axis->orientation == GTK_POS_RIGHT) {
        requisition->width = 80;
        requisition->height = 100;
    }
    else {
        requisition->width = 100;
        requisition->height = 80;
    }

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
    if (GTK_WIDGET_REALIZED(widget)) {

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
    }
    gwy_axis_adjust(axis, allocation->width, allocation->height);

}

static void
gwy_axis_adjust(GwyAxis *axis, gint width, gint height)
{

    if (axis->orientation == GTK_POS_TOP
        || axis->orientation == GTK_POS_BOTTOM) {
        axis->label_x_pos = width/2;
        if (axis->orientation == GTK_POS_TOP)
            axis->label_y_pos = 40;
        else
            axis->label_y_pos = height - 50;
    }
    if (axis->orientation == GTK_POS_LEFT
        || axis->orientation == GTK_POS_RIGHT) {
        axis->label_y_pos = height/2;
        if (axis->orientation == GTK_POS_LEFT)
            axis->label_x_pos = 40;
        else
            axis->label_x_pos = width - 40;
    }


    if (axis->is_auto)
        gwy_axis_autoset(axis, width, height);
    gwy_axis_scale(axis);

    if (axis->orientation == GTK_POS_TOP
        || axis->orientation == GTK_POS_BOTTOM)
        gwy_axis_precompute(axis, 0, width);
    else
        gwy_axis_precompute(axis, 0, height);


}

static void
gwy_axis_autoset(GwyAxis *axis, gint width, gint height)
{
    if (axis->orientation == GTK_POS_TOP
        || axis->orientation == GTK_POS_BOTTOM) {

        if (width < 150) {
            axis->par.major_thickness = 1;
            axis->par.major_maxticks = 5;
            axis->par.minor_division = 5;
        }
        else if (width < 600) {
            axis->par.major_thickness = 1;
            axis->par.major_maxticks = 7;
            axis->par.minor_division = 10;
        }
        else {
            axis->par.major_thickness = 1;
            axis->par.major_maxticks = 18;
            axis->par.minor_division = 10;
        }
    }
    if (axis->orientation == GTK_POS_LEFT
        || axis->orientation == GTK_POS_RIGHT) {

        if (height < 150) {
            axis->par.major_thickness = 1;
            axis->par.major_maxticks = 10;
            axis->par.minor_division = 5;
        }
        else if (height < 600) {
            axis->par.major_thickness = 1;
            axis->par.major_maxticks = 20;
            axis->par.minor_division = 10;
        }
        else {
            axis->par.major_thickness = 1;
            axis->par.major_maxticks = 25;
            axis->par.minor_division = 10;
        }
    }


}

/**
 * gwy_axis_set_logarithmic:
 * @axis: axis
 * @is_logarithmic: logarithimc mode
 *
 * Sets logarithmic mode. Untested.
 **/
void
gwy_axis_set_logarithmic(GwyAxis *axis,
                         gboolean is_logarithmic)
{
    axis->is_logarithmic = is_logarithmic;
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

    if (axis->is_standalone && axis->is_visible)
        gwy_axis_draw_axis(widget);
    if (axis->is_visible) gwy_axis_draw_ticks(widget);
    if (axis->is_visible) gwy_axis_draw_tlabels(widget);
    if (axis->is_visible) gwy_axis_draw_label(widget);

    return FALSE;
}

static void
gwy_axis_draw_axis(GtkWidget *widget)
{
    GwyAxis *axis;
    GdkGC *mygc;

    axis = GWY_AXIS(widget);
    mygc = gdk_gc_new(widget->window);
    gdk_gc_set_line_attributes (mygc, axis->par.line_thickness,
                                GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    switch (axis->orientation) {
        case GTK_POS_TOP:
        gdk_draw_line(widget->window, mygc,
                      0, 0,
                      widget->allocation.width-1, 0);
        break;

        case GTK_POS_BOTTOM:
        gdk_draw_line(widget->window, mygc,
                      0, widget->allocation.height-1,
                      widget->allocation.width-1, widget->allocation.height-1);
        break;

        case GTK_POS_LEFT:
        gdk_draw_line(widget->window, mygc,
                      0, 0,
                      0, widget->allocation.height-1);
        break;

        case GTK_POS_RIGHT:
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
gwy_axis_draw_ticks(GtkWidget *widget)
{
    guint i;
    GwyAxis *axis;
    GwyTick *pmit;
    GdkGC *mygc;
    GwyLabeledTick *pmjt;

    axis = GWY_AXIS(widget);

    mygc = gdk_gc_new(widget->window);

    gdk_gc_set_line_attributes (mygc, axis->par.major_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    for (i = 0; i < axis->mjticks->len; i++) {
        pmjt = &g_array_index (axis->mjticks, GwyLabeledTick, i);

        switch (axis->orientation) {
            case GTK_POS_TOP:
            gdk_draw_line(widget->window, mygc,
                          pmjt->t.scrpos,
                          0,
                          pmjt->t.scrpos,
                          axis->par.major_length);
            break;

            case GTK_POS_BOTTOM:
            gdk_draw_line(widget->window, mygc,
                          pmjt->t.scrpos,
                          widget->allocation.height-1,
                          pmjt->t.scrpos,
                          widget->allocation.height-1 - axis->par.major_length);
            break;

            case GTK_POS_LEFT:
            gdk_draw_line(widget->window, mygc,
                          0,
                          widget->allocation.height-1 - pmjt->t.scrpos,
                          axis->par.major_length,
                          widget->allocation.height-1 - pmjt->t.scrpos);
            break;

            case GTK_POS_RIGHT:
            gdk_draw_line(widget->window, mygc,
                          widget->allocation.width-1,
                          widget->allocation.height-1 - pmjt->t.scrpos,
                          widget->allocation.width-1 - axis->par.major_length,
                          widget->allocation.height-1 - pmjt->t.scrpos);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }

    gdk_gc_set_line_attributes(mygc, axis->par.minor_thickness,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

    for (i = 0; i < axis->miticks->len; i++) {
        pmit = &g_array_index (axis->miticks, GwyTick, i);

        switch (axis->orientation) {
            case GTK_POS_TOP:
            gdk_draw_line(widget->window, mygc,
                          pmit->scrpos,
                          0,
                          pmit->scrpos,
                          axis->par.minor_length);
            break;

            case GTK_POS_BOTTOM:
            gdk_draw_line(widget->window, mygc,
                          pmit->scrpos,
                          widget->allocation.height-1,
                          pmit->scrpos,
                          widget->allocation.height-1 - axis->par.minor_length);
            break;

            case GTK_POS_LEFT:
            gdk_draw_line(widget->window, mygc,
                          0,
                          widget->allocation.height-1 - pmit->scrpos,
                          axis->par.minor_length,
                          widget->allocation.height-1 - pmit->scrpos);
            break;

            case GTK_POS_RIGHT:
            gdk_draw_line(widget->window, mygc,
                          widget->allocation.width-1,
                          widget->allocation.height-1 - pmit->scrpos,
                          widget->allocation.width-1 - axis->par.minor_length,
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
gwy_axis_draw_tlabels(GtkWidget *widget)
{
    guint i;
    GwyAxis *axis;
    GwyLabeledTick *pmjt;
    PangoLayout *layout;
    PangoRectangle rect;
    GdkGC *mygc;
    gint sep, xpos = 0, ypos = 0;

    mygc = gdk_gc_new(widget->window);

    axis = GWY_AXIS(widget);
    layout = gtk_widget_create_pango_layout(widget, "");
    pango_layout_set_font_description(layout, axis->par.major_font);

    sep = 5;

    for (i = 0; i < axis->mjticks->len; i++) {
        pmjt = &g_array_index(axis->mjticks, GwyLabeledTick, i);
        pango_layout_set_text(layout,  pmjt->ttext->str, pmjt->ttext->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);

        switch (axis->orientation) {
            case GTK_POS_TOP:
            xpos = pmjt->t.scrpos - rect.width/2;
            ypos = axis->par.major_length + sep;
            break;

            case GTK_POS_BOTTOM:
            xpos = pmjt->t.scrpos - rect.width/2;
            ypos = widget->allocation.height-1
                   - axis->par.major_length - sep - rect.height;
            break;

            case GTK_POS_LEFT:
            xpos = axis->par.major_length + sep;
            ypos = widget->allocation.height-1 - pmjt->t.scrpos - rect.height/2;
            break;

            case GTK_POS_RIGHT:
            xpos = widget->allocation.width-1
                   - axis->par.major_length - sep - rect.width;
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
gwy_axis_draw_label(GtkWidget *widget)
{
    GwyAxis *axis;
    PangoLayout *layout;
    GdkGC *mygc;
    PangoRectangle rect;
    GString *plotlabel;

    mygc = gdk_gc_new(widget->window);

    axis = GWY_AXIS(widget);
    layout = gtk_widget_create_pango_layout(widget, "");
    pango_layout_set_font_description(layout, axis->par.major_font);

    plotlabel = g_string_new(axis->label_text->str);

    if (axis->has_unit) {
        g_string_append(plotlabel, " [");
        g_string_append(plotlabel, axis->unit);
        g_string_append(plotlabel, "]");
    }

    pango_layout_set_markup(layout,  plotlabel->str, plotlabel->len);
    pango_layout_get_pixel_extents(layout, NULL, &rect);

    switch (axis->orientation) {
        case GTK_POS_TOP:
        gdk_draw_layout(widget->window, mygc,
                        axis->label_x_pos - rect.width/2, axis->label_y_pos,
                        layout);
        break;

        case GTK_POS_BOTTOM:
        gdk_draw_layout(widget->window, mygc,
                        axis->label_x_pos - rect.width/2,
                        axis->label_y_pos,
                        layout);
        break;

        case GTK_POS_LEFT:
        gdk_draw_layout(widget->window, mygc,
                        axis->label_x_pos,
                        axis->label_y_pos,
                        layout);
        break;

        case GTK_POS_RIGHT:
        gdk_draw_layout(widget->window, mygc,
                        axis->label_x_pos - rect.width,
                        axis->label_y_pos,
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
gwy_axis_button_press(GtkWidget *widget,
                      GdkEventButton *event)
{
    GwyAxis *axis;

    gwy_debug("");
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_AXIS(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    axis = GWY_AXIS(widget);

    if (axis->enable_label_edit)
        gtk_widget_show_all(axis->dialog);

    return FALSE;
}

static gboolean
gwy_axis_button_release(GtkWidget *widget,
                        GdkEventButton *event)
{
    GwyAxis *axis;

    gwy_debug("");
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_AXIS(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    axis = GWY_AXIS(widget);

    return FALSE;
}

static void
gwy_axis_entry(GwyAxisDialog *dialog, gint arg1, gpointer user_data)
{
    GwyAxis *axis;
    GdkRectangle rec;

    gwy_debug("");

    axis = GWY_AXIS(user_data);
    g_assert(GWY_IS_AXIS(axis));

    rec.x = GTK_WIDGET(axis)->allocation.x;
    rec.y = GTK_WIDGET(axis)->allocation.y;
    rec.width = GTK_WIDGET(axis)->allocation.width;
    rec.height = GTK_WIDGET(axis)->allocation.height;

    if (arg1 == GTK_RESPONSE_APPLY) {
        gchar *text;

        text = gwy_sci_text_get_text(GWY_SCI_TEXT(dialog->sci_text));
        g_string_assign(axis->label_text, text);
        g_free(text);
        g_signal_emit(axis, axis_signals[LABEL_UPDATED], 0);
        gtk_widget_queue_draw(GTK_WIDGET(axis));
    }
    else if (arg1 == GTK_RESPONSE_CLOSE) {
        gtk_widget_hide(GTK_WIDGET(dialog));
    }
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
    GwyTick mit;
    GwyLabeledTick mjt;
    gdouble range, tickstep, majorbase, minortickstep, minorbase;

    if (a->reqmax == a->reqmin) {g_warning("Axis with zero range!"); a->reqmax = a->reqmin+1;}

    /*printf("reqmin=%f, reqmax=%f\n", a->reqmin, a->reqmax);*/
    range = fabs(a->reqmax - a->reqmin); /*total range of the field*/

    if (range > 1e40 || range < -1e40)
    {
        g_warning("Axis with extreme range (>1e40)!");
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
/*printf("majorbase=%f, tickstep=%f, reqmax=%f\n", majorbase, tickstep, a->reqmax);*/
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
    GwyLabeledTick mjt;
    GwyTick mit;

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
    GwyLabeledTick *mjt;


    /*never use logarithmic mode for negative numbers*/
    if (a->min < 0 && a->is_logarithmic == TRUE)
        return 1; /*this is an error*/

    /*remove old ticks*/
    for (i = 0; i < a->mjticks->len; i++) {
        mjt = &g_array_index(a->mjticks, GwyLabeledTick, i);
        g_string_free(mjt->ttext, TRUE);
    }
    g_array_free(a->mjticks, FALSE);
    g_array_free(a->miticks, FALSE);

    a->mjticks = g_array_new(FALSE, FALSE, sizeof(GwyLabeledTick));
    a->miticks = g_array_new(FALSE, FALSE, sizeof(GwyTick));

    /*find tick positions*/
    if (!a->is_logarithmic)
        gwy_axis_normalscale(a);
    else
        gwy_axis_logscale(a);
    /*label ticks*/
    gwy_axis_formatticks(a);
    /*precompute screen coordinates of ticks (must be done after each geometry change)*/

    return 0;
}

static gint
gwy_axis_precompute(GwyAxis *a, gint scrmin, gint scrmax)
{
    guint i;
    gdouble dist, range;
    GwyLabeledTick *pmjt;
    GwyTick *pmit;

    dist = (gdouble)scrmax-scrmin-1;
    range = a->max - a->min;
    if (a->is_logarithmic) range = log10(a->max)-log10(a->min);

    for (i = 0; i < a->mjticks->len; i++) {
        pmjt = &g_array_index (a->mjticks, GwyLabeledTick, i);
        if (!a->is_logarithmic)
            pmjt->t.scrpos = (gint)(0.5 + scrmin
                                    + (pmjt->t.value - a->min)/range*dist);
        else
            pmjt->t.scrpos = (gint)(0.5 + scrmin
                                    + (pmjt->t.value - log10(a->min))/range*dist);
    }

    for (i = 0; i < a->miticks->len; i++) {
        pmit = &g_array_index (a->miticks, GwyTick, i);
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
    gdouble range; /*only for automode and precision*/
    GwyLabeledTick mji, mjx, *pmjt;
    /*determine range*/
    if (a->mjticks->len == 0) {
        g_warning("No ticks found");
        return 1;
    }
    mji = g_array_index(a->mjticks, GwyLabeledTick, 0);
    mjx = g_array_index(a->mjticks, GwyLabeledTick, a->mjticks->len - 1);
    if (!a->is_logarithmic)
        range = fabs(mjx.t.value - mji.t.value);
    else
        range = fabs(pow(10, mjx.t.value) - pow(10, mji.t.value));

    for (i = 0; i< a->mjticks->len; i++)
    {
        /*find the value we want to put in string*/
        pmjt = &g_array_index(a->mjticks, GwyLabeledTick, i);
        if (!a->is_logarithmic)
            value = pmjt->t.value;
        else
            value = pow(10, pmjt->t.value);

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
    return 0;
}



/**
 * gwy_axis_set_visible:
 * @axis: axis widget
 * @is_visible: visibility
 *
 * Sets visibility of axis.
 **/
void
gwy_axis_set_visible(GwyAxis *axis, gboolean is_visible)
{
    axis->is_visible = is_visible;
}

/**
 * gwy_axis_set_auto:
 * @axis: axis widget
 * @is_auto: auto preperty
 *
 * Sets the auto property. If TRUE, axis changes fonts
 * and ticks sizes to produce reasonable output at different
 * widget sizes.
 **/
void
gwy_axis_set_auto(GwyAxis *axis, gboolean is_auto)
{
    axis->is_auto = is_auto;
}

/**
 * gwy_axis_set_req:
 * @axis: axis widget
 * @min: minimum requisistion
 * @max: maximum requisition
 *
 * Set requisition of axis boundaries. Axis will fix the boundaries
 * to satisfy requisition but still have reasonable tick values and spacing.
 **/
void
gwy_axis_set_req(GwyAxis *axis, gdouble min, gdouble max)
{
    axis->reqmin = min;
    axis->reqmax = max;

    /*prevent axis to allow null range. It has no sense*/
    if (min==max) axis->reqmax += 10.0;

    gwy_axis_adjust(axis,
                    (GTK_WIDGET(axis))->allocation.width,
                    (GTK_WIDGET(axis))->allocation.height);
}

/**
 * gwy_axis_set_style:
 * @axis: axis widget
 * @style: axis style
 *
 * Set axis style. The style affects used tick sizes, fonts etc.
 **/
void
gwy_axis_set_style(GwyAxis *axis, GwyAxisParams style)
{
    axis->par = style;
    gwy_axis_adjust(axis,
                    (GTK_WIDGET(axis))->allocation.width,
                    (GTK_WIDGET(axis))->allocation.height);
}

/**
 * gwy_axis_get_maximum:
 * @axis: axis widget
 *
 *
 *
 * Returns: real maximum of axis
 **/
gdouble
gwy_axis_get_maximum(GwyAxis *axis)
{
    return axis->max;
}

/**
 * gwy_axis_get_minimum:
 * @axis: axis widget
 *
 *
 *
 * Returns: real minimum of axis
 **/
gdouble
gwy_axis_get_minimum(GwyAxis *axis)
{
    return axis->min;
}

/**
 * gwy_axis_get_reqmaximum:
 * @axis: axis widget
 *
 *
 *
 * Returns: axis requisition maximum
 **/
gdouble
gwy_axis_get_reqmaximum(GwyAxis *axis)
{
    return axis->reqmax;
}

/**
 * gwy_axis_get_reqminimum:
 * @axis: axis widget
 *
 *
 *
 * Returns: axis requisition minimum
 **/
gdouble
gwy_axis_get_reqminimum(GwyAxis *axis)
{
    return axis->reqmin;
}

/**
 * gwy_axis_set_label:
 * @axis: axis widget
 * @label_text: label to be set
 *
 * sets the label text
 **/
void
gwy_axis_set_label(GwyAxis *axis, GString *label_text)
{
    gwy_debug("label_text = <%s>", label_text->str);
    g_string_assign(axis->label_text, label_text->str);
    gwy_sci_text_set_text(GWY_SCI_TEXT(GWY_AXIS_DIALOG(axis->dialog)->sci_text),
                          label_text->str);
    g_signal_emit(axis, axis_signals[LABEL_UPDATED], 0);
    gtk_widget_queue_draw(GTK_WIDGET(axis));
}

/**
 * gwy_axis_get_label:
 * @axis: axis widget
 *
 *
 *
 * Returns: axis label string
 **/
GString*
gwy_axis_get_label(GwyAxis *axis)
{
    return axis->label_text;
}

/* XXX: Fuck! There's NO way how the units could be unset! */
/* XXX: DoubleFuck! The thing GOBBLES the passed string! */
/**
 * gwy_axis_set_unit:
 * @axis: axis widget
 * @unit: label unit
 *
 * Sets the label unit. This will be added automatically
 * to the label.
 **/
void
gwy_axis_set_unit(GwyAxis *axis, char *unit)
{
    axis->unit = unit;
    axis->has_unit = 1;
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

/************************** Documentation ****************************/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
