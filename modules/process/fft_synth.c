/*
 *  @(#) $Id$
 *  Copyright (C) 2007,2009 David Necas (Yeti).
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
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/inttrans.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define FFT_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 320,
};

enum {
    RESPONSE_RESET = 1,
};

enum {
    PAGE_DIMENSIONS = 0,
    PAGE_GENERATOR = 1,
    PAGE_NPAGES
};

typedef struct {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;
    gdouble freq_min;
    gdouble freq_max;
    gdouble sigma;
    gboolean gauss_enable;
    gdouble gauss_tau;
    gboolean power_enable;
    gdouble power_p;
} FFTSynthArgs;

typedef struct {
    FFTSynthArgs *args;
    GwyDimensions *dims;
    gdouble pxsize;
    GwySIValueFormat *freqvf;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkObject *sigma;
    GtkWidget *sigma_units;
    GtkObject *freq_min;
    GtkWidget *freq_min_value;
    GtkWidget *freq_min_units;
    GtkObject *freq_max;
    GtkWidget *freq_max_value;
    GtkWidget *freq_max_units;
    GtkWidget *gauss_enable;
    GtkObject *gauss_tau;
    GtkWidget *gauss_tau_value;
    GtkWidget *gauss_tau_units;
    GtkWidget *power_enable;
    GtkObject *power_p;
    GwyContainer *mydata;

    GwyDataField *in_re;
    GwyDataField *in_im;
    GwyDataField *out_im;
    gboolean in_init;
} FFTSynthControls;

static gboolean   module_register        (void);
static void       fft_synth              (GwyContainer *data,
                                          GwyRunType run);
static void       run_noninteractive     (FFTSynthArgs *args,
                                          const GwyDimensionArgs *dimsargs,
                                          GwyContainer *data,
                                          GwyDataField *dfield,
                                          gint oldid,
                                          GQuark quark);
static gboolean   fft_synth_dialog       (FFTSynthArgs *args,
                                          GwyDimensionArgs *dimsargs,
                                          GwyContainer *data,
                                          GwyDataField *dfield,
                                          gint id);
static void       update_controls        (FFTSynthControls *controls,
                                          FFTSynthArgs *args);
static GtkWidget* random_seed_new        (GtkAdjustment *adj);
static GtkWidget* randomize_new          (gboolean *randomize);
static GtkWidget* instant_updates_new    (GtkWidget **update,
                                          GtkWidget **instant,
                                          gboolean *state);
static void       page_switched          (FFTSynthControls *controls,
                                          GtkNotebookPage *page,
                                          gint pagenum);
static void       seed_changed           (FFTSynthControls *controls,
                                          GtkAdjustment *adj);
static void       randomize_seed         (GtkAdjustment *adj);
static void       sigma_changed          (FFTSynthControls *controls,
                                          GtkAdjustment *adj);
static void       freq_min_changed       (FFTSynthControls *controls,
                                          GtkAdjustment *adj);
static void       update_freq_min_value  (FFTSynthControls *controls);
static void       freq_max_changed       (FFTSynthControls *controls,
                                          GtkAdjustment *adj);
static void       update_freq_max_value  (FFTSynthControls *controls);
static void       gauss_enable_changed   (FFTSynthControls *controls,
                                          GtkToggleButton *button);
static void       gauss_tau_changed      (FFTSynthControls *controls,
                                          GtkAdjustment *adj);
static void       update_gauss_tau_value (FFTSynthControls *controls);
static void       power_enable_changed   (FFTSynthControls *controls,
                                          GtkToggleButton *button);
static void       power_p_changed        (FFTSynthControls *controls,
                                          GtkAdjustment *adj);
static void       update_value_label     (GtkLabel *label,
                                          const GwySIValueFormat *vf,
                                          gdouble value);
static void       fft_synth_invalidate   (FFTSynthControls *controls);
static void       preview                (FFTSynthControls *controls);
static void       fft_synth_do           (const FFTSynthArgs *args,
                                          GwyDataField *in_re,
                                          GwyDataField *in_im,
                                          GwyDataField *out_re,
                                          GwyDataField *out_im);
static void       fft_synth_load_args    (GwyContainer *container,
                                          FFTSynthArgs *args,
                                          GwyDimensionArgs *dimsargs);
static void       fft_synth_save_args    (GwyContainer *container,
                                          const FFTSynthArgs *args,
                                          const GwyDimensionArgs *dimsargs);

static const FFTSynthArgs fft_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    0.0, G_SQRT2*G_PI,
    1.0,
    FALSE, 10.0,
    FALSE, 1.5,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates random surfaces using spectral synthesis."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fft_synth",
                              (GwyProcessFunc)&fft_synth,
                              N_("/S_ynthetic/_Spectral..."),
                              NULL,
                              FFT_SYNTH_RUN_MODES,
                              0,
                              N_("Generate surface using spectral synthesis"));

    return TRUE;
}

static void
fft_synth(GwyContainer *data, GwyRunType run)
{
    FFTSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & FFT_SYNTH_RUN_MODES);
    fft_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || fft_synth_dialog(&args, &dimsargs, data, dfield, id))
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);

    if (run == GWY_RUN_INTERACTIVE)
        fft_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(FFTSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint oldid,
                   GQuark quark)
{
    GwyDataField *in_re, *in_im, *out_re, *out_im;
    GwySIUnit *siunit;
    gboolean replace = dimsargs->replace && dfield;
    gdouble mag;
    gint newid;

    if (args->randomize)
        args->seed = g_random_int() & 0x7fffffff;

    mag = pow10(dimsargs->xypow10) * dimsargs->measure;
    if (replace)
        gwy_app_undo_qcheckpointv(data, 1, &quark);
    in_re = gwy_data_field_new(dimsargs->xres, dimsargs->yres,
                               mag*dimsargs->xres, mag*dimsargs->yres,
                               FALSE);
    in_im = gwy_data_field_new_alike(in_re, FALSE);
    out_re = replace ? dfield : gwy_data_field_new_alike(in_re, FALSE);
    out_im = gwy_data_field_new_alike(in_re, FALSE);
    fft_synth_do(args, in_re, in_im, out_re, out_im);
    g_object_unref(in_re);
    g_object_unref(in_im);
    g_object_unref(out_im);

    mag = gwy_data_field_get_rms(out_re);
    if (mag)
        gwy_data_field_multiply(out_re,
                                pow10(dimsargs->zpow10)*args->sigma/mag);

    if (dimsargs->replace)
        gwy_data_field_data_changed(out_re);
    else {
        siunit = gwy_data_field_get_si_unit_xy(out_re);
        gwy_si_unit_set_from_string(siunit, dimsargs->xyunits);

        siunit = gwy_data_field_get_si_unit_z(out_re);
        gwy_si_unit_set_from_string(siunit, dimsargs->zunits);

        if (data) {
            newid = gwy_app_data_browser_add_data_field(out_re, data, TRUE);
            gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                    GWY_DATA_ITEM_GRADIENT,
                                    0);
        }
        else {
            newid = 0;
            data = gwy_container_new();
            gwy_container_set_object(data, gwy_app_get_data_key_for_id(newid),
                                     out_re);
            gwy_app_data_browser_add(data);
            gwy_app_data_browser_reset_visibility(data,
                                                  GWY_VISIBILITY_RESET_SHOW_ALL);
            g_object_unref(data);
        }

        gwy_app_set_data_field_title(data, newid, _("Generated"));
        g_object_unref(out_re);
    }
}

static gboolean
fft_synth_dialog(FFTSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *spin;
    FFTSynthControls controls;
    GwyDataField *dfield;
    gint response;
    GwyPixmapLayer *layer;
    gint row;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    controls.pxsize = 1.0;
    dialog = gtk_dialog_new_with_buttons(_("Spectral Synthesis"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
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
                                FALSE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    if (data)
        gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 NULL);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);

    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox),
                       instant_updates_new(&controls.update_now,
                                           &controls.update, &args->update),
                       FALSE, FALSE, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(fft_synth_invalidate), &controls);
    g_signal_connect_swapped(controls.update_now, "clicked",
                             G_CALLBACK(preview), &controls);

    controls.seed = gtk_adjustment_new(args->seed, 1, 0x7fffffff, 1, 10, 0);
    gtk_box_pack_start(GTK_BOX(vbox),
                       random_seed_new(GTK_ADJUSTMENT(controls.seed)),
                       FALSE, FALSE, 0);
    g_signal_connect_swapped(controls.seed, "value-changed",
                             G_CALLBACK(seed_changed), &controls);

    controls.randomize = randomize_new(&args->randomize);
    gtk_box_pack_start(GTK_BOX(vbox), controls.randomize, FALSE, FALSE, 0);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(hbox), notebook, FALSE, FALSE, 4);
    g_signal_connect_swapped(notebook, "switch-page",
                             G_CALLBACK(page_switched), &controls);

    controls.dims = gwy_dimensions_new(dimsargs, dfield_template);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             gwy_dimensions_get_widget(controls.dims),
                             gtk_label_new(_("Dimensions")));

    table = gtk_table_new(12, 5, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.sigma = gtk_adjustment_new(args->sigma,
                                        0.0001, 10000.0, 0.1, 1.0, 0);
    spin = gwy_table_attach_hscale(table, row, _("_RMS:"), "",
                                   controls.sigma, GWY_HSCALE_LOG);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 4);
    controls.sigma_units = gwy_table_hscale_get_units(controls.sigma);
    g_signal_connect_swapped(controls.sigma, "value-changed",
                             G_CALLBACK(sigma_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 12);
    row++;

    controls.freq_min = gtk_adjustment_new(args->freq_min,
                                           0.0, G_SQRT2*G_PI, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("M_inimum frequency:"),
                            "px<sup>-1</sup>",
                            controls.freq_min, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.freq_min, "value-changed",
                             G_CALLBACK(freq_min_changed), &controls);
    row++;

    controls.freq_min_value = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.freq_min_value), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.freq_min_value,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    controls.freq_min_units = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.freq_min_units), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.freq_min_units,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.freq_max = gtk_adjustment_new(args->freq_max,
                                           0.0, G_SQRT2*G_PI, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("Ma_ximum frequency:"),
                            "px<sup>-1</sup>",
                            controls.freq_max, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.freq_max, "value-changed",
                             G_CALLBACK(freq_max_changed), &controls);
    row++;

    controls.freq_max_value = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.freq_max_value), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.freq_max_value,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    controls.freq_max_units = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.freq_max_units), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.freq_max_units,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 12);
    row++;

    controls.gauss_enable
        = gtk_check_button_new_with_mnemonic(_("Enable _Gaussian multiplier"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.gauss_enable),
                                 args->gauss_enable);
    gtk_table_attach(GTK_TABLE(table), controls.gauss_enable,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.gauss_enable, "toggled",
                             G_CALLBACK(gauss_enable_changed), &controls);
    row++;

    controls.gauss_tau = gtk_adjustment_new(args->gauss_tau,
                                            1.0, 1000.0, 0.1, 10.0, 0);
    gwy_table_attach_hscale(table, row, _("_Autocorrelation length:"), "px",
                            controls.gauss_tau, GWY_HSCALE_LOG);
    gwy_table_hscale_set_sensitive(controls.gauss_tau, args->gauss_enable);
    g_signal_connect_swapped(controls.gauss_tau, "value-changed",
                             G_CALLBACK(gauss_tau_changed), &controls);
    row++;

    controls.gauss_tau_value = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.gauss_tau_value), 1.0, 0.5);
    gtk_widget_set_sensitive(controls.gauss_tau_value, args->gauss_enable);
    gtk_table_attach(GTK_TABLE(table), controls.gauss_tau_value,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    controls.gauss_tau_units = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.gauss_tau_units), 0.0, 0.5);
    gtk_widget_set_sensitive(controls.gauss_tau_units, args->gauss_enable);
    gtk_table_attach(GTK_TABLE(table), controls.gauss_tau_units,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 12);
    row++;

    controls.power_enable
        = gtk_check_button_new_with_mnemonic(_("Enable _power multiplier"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.power_enable),
                                 args->power_enable);
    gtk_table_attach(GTK_TABLE(table), controls.power_enable,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.power_enable, "toggled",
                             G_CALLBACK(power_enable_changed), &controls);
    row++;

    controls.power_p = gtk_adjustment_new(args->power_p,
                                          0.0, 5.0, 0.01, 0.1, 0);
    spin = gwy_table_attach_hscale(table, row, _("Po_wer:"), NULL,
                                   controls.power_p, 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    gwy_table_hscale_set_sensitive(controls.power_p, args->power_enable);
    g_signal_connect_swapped(controls.power_p, "value-changed",
                             G_CALLBACK(power_p_changed), &controls);
    row++;

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    fft_synth_invalidate(&controls);

    while (TRUE) {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_OK:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            gwy_dimensions_free(controls.dims);
            gwy_object_unref(controls.in_re);
            gwy_object_unref(controls.in_im);
            gwy_object_unref(controls.out_im);
            if (controls.freqvf)
                gwy_si_unit_value_format_free(controls.freqvf);
            return response == GTK_RESPONSE_OK;
            break;

            case RESPONSE_RESET:
            {
                gboolean temp = args->update;
                gint temp2 = args->active_page;
                *args = fft_synth_defaults;
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
}

static void
update_controls(FFTSynthControls *controls,
                FFTSynthArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->sigma), args->sigma);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->freq_min),
                             args->freq_min);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->freq_max),
                             args->freq_max);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->gauss_tau),
                             args->gauss_tau);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->power_p), args->power_p);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->gauss_enable),
                                 args->gauss_enable);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->power_enable),
                                 args->power_enable);
}

static void
toggle_update_boolean(GtkToggleButton *toggle,
                      gboolean *var)
{
    *var = gtk_toggle_button_get_active(toggle);
}

static void
toggle_make_insensitive(GtkToggleButton *toggle,
                        GtkWidget *widget)
{
    gtk_widget_set_sensitive(widget, !gtk_toggle_button_get_active(toggle));
}

static GtkWidget*
instant_updates_new(GtkWidget **update,
                    GtkWidget **instant,
                    gboolean *state)
{
    GtkWidget *hbox;

    hbox = gtk_hbox_new(FALSE, 6);

    *update = gtk_button_new_with_mnemonic(_("_Update"));
    gtk_widget_set_sensitive(*update, !*state);
    gtk_box_pack_start(GTK_BOX(hbox), *update, FALSE, FALSE, 0);

    *instant = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(*instant), *state);
    gtk_box_pack_start(GTK_BOX(hbox), *instant, FALSE, FALSE, 0);
    g_signal_connect(*instant, "toggled",
                     G_CALLBACK(toggle_update_boolean), state);
    g_signal_connect(*instant, "toggled",
                     G_CALLBACK(toggle_make_insensitive), *update);

    return hbox;
}

static GtkWidget*
random_seed_new(GtkAdjustment *adj)
{
    GtkWidget *hbox, *button, *label, *spin;

    hbox = gtk_hbox_new(FALSE, 6);

    label = gtk_label_new_with_mnemonic(_("R_andom seed:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    spin = gtk_spin_button_new(adj, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

    button = gtk_button_new_with_mnemonic(gwy_sgettext("seed|_New"));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(randomize_seed), adj);

    return hbox;
}

static GtkWidget*
randomize_new(gboolean *randomize)
{
    GtkWidget *button = gtk_check_button_new_with_mnemonic(_("Randomize"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *randomize);
    g_signal_connect(button, "toggled",
                     G_CALLBACK(toggle_update_boolean), randomize);
    return button;
}

static void
page_switched(FFTSynthControls *controls,
              G_GNUC_UNUSED GtkNotebookPage *page,
              gint pagenum)
{
    if (controls->in_init)
        return;

    controls->args->active_page = pagenum;

    if (pagenum == PAGE_GENERATOR) {
        GwyDimensions *dims = controls->dims;
        GwySIUnit *units;

        if (controls->sigma_units)
            gtk_label_set_markup(GTK_LABEL(controls->sigma_units),
                                 dims->zvf->units);
        controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
        units = gwy_si_unit_power(dims->xysiunit, -1, NULL);
        controls->freqvf
            = gwy_si_unit_get_format_with_digits(units,
                                                 GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                 1.0/controls->pxsize, 4,
                                                 controls->freqvf);
        g_object_unref(units);

        gtk_label_set_markup(GTK_LABEL(controls->freq_min_units),
                             controls->freqvf->units);
        gtk_label_set_markup(GTK_LABEL(controls->freq_max_units),
                             controls->freqvf->units);
        gtk_label_set_markup(GTK_LABEL(controls->gauss_tau_units),
                             dims->xyvf->units);

        update_freq_min_value(controls);
        update_freq_max_value(controls);
        update_gauss_tau_value(controls);
    }
}

static void
seed_changed(FFTSynthControls *controls,
             GtkAdjustment *adj)
{
    controls->args->seed = gwy_adjustment_get_int(adj);
    fft_synth_invalidate(controls);
}

static void
randomize_seed(GtkAdjustment *adj)
{
    /* Use the GLib's global PRNG for seeding */
    gtk_adjustment_set_value(adj, g_random_int() & 0x7fffffff);
}

