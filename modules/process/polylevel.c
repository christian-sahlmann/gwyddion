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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/level.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define POLYLEVEL_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 120,
    MAX_DEGREE = 12
};

typedef struct {
    gint col_degree;
    gint row_degree;
    gboolean do_extract;
    gboolean same_degree;
} PolyLevelArgs;

typedef struct {
    PolyLevelArgs *args;
    GtkObject *col_degree;
    GtkObject *row_degree;
    GtkWidget *do_extract;
    GtkWidget *same_degree;
    GtkWidget *leveled_view;
    GtkWidget *bg_view;
    GwyContainer *data;
    gboolean in_update;
} PolyLevelControls;

static gboolean module_register                  (void);
static void     poly_level                       (GwyContainer *data,
                                                  GwyRunType run);
static void     poly_level_do                    (GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  GQuark quark,
                                                  gint oldid,
                                                  PolyLevelArgs *args);
static gboolean poly_level_dialog                (PolyLevelArgs *args,
                                                  GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  gint id);
static void     poly_level_dialog_update         (PolyLevelControls *controls,
                                                  PolyLevelArgs *args);
static void     poly_level_update_values         (PolyLevelControls *controls,
                                                  PolyLevelArgs *args);
static void     poly_level_same_degree_changed   (GtkWidget *button,
                                                  PolyLevelControls *controls);
static void     poly_level_degree_changed        (GtkObject *spin,
                                                  PolyLevelControls *controls);
static void     poly_level_update_preview        (PolyLevelControls *controls,
                                                  PolyLevelArgs *args);
static void     load_args                        (GwyContainer *container,
                                                  PolyLevelArgs *args);
static void     save_args                        (GwyContainer *container,
                                                  PolyLevelArgs *args);
static void     sanitize_args                    (PolyLevelArgs *args);

static const PolyLevelArgs poly_level_defaults = {
    3,
    3,
    FALSE,
    TRUE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Subtracts polynomial background."),
    "Yeti <yeti@gwyddion.net>",
    "2.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("polylevel",
                              (GwyProcessFunc)&poly_level,
                              N_("/_Level/_Polynomial Background..."),
                              GWY_STOCK_POLYNOM,
                              POLYLEVEL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Remove polynomial backgroud"));

    return TRUE;
}

static void
poly_level(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GQuark quark;
    PolyLevelArgs args;
    gboolean ok;
    gint id;

    g_return_if_fail(run & POLYLEVEL_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && quark);

    load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = poly_level_dialog(&args, data, dfield, id);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
    poly_level_do(data, dfield, quark, id, &args);
}

static void
poly_level_do(GwyContainer *data,
              GwyDataField *dfield,
              GQuark quark,
              gint oldid,
              PolyLevelArgs *args)
{
    gint xres, yres, newid, i;
    gdouble *coeffs;

    gwy_app_undo_qcheckpointv(data, 1, &quark);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    coeffs = gwy_data_field_fit_legendre(dfield,
                                         args->col_degree, args->row_degree,
                                         NULL);
    gwy_data_field_subtract_legendre(dfield,
                                     args->col_degree, args->row_degree,
                                     coeffs);
    gwy_data_field_data_changed(dfield);

    if (!args->do_extract) {
        g_free(coeffs);
        return;
    }

    dfield = gwy_data_field_new_alike(dfield, TRUE);
    /* Invert coeffs, we do not have anything like add_polynomial() */
    for (i = 0; i < (args->col_degree + 1)*(args->row_degree + 1); i++)
        coeffs[i] = -coeffs[i];
    gwy_data_field_subtract_legendre(dfield,
                                     args->col_degree, args->row_degree,
                                     coeffs);
    g_free(coeffs);

    newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
    g_object_unref(dfield);
    gwy_app_copy_data_items(data, data, oldid, newid,
                            GWY_DATA_ITEM_GRADIENT,
                            0);
    gwy_app_set_data_field_title(data, newid, _("Background"));
}

