/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtk.h>

#include <glib-object.h>
#include "gwygrapharea.h"
#include "gwygraphlabel.h"

#define GWY_GRAPH_AREA_TYPE_NAME "GwyGraphArea"

/* Forward declarations - widget related*/
static void     gwy_graph_area_class_init           (GwyGraphAreaClass *klass);
static void     gwy_graph_area_init                 (GwyGraphArea *area);
static void     gwy_graph_area_finalize             (GObject *object);

static void     gwy_graph_area_realize              (GtkWidget *widget);
static void     gwy_graph_area_unrealize            (GtkWidget *widget);
static void     gwy_graph_area_size_allocate        (GtkWidget *widget, GtkAllocation *allocation);

static gboolean gwy_graph_area_expose               (GtkWidget *widget,
                                                      GdkEventExpose *event);
static gboolean gwy_graph_area_button_press         (GtkWidget *widget,
                                                      GdkEventButton *event);
static gboolean gwy_graph_area_button_release       (GtkWidget *widget,
                                                      GdkEventButton *event);

/* Forward declarations - area related*/
void            gwy_graph_area_draw_area            (GtkWidget *widget);
void            gwy_graph_area_draw_curves          (GtkWidget *widget);
void            gwy_graph_area_plot_refresh         (GwyGraphArea *area);
/* Local data */


typedef struct _GtkLayoutChild   GtkLayoutChild;

struct _GtkLayoutChild {
    GtkWidget *widget;
    gint x;
    gint y;
};

static gboolean        gwy_graph_area_motion_notify     (GtkWidget *widget,
                                                        GdkEventMotion *event);
static GtkLayoutChild* gwy_graph_area_find_child        (GwyGraphArea *area,
                                                        gint x,
                                                        gint y);
static void            gwy_graph_area_draw_child_rectangle  (GwyGraphArea *area);
static void            gwy_graph_area_clamp_coords_for_child(GwyGraphArea *area,
                                                        gint *x,
                                                        gint *y);

/* Local data */

static GtkWidgetClass *parent_class = NULL;


GType
gwy_graph_area_get_type(void)
{
    static GType gwy_graph_area_type = 0;

    if (!gwy_graph_area_type) {
        static const GTypeInfo gwy_graph_area_info = {
            sizeof(GwyGraphAreaClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_area_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphArea),
            0,
            (GInstanceInitFunc)gwy_graph_area_init,
            NULL,
        };
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
        gwy_graph_area_type = g_type_register_static(GTK_TYPE_LAYOUT,
                                                      GWY_GRAPH_AREA_TYPE_NAME,
                                                      &gwy_graph_area_info,
                                                      0);
    }

    return gwy_graph_area_type;
}

static void
gwy_graph_area_class_init(GwyGraphAreaClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;
  
    
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);
    gobject_class->finalize = gwy_graph_area_finalize;

    widget_class->realize = gwy_graph_area_realize;
    widget_class->unrealize = gwy_graph_area_unrealize;
    widget_class->expose_event = gwy_graph_area_expose;
    widget_class->size_allocate = gwy_graph_area_size_allocate;
    
    widget_class->button_press_event = gwy_graph_area_button_press;
    widget_class->button_release_event = gwy_graph_area_button_release;
    widget_class->motion_notify_event = gwy_graph_area_motion_notify;
}

static void
gwy_graph_area_init(GwyGraphArea *area)
{
    GtkLabel *ble;
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    area->gc = NULL;
    area->active = NULL;
    area->x_min = 0;
    area->x_max = 0;
    area->y_min = 0;
    area->y_max = 0;
     
    area->lab = GWY_GRAPH_LABEL(gwy_graph_label_new());
    gtk_layout_put(GTK_LAYOUT(area), GTK_WIDGET(area->lab), 90, 90); 

    ble = gtk_label_new("ble");
    gtk_layout_put(GTK_LAYOUT(area), GTK_WIDGET(ble), 10, 10);
}

GtkWidget*
gwy_graph_area_new(GtkAdjustment *hadjustment, GtkAdjustment *vadjustment)
{
    GwyGraphArea *area;
    
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    area = (GwyGraphArea*)gtk_widget_new(GWY_TYPE_GRAPH_AREA, "hadjustment", hadjustment,
                                         "vadjustment", vadjustment, NULL);
    
    gtk_widget_add_events(GTK_WIDGET(area), GDK_BUTTON_PRESS_MASK 
                          | GDK_BUTTON_RELEASE_MASK
                          | GDK_BUTTON_MOTION_MASK);
    
    area->curves = g_ptr_array_new();

    
    return GTK_WIDGET(area);
}

