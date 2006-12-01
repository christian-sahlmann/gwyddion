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
#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libprocess/hough.h>
#include <libprocess/level.h>
#include <libprocess/grains.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define EDGE_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean    module_register              (void);
static void        edge                         (GwyContainer *data,
                                                 GwyRunType run,
                                                 const gchar *name);
static void        laplacian_do                 (GwyDataField *dfield,
                                                 GwyDataField *show);
static void        canny_do                     (GwyDataField *dfield,
                                                 GwyDataField *show);
static void        rms_do                       (GwyDataField *dfield,
                                                 GwyDataField *show);
static void        rms_edge_do                  (GwyDataField *dfield,
                                                 GwyDataField *show);
static void        nonlinearity_do              (GwyDataField *dfield,
                                                 GwyDataField *show);
static void        hough_lines_do               (GwyDataField *dfield,
                                                 GwyDataField *show);
static void        harris_do                    (GwyDataField *dfield,
                                                 GwyDataField *show);
static void        inclination_do               (GwyDataField *dfield,
                                                 GwyDataField *show);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Several edge detection methods (Laplacian of Gaussian, Canny, "
       "and some experimental), creates presentation."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("edge_laplacian",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Laplacian of Gaussian"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Laplacian of Gaussian edge detection "
                                 "presentation"));
    gwy_process_func_register("edge_canny",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Canny"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Canny edge detection presentation"));
    gwy_process_func_register("edge_rms",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_RMS"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Local RMS value based edge detection "
                                 "presentation"));
    gwy_process_func_register("edge_rms_edge",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/RMS _Edge"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Local RMS value based edge detection "
                                 "with postprocessing"));
    gwy_process_func_register("edge_nonlinearity",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/Local _Nonlinearity"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Local nonlinearity based edge detection "
                                 "presentation"));
    gwy_process_func_register("edge_hough_lines",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Hough Lines"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              /* FIXME */
                              N_("Hough lines presentation"));
    gwy_process_func_register("edge_harris",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Harris Corner"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              /* FIXME */
                              N_("Harris corner presentation"));
    gwy_process_func_register("edge_inclination",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Inclination"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Local inclination based edge detection "
                                 "presentation"));

    return TRUE;
}

static void
edge(GwyContainer *data, GwyRunType run, const gchar *name)
{
    static const struct {
        const gchar *name;
        void (*func)(GwyDataField *dfield, GwyDataField *show);
    }
    functions[] = {
        { "edge_canny",        canny_do,        },
        { "edge_harris",       harris_do,       },
        { "edge_hough_lines",  hough_lines_do,  },
        { "edge_inclination",  inclination_do,  },
        { "edge_laplacian",    laplacian_do,    },
        { "edge_nonlinearity", nonlinearity_do, },
        { "edge_rms",          rms_do,          },
        { "edge_rms_edge",     rms_edge_do,     },
    };
    GwyDataField *dfield, *showfield;
    GQuark dquark, squark;
    GwySIUnit *siunit;
    gint id;
    guint i;

    g_return_if_fail(run & EDGE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     GWY_APP_SHOW_FIELD, &showfield,
                                     0);
    g_return_if_fail(dfield && dquark && squark);

    gwy_app_undo_qcheckpointv(data, 1, &squark);
    if (!showfield) {
        showfield = gwy_data_field_new_alike(dfield, FALSE);
        siunit = gwy_si_unit_new(NULL);
        gwy_data_field_set_si_unit_z(showfield, siunit);
        g_object_unref(siunit);
        gwy_container_set_object(data, squark, showfield);
        g_object_unref(showfield);
    }

    for (i = 0; i < G_N_ELEMENTS(functions); i++) {
        if (gwy_strequal(name, functions[i].name)) {
            functions[i].func(dfield, showfield);
            break;
        }
    }
    if (i == G_N_ELEMENTS(functions)) {
        g_warning("edge does not provide function `%s'", name);
        gwy_data_field_copy(dfield, showfield, FALSE);
    }

    gwy_data_field_normalize(showfield);
    gwy_data_field_data_changed(showfield);
}

static void
laplacian_do(GwyDataField *dfield, GwyDataField *show)
{
    gint xres, yres, i, j;
    gdouble avg, *data;

    gwy_data_field_copy(dfield, show, FALSE);
    xres = gwy_data_field_get_xres(show);
    yres = gwy_data_field_get_yres(show);
    gwy_data_field_filter_laplacian(show);
    avg = gwy_data_field_area_get_avg(show, NULL, 1, 1, xres-2, yres-2);
    data = gwy_data_field_get_data(show);

    for (i = 0; i < yres; i++) {
        data[xres*i] = avg;
        data[xres - 1 + xres*i] = avg;
    }
    for (j = 0; j < xres; j++) {
        data[j] = avg;
        data[j + xres*(yres - 1)] = avg;
    }
}

