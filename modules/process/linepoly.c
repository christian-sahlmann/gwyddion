/*
 *  @(#) $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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
#include <libprocess/level.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define LINEPOLY_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

/* Lower symmetric part indexing */
/* i MUST be greater or equal than j */
#define SLi(a, i, j) a[(i)*((i) + 1)/2 + (j)]

enum {
    PREVIEW_SIZE = 320,
    MAX_DEGREE = 5,
};

typedef struct {
    gint max_degree;
    gboolean do_extract;
    GwyMaskingType masking;
} LinePolyArgs;

typedef struct {
    LinePolyArgs *args;
    GtkWidget *dialog;
    GtkObject *max_degree;
    GSList *masking_group;
    GtkWidget *do_extract;
    GtkWidget *dataview;
    GwyContainer *data;
    GwyDataField *dfield;
    gboolean in_update;
} LinePolyControls;

static gboolean module_register       (void);
static void     linepoly              (GwyContainer *data,
                                       GwyRunType run);
static void     linepoly_do           (GwyDataField *dfield,
                                       GwyDataField *mask,
                                       GwyDataField *bg,
                                       const LinePolyArgs *args);
static gboolean linepoly_dialog       (LinePolyArgs *args,
                                       GwyContainer *data,
                                       GwyDataField *dfield,
                                       GwyDataField *mfield,
                                       gint id);
static void     linepoly_dialog_update(LinePolyControls *controls,
                                       LinePolyArgs *args);
static void     degree_changed        (LinePolyControls *controls,
                                       GtkObject *adj);
static void     do_extract_changed    (LinePolyControls *controls,
                                       GtkToggleButton *check);
static void     masking_changed       (GtkToggleButton *button,
                                       LinePolyControls *controls);
static void     update_preview        (LinePolyControls *controls,
                                       LinePolyArgs *args);
static void     load_args             (GwyContainer *container,
                                       LinePolyArgs *args);
static void     save_args             (GwyContainer *container,
                                       LinePolyArgs *args);
static void     sanitize_args         (LinePolyArgs *args);

static const LinePolyArgs linepoly_defaults = {
    1,
    FALSE,
    GWY_MASK_IGNORE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Levels rows by subtracting polynomial background."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("linepoly",
                              (GwyProcessFunc)&linepoly,
                              N_("/_Correct Data/_Polynomial Line Correction..."),
                              GWY_STOCK_POLYNOM_LEVEL,
                              LINEPOLY_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Polynomial row levelling"));

    return TRUE;
}

static void
linepoly(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield, *bg = NULL;
    GQuark quark;
    LinePolyArgs args;
    gboolean ok;
    gint id, newid;

    g_return_if_fail(run & LINEPOLY_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && quark);

    load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = linepoly_dialog(&args, data, dfield, mfield, id);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    gwy_app_undo_qcheckpointv(data, 1, &quark);
    if (args.do_extract)
        bg = gwy_data_field_new_alike(dfield, FALSE);

    linepoly_do(dfield, mfield, bg, &args);
    gwy_data_field_data_changed(dfield);
    gwy_app_channel_log_add_proc(data, id, id);
    if (!bg)
        return;

    newid = gwy_app_data_browser_add_data_field(bg, data, TRUE);
    g_object_unref(bg);
    gwy_app_sync_data_items(data, data, id, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            0);
    gwy_app_set_data_field_title(data, newid, _("Row background"));
    gwy_app_channel_log_add(data, id, newid, NULL, NULL);
}

