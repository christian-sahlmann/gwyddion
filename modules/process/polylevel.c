/*
 *  @(#) $Id$
 *  Copyright (C) 2004,2008 David Necas (Yeti), Petr Klapetek.
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

#define POLYLEVEL_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 160,
    MAX_DEGREE = 12
};

typedef struct {
    gint col_degree;
    gint row_degree;
    gint max_degree;
    gboolean do_extract;
    gboolean same_degree;
    gboolean independent;
    GwyMaskingType masking;
} PolyLevelArgs;

typedef struct {
    PolyLevelArgs *args;
    GtkObject *col_degree;
    GtkObject *row_degree;
    GtkObject *max_degree;
    GSList *type_group;
    GtkWidget *same_degree;
    GtkWidget *independent;
    GSList *masking_group;
    GtkWidget *do_extract;
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
                                                  GwyDataField *mfield,
                                                  GQuark quark,
                                                  gint oldid,
                                                  const PolyLevelArgs *args);
static gboolean poly_level_dialog                (PolyLevelArgs *args,
                                                  GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield,
                                                  gint id);
static void     poly_level_dialog_update         (PolyLevelControls *controls,
                                                  PolyLevelArgs *args);
static void     poly_level_update_values         (PolyLevelControls *controls,
                                                  PolyLevelArgs *args);
static void     poly_level_type_changed          (GtkToggleButton *button,
                                                  PolyLevelControls *controls);
static void     poly_level_same_degree_changed   (GtkWidget *button,
                                                  PolyLevelControls *controls);
static void     poly_level_degree_changed        (GtkObject *spin,
                                                  PolyLevelControls *controls);
static void     poly_level_max_degree_changed    (GtkObject *spin,
                                                  PolyLevelControls *controls);
static void     poly_level_masking_changed       (GtkToggleButton *button,
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
    3,
    FALSE,
    TRUE,
    TRUE,
    GWY_MASK_IGNORE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Subtracts polynomial background."),
    "Yeti <yeti@gwyddion.net>",
    "2.5",
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
                              N_("Remove polynomial background"));

    return TRUE;
}

static void
poly_level(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield;
    GQuark quark;
    PolyLevelArgs args;
    gboolean ok;
    gint id;

    g_return_if_fail(run & POLYLEVEL_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && quark);

    load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = poly_level_dialog(&args, data, dfield, mfield, id);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
    poly_level_do(data, dfield, mfield, quark, id, &args);
}

static void
poly_level_do_independent(GwyDataField *dfield,
                          GwyDataField *result,
                          GwyDataField *bg,
                          gint col_degree, gint row_degree)
{
    gint i;
    gdouble *coeffs;

    coeffs = gwy_data_field_fit_legendre(dfield, col_degree, row_degree, NULL);
    gwy_data_field_subtract_legendre(result, col_degree, row_degree, coeffs);
    gwy_data_field_data_changed(result);

    if (bg) {
        /* Invert coeffs, we do not have anything like add_polynomial() */
        for (i = 0; i < (col_degree + 1)*(row_degree + 1); i++)
            coeffs[i] = -coeffs[i];
        gwy_data_field_subtract_legendre(bg, col_degree, row_degree, coeffs);
        gwy_data_field_data_changed(bg);
    }

    g_free(coeffs);
}

static void
poly_level_do_maximum(GwyDataField *dfield,
                      GwyDataField *result,
                      GwyDataField *bg,
                      gint max_degree)
{
    gint i;
    gdouble *coeffs;

    coeffs = gwy_data_field_fit_poly_max(dfield, max_degree, NULL);
    gwy_data_field_subtract_poly_max(result, max_degree, coeffs);
    gwy_data_field_data_changed(result);

    if (bg) {
        /* Invert coeffs, we do not have anything like add_polynomial() */
        for (i = 0; i < (max_degree + 1)*(max_degree + 2)/2; i++)
            coeffs[i] = -coeffs[i];
        gwy_data_field_subtract_poly_max(bg, max_degree, coeffs);
        gwy_data_field_data_changed(bg);
    }

    g_free(coeffs);
}

