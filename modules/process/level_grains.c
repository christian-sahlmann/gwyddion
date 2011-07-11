/*
 *  @(#) $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/arithmetic.h>
#include <libprocess/gwygrainvalue.h>
#include <libprocess/correct.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <app/gwyapp.h>

#define LEVEL_GRAINS_RUN_MODES (GWY_RUN_INTERACTIVE | GWY_RUN_IMMEDIATE)

typedef struct {
    GwyGrainQuantity base;
    gboolean do_extract;
} LevelGrainsArgs;

typedef struct {
    LevelGrainsArgs *args;
    GSList *base;
    GtkWidget *do_extract;
} LevelGrainsControls;

static gboolean module_register      (void);
static void     level_grains         (GwyContainer *data,
                                      GwyRunType run);
static void     level_grains_do      (const LevelGrainsArgs *args,
                                      GwyContainer *data,
                                      GQuark dquark,
                                      gint id,
                                      GwyDataField *dfield,
                                      GwyDataField *mfield);
static gboolean level_grains_dialog  (LevelGrainsArgs *args);
static GSList*  construct_bases_radio(GwyGrainQuantity current,
                                      GCallback callback,
                                      gpointer cbdata);
static void     base_changed         (GtkWidget *button,
                                      LevelGrainsControls *controls);
static void     do_extract_changed   (LevelGrainsControls *controls);
static void     load_args            (GwyContainer *container,
                                      LevelGrainsArgs *args);
static void     save_args            (GwyContainer *container,
                                      LevelGrainsArgs *args);

static GwyGrainQuantity level_grains_bases[] = {
    GWY_GRAIN_VALUE_MINIMUM,
    GWY_GRAIN_VALUE_MAXIMUM,
    GWY_GRAIN_VALUE_MEAN,
    GWY_GRAIN_VALUE_MEDIAN,
    GWY_GRAIN_VALUE_BOUNDARY_MINIMUM,
    GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM,
};

static LevelGrainsArgs level_grains_defaults = {
    GWY_GRAIN_VALUE_MINIMUM, FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Levels individual grains, interpolating the shifts between using "
       "Laplacian interpolation."),
    "David Nečas <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("level_grains",
                              (GwyProcessFunc)&level_grains,
                              N_("/_Grains/_Level Grains..."),
                              NULL,
                              LEVEL_GRAINS_RUN_MODES,
                              GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA,
                              N_("Level individual grains, interpolating the "
                                 "shifts between using Laplacian "
                                 "interpolation"));

    return TRUE;
}

static void
level_grains(GwyContainer *data,
             GwyRunType run)
{
    GwyDataField *dfield;
    GwyDataField *mfield;
    LevelGrainsArgs args;
    gboolean ok;
    GQuark quark;
    gint id;

    g_return_if_fail(run & LEVEL_GRAINS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield && quark);

    load_args(gwy_app_settings_get(), &args);
    if (run != GWY_RUN_IMMEDIATE) {
        ok = level_grains_dialog(&args);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    level_grains_do(&args, data, quark, id, dfield, mfield);
}

static void
level_grains_do(const LevelGrainsArgs *args,
                GwyContainer *data, GQuark dquark, gint id,
                GwyDataField *dfield, GwyDataField *mfield)
{
    GwyDataField *buffer, *background, *invmask;
    gdouble error, cor, maxerr, lastfrac, frac, starterr;
    gdouble *heights, *bgdata;
    gint *grains;
    gboolean cancelled = FALSE;
    gint i, xres, yres, ngrains;

    xres = gwy_data_field_get_xres(mfield);
    yres = gwy_data_field_get_yres(mfield);
    grains = g_new0(gint, xres*yres);

    ngrains = gwy_data_field_number_grains(mfield, grains);
    if (!ngrains) {
        g_free(grains);
        return;
    }

    heights = g_new(gdouble, ngrains+1);
    gwy_data_field_grains_get_values(dfield, heights, ngrains, grains,
                                     args->base);
    heights[0] = 0.0;

    background = gwy_data_field_new_alike(dfield, FALSE);
    bgdata = gwy_data_field_get_data(background);
    for (i = 0; i < xres*yres; i++)
        bgdata[i] = -heights[grains[i]];

    invmask = gwy_data_field_duplicate(mfield);
    gwy_data_field_multiply(invmask, -1.0);
    gwy_data_field_add(invmask, 1.0);

    maxerr = gwy_data_field_get_rms(dfield)/1.0e4;
    gwy_app_wait_start(gwy_app_find_window_for_channel(data, id),
                       _("Laplace interpolation..."));

    g_free(heights);
    g_free(grains);

    buffer = gwy_data_field_new_alike(background, TRUE);
    gwy_data_field_correct_average(background, invmask);

    cor = 0.2;
    error = 0.0;
    lastfrac = 0.0;
    starterr = 0.0;
    for (i = 0; i < 5000; i++) {
        gwy_data_field_correct_laplace_iteration(background, invmask, buffer,
                                                 cor, &error);
        if (error < maxerr)
            break;
        if (!i)
            starterr = error;

        frac = log(error/starterr)/log(maxerr/starterr);
        if ((i/(gdouble)(5000)) > frac)
            frac = i/(gdouble)(5000);
        if (lastfrac > frac)
            frac = lastfrac;

        if (!gwy_app_wait_set_fraction(frac)) {
            cancelled = TRUE;
            break;
        }
        lastfrac = frac;
    }
    gwy_app_wait_finish();

    if (!cancelled) {
        gwy_data_field_invert(background, FALSE, FALSE, TRUE);
        gwy_app_undo_qcheckpointv(data, 1, &dquark);
        gwy_data_field_subtract_fields(dfield, dfield, background);
        gwy_data_field_data_changed(dfield);

        if (args->do_extract) {
            gint newid;

            newid = gwy_app_data_browser_add_data_field(background, data, TRUE);
            gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                    GWY_DATA_ITEM_GRADIENT,
                                    0);
            gwy_app_set_data_field_title(data, newid, _("Background"));
        }
    }
    g_object_unref(buffer);
    g_object_unref(invmask);
    g_object_unref(background);
}

static gboolean
level_grains_dialog(LevelGrainsArgs *args)
{
    GtkWidget *dialog, *table, *label;
    LevelGrainsControls controls;
    gint response, row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Level Grains"),
                                         NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(2 + G_N_ELEMENTS(level_grains_bases), 1, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(_("Quantity to level:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.base = construct_bases_radio(args->base,
                                          G_CALLBACK(base_changed), &controls);
    row = gwy_radio_buttons_attach_to_table(controls.base, GTK_TABLE(table),
                                            1, row);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    controls.do_extract
        = gtk_check_button_new_with_mnemonic(_("E_xtract background"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_extract),
                                 args->do_extract);
    gtk_table_attach(GTK_TABLE(table), controls.do_extract,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.do_extract, "toggled",
                             G_CALLBACK(do_extract_changed), &controls);
    row++;

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

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    return TRUE;
}

static GSList*
construct_bases_radio(GwyGrainQuantity current,
                      GCallback callback, gpointer cbdata)
{
    enum { n = G_N_ELEMENTS(level_grains_bases) };
    GwyEnum entries[n];
    guint i;

    for (i = 0; i < n; i++) {
        GwyGrainQuantity q = level_grains_bases[i];
        GwyGrainValue *value = gwy_grain_values_get_builtin_grain_value(q);

        entries[i].value = q;
        entries[i].name = gwy_resource_get_name(GWY_RESOURCE(value));
    }

    return gwy_radio_buttons_create(entries, n, callback, cbdata, current);
}

static void
base_changed(G_GNUC_UNUSED GtkWidget *button,
             LevelGrainsControls *controls)
{
    controls->args->base = gwy_radio_buttons_get_current(controls->base);
}

static void
do_extract_changed(LevelGrainsControls *controls)
{
    controls->args->do_extract
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->do_extract));
}

static const gchar base_key[]       = "/module/level_grains/base";
static const gchar do_extract_key[] = "/module/level_grains/do_extract";

static void
sanitize_args(LevelGrainsArgs *args)
{
    guint i;

    args->do_extract = !!args->do_extract;
    for (i = 0; i < G_N_ELEMENTS(level_grains_bases); i++) {
        if (args->base == level_grains_bases[i])
            break;
    }
    if (i == G_N_ELEMENTS(level_grains_bases))
        args->base = level_grains_defaults.base;
}

static void
load_args(GwyContainer *container,
          LevelGrainsArgs *args)
{
    *args = level_grains_defaults;

    gwy_container_gis_enum_by_name(container, base_key, &args->base);
    gwy_container_gis_boolean_by_name(container, do_extract_key,
                                      &args->do_extract);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          LevelGrainsArgs *args)
{
    gwy_container_set_enum_by_name(container, base_key, args->base);
    gwy_container_set_boolean_by_name(container, do_extract_key,
                                      args->do_extract);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
