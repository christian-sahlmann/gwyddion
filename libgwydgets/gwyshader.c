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
#define DEBUG 1
#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include "gwyshader.h"

#define GWY_SHADER_TYPE_NAME "GwyShader"

#define GWY_SHADER_DELAY_LENGTH  300
#define BITS_PER_SAMPLE 8
#define SHADER_DEFAULT_SIZE 80

#ifndef HAVE_HYPOT
#define hypot(x, y) sqrt((x)*(x) + (y)*(y))
#endif

enum {
    PROP_0,
    PROP_GRADIENT,
    PROP_UPDATE_POLICY,
    PROP_LAST
};

enum {
    ANGLE_CHANGED,
    LAST_SIGNAL
};
/* Forward declarations */

static void     gwy_shader_class_init     (GwyShaderClass *klass);
static void     gwy_shader_init           (GwyShader *shader);
static void     gwy_shader_finalize       (GObject *object);
static void     gwy_shader_set_property   (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);
static void     gwy_shader_get_property   (GObject*object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec);
static void     gwy_shader_realize        (GtkWidget *widget);
static void     gwy_shader_unrealize      (GtkWidget *widget);
static void     gwy_shader_size_request   (GtkWidget *widget,
                                           GtkRequisition *requisition);
static void     gwy_shader_size_allocate  (GtkWidget *widget,
                                           GtkAllocation *allocation);
static void     gwy_shader_make_pixmap    (GwyShader *shader);
static void     gwy_shader_paint          (GwyShader *shader);
static gboolean gwy_shader_expose         (GtkWidget *widget,
                                           GdkEventExpose *event);
static gboolean gwy_shader_button_press   (GtkWidget *widget,
                                           GdkEventButton *event);
static gboolean gwy_shader_button_release (GtkWidget *widget,
                                           GdkEventButton *event);
static gboolean gwy_shader_motion_notify  (GtkWidget *widget,
                                           GdkEventMotion *event);
static gboolean gwy_shader_timer          (GwyShader *shader);
static void     gwy_shader_update_mouse   (GwyShader *shader,
                                           gint x, gint y);
static void     gwy_shader_update         (GwyShader *shader);


/* Local data */

static GtkWidgetClass *parent_class = NULL;

static guint shader_signals[LAST_SIGNAL] = { 0 };