static void
sigma_changed(FFTSynthControls *controls,
              GtkAdjustment *adj)
{
    controls->args->sigma = gtk_adjustment_get_value(adj);
    /* Sigma is not observable on the preview, don't do anything. */
}

static void
freq_min_changed(FFTSynthControls *controls,
                 GtkAdjustment *adj)
{
    controls->args->freq_min = gtk_adjustment_get_value(adj);
    update_freq_min_value(controls);
    fft_synth_invalidate(controls);
}

static void
update_freq_min_value(FFTSynthControls *controls)
{
    update_value_label(GTK_LABEL(controls->freq_min_value), controls->freqvf,
                       controls->args->freq_min/controls->pxsize);
}

static void
freq_max_changed(FFTSynthControls *controls,
                 GtkAdjustment *adj)
{
    controls->args->freq_max = gtk_adjustment_get_value(adj);
    update_freq_max_value(controls);
    fft_synth_invalidate(controls);
}

static void
update_freq_max_value(FFTSynthControls *controls)
{
    update_value_label(GTK_LABEL(controls->freq_max_value), controls->freqvf,
                       controls->args->freq_max/controls->pxsize);
}

static void
gauss_enable_changed(FFTSynthControls *controls,
                     GtkToggleButton *button)
{
    FFTSynthArgs *args = controls->args;

    args->gauss_enable = gtk_toggle_button_get_active(button);
    gwy_table_hscale_set_sensitive(controls->gauss_tau, args->gauss_enable);
    gtk_widget_set_sensitive(controls->gauss_tau_value, args->gauss_enable);
    gtk_widget_set_sensitive(controls->gauss_tau_units, args->gauss_enable);
    fft_synth_invalidate(controls);
}

