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

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/gwyapp.h>

#define CONTRAST_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

enum {
    MAX_DEPTH = 6,
    MAX_SIZE = 16
};

typedef struct {
    gint size;
    gint depth;
    gdouble weight;
} ContrastArgs;

typedef struct {
    GtkObject *size;
    GtkObject *depth;
    GtkObject *weight;
} ContrastControls;

static gboolean module_register              (const gchar *name);
static gboolean maximize_local_contrast      (GwyContainer *data,
                                              GwyRunType run);
static gboolean contrast_dialog              (ContrastArgs *args);
static gboolean contrast_do                  (GwyContainer *data,
                                              ContrastArgs *args);
static void     contrast_dialog_update       (ContrastControls *controls,
                                              ContrastArgs *args);
static void     contrast_update_values       (ContrastControls *controls,
                                              ContrastArgs *args);
static void     load_args                    (GwyContainer *container,
                                              ContrastArgs *args);
static void     save_args                    (GwyContainer *container,
                                              ContrastArgs *args);
static void     sanitize_args                (ContrastArgs *args);

ContrastArgs contrast_defaults = {
    7,
    4,
    0.7,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Maximizes local contrast."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo max_local_contrast_func_info = {
        "maximize_local_contrast",
        N_("/_Display/_Local Contrast..."),
        (GwyProcessFunc)&maximize_local_contrast,
        CONTRAST_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &max_local_contrast_func_info);

    return TRUE;
}

static gboolean
maximize_local_contrast(GwyContainer *data, GwyRunType run)
{
    ContrastArgs args;
    gboolean ok;

    g_return_val_if_fail(run & CONTRAST_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = contrast_defaults;
    else
        load_args(gwy_app_settings_get(), &args);

    ok = (run != GWY_RUN_MODAL) || contrast_dialog(&args);
    if (run == GWY_RUN_MODAL)
        save_args(gwy_app_settings_get(), &args);
    if (ok)
        ok = contrast_do(data, &args);

    return ok;
}

static gboolean
contrast_dialog(ContrastArgs *args)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *spin;
    ContrastControls controls;
    gint response;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Increase Local Contrast"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(3, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    controls.size = gtk_adjustment_new(args->size, 1, MAX_SIZE, 1, 5, 0);
    gwy_table_attach_hscale(table, row++, _("Kernel _size:"), "px",
                            controls.size, 0);

    controls.depth = gtk_adjustment_new(args->depth, 2, MAX_DEPTH, 1, 2, 0);
    gwy_table_attach_hscale(table, row++, _("Blending _depth:"), NULL,
                            controls.depth, 0);

    controls.weight = gtk_adjustment_new(args->weight, 0.0, 1.0, 0.01, 0.1, 0);
    spin = gwy_table_attach_hscale(table, row++, _("_Weight:"), NULL,
                                   controls.weight, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            contrast_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = contrast_defaults;
            contrast_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    contrast_update_values(&controls, args);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
contrast_dialog_update(ContrastControls *controls,
                       ContrastArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size), args->size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->depth), args->depth);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->weight), args->weight);
}

static void
contrast_update_values(ContrastControls *controls,
                       ContrastArgs *args)
{
    args->size = gwy_adjustment_get_int(controls->size);
    args->depth = gwy_adjustment_get_int(controls->depth);
    args->weight = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->weight));
}

