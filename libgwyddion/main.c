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

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include "gwyddion.h"
#include "gwytestser.h"

static void
log_handler(G_GNUC_UNUSED const gchar *log_domain,
            G_GNUC_UNUSED GLogLevelFlags log_level,
            const gchar *message,
            G_GNUC_UNUSED gpointer user_data)
{
    fputs(message, stderr);
    fputc('\n', stderr);
}

static void
ignore_handler(G_GNUC_UNUSED const gchar *log_domain,
               G_GNUC_UNUSED GLogLevelFlags log_level,
               G_GNUC_UNUSED const gchar *message,
               G_GNUC_UNUSED gpointer user_data)
{
}

#define FILENAME "testser.object"

#define print(obj,txt) \
    g_message("%s %s: theta = %g, radius = %g", \
              g_type_name(G_TYPE_FROM_INSTANCE(G_OBJECT(obj))), txt, \
              gwy_test_ser_get_theta(GWY_TEST_SER(obj)), \
              gwy_test_ser_get_radius(GWY_TEST_SER(obj))); \
    gwy_test_ser_print_history(GWY_TEST_SER(obj))

static void
test_serializable_iface(void)
{
    GObject *ser;
    gsize pos, size;
    guchar *buffer;
    GByteArray *array;
    FILE *fh;
    GError *err = NULL;

    /* create, write and free an object */
    ser = gwy_test_ser_new(0.42, 1.001);
    g_message("===== SERIALIZABLE INTERFACE ========================");
    print(ser, "created");

    array = gwy_serializable_serialize(ser, NULL);
    g_message("size of first object: %u", array->len);
    g_object_unref(ser);

    /* create, write and free another object */
    ser = gwy_test_ser_new(0.52, 2.002);
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 72.27);
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 666.0);
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 100.0);
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 1e44);
    print(ser, "created");

    array = gwy_serializable_serialize(ser, array);
    g_message("size of both objects: %" G_GSIZE_FORMAT, size);
    g_object_unref(ser);
    g_message("writing objects to %s", FILENAME);
    fh = g_fopen(FILENAME, "wb");
    fwrite(array->data, 1, array->len, fh);
    fclose(fh);
    g_byte_array_free(array, TRUE);

    /* create and free yet another object just to overwrite memory */
    ser = gwy_test_ser_new(1.618, 0.33333333);
    g_object_unref(ser);

    /* read the object back and restore it */
    g_message("reading objects from %s", FILENAME);
    if (!gwy_file_get_contents(FILENAME, &buffer, &size, &err)) {
        g_error("%s", err->message);
    }
    g_message("size = %" G_GSIZE_FORMAT, size);

    pos = 0;
    g_message("restoring the first one");
    ser = gwy_serializable_deserialize(buffer, size, &pos);
    print(ser, "restored");
    g_object_unref(ser);

    g_message("restoring the second one");
    ser = gwy_serializable_deserialize(buffer, size, &pos);
    print(ser, "restored");

    g_object_unref(ser);
    if (!gwy_file_abandon_contents(buffer, size, &err)) {
        g_error("%s", err->message);
    }
}

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

static void
test_container(void)
{
    GValue val, *p;
    gdouble y;
    GwyContainer *container;
    GQuark q;
    gboolean ok;

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

}

/* FIXME: this causes warnings because `ser' contains now-invalid SI units */
static const gchar *serialized_text =
    "\"foobar\" int32 -100\n"
    "\"pdf\" double 0.52270000000000005\n"
    "\"alena\" char #\n"
    "\"alice\" char 0x0d\n"
    "\"pdf/f\" double 1.4141999999999999\n"
    "\"yesno\" boolean True\n"
    "\"ser\" object 4777795465737453657200890000007468657461006417d9cef753e3f93f726164697573004402000000da12c1515555d53f9a99999999990140737472696e67005302000000302e33333333333300322e3200756e6974004f020000004777795349556e69740012000000756e69747374720073302e333333333333004777795349556e6974000d000000756e69747374720073322e3200\n"
    "\"x64\" int64 64\n";

static void
test_container_serialization(void)
{
    GValue *p;
    GQuark q;
    GObject *ser;
    gsize size, pos;
    GByteArray *array;
    guchar *buffer;
    GwyContainer *container;
    FILE *fh;
    GError *err = NULL;
    GPtrArray *pa;
    guint i;

    g_message("====== CONTAINER SERIALIZATION ======================");
    ser = gwy_test_ser_new(1.618, 0.33333333);
    container = GWY_CONTAINER(gwy_container_new());
    p = g_new0(GValue, 1);
    g_value_init(p, G_TYPE_INT);
    g_value_set_int(p, 1133);
    q = g_quark_from_string("foobar");
    gwy_container_set_value_by_name(container, "foobar", p, NULL);
    g_value_set_int(p, -100);
    gwy_container_set_value_by_name(container, "foobar", p, NULL);

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

    array = gwy_serializable_serialize(G_OBJECT(container), NULL);
    g_object_unref(container);
    g_assert(G_OBJECT(ser)->ref_count == 1);
    g_object_unref(ser);

    g_message("serializing an empty container");
    container = GWY_CONTAINER(gwy_container_new());
    array = gwy_serializable_serialize(G_OBJECT(container), array);
    g_object_unref(container);

    fh = g_fopen(FILENAME, "wb");
    fwrite(array->data, 1, array->len, fh);
    fclose(fh);
    g_byte_array_free(array, TRUE);

    g_message("reading objects from %s", FILENAME);
    if (!gwy_file_get_contents(FILENAME, &buffer, &size, &err)) {
        g_error("%s", err->message);
    }
    g_message("size = %" G_GSIZE_FORMAT, size);

    pos = 0;
    g_message("restoring container");
    container = GWY_CONTAINER(gwy_serializable_deserialize(buffer, size, &pos));

    g_message("'pdf/f' -> %g",
              gwy_container_get_double_by_name(container, "pdf/f"));

    ser = gwy_container_get_object_by_name(container, "ser");
    gwy_test_ser_set_radius(GWY_TEST_SER(ser), 2.2);
    g_assert(ser->ref_count == 1);

    gwy_container_set_string_by_name(container, "/string-n",
                                     g_strdup("a\nb\rc\td"));
    pa = gwy_container_serialize_to_text(container);
    g_object_unref(container);
    g_message("serialized to text");
    for (i = 0; i < pa->len; i++) {
        g_message("%s", (gchar*)pa->pdata[i]);
        g_free(pa->pdata[i]);
    }
    g_ptr_array_free(pa, TRUE);

    g_message("restoring the empty container");
    container = GWY_CONTAINER(gwy_serializable_deserialize(buffer, size, &pos));
    g_object_unref(container);

    container = gwy_container_deserialize_from_text(serialized_text);
    g_object_unref(container);
    if (!gwy_file_abandon_contents(buffer, size, &err)) {
        g_error("%s", err->message);
    }
}

