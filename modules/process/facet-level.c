/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/app.h>

#define LEVEL_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register            (const gchar *name);
static gboolean    facet_level                (GwyContainer *data,
                                               GwyRunType run);
static void        facet_level_coeffs         (GwyDataField *dfield,
                                               gdouble *bx, gdouble *by);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "facet_level",
    "Automatic facet-orientation based levelling.",
    "Yeti <yeti@physics.muni.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo facet_level_func_info = {
        "facet_level",
        "/_Level/_Facet Level",
        (GwyProcessFunc)&facet_level,
        LEVEL_RUN_MODES,
    };

    gwy_process_func_register(name, &facet_level_func_info);

    return TRUE;
}

static gboolean
facet_level(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    gdouble c, bx, by;
    gint i;

    g_return_val_if_fail(run & LEVEL_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data");

    /* to converge, do it twice */
    for (i = 0; i < 2; i++) {
        facet_level_coeffs(dfield, &bx, &by);
        c = -0.5*(bx*gwy_data_field_get_xreal(dfield)
                  + by*gwy_data_field_get_yreal(dfield));
        gwy_data_field_plane_level(dfield, c, bx, by);
    }

    return TRUE;
}

static void
facet_level_coeffs(GwyDataField *dfield, gdouble *bx, gdouble *by)
{
    gdouble *data, *row, *newrow;
    gdouble vx, vy, q, sumvx, sumvy, sumvz, xr, yr;
    gint xres, yres, i, j;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    if (xres < 2 || yres < 2) {
        g_warning("Cannot facet-level datafield with dimension less than 2");
        *bx = *by = 0;
        return;
    }
    xr = gwy_data_field_get_xreal(dfield)/xres;
    yr = gwy_data_field_get_yreal(dfield)/yres;

    data = gwy_data_field_get_data(dfield);
    sumvx = sumvy = sumvz = 0.0;
    newrow = data;
    for (i = 1; i < yres; i++) {
        row = newrow;
        newrow += xres;

        for (j = 1; j < xres; j++) {
            vx = 0.5*(newrow[j] + row[j] - newrow[j-1] - row[j-1])/xr;
            vy = 0.5*(newrow[j-1] + newrow[j] - row[j-1] - row[j])/yr;
            /* XXX: braindamaged heuristics; I thought q alone (i.e., normal
             * normalization) whould give nice facet leveling, but alas! the
             * higher norm values has to be suppressed much more -- it seems */
            q = exp(20.0*(vx*vx + vy*vy));
            sumvx += vx/q;
            sumvy += vy/q;
            sumvz -= 1.0/q;
        }
    }
    q = sumvz/-1.0;
    *bx = sumvx/q;
    *by = sumvy/q;
    gwy_debug("(%g, %g, %g) %g (%g, %g)", sumvx, sumvy, sumvz, q, *bx, *by);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
