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
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include "gwygradsphere.h"

#define GWY_GRAD_SPHERE_TYPE_NAME "GwyGradSphere"

#define GWY_SCROLL_DELAY_LENGTH  300
#define BITS_PER_SAMPLE 8
#define GRAD_SPHERE_DEFAULT_SIZE 80

enum {
    PROP_0,
    PROP_SPHERE_COORDS,
    PROP_UPDATE_POLICY,
    PROP_LAST
};

/* Forward declarations */

static void     gwy_grad_sphere_class_init     (GwyGradSphereClass *klass);
static void     gwy_grad_sphere_init           (GwyGradSphere *grad_sphere);
static void     gwy_grad_sphere_finalize       (GObject *object);
static void     gwy_grad_sphere_set_property   (GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec);
static void     gwy_grad_sphere_get_property   (GObject*object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec);
static void     gwy_grad_sphere_realize        (GtkWidget *widget);
static void     gwy_grad_sphere_unrealize      (GtkWidget *widget);
static void     gwy_grad_sphere_size_request   (GtkWidget *widget,
                                                GtkRequisition *requisition);
static void     gwy_grad_sphere_size_allocate  (GtkWidget *widget,
                                                GtkAllocation *allocation);
static void     gwy_grad_sphere_make_pixmap    (GwyGradSphere *grad_sphere);
static void     gwy_grad_sphere_paint          (GwyGradSphere *grad_sphere);
static gboolean gwy_grad_sphere_expose         (GtkWidget *widget,
                                                GdkEventExpose *event);
static gboolean gwy_grad_sphere_button_press   (GtkWidget *widget,
                                                GdkEventButton *event);
static gboolean gwy_grad_sphere_button_release (GtkWidget *widget,
                                                GdkEventButton *event);
static gboolean gwy_grad_sphere_motion_notify  (GtkWidget *widget,
                                                GdkEventMotion *event);
static gboolean gwy_grad_sphere_timer          (GwyGradSphere *grad_sphere);
static void     gwy_grad_sphere_update_mouse   (GwyGradSphere *grad_sphere,
                                                gint x, gint y);
static void     gwy_grad_sphere_update         (GwyGradSphere *grad_sphere);
static void     gwy_grad_sphere_coords_changed (GwySphereCoords *sphere_coords,
                                                gpointer data);


/* Local data */

static GtkWidgetClass *parent_class = NULL;

