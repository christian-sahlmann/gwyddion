/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
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
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define FBM_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400,
};

enum {
    RESPONSE_RESET = 1,
};

enum {
    PAGE_DIMENSIONS = 0,
    PAGE_GENERATOR  = 1,
    PAGE_NPAGES
};

typedef enum {
    FBM_DISTRIBUTION_GAUSSIAN    = 0,
    FBM_DISTRIBUTION_EXPONENTIAL = 1,
    FBM_DISTRIBUTION_UNIFORM     = 2,
    FBM_DISTRIBUTION_POWER       = 3,
    FBM_DISTRIBUTION_NTYPES
} FBMDistributionType;

typedef struct {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;
    FBMDistributionType distribution;
    gdouble power;
    gdouble H;
    gint hom_scale;
    gdouble sigma;
} FBMSynthArgs;

typedef struct {
    GwyDataField *field;
    gdouble *H_powers;
    gdouble hom_sigma;
    gboolean *visited;
    guint xres;
    guint yres;
    GwyRandGenSet *rngset;
} FBMSynthState;

typedef struct {
    FBMSynthArgs *args;
    GwyDimensions *dims;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkTable *table;
    GtkWidget *randomize;
    GtkObject *H;
    GtkObject *hom_scale;
    GtkWidget *distribution;
    GtkObject *power;
    GtkObject *sigma;
    GtkWidget *sigma_units;
    GtkWidget *sigma_init;
    GwyContainer *mydata;
    GwyDataField *surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
    gulong sid;
} FBMSynthControls;

static gboolean   module_register           (void);
static void       fbm_synth                 (GwyContainer *data,
                                             GwyRunType run);
static void       run_noninteractive        (FBMSynthArgs *args,
                                             const GwyDimensionArgs *dimsargs,
                                             GwyContainer *data,
                                             GwyDataField *dfield,
                                             gint oldid,
                                             GQuark quark);
static gboolean   fbm_synth_dialog          (FBMSynthArgs *args,
                                             GwyDimensionArgs *dimsargs,
                                             GwyContainer *data,
                                             GwyDataField *dfield,
                                             gint id);
static GtkWidget* distribution_selector_new (FBMSynthControls *controls);
static void       update_controls           (FBMSynthControls *controls,
                                             FBMSynthArgs *args);
static void       page_switched             (FBMSynthControls *controls,
                                             GtkNotebookPage *page,
                                             gint pagenum);
static void       update_values             (FBMSynthControls *controls);
static void       distribution_type_selected(GtkComboBox *combo,
                                             FBMSynthControls *controls);
static void       sigma_init_clicked        (FBMSynthControls *controls);
static void       H_changed                 (FBMSynthControls *controls,
                                             GtkAdjustment *adj);
static void       power_changed             (FBMSynthControls *controls,
                                             GtkAdjustment *adj);
static void       hom_scale_changed         (FBMSynthControls *controls,
                                             GtkAdjustment *adj);
static void       fbm_synth_invalidate      (FBMSynthControls *controls);
static gboolean   preview_gsource           (gpointer user_data);
static void       preview                   (FBMSynthControls *controls);
static void       fbm_synth_do              (const FBMSynthArgs *args,
                                             const GwyDimensionArgs *dimsargs,
                                             GwyDataField *dfield);
static void       fbm_synth_load_args       (GwyContainer *container,
                                             FBMSynthArgs *args,
                                             GwyDimensionArgs *dimsargs);
static void       fbm_synth_save_args       (GwyContainer *container,
                                             const FBMSynthArgs *args,
                                             const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS FBMSynthControls
#define GWY_SYNTH_INVALIDATE(controls) \
    fbm_synth_invalidate(controls)

#include "synth.h"

static const FBMSynthArgs fbm_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    FBM_DISTRIBUTION_UNIFORM, 3.0,
    0.5, 16384,
    1.0,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates random surfaces similar to fractional Brownian motion."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fbm_synth",
                              (GwyProcessFunc)&fbm_synth,
                              N_("/S_ynthetic/_Brownian..."),
                              NULL,
                              FBM_SYNTH_RUN_MODES,
                              0,
                              N_("Generate fractional Brownian motion-like "
                                 "surface"));

    return TRUE;
}