static void
test_duplication(void)
{
    GObject *obj, *ser;
    GwyContainer *container, *duplicate;

    g_message("====== DUPLICATION ======================");
    container = GWY_CONTAINER(gwy_container_new());

    gwy_container_set_double_by_name(container, "pdf", 0.5227);
    gwy_container_set_double_by_name(container, "pdf/f", 1.4142);
    gwy_container_set_int64_by_name(container, "x64", 64LL);

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

    container = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_double_by_name(container, "/foo", 0.5227);
    gwy_container_set_double_by_name(container, "/foobar", 1.4142);
    gwy_container_set_int64_by_name(container, "/foo/bar", 64LL);
    gwy_container_set_double_by_name(container, "/foo/barbar", 3.141592);
    gwy_container_set_int32_by_name(container, "/quux", 42);

    g_message("partially duplicating a container");
    duplicate = gwy_container_duplicate_by_prefix(container,
                                                  "/foo", "/quux", NULL);
    gwy_container_foreach(duplicate, NULL, (GHFunc)bar_callback, "dupl");
}

#define pent(txt) \
    { \
        gchar *_t = gwy_entities_text_to_utf8(txt); \
        fprintf(stderr, "`%s' -> `%s'\n", txt, _t); \
        g_free(_t); \
    }

static void
test_entities(void)
{
    g_message("====== ENTITIES ======================");

    pent("foo");
    pent("&alpha;");
    pent("&alpha;&beta;");
    pent("&&&;&&:;7&:&;7&:&&;;;;&;&;7&:;");
    pent(";&&");
    pent("&alphabeta;&alpha&beta;");
    pent("<a link=\"foo&lt;&quot;&gt;\">&amp;</a>");

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

static void
test_enums(void)
{
    g_message("====== ENUMS ======================");

    g_message(gwy_enum_to_string(GWY_FILE_LOAD, file_op_names, -1));
    g_message(gwy_enum_to_string(GWY_FILE_SAVE, file_op_names,
                                 G_N_ELEMENTS(file_op_names)));
    g_message(gwy_flags_to_string(GWY_RUN_INTERACTIVE | GWY_RUN_MODAL,
                                  run_mode_names, -1, NULL));
    g_message(gwy_flags_to_string(GWY_RUN_WITH_DEFAULTS | GWY_RUN_MODAL,
                                  run_mode_names, G_N_ELEMENTS(run_mode_names),
                                  " @@@ "));

    g_message("<with_defaults> = %d",
              gwy_string_to_enum("with_defaults", run_mode_names, -1));
    g_message("<modal> = %d",
              gwy_string_to_enum("modal", run_mode_names,
                                 G_N_ELEMENTS(run_mode_names)));
    g_message("<load save> = %d",
              gwy_string_to_flags("load save", file_op_names, -1, NULL));
    g_message("<load save> = %d",
              gwy_string_to_flags("load save", file_op_names,
                                  G_N_ELEMENTS(file_op_names), NULL));
    g_message("<noninteractive-interactive> = %d",
              gwy_string_to_flags("noninteractive-interactive",
                                  run_mode_names, -1, "-"));
}

#define linprint(x,e,s) \
    { \
        gsize _i; \
        fprintf(stderr, "%s [", s); \
        for (_i = 0; _i < G_N_ELEMENTS(e); _i++) \
            fprintf(stderr, "%s%g", _i == 0 ? "" : " ", x[_i]); \
        fprintf(stderr, "] == ["); \
        for (_i = 0; _i < G_N_ELEMENTS(e); _i++) \
            fprintf(stderr, "%s%g", _i == 0 ? "" : " ", e[_i]); \
        fprintf(stderr, "]\n"); \
    }

#define linsolv(m,b,x,e) \
    x = gwy_math_lin_solve(G_N_ELEMENTS(b), m, b, NULL); \
    linprint(x,e,"ro") \
    gwy_math_lin_solve_rewrite(G_N_ELEMENTS(b), m, b, x); \
    linprint(x,e,"rw") \
    g_free(x)

static void
test_math(void)
{
    gdouble *x;

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
}

static void
test_string_utils(void)
{
    gchar *s, *s2;

    g_message("====== STRING UTILS ======================");

    s = g_strdup("gwy_math_nlfit_new()");
    s2 = gwy_strkill(g_strdup(s), "");
    g_message("kill <%s> in <%s> -> <%s>", "", s, s2);
    g_free(s2);
    s2 = gwy_strkill(g_strdup(s), "_");
    g_message("kill <%s> in <%s> -> <%s>", "_", s, s2);
    g_free(s2);
    s2 = gwy_strkill(g_strdup(s), "_()w");
    g_message("kill <%s> in <%s> -> <%s>", "_()w", s, s2);
    g_free(s2);

    s2 = gwy_strreplace(s, "_", "FOO", (gsize)-1);
    g_message("replace all <%s> in <%s> with <%s> -> <%s>", "_", s, "FOO", s2);
    g_free(s2);
    s2 = gwy_strreplace(s, "_", "FOO", 2);
    g_message("replace 2 <%s> in <%s> with <%s> -> <%s>", "_", s, "FOO", s2);
    g_free(s2);
    s2 = gwy_strreplace(s, "_", "", 1);
    g_message("replace 1 <%s> in <%s> with <%s> -> <%s>", "_", s, "", s2);
    g_free(s2);

}

gdouble
gauss(gdouble x, G_GNUC_UNUSED gint n_par, const gdouble *b,
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

static void
test_nlfit(void)
{
    enum { count = 24 };
    GwyNLFitter *ms;
    gdouble xmq[count + 1];
    gdouble ymq[count + 1];
    gboolean fixed[3];
    gint linkmap[3];
    gdouble param[3];
    gboolean bb;
    gdouble xx, summ;
    gint i, j;

    g_message("====== NLFIT ======================");

    ms = gwy_math_nlfit_new(gauss, gwy_math_nlfit_derive);
    param[0] = 10;                  /*amplituda*/
    param[1] = 10;                  /*stred*/
    param[2] = 1;                   /*sirka*/

    xx = param[1] - param[2] * 5;

    for (i = 0; i < count; i++, xx += param[2] * 5/count * 2) {
        xmq[i] = xx;
        ymq[i] = gauss(xmq[i], 3, param, NULL, &bb) + grand()/5;
    }
    param[0] = 9;
    param[1] = 9;
    param[2] = 0.5;
    summ = gwy_math_nlfit_fit(ms, count, xmq, ymq, 3, param, NULL);

    fprintf(stderr, "Evaluated: %d\n", ms->eval);
    fprintf(stderr, "Sum: %f\n", summ);
    for (i = 0; i < 3; i++)
        fprintf(stderr, "Par[%d] = %f +- %f\n",
                i, param[i], gwy_math_nlfit_get_sigma(ms, i));
    fputs("\n", stderr);
    for (i = 0; i < count; i++)
        fprintf(stderr, "%3d: %.4f\t%.4f\t%.4f\n",
                i, xmq[i], ymq[i], gauss(xmq[i], 3, param, NULL, &bb));
    fputs("\n", stderr);
    for (i = 0; i < 3; i++) {
        for (j = 0; j <= i; j++)
            fprintf(stderr, "%.4f\t",
                    gwy_math_nlfit_get_correlations(ms, i, j));
        fputs("\n", stderr);
    }

    fputs("\n", stderr);
    param[0] = 10.0;
    param[1] = 12.0;
    param[2] = 1.0;
    fixed[0] = TRUE;
    fixed[1] = FALSE;
    fixed[2] = TRUE;
    summ = gwy_math_nlfit_fit_full(ms, count, xmq, ymq, NULL, 3, param,
                                   fixed, NULL, NULL);

    fprintf(stderr, "Evaluated: %d\n", ms->eval);
    fprintf(stderr, "Sum: %f\n", summ);
    for (i = 0; i < 3; i++)
        fprintf(stderr, "Par[%d] = %f +- %f\n",
                i, param[i], gwy_math_nlfit_get_sigma(ms, i));
    fputs("\n", stderr);
    for (i = 0; i < count; i++)
        fprintf(stderr, "%3d: %.4f\t%.4f\t%.4f\n",
                i, xmq[i], ymq[i], gauss(xmq[i], 3, param, NULL, &bb));
    fputs("\n", stderr);
    for (i = 0; i < 3; i++) {
        for (j = 0; j <= i; j++)
            fprintf(stderr, "%.4f\t",
                    gwy_math_nlfit_get_correlations(ms, i, j));
        fputs("\n", stderr);
    }

    fputs("\n", stderr);
    param[0] = 12.0;
    param[1] = 12.0;
    param[2] = 1.0;
    linkmap[0] = 0;
    linkmap[1] = 0;
    linkmap[2] = 2;
    summ = gwy_math_nlfit_fit_full(ms, count, xmq, ymq, NULL, 3, param,
                                   NULL, linkmap, NULL);

    fprintf(stderr, "Evaluated: %d\n", ms->eval);
    fprintf(stderr, "Sum: %f\n", summ);
    for (i = 0; i < 3; i++)
        fprintf(stderr, "Par[%d] = %f +- %f\n",
                i, param[i], gwy_math_nlfit_get_sigma(ms, i));
    fputs("\n", stderr);
    for (i = 0; i < count; i++)
        fprintf(stderr, "%3d: %.4f\t%.4f\t%.4f\n",
                i, xmq[i], ymq[i], gauss(xmq[i], 3, param, NULL, &bb));
    fputs("\n", stderr);
    for (i = 0; i < 3; i++) {
        for (j = 0; j <= i; j++)
            fprintf(stderr, "%.4f\t",
                    gwy_math_nlfit_get_correlations(ms, i, j));
        fputs("\n", stderr);
    }

    gwy_math_nlfit_free(ms);
}

#define dcpath(p) \
    do { \
        gchar *s; \
        fprintf(stderr, "<%s> -> <%s>\n", p, s = gwy_canonicalize_path(p)); \
        g_free(s); \
    } while (FALSE)

/* FIXME: must defgine G_OS_WIN32 in gwyutils.c for that! */
static void
test_path_normalization()
{
    g_message("====== PATH ======================");
    dcpath("/foo/bar/adgasg");
    dcpath("/foo/bar////adgasg");
    dcpath("/foo/bar////adgasg///");
    dcpath("//foo//bar////adgasg///");  /* On Win32 initial // must be kept */
    dcpath("/////");
    dcpath("main.c");
    dcpath("c:\\dgd\\fgfdsg\\fgsdfg");
    dcpath("d:\\dgd\\\\\\fgfdsg\\fgsdfg");
    dcpath("e:\\dgd\\\\\\fgfdsg/fgsdfg/baz");
    dcpath("../Makefile.am");
    dcpath("/foo/../Makefile.am");
    dcpath("/foo/bar/../baz/../../Makefile.am");
    dcpath("/foo/./bar/../baz/.././.././Makefile.am");
    dcpath("/..");
    dcpath("/../.");
    dcpath("/./");
    dcpath("///./");
    dcpath("/..///./");
    dcpath("/././../../.");
    dcpath("/././../.././");
}

static void
test_si_unit()
{
    const gchar *pairs[] = {
        "", "",
        "", "100",
        "km^2/m", "cm",
        "m kg s^-2", "km ug ms-2",
        "A s kg", "kg A s",
        "A s kg", "kg A s^2",
        "m cm km um/mm^5", "m^-1",
    };
    GwySIUnit *siunit1, *siunit2;
    guint i;

    g_message("===== SI UNIT ========================");

    siunit1 = (GwySIUnit*)gwy_si_unit_new("");
    siunit2 = (GwySIUnit*)gwy_si_unit_new("");
    for (i = 0; i < G_N_ELEMENTS(pairs)/2; i++) {
        gwy_si_unit_set_unit_string(siunit1, pairs[2*i]);
        gwy_si_unit_set_unit_string(siunit2, pairs[2*i + 1]);
        fprintf(stderr, "<%s> %s <%s>\n",
                pairs[2*i],
                gwy_si_unit_equal(siunit1, siunit2) ? "=" : "!=",
                pairs[2*i + 1]);
    }
    g_object_unref(siunit2);
    g_object_unref(siunit1);
}

#define dsiunitd(si, dig, val, vf) \
    vf = gwy_si_unit_get_format_with_digits(si, GWY_SI_UNIT_FORMAT_PLAIN, \
                                            val, dig, vf); \
    fprintf(stderr, "(%s,\t%g,\td=%d) -> %.*f %s\n", \
            gwy_si_unit_get_unit_string(si), val, dig, \
            vf->precision, val/vf->magnitude, vf->units)

#define dsiunitr(si, res, val, vf) \
    vf = gwy_si_unit_get_format_with_resolution(si, GWY_SI_UNIT_FORMAT_PLAIN, \
                                                val, res, vf); \
    fprintf(stderr, "(%s,\t%g,\tr=%g)\t-> %.*f %s\n", \
            gwy_si_unit_get_unit_string(si), val, res, \
            vf->precision, val/vf->magnitude, vf->units)

static void
test_si_unit_format(void)
{
    GwySIUnit *si;
    GwySIValueFormat *vformat = NULL;

    g_message("====== SI UNIT FORMAT ======================");

    si = (GwySIUnit*)gwy_si_unit_new("m");
    dsiunitd(si, 0, 1e1, vformat);
    dsiunitd(si, 0, 1e0, vformat);
    dsiunitd(si, 0, 1e-1, vformat);
    dsiunitd(si, 0, 1e-9, vformat);
    dsiunitd(si, 0, 1e-10, vformat);
    dsiunitd(si, 0, 1e-11, vformat);
    dsiunitd(si, 1, 1e1, vformat);
    dsiunitd(si, 1, 1e0, vformat);
    dsiunitd(si, 1, 1e-1, vformat);
    dsiunitd(si, 1, 1e-9, vformat);
    dsiunitd(si, 1, 1e-10, vformat);
    dsiunitd(si, 1, 1e-11, vformat);
    dsiunitd(si, 2, 1e1, vformat);
    dsiunitd(si, 2, 1e0, vformat);
    dsiunitd(si, 2, 1e-1, vformat);
    dsiunitd(si, 2, 1e-9, vformat);
    dsiunitd(si, 2, 1e-10, vformat);
    dsiunitd(si, 2, 1e-11, vformat);
    g_object_unref(si);

    si = (GwySIUnit*)gwy_si_unit_new("Hz");
    dsiunitd(si, 0, 1e1, vformat);
    dsiunitd(si, 0, 1e0, vformat);
    dsiunitd(si, 0, 1e-1, vformat);
    dsiunitd(si, 0, 1e-9, vformat);
    dsiunitd(si, 0, 1e-10, vformat);
    dsiunitd(si, 0, 1e-11, vformat);
    dsiunitd(si, 1, 1e1, vformat);
    dsiunitd(si, 1, 1e0, vformat);
    dsiunitd(si, 1, 1e-1, vformat);
    dsiunitd(si, 1, 1e-9, vformat);
    dsiunitd(si, 1, 1e-10, vformat);
    dsiunitd(si, 1, 1e-11, vformat);
    dsiunitd(si, 2, 1e1, vformat);
    dsiunitd(si, 2, 1e0, vformat);
    dsiunitd(si, 2, 1e-1, vformat);
    dsiunitd(si, 2, 1e-9, vformat);
    dsiunitd(si, 2, 1e-10, vformat);
    dsiunitd(si, 2, 1e-11, vformat);
    g_object_unref(si);

    si = (GwySIUnit*)gwy_si_unit_new("m");
    dsiunitr(si, 1e1, 1e1, vformat);
    dsiunitr(si, 1e0, 1e0, vformat);
    dsiunitr(si, 1e-1, 1e-1, vformat);
    dsiunitr(si, 1e-9, 1e-9, vformat);
    dsiunitr(si, 1e-10, 1e-10, vformat);
    dsiunitr(si, 1e-11, 1e-11, vformat);
    dsiunitr(si, 1e0, 1e1, vformat);
    dsiunitr(si, 1e-1, 1e0, vformat);
    dsiunitr(si, 1e-2, 1e-1, vformat);
    dsiunitr(si, 1e-10, 1e-9, vformat);
    dsiunitr(si, 1e-11, 1e-10, vformat);
    dsiunitr(si, 1e-12, 1e-11, vformat);
    dsiunitr(si, 1e-1, 1e1, vformat);
    dsiunitr(si, 1e-2, 1e0, vformat);
    dsiunitr(si, 1e-3, 1e-1, vformat);
    dsiunitr(si, 1e-11, 1e-9, vformat);
    dsiunitr(si, 1e-12, 1e-10, vformat);
    dsiunitr(si, 1e-13, 1e-11, vformat);
    g_object_unref(si);

    si = (GwySIUnit*)gwy_si_unit_new("Hz");
    dsiunitr(si, 1e1, 1e1, vformat);
    dsiunitr(si, 1e0, 1e0, vformat);
    dsiunitr(si, 1e-1, 1e-1, vformat);
    dsiunitr(si, 1e-9, 1e-9, vformat);
    dsiunitr(si, 1e-10, 1e-10, vformat);
    dsiunitr(si, 1e-11, 1e-11, vformat);
    dsiunitr(si, 1e0, 1e1, vformat);
    dsiunitr(si, 1e-1, 1e0, vformat);
    dsiunitr(si, 1e-2, 1e-1, vformat);
    dsiunitr(si, 1e-10, 1e-9, vformat);
    dsiunitr(si, 1e-11, 1e-10, vformat);
    dsiunitr(si, 1e-12, 1e-11, vformat);
    dsiunitr(si, 1e-1, 1e1, vformat);
    dsiunitr(si, 1e-2, 1e0, vformat);
    dsiunitr(si, 1e-3, 1e-1, vformat);
    dsiunitr(si, 1e-11, 1e-9, vformat);
    dsiunitr(si, 1e-12, 1e-10, vformat);
    dsiunitr(si, 1e-13, 1e-11, vformat);
    g_object_unref(si);

    gwy_si_unit_value_format_free(vformat);
}

#define siparsecompose(str) \
    siunit = (GwySIUnit*)gwy_si_unit_new_parse(str, &power10); \
    fprintf(stderr, "<%s> -> <%s>", \
            str, gwy_si_unit_get_unit_string(siunit)); \
    if (power10) \
        fprintf(stderr, " x 10^%d", power10); \
    fprintf(stderr, "\n")

static void
test_si_unit_parse(void)
{
    GwySIUnit *siunit;
    gint power10;

    g_message("====== SI UNIT 2 ======================");
    siparsecompose("");
    siparsecompose("100");
    siparsecompose("m");
    siparsecompose("0.1 cm");
    siparsecompose("um/s");
    siparsecompose("1e-2 deg");
    siparsecompose("kPa");
    siparsecompose("kHz/mV");
    siparsecompose("m^3 V^-2 s-2");
    siparsecompose("10^6 m s<sup>-2</sup>");
    siparsecompose("mm^4/ns^2");
    siparsecompose("uV/LSB");
    siparsecompose("m2");
    siparsecompose("m/m^2");
    siparsecompose("10 cm^2 km/m^3");
}

static void
test_si_unit_err(void)
{
    const gchar *chars = "0000111122223333444455556666777788889999...."
                         "eee                              "
                         G_CSET_a_2_z G_CSET_A_2_Z
                         "^^^^^^++++---/////<<<<>>>>";
    GwySIUnit *siunit;
    GTimer *timer;
    GRand *rng;
    GString *str;
    guint i, n, len, id;
    gchar c;

    g_message("====== SI UNIT GARBAGE ======================");
    id = g_log_set_handler("Gwyddion", G_LOG_LEVEL_WARNING, ignore_handler,
                           NULL);

    str = g_string_new("");
    rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    timer = g_timer_new();
    g_timer_stop(timer);
    siunit = gwy_si_unit_new("1");

    n = 666666;
    len = strlen(chars);
    fprintf(stderr, "Parsing units");
    for (i = 0; i < n; i++) {
        g_string_truncate(str, 0);
        do {
            c = chars[g_rand_int_range(rng, 0, len)];
            g_string_append_c(str, c);
        } while (g_rand_int_range(rng, 0, 12));
        g_timer_continue(timer);
        gwy_si_unit_set_unit_string(siunit, str->str);
        g_timer_stop(timer);
        if (i % 12000 == 0)
            putc('.', stderr);
    }
    putc('\n', stderr);

    fprintf(stderr, "Total units parsed: %u in %fs\n",
            n, g_timer_elapsed(timer, NULL));

    g_object_unref(siunit);
    g_timer_destroy(timer);
    g_rand_free(rng);
    g_string_free(str, TRUE);

    g_log_remove_handler("Gwyddion", id);
}

#define printexpr(str) \
    if (gwy_expr_evaluate(expr, str, variables, &err)) \
        fprintf(stderr, "<%s> = %g", str, variables[0]); \
    else { \
        fprintf(stderr, "<%s>: ERROR %s", str, err->message); \
        g_clear_error(&err); \
    } \
    fprintf(stderr, "\n")

#define printexprvar(str) \
    if (gwy_expr_compile(expr, str, &err)) { \
        nvars = gwy_expr_get_variables(expr, &varnames); \
        for (i = 1; i < nvars; i++) \
            fprintf(stderr, "%s=%g ", varnames[i], variables[i]); \
        fprintf(stderr, "<%s> = %g", str, gwy_expr_execute(expr, variables)); \
    } \
    else { \
        fprintf(stderr, "<%s>: ERROR %s", str, err->message); \
        g_clear_error(&err); \
    } \
    fprintf(stderr, "\n")

static void
test_expr(void)
{
    gdouble variables[4];
    gchar **varnames;
    GError *err = NULL;
    GwyExpr *expr;
    gint nvars, i;

    g_message("====== EXPR ======================");

    expr = gwy_expr_new();
    gwy_expr_define_constant(expr, "Pi", G_PI, NULL);
    printexpr("");
    printexpr("-100");
    printexpr("(-100)");
    printexpr("1+1");
    printexpr("(--10)--(-3)");
    printexpr("-1--1");
    printexpr("+1-+1");
    printexpr(")");
    printexpr("))");
    printexpr("1)+(3");
    printexpr("(");
    printexpr("x");
    printexpr("1+#");
    printexpr("+");
    printexpr("1*+2");
    printexpr("1**2");
    printexpr("1+");
    printexpr("*1");
    printexpr("1(^2)");
    printexpr("1,2");
    printexpr("sin 3");
    printexpr("sin(3)");
    printexpr("(sin 3)");
    printexpr("(sin(3))");
    printexpr("((sin 3))");
    printexpr("sin Pi/2");
    printexpr("(sin Pi)/2");
    printexpr("sin(Pi/2)");
    printexpr("sin(Pi)/2");
    printexpr("sin(Pi)/(2)");
    printexpr("hypot 3,4");
    printexpr("ln 4/ln 2");
    printexpr("hypot hypot 3,4,hypot 3,4");
    printexpr("1+2*3");
    printexpr("1+2 3");
    printexpr("1^2^3");
    printexpr("1/5 hypot 3,4");
    printexpr("1(2+3)");
    printexpr("1(2)");

    variables[1] = 1;  variables[2] = 2;  variables[3] = 3;
    printexprvar("x+y+z");
    printexprvar("x^y^z");
    printexprvar("2 a + b/c");
    printexprvar("pepa z depa");
    printexprvar("1 + x");
    variables[1] = 3;  variables[2] = 4;  variables[3] = 5;
    printexprvar("hypot hypot x, y, z");
    printexprvar("-x--y");
    printexprvar("a+a-b+a-b");
    printexprvar("1/a b");
    printexprvar("(a+b)(a+c)");
    printexprvar("c(a+b)");
    printexprvar("(a+b)c");
    printexprvar("x+1+2+3+4");
    printexprvar("1+2+3+4+x");
    gwy_expr_free(expr);
}

static void
test_expr_err(void)
{
    static const GwyEnum errtable[] = {
       { "CLOSING_PARENTHESIS", GWY_EXPR_ERROR_CLOSING_PARENTHESIS, },
       { "EMPTY", GWY_EXPR_ERROR_EMPTY, },
       { "EMPTY_PARENTHESES", GWY_EXPR_ERROR_EMPTY_PARENTHESES, },
       { "GARBAGE", GWY_EXPR_ERROR_GARBAGE, },
       { "INVALID_ARGUMENT", GWY_EXPR_ERROR_INVALID_ARGUMENT, },
       { "INVALID_TOKEN", GWY_EXPR_ERROR_INVALID_TOKEN, },
       { "MISSING_ARGUMENT", GWY_EXPR_ERROR_MISSING_ARGUMENT, },
       { "NOT_EXECUTABLE", GWY_EXPR_ERROR_NOT_EXECUTABLE, },
       { "OPENING_PARENTHESIS", GWY_EXPR_ERROR_OPENING_PARENTHESIS, },
       { "STRAY_COMMA", GWY_EXPR_ERROR_STRAY_COMMA, },
       { "UNRESOLVED_IDENTIFIERS", GWY_EXPR_ERROR_UNRESOLVED_IDENTIFIERS, },
       { "CONSTANT_NAME", GWY_EXPR_ERROR_CONSTANT_NAME, },
    };
    const gchar *chars = "0000111122223333444455556666777788889999...."
                         "0000111122223333444455556666777788889999...."
                         "0000111122223333444455556666777788889999...."
                         "0000111122223333444455556666777788889999...."
                         "EEEEEPPII                                   "
                         "powminmaxmodhypotatanabssqrtcbrt"
                         "sincostanasinacosatanexplnlogpowlogcoshsinhtanh"
                         "++++----~~~****////%&&%^^^^,,,,((((()))))"
                         "++++----~~~****////%&&%^^^^,,,,((((()))))";
    GwyExpr *expr;
    GTimer *timer;
    GError *err = NULL;
    GRand *rng;
    GString *str;
    guint *errors;
    guint i, n, len;
    gchar c;

    g_message("====== EXPR GARBAGE ======================");

    errors = g_new0(guint, G_N_ELEMENTS(errtable) + 2);
    str = g_string_new("");
    rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    timer = g_timer_new();
    g_timer_stop(timer);

    expr = gwy_expr_new();
    gwy_expr_define_constant(expr, "PI", G_PI, NULL);
    gwy_expr_define_constant(expr, "E", G_E, NULL);

    len = strlen(chars);
    n = 6666666;
    fprintf(stderr, "Compiling exprs");
    for (i = 0; i < n; i++) {
        g_string_truncate(str, 0);
        do {
            c = chars[g_rand_int_range(rng, 0, len)];
            g_string_append_c(str, c);
        } while (g_rand_int_range(rng, 0, 12));
        g_timer_continue(timer);
        if (!gwy_expr_compile(expr, str->str, &err)) {
            errors[MIN(err->code + 1, G_N_ELEMENTS(errtable))]++;
            g_clear_error(&err);
        }
        else
            errors[0]++;
        g_timer_stop(timer);
        if (i % 100000 == 0)
            putc('.', stderr);
    }
    putc('\n', stderr);

    fprintf(stderr, "OK: %u\n", errors[0]);
    for (i = 0; i < G_N_ELEMENTS(errtable); i++)
        fprintf(stderr, "%s: %u\n", errtable[i].name, errors[i+1]);
    fprintf(stderr, "UNKNOWN ERROR: %u\n", errors[G_N_ELEMENTS(errtable)+1]);
    fprintf(stderr, "Total expressions: %u in %fs\n",
            n, g_timer_elapsed(timer, NULL));

    g_timer_destroy(timer);
    g_rand_free(rng);
    gwy_expr_free(expr);
    g_string_free(str, TRUE);
    g_free(errors);
}

static void
test_expr_speed(void)
{
    GwyExpr *expr;
    GTimer *timer;
    gdouble *buf1, *buf2, *buf3;
    gdouble *buffers[3];
    gdouble vars[3];
    guint i, j, n, len = 65536;

    g_message("====== EXPR EXECUTION ======================");

    buf1 = g_new(gdouble, len);
    buf2 = g_new(gdouble, len);
    buf3 = g_new(gdouble, len);
    buffers[0] = NULL;
    buffers[1] = buf1;
    buffers[2] = buf2;
    timer = g_timer_new();
    g_timer_stop(timer);

    expr = gwy_expr_new();
    for (j = 0; j < len; j++) {
        buf1[j] = (j + 1.0)/(100.0 + j*j);
        buf2[j] = sin(j);
    }
    /* constant */
    fprintf(stderr, "Constant");
    n = 2000;
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++)
            buf3[j] = 1.0;
    }
    g_timer_stop(timer);
    fprintf(stderr, " direct=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 200;
    gwy_expr_compile(expr, "1.0", NULL);
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++) {
            buf3[j] = gwy_expr_execute(expr, vars);
        }
    }
    g_timer_stop(timer);
    fprintf(stderr, " expr=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 200;
    gwy_expr_compile(expr, "1.0", NULL);
    g_timer_start(timer);
    for (i = 0; i < n; i++)
        gwy_expr_vector_execute(expr, len, (const gdouble**)buffers, buf3);
    g_timer_stop(timer);
    fprintf(stderr, " vector=%f", 100*g_timer_elapsed(timer, NULL)/n);

    fprintf(stderr, "\n");

    /* a+b */
    fprintf(stderr, "a+b     ");
    n = 1000;
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++)
            buf3[j] = buf1[j] + buf2[j];
    }
    g_timer_stop(timer);
    fprintf(stderr, " direct=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 200;
    gwy_expr_compile(expr, "a+b", NULL);
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++) {
            vars[1] = buf1[j];
            vars[2] = buf2[j];
            buf3[j] = gwy_expr_execute(expr, vars);
        }
    }
    g_timer_stop(timer);
    fprintf(stderr, " expr=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 200;
    gwy_expr_compile(expr, "a+b", NULL);
    g_timer_start(timer);
    for (i = 0; i < n; i++)
        gwy_expr_vector_execute(expr, len, (const gdouble**)buffers, buf3);
    g_timer_stop(timer);
    fprintf(stderr, " vector=%f", 100*g_timer_elapsed(timer, NULL)/n);

    fprintf(stderr, "\n");

    /* (a+b)(a+b)/(1+2*a*b) */
    fprintf(stderr, "(a+b)(..");
    n = 200;
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++) {
            gdouble a = buf1[j];
            gdouble b = buf2[j];

            buf3[j] = (a+b)*(a+b)/(1+2*a*b);
        }
    }
    g_timer_stop(timer);
    fprintf(stderr, " direct=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 50;
    gwy_expr_compile(expr, "(a+b)(a+b)/(1+2*a*b)", NULL);
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++) {
            vars[1] = buf1[j];
            vars[2] = buf2[j];
            buf3[j] = gwy_expr_execute(expr, vars);
        }
    }
    g_timer_stop(timer);
    fprintf(stderr, " expr=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 50;
    gwy_expr_compile(expr, "(a+b)(a+b)/(1+2*a*b)", NULL);
    g_timer_start(timer);
    for (i = 0; i < n; i++)
        gwy_expr_vector_execute(expr, len, (const gdouble**)buffers, buf3);
    g_timer_stop(timer);
    fprintf(stderr, " vector=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 200;
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++) {
            buf3[j] = buf1[j] + buf2[j];
        }
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++) {
            buf3[j] *= buf3[j];
        }
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++) {
            buf3[j] /= 1.0 + 2*buf1[j]*buf2[j];
        }
    }
    g_timer_stop(timer);
    fprintf(stderr, " serial=%f", 100*g_timer_elapsed(timer, NULL)/n);

    fprintf(stderr, "\n");

    /* log x */
    fprintf(stderr, "log x   ");
    n = 100;
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++)
            buf3[j] = log(buf1[j]);
    }
    g_timer_stop(timer);
    fprintf(stderr, " direct=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 50;
    gwy_expr_compile(expr, "log x", NULL);
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++) {
            vars[1] = buf1[j];
            buf3[j] = gwy_expr_execute(expr, vars);
        }
    }
    g_timer_stop(timer);
    fprintf(stderr, " expr=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 50;
    gwy_expr_compile(expr, "log x", NULL);
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++) {
            vars[1] = buf1[j];
            buf3[j] = gwy_expr_execute(expr, vars);
        }
    }
    g_timer_stop(timer);
    fprintf(stderr, " vector=%f", 100*g_timer_elapsed(timer, NULL)/n);

    fprintf(stderr, "\n");

    /* max(a,b) */
    fprintf(stderr, "max a,b ");
    n = 1000;
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++)
            buf3[j] = MAX(buf1[j], buf2[j]);
    }
    g_timer_stop(timer);
    fprintf(stderr, " direct=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 200;
    gwy_expr_compile(expr, "max a,b", NULL);
    g_timer_start(timer);
    for (i = 0; i < n; i++) {
        for (j = 0; j < len; j++) {
            vars[1] = buf1[j];
            vars[2] = buf2[j];
            buf3[j] = gwy_expr_execute(expr, vars);
        }
    }
    g_timer_stop(timer);
    fprintf(stderr, " expr=%f", 100*g_timer_elapsed(timer, NULL)/n);

    n = 200;
    gwy_expr_compile(expr, "max a,b", NULL);
    g_timer_start(timer);
    for (i = 0; i < n; i++)
        gwy_expr_vector_execute(expr, len, (const gdouble**)buffers, buf3);
    g_timer_stop(timer);
    fprintf(stderr, " vector=%f", 100*g_timer_elapsed(timer, NULL)/n);

    fprintf(stderr, "\n");

    g_timer_destroy(timer);
    gwy_expr_free(expr);
    g_free(buf1);
    g_free(buf2);
    g_free(buf3);
}

