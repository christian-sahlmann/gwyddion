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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gwyddion.h"
#include "gwytestser.h"

#define FILENAME "testser.object"

#define print(obj,txt) \
    g_message("%s %s: theta = %g, radius = %g", \
              g_type_name(G_TYPE_FROM_INSTANCE(G_OBJECT(obj))), txt, \
              gwy_test_ser_get_theta(GWY_TEST_SER(obj)), \
              gwy_test_ser_get_radius(GWY_TEST_SER(obj))); \
    gwy_test_ser_print_history(GWY_TEST_SER(obj))

#define pent(txt) \
    { \
        gchar *_t = gwy_entities_text_to_utf8(txt); \
        printf("%s-Message: `%s' -> `%s'\n", G_LOG_DOMAIN, txt, _t); \
        g_free(_t); \
    }

#define linprint(x,e,s) \
    { \
        gsize _i; \
        printf("%s-Message: %s [", G_LOG_DOMAIN, s); \
        for (_i = 0; _i < G_N_ELEMENTS(e); _i++) \
            printf("%s%g", _i == 0 ? "" : " ", x[_i]); \
        printf("] == ["); \
        for (_i = 0; _i < G_N_ELEMENTS(e); _i++) \
            printf("%s%g", _i == 0 ? "" : " ", e[_i]); \
        printf("]\n"); \
    }

#define linsolv(m,b,x,e) \
    x = gwy_math_lin_solve(G_N_ELEMENTS(b), m, b, NULL); \
    linprint(x,e,"ro") \
    gwy_math_lin_solve_rewrite(G_N_ELEMENTS(b), m, b, x); \
    linprint(x,e,"rw") \
    g_free(x)

void
foo_callback(gpointer obj, gpointer data __attribute__((unused)))
{
    g_message("FOO! %s", g_type_name(G_TYPE_FROM_INSTANCE(obj)));
}

void
bar_callback(gpointer hkey, GValue *value, gchar *user_data)
{
    g_message("`%s' -> %s (%s)",
              g_quark_to_string(GPOINTER_TO_UINT(hkey)),
              G_VALUE_TYPE_NAME(value),
              user_data);
}

gdouble
gauss(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
      G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
    gdouble c;

    if (b[2] == 0) {
        *fres = FALSE;
        return 0;
    }
    *fres = TRUE;
    c = (x - b[1])/b[2];

    return b[0] * exp(-c*c/2);
}

gdouble
grand(void)
{
    gdouble c = 0;
    gint i;

    for (i = 0; i < 20; i++)
        c += random()/(gdouble)RAND_MAX;
    c /= 10.0;
    c -= 1.0;
    return c;
}

typedef enum {
    GWY_FILE_NONE   = 0,
    GWY_FILE_LOAD   = 1 << 0,
    GWY_FILE_SAVE   = 1 << 1,
    GWY_FILE_DETECT = 1 << 2,
    GWY_FILE_MASK   = 0x07
} GwyFileOperation;

typedef enum {
    GWY_RUN_NONE           = 0,
    GWY_RUN_WITH_DEFAULTS  = 1 << 0,
    GWY_RUN_NONINTERACTIVE = 1 << 1,
    GWY_RUN_MODAL          = 1 << 2,
    GWY_RUN_INTERACTIVE    = 1 << 3,
    GWY_RUN_MASK           = 0x0f
} GwyRunType;

static const GwyEnum run_mode_names[] = {
    { "interactive",    GWY_RUN_INTERACTIVE },
    { "noninteractive", GWY_RUN_NONINTERACTIVE },
    { "modal",          GWY_RUN_MODAL },
    { "with_defaults",  GWY_RUN_WITH_DEFAULTS },
    { NULL,             -1 }
};

static const GwyEnum file_op_names[] = {
    { "load", GWY_FILE_LOAD },
    { "save", GWY_FILE_SAVE },
    { NULL,   -1 }
};