static void
fbm_synth(GwyContainer *data, GwyRunType run)
{
    FBMSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & FBM_SYNTH_RUN_MODES);
    fbm_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || fbm_synth_dialog(&args, &dimsargs, data, dfield, id))
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);

    if (run == GWY_RUN_INTERACTIVE)
        fbm_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(FBMSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint oldid,
                   GQuark quark)
{
    GwySIUnit *siunit;
    gboolean replace = dimsargs->replace && dfield;
    gboolean add = dimsargs->add && dfield;
    gint newid;

    if (args->randomize)
        args->seed = g_random_int() & 0x7fffffff;

    if (replace) {
        /* Always take a reference so that we can always unref. */
        g_object_ref(dfield);

        gwy_app_undo_qcheckpointv(data, 1, &quark);
        if (!add)
            gwy_data_field_clear(dfield);

        gwy_app_channel_log_add_proc(data, oldid, oldid);
    }
    else {
        if (add)
            dfield = gwy_data_field_duplicate(dfield);
        else {
            gdouble mag = pow10(dimsargs->xypow10) * dimsargs->measure;
            dfield = gwy_data_field_new(dimsargs->xres, dimsargs->yres,
                                        mag*dimsargs->xres, mag*dimsargs->yres,
                                        TRUE);

            siunit = gwy_data_field_get_si_unit_xy(dfield);
            gwy_si_unit_set_from_string(siunit, dimsargs->xyunits);

            siunit = gwy_data_field_get_si_unit_z(dfield);
            gwy_si_unit_set_from_string(siunit, dimsargs->zunits);
        }
    }

    fbm_synth_do(args, dimsargs, dfield);

    if (replace)
        gwy_data_field_data_changed(dfield);
    else {
        if (data) {
            newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
            if (oldid != -1)
                gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                        GWY_DATA_ITEM_GRADIENT,
                                        0);
        }
        else {
            newid = 0;
            data = gwy_container_new();
            gwy_container_set_object(data, gwy_app_get_data_key_for_id(newid),
                                     dfield);
            gwy_app_data_browser_add(data);
            gwy_app_data_browser_reset_visibility(data,
                                                  GWY_VISIBILITY_RESET_SHOW_ALL);
            g_object_unref(data);
        }

        gwy_app_set_data_field_title(data, newid, _("Generated"));
        gwy_app_channel_log_add_proc(data, add ? oldid : -1, newid);
    }
    g_object_unref(dfield);
}

static gboolean
fbm_synth_dialog(FBMSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook;
    FBMSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Fractional Brownian Motion"),
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
        controls.zscale = gwy_data_field_get_rms(dfield_template);
    }
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 NULL);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);

    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox),
                       gwy_synth_instant_updates_new(&controls,
                                                     &controls.update_now,
                                                     &controls.update,
                                                     &args->update),
                       FALSE, FALSE, 0);
    g_signal_connect_swapped(controls.update_now, "clicked",
                             G_CALLBACK(preview), &controls);

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
    if (controls.dims->add)
        g_signal_connect_swapped(controls.dims->add, "toggled",
                                 G_CALLBACK(fbm_synth_invalidate), &controls);

    table = gtk_table_new(6 + (dfield_template ? 1 : 0), 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(controls.table, 2);
    gtk_table_set_col_spacings(controls.table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.H = gtk_adjustment_new(args->H, -0.999, 0.999, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("_Hurst parameter:"), NULL,
                            controls.H, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.H, "value-changed",
                             G_CALLBACK(H_changed), &controls);
    row++;

    controls.hom_scale = gtk_adjustment_new(args->hom_scale,
                                            2, 8192, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Stationarity scale:"), "px",
                            controls.hom_scale, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.hom_scale, "value-changed",
                             G_CALLBACK(hom_scale_changed), &controls);
    row++;

    controls.distribution = distribution_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Distribution:"), NULL,
                            GTK_OBJECT(controls.distribution),
                            GWY_HSCALE_WIDGET);
    row++;

    controls.power = gtk_adjustment_new(args->power, 2.01, 12.0, 0.01, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("Po_wer:"), NULL,
                            controls.power, GWY_HSCALE_SQRT);
    gwy_table_hscale_set_sensitive(controls.power,
                                   args->distribution == FBM_DISTRIBUTION_POWER);
    g_signal_connect_swapped(controls.power, "value-changed",
                             G_CALLBACK(power_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    row = gwy_synth_attach_height(&controls, row,
                                  &controls.sigma, &args->sigma,
                                  _("_RMS:"), NULL, &controls.sigma_units);

    if (dfield_template) {
        controls.sigma_init
            = gtk_button_new_with_mnemonic(_("_Like Current Channel"));
        g_signal_connect_swapped(controls.sigma_init, "clicked",
                                 G_CALLBACK(sigma_init_clicked), &controls);
        gtk_table_attach(GTK_TABLE(table), controls.sigma_init,
                         1, 3, row, row+1, GTK_FILL, 0, 0, 0);
        row++;
    }

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);
    fbm_synth_invalidate(&controls);

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
                gboolean temp = args->update;
                gint temp2 = args->active_page;
                *args = fbm_synth_defaults;
                args->active_page = temp2;
                args->update = temp;
            }
            controls.in_init = TRUE;
            update_controls(&controls, args);
            controls.in_init = FALSE;
            if (args->update)
                preview(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }

    if (controls.sid) {
        g_source_remove(controls.sid);
        controls.sid = 0;
    }
    g_object_unref(controls.mydata);
    gwy_object_unref(controls.surface);
    gwy_dimensions_free(controls.dims);

    return response == GTK_RESPONSE_OK;
}

