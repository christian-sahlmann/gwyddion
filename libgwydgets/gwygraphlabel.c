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
#include <string.h>
#include <gtk/gtkmain.h>
#include <glib-object.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwygraphbasics.h>
#include <libgwydgets/gwygraphlabel.h>

static void     gwy_graph_label_finalize      (GObject *object);
static void     gwy_graph_label_realize       (GtkWidget *widget);
static void     gwy_graph_label_unrealize     (GtkWidget *widget);
static void     gwy_graph_label_size_request  (GtkWidget *widget,
                                               GtkRequisition *requisition);
static void     gwy_graph_label_size_allocate (GtkWidget *widget,
                                               GtkAllocation *allocation);
static gboolean gwy_graph_label_expose        (GtkWidget *widget,
                                               GdkEventExpose *event);
static void     gwy_graph_label_refresh_all   (GwyGraphLabel *label);
static void     gwy_graph_label_calculate_size(GwyGraphLabel *label);
static void     gwy_graph_label_model_notify  (GwyGraphLabel *label,
                                               GParamSpec *param,
                                               GwyGraphModel *gmodel);
static void     gwy_graph_label_curve_notify  (GwyGraphLabel *label,
                                               gint i,
                                               GParamSpec *param);
static void     gwy_graph_label_draw_label    (GtkWidget *widget);

G_DEFINE_TYPE(GwyGraphLabel, gwy_graph_label, GTK_TYPE_WIDGET)

static void
gwy_graph_label_class_init(GwyGraphLabelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    gobject_class->finalize = gwy_graph_label_finalize;

    widget_class->realize = gwy_graph_label_realize;
    widget_class->expose_event = gwy_graph_label_expose;
    widget_class->size_request = gwy_graph_label_size_request;
    widget_class->unrealize = gwy_graph_label_unrealize;
    widget_class->size_allocate = gwy_graph_label_size_allocate;
}

static void
gwy_graph_label_init(GwyGraphLabel *label)
{
    label->reqwidth = 10;
    label->reqheight = 10;
    label->samplepos = g_array_new(FALSE, FALSE, sizeof(gint));
}

/**
 * gwy_graph_label_new:
 *
 * creates new graph label.
 *
 * Returns: new graph label widget
 **/
GtkWidget*
gwy_graph_label_new()
{
    GwyGraphLabel *label;
    PangoFontDescription *description;
    PangoContext *context;
    gint size;

    gwy_debug("");

    label = g_object_new(GWY_TYPE_GRAPH_LABEL, NULL);

    context = gtk_widget_get_pango_context(GTK_WIDGET(label));
    description = pango_context_get_font_description(context);

    /* Make major font a bit smaller */
    label->font_desc = pango_font_description_copy(description);
    size = pango_font_description_get_size(label->font_desc);
    size = MAX(1, size*10/11);
    pango_font_description_set_size(label->font_desc, size);

    gtk_widget_set_events(GTK_WIDGET(label), 0);

    return GTK_WIDGET(label);
}

static void
gwy_graph_label_finalize(GObject *object)
{
    GwyGraphLabel *label;

    label = GWY_GRAPH_LABEL(object);

    gwy_signal_handler_disconnect(label->graph_model, label->model_notify_id);
    gwy_signal_handler_disconnect(label->graph_model, label->curve_notify_id);

    gwy_object_unref(label->graph_model);
    pango_font_description_free(label->font_desc);
    g_array_free(label->samplepos, TRUE);

    G_OBJECT_CLASS(gwy_graph_label_parent_class)->finalize(object);
}

static void
gwy_graph_label_unrealize(GtkWidget *widget)
{
    GwyGraphLabel *label;

    label = GWY_GRAPH_LABEL(widget);

    if (GTK_WIDGET_CLASS(gwy_graph_label_parent_class)->unrealize)
        GTK_WIDGET_CLASS(gwy_graph_label_parent_class)->unrealize(widget);
}