static int
compare_double(gconstpointer a, gconstpointer b)
{
    gdouble p = *(gdouble*)a;
    gdouble q = *(gdouble*)b;

    if (p < q)
        return -1;
    if (p > q)
        return 1;
    return 0;
}

static void
test_sort(void)
{
    GRand *rng;
    GTimer *timer;
    gdouble *array;
    guint n, i, k, N = 4096;
    gdouble libcsort = 0.0, gwysort = 0.0;

    g_message("====== SORT ======================");

    rng = g_rand_new_with_seed(42);
    timer = g_timer_new();
    array = g_new(gdouble, 2*N);

    fprintf(stderr, "Sorting");
    for (k = 0; k < 50000; k++) {
        n = g_rand_int_range(rng, 2, N);
        for (i = 0; i < n; i++)
            array[i] = g_rand_double(rng);
        memcpy(array + N, array, N*sizeof(gdouble));

        g_timer_start(timer);
        gwy_math_sort(n, array);
        g_timer_stop(timer);
        gwysort += g_timer_elapsed(timer, NULL);

        for (i = 1; i < n; i++) {
            if (array[i] < array[i-1])
                g_warning("Badly sorted item at pos %u", i);
        }

        g_timer_start(timer);
        qsort(array + N, n, sizeof(gdouble), compare_double);
        g_timer_stop(timer);
        libcsort += g_timer_elapsed(timer, NULL);
        if (i % 1000 == 0)
            putc('.', stderr);
    }
    putc('\n', stderr);

    fprintf(stderr, "libc sort: %f\n", libcsort);
    fprintf(stderr, "gwy sort: %f\n", gwysort);

    g_free(array);
    g_rand_free(rng);
    g_timer_destroy(timer);
}