static void
gwy_graph_area_finalize(GObject *object)
{
    GwyGraphArea *area;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "finalizing a GwyGraphArea (refcount = %u)",
          object->ref_count);
    #endif

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_GRAPH_AREA(object));

    area = GWY_GRAPH_AREA(object);
    g_ptr_array_free(area->curves, 1);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_graph_area_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGraphArea *area;
    GtkAllocation *lab_alloc;
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    area = GWY_GRAPH_AREA(widget);
    lab_alloc = &GTK_WIDGET(area->lab)->allocation;

    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);
    if (lab_alloc->x != widget->allocation.width - lab_alloc->width - 5
        || lab_alloc->y != 5)
        gtk_layout_move(GTK_LAYOUT(area), GTK_WIDGET(area->lab), 
                        widget->allocation.width - lab_alloc->width - 5, 5);
    gwy_graph_area_plot_refresh(area);
}

static void
gwy_graph_area_realize(GtkWidget *widget)
{
    GdkColor fg, bg;
    GwyGraphArea *area;
    
    if (GTK_WIDGET_CLASS(parent_class)->realize)
        GTK_WIDGET_CLASS(parent_class)->realize(widget);
    
    area = GWY_GRAPH_AREA(widget);
    area->gc = gdk_gc_new(GTK_LAYOUT(widget)->bin_window);
    bg.pixel = 0xFFFFFFFF;
    fg.pixel = 0x00000000;
    gdk_gc_set_foreground(area->gc, &fg);
    gdk_gc_set_background(area->gc, &bg);
    
}

