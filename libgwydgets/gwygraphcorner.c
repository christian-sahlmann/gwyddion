/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <glib-object.h>

#include "gwygraphcorner.h"

#define GWY_GRAPH_CORNER_TYPE_NAME "GwyGraphCorner"


/* Forward declarations - widget related*/
static void     gwy_graph_corner_class_init           (GwyGraphCornerClass *klass);
static void     gwy_graph_corner_init                 (GwyGraphCorner *graph_corner);
static void     gwy_graph_corner_finalize             (GObject *object);

static void     gwy_graph_corner_realize              (GtkWidget *widget);
static void     gwy_graph_corner_unrealize            (GtkWidget *widget);
static void     gwy_graph_corner_size_request         (GtkWidget *widget,
                                                      GtkRequisition *requisition);
static void     gwy_graph_corner_size_allocate        (GtkWidget *widget,
                                                      GtkAllocation *allocation);
static gboolean gwy_graph_corner_expose               (GtkWidget *widget,
                                                      GdkEventExpose *event);
static gboolean gwy_graph_corner_button_press         (GtkWidget *widget,
                                                      GdkEventButton *event);
static gboolean gwy_graph_corner_button_release       (GtkWidget *widget,
                                                      GdkEventButton *event);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

GType
gwy_graph_corner_get_type(void)
{
    static GType gwy_graph_corner_type = 0;

    if (!gwy_graph_corner_type) {
        static const GTypeInfo gwy_graph_corner_info = {
            sizeof(GwyGraphCornerClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_corner_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphCorner),
            0,
            (GInstanceInitFunc)gwy_graph_corner_init,
            NULL,
        };
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
        gwy_graph_corner_type = g_type_register_static(GTK_TYPE_WIDGET,
                                                      GWY_GRAPH_CORNER_TYPE_NAME,
                                                      &gwy_graph_corner_info,
                                                      0);
    }

    return gwy_graph_corner_type;
}

static void
gwy_graph_corner_class_init(GwyGraphCornerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_graph_corner_finalize;

    widget_class->realize = gwy_graph_corner_realize;
    widget_class->expose_event = gwy_graph_corner_expose;
    widget_class->size_request = gwy_graph_corner_size_request;
    widget_class->unrealize = gwy_graph_corner_unrealize;
    widget_class->size_allocate = gwy_graph_corner_size_allocate;
    widget_class->button_press_event = gwy_graph_corner_button_press;
    widget_class->button_release_event = gwy_graph_corner_button_release;

}

static void
gwy_graph_corner_init(GwyGraphCorner *graph_corner)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

}

GtkWidget*
gwy_graph_corner_new()
{
    GwyGraphCorner *graph_corner;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    graph_corner = gtk_type_new (gwy_graph_corner_get_type ());
    
     return GTK_WIDGET(graph_corner);
}

static void
gwy_graph_corner_finalize(GObject *object)
{
    GwyGraphCorner *graph_corner;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "finalizing a GwyGraphCorner (refcount = %u)",
          object->ref_count);
    #endif

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_GRAPH_CORNER(object));

    graph_corner = GWY_GRAPH_CORNER(object);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_graph_corner_unrealize(GtkWidget *widget)
{
    GwyGraphCorner *graph_corner;

    graph_corner = GWY_GRAPH_CORNER(widget);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static void
gwy_graph_corner_realize(GtkWidget *widget)
{
    GwyGraphCorner *graph_corner;
    GdkWindowAttr attributes;
    gint attributes_mask;
    GtkStyle *s;
    
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "realizing a GwyGraphCorner (%ux%u)",
          widget->allocation.x, widget->allocation.height);
    #endif

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_GRAPH_CORNER(widget));

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    graph_corner = GWY_GRAPH_CORNER(widget);

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
    widget->window = gdk_window_new(widget->parent->window,
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
gwy_graph_corner_size_request(GtkWidget *widget,
                             GtkRequisition *requisition)
{
    GwyGraphCorner *graph_corner;
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    graph_corner = GWY_GRAPH_CORNER(widget);
    
    requisition->width = 10;
    requisition->height = 10;
}

static void
gwy_graph_corner_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GwyGraphCorner *graph_corner;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_GRAPH_CORNER(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(widget)) {
        graph_corner = GWY_GRAPH_CORNER(widget);

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
        
    }
}


static gboolean
gwy_graph_corner_expose(GtkWidget *widget,
                       GdkEventExpose *event)
{
    GwyGraphCorner *graph_corner;
    gint xc, yc;
    GdkPoint ps[4];
    
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH_CORNER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    graph_corner = GWY_GRAPH_CORNER(widget);

    gdk_window_clear_area(widget->window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);
    
    return FALSE;
}


static gboolean
gwy_graph_corner_button_press(GtkWidget *widget,
                             GdkEventButton *event)
{
    GwyGraphCorner *graph_corner;
    double x, y;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
	    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH_CORNER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    graph_corner = GWY_GRAPH_CORNER(widget);


    return FALSE;
}

static gboolean
gwy_graph_corner_button_release(GtkWidget *widget,
                               GdkEventButton *event)
{
    GwyGraphCorner *graph_corner;
    gdouble x, y;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
	
    
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH_CORNER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    graph_corner = GWY_GRAPH_CORNER(widget);


    return FALSE;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