/* create a smaller copy of data */
static GwyContainer*
create_preview_data(GwyContainer *data,
                    GwyDataField *dfield,
                    gint id)
{
    GwyContainer *pdata;
    GwyDataField *pfield;
    gint xres, yres;
    gdouble zoomval;

    pdata = gwy_container_new();
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    zoomval = (gdouble)PREVIEW_SIZE/MAX(xres, yres);
    xres = MAX(xres*zoomval, 3);
    yres = MAX(yres*zoomval, 3);

    /* Base data */
    pfield = gwy_data_field_new_resampled(dfield, xres, yres,
                                          GWY_INTERPOLATION_ROUND);
    gwy_container_set_object_by_name(pdata, "/source", pfield);
    g_object_unref(pfield);

    /* Leveled */
    pfield = gwy_data_field_new_alike(pfield, FALSE);
    gwy_container_set_object_by_name(pdata, "/0/data", pfield);
    g_object_unref(pfield);

    /* Background */
    pfield = gwy_data_field_new_alike(pfield, FALSE);
    gwy_container_set_object_by_name(pdata, "/1/data", pfield);
    g_object_unref(pfield);

    gwy_app_copy_data_items(data, pdata, id, 0, GWY_DATA_ITEM_GRADIENT, 0);
    gwy_app_copy_data_items(data, pdata, id, 1, GWY_DATA_ITEM_GRADIENT, 0);

    return pdata;
}

static gboolean
poly_level_dialog(PolyLevelArgs *args,
                  GwyContainer *data,
                  GwyDataField *dfield,
                  gint id)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *label, *hbox;
    GwyPixmapLayer *layer;
    PolyLevelControls controls;
    gint response;
    gint row;

    controls.args = args;
    controls.in_update = TRUE;
    controls.data = create_preview_data(data, dfield, id);

    dialog = gtk_dialog_new_with_buttons(_("Remove Polynomial Background"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    table = gtk_table_new(2, 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 8);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    controls.leveled_view = gwy_data_view_new(controls.data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.leveled_view), layer);
    gtk_table_attach(GTK_TABLE(table), controls.leveled_view,
                     0, 1, row, row+1, 0, 0, 2, 2);

    controls.bg_view = gwy_data_view_new(controls.data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/1/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/1/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.bg_view), layer);
    gtk_table_attach(GTK_TABLE(table), controls.bg_view,
                     1, 2, row, row+1, 0, 0, 2, 2);

    g_object_unref(controls.data);
    row++;

    label = gtk_label_new(_("Leveled data"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);

    label = gtk_label_new(_("Background"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
    row++;

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);
    row = 0;

    controls.col_degree = gtk_adjustment_new(args->col_degree,
                                             0, MAX_DEGREE, 1, 1, 0);
    gwy_table_attach_hscale(table, row++,
                            _("_Horizontal polynom degree:"), NULL,
                            controls.col_degree, 0);
    g_signal_connect(controls.col_degree, "value-changed",
                     G_CALLBACK(poly_level_degree_changed), &controls);

    controls.row_degree = gtk_adjustment_new(args->row_degree,
                                             0, MAX_DEGREE, 1, 1, 0);
    gwy_table_attach_hscale(table, row++,
                            _("_Vertical polynom degree:"), NULL,
                            controls.row_degree, 0);
    g_signal_connect(controls.row_degree, "value-changed",
                     G_CALLBACK(poly_level_degree_changed), &controls);

    controls.same_degree
        = gtk_check_button_new_with_mnemonic(_("_Same degrees"));
    gtk_table_attach(GTK_TABLE(table), controls.same_degree,
                     0, 4, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.same_degree),
                                 args->same_degree);
    g_signal_connect(controls.same_degree, "toggled",
                     G_CALLBACK(poly_level_same_degree_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.do_extract
        = gtk_check_button_new_with_mnemonic(_("E_xtract background"));
    gtk_table_attach(GTK_TABLE(table), controls.do_extract,
                     0, 4, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_extract),
                                 args->do_extract);
    row++;

    controls.in_update = FALSE;
    poly_level_update_preview(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            poly_level_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = poly_level_defaults;
            poly_level_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    poly_level_update_values(&controls, args);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
poly_level_dialog_update(PolyLevelControls *controls,
                         PolyLevelArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->col_degree),
                             args->col_degree);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->row_degree),
                             args->row_degree);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->do_extract),
                                 args->do_extract);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->same_degree),
                                 args->same_degree);
}

static void
poly_level_update_values(PolyLevelControls *controls,
                         PolyLevelArgs *args)
{
    args->col_degree = gwy_adjustment_get_int(controls->col_degree);
    args->row_degree = gwy_adjustment_get_int(controls->row_degree);
    args->do_extract
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->do_extract));
    args->same_degree
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->same_degree));
}