static void
gwy_graph_area_unrealize(GtkWidget *widget)
{
    GwyGraphArea *area;
    area = GWY_GRAPH_AREA(widget);
    
    if (area->gc) g_object_unref(area->gc);
        
    
    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static gboolean
gwy_graph_area_expose(GtkWidget *widget,
                       GdkEventExpose *event)
{
    GwyGraphArea *area;
    gint xc, yc;
    GdkPoint ps[4];
   
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    area = GWY_GRAPH_AREA(widget);
    
    gdk_window_clear_area(GTK_LAYOUT (widget)->bin_window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);
    
    gwy_graph_area_draw_curves(widget);
    gwy_graph_area_draw_area(widget);

    GTK_WIDGET_CLASS(parent_class)->expose_event(widget, event);
    return FALSE;
}


void gwy_graph_area_draw_area(GtkWidget *widget)
{
    GwyGraphArea *area;
    
    area = GWY_GRAPH_AREA(widget);
     
    /*plot boundary*/
    gdk_draw_line(GTK_LAYOUT (widget)->bin_window, area->gc,
                 0, 0, 0, widget->allocation.height-1);
    gdk_draw_line(GTK_LAYOUT (widget)->bin_window, area->gc,
                 0, 0, widget->allocation.width-1, 0);
    gdk_draw_line(GTK_LAYOUT (widget)->bin_window, area->gc,
                 widget->allocation.width-1, 0, widget->allocation.width-1, 
                 widget->allocation.height-1);
    gdk_draw_line(GTK_LAYOUT (widget)->bin_window, area->gc,
                 0, widget->allocation.height-1, widget->allocation.width-1, 
                 widget->allocation.height-1);
     
}    

void gwy_graph_area_draw_curves(GtkWidget *widget)
{
    GwyGraphArea *area;
    GwyGraphAreaCurve *pcurve;
    GdkColor fg;
    guint i, j;
    
    area = GWY_GRAPH_AREA(widget);
    
    fg.pixel = 0x00000000;
    
    for (i=0; i<area->curves->len; i++)
    {
        pcurve = g_ptr_array_index (area->curves, i); 
        gdk_gc_set_foreground(area->gc, &(pcurve->params.color));
        
        if (pcurve->params.is_line)
        {
            gdk_gc_set_line_attributes (area->gc, pcurve->params.line_size,
                      pcurve->params.line_style, GDK_CAP_ROUND, GDK_JOIN_MITER);
            gdk_draw_lines(GTK_LAYOUT (widget)->bin_window, area->gc,
                      pcurve->points, pcurve->data.N);
        }
        if (pcurve->params.is_point)
        {
            for (j=0; j<pcurve->data.N; j++)
            {
                gwy_graph_draw_point(GTK_LAYOUT(widget)->bin_window, area->gc, 
                                          pcurve->points[j].x, 
                                          pcurve->points[j].y, 
                                          pcurve->params.point_type, 
                                          pcurve->params.point_size, 
                                          &(pcurve->params.color),
                                          pcurve->params.is_line);
            }
        }
    }
    gdk_gc_set_line_attributes (area->gc, 1,
                  GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
    gdk_gc_set_foreground(area->gc, &fg);
}

static gboolean
gwy_graph_area_button_press(GtkWidget *widget, GdkEventButton *event)
{
    GwyGraphArea *area;
    GtkLayoutChild *child;
    gint x, y;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_if_fail(GWY_IS_GRAPH_AREA(widget));
    
    area = GWY_GRAPH_AREA(widget);
    gdk_window_get_position(event->window, &x, &y);
    x += (gint)event->x;
    y += (gint)event->y;
   
    child = gwy_graph_area_find_child(area, x, y);
    if (child) { printf("Child found.\n");
        area->active = child->widget;
        area->x0 = x;
        area->y0 = y;
        area->xoff = 0;
        area->yoff = 0;
        gwy_graph_area_draw_child_rectangle(area);
    }
    return FALSE;
}

static gboolean
gwy_graph_area_button_release(GtkWidget *widget, GdkEventButton *event)
{
    GwyGraphArea *area;
    gint x, y;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    area = GWY_GRAPH_AREA(widget);
    if (!area->active)
        return FALSE;

    gwy_graph_area_draw_child_rectangle(area);

    gdk_window_get_position(event->window, &x, &y);
    x += (gint)event->x;
    y += (gint)event->y;
    
    gwy_graph_area_clamp_coords_for_child(area, &x, &y);
    if (x != area->x0 || y != area->y0) {
        x -= area->x0 - area->active->allocation.x;
        y -= area->y0 - area->active->allocation.y;
        printf("Moving from %d %d to %d %d\n", area->x0, area->y0,  x, y);
        gtk_layout_move(GTK_LAYOUT(area), area->active, x, y);
    }

    area->active = NULL;

    return FALSE;
}

static gboolean
gwy_graph_area_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
    GwyGraphArea *area;
    gint x, y;

    area = GWY_GRAPH_AREA(widget);
    if (!area->active)
        return FALSE;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
 
    gdk_window_get_position(event->window, &x, &y);
    x += (gint)event->x;
    y += (gint)event->y;
    gwy_graph_area_clamp_coords_for_child(area, &x, &y);
    /* don't draw when we can't move */
    
    if (x - area->x0 == area->xoff
        && y - area->y0 == area->yoff)
        return FALSE;

    gwy_graph_area_draw_child_rectangle(area);
    area->xoff = x - area->x0;
    area->yoff = y - area->y0;
    printf("xoff=%d, yoff=%d\n", area->xoff, area->yoff);
    gwy_graph_area_draw_child_rectangle(area);

    return FALSE;
}

static GtkLayoutChild*
gwy_graph_area_find_child(GwyGraphArea *area, gint x, gint y)
{
    GList *chpl;
    for (chpl = GTK_LAYOUT(area)->children; chpl; chpl = g_list_next(chpl)) {
        GtkLayoutChild *child;
        GtkAllocation *allocation;

        child = (GtkLayoutChild*)chpl->data;
        allocation = &child->widget->allocation;
        printf("x,y=%d, %d,  child: %d, %d, %dx%d\n", x, y, allocation->x,
               allocation->y, allocation->width, allocation->height);
        if (x >= allocation->x
            && x < allocation->x + allocation->width
            && y >= allocation->y
            && y < allocation->y + allocation->height)
            return child;
    }
    return NULL;
}

static void
gwy_graph_area_clamp_coords_for_child(GwyGraphArea *area,
                                 gint *x,
                                 gint *y)
{
    GtkAllocation *allocation;
    gint min, max;

    allocation = &area->active->allocation;

    min = area->x0 - allocation->x;
    max = GTK_WIDGET(area)->allocation.width
          - (allocation->width - min) - 1;
    *x = CLAMP(*x, min, max);

    min = area->y0 - allocation->y;
    max = GTK_WIDGET(area)->allocation.height
          - (allocation->height - min) - 1;
    *y = CLAMP(*y, min, max);
}

static void
gwy_graph_area_draw_child_rectangle(GwyGraphArea *area)
{
    GtkAllocation *allocation;

    if (!area->active)
        return;

    allocation = &area->active->allocation;
    gdk_draw_rectangle(GTK_LAYOUT(area)->bin_window, area->gc, FALSE,
                       allocation->x + area->xoff,
                       allocation->y + area->yoff,
                       allocation->width,
                       allocation->height);
}

void 
gwy_graph_area_set_boundaries(GwyGraphArea *area, gdouble x_min, gdouble x_max, gdouble y_min, gdouble y_max)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
	area->x_min = x_min;
    area->y_min = y_min;
    area->x_max = x_max;
    area->y_max = y_max;
    gwy_graph_area_plot_refresh(area);
}

