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

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define LEVEL_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    gboolean exclude;
} LevelArgs;

static gboolean module_register(void);
static void      level               (GwyContainer *data,
                                      GwyRunType run);
static void      level_rotate        (GwyContainer *data,
                                      GwyRunType run);
static void      fix_zero            (GwyContainer *data,
                                      GwyRunType run);
static void      zero_mean           (GwyContainer *data,
                                      GwyRunType run);
static gboolean  level_dialog        (LevelArgs *args);
static void      exclude_changed_cb  (GtkWidget *toggle, LevelArgs *args);
static void      level_load_args     (GwyContainer *container, LevelArgs *args);
static void      level_save_args     (GwyContainer *container, LevelArgs *args);

static const LevelArgs level_defaults = {
    TRUE
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Levels data by simple plane subtraction or by rotation, "
       "and fixes minimal or mean value to zero."),
    "Yeti <yeti@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("level",
                              (GwyProcessFunc)&level,
                              N_("/_Level/_Level"),
                              GWY_STOCK_LEVEL,
                              LEVEL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Level data by plane subtraction"));
    gwy_process_func_register("level_rotate",
                              (GwyProcessFunc)&level_rotate,
                              N_("/_Level/Level _Rotate"),
                              NULL,
                              LEVEL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Automatically level data by plane rotation"));
    gwy_process_func_register("fix_zero",
                              (GwyProcessFunc)&fix_zero,
                              N_("/_Level/Fix _Zero"),
                              GWY_STOCK_FIX_ZERO,
                              LEVEL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Shift minimum data value to zero"));
    gwy_process_func_register("zero_mean",
                              (GwyProcessFunc)&zero_mean,
                              N_("/_Level/Zero _Mean Value"),
                              NULL,
                              LEVEL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Shift mean data value to zero"));

    return TRUE;
}

static void
level(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GwyDataField *mfield;
    LevelArgs args;
    gboolean ok;
    gdouble c, bx, by;
    GQuark quark;


    g_return_if_fail(run & LEVEL_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield && quark);

    if (run != GWY_RUN_IMMEDIATE && mfield) {
        level_load_args(gwy_app_settings_get(), &args);
        ok = level_dialog(&args);
        level_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
    if (!args.exclude)
        mfield = NULL;

    if (mfield) {
        /* need to invert mask, so it is inclusion mask */
        mfield = gwy_data_field_duplicate(mfield);
        gwy_data_field_multiply(mfield, -1.0);
        gwy_data_field_add(mfield, 1.0);
    }

    gwy_app_undo_qcheckpoint(data, quark, NULL);
    if (mfield)
        gwy_data_field_area_fit_plane(dfield, mfield, 0, 0,
                                      gwy_data_field_get_xres(dfield),
                                      gwy_data_field_get_yres(dfield),
                                      &c, &bx, &by);
    else
        gwy_data_field_fit_plane(dfield, &c, &bx, &by);
    c = -0.5*(bx*gwy_data_field_get_xres(dfield)
              + by*gwy_data_field_get_yres(dfield));
    gwy_data_field_plane_level(dfield, c, bx, by);
    gwy_data_field_data_changed(dfield);
    if (mfield)
        g_object_unref(mfield);
}

static void
level_rotate(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GwyDataField *mfield;
    LevelArgs args;
    gboolean ok;
    gdouble a, bx, by;
    GQuark quark;

    g_return_if_fail(run & LEVEL_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield && quark);

    if (run != GWY_RUN_IMMEDIATE && mfield) {
        level_load_args(gwy_app_settings_get(), &args);
        ok = level_dialog(&args);
        level_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
    if (!args.exclude)
        mfield = NULL;

    if (mfield) {
        /* need to invert mask, so it is inclusion mask */
        mfield = gwy_data_field_duplicate(mfield);
        gwy_data_field_multiply(mfield, -1.0);
        gwy_data_field_add(mfield, 1.0);
    }

    gwy_app_undo_qcheckpoint(data, quark, NULL);
    if (mfield)
        gwy_data_field_area_fit_plane(dfield, mfield, 0, 0,
                                      gwy_data_field_get_xres(dfield),
                                      gwy_data_field_get_yres(dfield),
                                      &a, &bx, &by);
    else
        gwy_data_field_fit_plane(dfield, &a, &bx, &by);
    bx = gwy_data_field_rtoj(dfield, bx);
    by = gwy_data_field_rtoi(dfield, by);
    gwy_data_field_plane_rotate(dfield, atan2(bx, 1), atan2(by, 1),
                                GWY_INTERPOLATION_BILINEAR);
    gwy_debug("b = %g, alpha = %g deg, c = %g, beta = %g deg",
              bx, 180/G_PI*atan2(bx, 1), by, 180/G_PI*atan2(by, 1));
    gwy_data_field_data_changed(dfield);
    if (mfield)
        g_object_unref(mfield);
}

static void
fix_zero(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GQuark quark;

    g_return_if_fail(run & LEVEL_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     0);
    g_return_if_fail(dfield && quark);
    gwy_app_undo_qcheckpoint(data, quark, NULL);
    gwy_data_field_add(dfield, -gwy_data_field_get_min(dfield));
    gwy_data_field_data_changed(dfield);
}

static void
zero_mean(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GQuark quark;

    g_return_if_fail(run & LEVEL_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     0);
    g_return_if_fail(dfield && quark);
    gwy_app_undo_qcheckpoint(data, quark, NULL);
    gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));
    gwy_data_field_data_changed(dfield);
}

static gboolean
level_dialog(LevelArgs *args)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *label, *table, *button;
    gint row, response;

    dialog = gtk_dialog_new_with_buttons(_("Level"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(12, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Exclude region under "
            "mask."));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), args->exclude);
    gtk_table_attach(GTK_TABLE(table), button,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(button, "toggled",
                     G_CALLBACK(exclude_changed_cb), args);

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
                //dialog_reset(&controls, args);
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
exclude_changed_cb(GtkWidget *toggle, LevelArgs *args)
{
    args->exclude = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
}

static const gchar exclude_key[] = "/module/level/exclude";

static void
level_load_args(GwyContainer *container, LevelArgs *args)
{
    *args = level_defaults;

    gwy_container_gis_boolean_by_name(container, exclude_key, &args->exclude);
}

static void
level_save_args(GwyContainer *container, LevelArgs *args)
{
    gwy_container_set_boolean_by_name(container, exclude_key, args->exclude);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
