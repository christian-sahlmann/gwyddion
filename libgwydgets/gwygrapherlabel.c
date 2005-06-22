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

#include <math.h>
#include <gtk/gtkmain.h>
#include <glib-object.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include "gwygrapher.h"
#include "gwygraphmodel.h"

#define GWY_GRAPH_LABEL_TYPE_NAME "GwyGraphLabel"


/* Forward declarations - widget related*/
static void     gwy_graph_label_class_init           (GwyGraphLabelClass *klass);
static void     gwy_graph_label_init                 (GwyGraphLabel *label);
static void     gwy_graph_label_finalize             (GObject *object);

static void     gwy_graph_label_realize              (GtkWidget *widget);
static void     gwy_graph_label_unrealize            (GtkWidget *widget);
static void     gwy_graph_label_size_request         (GtkWidget *widget,
                                                      GtkRequisition *requisition);
static void     gwy_graph_label_size_allocate        (GtkWidget *widget,
                                                      GtkAllocation *allocation);
static gboolean gwy_graph_label_expose               (GtkWidget *widget,
                                                      GdkEventExpose *event);

static gboolean gwy_graph_label_button_press         (GtkWidget *widget,
                                                      GdkEventButton *event);
/*static gboolean gwy_graph_label_button_release       (GtkWidget *widget,
                                                      GdkEventButton *event);
*/

/* Forward declarations - label related*/
void            gwy_graph_label_draw_label           (GtkWidget *widget);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

GType
gwy_graph_label_get_type(void)
{
    static GType gwy_graph_label_type = 0;

    if (!gwy_graph_label_type) {
        static const GTypeInfo gwy_graph_label_info = {
            sizeof(GwyGraphLabelClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_label_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphLabel),
            0,
            (GInstanceInitFunc)gwy_graph_label_init,
            NULL,
        };
        gwy_debug("");
        gwy_graph_label_type = g_type_register_static(GTK_TYPE_WIDGET,
                                                      GWY_GRAPH_LABEL_TYPE_NAME,
                                                      &gwy_graph_label_info,
                                                      0);
    }

    return gwy_graph_label_type;
}

static void
gwy_graph_label_class_init(GwyGraphLabelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

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
    gwy_debug("");
    label->reqwidth = 10;
    label->reqheight = 10;
    label->samplepos = NULL;
}

/**
 * gwy_graph_label_new:
 * 
 * creates new grapher label. 
 *
 * Returns: new grapher label 
 **/
GtkWidget*
gwy_graph_label_new()
{
    GwyGraphLabel *label;

    gwy_debug("");

    label = gtk_type_new (gwy_graph_label_get_type ());

    label->label_font = pango_font_description_new();
    pango_font_description_set_family(label->label_font, "Helvetica");
    pango_font_description_set_style(label->label_font, PANGO_STYLE_NORMAL);
    pango_font_description_set_variant(label->label_font, PANGO_VARIANT_NORMAL);
    pango_font_description_set_weight(label->label_font, PANGO_WEIGHT_NORMAL);
    pango_font_description_set_size(label->label_font, 10*PANGO_SCALE);

    gtk_widget_set_events(GTK_WIDGET(label), 0);

    return GTK_WIDGET(label);
}

