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

#define DEBUG 1

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define UNROTATE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function. */
typedef struct {
    GwyInterpolationType interp;
    gint size;
    gboolean logscale;
} UnrotateArgs;

typedef struct {
    GtkObject *size;
    GtkWidget *logscale;
} UnrotateControls;

static gboolean      module_register          (const gchar *name);
static gboolean      unrotate               (GwyContainer *data,
                                               GwyRunType run);
static gboolean      unrotate_dialog             (UnrotateArgs *args);
static void          unrotate_dialog_update      (UnrotateControls *controls,
                                               UnrotateArgs *args);
static void          plane_coeffs             (gdouble *datapos,
                                               gint rowstride,
                                               gint kernel_size,
                                               gdouble *bx,
                                               gdouble *by);
static void          compute_angle_dist       (GwyDataField *dfield,
                                               gint nder,
                                               gdouble *der);
static GwyDataField* unrotate_do              (GwyDataField *dfield,
                                               UnrotateArgs *args);
static void          load_args                (GwyContainer *container,
                                               UnrotateArgs *args);
static void          save_args                (GwyContainer *container,
                                               UnrotateArgs *args);
static GwyDataField* make_datafield           (GwyDataField *old,
                                               gint res,
                                               gulong *count,
                                               gdouble real,
                                               gboolean logscale);

UnrotateArgs unrotate_defaults = {
    GWY_INTERPOLATION_BILINEAR,
    200,
    FALSE,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "unrotate",
    "Rotates data to make main directions parallel with x and y axes.",
    "Yeti <yeti@gwyddion.net>",
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
    static GwyProcessFuncInfo unrotate_func_info = {
        "unrotate",
        "/_Correct data/_Unrotate...",
        (GwyProcessFunc)&unrotate,
        UNROTATE_RUN_MODES,
    };

    gwy_process_func_register(name, &unrotate_func_info);

    return TRUE;
}

static void
plane_coeffs(gdouble *datapos, gint rowstride, gint kernel_size,
             gdouble *bx, gdouble *by)
{
    gdouble sumxi, sumxixi, sumyi, sumyiyi;
    gdouble sumsi = 0.0;
    gdouble sumsixi = 0.0;
    gdouble sumsiyi = 0.0;
    gint i, j;

    sumxi = sumyi = (kernel_size-1.0)/2;
    sumxixi = sumyiyi = (2*kernel_size-1.0)*(kernel_size-1.0)/6;

    for (i = 0; i < kernel_size; i++) {
        gdouble *row = datapos + i*rowstride;

        for (j = 0; j < kernel_size; j++) {
            sumsi += row[j];
            sumsixi += row[j]*j;
            sumsiyi += row[j]*i;
        }
    }
    sumsi /= kernel_size*kernel_size;
    sumsixi /= kernel_size*kernel_size;
    sumsiyi /= kernel_size*kernel_size;

    *bx = (sumsixi - sumsi*sumxi) / (sumxixi - sumxi*sumxi);
    *by = (sumsiyi - sumsi*sumyi) / (sumyiyi - sumyi*sumyi);
}

