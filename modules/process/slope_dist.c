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

#define LEVEL_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function. */
typedef struct {
    gint size;
} SlopeDistArgs;

typedef struct {
    GtkObject *size;
    gboolean in_update;
} SlopeDistControls;

static gboolean      module_register            (const gchar *name);
static gboolean      slope_dist                 (GwyContainer *data,
                                                 GwyRunType run);
static gboolean      slope_dist_dialog          (SlopeDistArgs *args);
static void          size_changed_cb            (GtkAdjustment *adj,
                                                 SlopeDistArgs *args);
static void          slope_dist_dialog_update   (SlopeDistControls *controls,
                                                 SlopeDistArgs *args);
static GwyDataField* slope_dist_do              (GwyDataField *dfield,
                                                 SlopeDistArgs *args);
static void          slope_dist_load_args       (GwyContainer *container,
                                                 SlopeDistArgs *args);
static void          slope_dist_save_args       (GwyContainer *container,
                                                 SlopeDistArgs *args);
static gdouble       compute_slopes             (GwyDataField *dfield,
                                                 gdouble *xder,
                                                 gdouble *yder);

SlopeDistArgs slope_dist_defaults = {
    200,
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
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo slope_dist_func_info = {
        "slope_dist",
        "/_Statistics/_Slope distribution",
        (GwyProcessFunc)&slope_dist,
        LEVEL_RUN_MODES,
    };

    gwy_process_func_register(name, &slope_dist_func_info);

    return TRUE;
}

static gboolean
slope_dist(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    SlopeDistArgs args;
    const gchar *pal;
    gboolean ok;

    g_return_val_if_fail(run & LEVEL_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = slope_dist_defaults;
    else
        slope_dist_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || slope_dist_dialog(&args);
    if (ok) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        pal = gwy_container_get_string_by_name(data, "/0/base/palette");
        dfield = slope_dist_do(dfield, &args);
        data = GWY_CONTAINER(gwy_container_new());
        gwy_container_set_object_by_name(data, "/0/data", G_OBJECT(dfield));
        gwy_container_set_string_by_name(data, "/0/base/palette",
                                         g_strdup(pal));

        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                         _("Slope. dist."));

        if (run != GWY_RUN_WITH_DEFAULTS)
            slope_dist_save_args(gwy_app_settings_get(), &args);
    }

    return FALSE;
}

static gboolean
slope_dist_dialog(SlopeDistArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    SlopeDistControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Slope Distribution"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(1, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.size = gtk_adjustment_new(args->size, 10, 1000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 0, _("Output size:"), "",
                                       controls.size);
    g_object_set_data(G_OBJECT(controls.size), "controls", &controls);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect(controls.size, "value_changed",
                     G_CALLBACK(size_changed_cb), args);

    controls.in_update = FALSE;

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
            args->size = slope_dist_defaults.size;
            slope_dist_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->size = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.size));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
size_changed_cb(GtkAdjustment *adj,
                SlopeDistArgs *args)
{
    SlopeDistControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->size = ROUND(gtk_adjustment_get_value(adj));
    slope_dist_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
slope_dist_dialog_update(SlopeDistControls *controls,
                         SlopeDistArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size),
                             args->size);
}

static GwyDataField*
slope_dist_do(GwyDataField *dfield,
              SlopeDistArgs *args)
{
    GwySIUnit *zunit;
    gdouble *xder, *yder, *d;
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
    for (i = 0; i < xres*yres; i++) {
        xider = args->size*(xder[i]/(2.0*max) + 0.5);
        xider = CLAMP(xider, 0, args->size-1);
        yider = args->size*(yder[i]/(2.0*max) + 0.5);
        yider = CLAMP(yider, 0, args->size-1);

        count[yider*args->size + xider]++;
    }
    g_free(yder);
    g_free(xder);

    dfield = GWY_DATA_FIELD(gwy_data_field_new(args->size, args->size,
                                               2.0*max, 2.0*max,
                                               FALSE));
    zunit = GWY_SI_UNIT(gwy_si_unit_new(""));
    gwy_data_field_set_si_unit_z(dfield, zunit);
    g_object_unref(zunit);

    d = gwy_data_field_get_data(dfield);
    for (i = 0; i < args->size*args->size; i++)
        d[i] = count[i];
    g_free(count);

    return dfield;
}

static gdouble
compute_slopes(GwyDataField *dfield,
               gdouble *xder,
               gdouble *yder)
{
    gdouble d, max;
    gint xres, yres;
    gint col, row;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    max = 0.0;
    for (row = 0; row < yres; row++) {
        for (col = 0; col < xres; col++) {
            d = gwy_data_field_get_xder(dfield, col, row);
            xder[row*xres + col] = d;
            d = fabs(d);
            max = MAX(d, max);

            d = gwy_data_field_get_yder(dfield, col, row);
            yder[row*xres + col] = d;
            d = fabs(d);
            max = MAX(d, max);
        }
    }

    return max;
}

static const gchar *size_key = "/module/slope_dist/size";

static void
slope_dist_load_args(GwyContainer *container,
                     SlopeDistArgs *args)
{
    *args = slope_dist_defaults;

    gwy_container_gis_int32_by_name(container, size_key, &args->size);
}

static void
slope_dist_save_args(GwyContainer *container,
                     SlopeDistArgs *args)
{
    gwy_container_set_int32_by_name(container, size_key, args->size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