static void
gwy_graph_label_finalize(GObject *object)
{
    GwyGraphLabel *label;

    gwy_debug("finalizing a GwyGraphLabel (refcount = %u)", object->ref_count);

    g_return_if_fail(GWY_IS_GRAPH_LABEL(object));

    label = GWY_GRAPH_LABEL(object);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_graph_label_unrealize(GtkWidget *widget)
{
    GwyGraphLabel *label;

    label = GWY_GRAPH_LABEL(widget);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static void
gwy_graph_label_realize(GtkWidget *widget)
{
    GwyGraphLabel *label;
    GdkWindowAttr attributes;
    gint attributes_mask;
    GtkStyle *s;

    gwy_debug("realizing a GwyGraphLabel (%ux%u)",
              widget->allocation.width, widget->allocation.height);

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_GRAPH_LABEL(widget));

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

}


static void
gwy_graph_label_size_request(GtkWidget *widget,
                             GtkRequisition *requisition)
{
    GwyGraphLabel *label;
    gwy_debug("");

    if (widget==NULL)
    {
        requisition->width = 0;
        requisition->height = 0;
    }
    else
    {
        label = GWY_GRAPH_LABEL(widget);
        requisition->width = label->reqwidth;
        requisition->height = label->reqheight;
    }
}


static void
gwy_graph_label_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GwyGraphLabel *label;

    gwy_debug("");

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_GRAPH_LABEL(widget));
    g_return_if_fail(allocation != NULL);

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

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH_LABEL(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);
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

