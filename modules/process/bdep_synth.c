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
#include <libgwyddion/gwyrandgenset.h>
#include <libprocess/stats.h>
#include <libprocess/grains.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define BDEP_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define WORK_UPDATE_CHECK 1000000

enum {
    PREVIEW_SIZE = 320,
};

enum {
    RESPONSE_RESET = 1,
};

enum {
    PAGE_DIMENSIONS = 0,
    PAGE_GENERATOR  = 1,
    PAGE_GRAPHS     = 2,
    PAGE_NPAGES
};

typedef enum {
    GRAPH_MEAN   = 0,
    GRAPH_RMS    = 1,
    GRAPH_NFLAGS,
} GraphFlags;

typedef struct _BDepSynthControls BDepSynthControls;

typedef struct {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;   /* Always false */
    gboolean animated;
    gdouble coverage;
    gdouble height;
    gdouble height_noise;
    gboolean graph_flags[GRAPH_NFLAGS];
} BDepSynthArgs;

struct _BDepSynthControls {
    BDepSynthArgs *args;
    GwyDimensions *dims;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkWidget *animated;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkTable *table;
    GtkObject *coverage;
    GtkObject *height;
    GtkObject *height_noise;
    GtkWidget *height_units;
    GtkWidget *height_init;
    GtkWidget *graph_flags[GRAPH_NFLAGS];
    GwyContainer *mydata;
    GwyDataField *surface;
    gboolean in_init;
    gdouble zscale;
    gdouble pxsize;
};

static gboolean           module_register          (void);
static void               bdep_synth               (GwyContainer *data,
                                                    GwyRunType run);
static void               run_noninteractive       (BDepSynthArgs *args,
                                                    const GwyDimensionArgs *dimsargs,
                                                    GwyContainer *data,
                                                    GwyDataField *dfield,
                                                    gint oldid,
                                                    GQuark quark);
static gboolean           bdep_synth_dialog        (BDepSynthArgs *args,
                                                    GwyDimensionArgs *dimsargs,
                                                    GwyContainer *data,
                                                    GwyDataField *dfield,
                                                    gint id);
static void               update_controls          (BDepSynthControls *controls,
                                                    BDepSynthArgs *args);
static void               page_switched            (BDepSynthControls *controls,
                                                    GtkNotebookPage *page,
                                                    gint pagenum);
static void               update_values            (BDepSynthControls *controls);
static void               height_init_clicked      (BDepSynthControls *controls);
static void               bdep_synth_invalidate    (BDepSynthControls *controls);
static void               preview                  (BDepSynthControls *controls);
static gboolean           bdep_synth_do            (BDepSynthArgs *args,
                                                    GwyDataField *dfield,
                                                    GwyGraphCurveModel **gcmodels,
                                                    gdouble preview_time,
                                                    gdouble zscale);
static void               bdep_synth_load_args     (GwyContainer *container,
                                                    BDepSynthArgs *args,
                                                    GwyDimensionArgs *dimsargs);
static void               bdep_synth_save_args     (GwyContainer *container,
                                                    const BDepSynthArgs *args,
                                                    const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS BDepSynthControls
#define GWY_SYNTH_INVALIDATE(controls) bdep_synth_invalidate(controls)

#include "synth.h"

static const gchar* graph_flags[GRAPH_NFLAGS + 1/*unused*/] = {
    N_("Mean value"),
    N_("RMS"),
};

static const BDepSynthArgs bdep_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, FALSE, TRUE,
    10.0,
    1.0, 0.0,
    { FALSE, FALSE, },
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates surfaces by ballistic deposition."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("bdep_synth",
                              (GwyProcessFunc)&bdep_synth,
                              N_("/S_ynthetic/_Ballistic..."),
                              GWY_STOCK_SYNTHETIC_DIFFUSION,
                              BDEP_SYNTH_RUN_MODES,
                              0,
                              N_("Generate surface by ballistic deposition"));

    return TRUE;
}

