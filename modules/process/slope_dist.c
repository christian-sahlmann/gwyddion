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

#define SLOPE_DIST_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function. */
typedef struct {
    gint size;
    gboolean logscale;
} SlopeArgs;

typedef struct {
    GtkObject *size;
    GtkWidget *logscale;
} SlopeControls;

static gboolean      module_register          (const gchar *name);
static gboolean      slope_dist               (GwyContainer *data,
                                               GwyRunType run);
static gboolean      slope_dialog             (SlopeArgs *args);
static void          slope_dialog_update      (SlopeControls *controls,
                                               SlopeArgs *args);
static GwyDataField* slope_do                 (GwyDataField *dfield,
                                               SlopeArgs *args);
static void          load_args                (GwyContainer *container,
                                               SlopeArgs *args);
static void          save_args                (GwyContainer *container,
                                               SlopeArgs *args);
static gdouble       compute_slopes           (GwyDataField *dfield,
                                               gdouble *xder,
                                               gdouble *yder);
static GwyDataField* make_datafield           (GwyDataField *old,
                                               gint res,
                                               gulong *count,
                                               gdouble real,
                                               gboolean logscale);

SlopeArgs slope_defaults = {
    200,
    FALSE,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "slope_dist",
    "Slope distribution.",
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
    static GwyProcessFuncInfo slope_dist_func_info = {
        "slope_dist",
        "/_Statistics/_Slope distribution...",
        (GwyProcessFunc)&slope_dist,
        SLOPE_DIST_RUN_MODES,
    };

    gwy_process_func_register(name, &slope_dist_func_info);

    return TRUE;
}

static gboolean
slope_dist(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    SlopeArgs args;
    const gchar *pal;
    gboolean ok;

    g_return_val_if_fail(run & SLOPE_DIST_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = slope_defaults;
    else
        load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || slope_dialog(&args);
    if (ok) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        pal = gwy_container_get_string_by_name(data, "/0/base/palette");
        dfield = slope_do(dfield, &args);
        data = GWY_CONTAINER(gwy_container_new());
        gwy_container_set_object_by_name(data, "/0/data", G_OBJECT(dfield));
        g_object_unref(dfield);
        gwy_container_set_string_by_name(data, "/0/base/palette",
                                         g_strdup(pal));

        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                         _("Slope"));

        if (run != GWY_RUN_WITH_DEFAULTS)
            save_args(gwy_app_settings_get(), &args);
    }

    return FALSE;
}

static gboolean
slope_dialog(SlopeArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    SlopeControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Slope Distribution"), NULL,
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

    slope_dialog_update(&controls, args);

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
            *args = slope_defaults;
            slope_dialog_update(&controls, args);
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
slope_dialog_update(SlopeControls *controls,
                    SlopeArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size),
                             args->size);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->logscale),
                                 args->logscale);
}

static GwyDataField*
slope_do(GwyDataField *dfield,
         SlopeArgs *args)
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
    max = compute_slopes(dfield, xder, yder);
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
compute_slopes(GwyDataField *dfield,
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

static const gchar *size_key = "/module/slope_dist/size";
static const gchar *logscale_key = "/module/slope_dist/logscale";

static void
load_args(GwyContainer *container,
          SlopeArgs *args)
{
    *args = slope_defaults;

    gwy_container_gis_int32_by_name(container, size_key, &args->size);
    gwy_container_gis_boolean_by_name(container, logscale_key, &args->logscale);
}

static void
save_args(GwyContainer *container,
          SlopeArgs *args)
{
    gwy_container_set_int32_by_name(container, size_key, args->size);
    gwy_container_set_boolean_by_name(container, logscale_key, args->logscale);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