static void
gauss_tau_changed(FFTSynthControls *controls,
                  GtkAdjustment *adj)
{
    controls->args->gauss_tau = gtk_adjustment_get_value(adj);
    update_gauss_tau_value(controls);
    fft_synth_invalidate(controls);
}

static void
update_gauss_tau_value(FFTSynthControls *controls)
{
    update_value_label(GTK_LABEL(controls->gauss_tau_value),
                       controls->dims->xyvf,
                       controls->args->gauss_tau*controls->pxsize);
}

static void
power_enable_changed(FFTSynthControls *controls,
                     GtkToggleButton *button)
{
    controls->args->power_enable = gtk_toggle_button_get_active(button);
    gwy_table_hscale_set_sensitive(controls->power_p,
                                   controls->args->power_enable);
    fft_synth_invalidate(controls);
}

static void
power_p_changed(FFTSynthControls *controls,
                  GtkAdjustment *adj)
{
    controls->args->power_p = gtk_adjustment_get_value(adj);
    fft_synth_invalidate(controls);
}

static void
update_value_label(GtkLabel *label,
                   const GwySIValueFormat *vf,
                   gdouble value)
{
    gchar buf[32];

    g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, value/vf->magnitude);
    gtk_label_set_markup(label, buf);
}

static void
fft_synth_invalidate(FFTSynthControls *controls)
{
    /* create preview if instant updates are on */
    if (controls->args->update && !controls->in_init)
        preview(controls);
}

