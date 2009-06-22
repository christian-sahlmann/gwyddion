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

#define PROF_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400,
    GRAPH_WIDTH = 320,
    NLINES = 12,
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
    GtkObject *resolution;
    GtkWidget *fixres;
    GtkWidget *interpolation;
    GtkWidget *view;
    gdouble hx;
    gdouble hy;
    GwyDataField *psdffield;
    GwyDataField *modfield;
    GwySelection *selection;
    GtkWidget *graph;
    GwyDataLine *line;
    GwyGraphModel *gmodel;
    GtkWidget *separate;
    GwyContainer *mydata;
} ProfControls;

static gboolean      module_register            (void);
static void          fft_profile                (GwyContainer *data,
                                                 GwyRunType run);
static void          prof_dialog                (ProfArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 gint id);
static void          prof_fixres_changed        (ProfControls *controls,
                                                 GtkToggleButton *check);
static void          prof_resolution_changed    (ProfControls *controls,
                                                 GtkAdjustment *adj);
static void          prof_interpolation_changed (GtkComboBox *combo,
                                                 ProfControls *controls);
static void          prof_separate_changed      (ProfControls *controls);
static void          prof_selection_changed     (ProfControls *controls,
                                                 gint hint);
static void          prof_update_curve          (ProfControls *controls,
                                                 gint i);
static void          prof_execute               (ProfControls *controls,
                                                 GwyContainer *container);
static GwyDataField* prof_psdf                  (GwyDataField *dfield,
                                                 const ProfArgs *args);
static GwyDataField* prof_sqrt                  (GwyDataField *dfield);
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
    N_("Reads radial sections of two-dimensional power spectrum density "
       "function."),
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
                              N_("/_Statistics/_PDSF Section..."),
                              GWY_STOCK_GRAPH_HALFGAUSS,
                              PROF_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Read radial PSDF sections"));

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
    GtkWidget *hbox, *hbox2, *vbox, *label;
    GtkTable *table;
    GtkDialog *dialog;
    GwyGraph *graph;
    GwyGraphArea *area;
    ProfControls controls;
    gint response;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer;
    gint row;

    controls.args = args;
    controls.psdffield = prof_psdf(dfield, args);
    controls.modfield = prof_sqrt(controls.psdffield);
    controls.hx = gwy_data_field_get_xmeasure(dfield)/(2*G_PI);
    controls.hy = gwy_data_field_get_xmeasure(dfield)/(2*G_PI);

    controls.dialog = gtk_dialog_new_with_buttons(_("Radial PSDF Section"),
                                                  NULL, 0,
                                                  GTK_STOCK_CLEAR,
                                                  RESPONSE_CLEAR,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_CANCEL,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_OK,
                                                  NULL);
    dialog = GTK_DIALOG(controls.dialog);
    gtk_dialog_set_has_separator(dialog, FALSE);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
    gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_OK, FALSE);

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data",
                                     controls.modfield);
    g_object_unref(controls.modfield);
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
    g_object_set(controls.gmodel,
                 "title", _("PSDF Section"),
                 "axis-label-bottom", "k",
                 "axis-label-left", "W",
                 NULL);
    controls.line = gwy_data_line_new(1, 1.0, FALSE);

    controls.graph = gwy_graph_new(controls.gmodel);
    graph = GWY_GRAPH(controls.graph);
    gtk_widget_set_size_request(controls.graph, GRAPH_WIDTH, -1);
    gwy_graph_set_axis_visible(graph, GTK_POS_LEFT, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_RIGHT, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_TOP, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_BOTTOM, FALSE);
    gwy_graph_enable_user_input(graph, FALSE);
    area = GWY_GRAPH_AREA(gwy_graph_get_area(graph));
    gwy_graph_area_enable_user_input(area, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), controls.graph, TRUE, TRUE, 0);

    table = GTK_TABLE(gtk_table_new(3, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    row = 0;

    controls.resolution = gtk_adjustment_new(controls.args->resolution,
                                             MIN_RESOLUTION, MAX_RESOLUTION,
                                             1, 10, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Fix res.:"), NULL,
                            controls.resolution,
                            GWY_HSCALE_CHECK | GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.resolution, "value-changed",
                             G_CALLBACK(prof_resolution_changed), &controls);
    controls.fixres = gwy_table_hscale_get_check(controls.resolution);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.fixres),
                                 controls.args->fixres);
    g_signal_connect_swapped(controls.fixres, "toggled",
                             G_CALLBACK(prof_fixres_changed), &controls);
    gwy_table_hscale_set_sensitive(controls.resolution, controls.args->fixres);
    row++;

    controls.separate = gtk_check_button_new_with_mnemonic(_("_Separate curves"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.separate),
                                 args->separate);
    gtk_table_attach(table, controls.separate,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.separate, "toggled",
                             G_CALLBACK(prof_separate_changed), &controls);
    row++;

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_table_attach(table, hbox2,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    controls.interpolation
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(prof_interpolation_changed),
                                 &controls,
                                 controls.args->interpolation, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.interpolation);
    gtk_box_pack_end(GTK_BOX(hbox2), controls.interpolation, FALSE, FALSE, 0);
    row++;

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

            case RESPONSE_CLEAR:
            gwy_selection_clear(controls.selection);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(controls.dialog);
    prof_execute(&controls, data);
