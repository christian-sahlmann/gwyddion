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

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/undo.h>
#include <app/wait.h>

#define MEDIANBG_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function. */
typedef struct {
    gdouble size;
    gboolean do_extract;
    /* interface only */
    GwySIValueFormat valform;
    gdouble pixelsize;
} MedianBgArgs;

typedef struct {
    GtkObject *radius;
    GtkObject *size;
    GtkWidget *do_extract;
    gboolean in_update;
} MedianBgControls;

static gboolean      module_register           (const gchar *name);
static gboolean      median                    (GwyContainer *data,
                                                GwyRunType run);
static GwyDataField* median_background         (gint size,
                                                GwyDataField *dfield);
static gint*         median_make_circle        (gint radius);
static gboolean      median_dialog             (MedianBgArgs *args);
static void          radius_changed_cb         (GtkAdjustment *adj,
                                                MedianBgArgs *args);
static void          size_changed_cb           (GtkAdjustment *adj,
                                                MedianBgArgs *args);
static void          do_extract_changed_cb     (GtkWidget *check,
                                                MedianBgArgs *args);
static void          median_dialog_update      (MedianBgControls *controls,
                                                MedianBgArgs *args);
static void          median_sanitize_args      (MedianBgArgs *args);
static void          median_load_args          (GwyContainer *container,
                                                MedianBgArgs *args);
static void          median_save_args          (GwyContainer *container,
                                                MedianBgArgs *args);

MedianBgArgs median_defaults = {
    20,
    FALSE,
    { 1.0, 0, NULL },
    0,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "median_bg",
    N_("Rank-based background removal."),
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo median_func_info = {
        "median_bg",
        N_("/_Level/_Median Level..."),
        (GwyProcessFunc)&median,
        MEDIANBG_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &median_func_info);

    return TRUE;
}

static gboolean
median(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield, *background = NULL;
    MedianBgArgs args;
    gdouble xr, yr;
    gboolean ok;

    g_return_val_if_fail(run & MEDIANBG_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = median_defaults;
    else
        median_load_args(gwy_app_settings_get(), &args);

    /* FIXME: this is bogus for non-square pixels anyway */
    xr = gwy_data_field_get_xreal(dfield)/gwy_data_field_get_xres(dfield);
    yr = gwy_data_field_get_yreal(dfield)/gwy_data_field_get_yres(dfield);
    args.pixelsize = sqrt(xr*xr + yr*yr);
    gwy_data_field_get_value_format_xy(dfield, &args.valform);
    gwy_debug("pixelsize = %g, vf = (%g, %d, %s)",
              args.pixelsize, args.valform.magnitude, args.valform.precision,
              args.valform.units);

    ok = (run != GWY_RUN_MODAL) || median_dialog(&args);
    if (run == GWY_RUN_MODAL)
        median_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    gwy_app_wait_start(GTK_WIDGET(gwy_app_data_window_get_for_data(data)),
                       _("Median-leveling"));
    background = median_background(ROUND(args.size), dfield);
    gwy_app_wait_finish();
    if (!background)
        return FALSE;

    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    gwy_data_field_subtract_fields(dfield, dfield, background);

    if (!args.do_extract) {
        g_object_unref(background);
        return TRUE;
    }

    data = gwy_container_duplicate_by_prefix(data,
                                             "/0/base/palette",
                                             "/0/select",
                                             NULL);
    gwy_container_set_object_by_name(data, "/0/data", G_OBJECT(background));
    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                     "Background");

    return TRUE;
}

static gboolean
median_dialog(MedianBgArgs *args)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *spin;
    MedianBgControls controls;
    gint response, row;
    gdouble q;

    dialog = gtk_dialog_new_with_buttons(_("Median Level"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(5, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;
    controls.in_update = TRUE;

    q = args->pixelsize/args->valform.magnitude;
    gwy_debug("q = %f", q);
    controls.radius = gtk_adjustment_new(q*args->size, q, 16384*q, q, 10*q, 0);
    spin = gwy_table_attach_hscale(table, row, _("Real _radius:"),
                                   args->valform.units,
                                   controls.radius, GWY_HSCALE_SQRT);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), args->valform.precision);
    g_object_set_data(G_OBJECT(controls.radius), "controls", &controls);
    g_signal_connect(controls.radius, "value_changed",
                     G_CALLBACK(radius_changed_cb), args);
    row++;

    controls.size = gtk_adjustment_new(args->size, 1, 16384, 1, 10, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Pixel radius:"), "px",
                                   controls.size, GWY_HSCALE_SQRT);
    g_object_set_data(G_OBJECT(controls.size), "controls", &controls);
    g_signal_connect(controls.size, "value_changed",
                     G_CALLBACK(size_changed_cb), args);
    row++;

    controls.do_extract
        = gtk_check_button_new_with_mnemonic(_("E_xtract background"));
    gtk_table_attach(GTK_TABLE(table), controls.do_extract,
                     0, 4, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_extract),
                                 args->do_extract);
    g_signal_connect(controls.do_extract, "toggled",
                     G_CALLBACK(do_extract_changed_cb), args);
    row++;

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
            args->size = median_defaults.size;
            args->do_extract = median_defaults.do_extract;
            median_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