#ifdef HAVE_SINCOS
#define _gwy_sincos sincos
#else
static inline void
_gwy_sincos(gdouble x, gdouble *s, gdouble *c)
{
    *s = sin(x);
    *c = cos(x);
}
#endif

static void
preview(FFTSynthControls *controls)
{
    const FFTSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    if (!controls->in_re) {
        controls->in_re = gwy_data_field_new_alike(dfield, FALSE);
        controls->in_im = gwy_data_field_new_alike(dfield, FALSE);
        controls->out_im = gwy_data_field_new_alike(dfield, FALSE);
    }

    fft_synth_do(args, controls->in_re, controls->in_im,
                 dfield, controls->out_im);
}

static void
fft_synth_do(const FFTSynthArgs *args,
             GwyDataField *in_re,
             GwyDataField *in_im,
             GwyDataField *out_re,
             GwyDataField *out_im)
{
    GRand *rng;
    gdouble *re, *im;
    gdouble power_p, gauss_tau, freq_min, freq_max;
    gboolean power_enable, gauss_enable;
    gint xres, yres, i, j, k;

    rng = g_rand_new();
    g_rand_set_seed(rng, args->seed);

    re = gwy_data_field_get_data(in_re);
    im = gwy_data_field_get_data(in_im);

    xres = gwy_data_field_get_xres(out_re);
    yres = gwy_data_field_get_yres(out_re);

    /* Optimization hints */
    freq_min = args->freq_min/G_PI;
    freq_max = args->freq_max/G_PI;
    power_enable = args->power_enable;
    power_p = args->power_p;
    gauss_enable = args->gauss_enable;
    gauss_tau = args->gauss_tau*G_PI/2.0;

    k = 0;
    for (i = 0; i < yres; i++) {
        gdouble y = (i <= yres/2 ? i : yres-i)/(0.5*yres);
        for (j = 0; j < xres; j++) {
            gdouble x = (j <= xres/2 ? j : xres-j)/(0.5*xres);
            gdouble r = hypot(x, y);

            if (r < freq_min || r > freq_max) {
                /* XXX: This is necessary for stability! */
                g_rand_double(rng);
                g_rand_double(rng);
                re[k] = im[k] = 0.0;
            }
            else {
                gdouble f = g_rand_double(rng);
                gdouble phi = 2.0*G_PI*g_rand_double(rng);
                gdouble s, c;

                if (power_enable)
                    f /= pow(r, power_p);
                if (gauss_enable) {
                    gdouble t = r*gauss_tau;
                    f /= exp(0.5*t*t);
                }

                _gwy_sincos(phi, &s, &c);
                re[k] = f*s;
                im[k] = f*c;
            }
            k++;
        }
    }
    re[0] = im[0] = 0.0;

    gwy_data_field_2dfft_raw(in_re, in_im, out_re, out_im,
                             GWY_TRANSFORM_DIRECTION_BACKWARD);

    g_rand_free(rng);
    gwy_data_field_data_changed(out_re);
    gwy_data_field_data_changed(out_im);
}

