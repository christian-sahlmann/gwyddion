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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/inttrans.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define PROF_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400,
    NLINES = 18,
    MIN_RESOLUTION = 4,
    MAX_RESOLUTION = 16384
};

enum {
    RESPONSE_CLEAR = 1,
};

typedef struct {
    gboolean separate;
    gboolean fixres;
    gint resolution;
    GwyInterpolationType interpolation;
    GwyWindowingType windowing;
} ProfArgs;

typedef struct {
    ProfArgs *args;
    GtkWidget *dialog;
    GtkWidget *ok;
    GtkWidget *view;
    GwyDataField *fftfield;
    GwySelection *selection;
    GtkWidget *graph;
    GwyDataLine *line;
    GwyGraphModel *gmodel;
    GtkWidget *separate;
    GwyContainer *mydata;
    gboolean in_init;
} ProfControls;

static gboolean      module_register            (void);
static void          fft_profile                (GwyContainer *data,
                                                 GwyRunType run);
static void          prof_dialog                (ProfArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 gint id);
static void          prof_dialog_update_controls(ProfControls *controls,
                                                 ProfArgs *args);
static void          prof_dialog_update_values  (ProfControls *controls,
                                                 ProfArgs *args);
static void          prof_separate_changed      (ProfControls *controls);
static void          prof_selection_changed     (ProfControls *controls,
                                                 gint hint);
static void          prof_update_curve          (ProfControls *controls,
                                                 gint i);
static GwyDataField* prof_fft_modulus           (GwyDataField *dfield,
                                                 const ProfArgs *args);
static void          prof_load_args             (GwyContainer *container,
                                                 ProfArgs *args);
static void          prof_save_args             (GwyContainer *container,
                                                 ProfArgs *args);
static void          prof_sanitize_args         (ProfArgs *args);


static const ProfArgs prof_defaults = {
    FALSE, FALSE, 120,
    GWY_INTERPOLATION_LINEAR,
    GWY_WINDOWING_HANN,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Reads radial profiles of two-dimensional Fourier coefficients."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fft_profile",
                              (GwyProcessFunc)&fft_profile,
                              N_("/_Statistics/_FFT Profile..."),
                              GWY_STOCK_GRAPH_HALFGAUSS,
                              PROF_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Read radial FFT modulus profiles"));

    return TRUE;
}

