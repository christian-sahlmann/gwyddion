#include <stdio.h>
#include <string.h>
#include "gwyserializable.h"
#include "gwycontainer.h"
#include "gwytestser.h"

#define FILENAME "testser.object"

#define print(obj,txt) \
    g_message("%s %s: theta = %g, radius = %g", \
              g_type_name(G_TYPE_FROM_INSTANCE(G_OBJECT(obj))), txt, \
              gwy_test_ser_get_theta(GWY_TEST_SER(obj)), \
              gwy_test_ser_get_radius(GWY_TEST_SER(obj))); \
    gwy_test_ser_print_history(GWY_TEST_SER(obj))


void
foo_callback(gpointer obj, gpointer data __attribute__((unused)))
{
    g_message("FOO! %s", g_type_name(G_TYPE_FROM_INSTANCE(obj)));
}

int
main(int argc, char *argv[])
{
    GObject *ser;
    gsize size, pos;
    guchar *buffer;
    FILE *fh;
    GError *err = NULL;
    GValue val, *p, *p1;
    GObject *container;
    GQuark q;
    gulong id;

    g_type_init();

    /* create, write and free an object */
    ser = gwy_test_ser_new(0.42, 1.001);
    g_message("===== SERIALIZABLE INTERFACE ========================");
    print(ser, "created");

    size = 0;
    buffer = NULL;
    buffer = gwy_serializable_serialize(ser, buffer, &size);
    g_message("size of first object: %u", size);
    g_object_unref(ser);

    /* create, write and free another object */
    ser = gwy_test_ser_new(0.52, 2.002);
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 72.27);
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 666.0);
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 100.0);
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 1e44);
    print(ser, "created");

    buffer = gwy_serializable_serialize(ser, buffer, &size);
    g_message("size of both objects: %u", size);
    g_object_unref(ser);
    g_message("writing objects to %s", FILENAME);
    fh = fopen(FILENAME, "wb");
    fwrite(buffer, 1, size, fh);
    fclose(fh);
    g_free(buffer);

    /* create and free yet another object just to overwrite memory */
    ser = gwy_test_ser_new(1.618, 0.33333333);
    g_object_unref(ser);

    /* read the object back and restore it */
    g_message("reading objects from %s", FILENAME);
    g_file_get_contents(FILENAME, (gchar**)&buffer, &size, &err);
    g_message("size = %u", size);

    pos = 0;
    g_message("restoring the first one");
    ser = gwy_serializable_deserialize(buffer, size, &pos);
    print(ser, "restored");
    g_object_unref(ser);

    g_message("restoring the second one");
    ser = gwy_serializable_deserialize(buffer, size, &pos);
    print(ser, "restored");

    g_message("===== WATCHABLE INTERFACE ===========================");
    id = g_signal_connect(ser, "value_changed", G_CALLBACK(foo_callback), NULL);
    gwy_watchable_value_changed(ser);
    gwy_test_ser_set_theta(GWY_TEST_SER(ser), 0.1111);
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 33.4455);
    gwy_watchable_value_changed(ser);
    g_signal_handler_disconnect(ser, id);

    g_message("====== CONTAINER ====================================");
    container = gwy_container_new();
    p = g_new0(GValue, 1);
    g_value_init(p, G_TYPE_INT);
    g_value_set_int(p, 1133);
    q = g_quark_from_string("foobar");
    gwy_container_set_value_by_name(GWY_CONTAINER(container),
                                    "foobar", p, NULL);
    g_value_set_int(p, -100);
    gwy_container_set_value_by_name(GWY_CONTAINER(container),
                                    "foobar", p, NULL);
    val = gwy_container_get_value(GWY_CONTAINER(container), q);
    g_message("(value) 'foobar' -> %d", g_value_get_int(&val));
    g_message("(int32) 'foobar' -> %d",
                gwy_container_get_int32(GWY_CONTAINER(container), q));
    gwy_container_set_double(GWY_CONTAINER(container), q, 1.13);
    val = gwy_container_get_value(GWY_CONTAINER(container), q);
    g_message("(value) 'foobar' -> %g", g_value_get_double(&val));
    g_message("this should fail:");
    g_message("(int32) 'foobar' -> %d",
                gwy_container_get_int32(GWY_CONTAINER(container), q));

    g_message("====== CONTAINER SERIALIZATION ======================");
    gwy_container_set_double_by_name(GWY_CONTAINER(container), "pdf", 0.5227);
    gwy_container_set_double_by_name(GWY_CONTAINER(container), "pdf/f", 1.4142);
    gwy_container_set_int64_by_name(GWY_CONTAINER(container), "x64", 64LL);

    g_assert(G_OBJECT(ser)->ref_count == 1);
    gwy_container_set_object_by_name(GWY_CONTAINER(container), "ser", ser);
    g_assert(G_OBJECT(ser)->ref_count == 2);
    gwy_container_set_object_by_name(GWY_CONTAINER(container), "ser", ser);
    g_assert(G_OBJECT(ser)->ref_count == 2);
    ser = gwy_container_get_object_by_name(GWY_CONTAINER(container), "ser");
    g_assert(G_OBJECT(ser)->ref_count == 2);

    size = 0;
    buffer = NULL;
    buffer = gwy_serializable_serialize(container, buffer, &size);
    g_object_unref(container);
    g_assert(G_OBJECT(ser)->ref_count == 1);
    g_object_unref(ser);

    fh = fopen(FILENAME, "wb");
    fwrite(buffer, 1, size, fh);
    fclose(fh);
    g_free(buffer);

    g_message("reading objects from %s", FILENAME);
    g_file_get_contents(FILENAME, (gchar**)&buffer, &size, &err);
    g_message("size = %u", size);

    pos = 0;
    g_message("restoring container");
    container = gwy_serializable_deserialize(buffer, size, &pos);

    g_message("'pdf/f' -> %g",
              gwy_container_get_double_by_name(GWY_CONTAINER(container),
                                               "pdf/f"));

    ser = gwy_container_get_object_by_name(GWY_CONTAINER(container), "ser");
    g_object_unref(ser);
    g_assert(G_OBJECT(ser)->ref_count == 1);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