static void
poly_level_do_with_mask(GwyDataField *dfield,
                        GwyDataField *mask,
                        GwyDataField *result,
                        GwyDataField *bg,
                        const PolyLevelArgs *args)
{
    gint *term_powers;
    gdouble *coeffs;
    gint nterms, i, j, k;

    k = 0;
    if (args->independent) {
        nterms = (args->col_degree + 1)*(args->row_degree + 1);
        term_powers = g_new(gint, 2*nterms);
        for (i = 0; i <= args->col_degree; i++) {
            for (j = 0; j <= args->row_degree; j++) {
                term_powers[k++] = i;
                term_powers[k++] = j;
            }
        }
    }
    else {
        nterms = (args->max_degree + 1)*(args->max_degree + 2)/2;
        term_powers = g_new(gint, 2*nterms);
        for (i = 0; i <= args->max_degree; i++) {
            for (j = 0; j <= args->max_degree - i; j++) {
                term_powers[k++] = i;
                term_powers[k++] = j;
            }
        }
    }

    coeffs = gwy_data_field_fit_poly(dfield, mask, nterms, term_powers,
                                     args->masking == GWY_MASK_EXCLUDE, NULL);
    gwy_data_field_subtract_poly(result, nterms, term_powers, coeffs);
    gwy_data_field_data_changed(result);

    if (bg) {
        for (i = 0; i < nterms; i++) {
            coeffs[i] = -coeffs[i];
        }
        gwy_data_field_subtract_poly(bg, nterms, term_powers, coeffs);
        gwy_data_field_data_changed(bg);
    }

    g_free(coeffs);
    g_free(term_powers);
}

static void
poly_level_do(GwyContainer *data,
              GwyDataField *dfield,
              GwyDataField *mfield,
              GQuark quark,
              gint oldid,
              const PolyLevelArgs *args)
{
    GwyDataField *bg = NULL;
    gint newid;

    gwy_app_undo_qcheckpointv(data, 1, &quark);
    if (args->do_extract)
        bg = gwy_data_field_new_alike(dfield, TRUE);

    if (mfield && args->masking != GWY_MASK_IGNORE)
        poly_level_do_with_mask(dfield, mfield, dfield, bg, args);
    else if (args->independent)
        poly_level_do_independent(dfield, dfield, bg,
                                  args->col_degree, args->row_degree);
    else
        poly_level_do_maximum(dfield, dfield, bg, args->max_degree);

    if (!args->do_extract)
        return;

    newid = gwy_app_data_browser_add_data_field(bg, data, TRUE);
    g_object_unref(bg);
    gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            0);
    gwy_app_set_data_field_title(data, newid, _("Background"));
}

/* create a smaller copy of data */
static GwyContainer*
create_preview_data(GwyContainer *data,
                    GwyDataField *dfield,
                    GwyDataField *mfield,
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

    /* Mask */
    if (mfield) {
        pfield = gwy_data_field_new_resampled(mfield, xres, yres,
                                              GWY_INTERPOLATION_ROUND);
        gwy_container_set_object_by_name(pdata, "/mask", pfield);
        g_object_unref(pfield);
    }

    /* Leveled */
    pfield = gwy_data_field_new_alike(pfield, FALSE);
    gwy_container_set_object_by_name(pdata, "/0/data", pfield);
    g_object_unref(pfield);

    /* Background */
    pfield = gwy_data_field_new_alike(pfield, FALSE);
    gwy_container_set_object_by_name(pdata, "/1/data", pfield);
    g_object_unref(pfield);

    gwy_app_sync_data_items(data, pdata, id, 0, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            0);
    gwy_app_sync_data_items(data, pdata, id, 1, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            0);

    return pdata;
}