static void
canny_do(GwyDataField *dfield, GwyDataField *show)
{
    /*now we use fixed threshold, but in future, there could be API
     with some setting. We could also do smooting before apllying filter.*/
    gwy_data_field_copy(dfield, show, FALSE);
    gwy_data_field_filter_canny(show, 0.1);
}

static void
rms_do(GwyDataField *dfield, GwyDataField *show)
{
    gwy_data_field_copy(dfield, show, FALSE);
    gwy_data_field_filter_rms(show, 5);
}

static void
rms_edge_do(GwyDataField *dfield, GwyDataField *show)
{
    GwyDataField *tmp;
    gint xres, yres, i, j;
    gdouble *d;
    const gdouble *t;
    gdouble s;

    gwy_data_field_copy(dfield, show, FALSE);
    xres = gwy_data_field_get_xres(show);
    yres = gwy_data_field_get_yres(show);

    tmp = gwy_data_field_duplicate(show);
    gwy_data_field_filter_rms(tmp, 5);
    t = gwy_data_field_get_data_const(tmp);
    d = gwy_data_field_get_data(show);
    for (i = 0; i < yres; i++) {
        gint iim = MAX(i-2, 0)*xres;
        gint iip = MIN(i+2, yres-1)*xres;
        gint ii = i*xres;

        for (j = 0; j < xres; j++) {
            gint jm = MAX(j-2, 0);
            gint jp = MIN(j+2, xres-1);

            s = t[ii + jm] + t[ii + jp] + t[iim + j] + t[iip + j]
                + (t[iim + jm] + t[iim + jp] + t[iip + jm] + t[iip + jp])/2.0;
            s /= 6.0;

            d[ii + j] = MAX(t[ii + j] - s, 0);
        }
    }
    g_object_unref(tmp);
}

static void
nonlinearity_do(GwyDataField *dfield, GwyDataField *show)
{
    gint xres, yres, i;
    gdouble *data;

    xres = gwy_data_field_get_xres(show);
    yres = gwy_data_field_get_yres(show);
    gwy_data_field_local_plane_quantity(dfield, 5, GWY_PLANE_FIT_S0_REDUCED,
                                        show);
    data = gwy_data_field_get_data(show);
    for (i = 0; i < xres*yres; i++)
        data[i] = sqrt(data[i]);
}

static void
inclination_do(GwyDataField *dfield, GwyDataField *show)
{
    GwyPlaneFitQuantity quantites[] = {
        GWY_PLANE_FIT_BX, GWY_PLANE_FIT_BY
    };
    GwyDataField *fields[2];
    gint xres, yres, i;
    gdouble *data, *xdata;
    gdouble qx, qy;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    qx = gwy_data_field_get_xmeasure(dfield);
    qy = gwy_data_field_get_ymeasure(dfield);

    fields[0] = gwy_data_field_new_alike(dfield, FALSE);
    fields[1] = show;
    gwy_data_field_fit_local_planes(dfield, 3, 2, quantites, fields);

    data = gwy_data_field_get_data(show);
    xdata = gwy_data_field_get_data(fields[0]);
    for (i = 0; i < xres*yres; i++)
        data[i] = atan(hypot(xdata[i]/qx, data[i]/qy));

    g_object_unref(fields[0]);
}

static void
hough_lines_do(GwyDataField *dfield, GwyDataField *show)
{
    GwyDataField *x_gradient, *y_gradient;

    gwy_data_field_copy(dfield, show, FALSE);
    gwy_data_field_filter_canny(show, 0.1);

    x_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(x_gradient, GWY_ORIENTATION_HORIZONTAL);
    y_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(y_gradient, GWY_ORIENTATION_VERTICAL);

    gwy_data_field_hough_line_strenghten(show, x_gradient, y_gradient,
                                           1, 0.2);
}
static void
harris_do(GwyDataField *dfield, GwyDataField *show)
{
    GwyDataField *x_gradient, *y_gradient;

    gwy_data_field_copy(dfield, show, FALSE);
    x_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(x_gradient, GWY_ORIENTATION_HORIZONTAL);
    y_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(y_gradient, GWY_ORIENTATION_VERTICAL);

    gwy_data_field_filter_harris(x_gradient, y_gradient, show, 20, 0.1);
    /*gwy_data_field_clear(show);
    gwy_data_field_grains_mark_watershed_minima(ble, show, 1, 0, 5,
                       0.05*(gwy_data_field_get_max(ble) - gwy_data_field_get_min(ble)));
*/
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
