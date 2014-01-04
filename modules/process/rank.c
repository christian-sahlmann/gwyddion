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

#define RANK_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    MAX_SIZE = 50,
    BLOCK_SIZE = 200,
};

typedef struct {
    guint size;
} RankArgs;

typedef struct {
    RankArgs *args;
    GtkObject *size;
} RankControls;

static gboolean module_register  (void);
static void     rank             (GwyContainer *data,
                                  GwyRunType run);
static gboolean rank_dialog      (RankArgs *args);
static void     rank_dialog_reset(RankControls *controls);
static void     size_changed     (RankControls *controls,
                                  GtkAdjustment *adj);
static void     rank_do          (GwyContainer *data,
                                  RankArgs *args);
static gdouble  local_rank       (GwyDataField *data_field,
                                  gint size,
                                  gint col,
                                  gint row);
static void     load_args        (GwyContainer *container,
                                  RankArgs *args);
static void     save_args        (GwyContainer *container,
                                  RankArgs *args);
static void     sanitize_args    (RankArgs *args);

static const RankArgs rank_defaults = {
    15,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Enhances local contrast using a rank transform."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("rank",
                              (GwyProcessFunc)&rank,
                              N_("/_Presentation/_Rank..."),
                              NULL,
                              RANK_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Presentation with local contrast "
                                 "ehnanced using a rank transform"));

    return TRUE;
}

static void
rank(GwyContainer *data, GwyRunType run)
{
    RankArgs args;

    g_return_if_fail(run & RANK_RUN_MODES);
    load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        gboolean ok = rank_dialog(&args);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
    rank_do(data, &args);
}

static gboolean
rank_dialog(RankArgs *args)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table;
    RankControls controls;
    gint response;
    gint row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Rank Transform"),
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
            rank_dialog_reset(&controls);
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
rank_dialog_reset(RankControls *controls)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size),
                             rank_defaults.size);
}

static void
size_changed(RankControls *controls, GtkAdjustment *adj)
{
    controls->args->size = gwy_adjustment_get_int(adj);
}

static void
rank_do(GwyContainer *data, RankArgs *args)
{
    GwyDataField *dfield, *showfield;
    GQuark dquark, squark;
    gdouble *show;
    gint xres, yres, i, j, size, id;
    guint count, step;
    gdouble q;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     GWY_APP_SHOW_FIELD, &showfield,
                                     0);
    g_return_if_fail(dfield && dquark && squark);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    size = 2*args->size + 1;

    gwy_app_wait_start(gwy_app_find_window_for_channel(data, id),
                       _("Rank transform..."));
    gwy_app_wait_set_fraction(0.0);

    showfield = gwy_data_field_new_alike(dfield, FALSE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(showfield), "");
    show = gwy_data_field_get_data(showfield);

    step = MAX(10000, xres*yres/100);
    count = 0;
    q = 1.0/(xres*yres);
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            show[i*xres + j] = local_rank(dfield, size, j, i);
            if ((++count) % step == 0) {
                if (!gwy_app_wait_set_fraction(q*count))
                    goto cancelled;
            }
        }
    }

    gwy_data_field_normalize(showfield);
    if (!gwy_app_wait_set_fraction(1.0))
        goto cancelled;

    gwy_app_undo_qcheckpointv(data, 1, &squark);
    gwy_container_set_object(data, squark, showfield);
    gwy_data_field_data_changed(showfield);

cancelled:
    g_object_unref(showfield);
    gwy_app_wait_finish();
}

static gdouble
local_rank(GwyDataField *data_field,
           gint size,
           gint col, gint row)
{
    gint xres, yres;
    gint i, j;
    gint xfrom, xto, yfrom, yto;
    guint r, xlen, ylen;
    const gdouble *data;
    gdouble v;

    xres = data_field->xres;
    yres = data_field->yres;
    data = data_field->data;
    v = data[row*xres + col];

    yfrom = MAX(0, row - (size-1)/2);
    yto = MIN(yres-1, row + size/2);
    ylen = yto - yfrom + 1;

    xfrom = MAX(0, col - (size-1)/2);
    xto = MIN(xres-1, col + size/2);
    xlen = xto - xfrom + 1;

    r = 0;
    for (i = yfrom; i <= yto; i++) {
        const gdouble *d = data + i*xres + xfrom;
        for (j = xlen; j; j--, d++) {
            if (*d <= v) {
                r += 2;
                if (G_UNLIKELY(*d == v))
                    r--;
            }
        }
    }

    return 0.5*r/(xlen*ylen);
}

static const gchar size_key[]   = "/module/rank/size";

static void
sanitize_args(RankArgs *args)
{
    args->size = CLAMP(args->size, 1, MAX_SIZE);
}

static void
load_args(GwyContainer *container,
          RankArgs *args)
{
    *args = rank_defaults;

    gwy_container_gis_int32_by_name(container, size_key, &args->size);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          RankArgs *args)
{
    gwy_container_set_int32_by_name(container, size_key, args->size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