typedef struct {
    const gchar *p;
    const gchar *q;
} FooType;

static void
gwyenum_print(gpointer idx, gpointer item, G_GNUC_UNUSED gpointer data)
{
    guint i = GPOINTER_TO_UINT(idx);
    const GwyEnum *e = (const GwyEnum*)item;

    fprintf(stderr, "[%d] <%s> = <%d>\n", i, e->name, e->value);
}

static void
gwyenum_changed(GwyInventory *inventory, gint i, G_GNUC_UNUSED gpointer data)
{
    const GwyEnum *e;

    e = gwy_inventory_get_nth_item(inventory, i);
    fprintf(stderr, "[changed:%d] <%s> = <%d>\n", i, e->name, e->value);
}

static const gchar*
foo_get_name(gpointer item)
{
    return ((FooType*)item)->q;
}

gint
foo_compare(gconstpointer item1,
            gconstpointer item2)
{
    gint i;

    if ((i = strcmp(((FooType*)item1)->q, ((FooType*)item2)->q)))
        return i;
    return strcmp(((FooType*)item1)->p, ((FooType*)item2)->p);
}

static void
foo_changed(GwyInventory *inventory, gint i)
{
    const FooType *fp;

    fp = gwy_inventory_get_nth_item(inventory, i);
    fprintf(stderr, "[changed:%d] <%s> <%s>\n", i, fp->p, fp->q);
}