GType
gwy_shader_get_type(void)
{
    static GType gwy_shader_type = 0;

    if (!gwy_shader_type) {
        static const GTypeInfo gwy_shader_info = {
            sizeof(GwyShaderClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_shader_class_init,
            NULL,
            NULL,
            sizeof(GwyShader),
            0,
            (GInstanceInitFunc)gwy_shader_init,
            NULL,
        };
        gwy_debug("");
        gwy_shader_type = g_type_register_static(GTK_TYPE_WIDGET,
                                                 GWY_SHADER_TYPE_NAME,
                                                 &gwy_shader_info,
                                                 0);
    }

    return gwy_shader_type;
}

static void
gwy_shader_class_init(GwyShaderClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_shader_finalize;
    gobject_class->set_property = gwy_shader_set_property;
    gobject_class->get_property = gwy_shader_get_property;

    widget_class->realize = gwy_shader_realize;
    widget_class->expose_event = gwy_shader_expose;
    widget_class->size_request = gwy_shader_size_request;
    widget_class->unrealize = gwy_shader_unrealize;
    widget_class->size_allocate = gwy_shader_size_allocate;
    widget_class->button_press_event = gwy_shader_button_press;
    widget_class->button_release_event = gwy_shader_button_release;
    widget_class->motion_notify_event = gwy_shader_motion_notify;

    klass->angle_changed = NULL;

    g_object_class_install_property(
        gobject_class,
        PROP_GRADIENT,
        g_param_spec_string("gradient",
                            "Gradient name",
                            "Name of gradient the sphere is colored with",
                            NULL,
                            G_PARAM_READWRITE));
    g_object_class_install_property(
        gobject_class,
        PROP_UPDATE_POLICY,
        g_param_spec_enum("update_policy",
                          "Update Policy",
                          "When value change causes signal emission",
                          GTK_TYPE_UPDATE_TYPE,
                          GTK_UPDATE_CONTINUOUS,
                          G_PARAM_READWRITE));

/**
 * GwyShader::angle_changed:
 * @shader: The #GwyShader which received the signal.
 * @user_data: User data set when the signal handler was connected.
 *
 * The ::angle_changed signal is emitted when the spherical angle changes.
 */
    shader_signals[ANGLE_CHANGED]
        = g_signal_new("angle_changed",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyShaderClass, angle_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_shader_init(GwyShader *shader)
{
    gwy_debug("");
    shader->update_policy = GTK_UPDATE_CONTINUOUS;
}

/**
 * gwy_shader_new:
 * @sphere_coords: The spherical coordinates this gradient sphere should use.
 *
 * Creates a new #GwyShader.
 *
 * @sphere_coords can be %NULL, new spherical coordinates are allocated
 * then.
 *
 * The widget takes up all the space allocated for it.
 *
 * Returns: The new gradient sphere as a #GtkWidget.
 **/
GtkWidget*
gwy_shader_new(const gchar *gradient)
{
    GwyShader *shader;

    gwy_debug("");
    shader = (GwyShader*)g_object_new(GWY_TYPE_SHADER, NULL);

    shader->gradient = gwy_gradients_get_gradient(gradient);
    if (!shader->gradient)
        shader->gradient = gwy_gradients_get_gradient("Gray");
    if (!shader->gradient)
        shader->gradient = gwy_gradients_new_gradient("Gray");
    else
        g_object_ref(shader->gradient);

    shader->gradient_change_id
        = g_signal_connect_swapped(shader->gradient, "value_changed",
                                   G_CALLBACK(gwy_shader_update), shader);

    return (GtkWidget*)shader;
}

static void
gwy_shader_finalize(GObject *object)
{
    GwyShader *shader;

    gwy_debug(" ");
    shader = GWY_SHADER(object);

    g_signal_handler_disconnect(shader->gradient, shader->gradient_change_id);
    gwy_object_unref(shader->gradient);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_shader_unrealize(GtkWidget *widget)
{
    GwyShader *shader;

    shader = GWY_SHADER(widget);

    gwy_debug(" ");
    gwy_object_unref(shader->pixbuf);
    shader->radius = 0;

    if (shader->timer_id) {
        g_source_remove(shader->timer_id);
        shader->timer_id = 0;
    }

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}


static void
gwy_shader_set_property(GObject *object,
                        guint prop_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
    GwyShader *shader = GWY_SHADER(object);

    switch (prop_id) {
        case PROP_GRADIENT:
        gwy_shader_set_gradient(shader, g_value_get_string(value));
        break;

        case PROP_UPDATE_POLICY:
        gwy_shader_set_update_policy(shader, g_value_get_enum(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_shader_get_property(GObject*object,
                        guint prop_id,
                        GValue *value,
                        GParamSpec *pspec)
{
    GwyShader *shader = GWY_SHADER(object);

    switch (prop_id) {
        case PROP_GRADIENT:
        g_value_set_string(value, gwy_gradient_get_name(shader->gradient));
        break;

        case PROP_UPDATE_POLICY:
        g_value_set_enum(value, shader->update_policy);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_shader_get_update_policy:
 * @shader: a #GwyShader.
 *
 * Returns the update policy of a gradient spehere @shader.
 *
 * Returns: The update policy.
 **/
GtkUpdateType
gwy_shader_get_update_policy(GwyShader *shader)
{
    g_return_val_if_fail(GWY_IS_SHADER(shader), 0);

    return shader->update_policy;
}

gdouble
gwy_shader_get_theta(GwyShader *shader)
{
    g_return_val_if_fail(GWY_IS_SHADER(shader), 0.0);

    return shader->theta;
}

gdouble
gwy_shader_get_phi(GwyShader *shader)
{
    g_return_val_if_fail(GWY_IS_SHADER(shader), 0.0);

    return shader->phi;
}

const gchar*
gwy_shader_get_gradient(GwyShader *shader)
{
    g_return_val_if_fail(GWY_IS_SHADER(shader), NULL);

    return gwy_gradient_get_name(shader->gradient);
}

/**
 * gwy_shader_set_update_policy:
 * @shader: a #GwyShader.
 * @update_policy: the update policy a gradient sphere @shader should use.
 *
 * Sets update policy for a gradient sphere.
 **/
void
gwy_shader_set_update_policy(GwyShader *shader,
                             GtkUpdateType update_policy)
{
    g_return_if_fail(GWY_IS_SHADER(shader));
    g_return_if_fail(update_policy >= GTK_UPDATE_CONTINUOUS
                     && update_policy <= GTK_UPDATE_DELAYED);

    shader->update_policy = update_policy;
    /* FIXME: what about pending updates? */
    g_object_notify(G_OBJECT(shader), "update_policy");
}

void
gwy_shader_set_theta(GwyShader *shader,
                     gdouble theta)
{
    g_return_if_fail(GWY_IS_SHADER(shader));

    theta = CLAMP(theta, 0.0, G_PI);
    if (theta == shader->theta)
        return;

    shader->old_theta = shader->theta;
    shader->theta = theta;
    gwy_shader_update(shader);
}

void
gwy_shader_set_phi(GwyShader *shader,
                   gdouble phi)
{
    g_return_if_fail(GWY_IS_SHADER(shader));

    phi = fmod(phi, 2*G_PI);
    if (phi < 0.0)
        phi += 2*G_PI;

    if (phi == shader->phi)
        return;

    shader->old_phi = shader->phi;
    shader->phi = phi;
    gwy_shader_update(shader);
}

void
gwy_shader_set_spherical_angle(GwyShader *shader,
                               gdouble theta,
                               gdouble phi)
{
    g_return_if_fail(GWY_IS_SHADER(shader));

    theta = CLAMP(theta, 0.0, G_PI);
    phi = fmod(phi, 2*G_PI);
    if (phi < 0.0)
        phi += 2*G_PI;

    if (theta == shader->theta && phi == shader->phi)
        return;

    shader->old_theta = shader->theta;
    shader->theta = theta;
    shader->old_phi = shader->phi;
    shader->phi = phi;
    gwy_shader_update(shader);
}

/**
 * gwy_shader_set_gradient:
 * @shader: a #GwyShader.
 * @gradient: A gradient @shader should use.
 *
 * Sets the gradient a gradient sphere uses.
 **/
void
gwy_shader_set_gradient(GwyShader *shader,
                        const gchar *gradient)
{
    GwyGradient *grad, *old;

    g_return_if_fail(GWY_IS_SHADER(shader));

    grad = gwy_gradients_get_gradient(gradient);
    if (!grad || grad == shader->gradient)
        return;

    old = shader->gradient;
    g_signal_handler_disconnect(old, shader->gradient_change_id);
    g_object_ref(grad);
    shader->gradient = grad;
    shader->gradient_change_id
        = g_signal_connect_swapped(shader->gradient, "value_changed",
                                   G_CALLBACK(gwy_shader_update), shader);
    g_object_unref(old);

    gwy_shader_update(shader);
}

static void
gwy_shader_realize(GtkWidget *widget)
{
    GwyShader *shader;
    GdkWindowAttr attributes;
    gint attributes_mask;

    gwy_debug("realizing a GwyShader (%ux%u)",
          widget->allocation.x, widget->allocation.height);

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    shader = GWY_SHADER(widget);

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

    gwy_shader_make_pixmap(shader);
}

static void
gwy_shader_size_request(G_GNUC_UNUSED GtkWidget *widget,
                        GtkRequisition *requisition)
{
    gwy_debug("");

    requisition->width = SHADER_DEFAULT_SIZE;
    requisition->height = SHADER_DEFAULT_SIZE;
}

static void
gwy_shader_size_allocate(GtkWidget *widget,
                         GtkAllocation *allocation)
{
    GwyShader *shader;

    gwy_debug("");
    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(widget)) {
        shader = GWY_SHADER(widget);

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
        gwy_shader_make_pixmap(shader);
    }
}

static void
gwy_shader_make_pixmap(GwyShader *shader)
{
    GtkWidget *widget;
    int radius;

    widget = GTK_WIDGET(shader);

    radius = (MIN(widget->allocation.width, widget->allocation.height) - 1)/2;
    if (radius != shader->radius) {
        shader->radius = radius;
        gwy_object_unref(shader->pixbuf);
        /* FIXME: using clipping would be better than alpha */
        shader->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                        TRUE,
                                        BITS_PER_SAMPLE,
                                        2*radius + 1,
                                        2*radius + 1);
        gwy_debug_objects_creation(G_OBJECT(shader->pixbuf));
        gdk_pixbuf_fill(shader->pixbuf, 0x00000000);
        gwy_shader_paint(shader);
    }
}

static void
gwy_shader_paint(GwyShader *shader)
{
    gint i, j, r2, grad_size;
    gint height, width;
    gdouble sphi, cphi, sth, cth;
    guchar *pixels;
    guint rowstride;
    guint32 *gradient;

    sphi = sin(shader->phi);
    cphi = cos(shader->phi);
    sth = sin(shader->theta);
    cth = cos(shader->theta);
    gradient = (guint32*)gwy_gradient_get_samples(shader->gradient, &grad_size);
    pixels = gdk_pixbuf_get_pixels(shader->pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(shader->pixbuf);

    height = gdk_pixbuf_get_height(shader->pixbuf);
    width = gdk_pixbuf_get_width(shader->pixbuf);
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
    shader->old_theta = shader->theta;
    shader->old_theta = shader->phi;
}

static gboolean
gwy_shader_expose(GtkWidget *widget,
                       GdkEventExpose *event)
{
    GwyShader *shader;
    gint xc, yc, xs, ys, xe, ye;
    GdkRectangle rect;

    shader = GWY_SHADER(widget);

    gdk_region_get_clipbox(event->region, &rect);
    gwy_debug("bbox = %dx%d  at (%d,%d)",
              rect.width, rect.height, rect.x, rect.y);
    xc = (widget->allocation.width - 2*shader->radius - 1)/2;
    yc = (widget->allocation.height - 2*shader->radius - 1)/2;
    xs = MAX(rect.x, xc) - xc;
    ys = MAX(rect.y, yc) - yc;
    xe = MIN(rect.x + rect.width, xc + 2*shader->radius) - xc;
    ye = MIN(rect.y + rect.height, yc + 2*shader->radius) - yc;
    if (xs >= xe || ys >= ye)
        return FALSE;

    if (shader->old_theta != shader->theta || shader->old_phi != shader->phi)
        gwy_shader_paint(shader);

    gdk_draw_pixbuf(widget->window,
                    NULL,
                    shader->pixbuf,
                    xs, ys,
                    xc + xs, yc + ys,
                    xe - xs + 1, ye - ys + 1,
                    GDK_RGB_DITHER_NORMAL,
                    0, 0);

    return FALSE;
}

static gboolean
gwy_shader_button_press(GtkWidget *widget,
                        GdkEventButton *event)
{
    GwyShader *shader;
    double x, y;

    /* React to left button only */
    if (event->button != 1)
        return FALSE;

    shader = GWY_SHADER(widget);

    x = event->x - 0.5*widget->allocation.width;
    y = event->y - 0.5*widget->allocation.height;

    if (!shader->button && hypot(x, y) <= shader->radius) {
        gtk_grab_add(widget);
        shader->button = event->button;
        gwy_shader_update_mouse(shader, event->x, event->y);
    }

    return FALSE;
}

static gboolean
gwy_shader_button_release(GtkWidget *widget,
                          GdkEventButton *event)
{
    GwyShader *shader;

    /* React to left button only */
    if (event->button != 1)
        return FALSE;

    shader = GWY_SHADER(widget);

    gtk_grab_remove(widget);
    shader->button = 0;

    if (shader->update_policy == GTK_UPDATE_DELAYED) {
        g_source_remove(shader->timer_id);
        shader->timer_id = 0;
    }

    if (shader->update_policy != GTK_UPDATE_CONTINUOUS) {
        if (shader->old_theta != shader->theta
            || shader->old_phi != shader->phi)
            g_signal_emit_by_name(shader, "angle_changed");
    }

    return FALSE;
}

static gboolean
gwy_shader_motion_notify(GtkWidget *widget,
                         GdkEventMotion *event)
{
    GwyShader *shader;
    GdkModifierType mods;
    gint x, y;

    gwy_debug("motion event: (%f, %f)", event->x, event->y);

    shader = GWY_SHADER(widget);

    if (!shader->button)
        return FALSE;

    x = event->x;
    y = event->y;

    if (event->is_hint
        || (event->window != widget->window))
        gdk_window_get_pointer(widget->window, &x, &y, &mods);

    if (mods & GDK_BUTTON1_MASK)
        gwy_shader_update_mouse(shader, x, y);

    return FALSE;
}

static gboolean
gwy_shader_timer(GwyShader *shader)
{
    if (shader->update_policy == GTK_UPDATE_DELAYED)
        g_signal_emit_by_name(shader, "angle_changed");

    shader->timer_id = 0;
    return FALSE;
}

static void
gwy_shader_update_mouse(GwyShader *shader, gint x, gint y)
{
    gint xc, yc;
    gdouble r;

    gwy_debug("mouse update: (%d, %d)", x, y);

    xc = GTK_WIDGET(shader)->allocation.width / 2;
    yc = GTK_WIDGET(shader)->allocation.height / 2;

    shader->phi = atan2(yc - y, x - xc);
    if (shader->phi < 0.0)
        shader->phi += 2.0*G_PI;
    r = hypot((double)y - yc, (double)x - xc)/shader->radius;
    if (r >= 1.0)
        shader->theta = G_PI/2.0;
    else
        shader->theta = asin(r);

    if (shader->phi == shader->old_phi && shader->theta == shader->old_theta)
        return;

    gwy_shader_update(shader);

    switch (shader->update_policy) {
        case GTK_UPDATE_CONTINUOUS:
        g_signal_emit_by_name(shader, "angle_changed");
        break;

        case GTK_UPDATE_DELAYED:
        if (shader->timer_id)
            g_source_remove(shader->timer_id);
        shader->timer_id = g_timeout_add(GWY_SHADER_DELAY_LENGTH,
                                         (GSourceFunc)gwy_shader_timer,
                                         shader);
        break;

        case GTK_UPDATE_DISCONTINUOUS:
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
gwy_shader_update(GwyShader *shader)
{
    GtkWidget *widget;

    widget = GTK_WIDGET(shader);
    if (widget->window)
        gdk_window_invalidate_rect(widget->window, NULL, TRUE);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