static void
fft_profile(GwyContainer *data, GwyRunType run)
{
    ProfArgs args;
    GwyDataField *dfield;
    gint id;

    g_return_if_fail(run & PROF_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerPoint"));
    prof_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    prof_dialog(&args, data, dfield, id);
    prof_save_args(gwy_app_settings_get(), &args);
}

static void
prof_dialog(ProfArgs *args,
            GwyContainer *data,
            GwyDataField *dfield,
            gint id)
{
    GtkWidget *dialog, *table, *hbox, *vbox, *label, *pivot;
    ProfControls controls;
    gint response;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer;
    gint row;

    controls.args = args;
    controls.in_init = TRUE;
    controls.fftfield = prof_fft_modulus(dfield, args);

    dialog = gtk_dialog_new_with_buttons(_("Radial FFT Profile"), NULL, 0,
                                         NULL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLEAR, RESPONSE_CLEAR);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    controls.ok = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                        GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data",
                                     controls.fftfield);
    g_object_unref(controls.fftfield);
    gwy_container_set_string_by_name(controls.mydata, "/0/base/palette",
                                     g_strdup("DFit"));
    gwy_container_set_enum_by_name(controls.mydata, "/0/base/range-type",
                                   GWY_LAYER_BASIC_RANGE_AUTO);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
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

    vlayer = g_object_new(g_type_from_name("GwyLayerPoint"),
                          "draw-as-vector", TRUE,
                          "selection-key", "/0/select/vector",
                          NULL);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view), vlayer);
    controls.selection = gwy_vector_layer_ensure_selection(vlayer);
    gwy_selection_set_max_objects(controls.selection, NLINES);
    g_signal_connect_swapped(controls.selection, "changed",
                             G_CALLBACK(prof_selection_changed), &controls);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 4);

    controls.gmodel = gwy_graph_model_new();
    controls.line = gwy_data_line_new(1, 1.0, FALSE);

    controls.graph = gwy_graph_new(controls.gmodel);
    gwy_graph_set_axis_visible(GWY_GRAPH(controls.graph), GTK_POS_LEFT, FALSE);
    gwy_graph_set_axis_visible(GWY_GRAPH(controls.graph), GTK_POS_RIGHT, FALSE);
    gwy_graph_set_axis_visible(GWY_GRAPH(controls.graph), GTK_POS_TOP, FALSE);
    gwy_graph_set_axis_visible(GWY_GRAPH(controls.graph), GTK_POS_BOTTOM, FALSE);
    gwy_graph_enable_user_input(GWY_GRAPH(controls.graph), FALSE);
    /* TODO: setup the area */
    gtk_box_pack_start(GTK_BOX(vbox), controls.graph, TRUE, TRUE, 0);

    table = gtk_table_new(3, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    controls.separate = gtk_check_button_new_with_mnemonic(_("_Separate curves"));
    gtk_table_attach(GTK_TABLE(table), controls.separate,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.separate, "toggled",
                             G_CALLBACK(prof_separate_changed), &controls);
    row++;

    prof_dialog_update_controls(&controls, args);

    /* finished initializing, allow instant updates */
    controls.in_init = FALSE;

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            g_object_unref(controls.gmodel);
            g_object_unref(controls.line);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_CLEAR:
            g_printerr("IMPLEMENT ME!\n");
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    g_printerr("IMPLEMENT ME!\n");
    g_object_unref(controls.mydata);
    g_object_unref(controls.gmodel);
    g_object_unref(controls.line);
    gtk_widget_destroy(dialog);
}

static void
prof_dialog_update_controls(ProfControls *controls,
                            ProfArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->separate),
                                 args->separate);
}

static void
prof_dialog_update_values(ProfControls *controls,
                          ProfArgs *args)
{
    args->separate
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->separate));
}

static void
prof_separate_changed(ProfControls *controls)
{
    controls->args->separate
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->separate));
}

static void
prof_selection_changed(ProfControls *controls,
                       gint hint)
{
    gint i, n;

    n = gwy_selection_get_data(controls->selection, NULL);
    if (hint < 0) {
        gwy_graph_model_remove_all_curves(controls->gmodel);
        for (i = 0; i < n; i++)
            prof_update_curve(controls, i);
    }
    else {
        prof_update_curve(controls, hint);
    }

    gtk_widget_set_sensitive(controls->ok, n > 0);
}

static void
prof_update_curve(ProfControls *controls,
                  gint i)
{
    GwyGraphCurveModel *gcmodel;
    gdouble xy[4];
    gint xl0, yl0, xl1, yl1;
    gint n, lineres;
    gchar *desc;

    g_return_if_fail(gwy_selection_get_object(controls->selection, i, xy));

    /* FIXME: odd/even */
    xl0 = gwy_data_field_get_xres(controls->fftfield)/2;
    yl0 = gwy_data_field_get_yres(controls->fftfield)/2;
    xl1 = floor(gwy_data_field_rtoj(controls->fftfield, xy[0]));
    yl1 = floor(gwy_data_field_rtoi(controls->fftfield, xy[1]));

    if (!controls->args->fixres) {
        lineres = GWY_ROUND(hypot(abs(xl0 - xl1) + 1, abs(yl0 - yl1) + 1));
        lineres = MAX(lineres, MIN_RESOLUTION);
    }
    else
        lineres = controls->args->resolution;

    gwy_data_field_get_profile(controls->fftfield, controls->line,
                               xl0, yl0, xl1, yl1,
                               lineres,
                               1,
                               controls->args->interpolation);

    n = gwy_graph_model_get_n_curves(controls->gmodel);
    if (i < n) {
        gcmodel = gwy_graph_model_get_curve(controls->gmodel, i);
    }
    else {
        gcmodel = gwy_graph_curve_model_new();
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", gwy_graph_get_preset_color(i),
                     NULL);
        gwy_graph_model_add_curve(controls->gmodel, gcmodel);
        g_object_unref(gcmodel);

        if (i == 0)
            gwy_graph_model_set_units_from_data_line(controls->gmodel,
                                                     controls->line);
    }

    gwy_graph_curve_model_set_data_from_dataline(gcmodel, controls->line, 0, 0);
    desc = g_strdup_printf(_("Profile %.1f°"), 180.0/G_PI*atan2(xy[1], xy[0]));
    g_object_set(gcmodel, "description", desc, NULL);
    g_free(desc);
}

