/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define CONTRAST_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    MAX_SIZE = 50,
    BLOCK_SIZE = 200,
};

typedef struct {
    guint size;
} ContrastArgs;

typedef struct {
    ContrastArgs *args;
    GtkObject *size;
} ContrastControls;

static gboolean module_register      (void);
static void     local_contrast_rank  (GwyContainer *data,
                                      GwyRunType run);
static gboolean contrast_dialog      (ContrastArgs *args);
static void     contrast_dialog_reset(ContrastControls *controls);
static void     size_changed         (ContrastControls *controls,
                                      GtkAdjustment *adj);
static void     contrast_do          (GwyContainer *data,
                                      ContrastArgs *args);
static void     filter_median_full   (GwyDataField *data_field,
                                      gint size,
                                      gint col,
                                      gint row,
                                      gint width,
                                      gint height);
static void     load_args            (GwyContainer *container,
                                      ContrastArgs *args);
static void     save_args            (GwyContainer *container,
                                      ContrastArgs *args);
static void     sanitize_args        (ContrastArgs *args);

static const ContrastArgs contrast_defaults = {
    15,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Maximizes local contrast using a simple rank transform."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("local_contrast_rank",
                              (GwyProcessFunc)&local_contrast_rank,
                              N_("/_Presentation/Local _Contrast (Rank)..."),
                              NULL,
                              CONTRAST_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Presentation with local contrast "
                                 "maximized using a rank transform"));

    return TRUE;
}

static void
local_contrast_rank(GwyContainer *data, GwyRunType run)
{
    ContrastArgs args;

    g_return_if_fail(run & CONTRAST_RUN_MODES);
    load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        gboolean ok = contrast_dialog(&args);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
    contrast_do(data, &args);
}

static gboolean
contrast_dialog(ContrastArgs *args)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table;
    ContrastControls controls;
    gint response;
    gint row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Increase Local Contrast"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(1, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    controls.size = gtk_adjustment_new(args->size, 1, MAX_SIZE, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Kernel _size:"), "px",
                            controls.size, 0);
    g_signal_connect_swapped(controls.size, "value-changed",
                             G_CALLBACK(size_changed), &controls);

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
            contrast_dialog_reset(&controls);
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
contrast_dialog_reset(ContrastControls *controls)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size),
                             contrast_defaults.size);
}

static void
size_changed(ContrastControls *controls, GtkAdjustment *adj)
{
    controls->args->size = gwy_adjustment_get_int(adj);
}