static void
compute_angle_dist(GwyDataField *dfield,
                   gint nder,
                   gdouble *der)
{
    enum { kernel_size = 5 };

    gdouble *data;
    gdouble bx, by, qx, qy, phi;
    gint xres, yres;
    gint col, row, iphi;

    data = gwy_data_field_get_data(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    qx = xres/gwy_data_field_get_xreal(dfield)/(xres*yres);
    qy = yres/gwy_data_field_get_yreal(dfield)/(xres*yres);
    memset(der, 0, nder*sizeof(gdouble));
    for (row = 0; row + kernel_size < yres; row++) {
        for (col = 0; col + kernel_size < xres; col++) {
            plane_coeffs(data + row*xres + col, xres, kernel_size, &bx, &by);
            bx *= qx;
            by *= qy;
            phi = atan2(by, bx);
            iphi = (gint)floor(nder*(phi + G_PI)/(2.0*G_PI));
            iphi = CLAMP(iphi, 0, nder-1);
            der[iphi] += sqrt(bx*bx + by*by);
        }
    }
}

/* XXX: the returned value is in 0..1 range!  NOT degrees or radians! */
static gdouble
find_correction(gint nder,
                gdouble *der)
{
    static const guint symm[] = { 2, 3, 4, 6 };
    gint i;
    gsize j, m;
    gdouble x, avg, max, total, phi;
    gdouble sint[G_N_ELEMENTS(symm)], cost[G_N_ELEMENTS(symm)];
    gdouble sum, wsum;

    avg = 0.0;
    for (i = 0; i < nder; i++)
        avg += der[i];
    avg /= nder;

#ifdef DEBUG
    {
        FILE *fh;

        fh = fopen("unrotate.out", "w");
        for (i = 0; i < nder; i++)
            fprintf(fh, "%f %f\n", 2*G_PI*(i + 0.5)/nder, der[i]);
        fclose(fh);
    }
#endif

    m = 0;
    max = -G_MAXDOUBLE;
    for (j = 0; j < G_N_ELEMENTS(symm); j++) {
        sint[j] = cost[j] = 0.0;
        for (i = 0; i < nder; i++) {
            x = 2*G_PI*(i + 0.5)/nder;

            sint[j] += sin(symm[j]*x)*(der[i] - avg);
            cost[j] += cos(symm[j]*x)*(der[i] - avg);
        }

        phi = atan2(-sint[j], cost[j]);
        total = sqrt(sint[j]*sint[j] + cost[j]*cost[j]);
        if (symm[j] == 2 || symm[j] == 4)
            total /= 2;

        gwy_debug("sc%d = (%f, %f), total%d = (%f, %f)",
                  symm[j], sint[j], cost[j], symm[j], total, 180.0/G_PI*phi);
        sint[j] = phi;
        cost[j] = total;
        if (total > max) {
            max = total;
            m = j;
        }
    }

    /* switch to 0..1 range for phi
     * the computation is less cluttered with G_PI's then */
    phi = sint[m]/(2*G_PI)/symm[m];
    gwy_debug("phi1 = %f", phi);
    phi -= floor(phi) + 1.0;
    m = symm[m];
    gwy_debug("selected %u, phi = %f", m, phi);
    sum = wsum = 0.0;
    for (j = 0; j < m; j++) {
        gdouble low = (j + 5.0/6.0)/m - phi;
        gdouble high = (j + 7.0/6.0)/m - phi;
        gdouble s, w;
        gint ilow, ihigh;

        ilow = (gint)floor(low*nder);
        ihigh = (gint)floor(high*nder);
        gwy_debug("peak %u low = %f, high = %f, %d, %d",
                  j, low, high, ilow, ihigh);
        s = w = 0.0;
        for (i = ilow; i <= ihigh; i++) {
            s += (i + 0.5)*der[i % nder];
            w += der[i % nder];
        }

        s /= nder*w;
        gwy_debug("peak %u center: %f", j, 360*s);
        sum += (s - (gdouble)j/m)*w*w;
        wsum += w*w;
    }
    phi = sum/wsum;
    gwy_debug("FITTED phi = %f (%f)", phi, 360*phi);
    phi = fmod(phi + 1.0, 1.0/m);
    if (phi > 0.5/m)
        phi -= 1.0/m;
    gwy_debug("MINIMIZED phi = %f (%f)", phi, 360*phi);

    return phi;
}

static gboolean
unrotate(GwyContainer *data, GwyRunType run)
{
    enum { nder = 4800 };
    GtkWidget *data_window;
    GwyDataField *dfield;
    UnrotateArgs args;
    gdouble *der;
    gdouble phi;
    gboolean ok;

    g_return_val_if_fail(run & UNROTATE_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = unrotate_defaults;
    else
        load_args(gwy_app_settings_get(), &args);
    args = unrotate_defaults;  /* XXX */

    data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    gwy_app_clean_up_data(data);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    der = g_new(gdouble, nder);
    compute_angle_dist(dfield, nder, der);
    phi = 360*find_correction(nder, der);
    g_free(der);

    gwy_data_field_rotate(dfield, phi, args.interp);
    if (gwy_container_gis_object_by_name(data, "/0/mask", (GObject**)&dfield))
        gwy_data_field_rotate(dfield, phi, args.interp);
    if (gwy_container_gis_object_by_name(data, "/0/show", (GObject**)&dfield))
        gwy_data_field_rotate(dfield, phi, args.interp);
    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    /*
    ok = (run != GWY_RUN_MODAL) || unrotate_dialog(&args);
    if (ok) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        pal = gwy_container_get_string_by_name(data, "/0/base/palette");
        dfield = unrotate_do(dfield, &args);
        data = GWY_CONTAINER(gwy_container_new());
        gwy_container_set_object_by_name(data, "/0/data", G_OBJECT(dfield));
        g_object_unref(dfield);
        gwy_container_set_string_by_name(data, "/0/base/palette",
                                         g_strdup(pal));

        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                         _("Unrotate"));

        if (run != GWY_RUN_WITH_DEFAULTS)
            save_args(gwy_app_settings_get(), &args);
    }
    */

    return FALSE;
}

#if 0
static gboolean
unrotate_dialog(UnrotateArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    UnrotateControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Unrotate Distribution"), NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(2, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.size = gtk_adjustment_new(args->size, 10, 1000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 0, _("Output size:"), "samples",
                                       controls.size);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);

    controls.logscale
        = gtk_check_button_new_with_mnemonic(_("_Logarithmic value scale"));
    gtk_table_attach(GTK_TABLE(table), controls.logscale,
                     0, 3, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    unrotate_dialog_update(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = unrotate_defaults;
            unrotate_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->size = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.size));
    args->logscale
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls.logscale));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
unrotate_dialog_update(UnrotateControls *controls,
                    UnrotateArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size),
                             args->size);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->logscale),
                                 args->logscale);
}