static void
bdep_synth(GwyContainer *data, GwyRunType run)
{
    BDepSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & BDEP_SYNTH_RUN_MODES);
    bdep_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || bdep_synth_dialog(&args, &dimsargs, data, dfield, id)) {
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);
    }

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(BDepSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint oldid,
                   GQuark quark)
{
    GwyDataField *newfield;
    GwySIUnit *siunit;
    GwyGraphCurveModel *gcmodels[GRAPH_NFLAGS];
    gboolean replace = dimsargs->replace && dfield;
    gboolean add = dimsargs->add && dfield;
    gdouble zscale;
    gint newid;
    guint i;
    gboolean ok;

    if (args->randomize)
        args->seed = g_random_int() & 0x7fffffff;

    if (add || replace) {
        if (add)
            newfield = gwy_data_field_duplicate(dfield);
        else
            newfield = gwy_data_field_new_alike(dfield, TRUE);
    }
    else {
        gdouble mag = pow10(dimsargs->xypow10) * dimsargs->measure;
        newfield = gwy_data_field_new(dimsargs->xres, dimsargs->yres,
                                      mag*dimsargs->xres, mag*dimsargs->yres,
                                      TRUE);

        siunit = gwy_data_field_get_si_unit_xy(newfield);
        gwy_si_unit_set_from_string(siunit, dimsargs->xyunits);

        siunit = gwy_data_field_get_si_unit_z(newfield);
        gwy_si_unit_set_from_string(siunit, dimsargs->zunits);
    }

    zscale = pow10(dimsargs->zpow10) * args->height;
    gwy_app_wait_start(gwy_app_find_window_for_channel(data, oldid),
                       _("Starting..."));
    ok = bdep_synth_do(args, newfield, gcmodels, HUGE_VAL, zscale);
    gwy_app_wait_finish();

    if (!ok) {
        g_object_unref(newfield);
        return;
    }

    gwy_data_field_multiply(newfield, zscale);

    if (replace) {
        gwy_app_undo_qcheckpointv(data, 1, &quark);
        gwy_container_set_object(data, gwy_app_get_data_key_for_id(oldid),
                                 newfield);
        gwy_app_channel_log_add_proc(data, oldid, oldid);
        g_object_unref(newfield);
        newid = oldid;
    }
    else {
        if (data) {
            newid = gwy_app_data_browser_add_data_field(newfield, data, TRUE);
            if (oldid != -1)
                gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                        GWY_DATA_ITEM_GRADIENT,
                                        0);
        }
        else {
            newid = 0;
            data = gwy_container_new();
            gwy_container_set_object(data, gwy_app_get_data_key_for_id(newid),
                                     newfield);
            gwy_app_data_browser_add(data);
            gwy_app_data_browser_reset_visibility(data,
                                                  GWY_VISIBILITY_RESET_SHOW_ALL);
            g_object_unref(data);
        }

        gwy_app_set_data_field_title(data, newid, _("Generated"));
        gwy_app_channel_log_add_proc(data, add ? oldid : -1, newid);
        g_object_unref(newfield);
    }

    for (i = 0; i < GRAPH_NFLAGS; i++) {
        GwyGraphModel *gmodel;
        GwySIUnit *unit;
        gchar *s, *title;

        if (!gcmodels[i])
            continue;

        gmodel = gwy_graph_model_new();
        gwy_graph_model_add_curve(gmodel, gcmodels[i]);
        g_object_unref(gcmodels[i]);

        s = gwy_app_get_data_field_title(data, newid);
        title = g_strdup_printf("%s (%s)", graph_flags[i], s);
        g_free(s);
        g_object_set(gmodel,
                     "title", title,
                     "x-logarithmic", TRUE,
                     "y-logarithmic", TRUE,
                     "axis-label-bottom", _("Time"),
                     "axis-label-left", _(graph_flags[i]),
                     NULL);
        g_free(title);

        unit = gwy_data_field_get_si_unit_z(newfield);
        unit = gwy_si_unit_duplicate(unit);
        g_object_set(gmodel, "si-unit-x", unit, NULL);
        g_object_unref(unit);

        if (i == GRAPH_MEAN || i == GRAPH_RMS) {
            unit = gwy_si_unit_multiply(gwy_data_field_get_si_unit_z(newfield),
                                        gwy_data_field_get_si_unit_xy(newfield),
                                        NULL);
            g_object_set(gmodel, "si-unit-y", unit, NULL);
            g_object_unref(unit);
        }

        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
    }
}

