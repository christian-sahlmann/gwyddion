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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libprocess/correct.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define LAPLACE_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean module_register(const gchar *name);
static void     laplace        (GwyContainer *data,
                                GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Removes data under mask, "
       "interpolating them with Laplace equation solution."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_process_func_register("laplace",
                              (GwyProcessFunc)&laplace,
                              N_("/_Correct Data/_Remove Data Under Mask"),
                              NULL,
                              LAPLACE_RUN_MODES,
                              GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA,
                              N_("Interpolate data under mask by solution of "
                                 "Laplace equation"));

    return TRUE;
}


static void
laplace(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield, *buffer, *old;
    GQuark dquark;
    gdouble error, cor, maxer, lastfrac, frac, starter;
    gint i;
    gboolean cancelled = FALSE;

    g_return_if_fail(run & LAPLACE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield && dquark && mfield);

    maxer = gwy_data_field_get_rms(dfield)/1.0e4;
    gwy_app_wait_start(GTK_WIDGET(gwy_app_data_window_get_for_data(data)),
                       _("Laplace correction"));

    old = dfield;
    dfield = gwy_data_field_duplicate(dfield);
    buffer = gwy_data_field_new_alike(dfield, TRUE);
    gwy_data_field_correct_average(dfield, mfield);

    cor = 0.2;
    error = 0.0;
    lastfrac = 0.0;
    starter = 0.0;
    for (i = 0; i < 5000; i++) {
        gwy_data_field_correct_laplace_iteration(dfield, mfield, buffer,
                                                 &error, &cor);
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
        gwy_data_field_copy(dfield, old, FALSE);
        gwy_data_field_data_changed(old);
    }
    g_object_unref(dfield);
    g_object_unref(buffer);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