static const gchar prefix[]           = "/module/fft_synth";
static const gchar active_page_key[]  = "/module/fft_synth/active_page";
static const gchar update_key[]       = "/module/fft_synth/update";
static const gchar randomize_key[]    = "/module/fft_synth/randomize";
static const gchar seed_key[]         = "/module/fft_synth/seed";
static const gchar freq_min_key[]     = "/module/fft_synth/freq_min";
static const gchar freq_max_key[]     = "/module/fft_synth/freq_max";
static const gchar sigma_key[]        = "/module/fft_synth/sigma";
static const gchar gauss_enable_key[] = "/module/fft_synth/gauss_enable";
static const gchar gauss_tau_key[]    = "/module/fft_synth/gauss_tau";
static const gchar power_enable_key[] = "/module/fft_synth/power_enable";
static const gchar power_p_key[]      = "/module/fft_synth/power_p";

static void
fft_synth_sanitize_args(FFTSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = !!args->update;
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->freq_min = CLAMP(args->freq_min, 0.0, G_SQRT2*G_PI);
    args->freq_max = CLAMP(args->freq_max, 0.0, G_SQRT2*G_PI);
    args->sigma = CLAMP(args->sigma, 0.001, 10000.0);
    args->gauss_enable = !!args->gauss_enable;
    args->gauss_tau = CLAMP(args->gauss_tau, 1.0, 1000.0);
    args->power_enable = !!args->power_enable;
    args->power_p = CLAMP(args->power_p, 0.0, 5.0);
}