static gboolean
bdep_synth_dialog(BDepSynthArgs *args,
                  GwyDimensionArgs *dimsargs,
                  GwyContainer *data,
                  GwyDataField *dfield_template,
                  gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *hbox2, *check, *label;
    BDepSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row, i;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    controls.pxsize = 1.0;
    dialog = gtk_dialog_new_with_buttons(_("Ballistic Deposition"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    dfield = gwy_data_field_new(PREVIEW_SIZE, PREVIEW_SIZE,
                                dimsargs->measure*PREVIEW_SIZE,
                                dimsargs->measure*PREVIEW_SIZE,
                                TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    if (dfield_template) {
        gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                0);
        controls.surface = gwy_synth_surface_for_preview(dfield_template,
                                                         PREVIEW_SIZE);
        controls.zscale = 3.0*gwy_data_field_get_rms(dfield_template);
    }
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 NULL);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);

    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);

    hbox2 = gwy_synth_instant_updates_new(&controls,
                                          &controls.update_now,
                                          &controls.update,
                                          &args->update);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(controls.update, TRUE);
    g_signal_connect_swapped(controls.update_now, "clicked",
                             G_CALLBACK(preview), &controls);

    controls.animated = check
        = gtk_check_button_new_with_mnemonic(_("Progressive preview"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), args->animated);
    gtk_box_pack_start(GTK_BOX(hbox2), check, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(check), "target", &args->animated);
    g_signal_connect_swapped(check, "toggled",
                             G_CALLBACK(gwy_synth_boolean_changed), &controls);

    gtk_box_pack_start(GTK_BOX(vbox),
                       gwy_synth_random_seed_new(&controls,
                                                 &controls.seed, &args->seed),
                       FALSE, FALSE, 0);

    controls.randomize = gwy_synth_randomize_new(&args->randomize);
    gtk_box_pack_start(GTK_BOX(vbox), controls.randomize, FALSE, FALSE, 0);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(hbox), notebook, TRUE, TRUE, 4);
    g_signal_connect_swapped(notebook, "switch-page",
                             G_CALLBACK(page_switched), &controls);

    controls.dims = gwy_dimensions_new(dimsargs, dfield_template);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             gwy_dimensions_get_widget(controls.dims),
                             gtk_label_new(_("Dimensions")));

    table = gtk_table_new(4 + (dfield_template ? 1 : 0), 4, FALSE);
    /* This is used only for synt.h helpers. */
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.coverage = gtk_adjustment_new(args->coverage,
                                           0.0, 10000.0, 0.01, 1.0, 0);
    g_object_set_data(G_OBJECT(controls.coverage), "target", &args->coverage);
    gwy_table_attach_hscale(table, row, _("Co_verage:"), NULL,
                            controls.coverage, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.coverage, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    row = gwy_synth_attach_height(&controls, row,
                                  &controls.height, &args->height,
                                  _("_Height:"), NULL, &controls.height_units);

    if (dfield_template) {
        controls.height_init
            = gtk_button_new_with_mnemonic(_("_Like Current Channel"));
        g_signal_connect_swapped(controls.height_init, "clicked",
                                 G_CALLBACK(height_init_clicked), &controls);
        gtk_table_attach(GTK_TABLE(table), controls.height_init,
                         1, 3, row, row+1, GTK_FILL, 0, 0, 0);
        row++;
    }

    row = gwy_synth_attach_variance(&controls, row,
                                    &controls.height_noise,
                                    &args->height_noise);

    table = gtk_table_new(1 + GRAPH_NFLAGS, 4, FALSE);
    /* This is used only for synt.h helpers. */
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Evolution")));
    row = 0;

    label = gtk_label_new(_("Plot graphs:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    for (i = 0; i < GRAPH_NFLAGS; i++) {
        GtkToggleButton *toggle;
        controls.graph_flags[i]
            = gtk_check_button_new_with_label(_(graph_flags[i]));
        toggle = GTK_TOGGLE_BUTTON(controls.graph_flags[i]);
        gtk_toggle_button_set_active(toggle, args->graph_flags[i]);
        gtk_table_attach(GTK_TABLE(table), controls.graph_flags[i],
                         0, 3, row, row+1, GTK_FILL, 0, 0, 0);
        g_signal_connect(toggle, "toggled",
                         G_CALLBACK(gwy_synth_boolean_changed_silent),
                         &args->graph_flags[i]);
        row++;
    }

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);

    finished = FALSE;
    while (!finished) {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_OK:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            finished = TRUE;
            break;

            case RESPONSE_RESET:
            {
                gint temp2 = args->active_page;
                *args = bdep_synth_defaults;
                args->active_page = temp2;
            }
            controls.in_init = TRUE;
            update_controls(&controls, args);
            controls.in_init = FALSE;
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }

    bdep_synth_save_args(gwy_app_settings_get(), args, dimsargs);

    g_object_unref(controls.mydata);
    gwy_object_unref(controls.surface);
    gwy_dimensions_free(controls.dims);

    return response == GTK_RESPONSE_OK;
}