static void
gwy_graph_label_realize(GtkWidget *widget)
{
    GwyGraphLabel *label;
    GdkWindowAttr attributes;
    gint i, attributes_mask;
    GtkStyle *style;

    gwy_debug("realizing a GwyGraphLabel (%ux%u)",
              widget->allocation.width, widget->allocation.height);

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    label = GWY_GRAPH_LABEL(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget)
                            | GDK_EXPOSURE_MASK;
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
}


static void
gwy_graph_label_size_request(GtkWidget *widget,
                             GtkRequisition *requisition)
{
    GwyGraphLabel *label;

    gwy_debug("");

    label = GWY_GRAPH_LABEL(widget);
    requisition->width = label->reqwidth;
    requisition->height = label->reqheight;
}

static void
gwy_graph_label_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GwyGraphLabel *label;

    gwy_debug("");

    widget->allocation = *allocation;
    if (GTK_WIDGET_REALIZED(widget)) {
        label = GWY_GRAPH_LABEL(widget);

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
    }
}

static gboolean
gwy_graph_label_expose(GtkWidget *widget,
                       GdkEventExpose *event)
{
    GwyGraphLabel *label;

    gwy_debug("");

    if (event->count > 0)
        return FALSE;

    label = GWY_GRAPH_LABEL(widget);
    gdk_window_clear_area(widget->window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);
    gwy_graph_label_draw_label(widget);

    return FALSE;
}

/**
 * gwy_graph_label_set_model:
 * @label: A graph label.
 * @gmodel: New graph model.
 *
 * Sets new model of a graph label.
 **/
void
gwy_graph_label_set_model(GwyGraphLabel *label,
                          GwyGraphModel *gmodel)
{
    g_return_if_fail(GWY_IS_GRAPH_LABEL(label));
    g_return_if_fail(!gmodel || GWY_IS_GRAPH_MODEL(gmodel));

    if (label->graph_model == gmodel)
        return;

    gwy_signal_handler_disconnect(label->graph_model, label->model_notify_id);
    gwy_signal_handler_disconnect(label->graph_model, label->curve_notify_id);

    if (gmodel)
        g_object_ref(gmodel);
    gwy_object_unref(label->graph_model);
    label->graph_model = gmodel;

    if (gmodel) {
        label->model_notify_id
            = g_signal_connect_swapped(gmodel, "notify",
                                       G_CALLBACK(gwy_graph_label_model_notify),
                                       label);
        label->curve_notify_id
            = g_signal_connect_swapped(gmodel, "curve-notify",
                                       G_CALLBACK(gwy_graph_label_curve_notify),
                                       label);
    }

    gwy_graph_label_refresh_all(label);
}

GwyGraphModel*
gwy_graph_label_get_model(GwyGraphLabel *label)
{
    g_return_val_if_fail(GWY_IS_GRAPH_LABEL(label), NULL);

    return label->graph_model;
}

static void
gwy_graph_label_model_notify(GwyGraphLabel *label,
                             GParamSpec *pspec,
                             GwyGraphModel *gmodel)
{
    /* FIXME: A bit simplistic */
    if (g_str_has_prefix(pspec->name, "label-")) {
        const gchar *name = pspec->name + strlen("label-");

        if (gwy_strequal(name, "visible")) {
            gboolean visible;

            g_object_get(gmodel, pspec->name, &visible, NULL);
            if (GTK_WIDGET_REALIZED(label)) {
                if (visible)
                    gtk_widget_show(GTK_WIDGET(label));
                else
                    gtk_widget_hide(GTK_WIDGET(label));
            }
        }
        else
            gwy_graph_label_refresh_all(label);
    }

    if (gwy_strequal(pspec->name, "n-curves")) {
        gwy_graph_label_refresh_all(label);
        return;
    }
}

static void
gwy_graph_label_curve_notify(GwyGraphLabel *label,
                             G_GNUC_UNUSED gint i,
                             GParamSpec *pspec)
{
    /* These three should not change label size */
    if (gwy_strequal(pspec->name, "color")
        || gwy_strequal(pspec->name, "line-style")
        || gwy_strequal(pspec->name, "point-type")) {
        gtk_widget_queue_draw(GTK_WIDGET(label));
        return;
    }

    gwy_graph_label_refresh_all(label);
}

