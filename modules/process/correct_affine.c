/*
 *  @(#) $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/inttrans.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/linestats.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define AFFINE_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400,
};

enum {
    RESPONSE_RESET = 1,
};

typedef struct {
    gboolean whatever;
} AffcorArgs;

typedef struct {
    AffcorArgs *args;
    GtkWidget *dialog;
    GtkWidget *view;
    GwySelection *selection;
    GwyContainer *mydata;
} AffcorControls;

static gboolean module_register     (void);
static void     correct_affine      (GwyContainer *data,
                                     GwyRunType run);
static void     affcor_dialog       (AffcorArgs *args,
                                     GwyContainer *data,
                                     GwyDataField *dfield,
                                     gint id);
static void     init_selection      (GwySelection *selection,
                                     GwyDataField *dfield);
static void     selection_changed   (AffcorControls *controls,
                                     gint hint);
static void     affcor_load_args    (GwyContainer *container,
                                     AffcorArgs *args);
static void     affcor_save_args    (GwyContainer *container,
                                     AffcorArgs *args);
static void     affcor_sanitize_args(AffcorArgs *args);

static const AffcorArgs affcor_defaults = {
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Corrects affine distortion of images by matching image Bravais "
       "lattice to the true one."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("correct_affine",
                              (GwyProcessFunc)&correct_affine,
                              N_("/_Correct Data/_Affine Distortion..."),
                              NULL,
                              AFFINE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Correct affine distortion"));

    return TRUE;
}

static void
correct_affine(GwyContainer *data, GwyRunType run)
{
    AffcorArgs args;
    GwyDataField *dfield;
    gint id;

    g_return_if_fail(run & AFFINE_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerLattice"));
    affcor_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    affcor_dialog(&args, data, dfield, id);
    affcor_save_args(gwy_app_settings_get(), &args);
}

static void
affcor_dialog(AffcorArgs *args,
              GwyContainer *data,
              GwyDataField *dfield,
              gint id)
{
    GtkWidget *hbox, *hbox2, *vbox, *label;
    GtkDialog *dialog;
    AffcorControls controls;
    gint response;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer;

    controls.args = args;

    controls.dialog = gtk_dialog_new_with_buttons(_("Affine Correction"),
                                                  NULL, 0,
                                                  _("_Reset"),
                                                  RESPONSE_RESET,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_CANCEL,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_OK,
                                                  NULL);
    dialog = GTK_DIALOG(controls.dialog);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
    gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_OK, FALSE);

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data",
                                     dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    vlayer = g_object_new(g_type_from_name("GwyLayerLattice"),
                          "selection-key", "/0/select/vector",
                          NULL);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view), vlayer);
    controls.selection = gwy_vector_layer_ensure_selection(vlayer);
    gwy_selection_set_max_objects(controls.selection, 1);
    init_selection(controls.selection, dfield);
    g_signal_connect_swapped(controls.selection, "changed",
                             G_CALLBACK(selection_changed), &controls);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);



    gtk_widget_show_all(controls.dialog);
    do {
        response = gtk_dialog_run(dialog);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(controls.dialog);
            case GTK_RESPONSE_NONE:
            goto finalize;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            /* TODO gwy_selection_clear(controls.selection); */
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(controls.dialog);
finalize:
    g_object_unref(controls.mydata);
}

static void
init_selection(GwySelection *selection,
               GwyDataField *dfield)
{
    gdouble xy[4] = { 0.0, 0.0, 0.0, 0.0 };

    xy[0] = dfield->xreal/20;
    xy[3] = dfield->yreal/20;
    gwy_selection_set_data(selection, 1, xy);
}

static void
selection_changed(AffcorControls *controls,
                  gint hint)
{
}

//static const gchar separate_key[]      = "/module/fft_profile/separate";

static void
affcor_sanitize_args(AffcorArgs *args)
{
}

static void
affcor_load_args(GwyContainer *container,
                 AffcorArgs *args)
{
    *args = affcor_defaults;

    affcor_sanitize_args(args);
}

static void
affcor_save_args(GwyContainer *container,
                 AffcorArgs *args)
{
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