static GtkWidget*
distribution_selector_new(FBMSynthControls *controls)
{
    static const GwyEnum generators[] = {
        { N_("distribution|Uniform"),     FBM_DISTRIBUTION_UNIFORM,     },
        { N_("distribution|Gaussian"),    FBM_DISTRIBUTION_GAUSSIAN,    },
        { N_("distribution|Exponential"), FBM_DISTRIBUTION_EXPONENTIAL, },
        { N_("distribution|Power"),       FBM_DISTRIBUTION_POWER,       },
    };

    GtkWidget *combo;

    combo = gwy_enum_combo_box_new(generators, G_N_ELEMENTS(generators),
                                   G_CALLBACK(distribution_type_selected),
                                   controls, controls->args->distribution, TRUE);
    return combo;
}

static void
update_controls(FBMSynthControls *controls,
                FBMSynthArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->H), args->H);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->power), args->power);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->hom_scale),
                             args->hom_scale);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->sigma), args->sigma);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->distribution),
                                  args->distribution);
}

static void
page_switched(FBMSynthControls *controls,
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
update_values(FBMSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
    if (controls->sigma_units)
        gtk_label_set_markup(GTK_LABEL(controls->sigma_units),
                             dims->zvf->units);
}

static void
distribution_type_selected(GtkComboBox *combo,
                           FBMSynthControls *controls)
{
    FBMSynthArgs *args = controls->args;

    args->distribution = gwy_enum_combo_box_get_active(combo);
    gwy_table_hscale_set_sensitive(controls->power,
                                   args->distribution == FBM_DISTRIBUTION_POWER);
    fbm_synth_invalidate(controls);
}

static void
sigma_init_clicked(FBMSynthControls *controls)
{
    gdouble mag = pow10(controls->dims->args->zpow10);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->sigma),
                             controls->zscale/mag);
}

static void
H_changed(FBMSynthControls *controls,
          GtkAdjustment *adj)
{
    controls->args->H = gtk_adjustment_get_value(adj);
    fbm_synth_invalidate(controls);
}

static void
power_changed(FBMSynthControls *controls,
              GtkAdjustment *adj)
{
    controls->args->power = gtk_adjustment_get_value(adj);
    fbm_synth_invalidate(controls);

}

static void
hom_scale_changed(FBMSynthControls *controls,
                  GtkAdjustment *adj)
{
    controls->args->hom_scale = gwy_adjustment_get_int(adj);
    fbm_synth_invalidate(controls);
}

static void
fbm_synth_invalidate(FBMSynthControls *controls)
{
    /* create preview if instant updates are on */
    if (controls->args->update && !controls->in_init && !controls->sid) {
        controls->sid = g_idle_add_full(G_PRIORITY_LOW, preview_gsource,
                                        controls, NULL);
    }
}

static gboolean
preview_gsource(gpointer user_data)
{
    FBMSynthControls *controls = (FBMSynthControls*)user_data;
    controls->sid = 0;

    preview(controls);

    return FALSE;
}

static void
preview(FBMSynthControls *controls)
{
    FBMSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, FALSE);
    else
        gwy_data_field_clear(dfield);

    fbm_synth_do(args, controls->dims->args, dfield);

    gwy_data_field_data_changed(dfield);
}

static FBMSynthState*
fbm_synth_state_new(GwyDataField *dfield,
                    const FBMSynthArgs *args)
{
    FBMSynthState *fbmstate = g_new0(FBMSynthState, 1);
    guint npowers, i, xres = dfield->xres, yres = dfield->yres;
    gdouble H = args->H;

    fbmstate->field = dfield;
    fbmstate->xres = xres;
    fbmstate->yres = yres;
    fbmstate->visited = g_new0(gboolean, xres*yres);
    fbmstate->rngset = gwy_rand_gen_set_new(1);
    gwy_rand_gen_set_init(fbmstate->rngset, args->seed);

    npowers = MAX(xres, yres) + 1;
    fbmstate->H_powers = g_new(gdouble, npowers);
    fbmstate->H_powers[0] = 0.0;
    for (i = 1; i < npowers; i++)
        fbmstate->H_powers[i] = pow(i, H);

    fbmstate->hom_sigma = pow(args->hom_scale, H);

    return fbmstate;
}

