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
#include <libprocess/stats.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/undo.h>

#define SPHREV_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

typedef enum {
    SPHREV_HORIZONTAL = 1,
    SPHREV_VERTICAL,
    SPHREV_BOTH
} Sphrev1DDirection;

/* Data for this function. */
typedef struct {
    Sphrev1DDirection direction;
    gdouble size;
    gboolean do_extract;
    /* interface only */
    GwySIValueFormat valform;
    gdouble pixelsize;
} Sphrev1DArgs;

typedef struct {
    GSList *direction;
    GtkObject *radius;
    GtkObject *size;
    GtkWidget *do_extract;
    gboolean in_update;
} Sphrev1DControls;

static gboolean      module_register           (const gchar *name);
static gboolean      sphrev                    (GwyContainer *data,
                                                GwyRunType run);
static GwyDataField* sphrev_horizontal         (Sphrev1DArgs *args,
                                                GwyDataField *dfield);
static GwyDataField* sphrev_vertical           (Sphrev1DArgs *args,
                                                GwyDataField *dfield);
static GwyDataLine*  sphrev_make_sphere        (gdouble radius,
                                                gint maxres);
static gboolean      sphrev_dialog             (Sphrev1DArgs *args);
static void          direction_changed_cb      (GObject *item,
                                                Sphrev1DArgs *args);
static void          radius_changed_cb         (GtkAdjustment *adj,
                                                Sphrev1DArgs *args);
static void          size_changed_cb           (GtkAdjustment *adj,
                                                Sphrev1DArgs *args);
static void          do_extract_changed_cb     (GtkWidget *check,
                                                Sphrev1DArgs *args);
static void          sphrev_dialog_update      (Sphrev1DControls *controls,
                                                Sphrev1DArgs *args);
static void          sphrev_sanitize_args      (Sphrev1DArgs *args);
static void          sphrev_load_args          (GwyContainer *container,
                                                Sphrev1DArgs *args);
static void          sphrev_save_args          (GwyContainer *container,
                                                Sphrev1DArgs *args);

Sphrev1DArgs sphrev_defaults = {
    SPHREV_HORIZONTAL,
    20,
    FALSE,
    { 1.0, 0, NULL },
    0,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Subtracts background by arc or sphere revolution."),
    "Yeti <yeti@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo sphrev_func_info = {
        "arc_revolve",
        N_("/_Level/Revolve _Arc..."),
        (GwyProcessFunc)&sphrev,
        SPHREV_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &sphrev_func_info);

    return TRUE;
}