GType
gwy_grad_sphere_get_type(void)
{
    static GType gwy_grad_sphere_type = 0;

    if (!gwy_grad_sphere_type) {
        static const GTypeInfo gwy_grad_sphere_info = {
            sizeof(GwyGradSphereClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_grad_sphere_class_init,
            NULL,
            NULL,
            sizeof(GwyGradSphere),
            0,
            (GInstanceInitFunc)gwy_grad_sphere_init,
            NULL,
        };
        gwy_debug("");
        gwy_grad_sphere_type = g_type_register_static(GTK_TYPE_WIDGET,
                                                      GWY_GRAD_SPHERE_TYPE_NAME,
                                                      &gwy_grad_sphere_info,
                                                      0);
    }

    return gwy_grad_sphere_type;
}

static void
gwy_grad_sphere_class_init(GwyGradSphereClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_grad_sphere_finalize;
    gobject_class->set_property = gwy_grad_sphere_set_property;
    gobject_class->get_property = gwy_grad_sphere_get_property;

    widget_class->realize = gwy_grad_sphere_realize;
    widget_class->expose_event = gwy_grad_sphere_expose;
    widget_class->size_request = gwy_grad_sphere_size_request;
    widget_class->unrealize = gwy_grad_sphere_unrealize;
    widget_class->size_allocate = gwy_grad_sphere_size_allocate;
    widget_class->button_press_event = gwy_grad_sphere_button_press;
    widget_class->button_release_event = gwy_grad_sphere_button_release;
    widget_class->motion_notify_event = gwy_grad_sphere_motion_notify;

    g_object_class_install_property(
        gobject_class,
        PROP_SPHERE_COORDS,
        g_param_spec_object("sphere_coords",
                            _("Spherical coordinates"),
                            _("The GwySphereCoords the shpere"),
                            GWY_TYPE_SPHERE_COORDS,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
        gobject_class,
        PROP_UPDATE_POLICY,
        g_param_spec_enum("update_policy",
                          _("Update Policy"),
                          _("When value changed causes signal emission"),
                          GTK_TYPE_UPDATE_TYPE,
                          GTK_UPDATE_CONTINUOUS,
                          G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gwy_grad_sphere_init(GwyGradSphere *grad_sphere)
{
    gwy_debug("");

    grad_sphere->update_policy = GTK_UPDATE_CONTINUOUS;
    grad_sphere->button = 0;
    grad_sphere->timer = 0;
    grad_sphere->radius = -1;
    grad_sphere->phi = 0.0;
    grad_sphere->theta = 0.0;
    grad_sphere->old_phi = 0.0;
    grad_sphere->old_theta = 0.0;
    grad_sphere->sphere_coords = NULL;
    grad_sphere->palette = NULL;
    grad_sphere->sphere_pixbuf = NULL;
}

/**
 * gwy_grad_sphere_new:
 * @sphere_coords: The spherical coordinates this gradient sphere should use.
 *
 * Creates a new #GwyGradSphere.
 *
 * @sphere_coords can be %NULL, new spherical coordinates are allocated
 * then.
 *
 * The widget takes up all the space allocated for it.
 *
 * Returns: The new gradient sphere as a #GtkWidget.
 **/
GtkWidget*
gwy_grad_sphere_new(GwySphereCoords *sphere_coords)
{
    GtkWidget *widget;
    GwyGradSphere *grad_sphere;

    gwy_debug("");

    if (sphere_coords)
        g_return_val_if_fail(GWY_IS_SPHERE_COORDS(sphere_coords), NULL);
    else
        sphere_coords = (GwySphereCoords*)gwy_sphere_coords_new(0.0, 0.0);
    g_object_ref(G_OBJECT(sphere_coords));
    gtk_object_sink(GTK_OBJECT(sphere_coords));

    widget = gtk_widget_new(GWY_TYPE_GRAD_SPHERE,
                            "sphere_coords", sphere_coords,
                            NULL);
    grad_sphere = (GwyGradSphere*)widget;
    g_object_unref(sphere_coords);

    grad_sphere->palette = (GwyPalette*)(gwy_palette_new(NULL));
    gwy_palette_set_by_name(grad_sphere->palette, GWY_PALETTE_GRAY);
    g_signal_connect_swapped(grad_sphere->palette, "value_changed",
                             G_CALLBACK(gwy_grad_sphere_update), grad_sphere);

    return widget;
}

static void
gwy_grad_sphere_finalize(GObject *object)
{
    GwyGradSphere *grad_sphere;

    gwy_debug("finalizing a GwyGradSphere (refcount = %u)",
              object->ref_count);

    g_return_if_fail(GWY_IS_GRAD_SPHERE(object));

    grad_sphere = GWY_GRAD_SPHERE(object);

    gwy_debug("    unreferencing child sphere_coords (refcount = %u)",
              G_OBJECT(grad_sphere->sphere_coords)->ref_count);
    g_signal_handlers_disconnect_matched(grad_sphere->sphere_coords,
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, grad_sphere);
    gwy_object_unref(grad_sphere->sphere_coords);
    g_signal_handlers_disconnect_matched(grad_sphere->palette,
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, grad_sphere);
    gwy_object_unref(grad_sphere->palette);

    G_OBJECT_CLASS(parent_class)->finalize(object);

    gwy_debug("    ... (refcount = %u)", object->ref_count);
}

static void
gwy_grad_sphere_unrealize(GtkWidget *widget)
{
    GwyGradSphere *grad_sphere;

    grad_sphere = GWY_GRAD_SPHERE(widget);

    gwy_debug("    unreferencing child pixbuf (refcount = %u)",
              G_OBJECT(grad_sphere->sphere_pixbuf)->ref_count);
    g_object_unref(grad_sphere->sphere_pixbuf);
    grad_sphere->sphere_pixbuf = NULL;
    grad_sphere->radius = -1;

    if (grad_sphere->timer) {
        gtk_timeout_remove(grad_sphere->timer);
        grad_sphere->timer = 0;
    }

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}


static void
gwy_grad_sphere_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyGradSphere *grad_sphere = GWY_GRAD_SPHERE(object);

    switch (prop_id) {
        case PROP_SPHERE_COORDS:
        gwy_grad_sphere_set_sphere_coords(grad_sphere,
                                          g_value_get_object(value));
        break;

        case PROP_UPDATE_POLICY:
        gwy_grad_sphere_set_update_policy(grad_sphere,
                                          g_value_get_enum(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_grad_sphere_get_property(GObject*object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyGradSphere *grad_sphere = GWY_GRAD_SPHERE(object);

    switch (prop_id) {
        case PROP_SPHERE_COORDS:
        g_value_set_object(value,
                           G_OBJECT(gwy_grad_sphere_get_sphere_coords(grad_sphere)));
        break;

        case PROP_UPDATE_POLICY:
        g_value_set_enum(value,
                         grad_sphere->update_policy);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_grad_sphere_get_update_policy:
 * @grad_sphere: a #GwyGradSphere.
 *
 * Returns the update policy of a gradient spehere @grad_sphere.
 *
 * Returns: The update policy.
 **/
GtkUpdateType
gwy_grad_sphere_get_update_policy(GwyGradSphere *grad_sphere)
{
    g_return_val_if_fail(GWY_IS_GRAD_SPHERE(grad_sphere), 0);

    return grad_sphere->update_policy;
}

/**
 * gwy_grad_sphere_get_sphere_coords:
 * @grad_sphere: a #GwyGradSphere.
 *
 * Returns the spherical coordinates a gradient spehere @grad_sphere uses.
 *
 * Returns: The coordinates.
 **/
GwySphereCoords*
gwy_grad_sphere_get_sphere_coords(GwyGradSphere *grad_sphere)
{
    g_return_val_if_fail(GWY_IS_GRAD_SPHERE(grad_sphere), NULL);

    return grad_sphere->sphere_coords;
}

/**
 * gwy_grad_sphere_set_update_policy:
 * @grad_sphere: a #GwyGradSphere.
 * @update_policy: the update policy a gradient sphere @grad_sphere should use.
 *
 * Sets update policy for a gradient sphere.
 **/
void
gwy_grad_sphere_set_update_policy(GwyGradSphere *grad_sphere,
                                  GtkUpdateType update_policy)
{
    g_return_if_fail(GWY_IS_GRAD_SPHERE(grad_sphere));

    grad_sphere->update_policy = update_policy;
}

/**
 * gwy_grad_sphere_set_sphere_coords:
 * @grad_sphere: a #GwyGradSphere.
 * @sphere_coords: the spherical coordinates this gradient sphere should use.
 *
 * Sets spherical coordinates for a gradient sphere.
 **/
void
gwy_grad_sphere_set_sphere_coords(GwyGradSphere *grad_sphere,
                                  GwySphereCoords *sphere_coords)
{
    GwySphereCoords *old;

    g_return_if_fail(GWY_IS_GRAD_SPHERE(grad_sphere));
    g_return_if_fail(GWY_IS_SPHERE_COORDS(sphere_coords));

    old = grad_sphere->sphere_coords;
    if (old)
         g_signal_handlers_disconnect_matched(old, G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL, NULL, grad_sphere);
    grad_sphere->sphere_coords = sphere_coords;
    g_object_ref(G_OBJECT(sphere_coords));
    gtk_object_sink(GTK_OBJECT(sphere_coords));
    if (old)
         g_object_unref(old);

    g_signal_connect(G_OBJECT(sphere_coords), "value_changed",
                     G_CALLBACK(gwy_grad_sphere_coords_changed),
                     grad_sphere);

    grad_sphere->old_phi = sphere_coords->phi;
    grad_sphere->old_theta = sphere_coords->theta;

    gwy_grad_sphere_update(grad_sphere);
}

static void
gwy_grad_sphere_realize(GtkWidget *widget)
{
    GwyGradSphere *grad_sphere;
    GdkWindowAttr attributes;
    gint attributes_mask;

    gwy_debug("realizing a GwyGradSphere (%ux%u)",
          widget->allocation.x, widget->allocation.height);

    g_return_if_fail(GWY_IS_GRAD_SPHERE(widget));

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    grad_sphere = GWY_GRAD_SPHERE(widget);

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
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    gwy_grad_sphere_make_pixmap(grad_sphere);
}

static void
gwy_grad_sphere_size_request(G_GNUC_UNUSED GtkWidget *widget,
                             GtkRequisition *requisition)
{
    gwy_debug("");

    requisition->width = GRAD_SPHERE_DEFAULT_SIZE;
    requisition->height = GRAD_SPHERE_DEFAULT_SIZE;
}

static void
gwy_grad_sphere_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GwyGradSphere *grad_sphere;

    gwy_debug("");

    g_return_if_fail(GWY_IS_GRAD_SPHERE(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(widget)) {
        grad_sphere = GWY_GRAD_SPHERE(widget);

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
        gwy_grad_sphere_make_pixmap(grad_sphere);
    }
}

static void
gwy_grad_sphere_make_pixmap(GwyGradSphere *grad_sphere)
{
    GtkWidget *widget;
    int radius;

    widget = GTK_WIDGET(grad_sphere);

    radius = 0.45 * MIN(widget->allocation.width, widget->allocation.height);
    if (radius != grad_sphere->radius) {
        grad_sphere->radius = radius;
        if (grad_sphere->sphere_pixbuf)
            g_object_unref(grad_sphere->sphere_pixbuf);
        /* FIXME: using clipping would be better than alpha */
        grad_sphere->sphere_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                                    TRUE,
                                                    BITS_PER_SAMPLE,
                                                    2*radius + 1,
                                                    2*radius + 1);
        gdk_pixbuf_fill(grad_sphere->sphere_pixbuf, 0x00000000);
        gwy_grad_sphere_paint(grad_sphere);
    }
}

static void
gwy_grad_sphere_paint(GwyGradSphere *grad_sphere)
{
    gint i, j, r2, grad_size;
    gint height, width;
    gdouble sphi, cphi, sth, cth;
    guchar *pixels;
    guint rowstride;
    guint32 *gradient;

    sphi = sin(grad_sphere->phi);
    cphi = cos(grad_sphere->phi);
    sth = sin(grad_sphere->theta);
    cth = cos(grad_sphere->theta);
    gradient = (guint32*)gwy_palette_get_samples(grad_sphere->palette,
                                                 &grad_size);
    pixels = gdk_pixbuf_get_pixels(grad_sphere->sphere_pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(grad_sphere->sphere_pixbuf);

    height = gdk_pixbuf_get_height(grad_sphere->sphere_pixbuf);
    width = gdk_pixbuf_get_width(grad_sphere->sphere_pixbuf);
    r2 = MIN(width, height);
    for (i = 0; i < height; i++) {
        gint i2 = 2*i + 1;
        double q = sqrt((gdouble)((i2)*(2*r2 - i2)));
        gint j_low = ceil(0.5*(width - 1 - q));
        gint j_hi = floor(0.5*(width - 1 + q));
        gdouble y = 1.0 - (gdouble)i2/r2;
        guchar *row = pixels + i*rowstride + 4*j_low;

        for (j = j_low; j <= j_hi; j++) {
            gint j2 = 2*j + 1;
            gdouble x = (gdouble)j2/r2 - 1.0;
            gdouble z = (x*cphi + y*sphi) * sth
                        + sqrt(1.0 - x*x - y*y) * cth;
            gint v = (grad_size - 0.001)*(0.5*z + 0.5);

            *(guint32*)row = gradient[v];
            row += 4;
        }
    }
}

static gboolean
gwy_grad_sphere_expose(GtkWidget *widget,
                       GdkEventExpose *event)
{
    GwyGradSphere *grad_sphere;
    gint xc, yc;

    g_return_val_if_fail(GWY_IS_GRAD_SPHERE(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    grad_sphere = GWY_GRAD_SPHERE(widget);

    /* needed?
    gdk_window_clear_area(widget->window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);
    */

    if (grad_sphere->old_theta != grad_sphere->theta
        || grad_sphere->old_phi != grad_sphere->phi) {
        gwy_grad_sphere_paint(grad_sphere);
        grad_sphere->old_theta = grad_sphere->theta;
        grad_sphere->old_theta = grad_sphere->phi;
    }

    xc = (widget->allocation.width - 2*grad_sphere->radius - 1)/2;
    yc = (widget->allocation.height - 2*grad_sphere->radius - 1)/2;
    /* TODO: draw only intersection */
    gdk_draw_pixbuf(widget->window,
                    NULL,
                    grad_sphere->sphere_pixbuf,
                    0, 0,
                    xc, yc,
                    -1, -1,
                    GDK_RGB_DITHER_NORMAL,
                    0, 0);

    return FALSE;
}

static gboolean
gwy_grad_sphere_button_press(GtkWidget *widget,
                             GdkEventButton *event)
{
    GwyGradSphere *grad_sphere;
    double x, y;

    g_return_val_if_fail(GWY_IS_GRAD_SPHERE(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    grad_sphere = GWY_GRAD_SPHERE(widget);

    /* Determine if button press was within the sphere */

    x = event->x - 0.5*widget->allocation.width;
    y = event->y - 0.5*widget->allocation.height;

    if (!grad_sphere->button && hypot(x, y) <= grad_sphere->radius) {
        gtk_grab_add(widget);
        grad_sphere->button = event->button;
        gwy_grad_sphere_update_mouse(grad_sphere, event->x, event->y);
    }

    return FALSE;
}

static gboolean
gwy_grad_sphere_button_release(GtkWidget *widget,
                               GdkEventButton *event)
{
    GwyGradSphere *grad_sphere;

    g_return_val_if_fail(GWY_IS_GRAD_SPHERE(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    grad_sphere = GWY_GRAD_SPHERE(widget);

    if (grad_sphere->button == event->button) {
        gtk_grab_remove(widget);
        grad_sphere->button = 0;

        if (grad_sphere->update_policy == GTK_UPDATE_DELAYED) {
            gtk_timeout_remove(grad_sphere->timer);
            grad_sphere->timer = 0;
        }

        if (grad_sphere->update_policy != GTK_UPDATE_CONTINUOUS) {
            if (grad_sphere->old_theta != grad_sphere->sphere_coords->theta
                || grad_sphere->old_phi != grad_sphere->sphere_coords->phi)
                g_signal_emit_by_name(grad_sphere->sphere_coords,
                                      "value_changed");
        }
    }

    return FALSE;
}

static gboolean
gwy_grad_sphere_motion_notify(GtkWidget *widget,
                              GdkEventMotion *event)
{
    GwyGradSphere *grad_sphere;
    GdkModifierType mods;
    gint x, y, mask;

    gwy_debug("motion event: (%f, %f)", event->x, event->y);
    g_return_val_if_fail(GWY_IS_GRAD_SPHERE(widget), FALSE);
    g_return_val_if_fail(event, FALSE);

    grad_sphere = GWY_GRAD_SPHERE(widget);

    if (grad_sphere->button != 0) {
        x = event->x;
        y = event->y;

        if (event->is_hint
            || (event->window != widget->window))
            gdk_window_get_pointer(widget->window, &x, &y, &mods);

        switch (grad_sphere->button) {
            case 1:
            mask = GDK_BUTTON1_MASK;
            break;

            case 2:
            mask = GDK_BUTTON2_MASK;
            break;

            case 3:
            mask = GDK_BUTTON3_MASK;
            break;

            default:
            mask = 0;
            break;
        }

        if (mods & mask)
            gwy_grad_sphere_update_mouse(grad_sphere, x, y);
    }

    return FALSE;
}

static gboolean
gwy_grad_sphere_timer(GwyGradSphere *grad_sphere)
{
    g_return_val_if_fail(GWY_IS_GRAD_SPHERE(grad_sphere), FALSE);

    if (grad_sphere->update_policy == GTK_UPDATE_DELAYED)
        g_signal_emit_by_name(grad_sphere->sphere_coords,
                              "value_changed");

    grad_sphere->timer = 0;
    return FALSE;
}

static void
gwy_grad_sphere_update_mouse(GwyGradSphere *grad_sphere, gint x, gint y)
{
    gint xc, yc;
    gdouble old_phi, old_theta, r;

    gwy_debug("mouse update: (%f, %f)", x, y);
    g_return_if_fail(GWY_IS_GRAD_SPHERE(grad_sphere));

    xc = GTK_WIDGET(grad_sphere)->allocation.width / 2;
    yc = GTK_WIDGET(grad_sphere)->allocation.height / 2;

    old_phi = grad_sphere->sphere_coords->phi;
    old_theta = grad_sphere->sphere_coords->theta;

    grad_sphere->phi = atan2(yc - y, x - xc);
    if (grad_sphere->phi < 0.0)
        grad_sphere->phi += 2.0*G_PI;
    r = hypot((double)y - yc, (double)x - xc)/grad_sphere->radius;
    if (r >= 1.0)
        grad_sphere->theta = G_PI/2.0;
    else
        grad_sphere->theta = asin(r);

    grad_sphere->sphere_coords->phi = grad_sphere->phi;
    grad_sphere->sphere_coords->theta = grad_sphere->theta;

    if (grad_sphere->sphere_coords->phi != old_phi
        || grad_sphere->sphere_coords->theta != old_theta) {
        gtk_widget_queue_draw_area(GTK_WIDGET(grad_sphere),
                                   GTK_WIDGET(grad_sphere)->allocation.x,
                                   GTK_WIDGET(grad_sphere)->allocation.y,
                                   GTK_WIDGET(grad_sphere)->allocation.width,
                                   GTK_WIDGET(grad_sphere)->allocation.height);


        switch (grad_sphere->update_policy) {
            case GTK_UPDATE_CONTINUOUS:
            g_signal_emit_by_name(grad_sphere->sphere_coords, "value_changed");
            break;

            case GTK_UPDATE_DELAYED:
            if (grad_sphere->timer)
                gtk_timeout_remove(grad_sphere->timer);
            grad_sphere->timer
                = gtk_timeout_add(GWY_SCROLL_DELAY_LENGTH,
                                  (GtkFunction)gwy_grad_sphere_timer,
                                  grad_sphere);
            break;

            case GTK_UPDATE_DISCONTINUOUS:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }
}

static void
gwy_grad_sphere_update(GwyGradSphere *grad_sphere)
{
    GtkWidget *widget;

    g_return_if_fail(grad_sphere != NULL);
    g_return_if_fail(GWY_IS_GRAD_SPHERE(grad_sphere));

    grad_sphere->theta = grad_sphere->sphere_coords->theta;
    grad_sphere->phi = grad_sphere->sphere_coords->phi;

    widget = GTK_WIDGET(grad_sphere);
    if (widget->window)
        gdk_window_invalidate_rect(widget->window,
                                   NULL,
                                   TRUE);
}

static void
gwy_grad_sphere_coords_changed(GwySphereCoords *sphere_coords,
                               gpointer data)
{
    GwyGradSphere *grad_sphere;

    g_return_if_fail(sphere_coords != NULL);
    g_return_if_fail(data != NULL);

    grad_sphere = GWY_GRAD_SPHERE(data);

    if (grad_sphere->theta != sphere_coords->theta
        || grad_sphere->old_phi != sphere_coords->phi)
        gwy_grad_sphere_update(grad_sphere);
}

GwyPalette*
gwy_grad_sphere_get_palette(GwyGradSphere *grad_sphere)
{
    g_return_val_if_fail(grad_sphere, NULL);

    return grad_sphere->palette;
}

void
gwy_grad_sphere_set_palette(GwyGradSphere *grad_sphere,
                            GwyPalette *palette)
{
    GwyPalette *old;

    g_return_if_fail(GWY_IS_GRAD_SPHERE(grad_sphere));
    g_return_if_fail(GWY_IS_PALETTE(palette));

    old = grad_sphere->palette;
    g_signal_handlers_disconnect_matched(old, G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, grad_sphere);
    g_object_ref(palette);
    grad_sphere->palette = palette;
    g_signal_connect_swapped(grad_sphere->palette, "value_changed",
                             G_CALLBACK(gwy_grad_sphere_update), grad_sphere);
    g_object_unref(old);

    gwy_grad_sphere_update(grad_sphere);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