static GwyDataField*
prof_fft_modulus(GwyDataField *dfield,
                 const ProfArgs *args)
{
    GwyDataField *fftre, *fftim;
    GwySIUnit *xyunit;
    const gdouble *im;
    gdouble *re;
    gint xres, yres, i, res;
    gdouble r;

    fftre = gwy_data_field_new_alike(dfield, FALSE);
    fftim = gwy_data_field_new_alike(dfield, FALSE);

    gwy_data_field_2dfft(dfield, NULL, fftre, fftim,
                         args->windowing,
                         GWY_TRANSFORM_DIRECTION_FORWARD,
                         GWY_INTERPOLATION_LINEAR,  /* ignored */
                         FALSE, 1);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    re = gwy_data_field_get_data(fftre);
    im = gwy_data_field_get_data_const(fftim);

    /* Put the modulus to fftre */
    for (i = 0; i < xres*yres; i++)
        re[i] = hypot(re[i], im[i]);

    g_object_unref(fftim);

    gwy_data_field_2dfft_humanize(fftre);
    xyunit = gwy_data_field_get_si_unit_xy(fftre);
    gwy_si_unit_power(xyunit, -1, xyunit);

    gwy_data_field_set_xreal(fftre, 1.0/gwy_data_field_get_xmeasure(fftre));
    gwy_data_field_set_yreal(fftre, 1.0/gwy_data_field_get_ymeasure(fftre));

    res = gwy_data_field_get_xres(fftre);
    r = (res + 1 + res % 2)/2.0;
    gwy_data_field_set_xoffset(fftre, -gwy_data_field_jtor(fftre, r));

    res = gwy_data_field_get_yres(fftre);
    r = (res + 1 + res % 2)/2.0;
    gwy_data_field_set_yoffset(fftre, -gwy_data_field_itor(fftre, r));

    return fftre;
}

static const gchar separate_key[]      = "/module/fft_profile/separate";
static const gchar fixres_key[]        = "/module/fft_profile/fixres";
static const gchar resolution_key[]    = "/module/fft_profile/resolution";
static const gchar interpolation_key[] = "/module/fft_profile/interpolation";

static void
prof_sanitize_args(ProfArgs *args)
{
    args->separate = !!args->separate;
    args->fixres = !!args->fixres;
    args->resolution = CLAMP(args->resolution, MIN_RESOLUTION, MAX_RESOLUTION);
    args->interpolation = gwy_enum_sanitize_value(args->interpolation,
                                                  GWY_TYPE_INTERPOLATION_TYPE);
}

static void
prof_load_args(GwyContainer *container,
               ProfArgs *args)
{
    *args = prof_defaults;

    gwy_container_gis_boolean_by_name(container, separate_key, &args->separate);
    gwy_container_gis_boolean_by_name(container, fixres_key, &args->fixres);
    gwy_container_gis_int32_by_name(container, resolution_key,
                                    &args->resolution);
    gwy_container_gis_enum_by_name(container, interpolation_key,
                                   &args->interpolation);
    prof_sanitize_args(args);
}

static void
prof_save_args(GwyContainer *container,
               ProfArgs *args)
{
    gwy_container_set_boolean_by_name(container, separate_key, args->separate);
    gwy_container_set_boolean_by_name(container, fixres_key, args->fixres);
    gwy_container_set_int32_by_name(container, resolution_key,
                                    args->resolution);
    gwy_container_set_enum_by_name(container, interpolation_key,
                                   args->interpolation);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
