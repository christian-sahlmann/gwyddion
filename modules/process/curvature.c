/*
 *  @(#) $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.net.
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

#define CURVATURE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 320,
};

typedef struct {
    gboolean set_selection;
    gboolean plot_graph;
    GwyMaskingType masking;
} CurvatureArgs;

typedef struct {
    CurvatureArgs *args;
    GSList *masking_group;
    GtkWidget *set_selection;
    GtkWidget *plot_graph;
    GtkWidget *view;
    GtkWidget *graph;
    GtkWidget *params;
    GwyGraphModel *gmodel;
    GwySelection *selection;
    GwyContainer *data;
} CurvatureControls;

static gboolean module_register                (void);
static void     curvature                      (GwyContainer *data,
                                                GwyRunType run);
static void     curvature_do                   (GwyContainer *data,
                                                GwyDataField *dfield,
                                                GwyDataField *mfield,
                                                gint oldid,
                                                const CurvatureArgs *args);
static gboolean curvature_dialog               (CurvatureArgs *args,
                                                GwyContainer *data,
                                                GwyDataField *dfield,
                                                GwyDataField *mfield,
                                                gint id);
static void     curvature_set_selection_changed(GtkToggleButton *button,
                                                CurvatureControls *controls);
static void     curvature_plot_graph_changed   (GtkToggleButton *button,
                                                CurvatureControls *controls);
static void     curvature_dialog_update        (CurvatureControls *controls,
                                                CurvatureArgs *args);
static void     curvature_masking_changed      (GtkToggleButton *button,
                                                CurvatureControls *controls);
static void     curvature_update_preview       (CurvatureControls *controls,
                                                CurvatureArgs *args);
static void     load_args                      (GwyContainer *container,
                                                CurvatureArgs *args);
static void     save_args                      (GwyContainer *container,
                                                CurvatureArgs *args);
static void     sanitize_args                  (CurvatureArgs *args);

static const CurvatureArgs curvature_defaults = {
    TRUE,
    FALSE,
    GWY_MASK_IGNORE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates overall curvature."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("curvature",
                              (GwyProcessFunc)&curvature,
                              N_("/_Level/_Curvature..."),
                              NULL,
                              CURVATURE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Calculate overall curvature"));

    return TRUE;
}

static void
curvature(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield;
    CurvatureArgs args;
    gboolean ok;
    gint id;

    g_return_if_fail(run & CURVATURE_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerPoint"));
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = curvature_dialog(&args, data, dfield, mfield, id);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
    curvature_do(data, dfield, mfield, id, &args);
}

static void
curvature_calculate(GwyDataField *dfield,
                    GwyDataField *mask,
                    const CurvatureArgs *args,
                    GwySelection *selection,
                    GwyGraphModel *gmodel)
{
    enum { DEGREE = 2 };
    gint *term_powers;
    gdouble *coeffs;
    gint nterms, i, j, k;

    k = 0;
    nterms = (DEGREE + 1)*(DEGREE + 2)/2;
    term_powers = g_new(gint, 2*nterms);
    for (i = 0; i <= DEGREE; i++) {
        for (j = 0; j <= DEGREE - i; j++) {
            term_powers[k++] = i;
            term_powers[k++] = j;
        }
    }

    coeffs = gwy_data_field_fit_poly(dfield, mask, nterms, term_powers,
                                     args->masking == GWY_MASK_EXCLUDE, NULL);

    g_free(coeffs);
    g_free(term_powers);
}

static void
curvature_do(GwyContainer *data,
             GwyDataField *dfield,
             GwyDataField *mfield,
             gint oldid,
             const CurvatureArgs *args)
{
    gint newid;

    curvature_calculate(dfield, mfield, args, NULL, NULL);

    if (args->set_selection) {
    }

    if (args->plot_graph) {
        //newid = gwy_app_data_browser_add_graph_model(bg, data, TRUE);
        //g_object_unref(bg);
    }
}

static gboolean
curvature_dialog(CurvatureArgs *args,
                 GwyContainer *data,
                 GwyDataField *dfield,
                 GwyDataField *mfield,
                 gint id)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *label, *hbox, *vbox, *scwin;
    GwyPixmapLayer *player;
    GwyVectorLayer *vlayer;
    CurvatureControls controls;
    gint response;
    gint row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Curvature"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    controls.data = gwy_container_new();
    gwy_container_set_object_by_name(controls.data, "/0/data", dfield);
    if (mfield)
        gwy_container_set_object_by_name(controls.data, "/0/mask", mfield);
    gwy_app_sync_data_items(data, controls.data, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    controls.view = gwy_data_view_new(controls.data);
    player = gwy_layer_basic_new();
    g_object_set(player,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), player);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    vlayer = g_object_new(g_type_from_name("GwyLayerLine"), NULL);
    gwy_vector_layer_set_selection_key(vlayer, "/0/select/pointer");
    gwy_vector_layer_set_editable(vlayer, FALSE);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view),
                                GWY_VECTOR_LAYER(vlayer));
    controls.selection = gwy_vector_layer_ensure_selection(vlayer);
    g_object_set(controls.selection, "max-objects", 2, NULL);

    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 4);

    controls.gmodel = gwy_graph_model_new();
    controls.graph = gwy_graph_new(controls.gmodel);
    g_object_unref(controls.gmodel);

    gtk_box_pack_start(GTK_BOX(vbox), controls.graph, FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scwin, TRUE, TRUE, 4);

    controls.params = gtk_tree_view_new();
    gtk_container_add(GTK_CONTAINER(scwin), controls.params);

    table = gtk_table_new(2 + (mfield ? 4 : 0), 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    controls.set_selection
        = gtk_check_button_new_with_mnemonic(_("_Set selection"));
    gtk_table_attach(GTK_TABLE(table), controls.set_selection,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.set_selection),
                                 args->set_selection);
    g_signal_connect(controls.set_selection, "toggled",
                     G_CALLBACK(curvature_set_selection_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.plot_graph
        = gtk_check_button_new_with_mnemonic(_("_Plot graph"));
    gtk_table_attach(GTK_TABLE(table), controls.plot_graph,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.plot_graph),
                                 args->plot_graph);
    g_signal_connect(controls.plot_graph, "toggled",
                     G_CALLBACK(curvature_plot_graph_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    if (mfield) {
        label = gwy_label_new_header(_("Masking Mode"));
        gtk_table_attach(GTK_TABLE(table), label,
                        0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;

        controls.masking_group
            = gwy_radio_buttons_createl(G_CALLBACK(curvature_masking_changed),
                                        &controls, args->masking,
                                        _("_Exclude region under mask"),
                                        GWY_MASK_EXCLUDE,
                                        _("Exclude region _outside mask"),
                                        GWY_MASK_INCLUDE,
                                        _("Use entire _image (ignore mask)"),
                                        GWY_MASK_IGNORE,
                                        NULL);
        row = gwy_radio_buttons_attach_to_table(controls.masking_group,
                                                GTK_TABLE(table), 3, row);
    }
    else
        controls.masking_group = NULL;

    curvature_update_preview(&controls, args);

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
            *args = curvature_defaults;
            curvature_dialog_update(&controls, args);
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
curvature_dialog_update(CurvatureControls *controls,
                         CurvatureArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->set_selection),
                                 args->set_selection);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->plot_graph),
                                 args->plot_graph);
    if (controls->masking_group)
        gwy_radio_buttons_set_current(controls->masking_group, args->masking);
}

static void
curvature_set_selection_changed(GtkToggleButton *button,
                                CurvatureControls *controls)
{
    CurvatureArgs *args = controls->args;

    args->set_selection = gtk_toggle_button_get_active(button);
}

static void
curvature_plot_graph_changed(GtkToggleButton *button,
                             CurvatureControls *controls)
{
    CurvatureArgs *args = controls->args;

    args->plot_graph = gtk_toggle_button_get_active(button);
}

static void
curvature_masking_changed(GtkToggleButton *button,
                          CurvatureControls *controls)
{
    CurvatureArgs *args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args = controls->args;
    args->masking = gwy_radio_buttons_get_current(controls->masking_group);
    curvature_update_preview(controls, args);
}

static void
curvature_update_preview(CurvatureControls *controls,
                         CurvatureArgs *args)
{
    GwyDataField *source, *mask = NULL;

    gwy_container_gis_object_by_name(controls->data, "/0/data", &source);
    gwy_container_gis_object_by_name(controls->data, "/0/mask", &mask);

    /*

    if (mask && args->masking != GWY_MASK_IGNORE)
        curvature_do_with_mask(source, mask, leveled, bg, args);
    else if (args->independent)
        curvature_do_independent(source, leveled, bg,
                                  args->col_degree, args->row_degree);
    else
        curvature_do_maximum(source, leveled, bg, args->max_degree);
        */
}

static const gchar set_selection_key[] = "/module/curvature/set_selection";
static const gchar plot_graph_key[]    = "/module/curvature/plot_graph";
static const gchar masking_key[]       = "/module/curvature/masking";

static void
sanitize_args(CurvatureArgs *args)
{
    args->masking = MIN(args->masking, GWY_MASK_INCLUDE);
    args->set_selection = !!args->set_selection;
    args->plot_graph = !!args->plot_graph;
}

static void
load_args(GwyContainer *container,
          CurvatureArgs *args)
{
    *args = curvature_defaults;

    gwy_container_gis_enum_by_name(container, masking_key, &args->masking);
    gwy_container_gis_boolean_by_name(container, set_selection_key,
                                      &args->set_selection);
    gwy_container_gis_boolean_by_name(container, plot_graph_key,
                                      &args->plot_graph);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          CurvatureArgs *args)
{
    gwy_container_set_enum_by_name(container, masking_key, args->masking);
    gwy_container_set_boolean_by_name(container, set_selection_key,
                                      args->set_selection);
    gwy_container_set_boolean_by_name(container, plot_graph_key,
                                      args->plot_graph);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