static GwyDataField*
unrotate_do(GwyDataField *dfield,
         UnrotateArgs *args)
{
    gdouble *xder, *yder;
    gdouble max;
    gint xres, yres;
    gint xider, yider, i;
    gulong *count;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    xder = g_new(gdouble, xres*yres);
    yder = g_new(gdouble, xres*yres);
    max = compute_unrotates(dfield, xder, yder);
    count = g_new0(gulong, args->size*args->size);
    for (i = 0; i < (xres - 2)*(yres - 2); i++) {
        xider = args->size*(xder[i]/(2.0*max) + 0.5);
        xider = CLAMP(xider, 0, args->size-1);
        yider = args->size*(yder[i]/(2.0*max) + 0.5);
        yider = CLAMP(yider, 0, args->size-1);

        count[yider*args->size + xider]++;
    }
    g_free(yder);
    g_free(xder);

    return make_datafield(dfield, args->size, count, 2.0*max, args->logscale);
}

static gdouble
compute_unrotates(GwyDataField *dfield,
               gdouble *xder,
               gdouble *yder)
{
    gdouble *data;
    gdouble qx, qy;
    gdouble d, max;
    gint xres, yres;
    gint col, row;

    data = gwy_data_field_get_data(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    qx = xres/gwy_data_field_get_xreal(dfield);
    qy = yres/gwy_data_field_get_yreal(dfield);
    max = 0.0;
    for (row = 1; row + 1 < yres; row++) {
        for (col = 1; col + 1 < xres; col++) {
            d = data[row*xres + col + 1] - data[row*xres + col - 1];
            d *= qx;
            *(xder++) = d;
            d = fabs(d);
            max = MAX(d, max);

            d = data[row*xres + xres + col] - data[row*xres - xres + col];
            d *= qy;
            *(yder++) = d;
            d = fabs(d);
            max = MAX(d, max);
        }
    }

    return max;
}

static GwyDataField*
make_datafield(GwyDataField *old,
               gint res, gulong *count,
               gdouble real, gboolean logscale)
{
    GwyDataField *dfield;
    GwySIUnit *unit;
    gchar *xyu, *zu, *u;
    gdouble *d;
    gint i;

    dfield = GWY_DATA_FIELD(gwy_data_field_new(res, res, real, real, FALSE));

    unit = GWY_SI_UNIT(gwy_si_unit_new(""));
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    zu = gwy_si_unit_get_unit_string(gwy_data_field_get_si_unit_z(old));
    xyu = gwy_si_unit_get_unit_string(gwy_data_field_get_si_unit_xy(old));
    if (!strcmp(zu, xyu))
        unit = GWY_SI_UNIT(gwy_si_unit_new(""));
    else {
        u = g_strconcat(zu, "/", xyu, NULL);
        unit = GWY_SI_UNIT(gwy_si_unit_new(u));
        g_free(u);
    }

    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_free(xyu);
    g_free(zu);
    g_object_unref(unit);

    d = gwy_data_field_get_data(dfield);
    if (logscale) {
        for (i = 0; i < res*res; i++)
            d[i] = count[i] ? log((gdouble)count[i]) + 1.0 : 0.0;
    }
    else {
        for (i = 0; i < res*res; i++)
            d[i] = count[i];
    }
    g_free(count);

    return dfield;
}
#endif

static const gchar *size_key = "/module/unrotate/size";
static const gchar *logscale_key = "/module/unrotate/logscale";

static void
load_args(GwyContainer *container,
          UnrotateArgs *args)
{
    *args = unrotate_defaults;

    /*
    gwy_container_gis_int32_by_name(container, size_key, &args->size);
    gwy_container_gis_boolean_by_name(container, logscale_key, &args->logscale);
    */
}

static void
save_args(GwyContainer *container,
          UnrotateArgs *args)
{
    /*
    gwy_container_set_int32_by_name(container, size_key, args->size);
    gwy_container_set_boolean_by_name(container, logscale_key, args->logscale);
    */
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
