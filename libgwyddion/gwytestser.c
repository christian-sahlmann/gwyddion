#include <string.h>
#include <stdio.h>
#include <glib-object.h>

#include "gwytestser.h"

#define GWY_TEST_SER_TYPE_NAME "GwyTestSer"

static void       gwy_test_ser_class_init        (GwySerializableClass *klass);
static void       gwy_test_ser_init              (GwyTestSer *test_ser);
static void       gwy_test_ser_finalize          (GwyTestSer *test_ser);
static void       gwy_test_ser_serializable_init (gpointer giface);
static void       gwy_test_ser_watchable_init    (gpointer giface);
static guchar*    gwy_test_ser_serialize         (GObject *obj,
                                                  guchar *buffer,
                                                  gsize *size);
static GObject*   gwy_test_ser_deserialize       (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
static void       gwy_test_ser_value_changed     (GObject *test_ser);

GType
gwy_test_ser_get_type(void)
{
    static GType gwy_test_ser_type = 0;

    if (!gwy_test_ser_type) {
        static const GTypeInfo gwy_test_ser_info = {
            sizeof(GwyTestSerClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_test_ser_class_init,
            NULL,
            NULL,
            sizeof(GwyTestSer),
            0,
            (GInstanceInitFunc)gwy_test_ser_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_test_ser_serializable_init,
            NULL,
            GUINT_TO_POINTER(42)
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_test_ser_watchable_init,
            NULL,
            GUINT_TO_POINTER(37)
        };

        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        gwy_test_ser_type = g_type_register_static(G_TYPE_OBJECT,
                                                   GWY_TEST_SER_TYPE_NAME,
                                                   &gwy_test_ser_info,
                                                   0);
        g_type_add_interface_static(gwy_test_ser_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_test_ser_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_test_ser_type;
}

static void
gwy_test_ser_serializable_init(gpointer giface)
{
    GwySerializableClass *iface = giface;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    /* initialize stuff */
    iface->serialize = gwy_test_ser_serialize;
    iface->deserialize = gwy_test_ser_deserialize;
}

static void
gwy_test_ser_watchable_init(gpointer giface)
{
    GwyWatchableClass *iface = giface;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_WATCHABLE);

    /* initialize stuff */
    iface->value_changed = NULL; /*gwy_test_ser_value_changed;*/
}

static void
gwy_test_ser_class_init(GwySerializableClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    gobject_class->finalize = gwy_test_ser_finalize;
}

static void
gwy_test_ser_init(GwyTestSer *test_ser)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    test_ser->radius = NULL;
    test_ser->history_size = 0;
    test_ser->theta = 0.0;
}

static void
gwy_test_ser_finalize(GwyTestSer *test_ser)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_free(test_ser->radius);
}

GObject*
gwy_test_ser_new(gdouble theta,
                 gdouble radius)
{
    GwyTestSer *test_ser;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    test_ser = g_object_new(GWY_TYPE_TEST_SER, NULL);

    test_ser->theta = theta;
    gwy_test_ser_set_radius(test_ser, 1.0);

    return (GObject*)(test_ser);
}

static guchar*
gwy_test_ser_serialize(GObject *obj,
                       guchar *buffer,
                       gsize *size)
{
    GwyTestSer *test_ser;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(GWY_IS_TEST_SER(obj), NULL);

    test_ser = GWY_TEST_SER(obj);
    return gwy_serialize_pack(buffer, size, "sdD",
                              GWY_TEST_SER_TYPE_NAME,
                              test_ser->theta,
                              test_ser->history_size,
                              test_ser->radius);

}

static GObject*
gwy_test_ser_deserialize(const guchar *stream,
                         gsize size,
                         gsize *position)
{
    gsize pos, hsize;
    double theta, phi, *radius;
    GwyTestSer *test_ser;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(stream, NULL);

    pos = gwy_serialize_check_string(stream + *position, size - *position,
                                     GWY_TEST_SER_TYPE_NAME);
    g_return_val_if_fail(pos, NULL);
    *position += pos;

    theta = gwy_serialize_unpack_double(stream, size, position);
    radius = gwy_serialize_unpack_double_array(stream, size, position, &hsize);

    test_ser = (GwyTestSer*)gwy_test_ser_new(theta, 0.0);
    g_free(test_ser->radius);
    test_ser->radius = radius;
    test_ser->history_size = hsize;

    return (GObject*)test_ser;
}

void
gwy_test_ser_set_radius(GwyTestSer *test_ser,
                        gdouble radius)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    if (!test_ser->history_size
        || radius != test_ser->radius[test_ser->history_size - 1]) {
        test_ser->history_size++;
        test_ser->radius = g_renew(gdouble, test_ser->radius,
                                   test_ser->history_size);
        test_ser->radius[test_ser->history_size - 1] = radius;

        gwy_test_ser_value_changed(G_OBJECT(test_ser));
    }
}

void
gwy_test_ser_set_theta(GwyTestSer *test_ser,
                       gdouble theta)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    if (theta != test_ser->theta) {
        test_ser->theta = theta;

        gwy_test_ser_value_changed(G_OBJECT(test_ser));
    }
}

gdouble
gwy_test_ser_get_radius(GwyTestSer *test_ser)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    return test_ser->radius[test_ser->history_size - 1];
}

gdouble
gwy_test_ser_get_theta(GwyTestSer *test_ser)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    return test_ser->theta;
}

static void
gwy_test_ser_value_changed(GObject *test_ser)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "signal: GwyTestSer changed");
    #endif
    g_signal_emit_by_name(GWY_TEST_SER(test_ser), "value_changed", NULL);
}

void
gwy_test_ser_print_history(GwyTestSer *test_ser)
{
    gsize i;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    fprintf(stderr, "** Message: radius history:");
    for (i = 0; i < test_ser->history_size; i++)
        fprintf(stderr, " %g", test_ser->radius[i]);
    fputs("\n", stderr);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