static gboolean
sphrev(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield, *background = NULL;
    Sphrev1DArgs args;
    gdouble xr, yr;
    gboolean ok;

    g_return_val_if_fail(run & SPHREV_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = sphrev_defaults;
    else
        sphrev_load_args(gwy_app_settings_get(), &args);

    /* FIXME: this is bogus for non-square pixels anyway */
    xr = gwy_data_field_get_xreal(dfield)/gwy_data_field_get_xres(dfield);
    yr = gwy_data_field_get_yreal(dfield)/gwy_data_field_get_yres(dfield);
    args.pixelsize = hypot(xr, yr);
    gwy_data_field_get_value_format_xy(dfield, &args.valform);
    gwy_debug("pixelsize = %g, vf = (%g, %d, %s)",
              args.pixelsize, args.valform.magnitude, args.valform.precision,
              args.valform.units);

    ok = (run != GWY_RUN_MODAL) || sphrev_dialog(&args);
    if (run == GWY_RUN_MODAL)
        sphrev_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    switch (args.direction) {
        case SPHREV_HORIZONTAL:
        background = sphrev_horizontal(&args, dfield);
        break;

        case SPHREV_VERTICAL:
        background = sphrev_vertical(&args, dfield);
        break;

        case SPHREV_BOTH: {
            GwyDataField *tmp;

            background = sphrev_horizontal(&args, dfield);
            tmp = sphrev_vertical(&args, dfield);
            gwy_data_field_sum_fields(background, background, tmp);
            g_object_unref(tmp);
            gwy_data_field_multiply(background, 0.5);
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }
    gwy_data_field_subtract_fields(dfield, dfield, background);

    if (!args.do_extract) {
        g_object_unref(background);
        return TRUE;
    }

    data = gwy_container_duplicate_by_prefix(data,
                                             "/0/base/palette",
                                             "/0/select",
                                             NULL);
    gwy_container_set_object_by_name(data, "/0/data", background);
    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                     "Background");

    return TRUE;
}

static gboolean
sphrev_dialog(Sphrev1DArgs *args)
{
    const GwyEnum directions[] = {
        { N_("_Horizontal direction"), SPHREV_HORIZONTAL, },
        { N_("_Vertical direction"),   SPHREV_VERTICAL,   },
        { N_("_Both directions"),      SPHREV_BOTH,   },
    };
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *spin;
    Sphrev1DControls controls;
    gint response, row;
    GSList *radio;
    gdouble q;

    dialog = gtk_dialog_new_with_buttons(_("Revolve Arc"), NULL, 0,
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
                                   args->valform.units, controls.radius,
                                   GWY_HSCALE_SQRT);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), args->valform.precision);
    g_object_set_data(G_OBJECT(controls.radius), "controls", &controls);
    g_signal_connect(controls.radius, "value_changed",
                     G_CALLBACK(radius_changed_cb), args);
    row++;

    controls.size = gtk_adjustment_new(args->size, 1, 16384, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Pixel radius:"), "px",
                            controls.size, GWY_HSCALE_SQRT);
    g_object_set_data(G_OBJECT(controls.size), "controls", &controls);
    g_signal_connect(controls.size, "value_changed",
                     G_CALLBACK(size_changed_cb), args);
    row++;

    radio = gwy_radio_buttons_create(directions, G_N_ELEMENTS(directions),
                                     "direction-type",
                                     G_CALLBACK(direction_changed_cb), args,
                                     args->direction);
    controls.direction = radio;
    while (radio) {
        gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(radio->data),
                         0, 4, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
        row++;
        radio = g_slist_next(radio);
    }

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
            args->size = sphrev_defaults.size;
            args->direction = sphrev_defaults.direction;
            args->do_extract = sphrev_defaults.do_extract;
            sphrev_dialog_update(&controls, args);
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
direction_changed_cb(GObject *item,
                     Sphrev1DArgs *args)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)))
        return;

    args->direction
        = GPOINTER_TO_INT(g_object_get_data(item, "direction-type"));
}

static void
radius_changed_cb(GtkAdjustment *adj,
                  Sphrev1DArgs *args)
{
    Sphrev1DControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->size = gtk_adjustment_get_value(adj)
                 * args->valform.magnitude/args->pixelsize;
    sphrev_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
size_changed_cb(GtkAdjustment *adj,
                Sphrev1DArgs *args)
{
    Sphrev1DControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->size = gtk_adjustment_get_value(adj);
    sphrev_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
do_extract_changed_cb(GtkWidget *check,
                      Sphrev1DArgs *args)
{
    args->do_extract = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check));
}

static void
sphrev_dialog_update(Sphrev1DControls *controls,
                     Sphrev1DArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->radius),
                             args->size
                             * args->pixelsize/args->valform.magnitude);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size), args->size);
    gwy_radio_buttons_set_current(controls->direction, "direction-type",
                                  args->direction);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->do_extract),
                                 args->do_extract);
}

/* An efficient summing algorithm.  Although I'm author of this code, don't
 * ask me how it works... */
static void
moving_sums(gint res, gdouble *row, gdouble *buffer, gint size)
{
    gdouble *sum, *sum2;
    gint i, ls2, rs2;

    memset(buffer, 0, 2*res*sizeof(gdouble));
    sum = buffer;
    sum2 = buffer + res;

    ls2 = size/2;
    rs2 = (size - 1)/2;

    /* Shortcut: very large size */
    if (rs2 >= res) {
        for (i = 0; i < res; i++) {
            sum[i] += row[i];
            sum2[i] += row[i]*row[i];
        }
        for (i = 1; i < res; i++) {
            sum[i] = sum[0];
            sum2[i] = sum2[0];
        }
        return;
    }

    /* Phase 1: Fill first element */
    for (i = 0; i <= rs2; i++) {
       sum[0] += row[i];
       sum2[0] += row[i]*row[i];
    }

    /* Phase 2: Next elements only gather new data */
    for (i = 1; i <= MIN(ls2, res-1 - rs2); i++) {
        sum[i] = sum[i-1] + row[i + rs2];
        sum2[i] = sum2[i-1] + row[i + rs2]*row[i + rs2];
    }

    /* Phase 3a: Moving a sprat! */
    for (i = ls2+1; i <= res-1 - rs2; i++) {
        sum[i] = sum[i-1] + row[i + rs2] - row[i - ls2 - 1];
        sum2[i] = sum2[i-1] + row[i + rs2]*row[i + rs2]
                  - row[i - ls2 - 1]*row[i - ls2 - 1];
    }

    /* Phase 3b: Moving a whale! */
    for (i = res-1 - rs2; i <= ls2; i++) {
        sum[i] = sum[i-1];
        sum2[i] = sum2[i-1];
    }

    /* Phase 4: Next elements only lose data */
    for (i = MAX(ls2+1, res - rs2); i < res; i++) {
        sum[i] = sum[i-1] - row[i - ls2 - 1];
        sum2[i] = sum2[i-1] - row[i - ls2 - 1]*row[i - ls2 - 1];
    }
}