static void
update_controls(BDepSynthControls *controls,
                BDepSynthArgs *args)
{
    guint i;

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->animated),
                                 args->animated);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->coverage),
                             args->coverage);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height), args->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height_noise),
                             args->height_noise);
    for (i = 0; i < GRAPH_NFLAGS; i++) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->graph_flags[i]),
                                     args->graph_flags[i]);
    }
}

static void
page_switched(BDepSynthControls *controls,
              G_GNUC_UNUSED GtkNotebookPage *page,
              gint pagenum)
{
    if (controls->in_init)
        return;

    controls->args->active_page = pagenum;
    if (pagenum == PAGE_GENERATOR)
        update_values(controls);
}

static void
update_values(BDepSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
    if (controls->height_units)
        gtk_label_set_markup(GTK_LABEL(controls->height_units),
                             dims->zvf->units);
}

static void
height_init_clicked(BDepSynthControls *controls)
{
    gdouble mag = pow10(controls->dims->args->zpow10);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height),
                             controls->zscale/mag);
}

static void
bdep_synth_invalidate(G_GNUC_UNUSED BDepSynthControls *controls)
{
}

static void
preview(BDepSynthControls *controls)
{
    BDepSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, TRUE);
    else
        gwy_data_field_clear(dfield);

    gwy_app_wait_start(GTK_WINDOW(controls->dialog), _("Starting..."));
    bdep_synth_do(args, dfield, NULL, 1.25, 1.0);
    gwy_app_wait_finish();

    gwy_data_field_data_changed(dfield);
}

static gboolean
bdep_synth_do(BDepSynthArgs *args,
              GwyDataField *dfield,
              GwyGraphCurveModel **gcmodels,
              gdouble preview_time,
              gdouble zscale)
{
    gint xres, yres, n;
    guint i;
    gdouble nextgraphx;
    gdouble lasttime = 0.0, lastpreviewtime = 0.0, currtime;
    gdouble flux, height, hnoise;
    GTimer *timer;
    guint64 workdone, niter, iter;
    GwyRandGenSet *rngset;
    GRand *rng_k, *rng_height;
    GArray **evolution = NULL;
    gdouble *d;
    gboolean any_graphs = FALSE;
    gboolean finished = FALSE;

    timer = g_timer_new();

    evolution = g_new0(GArray*, GRAPH_NFLAGS + 1);
    if (gcmodels) {
        for (i = 0; i < GRAPH_NFLAGS; i++) {
            if (args->graph_flags[i]) {
                evolution[i] = g_array_new(FALSE, FALSE, sizeof(gdouble));
                any_graphs = TRUE;
            }
        }
        if (any_graphs)
            evolution[GRAPH_NFLAGS] = g_array_new(FALSE, FALSE, sizeof(gdouble));
    }

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    n = xres*yres;
    flux = 1.0/n;

    rngset = gwy_rand_gen_set_new(2);
    gwy_rand_gen_set_init(rngset, args->seed);
    rng_k = gwy_rand_gen_set_rng(rngset, 0);
    rng_height = gwy_rand_gen_set_rng(rngset, 1);

    gwy_app_wait_set_message(_("Depositing particles..."));
    gwy_app_wait_set_fraction(0.0);

    d = dfield->data;
    hnoise = args->height_noise;
    niter = (guint64)(args->coverage/flux + 0.5);
    nextgraphx = 0.0;
    workdone = 0.0;
    iter = 0;

    while (iter < niter) {
        guint k = g_rand_int_range(rng_k, 0, n);
        gdouble v = (hnoise
                     ? hnoise*g_rand_double(rng_height) + 1.0 - hnoise
                     : 1.0);
        guint ii = (k/xres)*xres, j = k % xres;
        gdouble h = d[k] + v;
        guint iim = G_LIKELY(ii) ? ii - xres : n - xres;
        guint iip = G_LIKELY(ii != n - xres) ? ii + xres : 0;
        guint jl = G_LIKELY(j) ? j-1 : xres-1;
        guint jr = G_LIKELY(j != xres-1) ? j+1 : 0;
        gdouble h1 = fmax(d[iim + j], d[ii + jl]);
        gdouble h2 = fmax(d[ii + jr], d[iip + j]);
        d[k] = fmax(h, fmax(h1, h2));

        iter++;
        workdone++;

        if (workdone >= WORK_UPDATE_CHECK) {
            currtime = g_timer_elapsed(timer, NULL);
            if (currtime - lasttime >= 0.25) {
                if (!gwy_app_wait_set_fraction((gdouble)iter/niter))
                    goto fail;
                lasttime = currtime;

                if (args->animated
                    && currtime - lastpreviewtime >= preview_time) {
                    gwy_data_field_invalidate(dfield);
                    gwy_data_field_data_changed(dfield);
                    lastpreviewtime = lasttime;
                }
            }
            workdone -= WORK_UPDATE_CHECK;
        }

        if (any_graphs && iter >= nextgraphx) {
            gwy_data_field_invalidate(dfield);
            height = iter*flux*zscale;
            g_array_append_val(evolution[GRAPH_NFLAGS], height);
            if (evolution[GRAPH_MEAN]) {
                gdouble avg = gwy_data_field_get_avg(dfield);
                g_array_append_val(evolution[GRAPH_MEAN], avg);
            }
            if (evolution[GRAPH_RMS]) {
                gdouble avg = gwy_data_field_get_rms(dfield);
                g_array_append_val(evolution[GRAPH_RMS], avg);
            }

            nextgraphx += 0.0001/flux + MIN(0.2*nextgraphx, 0.08/flux);
        }
    }

    if (gcmodels) {
        const gdouble *xdata;
        for (i = 0; i < GRAPH_NFLAGS; i++) {
            if (!evolution[i]) {
                gcmodels[i] = NULL;
                continue;
            }

            xdata = (const gdouble*)evolution[GRAPH_NFLAGS]->data;
            gcmodels[i] = gwy_graph_curve_model_new();
            gwy_graph_curve_model_set_data(gcmodels[i],
                                           xdata,
                                           (gdouble*)evolution[i]->data,
                                           evolution[GRAPH_NFLAGS]->len);
            g_object_set(gcmodels[i], "description", _(graph_flags[i]), NULL);
        }
    }

    gwy_data_field_invalidate(dfield);
    gwy_data_field_data_changed(dfield);
    finished = TRUE;

fail:
    g_timer_destroy(timer);
    gwy_rand_gen_set_free(rngset);
    for (i = 0; i <= GRAPH_NFLAGS; i++) {
        if (evolution[i])
            g_array_free(evolution[i], TRUE);
    }
    g_free(evolution);

    return finished;
}

