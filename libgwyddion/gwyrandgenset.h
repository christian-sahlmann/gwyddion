/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef __GWY_RAND_GEN_SET_H__
#define __GWY_RAND_GEN_SET_H__

#include <glib.h>
#include <libgwyddion/gwymacros.h>

G_BEGIN_DECLS

typedef struct _GwyRandGenSet GwyRandGenSet;

GwyRandGenSet* gwy_rand_gen_set_new        (guint n);
void           gwy_rand_gen_set_init       (GwyRandGenSet *rngset,
                                            guint seed);
void           gwy_rand_gen_set_free       (GwyRandGenSet *rngset);
GRand*         gwy_rand_gen_get_rng        (GwyRandGenSet *rngset,
                                            guint i);
gdouble        gwy_rand_gen_set_range      (GwyRandGenSet *rngset,
                                            guint i,
                                            gdouble lower,
                                            gdouble upper);
gdouble        gwy_rand_gen_set_uniform    (GwyRandGenSet *rngset,
                                            guint i,
                                            gdouble sigma);
gdouble        gwy_rand_gen_set_gaussian   (GwyRandGenSet *rngset,
                                            guint i,
                                            gdouble sigma);
gdouble        gwy_rand_gen_set_exponential(GwyRandGenSet *rngset,
                                            guint i,
                                            gdouble sigma);
gdouble        gwy_rand_gen_set_triangular (GwyRandGenSet *rngset,
                                            guint i,
                                            gdouble sigma);
gdouble        gwy_rand_gen_set_multiplier (GwyRandGenSet *rngset,
                                            guint i,
                                            gdouble range);
guint32        gwy_rand_gen_set_int        (GwyRandGenSet *rngset,
                                            guint i);

G_END_DECLS

#endif /* __GWY_RAND_GEN_SET_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
