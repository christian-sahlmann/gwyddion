#include <math.h>

#include <libgwyddion/gwywatchable.h>
#include <libgwyddion/gwyserializable.h>
#include "gwyspherecoords.h"

#define GWY_SPHERE_COORDS_TYPE_NAME "GwySphereCoords"

#define _(x) x
#undef DEBUG

static void     gwy_sphere_coords_class_init        (GwySphereCoordsClass *klass);
static void     gwy_sphere_coords_init              (GwySphereCoords *sphere_coords);
static void     gwy_sphere_coords_serializable_init (gpointer giface);
static void     gwy_sphere_coords_watchable_init    (gpointer giface);
static guchar*  gwy_sphere_coords_serialize         (GObject *obj,
                                                     guchar *buffer,
                                                     gsize *size);
static GObject* gwy_sphere_coords_deserialize       (const guchar *buffer,
                                                     gsize size,
                                                     gsize *position);
void puts(const char*);
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

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_sphere_coords_serializable_init, NULL, 0
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_sphere_coords_watchable_init, NULL, 0
        };

        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
        gwy_sphere_coords_type = g_type_register_static(GTK_TYPE_OBJECT,
                                                        GWY_SPHERE_COORDS_TYPE_NAME,
                                                        &gwy_sphere_coords_info,
                                                        0);
        g_type_add_interface_static(gwy_sphere_coords_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_sphere_coords_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_sphere_coords_type;
}

static void
gwy_sphere_coords_serializable_init(gpointer giface)
{
    GwySerializableClass *iface = giface;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    iface->serialize = gwy_sphere_coords_serialize;
    iface->deserialize = gwy_sphere_coords_deserialize;
}

static void
gwy_sphere_coords_watchable_init(gpointer giface)
{
    GwyWatchableClass *iface = giface;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_WATCHABLE);

    iface->value_changed = NULL;
}

static void
gwy_sphere_coords_class_init(GwySphereCoordsClass *klass)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
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

/**
 * gwy_sphere_coords_new:
 * @theta: The angle from sphere north pole, in radians.
 * @phi: The angle from sphere zero meridian, in radians.
 *
 * Creates a new spherical coordinates.
 *
 * Returns: New spherical coordinates as a #GtkObject.
 **/
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

/**
 * gwy_sphere_coords_get_theta:
 * @sphere_coords: A #GwySphereCoords.
 *
 * Returns the theta angle i.e., angle from sphere north pole, in radians.
 *
 * Returns: The theta angle.
 **/
gdouble
gwy_sphere_coords_get_theta(GwySphereCoords *sphere_coords)
{
    g_return_val_if_fail(GWY_IS_SPHERE_COORDS(sphere_coords), 0.0);

    return sphere_coords->theta;
}

/**
 * gwy_sphere_coords_get_phi:
 * @sphere_coords: A #GwySphereCoords.
 *
 * Returns the phi angle i.e., angle from sphere zero meridian, in radians.
 *
 * Returns: The phi angle.
 **/
gdouble
gwy_sphere_coords_get_phi(GwySphereCoords *sphere_coords)
{
    g_return_val_if_fail(GWY_IS_SPHERE_COORDS(sphere_coords), 0.0);

    return sphere_coords->phi;
}

/**
 * gwy_sphere_coords_set_value:
 * @sphere_coords: A #GwySphereCoords.
 * @theta: The angle from sphere north pole, in radians.
 * @phi: The angle from sphere zero meridian, in radians.
 *
 * Sets the spherical coordinates to specified values.
 *
 * Emits a "value_changed" signal on @sphere_coords if the coordinates
 * actually changed.
 **/
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

        gwy_watchable_value_changed(G_OBJECT(sphere_coords));
    }
}

static guchar*
gwy_sphere_coords_serialize(GObject *obj,
                            guchar *buffer,
                            gsize *size)
{
    GwySphereCoords *sphere_coords;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(GWY_IS_SPHERE_COORDS(obj), NULL);

    sphere_coords = GWY_SPHERE_COORDS(obj);
    {
        GwySerializeSpec spec[] = {
            { 'd', "theta", &sphere_coords->theta, NULL },
            { 'd', "phi", &sphere_coords->phi, NULL },
        };
        return gwy_serialize_pack_object_struct(buffer, size,
                                                GWY_SPHERE_COORDS_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_sphere_coords_deserialize(const guchar *buffer,
                              gsize size,
                              gsize *position)
{
    gdouble theta, phi;
    gsize pos, mysize;
    GwySerializeSpec spec[] = {
        { 'd', "theta", &theta, NULL },
        { 'd', "phi", &phi, NULL },
    };

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_SPHERE_COORDS_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec))
        return NULL;

    return (GObject*)gwy_sphere_coords_new(theta, phi);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