static void
poly_level_same_degree_changed(GtkWidget *button,
                               PolyLevelControls *controls)
{
    PolyLevelArgs *args;

    args = controls->args;
    args->same_degree = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
    gwy_debug("same_degree = %d", args->same_degree);
    if (!args->same_degree || controls->in_update)
        return;

    controls->in_update = TRUE;
    args->row_degree = args->col_degree;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->row_degree),
                             args->row_degree);
    poly_level_update_preview(controls, controls->args);
    controls->in_update = FALSE;
}

static void
poly_level_degree_changed(GtkObject *spin,
                          PolyLevelControls *controls)
{
    PolyLevelArgs *args;
    gdouble v;
    gint degree;
    gboolean update;

    if (controls->in_update)
        return;

    args = controls->args;
    v = gtk_adjustment_get_value(GTK_ADJUSTMENT(spin));
    degree = ROUND(v);
    if (spin == controls->col_degree) {
        update = args->col_degree != degree;
        args->col_degree = degree;
    }
    else {
        update = args->row_degree != degree;
        args->row_degree = degree;
    }

    if (!args->same_degree) {
        poly_level_update_preview(controls, controls->args);
        return;
    }

    controls->in_update = TRUE;
    if (spin == controls->col_degree) {
        gwy_debug("syncing row := col");
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->row_degree), v);
        args->row_degree = args->col_degree;
    }
    else {
        gwy_debug("syncing col := row");
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->col_degree), v);
        args->col_degree = args->row_degree;
    }
    gwy_debug("col_degree = %f %d, row_degree = %f %d",
              gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->col_degree)),
              args->col_degree,
              gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->row_degree)),
              args->row_degree);

    if (update)
        poly_level_update_preview(controls, controls->args);
    controls->in_update = FALSE;
}

static void
poly_level_update_preview(PolyLevelControls *controls,
                          PolyLevelArgs *args)
{
    GwyDataField *source, *leveled, *bg;
    gdouble *coeffs;
    gint xres, yres, i;

    gwy_container_gis_object_by_name(controls->data, "/source", &source);
    gwy_container_gis_object_by_name(controls->data, "/0/data", &leveled);
    gwy_container_gis_object_by_name(controls->data, "/1/data", &bg);

    xres = gwy_data_field_get_xres(source);
    yres = gwy_data_field_get_yres(source);
    coeffs = gwy_data_field_fit_legendre(source,
                                         args->col_degree, args->row_degree,
                                         NULL);

    gwy_data_field_copy(source, leveled, FALSE);
    gwy_data_field_subtract_legendre(leveled,
                                     args->col_degree, args->row_degree,
                                     coeffs);
    gwy_data_field_data_changed(leveled);

    for (i = 0; i < (args->col_degree + 1)*(args->row_degree + 1); i++)
        coeffs[i] = -coeffs[i];

    gwy_data_field_clear(bg);
    gwy_data_field_subtract_legendre(bg,
                                     args->col_degree, args->row_degree,
                                     coeffs);
    gwy_data_field_data_changed(bg);

    g_free(coeffs);
}

static const gchar col_degree_key[]  = "/module/polylevel/col_degree";
static const gchar row_degree_key[]  = "/module/polylevel/row_degree";
static const gchar do_extract_key[]  = "/module/polylevel/do_extract";
static const gchar same_degree_key[] = "/module/polylevel/same_degree";

static void
sanitize_args(PolyLevelArgs *args)
{
    args->col_degree = CLAMP(args->col_degree, 0, MAX_DEGREE);
    args->row_degree = CLAMP(args->row_degree, 0, MAX_DEGREE);
    args->do_extract = !!args->do_extract;
    args->same_degree = !!args->same_degree;
    if (args->same_degree)
        args->row_degree = args->col_degree;
}

static void
load_args(GwyContainer *container,
          PolyLevelArgs *args)
{
    *args = poly_level_defaults;

    gwy_container_gis_int32_by_name(container, col_degree_key,
                                    &args->col_degree);
    gwy_container_gis_int32_by_name(container, row_degree_key,
                                    &args->row_degree);
    gwy_container_gis_boolean_by_name(container, do_extract_key,
                                      &args->do_extract);
    gwy_container_gis_boolean_by_name(container, same_degree_key,
                                      &args->same_degree);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          PolyLevelArgs *args)
{
    gwy_container_set_int32_by_name(container, col_degree_key,
                                    args->col_degree);
    gwy_container_set_int32_by_name(container, row_degree_key,
                                    args->row_degree);
    gwy_container_set_boolean_by_name(container, do_extract_key,
                                      args->do_extract);
    gwy_container_set_boolean_by_name(container, same_degree_key,
                                      args->same_degree);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
