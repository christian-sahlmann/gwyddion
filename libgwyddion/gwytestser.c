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

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwysiunit.h>
#include "gwytestser.h"

#define GWY_TEST_SER_TYPE_NAME "GwyTestSer"

enum {
    VALUE_CHANGED,
    LAST_SIGNAL
};

static void        gwy_test_ser_finalize          (GObject *object);
static void        gwy_test_ser_serializable_init (GwySerializableIface *iface);
static GByteArray* gwy_test_ser_serialize         (GObject *obj,
                                                   GByteArray *buffer);
static GObject*    gwy_test_ser_deserialize       (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static void        gwy_test_ser_value_changed     (GObject *test_ser);

static guint test_ser_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwyTestSer, gwy_test_ser, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_test_ser_serializable_init))

static void
gwy_test_ser_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_test_ser_serialize;
    iface->deserialize = gwy_test_ser_deserialize;
    iface->duplicate = NULL;  /* don't have one */
}

static void
gwy_test_ser_class_init(GwyTestSerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    gobject_class->finalize = gwy_test_ser_finalize;

    test_ser_signals[VALUE_CHANGED]
        = g_signal_new("value-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyTestSerClass, value_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_test_ser_init(GwyTestSer *test_ser)
{
    gwy_debug("");
}

static void
gwy_test_ser_finalize(GObject *object)
{
    GwyTestSer *test_ser = (GwyTestSer*)object;

    gwy_debug("");
    if (test_ser->radius)
        g_array_free(test_ser->radius, TRUE);
    if (test_ser->string)
        g_array_free(test_ser->string, TRUE);
    if (test_ser->unit)
        g_array_free(test_ser->unit, TRUE);

    G_OBJECT_CLASS(gwy_test_ser_parent_class)->finalize(object);
}

GObject*
gwy_test_ser_new(gdouble theta,
                 gdouble radius)
{
    GwyTestSer *test_ser;

    gwy_debug("");
    test_ser = g_object_new(GWY_TYPE_TEST_SER, NULL);

    test_ser->theta = theta;
    gwy_test_ser_set_radius(test_ser, radius);

    return (GObject*)(test_ser);
}

static GByteArray*
gwy_test_ser_serialize(GObject *obj,
                       GByteArray *buffer)
{
    GwyTestSer *test_ser;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_TEST_SER(obj), NULL);

    test_ser = GWY_TEST_SER(obj);
    {
        GwySerializeSpec spec[] = {
            { 'd', "theta", &test_ser->theta, NULL, },
            { 'D', "radius", &test_ser->radius->data, &test_ser->radius->len, },
            { 'S', "string", &test_ser->string->data, &test_ser->string->len, },
            { 'O', "unit", &test_ser->unit->data, &test_ser->unit->len, },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_TEST_SER_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_test_ser_deserialize(const guchar *buffer,
                         gsize size,
                         gsize *position)
{
    double theta, *radius = NULL;
    gchar *string = NULL;
    GObject *unit = NULL;
    guint32 rsize, ssize, usize;
    GwySerializeSpec spec[] = {
        { 'd', "theta", &theta, NULL, },
        { 'D', "radius", &radius, &rsize, },
        { 'S', "string", &string, &ssize, },
        { 'O', "unit", &unit, &usize, },
    };
    GwyTestSer *test_ser;

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_TEST_SER_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)
        || !radius || !string || !unit
        || rsize != ssize || rsize != usize) {
        g_free(radius);
        g_free(string);
        g_free(unit);
        return NULL;
    }

    test_ser = (GwyTestSer*)gwy_test_ser_new(theta, 0.0);
    g_array_set_size(test_ser->radius, 0);
    g_array_set_size(test_ser->string, 0);
    g_array_set_size(test_ser->unit, 0);
    g_array_append_vals(test_ser->radius, radius, rsize);
    g_array_append_vals(test_ser->string, string, ssize);
    g_array_append_vals(test_ser->unit, unit, usize);

    g_free(radius);
    g_free(string);
    g_free(unit);

    return (GObject*)test_ser;
}

void
gwy_test_ser_set_radius(GwyTestSer *test_ser,
                        gdouble radius)
{
    gchar *s;
    GwySIUnit *u;

    gwy_debug("");

    if (!test_ser->radius) {
        test_ser->radius = g_array_new(FALSE, FALSE, sizeof(gdouble));
        test_ser->string = g_array_new(FALSE, FALSE, sizeof(gchar*));
        test_ser->unit = g_array_new(FALSE, FALSE, sizeof(GObject*));
    }

    s = g_strdup_printf("%g", radius);
    /* that's silly, get over it */
    u = gwy_si_unit_new("m");
    g_array_append_val(test_ser->radius, radius);
    g_array_append_val(test_ser->string, s);
    g_array_append_val(test_ser->unit, u);

    gwy_test_ser_value_changed(G_OBJECT(test_ser));
}

void
gwy_test_ser_set_theta(GwyTestSer *test_ser,
                       gdouble theta)
{
    gwy_debug("");

    if (theta != test_ser->theta) {
        test_ser->theta = theta;

        gwy_test_ser_value_changed(G_OBJECT(test_ser));
    }
}

gdouble
gwy_test_ser_get_radius(GwyTestSer *test_ser)
{
    gwy_debug("");

    return g_array_index(test_ser->radius, gdouble, test_ser->radius->len - 1);
}

gchar*
gwy_test_ser_get_string(GwyTestSer *test_ser)
{
    gwy_debug("");

    return g_array_index(test_ser->string, gchar*, test_ser->string->len - 1);
}

GObject*
gwy_test_ser_get_unit(GwyTestSer *test_ser)
{
    gwy_debug("");

    return g_array_index(test_ser->unit, GObject*, test_ser->unit->len - 1);
}

gdouble
gwy_test_ser_get_theta(GwyTestSer *test_ser)
{
    gwy_debug("");

    return test_ser->theta;
}

static void
gwy_test_ser_value_changed(GObject *test_ser)
{
    gwy_debug("signal: GwyTestSer changed");
    g_signal_emit_by_name(GWY_TEST_SER(test_ser), "value-changed", NULL);
}

void
gwy_test_ser_print_history(GwyTestSer *test_ser)
{
    gsize i;

    gwy_debug("");

    fprintf(stderr, "** Message: radius history:");
    for (i = 0; i < test_ser->radius->len; i++)
        fprintf(stderr, " %g", g_array_index(test_ser->radius, gdouble, i));
    fputs("\n", stderr);
    fprintf(stderr, "** Message: string history:");
    for (i = 0; i < test_ser->string->len; i++)
        fprintf(stderr, " %s", g_array_index(test_ser->string, gchar*, i));
    fputs("\n", stderr);
    fprintf(stderr, "** Message: unit history:");
    for (i = 0; i < test_ser->unit->len; i++)
        fprintf(stderr, " %s",
                gwy_si_unit_get_unit_string(g_array_index(test_ser->unit,
                                                          GwySIUnit*, i)));
    fputs("\n", stderr);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
