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

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/undo.h>

#define UNROTATE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function. */
typedef struct {
    gint col_degree;
    gint row_degree;
    gboolean do_extract;
} PolyLevelArgs;

typedef struct {
    GtkObject *col_degree;
    GtkObject *row_degree;
    GtkWidget *do_extract;
} PolyLevelControls;

static gboolean         module_register          (const gchar *name);
static gboolean         poly_level               (GwyContainer *data,
                                                  GwyRunType run);
static void             poly_level_do            (GwyContainer *data,
                                                  PolyLevelArgs *args);
static gboolean         poly_level_dialog        (PolyLevelArgs *args);
static void             poly_level_dialog_update (PolyLevelControls *controls,
                                                  PolyLevelArgs *args);
static void             poly_level_update_values (PolyLevelControls *controls,
                                                  PolyLevelArgs *args);
static void             load_args                (GwyContainer *container,
                                                  PolyLevelArgs *args);
static void             save_args                (GwyContainer *container,
                                                  PolyLevelArgs *args);
static void             sanitize_args            (PolyLevelArgs *args);

PolyLevelArgs poly_level_defaults = {
    3,
    3,
    FALSE
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "poly_level",
    N_("Subtract polynomial background."),
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
    static GwyProcessFuncInfo poly_level_func_info = {
        "poly_level",
        N_("/_Level/_Polynomial Background..."),
        (GwyProcessFunc)&poly_level,
        UNROTATE_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &poly_level_func_info);

    return TRUE;
}

static gboolean
poly_level(GwyContainer *data, GwyRunType run)
{
    PolyLevelArgs args;
    gboolean ok;

    g_return_val_if_fail(run & UNROTATE_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = poly_level_defaults;
    else
        load_args(gwy_app_settings_get(), &args);

    ok = (run != GWY_RUN_MODAL) || poly_level_dialog(&args);
    if (run == GWY_RUN_MODAL)
        save_args(gwy_app_settings_get(), &args);
    if (ok)
        poly_level_do(data, &args);

    return ok;
}

static void
poly_level_do(GwyContainer *data,
              PolyLevelArgs *args)
{
    GtkWidget *data_window;
    GwyContainer *newdata;
    const guchar *pal = GWY_PALETTE_GRAY;
    GwyDataField *dfield;
    gdouble *coeffs;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    coeffs = gwy_data_field_area_fit_polynom(dfield, 0, 0,
                                             gwy_data_field_get_xres(dfield),
                                             gwy_data_field_get_yres(dfield),
                                             args->col_degree, args->row_degree,
                                             NULL);
    gwy_data_field_area_subtract_polynom(dfield, 0, 0,
                                         gwy_data_field_get_xres(dfield),
                                         gwy_data_field_get_yres(dfield),
                                         args->col_degree, args->row_degree,
                                         coeffs);
    if (!args->do_extract) {
        g_free(coeffs);
        return;
    }

    dfield = (GwyDataField*)gwy_serializable_duplicate(G_OBJECT(dfield));
    gwy_data_field_fill(dfield, 0.0);
    gwy_data_field_area_subtract_polynom(dfield, 0, 0,
                                         gwy_data_field_get_xres(dfield),
                                         gwy_data_field_get_yres(dfield),
                                         args->col_degree, args->row_degree,
                                         coeffs);
    gwy_data_field_invert(dfield, FALSE, FALSE, TRUE);
    g_free(coeffs);

    gwy_container_gis_string_by_name(data, "/0/base/palette", &pal);
    newdata = (GwyContainer*)gwy_container_new();
    gwy_container_set_string_by_name(newdata, "/0/base/palette", g_strdup(pal));
    gwy_container_set_object_by_name(newdata, "/0/data", G_OBJECT(dfield));
    data_window = gwy_app_data_window_create(newdata);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                     _("Background"));
}

static gboolean
poly_level_dialog(PolyLevelArgs *args)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table;
    PolyLevelControls controls;
    gint response;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Remove Polynomial Background"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(2, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    controls.col_degree = gtk_adjustment_new(args->col_degree, 0, 5, 1, 1, 0);
    gwy_table_attach_spinbutton(table, row++,
                                _("_Horizontal polynom degree:"), "",
                                controls.col_degree);

    controls.row_degree = gtk_adjustment_new(args->row_degree, 0, 5, 1, 1, 0);
    gwy_table_attach_spinbutton(table, row++,
                                _("_Vertical polynom degree:"), "",
                                controls.row_degree);

    controls.do_extract
        = gtk_check_button_new_with_mnemonic(_("E_xtract background"));
    gtk_table_attach(GTK_TABLE(table), controls.do_extract,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_extract),
                                 args->do_extract);
    row++;

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
}

static void
poly_level_update_values(PolyLevelControls *controls,
                         PolyLevelArgs *args)
{
    args->col_degree
        = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->col_degree));
    args->row_degree
        = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->row_degree));
    args->do_extract
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->do_extract));
}

static const gchar *col_degree_key = "/module/poly_level/col_degree";
static const gchar *row_degree_key = "/module/poly_level/row_degree";
static const gchar *do_extract_key = "/module/poly_level/do_extract";

static void
sanitize_args(PolyLevelArgs *args)
{
    args->col_degree = CLAMP(args->col_degree, 0, 3);
    args->row_degree = CLAMP(args->row_degree, 0, 3);
    args->do_extract = !!args->do_extract;
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
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