static void
fbm_synth_state_free(FBMSynthState *fbmstate)
{
    g_free(fbmstate->H_powers);
    g_free(fbmstate->visited);
    gwy_rand_gen_set_free(fbmstate->rngset);
    g_free(fbmstate);
}

static void
initialise(FBMSynthState *fbmstate)
{
    GwyRandGenSet *rngset = fbmstate->rngset;
    guint xres = fbmstate->xres, yres = fbmstate->yres;
    gdouble *data = fbmstate->field->data;
    gboolean *visited = fbmstate->visited;
    gdouble sigma = fbmstate->hom_sigma;

    data[0] = gwy_rand_gen_set_uniform(rngset, 0, sigma);
    data[xres-1] = gwy_rand_gen_set_uniform(rngset, 0, sigma);
    data[xres*(yres-1)] = gwy_rand_gen_set_uniform(rngset, 0, sigma);
    data[xres*yres - 1] = gwy_rand_gen_set_uniform(rngset, 0, sigma);
    visited[0] = TRUE;
    visited[xres-1] = TRUE;
    visited[xres*(yres-1)] = TRUE;
    visited[xres*yres - 1] = TRUE;
}

static gdouble
generate_midvalue(gdouble a, guint da, gdouble b, guint db,
                  FBMSynthState *fbmstate, const FBMSynthArgs *args)
{
    GwyRandGenSet *rngset = fbmstate->rngset;
    guint dtot = da + db;

    if (dtot >= (guint)args->hom_scale)
        return gwy_rand_gen_set_uniform(rngset, 0, fbmstate->hom_sigma);
    else {
        const gdouble *H_powers = fbmstate->H_powers;
        gdouble daH = H_powers[da], dbH = H_powers[db], dtotH = H_powers[dtot];
        gdouble da2 = da*da, db2 = db*db, dtot2 = dtot*dtot;
        gdouble mid = (a*db + b*da)/dtot;
        gdouble sigma2 = 0.5*(daH*daH + dbH*dbH
                              - dtotH*dtotH*(da2 + db2)/dtot2);
        gdouble sigma = sqrt(sigma2);

        if (args->distribution == FBM_DISTRIBUTION_UNIFORM)
            return mid + gwy_rand_gen_set_uniform(rngset, 0, sigma);
        if (args->distribution == FBM_DISTRIBUTION_GAUSSIAN)
            return mid + gwy_rand_gen_set_gaussian(rngset, 0, sigma);
        if (args->distribution == FBM_DISTRIBUTION_EXPONENTIAL)
            return mid + gwy_rand_gen_set_exponential(rngset, 0, sigma);
        if (args->distribution == FBM_DISTRIBUTION_POWER) {
            GRand *rng = gwy_rand_gen_set_rng(rngset, 0);
            gdouble r = 1.0/pow(g_rand_double(rng), 1.0/args->power) - 1.0;
            if (g_rand_boolean(rng))
                return mid + sigma*r;
            else
                return mid - sigma*r;
        }
        g_return_val_if_reached(0.0);
    }
}