finalize:
    g_object_unref(controls.psdffield);
    g_object_unref(controls.mydata);
    g_object_unref(controls.gmodel);
    g_object_unref(controls.line);
}

static void
prof_resolution_changed(ProfControls *controls,
                        GtkAdjustment *adj)
{
    controls->args->resolution = gwy_adjustment_get_int(adj);
    /* Resolution can be changed only when fixres == TRUE */
    prof_selection_changed(controls, -1);
}

static void
prof_fixres_changed(ProfControls *controls,
                    GtkToggleButton *check)
{
    controls->args->fixres = gtk_toggle_button_get_active(check);
    prof_selection_changed(controls, -1);
}

static void
prof_interpolation_changed(GtkComboBox *combo,
                           ProfControls *controls)
{
    controls->args->interpolation = gwy_enum_combo_box_get_active(combo);
    prof_selection_changed(controls, -1);
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

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      GTK_RESPONSE_OK, n > 0);
}

static void
prof_update_curve(ProfControls *controls,
                  gint i)
{
    GwyGraphCurveModel *gcmodel;
    gdouble xy[4], h;
    gint xl0, yl0, xl1, yl1;
    gint n, lineres;
    gchar *desc;

    g_return_if_fail(gwy_selection_get_object(controls->selection, i, xy));

    /* The ω=0 pixel is always at res/2, for even dimensions it means it is
     * shifted half-a-pixel to the right from the precise centre. */
    xl0 = gwy_data_field_get_xres(controls->psdffield)/2;
    yl0 = gwy_data_field_get_yres(controls->psdffield)/2;
    xl1 = floor(gwy_data_field_rtoj(controls->psdffield, xy[0]));
    yl1 = floor(gwy_data_field_rtoi(controls->psdffield, xy[1]));
    xy[0] += gwy_data_field_get_xoffset(controls->psdffield);
    xy[1] += gwy_data_field_get_yoffset(controls->psdffield);
    h = hypot(controls->hx*xy[0], controls->hy*xy[1])/hypot(xy[0], xy[1]);

    if (!controls->args->fixres) {
        lineres = GWY_ROUND(hypot(abs(xl0 - xl1) + 1, abs(yl0 - yl1) + 1));
        lineres = MAX(lineres, MIN_RESOLUTION);
    }
    else
        lineres = controls->args->resolution;

    gwy_data_field_get_profile(controls->psdffield, controls->line,
                               xl0, yl0, xl1, yl1,
                               lineres,
                               1,
                               controls->args->interpolation);
    gwy_data_line_multiply(controls->line, h);

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
    desc = g_strdup_printf(_("PSDF %.0f°"), 180.0/G_PI*atan2(-xy[1], xy[0]));
    g_object_set(gcmodel, "description", desc, NULL);
    g_free(desc);
}

static void
prof_execute(ProfControls *controls,
             GwyContainer *container)
{
    GwyGraphCurveModel *gcmodel;
    GwyGraphModel *gmodel;
    gchar *s;
    gint i, n;

    n = gwy_selection_get_data(controls->selection, NULL);
    g_return_if_fail(n);

    if (!controls->args->separate) {
        gmodel = gwy_graph_model_duplicate(controls->gmodel);
        g_object_set(gmodel, "label-visible", TRUE, NULL);
        gwy_app_data_browser_add_graph_model(gmodel, container, TRUE);
        g_object_unref(gmodel);

        return;
    }

    for (i = 0; i < n; i++) {
        gmodel = gwy_graph_model_new_alike(controls->gmodel);
        g_object_set(gmodel, "label-visible", TRUE, NULL);
        gcmodel = gwy_graph_model_get_curve(controls->gmodel, i);
        gcmodel = gwy_graph_curve_model_duplicate(gcmodel);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
        g_object_get(gcmodel, "description", &s, NULL);
        g_object_set(gmodel, "title", s, NULL);
        g_free(s);
        gwy_app_data_browser_add_graph_model(gmodel, container, TRUE);
        g_object_unref(gmodel);
    }
}

static GwyDataField*
prof_psdf(GwyDataField *dfield,
          const ProfArgs *args)
{
    GwyDataField *fftre, *fftim;
    GwySIUnit *xyunit, *zunit;
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

    /* Put the PSDF to fftre. */
    for (i = 0; i < xres*yres; i++)
        re[i] = re[i]*re[i] + im[i]*im[i];

    g_object_unref(fftim);

    gwy_data_field_2dfft_humanize(fftre);
    xyunit = gwy_data_field_get_si_unit_xy(fftre);
    gwy_si_unit_power(gwy_data_field_get_si_unit_xy(dfield), -1, xyunit);
    zunit = gwy_data_field_get_si_unit_z(fftre);
    gwy_si_unit_power(gwy_data_field_get_si_unit_z(dfield), 2, zunit);
    gwy_si_unit_multiply(gwy_data_field_get_si_unit_xy(dfield), zunit, zunit);

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

static GwyDataField*
prof_sqrt(GwyDataField *dfield)
{
    gint xres, yres, i;
    gdouble *data;

    dfield = gwy_data_field_duplicate(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    for (i = 0; i < xres*yres; i++)
        data[i] = sqrt(data[i]);

    return dfield;
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
