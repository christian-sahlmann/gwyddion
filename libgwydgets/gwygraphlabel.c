/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <glib-object.h>

#include "gwygraphlabel.h"

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
static gboolean gwy_graph_label_button_release       (GtkWidget *widget,
                                                      GdkEventButton *event);

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
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_graph_label_finalize;

    widget_class->realize = gwy_graph_label_realize;
    widget_class->expose_event = gwy_graph_label_expose;
    widget_class->size_request = gwy_graph_label_size_request;
    widget_class->unrealize = gwy_graph_label_unrealize;
    widget_class->size_allocate = gwy_graph_label_size_allocate;
    widget_class->button_press_event = gwy_graph_label_button_press;
    widget_class->button_release_event = gwy_graph_label_button_release;

}

static void
gwy_graph_label_init(GwyGraphLabel *label)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    label->is_visible = 1;

    label->par.is_frame = 1;
    label->par.frame_thickness = 1;    
    label->par.sample_length = 20;
    label->par.position = GWY_GRAPH_LABEL_NORTHEAST;
    
}

GtkWidget*
gwy_graph_label_new()
{
    GwyGraphLabel *label;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    label = gtk_type_new (gwy_graph_label_get_type ());
    
    label->par.font = pango_font_description_new();
    pango_font_description_set_family(label->par.font, "Helvetica");
    pango_font_description_set_style(label->par.font, PANGO_STYLE_NORMAL);
    pango_font_description_set_variant(label->par.font, PANGO_VARIANT_NORMAL);
    pango_font_description_set_weight(label->par.font, PANGO_WEIGHT_NORMAL);
    pango_font_description_set_size(label->par.font, 10*PANGO_SCALE);

    gtk_widget_add_events(GTK_WIDGET(label), GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

    label->curve_params = g_ptr_array_new();
    
    return GTK_WIDGET(label);
}

static void
gwy_graph_label_finalize(GObject *object)
{
    GwyGraphLabel *label;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "finalizing a GwyGraphLabel (refcount = %u)",
          object->ref_count);
    #endif

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_GRAPH_LABEL(object));

    label = GWY_GRAPH_LABEL(object);

    G_OBJECT_CLASS(parent_class)->finalize(object);
    g_ptr_array_free(label->curve_params, 0);
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
     
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "realizing a GwyGraphLabel (%ux%u)",
          widget->allocation.width, widget->allocation.height);
    #endif

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

}


static void
gwy_graph_label_size_request(GtkWidget *widget,
                             GtkRequisition *requisition)
{
    GwyGraphLabel *label;
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    label = GWY_GRAPH_LABEL(widget);
    requisition->width = 70;
    requisition->height = 10 + label->curve_params->len * 20;
}

static void
gwy_graph_label_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GwyGraphLabel *label;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

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
    gint xc, yc;
    GdkPoint ps[4];
    
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH_LABEL(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);
#ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
#endif

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


void gwy_graph_label_draw_label(GtkWidget *widget)
{
    gint i, ypos;
    GwyGraphLabel *label;
    PangoLayout *layout;
    PangoRectangle rect;
    GdkGC *mygc;
    GwyGraphAreaCurveParams *cparams;

    mygc = gdk_gc_new(widget->window);

    label = GWY_GRAPH_LABEL(widget);
    layout = gtk_widget_create_pango_layout(widget, "");
    pango_layout_set_font_description(layout, label->par.font);

    ypos = 5;
    for (i=0; i<label->curve_params->len; i++)
    {
        cparams = g_ptr_array_index (label->curve_params, i);
        pango_layout_set_text(layout, cparams->description->str, cparams->description->len);
        gdk_draw_layout(widget->window, mygc, 10, ypos, layout);      
        pango_layout_get_pixel_extents(layout, NULL, &rect);
        ypos += rect.height + 5;
    }

   
    /*pango_layout_set_markup(layout,  label->label_text->str, label->label_text->len);
      */  
    gdk_gc_set_line_attributes (mygc, label->par.frame_thickness,
                  GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
    
    
    gdk_draw_line(widget->window, mygc, 
                  label->par.frame_thickness/2, 
                  label->par.frame_thickness/2, 
                  widget->allocation.width - label->par.frame_thickness/2 - 1, 
                  label->par.frame_thickness/2);
    gdk_draw_line(widget->window, mygc, 
                  label->par.frame_thickness/2, 
                  widget->allocation.height - label->par.frame_thickness/2 - 1, 
                  widget->allocation.width - label->par.frame_thickness/2 - 1, 
                  widget->allocation.height - label->par.frame_thickness/2 - 1);
    gdk_draw_line(widget->window, mygc, 
                  label->par.frame_thickness/2, 
                  label->par.frame_thickness/2, 
                  label->par.frame_thickness/2, 
                  widget->allocation.height - label->par.frame_thickness/2 - 1);
    gdk_draw_line(widget->window, mygc, 
                  widget->allocation.width - label->par.frame_thickness/2 - 1, 
                  label->par.frame_thickness/2, 
                  widget->allocation.width - label->par.frame_thickness/2 - 1, 
                  widget->allocation.height - label->par.frame_thickness/2 - 1);
    g_object_unref((GObject *)mygc);
}    



static gboolean
gwy_graph_label_button_press(GtkWidget *widget,
                             GdkEventButton *event)
{
    GwyGraphLabel *label;
    double x, y;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
	    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH_LABEL(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    label = GWY_GRAPH_LABEL(widget);

    return FALSE;
}

static gboolean
gwy_graph_label_button_release(GtkWidget *widget,
                               GdkEventButton *event)
{
    GwyGraphLabel *label;
    gdouble x, y;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
	
    
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH_LABEL(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    label = GWY_GRAPH_LABEL(widget);


    return FALSE;
}

void
gwy_graph_label_add_curve(GwyGraphLabel *label, GwyGraphAreaCurveParams *params)
{
    GwyGraphAreaCurveParams *cparams;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    cparams = g_new(GwyGraphAreaCurveParams, 1);
    cparams->description = g_string_new(params->description->str);

    cparams->color = params->color;
    cparams->line_style = params->line_style;
    cparams->point_type = params->point_type;
    cparams->point_size = params->point_size;
    cparams->is_point = params->is_point;
    cparams->is_line = params->is_line;
    
    g_ptr_array_add(label->curve_params, (gpointer)(cparams)); 
}

void
gwy_graph_label_clear(GwyGraphLabel *label)
{
    guint i;
    GwyGraphAreaCurveParams *cparams;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    
    for (i=0; i<label->curve_params->len; i++)
    {
        cparams = g_ptr_array_index (label->curve_params, i);
        g_string_free(cparams->description, 1);
    }
    g_ptr_array_free(label->curve_params, 1);
    label->curve_params = g_ptr_array_new();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