/**
 * gwy_graph_label_draw_on_drawable:
 * @label: graph label
 * @drawable: the #GdkDrawable
 * @gc: a #GdkGC graphics context.
 * @layout: pango layout
 * @x: x position where label is to be drawn
 * @y: y position where label is to be drawn
 * @width: width of the label
 * @height: hieght of the label
 *
 * draws a graph label on a drawable
 **/
void
gwy_graph_label_draw_on_drawable(GwyGraphLabel *label,
                                 GdkDrawable *drawable,
                                 GdkGC *gc,
                                 PangoLayout *layout,
                                 gint x, gint y,
                                 gint width, gint height)
{
    gint ypos, winheight, winwidth, winx, winy, frame_off;
    gint i, nc;
    GwyGraphCurveModel *curvemodel;
    GwyGraphModel *model;
    PangoRectangle rect;
    GdkColor fg = { 0, 0, 0, 0 };
    GdkColor color = { 0, 65535, 65535, 65535 };

    if (!label->graph_model)
        return;

    model = GWY_GRAPH_MODEL(label->graph_model);
    pango_layout_set_font_description(layout, label->font_desc);

    frame_off = model->label_frame_thickness/2;
    ypos = 5 + frame_off;

    gdk_gc_set_rgb_fg_color(gc, &color);
    gdk_draw_rectangle(drawable, gc, TRUE, x, y, width, height);
    gdk_gc_set_rgb_fg_color(gc, &fg);

    winx = x;
    winy = y;
    winwidth = width;
    winheight = height;

    nc = gwy_graph_model_get_n_curves(model);
    for (i = 0; i < nc; i++) {
        curvemodel = gwy_graph_model_get_curve(model, i);

        pango_layout_set_markup(layout, curvemodel->description->str,
                                curvemodel->description->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);

        if (model->label_reverse)
            gdk_draw_layout(drawable, gc,
                            winx + winwidth - rect.width - 25 - frame_off,
                            winy + ypos,
                            layout);
        else
            gdk_draw_layout(drawable, gc,
                            winx + 25 + frame_off,
                            winy + ypos,
                            layout);

        g_array_index(label->samplepos, gint, i) = ypos;

        if (curvemodel->mode == GWY_GRAPH_CURVE_LINE
            || curvemodel->mode == GWY_GRAPH_CURVE_LINE_POINTS) {
            if (model->label_reverse)
                gwy_graph_draw_line(drawable, gc,
                                    winx + winwidth - 20 - frame_off,
                                    winy + ypos + rect.height/2,
                                    winx + winwidth - 5,
                                    winy + ypos + rect.height/2,
                                    curvemodel->line_style,
                                    curvemodel->line_width,
                                    &(curvemodel->color));
            else
                gwy_graph_draw_line(drawable, gc,
                                    winx + 5 + frame_off,
                                    winy + ypos + rect.height/2,
                                    winx + 20 + frame_off,
                                    winy + ypos + rect.height/2,
                                    curvemodel->line_style,
                                    curvemodel->line_width,
                                    &(curvemodel->color));
        }
        if (curvemodel->mode == GWY_GRAPH_CURVE_POINTS
            || curvemodel->mode == GWY_GRAPH_CURVE_LINE_POINTS) {
            if (model->label_reverse)
                gwy_graph_draw_point(drawable, gc,
                                     winx + winwidth - 13 - frame_off,
                                     winy + ypos + rect.height/2,
                                     curvemodel->point_type,
                                     curvemodel->point_size,
                                     &(curvemodel->color), FALSE);
            else
                gwy_graph_draw_point(drawable, gc,
                                     winx + 12 + frame_off,
                                     winy + ypos + rect.height/2,
                                     curvemodel->point_type,
                                     curvemodel->point_size,
                                     &(curvemodel->color), FALSE);
        }
        gdk_gc_set_rgb_fg_color(gc, &fg);

        ypos += rect.height + 5;
    }

    if (model->label_frame_thickness > 0) {
        gdk_gc_set_line_attributes(gc, model->label_frame_thickness,
                                   GDK_LINE_SOLID, GDK_CAP_ROUND,
                                   GDK_JOIN_MITER);

        gdk_draw_line(drawable, gc,
                      winx + model->label_frame_thickness/2,
                      winy + model->label_frame_thickness/2,
                      winx + winwidth - model->label_frame_thickness/2 - 1,
                      winy + model->label_frame_thickness/2);
        gdk_draw_line(drawable, gc,
                      winx + model->label_frame_thickness/2,
                      winy + winheight - model->label_frame_thickness/2 - 1,
                      winx + winwidth - model->label_frame_thickness/2 - 1,
                      winy + winheight - model->label_frame_thickness/2 - 1);
        gdk_draw_line(drawable, gc,
                      winx + model->label_frame_thickness/2,
                      winy + model->label_frame_thickness/2,
                      winx + model->label_frame_thickness/2,
                      winy + winheight - model->label_frame_thickness/2 - 1);
        gdk_draw_line(drawable, gc,
                      winx + winwidth - model->label_frame_thickness/2 - 1,
                      winy + model->label_frame_thickness/2,
                      winx + winwidth - model->label_frame_thickness/2 - 1,
                      winy + winheight - model->label_frame_thickness/2 - 1);
    }
}

