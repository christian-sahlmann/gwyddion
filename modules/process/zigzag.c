/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti), Daniil Bratashov (dn2010)
 *  E-mail: yeti@gwyddion.net, dn2010@gmail.com
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

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyversion.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libprocess/gwyprocess.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include "config.h"

#define ZIGZAG_RUN_MODES (GWY_RUN_IMMEDIATE)


static gboolean    module_register                (void);
static void        zigzag                         (GwyContainer *data,
                                                   GwyRunType run);
static gint        data_line_mindiff              (GwyDataLine *line,
                                                   GwyDataLine *kernel);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Fixes zigzag-like interlacing pattern"),
    "Daniil Bratashov <dn2010@gmail.com>",
    "0.1",
    "David Nečas (Yeti) & Daniil Bratashov (dn2010)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("zigzag",
                              (GwyProcessFunc)&zigzag,
                              N_("/_Correct Data/_Zigzag"),
                              NULL,
                              ZIGZAG_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Fix zigzag interlacing pattern"));

    return TRUE;
}

static void
zigzag(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GwyDataLine *line, *kernel;
    GQuark dquark;
    gint i, j, id, xres, yres, pos;
    gdouble *p, *lp;

    g_return_if_fail(run & ZIGZAG_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    gwy_app_undo_qcheckpointv(data, 1, &dquark);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    line = gwy_data_line_new(xres, xres, TRUE);
    kernel = gwy_data_line_new(xres/2, xres/2, TRUE);
    gwy_data_field_get_row(dfield, line,  0);
    p = gwy_data_field_get_data(dfield);
    gwy_data_field_get_row(dfield, line, 0);

    for (i = 1; i < yres; i++) {
        gwy_data_field_get_row_part(dfield, kernel, i,
                                                xres / 8, xres * 7 / 8);
        pos = data_line_mindiff(line, kernel);
        gwy_data_field_get_row(dfield, line, i);
        lp = gwy_data_line_get_data(line);
        if (pos >= xres / 2)
            for (j = pos - xres / 2; j < xres; j++)
                *(p + i * xres + j) = *(lp++);
        else {
            lp += xres / 2 - pos;
            for (j = 0; j < xres / 2 + pos; j++)
                *(p + i * xres + j) = *(lp++);
        }
    }

    gwy_data_field_data_changed(dfield);
    gwy_app_channel_log_add_proc(data, id, id);
}

static gint
data_line_mindiff(GwyDataLine *line, GwyDataLine *kernel)
{
    gint len, klen, i, j, mini;
    gdouble sum, min;
    gdouble *ldata, *kdata;

    len = gwy_data_line_get_res(line);
    klen = gwy_data_line_get_res(kernel);
    ldata = gwy_data_line_get_data(line);
    kdata = gwy_data_line_get_data(kernel);
    min = G_MAXDOUBLE;
    mini = len / 2 - klen / 2;
    for (i = 0; i < len - klen; i++) {
        sum = 0;
        for (j = 0; j < klen; j++)
            sum += (*(kdata + j) - *(ldata + i + j))
                                     *(*(kdata + j) - *(ldata + i + j));
        /* damping factor, preventing large offsets */
        sum += 0.03 * (mini + klen / 2 - len / 2)
                             * (mini + klen / 2 - len / 2) * sum / klen;
        if (sum < min) {
            min = sum;
            mini = i;
        }
    }

    return mini + klen / 2;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