static GwyDataField*
sphrev_horizontal(Sphrev1DArgs *args,
                  GwyDataField *dfield)
{
    GwyDataField *rfield;
    GwyDataLine *sphere;
    gdouble *data, *rdata, *sphdata, *sum, *sum2, *weight, *tmp;
    gdouble q;
    gint i, j, k, size, xres, yres;

    data = gwy_data_field_get_data(dfield);
    rfield = gwy_data_field_duplicate(dfield);
    xres = gwy_data_field_get_xres(rfield);
    yres = gwy_data_field_get_yres(rfield);
    rdata = gwy_data_field_get_data(rfield);

    q = gwy_data_field_get_rms(dfield)/sqrt(2.0/3.0 - G_PI/16.0);
    sphere = sphrev_make_sphere(args->size, gwy_data_field_get_xres(dfield));

    /* Scale-freeing.
     * Data is normalized to have the same RMS as if it was composed from
     * spheres of radius args->radius.  Actually we normalize the sphere
     * instead, but the effect is the same.  */
    gwy_data_line_multiply(sphere, -q);
    sphdata = gwy_data_line_get_data(sphere);
    size = gwy_data_line_get_res(sphere)/2;

    sum = g_new(gdouble, 4*xres);
    sum2 = sum + xres;
    weight = sum + 2*xres;
    tmp = sum + 3*xres;

    /* Weights for RMS filter.  The fool-proof way is to sum 1's. */
    for (j = 0; j < xres; j++)
        weight[j] = 1.0;
    moving_sums(xres, weight, sum, size);
    memcpy(weight, sum, xres*sizeof(gdouble));

    for (i = 0; i < yres; i++) {
        gdouble *rrow = rdata + i*xres;
        gdouble *drow = data + i*xres;

        /* Kill data that stick down too much */
        moving_sums(xres, data + i*xres, sum, size);
        for (j = 0; j < xres; j++) {
            /* transform to avg - 2.5*rms */
            sum[j] = sum[j]/weight[j];
            sum2[j] = 2.5*sqrt(sum2[j]/weight[j] - sum[j]*sum[j]);
            sum[j] -= sum2[j];
        }
        for (j = 0; j < xres; j++)
            tmp[j] = MAX(drow[j], sum[j]);

        /* Find the touching point */
        for (j = 0; j < xres; j++) {
            gdouble *row = tmp + j;
            gint from, to, km;
            gdouble min;

            from = MAX(0, j-size) - j;
            to = MIN(j+size, xres-1) - j;
            min = G_MAXDOUBLE;
            km = 0;
            for (k = from; k <= to; k++) {
                if (-(sphdata[size+k] - row[k]) < min) {
                    min = -(sphdata[size+k] - row[k]);
                    km = k;
                }
            }
            rrow[j] = min;
        }
    }

    g_free(sum);
    g_object_unref(sphere);

    return rfield;
}