static void
gwy_graph_label_draw_label(GtkWidget *widget)
{
    gint winheight, winwidth, windepth, winx, winy;
    GwyGraphLabel *label;
    PangoLayout *layout;
    GdkGC *mygc;

    mygc = gdk_gc_new(widget->window);

    label = GWY_GRAPH_LABEL(widget);
    layout = gtk_widget_create_pango_layout(widget, NULL);

    gdk_window_get_geometry(widget->window,
                            &winx, &winy, &winwidth, &winheight, &windepth);
    gwy_graph_label_draw_on_drawable(label, GDK_DRAWABLE(widget->window),
                                     mygc, layout,
                                     0, 0, winwidth, winheight);
    g_object_unref(mygc);
    g_object_unref(layout);

}

/**
 * gwy_graph_label_refresh:
 * @label: graph label
 *
 * synchronize label with information in graphmodel
 **/
static void
gwy_graph_label_refresh_all(GwyGraphLabel *label)
{
    gint nc;

    nc = gwy_graph_model_get_n_curves(label->graph_model);
    g_array_set_size(label->samplepos, nc);
    gwy_graph_label_calculate_size(label);
    gtk_widget_queue_resize(GTK_WIDGET(label));
    gtk_widget_queue_draw(GTK_WIDGET(label));
}

/* Determine requested size of label
 * (It will be needed by grapharea to put the label into layout) */
/* FIXME: It seems to overestimate the size a bit, it's visible on labels with
 * many curves. */
static void
gwy_graph_label_calculate_size(GwyGraphLabel *label)
{
    gint i, nc;
    PangoLayout *layout;
    PangoRectangle rect;
    GwyGraphCurveModel *curvemodel;
    GwyGraphModel *model;

    label->reqheight = 0;
    label->reqwidth = 0;

    model = GWY_GRAPH_MODEL(label->graph_model);
    nc = gwy_graph_model_get_n_curves(model);
    for (i = 0; i < nc; i++) {
        curvemodel = gwy_graph_model_get_curve(model, i);

        layout = gtk_widget_create_pango_layout(GTK_WIDGET(label), NULL);

        pango_layout_set_font_description(layout, label->font_desc);
        pango_layout_set_markup(layout, curvemodel->description->str,
                                curvemodel->description->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);

        if (label->reqwidth < rect.width)
            label->reqwidth = rect.width + 30 + model->label_frame_thickness;
        label->reqheight += rect.height + 5 + model->label_frame_thickness;

        g_object_unref(layout);
    }
    if (label->reqwidth == 0)
        label->reqwidth = 30;
    if (label->reqheight == 0)
        label->reqheight = 30;
}

