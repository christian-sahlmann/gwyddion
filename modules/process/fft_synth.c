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
    RESPONSE_RESET   = 1,
    RESPONSE_PREVIEW = 2
};

typedef struct {
    guint32 seed;
    gboolean replace;
    gdouble freq_min;
    gdouble freq_max;
    gboolean gauss_enable;
    gdouble gauss_tau;
    gboolean power_enable;
    gdouble power_p;
    gdouble power_tau;
    gboolean update;
} FFTSynthArgs;

typedef struct {
    FFTSynthArgs *args;
    GwyDimensions *dims;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkObject *seed;
    GtkWidget *seed_new;
    GtkObject *freq_min;
    GtkObject *freq_max;
    GtkWidget *gauss_enable;
    GtkObject *gauss_tau;
    GtkWidget *power_enable;
    GtkObject *power_p;
    GtkObject *power_tau;
    GtkWidget *update;
    GwyContainer *mydata;

    GwyDataField *in_re;
    GwyDataField *in_im;
    GwyDataField *out_im;
    gboolean computed;
    gboolean in_init;
} FFTSynthControls;

static gboolean module_register                 (void);
static void     fft_synth                       (GwyContainer *data,
                                                 GwyRunType run);
static void     run_noninteractive              (FFTSynthArgs *args,
                                                 GwyDimensionArgs *dimsargs,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 gint id);
static void     fft_synth_dialog                (FFTSynthArgs *args,
                                                 GwyDimensionArgs *dimsargs,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 gint id);
static void     fft_synth_dialog_update_controls(FFTSynthControls *controls,
                                                 FFTSynthArgs *args);
static void     fft_synth_dialog_update_values  (FFTSynthControls *controls,
                                                 FFTSynthArgs *args);
static void     fft_synth_invalidate            (FFTSynthControls *controls);
static void     update_change_cb                (FFTSynthControls *controls);
static void     preview                         (FFTSynthControls *controls,
                                                 FFTSynthArgs *args);
static void     fft_synth_load_args             (GwyContainer *container,
                                                 FFTSynthArgs *args);
static void     fft_synth_save_args             (GwyContainer *container,
                                                 FFTSynthArgs *args);

static const FFTSynthArgs fft_synth_defaults = {
    42,
    FALSE,
    0.0, 1.0,
    FALSE, 0.1,
    FALSE, 1.5, 0.1,
    FALSE,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates random surfaces using spectral synthesis."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2007",
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
                              GWY_MENU_FLAG_DATA,
                              N_("Generate surface using spectral synthesis"));

    return TRUE;
}

static void
fft_synth(GwyContainer *data, GwyRunType run)
{
    FFTSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    gint id;

    g_return_if_fail(run & FFT_SYNTH_RUN_MODES);
    fft_synth_load_args(gwy_app_settings_get(), &args);
    gwy_clear(&dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, &dimsargs);
    gwy_dimensions_load_args(&dimsargs, gwy_app_settings_get(),
                             "/module/fft_synth");
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, &dimsargs, data, dfield, id);
    else {
        fft_synth_dialog(&args, &dimsargs, data, dfield, id);
        fft_synth_save_args(gwy_app_settings_get(), &args);
        gwy_dimensions_save_args(&dimsargs, gwy_app_settings_get(),
                                 "/module/fft_synth");
    }
    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(FFTSynthArgs *args,
                   GwyDimensionArgs *dimsargs,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint id)
{
    /*
    GwyDataField *mfield;

    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    mfield = create_mask_field(dfield);
    mark_fft_synth(dfield, mfield, args);
    gwy_container_set_object(data, mquark, mfield);
    g_object_unref(mfield);
    */
}