static void
recurse(FBMSynthState *fbmstate, const FBMSynthArgs *args,
        guint xlow, guint ylow, guint xhigh, guint yhigh, guint depth)
{
    gdouble *data = fbmstate->field->data;
    gboolean *visited = fbmstate->visited;
    guint xres = fbmstate->xres;

    if (xhigh - xlow + (depth % 2) > yhigh - ylow) {
        guint xc = (xlow + xhigh)/2;
        guint k;

        k = ylow*xres + xc;
        if (!visited[k]) {
            data[k] = generate_midvalue(data[ylow*xres + xlow], xc - xlow,
                                        data[ylow*xres + xhigh], xhigh - xc,
                                        fbmstate, args);
            visited[k] = TRUE;
        }

        k = yhigh*xres + xc;
        data[k] = generate_midvalue(data[yhigh*xres + xlow], xc - xlow,
                                    data[yhigh*xres + xhigh], xhigh - xc,
                                    fbmstate, args);
        visited[k] = TRUE;

        if (yhigh - ylow > 1 || xc - xlow > 1)
            recurse(fbmstate, args, xlow, ylow, xc, yhigh, depth+1);
        if (yhigh - ylow > 1 || xhigh - xc > 1)
            recurse(fbmstate, args, xc, ylow, xhigh, yhigh, depth+1);
    }
    else {
        guint yc = (ylow + yhigh)/2;
        guint k;

        k = yc*xres + xlow;
        if (!visited[k]) {
            data[k] = generate_midvalue(data[ylow*xres + xlow], yc - ylow,
                                        data[yhigh*xres + xlow], yhigh - yc,
                                        fbmstate, args);
            visited[k] = TRUE;
        }

        k = yc*xres + xhigh;
        data[k] = generate_midvalue(data[ylow*xres + xhigh], yc - ylow,
                                    data[yhigh*xres + xhigh], yhigh - yc,
                                    fbmstate, args);
        visited[k] = TRUE;

        if (xhigh - xlow > 1 || yc - ylow > 1)
            recurse(fbmstate, args, xlow, ylow, xhigh, yc, depth+1);
        if (xhigh - xlow > 1 || yhigh - yc > 1)
            recurse(fbmstate, args, xlow, yc, xhigh, yhigh, depth+1);
    }
}

static void
fbm_synth_do(const FBMSynthArgs *args,
             const GwyDimensionArgs *dimsargs,
             GwyDataField *dfield)
{
    FBMSynthState *fbmstate;
    GwyDataField *addfield = dfield;
    gdouble mag, rms;

    if (dimsargs->add)
        addfield = gwy_data_field_new_alike(dfield, FALSE);

    fbmstate = fbm_synth_state_new(addfield, args);
    initialise(fbmstate);
    recurse(fbmstate, args,
            0, 0, fbmstate->xres-1, fbmstate->yres-1, 0);
    gwy_data_field_invalidate(addfield);
    fbm_synth_state_free(fbmstate);

    mag = pow10(dimsargs->zpow10);
    rms = gwy_data_field_get_rms(addfield);
    if (rms)
        gwy_data_field_multiply(addfield, args->sigma*mag/rms);

    if (dimsargs->add) {
        gwy_data_field_sum_fields(dfield, dfield, addfield);
        g_object_unref(addfield);
    }
}

static const gchar prefix[]           = "/module/fbm_synth";
static const gchar active_page_key[]  = "/module/fbm_synth/active_page";
static const gchar update_key[]       = "/module/fbm_synth/update";
static const gchar randomize_key[]    = "/module/fbm_synth/randomize";
static const gchar seed_key[]         = "/module/fbm_synth/seed";
static const gchar H_key[]            = "/module/fbm_synth/H";
static const gchar hom_scale_key[]    = "/module/fbm_synth/hom_scale";
static const gchar distribution_key[] = "/module/fbm_synth/distribution";
static const gchar power_key[]        = "/module/fbm_synth/power";
static const gchar sigma_key[]        = "/module/fbm_synth/sigma";

static void
fbm_synth_sanitize_args(FBMSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = !!args->update;
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->H = CLAMP(args->H, 0.001, 0.999);
    args->hom_scale = CLAMP(args->hom_scale, 2, 8192);
    args->distribution = MIN(args->distribution, FBM_DISTRIBUTION_NTYPES-1);
    args->power = CLAMP(args->power, 2.01, 12.0);
    args->sigma = CLAMP(args->sigma, 0.001, 10000.0);
}

static void
fbm_synth_load_args(GwyContainer *container,
                      FBMSynthArgs *args,
                      GwyDimensionArgs *dimsargs)
{
    *args = fbm_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_double_by_name(container, H_key, &args->H);
    gwy_container_gis_int32_by_name(container, hom_scale_key, &args->hom_scale);
    gwy_container_gis_enum_by_name(container, distribution_key,
                                   &args->distribution);
    gwy_container_gis_double_by_name(container, power_key, &args->power);
    gwy_container_gis_double_by_name(container, sigma_key, &args->sigma);
    fbm_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
fbm_synth_save_args(GwyContainer *container,
                      const FBMSynthArgs *args,
                      const GwyDimensionArgs *dimsargs)
{
    gwy_container_set_int32_by_name(container, active_page_key,
                                    args->active_page);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
    gwy_container_set_double_by_name(container, H_key, args->H);
    gwy_container_set_int32_by_name(container, hom_scale_key, args->hom_scale);
    gwy_container_set_enum_by_name(container, distribution_key,
                                   args->distribution);
    gwy_container_set_double_by_name(container, power_key, args->power);
    gwy_container_set_double_by_name(container, sigma_key, args->sigma);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