static void
contrast_do(GwyContainer *data, ContrastArgs *args)
{
    GwyDataField *dfield, *minfield, *maxfield, *medfield, *showfield;
    GQuark dquark, squark;
    const gdouble *dat, *min, *max, *med;
    gdouble *show;
    gdouble gmin, gmax;
    gint xres, yres, ni, nj, i, j, k, size, id;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     GWY_APP_SHOW_FIELD, &showfield,
                                     0);
    g_return_if_fail(dfield && dquark && squark);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gmin = gwy_data_field_get_min(dfield);
    gmax = gwy_data_field_get_max(dfield);
    if (gmax == gmin)
        return;

    size = 2*args->size + 1;

    gwy_app_wait_start(gwy_app_find_window_for_channel(data, id),
                       _("Initializing"));
    minfield = gwy_data_field_duplicate(dfield);
    maxfield = gwy_data_field_duplicate(dfield);
    medfield = gwy_data_field_duplicate(dfield);

    showfield = gwy_data_field_new_alike(dfield, FALSE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(showfield), "");

    gwy_app_wait_set_fraction(0.0);
    if (!gwy_app_wait_set_message("Minimum..."))
        goto cancelled;
    gwy_data_field_filter_minimum(minfield, size);

    gwy_app_wait_set_fraction(0.05);
    if (!gwy_app_wait_set_message("Maximum..."))
        goto cancelled;
    gwy_data_field_filter_maximum(maxfield, size);

    gwy_app_wait_set_fraction(0.1);
    if (!gwy_app_wait_set_message("Median..."))
        goto cancelled;
    ni = (yres + BLOCK_SIZE-1)/BLOCK_SIZE;
    nj = (xres + BLOCK_SIZE-1)/BLOCK_SIZE;
    k = 0;
    for (i = 0; i < ni; i++) {
        gint row = yres*i/ni, height = yres*(i+1)/ni - row;
        for (j = 0; j < nj; j++) {
            gint col = xres*j/nj, width = xres*(j+1)/nj - col;
            filter_median_full(medfield, size, col, row, width, height);
            k++;
            if (!gwy_app_wait_set_fraction(0.1 + 0.89*k/(ni*nj)))
                goto cancelled;
        }
    }

    gwy_app_wait_set_fraction(0.99);
    if (!gwy_app_wait_set_message("Local contrast..."))
        goto cancelled;
    dat = gwy_data_field_get_data_const(dfield);
    min = gwy_data_field_get_data_const(minfield);
    max = gwy_data_field_get_data_const(maxfield);
    med = gwy_data_field_get_data_const(medfield);
    show = gwy_data_field_get_data(showfield);

    for (k = 0; k < xres*yres; k++) {
        if (dat[k] > med[k])
            show[k] = (dat[k] - med[k])/(max[k] - med[k]);
        else if (dat[k] < med[k])
            show[k] = (dat[k] - med[k])/(med[k] - min[k]);
        else
            show[k] = 0.0;
    }
    gwy_data_field_normalize(showfield);

    if (!gwy_app_wait_set_fraction(1.0))
        goto cancelled;

    gwy_app_wait_finish();
    gwy_app_undo_qcheckpointv(data, 1, &squark);
    gwy_container_set_object(data, squark, showfield);
    gwy_data_field_data_changed(showfield);

cancelled:
    g_object_unref(showfield);
    g_object_unref(minfield);
    g_object_unref(maxfield);
    g_object_unref(medfield);
}

/* The same as gwy_data_field_area_filter_median() but it uses data from the
 * entire field (where available) not just the rectangle. */
static void
filter_median_full(GwyDataField *data_field,
                   gint size,
                   gint col, gint row,
                   gint width, gint height)
{

    gint xres, yres;
    gint i, j, k, len;
    gint xfrom, xto, yfrom, yto;
    gdouble *buffer, *data, *kernel;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(size > 0);
    xres = data_field->xres;
    yres = data_field->yres;
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= xres
                     && row + height <= yres);

    buffer = g_new(gdouble, width*height);
    kernel = g_new(gdouble, size*size);
    data = data_field->data;

    for (i = 0; i < height; i++) {
        yfrom = MAX(0, row + i - (size-1)/2);
        yto = MIN(yres-1, row + i + size/2);
        for (j = 0; j < width; j++) {
            xfrom = MAX(0, col + j - (size-1)/2);
            xto = MIN(xres-1, col + j + size/2);
            len = xto - xfrom + 1;
            for (k = yfrom; k <= yto; k++)
                memcpy(kernel + len*(k - yfrom),
                       data + k*xres + xfrom,
                       len*sizeof(gdouble));
            buffer[i*width + j] = gwy_math_median(len*(yto - yfrom + 1),
                                                  kernel);
        }
    }

    g_free(kernel);
    for (i = 0; i < height; i++)
        memcpy(data + (row + i)*xres + col,
               buffer + i*width,
               width*sizeof(gdouble));
    g_free(buffer);
}

static const gchar size_key[]   = "/module/local_contrast_rank/size";

static void
sanitize_args(ContrastArgs *args)
{
    args->size = CLAMP(args->size, 1, MAX_SIZE);
}

static void
load_args(GwyContainer *container,
          ContrastArgs *args)
{
    *args = contrast_defaults;

    gwy_container_gis_int32_by_name(container, size_key, &args->size);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          ContrastArgs *args)
{
    gwy_container_set_int32_by_name(container, size_key, args->size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