static void
foo_inserted(GwyInventory *inventory, gint i)
{
    const FooType *fp;

    fp = gwy_inventory_get_nth_item(inventory, i);
    fprintf(stderr, "[inserted:%d] <%s> <%s>\n", i, fp->p, fp->q);
}

static void
foo_deleted(G_GNUC_UNUSED GwyInventory *inventory, gint i)
{
    fprintf(stderr, "[deleted:%d]\n", i);
}

static const GType foo_traits[] = { G_TYPE_STRING };
static const gchar* foo_trait_names[] = { "first-name" };

static const GType*
foo_get_traits(gint *n)
{
    *n = G_N_ELEMENTS(foo_traits);
    return foo_traits;
}

static const gchar*
foo_get_trait_name(gint n)
{
    g_assert(n == 0);
    return foo_trait_names[n];
}

static void
foo_get_trait_value(gpointer item,
                    gint n,
                    GValue *value)
{
    g_assert(n == 0);
    g_value_init(value, foo_traits[n]);
    g_value_set_static_string(value, ((FooType*)item)->p);
}

static void
foo_reordered(GwyInventory *inventory, gpointer p)
{
    gint *new_order = (gint*)p;
    const FooType *fp;
    gint i, n;

    n = gwy_inventory_get_n_items(inventory);
    fprintf(stderr, "new_order:\n");
    for (i = 0; i < n; i++) {
        fp = gwy_inventory_get_nth_item(inventory, i);
        fprintf(stderr, "%d -> %d <%s>\n", new_order[i], i, fp->q);
    }
}