static const gchar prefix[]           = "/module/bdep_synth";
static const gchar active_page_key[]  = "/module/bdep_synth/active_page";
static const gchar animated_key[]     = "/module/bdep_synth/animated";
static const gchar coverage_key[]     = "/module/bdep_synth/coverage";
static const gchar graph_flags_key[]  = "/module/bdep_synth/graph_flags";
static const gchar height_key[]       = "/module/bdep_synth/height";
static const gchar height_noise_key[] = "/module/bdep_synth/height_noise";
static const gchar randomize_key[]    = "/module/bdep_synth/randomize";
static const gchar seed_key[]         = "/module/bdep_synth/seed";

static void
bdep_synth_sanitize_args(BDepSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->animated = !!args->animated;
    args->height = CLAMP(args->height, 0.001, 1000.0);
    args->height_noise = CLAMP(args->height, 0.0, 1.0);
    args->coverage = CLAMP(args->coverage, 0.0, 10000.0);
}

static void
bdep_synth_load_args(GwyContainer *container,
                     BDepSynthArgs *args,
                     GwyDimensionArgs *dimsargs)
{
    guint i;
    gint gflags = 0;

    *args = bdep_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_boolean_by_name(container, animated_key,
                                      &args->animated);
    gwy_container_gis_double_by_name(container, height_key, &args->height);
    gwy_container_gis_double_by_name(container, height_noise_key,
                                     &args->height_noise);
    gwy_container_gis_double_by_name(container, coverage_key, &args->coverage);
    gwy_container_gis_int32_by_name(container, graph_flags_key, &gflags);

    for (i = 0; i < GRAPH_NFLAGS; i++)
        args->graph_flags[i] = !!(gflags & (1 << i));

    bdep_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
bdep_synth_save_args(GwyContainer *container,
                    const BDepSynthArgs *args,
                    const GwyDimensionArgs *dimsargs)
{
    guint i;
    gint gflags = 0;

    for (i = 0; i < GRAPH_NFLAGS; i++)
        gflags |= (args->graph_flags[i] ? (1 << i) : 0);

    gwy_container_set_int32_by_name(container, active_page_key,
                                    args->active_page);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
    gwy_container_set_boolean_by_name(container, animated_key,
                                      args->animated);
    gwy_container_set_double_by_name(container, height_key, args->height);
    gwy_container_set_double_by_name(container, height_noise_key,
                                     args->height_noise);
    gwy_container_set_double_by_name(container, coverage_key, args->coverage);
    gwy_container_set_int32_by_name(container, graph_flags_key, gflags);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
