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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/level.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define LEVEL_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    GwyMaskingType masking;
} LevelArgs;

typedef struct {
    LevelArgs *args;
    GSList *masking;
} LevelControls;

static gboolean module_register   (void);
static void     facet_level       (GwyContainer *data,
                                   GwyRunType run);
static gboolean level_dialog      (LevelArgs *args,
                                   const gchar *title);
static void     masking_changed   (GtkToggleButton *button,
                                   LevelControls *controls);
static void     level_load_args   (GwyContainer *container,
                                   LevelArgs *args);
static void     level_save_args   (GwyContainer *container,
                                   LevelArgs *args);

static const LevelArgs level_defaults = {
    GWY_MASK_EXCLUDE
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Automatic facet-orientation based levelling. "
       "Levels data to make facets point up."),
    "Yeti <yeti@gwyddion.net>",
    "2.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("facet-level",
                              (GwyProcessFunc)&facet_level,
                              N_("/_Level/_Facet Level"),
                              GWY_STOCK_FACET_LEVEL,
                              LEVEL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Level data to make facets point upward"));

    return TRUE;
}

static void
facet_level(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield, *old;
    LevelArgs args;
    GQuark quark;
    gdouble c, bx, by, b2;
    gdouble p, progress, maxb2 = 666.0, eps = 1e-8;
    gint i, id;
    gboolean ok;
    gboolean cancelled = FALSE;

    g_return_if_fail(run & LEVEL_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && quark);

    if (!gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(dfield),
                           gwy_data_field_get_si_unit_z(dfield))) {
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new
                        (gwy_app_find_window_for_channel(data, id),
                         GTK_DIALOG_DESTROY_WITH_PARENT,
                         GTK_MESSAGE_ERROR,
                         GTK_BUTTONS_OK,
                         _("Facet level: Lateral dimensions and value must "
                           "be the same physical quantity."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    old = dfield;
    dfield = gwy_data_field_duplicate(dfield);

    level_load_args(gwy_app_settings_get(), &args);
    if (run != GWY_RUN_IMMEDIATE && mfield) {
        ok = level_dialog(&args, _("Facet Level"));
        level_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
    if (!mfield)
        args.masking = GWY_MASK_IGNORE;

    /* converge
     * FIXME: this can take a long time */
    i = 0;
    progress = 0.0;
    gwy_app_wait_start(gwy_app_find_window_for_channel(data, id),
                       _("Facet-leveling"));
    while (i < 100) {
        if (!gwy_data_field_fit_facet_plane(dfield, mfield, args.masking,
                                            &c, &bx, &by)) {
            /* Not actually cancelled, but do not save undo */
            cancelled = TRUE;
            break;
        }
        gwy_data_field_plane_level(dfield, c, bx, by);
        bx /= gwy_data_field_get_xmeasure(dfield);
        by /= gwy_data_field_get_ymeasure(dfield);
        b2 = bx*bx + by*by;
        if (!i)
            maxb2 = MAX(b2, eps);
        if (b2 < eps)
            break;
        i++;
        p = log(b2/maxb2)/log(eps/maxb2);
        gwy_debug("progress = %f, p = %f, ip = %f", progress, p, i/100.0);
        /* never decrease progress, that would look silly */
        progress = MAX(progress, p);
        progress = MAX(progress, i/100.0);
        if (!gwy_app_wait_set_fraction(progress)) {
            cancelled = TRUE;
            break;
        }
    };
    gwy_app_wait_finish();
    if (!cancelled) {
        gwy_app_undo_qcheckpointv(data, 1, &quark);
        gwy_data_field_copy(dfield, old, FALSE);
        gwy_app_channel_log_add_proc(data, id, id);
        gwy_data_field_data_changed(old);
    }
    g_object_unref(dfield);
}

static gboolean
level_dialog(LevelArgs *args,
             const gchar *title)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *label, *table;
    gint row, response;
    LevelControls controls;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(title, NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(12, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    row = 0;

    label = gwy_label_new_header(_("Masking Mode"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.masking = gwy_radio_buttons_create(gwy_masking_type_get_enum(), -1,
                                                G_CALLBACK(masking_changed),
                                                &controls, args->masking);
    row = gwy_radio_buttons_attach_to_table(controls.masking, GTK_TABLE(table),
                                            3, row);

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
            *args = level_defaults;
            gwy_radio_buttons_set_current(controls.masking, args->masking);
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
masking_changed(GtkToggleButton *button, LevelControls *controls)
{
    if (!gtk_toggle_button_get_active(button))
        return;

    controls->args->masking = gwy_radio_buttons_get_current(controls->masking);
}

static const gchar masking_key[] = "/module/facet-level/mode";

static void
level_load_args(GwyContainer *container, LevelArgs *args)
{
    *args = level_defaults;

    gwy_container_gis_enum_by_name(container, masking_key,
                                   &args->masking);
}

static void
level_save_args(GwyContainer *container, LevelArgs *args)
{
    gwy_container_set_enum_by_name(container, masking_key,
                                   args->masking);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