radius_changed_cb(GtkAdjustment *adj,
                  MedianBgArgs *args)
{
    MedianBgControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->size = gtk_adjustment_get_value(adj)
                 * args->valform.magnitude/args->pixelsize;
    median_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
size_changed_cb(GtkAdjustment *adj,
                MedianBgArgs *args)
{
    MedianBgControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->size = gtk_adjustment_get_value(adj);
    median_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
do_extract_changed_cb(GtkWidget *check,
                      MedianBgArgs *args)
{
    args->do_extract = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check));
}

static void
median_dialog_update(MedianBgControls *controls,
                     MedianBgArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->radius),
                             args->size
                             * args->pixelsize/args->valform.magnitude);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size), args->size);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->do_extract),
                                 args->do_extract);
}

static GwyDataField*
median_background(gint size,
                  GwyDataField *dfield)
{
    GwyDataField *rfield;
    gint *circle;
    gdouble *data, *rdata, *buffer;
    gint i, j, xres, yres, buflen;

    data = gwy_data_field_get_data(dfield);
    rfield = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
    xres = gwy_data_field_get_xres(rfield);
    yres = gwy_data_field_get_yres(rfield);
    rdata = gwy_data_field_get_data(rfield);

    buflen = 0;
    circle = median_make_circle(size);
    for (i = 0; i < 2*size + 1; i++)
        buflen += 2*circle[i] + 1;
    buffer = g_new(gdouble, buflen);

    for (i = 0; i < yres; i++) {

        for (j = 0; j < xres; j++) {
            gint n, k, from, to;

            n = 0;
            for (k = MAX(0, i - size); k <= MIN(yres - 1, i + size); k++) {
                gdouble *row = data + k*xres;

                from = MAX(0, j - circle[k - i + size]);
                to = MIN(xres - 1, j + circle[k - i + size]);
                memcpy(buffer + n, row + from, (to - from + 1)*sizeof(gdouble));
                n += to - from + 1;
            }
            rdata[i*xres + j] = gwy_math_median(n, buffer);
        }
        if (i % 10 == 0
            && !gwy_app_wait_set_fraction((gdouble)i/yres)) {
            g_free(circle);
            g_object_unref(rfield);

            return NULL;
        }
    }

    g_free(circle);

    return rfield;
}

static gint*
median_make_circle(gint radius)
{
    gint *data;
    gint i;

    data = g_new(gint, 2*radius + 1);

    for (i = 0; i <= radius; i++) {
        gdouble u = (gdouble)i/radius;

        if (G_UNLIKELY(u > 1.0))
            data[radius+i] = data[radius-i] = 0;
        else
            data[radius+i] = data[radius-i] = ROUND(radius*sqrt(1.0 - u*u));

    }

    return data;
}

static const gchar *radius_key = "/module/median_bg/radius";
static const gchar *do_extract_key = "/module/median_bg/do_extract";

static void
median_sanitize_args(MedianBgArgs *args)
{
    args->size = CLAMP(args->size, 1, 16384);
    args->do_extract = !!args->do_extract;
}

static void
median_load_args(GwyContainer *container,
                 MedianBgArgs *args)
{
    *args = median_defaults;

    gwy_container_gis_double_by_name(container, radius_key, &args->size);
    gwy_container_gis_boolean_by_name(container, do_extract_key,
                                      &args->do_extract);
    median_sanitize_args(args);
}

static void
median_save_args(GwyContainer *container,
                 MedianBgArgs *args)
{
    gwy_container_set_double_by_name(container, radius_key, args->size);
    gwy_container_set_boolean_by_name(container, do_extract_key,
                                      args->do_extract);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