/**
 * gwy_graph_label_enable_user_input:
 * @label: A graph label.
 * @enable: Whether to enable the user input.
 *
 * Enables or disables user input to a graph label.
 **/
void
gwy_graph_label_enable_user_input(GwyGraphLabel *label, gboolean enable)
{
    g_return_if_fail(GWY_IS_GRAPH_LABEL(label));

    label->enable_user_input = !!enable;
}

/**
 * gwy_graph_label_export_vector:
 * @label: A graph label.
 * @x: x position of the graph label
 * @y: y position of the graph label
 * @width: width of the graph label
 * @height: hieght of the graph label
 * @fontsize: fontsize of the label
 *
 * Returns: the graph label vector (piece of postscript code) as a string
 **/
/* XXX: Malformed documentation */
GString*
gwy_graph_label_export_vector(GwyGraphLabel *label,
                              gint x, gint y,
                              gint width, gint height,
                              gint fontsize)
{
    gint i, nc;
    GwyGraphCurveModel *curvemodel;
    GwyGraphModel *model;
    GString *out;
    gint xpos, ypos;
    const GwyRGBA *color;
    gint pointsize;
    gint linesize;

    out = g_string_new("%%Label\n");

    g_string_append_printf(out, "/Times-Roman findfont\n");
    g_string_append_printf(out, "%d scalefont\n setfont\n", fontsize);

    model = GWY_GRAPH_MODEL(label->graph_model);
    g_string_append_printf(out, "/box {\n"
                           "newpath\n"
                           "%d setlinewidth\n"
                           "%d %d M\n"
                           "%d %d L\n"
                           "%d %d L\n"
                           "%d %d L\n"
                           "closepath\n"
                           "} def\n",
                           model->label_frame_thickness,
                           x, y,
                           x + width, y,
                           x + width, y + height,
                           x, y + height);

    g_string_append_printf(out, "gsave\n");
    g_string_append_printf(out, "box\n");
    g_string_append_printf(out, "gsave\n");
    g_string_append_printf(out, "stroke\n");
    g_string_append_printf(out, "grestore\n");
    g_string_append_printf(out, "clip\n");
    g_string_append_printf(out, "1 setgray\n");
    g_string_append_printf(out, "fill\n");

    xpos = 5;
    ypos = height - fontsize;

    nc = gwy_graph_model_get_n_curves(model);
    for (i = 0; i < nc; i++) {
        curvemodel = gwy_graph_model_get_curve(model, i);
        pointsize = gwy_graph_curve_model_get_point_size(curvemodel);
        linesize = gwy_graph_curve_model_get_line_width(curvemodel);
        color = gwy_graph_curve_model_get_color(curvemodel);
        g_string_append_printf(out, "/hpt %d def\n", pointsize);
        g_string_append_printf(out, "/vpt %d def\n", pointsize);
        g_string_append_printf(out, "/hpt2 hpt 2 mul def\n");
        g_string_append_printf(out, "/vpt2 vpt 2 mul def\n");
        g_string_append_printf(out, "%d setlinewidth\n", linesize);
        g_string_append_printf(out, "%f %f %f setrgbcolor\n",
                               color->r, color->g, color->b);
        g_string_append_printf(out, "%d %d M\n", x + xpos, y + ypos);
        g_string_append_printf(out, "%d %d L\n", x + xpos + 20, y + ypos);
        g_string_append_printf(out, "%d %d R\n", 5, -(gint)(fontsize/4));
        g_string_append_printf(out, "(%s) show\n",
                               gwy_graph_curve_model_get_description(curvemodel));
        g_string_append_printf(out, "stroke\n");
        ypos -= fontsize + 5;
    }
    g_string_append_printf(out, "grestore\n");

    return out;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygraphlabel
 * @title: GwyGraphLabel
 * @short_description: Graph curve legend
 *
 * #GwyGraphLabel is a part of #GwyGraph, it renders frame with graph curve
 * legend.  It can be probabaly used only within #GwyGraph.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