int
main(void)
{
    enum { count = 24 };
    GObject *ser;
    gsize size, pos;
    guchar *buffer;
    FILE *fh;
    GError *err = NULL;
    GValue val, *p;
    GwyContainer *container;
    GObject *obj;
    GQuark q;
    gulong id;
    gdouble *x;
    gdouble y;
    gboolean ok;
    GwyNLFitter *ms;
    gdouble xmq[count + 1];
    gdouble ymq[count + 1];
    gdouble vmq[count + 1];
    gdouble param[3];
    gboolean bb;
    gdouble xx, summ;
    gint i, j;


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
    container = GWY_CONTAINER(gwy_container_new());
    p = g_new0(GValue, 1);
    g_value_init(p, G_TYPE_INT);
    g_value_set_int(p, 1133);
    q = g_quark_from_string("foobar");
    gwy_container_set_value_by_name(container, "foobar", p, NULL);
    g_value_set_int(p, -100);
    gwy_container_set_value_by_name(container, "foobar", p, NULL);
    val = gwy_container_get_value(container, q);
    g_message("(value) 'foobar' -> %d", g_value_get_int(&val));
    g_message("(int32) 'foobar' -> %d", gwy_container_get_int32(container, q));
    gwy_container_set_double(container, q, 1.13);
    val = gwy_container_get_value(container, q);
    g_message("(value) 'foobar' -> %g", g_value_get_double(&val));
    g_message("this should fail:");
    g_message("(int32) 'foobar' -> %d", gwy_container_get_int32(container, q));

    gwy_container_set_double_by_name(container, "pdf", 0.5227);
    gwy_container_set_double_by_name(container, "pdf/f", 1.4142);
    gwy_container_set_double_by_name(container, "pdfoo", 7.76);

    y = 0.1;
    g_message("y = %g", y);
    ok = gwy_container_gis_double_by_name(container, "pdf", &y);
    g_message("gis 'pdf': y = %g (%s)", y, ok ? "OK" : "NO");
    ok = gwy_container_gis_double_by_name(container, "no-such-double", &y);
    g_message("gis 'no-such-double': y = %g (%s)", y, ok ? "OK" : "NO");

    gwy_container_foreach(container, NULL, (GHFunc)bar_callback, "bar bar bar");

    gwy_container_remove_by_prefix(container, "pdf");
    g_message("'pdf': %s", gwy_container_contains_by_name(container, "pdf")
                           ? "PRESENT" : "REMOVED");
    g_message("'pdf/f': %s", gwy_container_contains_by_name(container, "pdf/f")
                             ? "PRESENT" : "REMOVED");
    g_message("'pdfoo': %s", gwy_container_contains_by_name(container, "pdfoo")
                             ? "PRESENT" : "REMOVED");

    g_message("====== CONTAINER SERIALIZATION ======================");
    gwy_container_set_double_by_name(container, "pdf", 0.5227);
    gwy_container_set_double_by_name(container, "pdf/f", 1.4142);
    gwy_container_set_int64_by_name(container, "x64", 64LL);

    g_assert(G_OBJECT(ser)->ref_count == 1);
    gwy_container_set_object_by_name(container, "ser", ser);
    g_assert(G_OBJECT(ser)->ref_count == 2);
    gwy_container_set_object_by_name(container, "ser", ser);
    g_assert(G_OBJECT(ser)->ref_count == 2);
    ser = gwy_container_get_object_by_name(container, "ser");
    g_assert(G_OBJECT(ser)->ref_count == 2);

    size = 0;
    buffer = NULL;
    buffer = gwy_serializable_serialize(G_OBJECT(container), buffer, &size);
    g_object_unref(container);
    g_assert(G_OBJECT(ser)->ref_count == 1);
    g_object_unref(ser);

    g_message("serializing an empty container");
    container = GWY_CONTAINER(gwy_container_new());
    buffer = gwy_serializable_serialize(G_OBJECT(container), buffer, &size);
    g_object_unref(container);

    fh = fopen(FILENAME, "wb");
    fwrite(buffer, 1, size, fh);
    fclose(fh);
    g_free(buffer);

    g_message("reading objects from %s", FILENAME);
    g_file_get_contents(FILENAME, (gchar**)&buffer, &size, &err);
    g_message("size = %u", size);

    pos = 0;
    g_message("restoring container");
    container = GWY_CONTAINER(gwy_serializable_deserialize(buffer, size, &pos));

    g_message("'pdf/f' -> %g",
              gwy_container_get_double_by_name(container, "pdf/f"));

    ser = gwy_container_get_object_by_name(container, "ser");
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 2.2);
    g_assert(ser->ref_count == 1);

    g_object_unref(container);
    g_message("restoring the empty container");
    container = GWY_CONTAINER(gwy_serializable_deserialize(buffer, size, &pos));


    g_message("====== DUPLICATION ======================");

    gwy_container_set_double_by_name(container, "dbl", 3.141592);
    ser = gwy_test_ser_new(0.88, 0.99);
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 13.13);
    gwy_container_set_object_by_name(container, "ser", ser);
    g_object_unref(ser);
    g_message("duplicating a container");
    obj = gwy_serializable_duplicate(G_OBJECT(container));
    g_object_unref(container);
    g_message("'dbl' -> %g",
              gwy_container_get_double_by_name(GWY_CONTAINER(obj), "dbl"));
    ser = gwy_container_get_object_by_name(GWY_CONTAINER(obj), "ser");
    print(ser, "restored");

    g_message("====== ENTITIES ======================");
    pent("foo");
    pent("&alpha;");
    pent("&alpha;&beta;");
    pent("&&&;&&:;7&:&;7&:&&;;;;&;&;7&:;");
    pent(";&&");
    pent("&alphabeta;&alpha&beta;");
    pent("<a link=\"foo&lt;&quot;&gt;\">&amp;</a>");

    g_message("====== ENUMS ======================");
    g_message(gwy_enum_to_string(GWY_FILE_LOAD, file_op_names, -1));
    g_message(gwy_enum_to_string(GWY_FILE_SAVE, file_op_names,
                                 G_N_ELEMENTS(file_op_names)));
    g_message(gwy_flags_to_string(GWY_RUN_INTERACTIVE | GWY_RUN_MODAL,
                                  run_mode_names, -1, NULL));
    g_message(gwy_flags_to_string(GWY_RUN_WITH_DEFAULTS | GWY_RUN_MODAL,
                                  run_mode_names, G_N_ELEMENTS(run_mode_names),
                                  " @@@ "));

    g_message("%d", gwy_string_to_enum("with_defaults", run_mode_names, -1));
    g_message("%d", gwy_string_to_enum("modal", run_mode_names,
                                       G_N_ELEMENTS(run_mode_names)));
    g_message("%d", gwy_string_to_flags("load save", file_op_names, -1, NULL));
    g_message("%d", gwy_string_to_flags("load save", file_op_names,
                                       G_N_ELEMENTS(file_op_names), NULL));
    g_message("%d", gwy_string_to_flags("noninteractive-interactive",
                                        run_mode_names, -1, "-"));

    g_message("====== MATH ======================");
    {
        gdouble m[] = { 1, 2, 3, 4 };
        gdouble b[] = { 5, 6 };
        gdouble e[] = { -4, 4.5 };
        linsolv(m, b, x, e);
    }
    {
        gdouble m[] = { 1, 2, 3, 5, 6, 7, 1, 2, 4 };
        gdouble b[] = { 4, 8, 8 };
        gdouble e[] = { 2, -5, 4 };
        linsolv(m, b, x, e);

    }
    {
        gdouble m[] = { 1, 1.000001, 0.999999, 1 };
        gdouble b[] = { 1, 1 };
        gdouble e[] = { -1e6, 1e6 };
        linsolv(m, b, x, e);
    }

    g_message("====== NLFIT ======================");
    ms = gwy_math_nlfit_new(gauss, gwy_math_nlfit_derive);
    param[0] = 10;                  /*amplituda*/
    param[1] = 10;                  /*stred*/
    param[2] = 1;                   /*sirka*/

    xx = param[1] - param[2] * 5;

    for (i = 0; i < count; i++, xx += param[2] * 5/count * 2) {
        xmq[i] = xx;
        ymq[i] = gauss(xmq[i], 3, param, NULL, &bb) + grand()/5;
        vmq[i] = 1;
    }
    param[0] = 9;
    param[1] = 9;
    param[2] = 0.5;
    summ = gwy_math_nlfit_fit(ms, count, xmq, ymq, vmq, 3, param, NULL);

    printf("Evaluated: %d\n", ms->eval);
    printf("Suma: %f\n", summ);
    for (i = 0; i < 3; i++)
        printf("Par[%d] = %f +- %f\n",
               i, param[i], gwy_math_nlfit_get_sigma(ms, i));
    puts("");
    for (i = 0; i < count; i++)
        printf("%3d: %.4f\t%.4f\t%.4f\t%.4f\n",
               i, xmq[i], ymq[i], vmq[i], gauss(xmq[i], 3, param, NULL, &bb));

    for (i = 0; i < 3; i++) {
        for (j = 0; j < i; j++)
            printf("%.4f\t", gwy_math_nlfit_get_correlations(ms, i, j));
        puts("");
    }
    gwy_math_nlfit_free(ms);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