static void
fft_synth_load_args(GwyContainer *container,
                    FFTSynthArgs *args,
                    GwyDimensionArgs *dimsargs)
{
    *args = fft_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_double_by_name(container, freq_min_key, &args->freq_min);
    gwy_container_gis_double_by_name(container, freq_max_key, &args->freq_max);
    gwy_container_gis_double_by_name(container, sigma_key, &args->sigma);
    gwy_container_gis_boolean_by_name(container, gauss_enable_key,
                                      &args->gauss_enable);
    gwy_container_gis_double_by_name(container, gauss_tau_key,
                                     &args->gauss_tau);
    gwy_container_gis_boolean_by_name(container, power_enable_key,
                                      &args->power_enable);
    gwy_container_gis_double_by_name(container, power_p_key, &args->power_p);
    fft_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
fft_synth_save_args(GwyContainer *container,
                    const FFTSynthArgs *args,
                    const GwyDimensionArgs *dimsargs)
{
    gwy_container_set_int32_by_name(container, active_page_key,
                                    args->active_page);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
    gwy_container_set_double_by_name(container, freq_min_key, args->freq_min);
    gwy_container_set_double_by_name(container, freq_max_key, args->freq_max);
    gwy_container_set_double_by_name(container, sigma_key, args->sigma);
    gwy_container_set_boolean_by_name(container, gauss_enable_key,
                                      args->gauss_enable);
    gwy_container_set_double_by_name(container, gauss_tau_key,
                                     args->gauss_tau);
    gwy_container_set_boolean_by_name(container, power_enable_key,
                                      args->power_enable);
    gwy_container_set_double_by_name(container, power_p_key, args->power_p);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