static gboolean
poly_level_dialog(PolyLevelArgs *args,
                  GwyContainer *data,
                  GwyDataField *dfield,
                  GwyDataField *mfield,
                  gint id)
{
    enum { RESPONSE_RESET = 1 };
    static const GwyEnum types[] = {
        { N_("Independent degrees"),  TRUE,  },
        { N_("Limited total degree"), FALSE, },
    };
    GtkWidget *dialog, *table, *label, *hbox;
    GwyPixmapLayer *layer;
    PolyLevelControls controls;
    gint response;
    gint row;

    controls.args = args;
    controls.in_update = TRUE;
    controls.data = create_preview_data(data, dfield, mfield, id);

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
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 12);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    controls.leveled_view = gwy_data_view_new(controls.data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.leveled_view),
                                  "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.leveled_view), layer);
    gtk_table_attach(GTK_TABLE(table), controls.leveled_view,
                     0, 1, row, row+1, 0, 0, 0, 0);

    controls.bg_view = gwy_data_view_new(controls.data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/1/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/1/base/palette");
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.bg_view), "/1/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.bg_view), layer);
    gtk_table_attach(GTK_TABLE(table), controls.bg_view,
                     1, 2, row, row+1, 0, 0, 0, 0);

    g_object_unref(controls.data);
    row++;

    label = gtk_label_new(_("Leveled data"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    label = gtk_label_new(_("Background"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    row++;

    table = gtk_table_new(7 + (mfield ? 4 : 0), 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);
    row = 0;

    controls.type_group
        = gwy_radio_buttons_create(types, G_N_ELEMENTS(types),
                                   G_CALLBACK(poly_level_type_changed),
                                   &controls,
                                   args->independent);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(controls.type_group->data),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

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
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.same_degree),
                                 args->same_degree);
    g_signal_connect(controls.same_degree, "toggled",
                     G_CALLBACK(poly_level_same_degree_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_table_attach(GTK_TABLE(table),
                     GTK_WIDGET(controls.type_group->next->data),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.max_degree = gtk_adjustment_new(args->max_degree,
                                             0, MAX_DEGREE, 1, 1, 0);
    gwy_table_attach_hscale(table, row,
                            _("_Maximum polynom degree:"), NULL,
                            controls.max_degree, 0);
    g_signal_connect(controls.max_degree, "value-changed",
                     G_CALLBACK(poly_level_max_degree_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.do_extract
        = gtk_check_button_new_with_mnemonic(_("E_xtract background"));
    gtk_table_attach(GTK_TABLE(table), controls.do_extract,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_extract),
                                 args->do_extract);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    if (mfield) {
        label = gwy_label_new_header(_("Masking Mode"));
        gtk_table_attach(GTK_TABLE(table), label,
                        0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;

        controls.masking_group
            = gwy_radio_buttons_create(gwy_masking_type_get_enum(), -1,
                                       G_CALLBACK(poly_level_masking_changed),
                                       &controls, args->masking);
        row = gwy_radio_buttons_attach_to_table(controls.masking_group,
                                                GTK_TABLE(table), 3, row);
    }
    else
        controls.masking_group = NULL;

    controls.in_update = FALSE;
    gtk_widget_set_sensitive(controls.same_degree, args->independent);
    gwy_table_hscale_set_sensitive(controls.row_degree, args->independent);
    gwy_table_hscale_set_sensitive(controls.col_degree, args->independent);
    gwy_table_hscale_set_sensitive(controls.max_degree, !args->independent);
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
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->max_degree),
                             args->max_degree);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->do_extract),
                                 args->do_extract);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->same_degree),
                                 args->same_degree);
    gwy_radio_buttons_set_current(controls->type_group, args->independent);
    if (controls->masking_group)
        gwy_radio_buttons_set_current(controls->masking_group, args->masking);
}

static void
poly_level_update_values(PolyLevelControls *controls,
                         PolyLevelArgs *args)
{
    args->col_degree = gwy_adjustment_get_int(controls->col_degree);
    args->row_degree = gwy_adjustment_get_int(controls->row_degree);
    args->max_degree = gwy_adjustment_get_int(controls->max_degree);
    args->do_extract
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->do_extract));
    args->same_degree
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->same_degree));
    args->independent = gwy_radio_buttons_get_current(controls->type_group);
    if (controls->masking_group)
        args->masking = gwy_radio_buttons_get_current(controls->masking_group);
}

