#include <math.h>
#include <gtk/gtkmarshal.h>

#include "gwyspherecoords.h"

#define GWY_SPHERE_COORDS_TYPE_NAME "GwySphereCoords"

#define _(x) x

enum {
    VALUE_CHANGED,
    LAST_SIGNAL
};

static guint gwy_sphere_coords_signals[LAST_SIGNAL] = { 0 };

static void gwy_sphere_coords_class_init (GwySphereCoordsClass *klass);
static void gwy_sphere_coords_init       (GwySphereCoords      *sphere_coords);

GType
gwy_sphere_coords_get_type(void)
{
    static GType gwy_sphere_coords_type = 0;

    if (!gwy_sphere_coords_type) {
        static const GTypeInfo gwy_sphere_coords_info = {
            sizeof(GwySphereCoordsClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_sphere_coords_class_init,
            NULL,
            NULL,
            sizeof(GwySphereCoords),
            0,
            (GInstanceInitFunc)gwy_sphere_coords_init,
            NULL,
        };

        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
        gwy_sphere_coords_type = g_type_register_static(GTK_TYPE_OBJECT,
                                                        GWY_SPHERE_COORDS_TYPE_NAME,
                                                        &gwy_sphere_coords_info,
                                                        0);
    }

    return gwy_sphere_coords_type;
}

static void
gwy_sphere_coords_class_init(GwySphereCoordsClass *klass)
{
    klass->value_changed = NULL;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    gwy_sphere_coords_signals[VALUE_CHANGED] =
        g_signal_new("value_changed",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                     G_STRUCT_OFFSET(GwySphereCoordsClass, value_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void
gwy_sphere_coords_init(GwySphereCoords *sphere_coords)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    sphere_coords->theta = 0.0;
    sphere_coords->phi = 0.0;
}

GtkObject*
gwy_sphere_coords_new(gdouble theta,
                      gdouble phi)
{
    GwySphereCoords *sphere_coords;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    sphere_coords = g_object_new(GWY_TYPE_SPHERE_COORDS, NULL);

    sphere_coords->theta = theta;
    sphere_coords->phi = phi;

    return (GtkObject*)(sphere_coords);
}

gdouble
gwy_sphere_coords_get_theta(GwySphereCoords *sphere_coords)
{
    g_return_val_if_fail(GWY_IS_SPHERE_COORDS(sphere_coords), 0.0);

    return sphere_coords->theta;
}

gdouble
gwy_sphere_coords_get_phi(GwySphereCoords *sphere_coords)
{
    g_return_val_if_fail(GWY_IS_SPHERE_COORDS(sphere_coords), 0.0);

    return sphere_coords->phi;
}

void
gwy_sphere_coords_set_value(GwySphereCoords *sphere_coords,
                            gdouble theta,
                            gdouble phi)
{
    g_return_if_fail(GWY_IS_SPHERE_COORDS(sphere_coords));

    theta = CLAMP(theta, 0.0, M_PI);
    phi = fmod(phi, 2.0*M_PI);
    if (phi < 0.0)
        phi += 2.0*M_PI;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "setting GwySphereCoords to %f %f",
          theta, phi);
    #endif
    if (theta != sphere_coords->theta || phi != sphere_coords->phi) {
        sphere_coords->theta = theta;
        sphere_coords->phi = phi;

        gwy_sphere_coords_value_changed(sphere_coords);
    }
}

void
gwy_sphere_coords_value_changed(GwySphereCoords *sphere_coords)
{
    g_return_if_fail(GWY_IS_SPHERE_COORDS(sphere_coords));

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "signal: GwySphereCoords changed");
    #endif
    g_signal_emit(sphere_coords, gwy_sphere_coords_signals[VALUE_CHANGED], 0);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