static void
fft_synth_dialog(FFTSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 GwyContainer *data,
                 GwyDataField *dfield,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *hbox2, *label, *notebook;
    FFTSynthControls controls;
    gint response;
    GwyPixmapLayer *layer;
    gdouble zoomval;
    gboolean temp;
    gint row;

    memset(&controls, 0, sizeof(FFTSynthControls));
    controls.in_init = TRUE;
    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Spectral Synthesis"),
                                         NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    dfield = gwy_data_field_new(dimsargs->xres, dimsargs->yres,
                                dimsargs->measure*dimsargs->xres,
                                dimsargs->measure*dimsargs->yres,
                                FALSE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
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
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);

    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_box_pack_start(GTK_BOX(vbox), controls.update, FALSE, FALSE, 0);
    /*
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_change_cb), &controls);
                             */

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(hbox), notebook, FALSE, FALSE, 4);

    controls.dims = gwy_dimensions_new(dimsargs);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             gwy_dimensions_get_widget(controls.dims),
                             gtk_label_new(_("Dimensions")));

    /* TODO: RMS */

    table = gtk_table_new(11, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.seed = gtk_adjustment_new(args->seed,
                                       1.0, 0xffffffff, 1.0, 10.0, 0);
    gwy_table_attach_hscale(table, row, _("Random seed:"), NULL,
                            controls.seed, GWY_HSCALE_NO_SCALE);

    controls.seed_new = gtk_button_new_with_mnemonic(_("_New Seed"));
    gtk_table_attach(GTK_TABLE(table), controls.seed_new,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.freq_min = gtk_adjustment_new(args->freq_min,
                                           0.0, 1.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row++, _("M_inimum frequency:"), NULL,
                            controls.freq_min, 0);
    row++;

    controls.freq_max = gtk_adjustment_new(args->freq_max,
                                           0.0, 1.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row++, _("Ma_ximum frequency:"), NULL,
                            controls.freq_max, 0);
    row++;

    controls.gauss_enable
        = gtk_check_button_new_with_mnemonic(_("Enable _Gaussian multiplier"));
    gtk_table_attach(GTK_TABLE(table), controls.gauss_enable,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.gauss_tau = gtk_adjustment_new(args->gauss_tau,
                                           0.0, 2.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row++, _("Correlation _length:"),
                            NULL,
                            controls.gauss_tau, 0);
    row++;

    controls.power_enable
        = gtk_check_button_new_with_mnemonic(_("Enable _power multiplier"));
    gtk_table_attach(GTK_TABLE(table), controls.power_enable,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.power_p = gtk_adjustment_new(args->power_p,
                                          0.0, 3.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row++, _("Po_wer:"), NULL,
                            controls.power_p, 0);
    row++;
    controls.power_tau = gtk_adjustment_new(args->power_tau,
                                           0.0, 2.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row++, _("Correlation lengt_h:"),
                            NULL,
                            controls.power_tau, 0);
    row++;

    fft_synth_invalidate(&controls);
    controls.in_init = FALSE;

    /* show initial preview if instant updates are on */
    if (args->update) {
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls.dialog),
                                          RESPONSE_PREVIEW, FALSE);
        preview(&controls, args);
    }

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            fft_synth_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            gwy_dimensions_free(controls.dims);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            temp = args->update;
            *args = fft_synth_defaults;
            args->update = temp;
            controls.in_init = TRUE;
            fft_synth_dialog_update_controls(&controls, args);
            controls.in_init = FALSE;
            preview(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            fft_synth_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    fft_synth_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);

    /*
    if (controls.computed) {
        gwy_app_undo_qcheckpointv(data, 1, &mquark);
        gwy_container_set_object(data, mquark, mfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        run_noninteractive(args, data, dfield, mquark);
    }
    */
    gwy_dimensions_free(controls.dims);
}

static void
fft_synth_dialog_update_controls(FFTSynthControls *controls,
                                  FFTSynthArgs *args)
{
    /*
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_high),
                             args->threshold_high);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_low),
                             args->threshold_low);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->min_len),
                             args->min_len);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->max_width),
                             args->max_width);
    gwy_radio_buttons_set_current(controls->type, args->type);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
                                 */
}

static void
fft_synth_dialog_update_values(FFTSynthControls *controls,
                                FFTSynthArgs *args)
{
    /*
    args->threshold_high
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_high));
    args->threshold_low
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_low));
    args->min_len = gwy_adjustment_get_int(controls->min_len);
    args->max_width = gwy_adjustment_get_int(controls->max_width);
    args->type = gwy_radio_buttons_get_current(controls->type);
    args->update
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));
        */
}

static void
fft_synth_invalidate(FFTSynthControls *controls)
{
    controls->computed = FALSE;

    /* create preview if instant updates are on */
    if (controls->args->update && !controls->in_init) {
        fft_synth_dialog_update_values(controls, controls->args);
        preview(controls, controls->args);
    }
}

static void
update_change_cb(FFTSynthControls *controls)
{
    controls->args->update
            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);

    if (controls->args->update)
        fft_synth_invalidate(controls);
}