static gboolean
contrast_do(GwyContainer *data, ContrastArgs *args)
{
    GwyDataField *dfield, *minfield, *maxfield, *showfield;
    const gdouble *dat, *min, *max;
    gdouble *show, *weight;
    gdouble mins, maxs, v, vc, minv, maxv;
    gdouble sum, gmin, gmax;
    gint xres, yres, i, j, k, l;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gmin = gwy_data_field_get_min(dfield);
    gmax = gwy_data_field_get_max(dfield);
    if (gmax == gmin)
        return FALSE;

    gwy_app_undo_checkpoint(data, "/0/show", NULL);
    if (gwy_container_gis_object_by_name(data, "/0/show",
                                         (GObject**)&showfield)) {
        gwy_data_field_resample(showfield, xres, yres, GWY_INTERPOLATION_NONE);
        gwy_data_field_area_copy(dfield, showfield, 0, 0, xres, yres, 0, 0);
    }
    else {
        showfield = GWY_DATA_FIELD(gwy_data_field_new_alike(dfield, FALSE));
        gwy_container_set_object_by_name(data, "/0/show", G_OBJECT(showfield));
        g_object_unref(showfield);
    }

    minfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_minimum(minfield, args->size);

    maxfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_maximum(maxfield, args->size);

    dat = gwy_data_field_get_data_const(dfield);
    min = gwy_data_field_get_data_const(minfield);
    max = gwy_data_field_get_data_const(maxfield);
    show = gwy_data_field_get_data(showfield);

    weight = g_new(gdouble, args->depth);
    sum = 0.0;
    for (i = 0; i < args->depth; i++) {
        weight[i] = exp(-log(args->depth - 1.0)*i/(args->depth - 1.0));
        sum += weight[i];
    }

    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            minv = maxv = dat[i*xres + j];
            mins = minv*weight[0];
            maxs = maxv*weight[0];
            for (k = 1; k < args->depth; k++) {
                for (l = 0; l < 2*k+1; l++) {
                    /* top line */
                    v = max[MAX(0, i - k*args->size)*xres
                            + CLAMP(j + (l - k)*args->size, 0, xres-1)];
                    if (v > maxv)
                        maxv = v;

                    v = min[MAX(0, i - k*args->size)*xres
                            + CLAMP(j + (l - k)*args->size, 0, xres-1)];
                    if (v < minv)
                        minv = v;

                    /* bottom line */
                    v = max[MIN(yres-1, i + k*args->size)*xres
                            + CLAMP(j + (l - k)*args->size, 0, xres-1)];
                    if (v > maxv)
                        maxv = v;

                    v = min[MIN(yres-1, i + k*args->size)*xres
                            + CLAMP(j + (l - k)*args->size, 0, xres-1)];
                    if (v < minv)
                        minv = v;

                    /* left line */
                    v = max[CLAMP(i + (l - k)*args->size, 0, yres-1)*xres
                            + MAX(0, j - k*args->size)];
                    if (v > maxv)
                        maxv = v;

                    v = min[CLAMP(i + (l - k)*args->size, 0, yres-1)*xres
                            + MAX(0, j - k*args->size)];
                    if (v < minv)
                        minv = v;

                    /* right line */
                    v = max[CLAMP(i + (l - k)*args->size, 0, yres-1)*xres
                            + MIN(xres-1, j + k*args->size)];
                    if (v > maxv)
                        maxv = v;

                    v = min[CLAMP(i + (l - k)*args->size, 0, yres-1)*xres
                            + MIN(xres-1, j + k*args->size)];
                    if (v < minv)
                        minv = v;
                }
                mins += minv*weight[k];
                maxs += maxv*weight[k];
            }
            mins /= sum;
            maxs /= sum;
            v = dat[i*xres + j];
            if (G_LIKELY(mins < maxs)) {
                vc = (gmax - gmin)/(maxs - mins)*(v - mins) + gmin;
                v = args->weight*vc + (1.0 - args->weight)*v;
                v = CLAMP(v, gmin, gmax);
            }
            show[i*xres +j] = v;
        }
    }

    g_free(weight);
    g_object_unref(minfield);
    g_object_unref(maxfield);

    return TRUE;
}

static const gchar *size_key = "/module/local_contrast/size";
static const gchar *depth_key = "/module/local_contrast/depth";
static const gchar *weight_key = "/module/local_contrast/weight";

static void
sanitize_args(ContrastArgs *args)
{
    args->size = CLAMP(args->size, 1, MAX_SIZE);
    args->depth = CLAMP(args->depth, 2, MAX_DEPTH);
    args->weight = CLAMP(args->weight, 0.0, 1.0);
}

static void
load_args(GwyContainer *container,
          ContrastArgs *args)
{
    *args = contrast_defaults;

    gwy_container_gis_int32_by_name(container, size_key, &args->size);
    gwy_container_gis_int32_by_name(container, depth_key, &args->depth);
    gwy_container_gis_double_by_name(container, weight_key, &args->weight);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          ContrastArgs *args)
{
    gwy_container_set_int32_by_name(container, size_key, args->size);
    gwy_container_set_int32_by_name(container, depth_key, args->depth);
    gwy_container_set_double_by_name(container, weight_key, args->weight);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