void
gwy_graph_area_plot_refresh(GwyGraphArea *area)
{
    guint i; 
    gint j;
    GwyGraphAreaCurve *pcurve;
    GtkWidget *widget;
    
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
	
    widget = GTK_WIDGET(area);
    /*recompute all data to be plotted according to window size*/
    area->x_shift = area->x_min;
    area->y_shift = area->y_min ;
    area->x_multiply = widget->allocation.width/(area->x_max - area->x_min);
    area->y_multiply = widget->allocation.height/(area->y_max - area->y_min);
  
    for (i=0; i<area->curves->len; i++)
    {
        pcurve = g_ptr_array_index (area->curves, i);
        for (j=0; j<pcurve->data.N; j++)
        {
            pcurve->points[j].x = (pcurve->data.xvals[j] - area->x_shift)*area->x_multiply;
            pcurve->points[j].y = widget->allocation.height - 1 - (pcurve->data.yvals[j] - area->y_shift)*area->y_multiply;
        }
    }
}

void 
gwy_graph_area_add_curve(GwyGraphArea *area, GwyGraphAreaCurve *curve)
{
    gint i;
   
    GwyGraphAreaCurve *pcurve; 
    
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
	    /*alloc one element, make deep copy and add it to array*/
   
    pcurve = g_new(GwyGraphAreaCurve, 1);
    pcurve->data.xvals = (gdouble *) g_try_malloc(curve->data.N*sizeof(gdouble));
    pcurve->data.yvals = (gdouble *) g_try_malloc(curve->data.N*sizeof(gdouble));
    pcurve->points = (GdkPoint *) g_try_malloc(curve->data.N*sizeof(GdkPoint));
    
    pcurve->params.is_line = curve->params.is_line;
    pcurve->params.is_point = curve->params.is_point;
    pcurve->params.line_style = curve->params.line_style;
    pcurve->params.line_size = curve->params.line_size;
    pcurve->params.point_type = curve->params.point_type;
    pcurve->params.point_size = curve->params.point_size;
    pcurve->params.color = curve->params.color;
    pcurve->params.description = g_string_new(curve->params.description->str);
     
    pcurve->data.N = curve->data.N;
    for (i=0; i<curve->data.N; i++)
    {
        pcurve->data.xvals[i] = curve->data.xvals[i];
        pcurve->data.yvals[i] = curve->data.yvals[i];
        pcurve->points[i].x = 0;
        pcurve->points[i].y = 0;
    }
    
    g_ptr_array_add(area->curves, (gpointer)(pcurve));
    gwy_graph_label_add_curve(area->lab, &(pcurve->params));
    
}

void 
gwy_graph_area_clear(GwyGraphArea *area)
{
    guint i;
    GwyGraphAreaCurve *pcurve;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
	    /*dealloc everything*/
    for (i=0; i<area->curves->len; i++)
    {
        pcurve = g_ptr_array_index (area->curves, i);
        g_free(pcurve->data.xvals);
        g_free(pcurve->data.yvals);
        g_free(pcurve->points);
        g_string_free(pcurve->params.description, 1);
    }

    gwy_graph_label_clear(area->lab);
    
    g_ptr_array_free(area->curves, 1);
    area->curves = g_ptr_array_new();        
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