static void
linepoly_do(GwyDataField *dfield,
            GwyDataField *mask,
            GwyDataField *bg,
            const LinePolyArgs *args)
{
    GwyMaskingType masking;
    gdouble *xpowers, *zxpowers, *matrix;
    gint xres, yres, degree, i, j, k, n;
    gdouble xc;
    const gdouble *m;
    gdouble *d, *b;

    masking = args->masking;
    if (masking == GWY_MASK_IGNORE)
        mask = NULL;
    if (!mask)
        masking = GWY_MASK_IGNORE;

    degree = args->max_degree;
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xc = 0.5*(xres - 1);
    d = gwy_data_field_get_data(dfield);

    m = mask ? gwy_data_field_get_data_const(mask) : NULL;
    b = bg ? gwy_data_field_get_data(bg) : NULL;

    xpowers = g_new(gdouble, 2*degree+1);
    zxpowers = g_new(gdouble, degree+1);
    matrix = g_new(gdouble, (degree+1)*(degree+2)/2);
    for (i = 0; i < yres; i++) {
        gwy_clear(xpowers, 2*degree+1);
        gwy_clear(zxpowers, degree+1);

        n = 0;
        for (j = 0; j < xres; j++) {
            gdouble p = 1.0, x = j - xc;

            if ((masking == GWY_MASK_INCLUDE && m[j] <= 0.0)
                || (masking == GWY_MASK_EXCLUDE && m[j] >= 1.0))
                continue;

            for (k = 0; k <= degree; k++) {
                xpowers[k] += p;
                zxpowers[k] += p*d[j];
                p *= x;
            }
            for (k = degree+1; k <= 2*degree; k++) {
                xpowers[k] += p;
                p *= x;
            }
            n++;
        }

        /* Solve polynomial coefficients. */
        if (n > degree) {
            for (j = 0; j <= degree; j++) {
                for (k = 0; k <= j; k++)
                    SLi(matrix, j, k) = xpowers[j + k];
            }
            gwy_math_choleski_decompose(degree+1, matrix);
            gwy_math_choleski_solve(degree+1, matrix, zxpowers);
        }
        else
            gwy_clear(zxpowers, degree+1);

        /* Subtract. */
        for (j = 0; j < xres; j++) {
            gdouble p = 1.0, x = j - xc, z = 0.0;

            for (k = 0; k <= degree; k++) {
                z += p*zxpowers[k];
                p *= x;
            }

            d[j] -= z;
            if (b)
                b[j] += z;
        }

        d += xres;
        m = m ? m+xres : NULL;
        b = b ? b+xres : NULL;
    }

    g_free(matrix);
    g_free(zxpowers);
    g_free(xpowers);
}

static gboolean
linepoly_dialog(LinePolyArgs *args,
                GwyContainer *data,
                GwyDataField *dfield,
                GwyDataField *mfield,
                gint id)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *label, *hbox;
    GwyPixmapLayer *layer;
    LinePolyControls controls;
    gint response;
    gint row;

    controls.args = args;
    controls.in_update = TRUE;
    controls.dfield = dfield;
    controls.data = gwy_container_new();

    dfield = gwy_data_field_duplicate(dfield);
    gwy_container_set_object_by_name(controls.data, "/0/data", dfield);
    gwy_object_unref(dfield);
    gwy_app_sync_data_items(data, controls.data, id, 0, FALSE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            0);
    if (mfield)
        gwy_container_set_object_by_name(controls.data, "/mask", mfield);

    dialog = gtk_dialog_new_with_buttons(_("Polynomial Row Levelling"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    controls.dataview = gwy_data_view_new(controls.data);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.dataview), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.dataview), layer);
    gtk_box_pack_start(GTK_BOX(hbox), controls.dataview, FALSE, FALSE, 4);

    table = gtk_table_new(2 + (mfield ? 4 : 0), 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);
    row = 0;

    controls.max_degree = gtk_adjustment_new(args->max_degree,
                                             0, MAX_DEGREE, 1, 1, 0);
    gwy_table_attach_hscale(table, row++,
                            _("_Polynomial degree:"), NULL,
                            controls.max_degree, 0);
    g_signal_connect_swapped(controls.max_degree, "value-changed",
                             G_CALLBACK(degree_changed), &controls);

    controls.do_extract
        = gtk_check_button_new_with_mnemonic(_("E_xtract background"));
    gtk_table_attach(GTK_TABLE(table), controls.do_extract,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_extract),
                                 args->do_extract);
    g_signal_connect_swapped(controls.do_extract, "toggled",
                             G_CALLBACK(do_extract_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    if (mfield) {
        label = gwy_label_new_header(_("Masking Mode"));
        gtk_table_attach(GTK_TABLE(table), label,
                        0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;

        controls.masking_group
            = gwy_radio_buttons_create(gwy_masking_type_get_enum(), -1,
                                       G_CALLBACK(masking_changed),
                                       &controls, args->masking);
        row = gwy_radio_buttons_attach_to_table(controls.masking_group,
                                                GTK_TABLE(table), 3, row);
    }
    else
        controls.masking_group = NULL;

    controls.in_update = FALSE;
    update_preview(&controls, args);

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
            *args = linepoly_defaults;
            linepoly_dialog_update(&controls, args);
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
linepoly_dialog_update(LinePolyControls *controls,
                       LinePolyArgs *args)
{
    controls->in_update = TRUE;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->max_degree),
                             args->max_degree);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->do_extract),
                                 args->do_extract);
    if (controls->masking_group)
        gwy_radio_buttons_set_current(controls->masking_group, args->masking);
    controls->in_update = FALSE;
    update_preview(controls, args);
}

