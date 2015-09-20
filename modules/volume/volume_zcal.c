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
#include <libprocess/brick.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-volume.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define ZCAL_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 360,
};

typedef enum {
    ZCAL_EXTRACT = 0,
    ZCAL_REMOVE  = 1,
    ZCAL_ATTACH  = 2,
    NACTIONS
} ZCalActionType;

typedef struct {
    ZCalActionType mode;
    gchar *filename;
    /* Dynamic state. */
    GwyBrick *brick;
    GwyDataLine *calibration;
} ZCalArgs;

typedef struct {
    ZCalArgs *args;
    GtkWidget *dialog;
    GSList *mode;
    GtkWidget *filebutton;
    GtkWidget *graph;
} ZCalControls;

static gboolean module_register             (void);
static void     zcal                        (GwyContainer *data,
                                             GwyRunType run);
static gboolean zcal_dialog                 (ZCalArgs *args);
static void     mode_changed                (GtkWidget *button,
                                             ZCalControls *controls);
static void     setup_graph_from_calibration(GwyGraphModel *gmodel,
                                             GwyDataLine *calibration);
static void     zcal_do                     (ZCalArgs *args);
static void     zcal_sanitize_args          (ZCalArgs *args);
static void     zcal_load_args              (GwyContainer *container,
                                             ZCalArgs *args);
static void     zcal_save_args              (GwyContainer *container,
                                             ZCalArgs *args);

static const ZCalArgs zcal_defaults = {
    ZCAL_ATTACH, NULL,
    /* Dynamic state. */
    NULL, NULL
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Attaches, extracts and removes volume data z-axis calibration."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_volume_func_register("volume_zcal",
                             (GwyVolumeFunc)&zcal,
                             N_("/_Z-Calibration..."),
                             NULL,
                             ZCAL_RUN_MODES,
                             GWY_MENU_FLAG_VOLUME,
                             N_("Manage z-axis calibration"));

    return TRUE;
}

static void
zcal(G_GNUC_UNUSED GwyContainer *data, GwyRunType run)
{
    ZCalArgs args;
    GwyBrick *brick = NULL;
    gboolean ok = TRUE, mode_needs_zcal;

    g_return_if_fail(run & ZCAL_RUN_MODES);

    zcal_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_BRICK, &brick, 0);
    g_return_if_fail(GWY_IS_BRICK(brick));
    args.brick = brick;
    args.calibration = gwy_brick_get_zcalibration(brick);

    mode_needs_zcal = (args.mode == ZCAL_REMOVE || args.mode == ZCAL_EXTRACT);

    if (run == GWY_RUN_INTERACTIVE) {
        if (mode_needs_zcal && !args.calibration)
            args.mode = ZCAL_ATTACH;
        ok = zcal_dialog(&args);
        zcal_save_args(gwy_app_settings_get(), &args);
    }
    else {
        /* Carefully avoid actions that may not be possible now. */
        if (mode_needs_zcal && !args.calibration)
            ok = FALSE;
    }

    /* For ZCAL_ATTACH, this can still fail if the file is not available or
     * compatible with the brick: zcal_do() must deal with it. */
    if (ok)
        zcal_do(&args);

    g_free(args.filename);
}

static gboolean
zcal_dialog(ZCalArgs *args)
{
    static const GwyEnum modes[] = {
        { N_("_Extract to a graph"), ZCAL_EXTRACT, },
        { N_("_Remove"),             ZCAL_REMOVE,  },
        { N_("_Attach from file"),   ZCAL_ATTACH,  },
    };

    GtkWidget *dialog, *table, *hbox, *label;
    ZCalControls controls;
    GwyGraphModel *gmodel;
    gint response, row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Volume Z Calibration"),
                                         NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_volume_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    table = gtk_table_new(5, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(_("Z-calibration action:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.mode
        = gwy_radio_buttons_create(modes, G_N_ELEMENTS(modes),
                                   G_CALLBACK(mode_changed), &controls,
                                   args->mode);
    row = gwy_radio_buttons_attach_to_table(controls.mode,
                                            GTK_TABLE(table), 2, row);
    if (!args->calibration) {
        gtk_widget_set_sensitive(gwy_radio_buttons_find(controls.mode,
                                                        ZCAL_EXTRACT), FALSE);
        gtk_widget_set_sensitive(gwy_radio_buttons_find(controls.mode,
                                                        ZCAL_REMOVE), FALSE);
    }

    controls.filebutton
        = gtk_file_chooser_button_new(_("Volume Z Calibration"),
                                      GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_table_attach(GTK_TABLE(table), controls.filebutton,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    gmodel = gwy_graph_model_new();
    g_object_set(gmodel,
                 "axis-label-bottom", _("Index"),
                 "axis-label-left", _("Z axis value"),
                 NULL);
    setup_graph_from_calibration(gmodel, args->calibration);
    controls.graph = gwy_graph_new(gmodel);
    g_object_unref(gmodel);
    gwy_graph_enable_user_input(GWY_GRAPH(controls.graph), FALSE);
    gtk_widget_set_size_request(controls.graph, PREVIEW_SIZE, PREVIEW_SIZE);
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, TRUE, TRUE, 0);

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

static void
mode_changed(GtkWidget *button, ZCalControls *controls)
{
    controls->args->mode = gwy_radio_button_get_value(button);
    /* TODO: We might want to manage the file chooser sensitivity? */
}

static void
setup_graph_from_calibration(GwyGraphModel *gmodel,
                             GwyDataLine *calibration)
{
    GwyGraphCurveModel *gcmodel;
    const gdouble *ydata;
    GwySIUnit *zunit;
    gdouble *xdata;
    gint res, i;

    if (!calibration) {
        gwy_graph_model_remove_all_curves(gmodel);
        return;
    }

    if (!gwy_graph_model_get_n_curves(gmodel)) {
        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }
    else
        gcmodel = gwy_graph_model_get_curve(gmodel, 0);

    res = gwy_data_line_get_res(calibration);
    ydata = gwy_data_line_get_data_const(calibration);
    xdata = g_new(gdouble, res);
    for (i = 0; i < res; i++)
        xdata[i] = i;
    gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, res);
    g_free(xdata);

    zunit = gwy_data_line_get_si_unit_y(calibration);
    zunit = gwy_si_unit_duplicate(zunit);
    g_object_set(gmodel, "si-unit-x", zunit, NULL);
    g_object_unref(zunit);
}

static void
zcal_do(ZCalArgs *args)
{
    /* Run gwy_brick_data_changed() when we attach the calibration. */
}

static const gchar filename_key[] = "/module/volume_zcal/filename";
static const gchar mode_key[]     = "/module/volume_zcal/mode";

static void
zcal_sanitize_args(ZCalArgs *args)
{
    args->mode = MIN(args->mode, NACTIONS-1);
    args->filename = g_strdup(args->filename);
}

static void
zcal_load_args(GwyContainer *container,
                ZCalArgs *args)
{
    *args = zcal_defaults;

    gwy_container_gis_enum_by_name(container, mode_key, &args->mode);
    gwy_container_gis_string_by_name(container, filename_key,
                                     (const guchar**)&args->filename);
    zcal_sanitize_args(args);
}

static void
zcal_save_args(GwyContainer *container,
                ZCalArgs *args)
{
    gwy_container_set_enum_by_name(container, mode_key, args->mode);
    if (args->filename)
        gwy_container_set_const_string_by_name(container, filename_key,
                                               args->filename);
    else
        gwy_container_remove_by_name(container, filename_key);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
