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

#ifndef __GWY_TEST_SER_H__
#define __GWY_TEST_SER_H__

#include <glib-object.h>

#include "gwyserializable.h"
#include "gwywatchable.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_TEST_SER                  (gwy_test_ser_get_type())
#define GWY_TEST_SER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TEST_SER, GwyTestSer))
#define GWY_TEST_SER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_TEST_SER, GwyTestSerClass))
#define GWY_IS_TEST_SER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TEST_SER))
#define GWY_IS_TEST_SER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_TEST_SER))
#define GWY_TEST_SER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TEST_SER, GwyTestSerClass))


typedef struct _GwyTestSer GwyTestSer;
typedef struct _GwyTestSerClass GwyTestSerClass;

struct _GwyTestSer {
    GObject parent_instance;

    gdouble theta;
    gdouble *radius;
    gsize history_size;
};

struct _GwyTestSerClass {
    GObjectClass parent_class;
};


GType      gwy_test_ser_get_type       (void) G_GNUC_CONST;
GObject*   gwy_test_ser_new            (gdouble theta,
                                        gdouble radius);
void       gwy_test_ser_set_radius     (GwyTestSer *test_ser,
                                        gdouble radius);
void       gwy_test_ser_set_theta      (GwyTestSer *test_ser,
                                        gdouble theta);
gdouble    gwy_test_ser_get_radius     (GwyTestSer *test_ser);
gdouble    gwy_test_ser_get_theta      (GwyTestSer *test_ser);
void       gwy_test_ser_print_history  (GwyTestSer *test_ser);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_TEST_SER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