static void
preview(FFTSynthControls *controls,
        FFTSynthArgs *args)
{
    GwyDataField *dfield;
    GRand *rng;
    gdouble *re, *im;
    gdouble x, y, f, phi;
    gint xres, yres, i, j;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    if (!controls->in_re) {
        controls->in_re = gwy_data_field_new_alike(dfield, FALSE);
        controls->in_im = gwy_data_field_new_alike(dfield, FALSE);
        controls->out_im = gwy_data_field_new_alike(dfield, FALSE);
    }

    rng = g_rand_new();
    g_rand_set_seed(rng, args->seed);

    re = gwy_data_field_get_data(controls->in_re);
    im = gwy_data_field_get_data(controls->in_im);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    for (i = 0; i < yres; i++) {
        y = (i <= yres/2 ? i : yres-i)/(yres/2.0);
        for (j = 0; j < xres; j++) {
            x = (j <= xres/2 ? j : xres-j)/(xres/2.0);
            f = pow(hypot(x, y) + 0.01, -3);
            f *= g_rand_double(rng);
            phi = 2.0*G_PI*g_rand_double(rng);
            re[i*xres + j] = f*sin(phi);
            im[i*xres + j] = f*cos(phi);
        }
    }

    gwy_data_field_2dfft_raw(controls->in_re, controls->in_im,
                             dfield, controls->out_im,
                             GWY_TRANSFORM_DIRECTION_BACKWARD);

    g_rand_free(rng);
    gwy_data_field_data_changed(dfield);

    controls->computed = TRUE;
}

/*
static const gchar type_key[]           = "/module/fft_synth/type";
static const gchar threshold_low_key[]  = "/module/fft_synth/threshold_low";
static const gchar threshold_high_key[] = "/module/fft_synth/threshold_high";
static const gchar min_len_key[]        = "/module/fft_synth/min_len";
static const gchar max_width_key[]      = "/module/fft_synth/max_width";
static const gchar update_key[]         = "/module/fft_synth/update";

static void
fft_synth_sanitize_args(FFTSynthArgs *args)
{
    args->type = CLAMP(args->type, FEATURES_POSITIVE, FEATURES_BOTH);
    args->threshold_low = MAX(args->threshold_low, 0.0);
    args->threshold_high = MAX(args->threshold_low, args->threshold_high);
    args->min_len = CLAMP(args->min_len, 1, MAX_LENGTH);
    args->max_width = CLAMP(args->max_width, 1, 16);
    args->update = !!args->update;
}

static void
fft_synth_load_args(GwyContainer *container,
                     FFTSynthArgs *args)
{
    *args = fft_synth_defaults;

    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    gwy_container_gis_double_by_name(container, threshold_high_key,
                                     &args->threshold_high);
    gwy_container_gis_double_by_name(container, threshold_low_key,
                                     &args->threshold_low);
    gwy_container_gis_int32_by_name(container, min_len_key, &args->min_len);
    gwy_container_gis_int32_by_name(container, max_width_key, &args->max_width);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    fft_synth_sanitize_args(args);
}

static void
fft_synth_save_args(GwyContainer *container,
                     FFTSynthArgs *args)
{
    gwy_container_set_enum_by_name(container, type_key, args->type);
    gwy_container_set_double_by_name(container, threshold_high_key,
                                     args->threshold_high);
    gwy_container_set_double_by_name(container, threshold_low_key,
                                     args->threshold_low);
    gwy_container_set_int32_by_name(container, min_len_key, args->min_len);
    gwy_container_set_int32_by_name(container, max_width_key, args->max_width);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
}
*/

static void
fft_synth_load_args(GwyContainer *container,
                    FFTSynthArgs *args)
{
    *args = fft_synth_defaults;
}

static void
fft_synth_save_args(GwyContainer *container,
                    FFTSynthArgs *args)
{
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
