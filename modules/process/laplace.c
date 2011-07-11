/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/correct.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define LAPLACE_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean module_register(void);
static void     laplace        (GwyContainer *data,
                                GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Removes data under mask, "
       "interpolating them with Laplace equation solution."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("laplace",
                              (GwyProcessFunc)&laplace,
                              N_("/_Correct Data/_Remove Data Under Mask"),
                              GWY_STOCK_REMOVE_UNDER_MASK,
                              LAPLACE_RUN_MODES,
                              GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA,
                              N_("Interpolate data under mask by solution of "
                                 "Laplace equation"));

    return TRUE;
}


static void
laplace(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield, *buffer;
    GQuark dquark;
    gdouble error, cor, maxer, lastfrac, frac, starter;
    gint i, id;
    gboolean cancelled = FALSE;

    g_return_if_fail(run & LAPLACE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && dquark && mfield);

    maxer = gwy_data_field_get_rms(dfield)/1.0e4;
    gwy_app_wait_start(gwy_app_find_window_for_channel(data, id),
                       _("Laplace correction"));

    dfield = gwy_data_field_duplicate(dfield);
    buffer = gwy_data_field_new_alike(dfield, TRUE);
    gwy_data_field_correct_average(dfield, mfield);

    cor = 0.2;
    error = 0.0;
    lastfrac = 0.0;
    starter = 0.0;
    for (i = 0; i < 5000; i++) {
        gwy_data_field_correct_laplace_iteration(dfield, mfield, buffer,
                                                 cor, &error);
        if (error < maxer)
            break;
        if (!i)
            starter = error;

        frac = log(error/starter)/log(maxer/starter);
        if ((i/(gdouble)(5000)) > frac)
            frac = i/(gdouble)(5000);
        if (lastfrac > frac)
            frac = lastfrac;

        if (!gwy_app_wait_set_fraction(frac)) {
            cancelled = TRUE;
            break;
        }
        lastfrac = frac;
    }
    gwy_app_wait_finish();
    if (!cancelled) {
        gwy_app_undo_qcheckpointv(data, 1, &dquark);
        gwy_container_set_object(data, dquark, dfield);
    }
    g_object_unref(dfield);
    g_object_unref(buffer);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