static void
foo_print(gpointer idx, gpointer item, gpointer inv)
{
    guint i = GPOINTER_TO_UINT(idx);
    GwyInventory *inventory = (GwyInventory*)inv;
    const GwyInventoryItemType *item_type;
    GValue value;

    item_type = gwy_inventory_get_item_type(inventory);
    fprintf(stderr, "[%d]", i);
    if (item_type->get_name)
        fprintf(stderr, " name:<%s>", item_type->get_name(item));
    memset(&value, 0, sizeof(value));
    item_type->get_trait_value(item, 0, &value);
    fprintf(stderr, " <%s>:<%s>",
            item_type->get_trait_name(0), g_value_get_string(&value));
    g_value_unset(&value);
    fprintf(stderr, "\n");
}

static void
test_inventory(void)
{
    static const GwyEnum table[] = {
        { "Goldfish", 1 },
        { "Daffodil", 4 },
        { "Convolvulus", 3 },
        { "Reeve", 0 },
        { "Carabid", 16 },
    };
    GwyInventoryItemType foo_item_type = {
        0,
        NULL,
        NULL,
        foo_get_name,
        foo_compare,
        NULL,
        foo_get_traits,
        foo_get_trait_name,
        foo_get_trait_value,
    };
    FooType *pf;
    FooType some_foos[] = {
        { "Terry", "Gilliam" },
        { "Terry", "Jones" },
        { "Graham", "Chapman" },
    };
    gpointer foo_items[] = { some_foos, some_foos+1, some_foos+2 };
    GwyInventory *inventory;
    const GwyEnum *e;

    g_message("====== INVENTORY ======================");
    /* Constant */
    inventory = gwy_enum_inventory_new(table, G_N_ELEMENTS(table));
    g_signal_connect(inventory, "item-updated",
                     G_CALLBACK(gwyenum_changed), NULL);

    e = gwy_inventory_get_item(inventory, "Daffodil");
    if (!e)
        g_warning("<Daffodil> not found");
    else
        fprintf(stderr, "<%s> = <%d>\n", e->name, e->value);

    e = gwy_inventory_get_item_or_default(inventory, "Grannom");
    if (!e)
        g_warning("<Grannom> neither default not found");
    else
        fprintf(stderr, "default: <%s> = <%d>\n", e->name, e->value);

    gwy_inventory_set_default_item(inventory, "Carabid");
    e = gwy_inventory_get_item_or_default(inventory, "Grannom");
    if (!e)
        g_warning("<Grannom> neither default not found");
    else
        fprintf(stderr, "default: <%s> = <%d>\n", e->name, e->value);

    gwy_inventory_foreach(inventory, gwyenum_print, NULL);
    gwy_inventory_item_updated(inventory, "Reeve");
    gwy_inventory_nth_item_updated(inventory, 2);
    gwy_inventory_item_updated(inventory, "Grannom");
    gwy_inventory_insert_item(inventory, (gpointer)(table + 2));
    g_object_unref(inventory);

    /* Foo struct */
    inventory = gwy_inventory_new_filled(&foo_item_type,
                                         G_N_ELEMENTS(some_foos), foo_items);
    g_signal_connect(inventory, "item-updated",
                     G_CALLBACK(foo_changed), NULL);
    g_signal_connect(inventory, "item-inserted",
                     G_CALLBACK(foo_inserted), NULL);
    g_signal_connect(inventory, "item-deleted",
                     G_CALLBACK(foo_deleted), NULL);
    g_signal_connect(inventory, "items-reordered",
                     G_CALLBACK(foo_reordered), NULL);

    pf = gwy_inventory_get_item(inventory, "Gilliam");
    if (!pf)
        g_warning("<Gilliam> not found");
    else
        fprintf(stderr, "<%s>: <%s>\n", pf->q, pf->p);

    pf = gwy_inventory_get_item_or_default(inventory, "Smith");
    if (!pf)
        g_warning("<Smith> neither default not found");
    else
        fprintf(stderr, "default: <%s>: <%s>\n", pf->q, pf->p);

    gwy_inventory_set_default_item(inventory, "Idle");
    pf = gwy_inventory_get_item_or_default(inventory, "Smith");
    if (!pf)
        g_warning("<Smith> neither default not found");
    else
        fprintf(stderr, "default: <%s>: <%s>\n", pf->q, pf->p);

    gwy_inventory_foreach(inventory, foo_print, inventory);

    pf = g_new(FooType, 1);
    pf->p = "Eric";
    pf->q = "Idle";
    gwy_inventory_insert_item(inventory, pf);
    pf = g_new(FooType, 1);
    pf->p = "John";
    pf->q = "Cleese";
    gwy_inventory_insert_item(inventory, pf);
    pf = g_new(FooType, 1);
    pf->p = "Michael";
    pf->q = "Palin";
    gwy_inventory_insert_item(inventory, pf);

    pf = gwy_inventory_get_item_or_default(inventory, "Smith");
    if (!pf)
        g_warning("<Smith> neither default not found");
    else
        fprintf(stderr, "default: <%s>: <%s>\n", pf->q, pf->p);

    gwy_inventory_foreach(inventory, foo_print, inventory);
    gwy_inventory_item_updated(inventory, "Palin");
    gwy_inventory_nth_item_updated(inventory, 2);
    gwy_inventory_item_updated(inventory, "Smith");

    gwy_inventory_restore_order(inventory);
    gwy_inventory_foreach(inventory, foo_print, inventory);

    gwy_inventory_delete_item(inventory, "Chapman");
    gwy_inventory_foreach(inventory, foo_print, inventory);
    gwy_inventory_delete_item(inventory, "Palin");
    gwy_inventory_delete_item(inventory, "Smith");
    gwy_inventory_foreach(inventory, foo_print, inventory);

    gwy_inventory_nth_item_updated(inventory, 2);
    gwy_inventory_rename_item(inventory, "Idle", "Clapton");

    pf = g_new(FooType, 1);
    pf->p = "Graham";
    pf->q = "Chapman";
    gwy_inventory_insert_item(inventory, pf);
    gwy_inventory_foreach(inventory, foo_print, inventory);

    g_object_unref(inventory);
}

G_GNUC_UNUSED static void
test_all(void)
{
    test_serializable_iface();
    test_container();
    test_container_serialization();
    test_duplication();
    test_entities();
    test_enums();
    test_math();
    test_string_utils();
    test_nlfit();
    test_path_normalization();
    test_si_unit();
    test_si_unit_format();
    test_si_unit_parse();
    test_si_unit_err();
    test_expr();
    test_expr_err();
    test_expr_speed();
    test_sort();
    test_inventory();
}

int
main(void)
{
    g_type_init();
    g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, log_handler, NULL);
    test_inventory();

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