static GwyDataField*
sphrev_vertical(Sphrev1DArgs *args,
                GwyDataField *dfield)
{
    GwyDataField *rfield;
    GwyDataLine *sphere;
    gdouble *data, *rdata, *sphdata, *sum, *sum2, *weight, *tmp;
    gdouble q;
    gint i, j, k, size, xres, yres;

    data = gwy_data_field_get_data(dfield);
    rfield = gwy_data_field_duplicate(dfield);
    xres = gwy_data_field_get_xres(rfield);
    yres = gwy_data_field_get_yres(rfield);
    rdata = gwy_data_field_get_data(rfield);

    q = gwy_data_field_get_rms(dfield)/sqrt(2.0/3.0 - G_PI/16.0);
    sphere = sphrev_make_sphere(args->size, gwy_data_field_get_yres(dfield));

    /* Scale-freeing.
     * Data is normalized to have the same RMS as if it was composed from
     * spheres of radius args->radius.  Actually we normalize the sphere
     * instead, but the effect is the same.  */
    gwy_data_line_multiply(sphere, -q);
    sphdata = gwy_data_line_get_data(sphere);
    size = gwy_data_line_get_res(sphere)/2;

    sum = g_new(gdouble, 4*yres);
    sum2 = sum + yres;
    weight = sum + 2*yres;
    tmp = sum + 3*yres;

    /* Weights for RMS filter.  The xresl-proof way is to sum 1's. */
    for (j = 0; j < yres; j++)
        weight[j] = 1.0;
    moving_sums(yres, weight, sum, size);
    memcpy(weight, sum, yres*sizeof(gdouble));

    for (i = 0; i < xres; i++) {
        gdouble *rcol = rdata + i;
        gdouble *dcol = data + i;

        /* Kill data that stick down too much */
        for (j = 0; j < yres; j++)
            tmp[j] = dcol[j*xres];
        moving_sums(yres, tmp, sum, size);
        for (j = 0; j < yres; j++) {
            /* transform to avg - 2.5*rms */
            sum[j] = sum[j]/weight[j];
            sum2[j] = 2.5*sqrt(sum2[j]/weight[j] - sum[j]*sum[j]);
            sum[j] -= sum2[j];
        }
        for (j = 0; j < yres; j++)
            tmp[j] = MAX(dcol[j*xres], sum[j]);

        /* Find the touching point */
        for (j = 0; j < yres; j++) {
            gdouble *col = tmp + j;
            gint from, to, km;
            gdouble min;

            from = MAX(0, j-size) - j;
            to = MIN(j+size, yres-1) - j;
            min = G_MAXDOUBLE;
            km = 0;
            for (k = from; k <= to; k++) {
                if (-(sphdata[size+k] - col[k]) < min) {
                    min = -(sphdata[size+k] - col[k]);
                    km = k;
                }
            }
            rcol[j*xres] = min;
        }
    }

    g_free(sum);
    g_object_unref(sphere);

    return rfield;
}

static GwyDataLine*
sphrev_make_sphere(gdouble radius, gint maxres)
{
    GwyDataLine *dline;
    gdouble *data;
    gint i, size;

    size = ROUND(MIN(radius, maxres));
    dline = GWY_DATA_LINE(gwy_data_line_new(2*size+1, 1.0, FALSE));
    data = gwy_data_line_get_data(dline);

    if (radius/8 > maxres) {
        /* Pathological case: very flat sphere */
        for (i = 0; i <= size; i++) {
            gdouble u = i/radius;

            data[size+i] = data[size-i] = u*u/2.0*(1.0 + u*u/4.0*(1 + u*u/2.0));
        }
    }
    else {
        /* Normal sphere */
        for (i = 0; i <= size; i++) {
            gdouble u = i/radius;

            if (G_UNLIKELY(u > 1.0))
                data[size+i] = data[size-i] = 1.0;
            else
                data[size+i] = data[size-i] = 1.0 - sqrt(1.0 - u*u);
        }
    }

    return dline;
}

static const gchar *radius_key = "/module/arc_revolve/radius";
static const gchar *direction_key = "/module/arc_revolve/direction";
static const gchar *do_extract_key = "/module/arc_revolve/do_extract";

static void
sphrev_sanitize_args(Sphrev1DArgs *args)
{
    args->size = CLAMP(args->size, 1, 16384);
    args->direction = CLAMP(args->direction, SPHREV_HORIZONTAL, SPHREV_BOTH);
    args->do_extract = !!args->do_extract;
}

static void
sphrev_load_args(GwyContainer *container,
                 Sphrev1DArgs *args)
{
    *args = sphrev_defaults;

    gwy_container_gis_double_by_name(container, radius_key, &args->size);
    gwy_container_gis_enum_by_name(container, direction_key, &args->direction);
    gwy_container_gis_boolean_by_name(container, do_extract_key,
                                      &args->do_extract);
    sphrev_sanitize_args(args);
}

static void
sphrev_save_args(GwyContainer *container,
                 Sphrev1DArgs *args)
{
    gwy_container_set_double_by_name(container, radius_key, args->size);
    gwy_container_set_enum_by_name(container, direction_key, args->direction);
    gwy_container_set_boolean_by_name(container, do_extract_key,
                                      args->do_extract);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