void gwy_graph_label_draw_label_on_drawable(GdkDrawable *drawable, GdkGC *gc, PangoLayout *layout,
                                              gint x, gint y, gint width, gint height,
                                              GwyGraphLabel *label)
{
    gint ypos, winheight, winwidth, winx, winy, frame_off;
    gint i;
    GwyGraphCurveModel *curvemodel;
    GwyGraphModel *model;
    PangoRectangle rect;
    GdkColor fg;
    GdkColormap* cmap;
    
    model = GWY_GRAPH_MODEL(label->graph_model);
    pango_layout_set_font_description(layout, label->label_font);

    frame_off = model->label_frame_thickness/2;
    ypos = 5 + frame_off;
    
    cmap = gdk_colormap_get_system();
    fg.red = 0;
    fg.green = 0;
    fg.blue = 0;
    gdk_colormap_alloc_color(cmap, &fg, TRUE, TRUE);

    winx = x;
    winy = y;
    winwidth = x + width;
    winheight = y + height;
    for (i=0; i<model->ncurves; i++)
    {
        curvemodel = GWY_GRAPH_CURVE_MODEL(model->curves[i]);
        
        pango_layout_set_text(layout, curvemodel->description->str, curvemodel->description->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);
        
        if (model->label_reverse)
            gdk_draw_layout(drawable, gc, winwidth - rect.width - 25 - frame_off, ypos, layout);
        else
            gdk_draw_layout(drawable, gc, 25 + frame_off, ypos, layout);
        
        label->samplepos[i] = ypos;

        if (curvemodel->type == GWY_GRAPH_CURVE_LINE || curvemodel->type == GWY_GRAPH_CURVE_LINE_POINTS)
        {
            if (model->label_reverse)
                gwy_graph_draw_line(drawable, gc, 
                                      winwidth - 20 - frame_off, ypos + rect.height/2, winwidth - 5, ypos + rect.height/2,
                                      curvemodel->line_style, curvemodel->line_size,
                                      &(curvemodel->color));
            else
                gwy_graph_draw_line(drawable, gc, 
                                      5 + frame_off, ypos + rect.height/2, 20 + frame_off, ypos + rect.height/2,
                                      curvemodel->line_style, curvemodel->line_size,
                                      &(curvemodel->color));
        }
        if (curvemodel->type == GWY_GRAPH_CURVE_POINTS || curvemodel->type == GWY_GRAPH_CURVE_LINE_POINTS)
        {
            if (model->label_reverse)
                gwy_graph_draw_point (drawable, gc, 
                                     winwidth - 13 - frame_off, ypos + rect.height/2,
                                   curvemodel->point_type, curvemodel->point_size,
                                   &(curvemodel->color), FALSE); 
            else
                gwy_graph_draw_point (drawable, gc, 
                                     12 + frame_off, ypos + rect.height/2,
                                   curvemodel->point_type, curvemodel->point_size,
                                   &(curvemodel->color), FALSE);
        }
        
        gdk_gc_set_foreground(gc, &fg);
        
        ypos += rect.height + 5;     
    }
    
    if ( model->label_frame_thickness > 0)
    {
        gdk_gc_set_line_attributes (gc, model->label_frame_thickness,
                      GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

        gdk_draw_line(drawable, gc,
                      model->label_frame_thickness/2,
                      model->label_frame_thickness/2,
                      winwidth - model->label_frame_thickness/2 - 1,
                      model->label_frame_thickness/2);
        gdk_draw_line(drawable, gc,
                      model->label_frame_thickness/2,
                      winheight - model->label_frame_thickness/2 - 1,
                      winwidth - model->label_frame_thickness/2 - 1,
                      winheight - model->label_frame_thickness/2 - 1);
        gdk_draw_line(drawable, gc,
                      model->label_frame_thickness/2,
                      model->label_frame_thickness/2,
                      model->label_frame_thickness/2,
                      winheight - model->label_frame_thickness/2 - 1);
        gdk_draw_line(drawable, gc,
                      winwidth - model->label_frame_thickness/2 - 1,
                      model->label_frame_thickness/2,
                      winwidth - model->label_frame_thickness/2 - 1,
                      winheight - model->label_frame_thickness/2 - 1);
    }
  
}

void gwy_graph_label_draw_label(GtkWidget *widget)
{
    gint winheight, winwidth, windepth, winx, winy;
    GwyGraphLabel *label;
    PangoLayout *layout;
    GdkGC *mygc;

    mygc = gdk_gc_new(widget->window);

    label = GWY_GRAPH_LABEL(widget);
    layout = gtk_widget_create_pango_layout(widget, "");

    gdk_window_get_geometry(widget->window, &winx, &winy, &winwidth, &winheight, &windepth);    
    gwy_graph_label_draw_label_on_drawable(GDK_DRAWABLE(widget->window), mygc, layout,
                                             0, 0, winwidth, winheight,
                                             label);
    g_object_unref((GObject *)mygc);
    
}

/*determine requested size of label (will be needed by grapharea to put the label into layout)*/
static void
set_requised_size(GwyGraphLabel *label)
{
    gint i;
    PangoLayout *layout;
    PangoRectangle rect;
    GwyGraphCurveModel *curvemodel;
    GwyGraphModel *model = GWY_GRAPH_MODEL(label->graph_model);
  
    label->reqheight = 0;
    label->reqwidth = 0;
    
    for (i=0; i<model->ncurves; i++)
    {
        curvemodel = GWY_GRAPH_CURVE_MODEL(model->curves[i]);
        
        layout = gtk_widget_create_pango_layout(GTK_WIDGET(label), "");
       
        pango_layout_set_font_description(layout, label->label_font);
        pango_layout_set_text(layout, curvemodel->description->str, curvemodel->description->len);
        pango_layout_get_pixel_extents(layout, NULL, &rect);

        if (label->reqwidth < rect.width) label->reqwidth = rect.width + 30 + model->label_frame_thickness;
        label->reqheight += rect.height + 5 + model->label_frame_thickness;
    }
    if (label->reqwidth == 0) label->reqwidth = 30;
    if (label->reqheight == 0) label->reqheight = 30;
}

/*synchronize label with information in graphmodel*/
void 
gwy_graph_label_refresh(GwyGraphLabel *label)
{
    GwyGraphModel *model = GWY_GRAPH_MODEL(label->graph_model);
    
    /*repaint label samples and descriptions*/
    if (label->samplepos) g_free(label->samplepos);
    if (model->ncurves > 0) label->samplepos = g_new(gint, model->ncurves);
    else label->samplepos = NULL;
    
    set_requised_size(label);
    gtk_widget_queue_resize(GTK_WIDGET(label));
    gtk_widget_queue_draw(GTK_WIDGET(label));
}

/*set model*/
void
gwy_graph_label_change_model(GwyGraphLabel *label, gpointer gmodel)
{
    label->graph_model = gmodel;
}


void
gwy_graph_label_enable_user_input(GwyGraphLabel *label, gboolean enable)
{
    label->enable_user_input = enable;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
