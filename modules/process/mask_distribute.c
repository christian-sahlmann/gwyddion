/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define MASKDIST_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    DISTRIBUTE_WITHIN_FILE = 0,
    DISTRIBUTE_TO_ALL_FILES = 1,
    DISTRIBUTE_NMODES
} MaskDistribMode;

typedef struct {
    MaskDistribMode mode;
    gboolean keep_existing;
} MaskDistribArgs;

typedef struct {
    MaskDistribArgs *args;
    GSList *mode;
    GtkWidget *keep_existing;
} MaskDistribControls;

typedef struct {
    GwyContainer *container;
    GwyDataField *mfield;
    gint id;
    const MaskDistribArgs *args;
    GArray *undo_quarks;
} MaskDistribData;

static gboolean module_register      (void);
static void     mask_distribute      (GwyContainer *data,
                                      GwyRunType run);
static gboolean mask_distrib_dialog  (MaskDistribArgs *args);
static void     mode_changed         (GtkToggleButton *button,
                                      MaskDistribControls *controls);
static void     keep_existing_changed(GtkToggleButton *button,
                                      MaskDistribControls *controls);
static void     mask_distrib_do      (GwyContainer *data,
                                      gint id,
                                      GwyDataField *mfield,
                                      MaskDistribArgs *args);
static void     distribute_in_one    (GwyContainer *container,
                                      MaskDistribData *distdata);
static void     load_args            (GwyContainer *container,
                                      MaskDistribArgs *args);
static void     save_args            (GwyContainer *container,
                                      MaskDistribArgs *args);

static const MaskDistribArgs mask_distrib_defaults = {
    DISTRIBUTE_WITHIN_FILE,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Distributes masks to other channels."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("mask_distribute",
                              (GwyProcessFunc)&mask_distribute,
                              N_("/_Mask/_Distribute..."),
                              NULL,
                              MASKDIST_RUN_MODES,
                              GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA,
                              N_("Distribute mask to other channels"));

    return TRUE;
}

static void
mask_distribute(GwyContainer *data, GwyRunType run)
{
    MaskDistribArgs args;
    GwyContainer *settings;
    GwyDataField *mfield;
    gboolean ok;
    gint id;

    g_return_if_fail(run & MASKDIST_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(mfield && id >= 0);

    settings = gwy_app_settings_get();
    load_args(settings, &args);
    if (run != GWY_RUN_IMMEDIATE) {
        ok = mask_distrib_dialog(&args);
        save_args(settings, &args);
        if (!ok)
            return;
    }

    mask_distrib_do(data, id, mfield, &args);
}

static gboolean
mask_distrib_dialog(MaskDistribArgs *args)
{
    enum { RESPONSE_RESET = 1 };

    static const GwyEnum modes[] = {
        { N_("Channels within the file"), DISTRIBUTE_WITHIN_FILE,  },
        { N_("Channels in all files"),    DISTRIBUTE_TO_ALL_FILES, },
    };

    GtkWidget *dialog, *label, *table;
    gint row, response;
    MaskDistribControls controls;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Distribute Mask"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = gtk_table_new(4, 1, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    row = 0;

    label = gwy_label_new_header(_("Distribute to:"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.mode = gwy_radio_buttons_create(modes, G_N_ELEMENTS(modes),
                                             G_CALLBACK(mode_changed),
                                             &controls, args->mode);
    row = gwy_radio_buttons_attach_to_table(controls.mode, GTK_TABLE(table),
                                            1, row);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    controls.keep_existing
        = gtk_check_button_new_with_mnemonic(_("Preserve existing masks"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.keep_existing),
                                 args->keep_existing);
    gtk_table_attach(GTK_TABLE(table), controls.keep_existing,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.keep_existing, "toggled",
                     G_CALLBACK(keep_existing_changed), &controls);
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

            case RESPONSE_RESET:
            *args = mask_distrib_defaults;
            gwy_radio_buttons_set_current(controls.mode, args->mode);
            gtk_toggle_button_set_active
                                    (GTK_TOGGLE_BUTTON(controls.keep_existing),
                                     args->keep_existing);
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
mode_changed(GtkToggleButton *button, MaskDistribControls *controls)
{
    MaskDistribArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args->mode = gwy_radio_buttons_get_current(controls->mode);
}

static void
keep_existing_changed(GtkToggleButton *button, MaskDistribControls *controls)
{
    MaskDistribArgs *args = controls->args;
    args->keep_existing = gtk_toggle_button_get_active(button);
}

static void
mask_distrib_do(GwyContainer *data, gint id,
                GwyDataField *mfield,
                MaskDistribArgs *args)
{
    MaskDistribData distdata;

    distdata.container = data;
    distdata.id = id;
    distdata.mfield = mfield;
    distdata.args = args;
    distdata.undo_quarks = g_array_new(FALSE, FALSE, sizeof(GQuark));

    if (args->mode == DISTRIBUTE_TO_ALL_FILES)
        gwy_app_data_browser_foreach((GwyAppDataForeachFunc)distribute_in_one,
                                     &distdata);
    else
        distribute_in_one(data, &distdata);

    g_array_free(distdata.undo_quarks, TRUE);
}

static void
distribute_in_one(GwyContainer *container, MaskDistribData *distdata)
{
    GwyDataField *dfield, *mfield;
    GQuark quark;
    gint *ids;
    guint i;

    g_array_set_size(distdata->undo_quarks, 0);
    ids = gwy_app_data_browser_get_data_ids(container);

    for (i = 0; ids[i] >= 0; i++) {
        if (container == distdata->container && ids[i] == distdata->id)
            continue;

        quark = gwy_app_get_data_key_for_id(ids[i]);
        dfield = gwy_container_get_object(container, quark);
        g_return_if_fail(dfield);

        quark = gwy_app_get_mask_key_for_id(ids[i]);
        if (!gwy_container_gis_object(container, quark, &mfield))
            mfield = NULL;

        if (mfield && distdata->args->keep_existing)
            continue;

        if (gwy_data_field_check_compatibility(distdata->mfield, dfield,
                                               GWY_DATA_COMPATIBILITY_RES
                                               | GWY_DATA_COMPATIBILITY_REAL))
            continue;

        g_array_append_val(distdata->undo_quarks, quark);
    }
    g_free(ids);

    if (!distdata->undo_quarks->len)
        return;

    gwy_app_undo_qcheckpointv(container,
                              distdata->undo_quarks->len,
                              (GQuark*)distdata->undo_quarks->data);
    for (i = 0; i < distdata->undo_quarks->len; i++) {
        quark = g_array_index(distdata->undo_quarks, GQuark, i);
        mfield = gwy_data_field_duplicate(distdata->mfield);
        gwy_container_set_object(container, quark, mfield);
        g_object_unref(mfield);
    }
}

static const gchar keep_existing_key[] = "/module/mask_distribute/keep_existing";
static const gchar mode_key[]          = "/module/mask_distribute/mode";

static void
sanitize_args(MaskDistribArgs *args)
{
    args->keep_existing = !!args->keep_existing;
    args->mode = MIN(args->mode, DISTRIBUTE_NMODES-1);
}

static void
load_args(GwyContainer *container, MaskDistribArgs *args)
{
    *args = mask_distrib_defaults;

    gwy_container_gis_enum_by_name(container, mode_key, &args->mode);
    gwy_container_gis_boolean_by_name(container, keep_existing_key,
                                      &args->keep_existing);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container, MaskDistribArgs *args)
{
    gwy_container_set_enum_by_name(container, mode_key, args->mode);
    gwy_container_set_boolean_by_name(container, keep_existing_key,
                                      args->keep_existing);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