static void
poly_level_type_changed(GtkToggleButton *button,
                        PolyLevelControls *controls)
{
    PolyLevelArgs *args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args = controls->args;
    args->independent = gwy_radio_buttons_get_current(controls->type_group);
    gtk_widget_set_sensitive(controls->same_degree, args->independent);
    gwy_table_hscale_set_sensitive(controls->row_degree, args->independent);
    gwy_table_hscale_set_sensitive(controls->col_degree, args->independent);
    gwy_table_hscale_set_sensitive(controls->max_degree, !args->independent);
    poly_level_update_preview(controls, args);
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
    degree = GWY_ROUND(v);
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
poly_level_max_degree_changed(GtkObject *spin,
                              PolyLevelControls *controls)
{
    PolyLevelArgs *args;
    gdouble v;
    gint degree;

    if (controls->in_update)
        return;

    args = controls->args;
    v = gtk_adjustment_get_value(GTK_ADJUSTMENT(spin));
    degree = GWY_ROUND(v);
    if (degree == args->max_degree)
        return;

    args->max_degree = degree;
    poly_level_update_preview(controls, controls->args);
}

static void
poly_level_masking_changed(GtkToggleButton *button,
                           PolyLevelControls *controls)
{
    PolyLevelArgs *args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args = controls->args;
    args->masking = gwy_radio_buttons_get_current(controls->masking_group);
    poly_level_update_preview(controls, args);
}

static void
poly_level_update_preview(PolyLevelControls *controls,
                          PolyLevelArgs *args)
{
    GwyDataField *source, *leveled, *bg, *mask = NULL;

    gwy_container_gis_object_by_name(controls->data, "/source", &source);
    gwy_container_gis_object_by_name(controls->data, "/mask", &mask);
    gwy_container_gis_object_by_name(controls->data, "/0/data", &leveled);
    gwy_container_gis_object_by_name(controls->data, "/1/data", &bg);

    gwy_data_field_copy(source, leveled, FALSE);
    gwy_data_field_clear(bg);

    if (mask && args->masking != GWY_MASK_IGNORE)
        poly_level_do_with_mask(source, mask, leveled, bg, args);
    else if (args->independent)
        poly_level_do_independent(source, leveled, bg,
                                  args->col_degree, args->row_degree);
    else
        poly_level_do_maximum(source, leveled, bg, args->max_degree);
}

static const gchar col_degree_key[]  = "/module/polylevel/col_degree";
static const gchar row_degree_key[]  = "/module/polylevel/row_degree";
static const gchar max_degree_key[]  = "/module/polylevel/max_degree";
static const gchar do_extract_key[]  = "/module/polylevel/do_extract";
static const gchar same_degree_key[] = "/module/polylevel/same_degree";
static const gchar independent_key[] = "/module/polylevel/independent";
static const gchar masking_key[]     = "/module/polylevel/masking";

static void
sanitize_args(PolyLevelArgs *args)
{
    args->col_degree = CLAMP(args->col_degree, 0, MAX_DEGREE);
    args->row_degree = CLAMP(args->row_degree, 0, MAX_DEGREE);
    args->max_degree = CLAMP(args->max_degree, 0, MAX_DEGREE);
    args->masking = MIN(args->masking, GWY_MASK_INCLUDE);
    args->do_extract = !!args->do_extract;
    args->independent = !!args->independent;
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
    gwy_container_gis_int32_by_name(container, max_degree_key,
                                    &args->max_degree);
    gwy_container_gis_enum_by_name(container, masking_key,
                                   &args->masking);
    gwy_container_gis_boolean_by_name(container, do_extract_key,
                                      &args->do_extract);
    gwy_container_gis_boolean_by_name(container, same_degree_key,
                                      &args->same_degree);
    gwy_container_gis_boolean_by_name(container, independent_key,
                                      &args->independent);
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
    gwy_container_set_int32_by_name(container, max_degree_key,
                                    args->max_degree);
    gwy_container_set_enum_by_name(container, masking_key,
                                   args->masking);
    gwy_container_set_boolean_by_name(container, do_extract_key,
                                      args->do_extract);
    gwy_container_set_boolean_by_name(container, same_degree_key,
                                      args->same_degree);
    gwy_container_set_boolean_by_name(container, independent_key,
                                      args->independent);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
