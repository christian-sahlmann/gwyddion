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

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libprocess/correct.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/gwyapp.h>
#include <app/wait.h>

#define LAPLACE_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)


static gboolean    module_register            (const gchar *name);
static gboolean    laplace                        (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Removes data under mask, "
       "interpolating them with Laplace equation solution."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo laplace_func_info = {
        "laplace",
        N_("/_Correct Data/_Remove Data Under Mask"),
        (GwyProcessFunc)&laplace,
        LAPLACE_RUN_MODES,
        GWY_MENU_FLAG_DATA_MASK,
    };

    gwy_process_func_register(name, &laplace_func_info);

    return TRUE;
}


static gboolean
laplace(GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog;
    GwyDataField *dfield, *maskfield, *buffer;
    gdouble error, cor, maxer, lastfrac, frac, starter;
    gint i;

    g_assert(run & LAPLACE_RUN_MODES);

    if (gwy_container_contains_by_name(data, "/0/mask")) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        maskfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                    "/0/mask"));
        buffer = gwy_data_field_new_alike(dfield, TRUE);

        gwy_app_undo_checkpoint(data, "/0/data", "/0/mask", NULL);

        cor = 0.2;
        error = 0;
        maxer = gwy_data_field_get_rms(dfield)/1.0e4;
        gwy_app_wait_start(GTK_WIDGET(gwy_app_data_window_get_for_data(data)),
                           _("Laplace correction"));

        gwy_data_field_correct_average(dfield, maskfield);

        lastfrac = 0;
        starter = 0;
        for (i = 0; i < 5000; i++) {
            gwy_data_field_correct_laplace_iteration(dfield, maskfield, buffer,
                                                     &error, &cor);
            if (error < maxer)
                break;
            if (i==0)
                starter = error;

            frac = log(error/starter)/log(maxer/starter);
            if ((i/(gdouble)(5000)) > frac)
                frac = i/(gdouble)(5000);
            if (lastfrac > frac)
                frac = lastfrac;

            if (!gwy_app_wait_set_fraction(frac))
                break;
            lastfrac = frac;
        }
        gwy_app_wait_finish();

        gwy_container_remove_by_name(data, "/0/mask");
        g_object_unref(buffer);
    }
    else
    {
        /* XXX: this should not happen in the first place! */
        dialog = gtk_message_dialog_new
            (GTK_WINDOW(gwy_app_data_window_get_for_data(data)),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_INFO,
             GTK_BUTTONS_CLOSE,
             _("There is no mask to be used for computation."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;

    }
    return TRUE;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