static void
degree_changed(LinePolyControls *controls,
               GtkObject *adj)
{
    LinePolyArgs *args = controls->args;

    args->max_degree = gwy_adjustment_get_int(GTK_ADJUSTMENT(adj));
    if (controls->in_update)
        return;

    update_preview(controls, controls->args);
}

static void
do_extract_changed(LinePolyControls *controls,
                   GtkToggleButton *check)
{
    controls->args->do_extract = gtk_toggle_button_get_active(check);
}

static void
masking_changed(GtkToggleButton *button,
                LinePolyControls *controls)
{
    LinePolyArgs *args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args = controls->args;
    args->masking = gwy_radio_buttons_get_current(controls->masking_group);
    if (controls->in_update)
        return;

    update_preview(controls, args);
}

static void
update_preview(LinePolyControls *controls, LinePolyArgs *args)
{
    GwyDataField *source, *leveled, *mask = NULL;

    source = controls->dfield;
    gwy_container_gis_object_by_name(controls->data, "/mask", &mask);
    gwy_container_gis_object_by_name(controls->data, "/0/data", &leveled);
    gwy_data_field_copy(source, leveled, FALSE);
    linepoly_do(leveled, mask, NULL, args);
    gwy_data_field_data_changed(leveled);
}

static const gchar do_extract_key[] = "/module/linepoly/do_extract";
static const gchar masking_key[]    = "/module/linepoly/masking";
static const gchar max_degree_key[] = "/module/linepoly/max_degree";

static void
sanitize_args(LinePolyArgs *args)
{
    args->max_degree = CLAMP(args->max_degree, 0, MAX_DEGREE);
    args->masking = MIN(args->masking, GWY_MASK_INCLUDE);
    args->do_extract = !!args->do_extract;
}

static void
load_args(GwyContainer *container,
          LinePolyArgs *args)
{
    *args = linepoly_defaults;

    gwy_container_gis_int32_by_name(container, max_degree_key,
                                    &args->max_degree);
    gwy_container_gis_enum_by_name(container, masking_key,
                                   &args->masking);
    gwy_container_gis_boolean_by_name(container, do_extract_key,
                                      &args->do_extract);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          LinePolyArgs *args)
{
    gwy_container_set_int32_by_name(container, max_degree_key,
                                    args->max_degree);
    gwy_container_set_enum_by_name(container, masking_key,
                                   args->masking);
    gwy_container_set_boolean_by_name(container, do_extract_key,
                                      args->do_extract);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
