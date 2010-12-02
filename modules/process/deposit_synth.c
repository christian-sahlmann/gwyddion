/*
 *  @(#) $Id: deposit_synth.c 11510 2010-11-04 15:07:43Z yeti-dn $
 *  Copyright (C) 2007,2009,2010 David Necas (Yeti).
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
#include <libprocess/arithmetic.h>
#include <libprocess/inttrans.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define DEPOSIT_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)


// 1. store link to original data
// 2. create result field
// 3. put copy of original or blank field to result
// 3. have function to do preview of result
// 4. run simulation with result field
// 5. repeat (3)

// N. have function to insert result to data browser or swap it for present channel
// N+1. run noninteractive or interactive with function N at end 


enum {
    PREVIEW_SIZE = 400,
    MAXN         = 5000,
};

enum {
    RESPONSE_RESET = 1,
};

enum {
    PAGE_DIMENSIONS = 0,
    PAGE_GENERATOR = 1,
    PAGE_NPAGES
};

enum {
    RES_TOO_FEW = -1,
    RES_TOO_MANY = -2,
    RES_TOO_SMALL = -3,
    RES_TOO_LARGE = -4
};

typedef struct {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;
    gdouble size;
    gdouble width;
    gdouble coverage;
    gint revise;
} DepositSynthArgs;

typedef struct {
    DepositSynthArgs *args;
    GwyDimensions *dims;
    gdouble pxsize;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkObject *size;
    GwySIValueFormat *format_size;
    GtkWidget *size_units;
    GtkObject *width;
    GwySIValueFormat *format_width;
    GtkWidget *width_units;
    GtkObject *coverage;
    GtkObject *revise;
    GtkWidget *message;
    GwyContainer *mydata;
    GwyDataField *original;
    gboolean data_done;
    GwyDataField *out;
    gboolean in_init;
    gulong sid;
} DepositSynthControls;

static gboolean      module_register       (void);
static void          deposit_synth             (GwyContainer *data,
                                            GwyRunType run);
static void          run_noninteractive    (DepositSynthArgs *args,
                                            const GwyDimensionArgs *dimsargs,
                                            GwyContainer *data,
                                            GwyDataField *dfield,
                                            gint oldid,
                                            GQuark quark);
static gboolean      deposit_synth_dialog      (DepositSynthArgs *args,
                                            GwyDimensionArgs *dimsargs,
                                            GwyContainer *data,
                                            GwyDataField *dfield,
                                            gint id,
                                            GQuark quark);
static GwyDataField* surface_for_preview   (GwyDataField *dfield,
                                            guint size);
static void          update_controls       (DepositSynthControls *controls,
                                            DepositSynthArgs *args);
static GtkWidget*    random_seed_new       (GtkAdjustment *adj);
static GtkWidget*    randomize_new         (gboolean *randomize);
static GtkWidget*    instant_updates_new   (GtkWidget **update,
                                            GtkWidget **instant,
                                            gboolean *state);
static void          page_switched         (DepositSynthControls *controls,
                                            GtkNotebookPage *page,
                                            gint pagenum);
static void          seed_changed          (DepositSynthControls *controls,
                                            GtkAdjustment *adj);
static void          randomize_seed        (GtkAdjustment *adj);
static void          size_changed         (DepositSynthControls *controls,
                                            GtkAdjustment *adj);
static void          width_changed      (DepositSynthControls *controls,
                                            GtkAdjustment *adj);
static void          coverage_changed      (DepositSynthControls *controls,
                                            GtkAdjustment *adj);
static void          revise_changed     (DepositSynthControls *controls,
                                            GtkAdjustment *adj);
static void          deposit_synth_invalidate  (DepositSynthControls *controls);
static gboolean      preview_gsource       (gpointer user_data);
static void          preview               (DepositSynthControls *controls);
static gint          deposit_synth_do      (const DepositSynthArgs *args,
                                            GwyDataField *dfield,
                                            GwyDataField *showfield,
                                            gboolean *success);
static void          deposit_synth_load_args   (GwyContainer *container,
                                            DepositSynthArgs *args,
                                            GwyDimensionArgs *dimsargs);
static void          deposit_synth_save_args   (GwyContainer *container,
                                            const DepositSynthArgs *args,
                                            const GwyDimensionArgs *dimsargs);
static inline gboolean gwy_data_field_inside(GwyDataField *data_field, gint i, gint j);


static const DepositSynthArgs deposit_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, FALSE,
    1, 1,
    10, 0,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates particles using simple dynamical model"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "Petr Klapetek",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("deposit_synth",
                              (GwyProcessFunc)&deposit_synth,
                              N_("/S_ynthetic/_Particles..."),
                              NULL,
                              DEPOSIT_SYNTH_RUN_MODES,
                              0,
                              N_("Generate particles using dynamical model"));

    return TRUE;
}

static void
deposit_synth(GwyContainer *data, GwyRunType run)
{
    DepositSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & DEPOSIT_SYNTH_RUN_MODES);
    deposit_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);
    else if (run == GWY_RUN_INTERACTIVE) {
        deposit_synth_dialog(&args, &dimsargs, data, dfield, id, quark);
        deposit_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);
    }

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(DepositSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint oldid,
                   GQuark quark)
{
    GwyDataField *out;
    GwySIUnit *siunit;
    GwyContainer *newdata;
    GtkWidget *dialog;
    gboolean replace = dimsargs->replace && dfield;
    gboolean add = dimsargs->add && dfield;
    gdouble mag; 
    gboolean success;
    gint newid, ndata;
    gchar message[50];

    if (args->randomize)
        args->seed = g_random_int() & 0x7fffffff;

    if (replace) {
        gwy_app_undo_qcheckpointv(data, 1, &quark);

            out = gwy_data_field_new_alike(dfield, FALSE);
        if (add && dfield)
            gwy_data_field_copy(dfield, out, FALSE);
        else
            gwy_data_field_fill(out, 0);
        
    }
    else {
        if (add && dfield) {
            out = gwy_data_field_new_alike(dfield, FALSE);
            gwy_data_field_copy(dfield, out, FALSE);
        }
        else {
            mag = pow10(dimsargs->xypow10) * dimsargs->measure;
            out = gwy_data_field_new(dimsargs->xres, dimsargs->yres,
                                        mag*dimsargs->xres, mag*dimsargs->yres,
                                        TRUE);

            siunit = gwy_data_field_get_si_unit_xy(out);
            gwy_si_unit_set_from_string(siunit, dimsargs->xyunits);

            siunit = gwy_data_field_get_si_unit_z(out);
            gwy_si_unit_set_from_string(siunit, dimsargs->zunits);
        }
    }

    gwy_app_wait_start(gwy_app_find_window_for_channel(data, oldid), "Starting...");
    ndata = deposit_synth_do(args, out, NULL, &success);
    gwy_app_wait_finish();

    if (ndata <=0) {
        if (ndata==RES_TOO_MANY) g_snprintf(message, sizeof(message), "Error: too many particles.");
        else if (ndata==RES_TOO_FEW) g_snprintf(message, sizeof(message), "Error: no particles.");
        else if (ndata==RES_TOO_LARGE) g_snprintf(message, sizeof(message), "Error: particles too large.");
        else if (ndata==RES_TOO_SMALL) g_snprintf(message, sizeof(message), "Error: particles too small.");
        dialog = gtk_message_dialog_new (gwy_app_find_window_for_channel(data, oldid),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", message);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

    } else {
        if (!success) {
            dialog = gtk_message_dialog_new (gwy_app_find_window_for_channel(data, oldid),
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_WARNING,
                                             GTK_BUTTONS_CLOSE,
                                             "Not all the particles could be deposited, try more revise steps.");
            gtk_dialog_run (GTK_DIALOG (dialog));
            gtk_widget_destroy (dialog);
        }
        if (replace) {
            gwy_data_field_copy(out, dfield, FALSE);
            gwy_data_field_data_changed(dfield);
        }
        else {
            if (data) {
                newid = gwy_app_data_browser_add_data_field(out, data, TRUE);
                gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                        GWY_DATA_ITEM_GRADIENT,
                                        0);
                gwy_app_set_data_field_title(data, newid, _("Generated"));

            }
            else {
                newid = 0;
                newdata = gwy_container_new();
                gwy_container_set_object(newdata, gwy_app_get_data_key_for_id(newid),
                                         out);
                gwy_app_data_browser_add(newdata);
                gwy_app_data_browser_reset_visibility(newdata,
                                                      GWY_VISIBILITY_RESET_SHOW_ALL);
                g_object_unref(newdata);
                gwy_app_set_data_field_title(newdata, newid, _("Generated"));

            }

        }
    }

    g_object_unref(out);
}

static gboolean
deposit_synth_dialog(DepositSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id, GQuark quark)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *spin;
    DepositSynthControls controls;
    GwyContainer *newdata;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row, newid;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    controls.pxsize = 1.0;
    dialog = gtk_dialog_new_with_buttons(_("Particle Generation"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    controls.original = dfield_template;
    controls.data_done = FALSE;
    
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


    if (data)
        gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                0);
    if (dfield_template) {
        dfield = surface_for_preview(controls.original, PREVIEW_SIZE);
        gwy_data_field_data_changed(dfield);
    //    printf("surface from original put to dfield\n");
    } 

    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
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
                             G_CALLBACK(deposit_synth_invalidate), &controls);
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
    if (controls.dims->add)
        g_signal_connect_swapped(controls.dims->add, "toggled",
                                 G_CALLBACK(deposit_synth_invalidate), &controls);

    table = gtk_table_new(12 + (dfield_template ? 1 : 0), 5, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.size = gtk_adjustment_new(args->size/pow10(controls.dims->args->xypow10),
                                        0, 100.0, 0.1, 1.0, 0);
    spin = gwy_table_attach_hscale(table, row, _("R_adius:"), controls.dims->args->xyunits,
                                   controls.size, GWY_HSCALE_DEFAULT);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 4);
    controls.size_units = gwy_table_hscale_get_units(controls.size);
    g_signal_connect_swapped(controls.size, "value-changed",
                             G_CALLBACK(size_changed), &controls);
    row++;

    controls.width = gtk_adjustment_new(args->width/pow10(controls.dims->args->xypow10),
                                        0, 100.0, 0.1, 1.0, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Width:"), controls.dims->args->xyunits,
                                   controls.width, GWY_HSCALE_DEFAULT);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 4);
    controls.width_units = gwy_table_hscale_get_units(controls.width);
    g_signal_connect_swapped(controls.width, "value-changed",
                             G_CALLBACK(width_changed), &controls);
    row++;

    controls.coverage = gtk_adjustment_new(args->coverage,
                                           0.0, 100, 0.1, 1, 0);
    gwy_table_attach_hscale(table, row, _("_Coverage:"),
                            "%",
                            controls.coverage, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.coverage, "value-changed",
                             G_CALLBACK(coverage_changed), &controls);
    row++;

    controls.revise = gtk_adjustment_new(args->revise,
                                           0.0, 10000, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Revise:"),
                            "",
                            controls.revise, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.revise, "value-changed",
                             G_CALLBACK(revise_changed), &controls);
    row++;


    controls.message = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.message), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.message,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;



    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    deposit_synth_invalidate(&controls);

    finished = FALSE;
    while (!finished) {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_OK:
            case GTK_RESPONSE_NONE:
            finished = TRUE;
            break;

            case RESPONSE_RESET:
            {
                gboolean temp = args->update;
                gint temp2 = args->active_page;
                *args = deposit_synth_defaults;
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


    if (response == GTK_RESPONSE_OK)
    {
        if (!controls.data_done) preview(&controls);

        if (controls.dims->args->replace) {
            gwy_app_undo_qcheckpointv(data, 1, &quark);
            gwy_data_field_copy(controls.out, controls.original, FALSE);
            gwy_data_field_data_changed(controls.original);
        }
        else {
            if (data) {
                newid = gwy_app_data_browser_add_data_field(controls.out, data, TRUE);
                gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                        GWY_DATA_ITEM_GRADIENT,
                                        0);
                gwy_app_set_data_field_title(data, newid, _("Generated"));

            }
            else {
                newid = 0;
                newdata = gwy_container_new();
                gwy_container_set_object(newdata, gwy_app_get_data_key_for_id(newid),
                                         controls.out);
                gwy_app_data_browser_add(newdata);
                gwy_app_data_browser_reset_visibility(newdata,
                                                      GWY_VISIBILITY_RESET_SHOW_ALL);
                g_object_unref(newdata);
                gwy_app_set_data_field_title(newdata, newid, _("Generated"));

            }

        }
    }
    gtk_widget_destroy(dialog);

    if (controls.sid) {
        g_source_remove(controls.sid);
        controls.sid = 0;
    }

    g_object_unref(controls.mydata);
    gwy_dimensions_free(controls.dims);


    return response == GTK_RESPONSE_OK;
}

/* Create a square base surface for preview generation of an exact size */
static GwyDataField*
surface_for_preview(GwyDataField *dfield,
                    guint size)
{
    GwyDataField *retval;
    gint xres, yres, xoff, yoff;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    /* If the field is large enough, just cut an area from the centre. */
    if (xres >= size && yres >= size) {
        xoff = (xres - size)/2;
        yoff = (yres - size)/2;
        return gwy_data_field_area_extract(dfield, xoff, yoff, size, size);
    }

    if (xres <= yres) {
        yoff = (yres - xres)/2;
        dfield = gwy_data_field_area_extract(dfield, 0, yoff, xres, xres);
    }
    else {
        xoff = (xres - yres)/2;
        dfield = gwy_data_field_area_extract(dfield, xoff, 0, yres, yres);
    }

    retval = gwy_data_field_new_resampled(dfield, size, size,
                                          GWY_INTERPOLATION_KEY);
    g_object_unref(dfield);

    return retval;
}
static void
update_controls(DepositSynthControls *controls,
                DepositSynthArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size), args->size/pow10(controls->dims->args->xypow10));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->width),
                             args->width/pow10(controls->dims->args->xypow10));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->coverage),
                             args->coverage);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->revise),
                             args->revise);
}

static gdouble
rand_gen_gaussian(GRand *rng,
                  gdouble sigma)
{
    static gboolean have_spare = FALSE;
    static gdouble spare;

    gdouble x, y, w;

    /* Calling with NULL rng just clears the spare random value. */
    if (have_spare || G_UNLIKELY(!rng)) {
        have_spare = FALSE;
        return sigma*spare;
    }

    do {
        x = -1.0 + 2.0*g_rand_double(rng);
        y = -1.0 + 2.0*g_rand_double(rng);
        w = x*x + y*y;
    } while (w >= 1.0 || G_UNLIKELY(w == 0.0));

    w = sqrt(-2.0*log(w)/w);
    spare = y*w;
    have_spare = TRUE;

    return sigma*x*w;
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
page_switched(DepositSynthControls *controls,
              G_GNUC_UNUSED GtkNotebookPage *page,
              gint pagenum)
{
    if (controls->in_init)
        return;

    controls->args->active_page = pagenum;

    if (pagenum == PAGE_GENERATOR) {
        GwyDimensions *dims = controls->dims;

        gtk_label_set_markup(GTK_LABEL(controls->size_units),
                             dims->xyvf->units);
        gtk_label_set_markup(GTK_LABEL(controls->width_units),
                             dims->xyvf->units);

        //update_size_value(controls);
        //update_width_value(controls);
    }
}

static void
seed_changed(DepositSynthControls *controls,
             GtkAdjustment *adj)
{
    controls->args->seed = gwy_adjustment_get_int(adj);
    deposit_synth_invalidate(controls);
}

static void
randomize_seed(GtkAdjustment *adj)
{
    /* Use the GLib's global PRNG for seeding */
    gtk_adjustment_set_value(adj, g_random_int() & 0x7fffffff);
}

static void          
size_changed(DepositSynthControls *controls,
             GtkAdjustment *adj)
{
    controls->args->size = gtk_adjustment_get_value(adj)*pow10(controls->dims->args->xypow10);
    deposit_synth_invalidate(controls);

}
static void          
width_changed(DepositSynthControls *controls,
             GtkAdjustment *adj)
{
    controls->args->width = gtk_adjustment_get_value(adj)*pow10(controls->dims->args->xypow10);
    deposit_synth_invalidate(controls);

}
static void          
coverage_changed(DepositSynthControls *controls,
             GtkAdjustment *adj)
{
    controls->args->coverage = gtk_adjustment_get_value(adj);
    deposit_synth_invalidate(controls);

}
static void          
revise_changed(DepositSynthControls *controls,
             GtkAdjustment *adj)
{
    controls->args->revise = gtk_adjustment_get_value(adj);
    deposit_synth_invalidate(controls);

}


static void
deposit_synth_invalidate(DepositSynthControls *controls)
{
    controls->data_done = FALSE;
    /* create preview if instant updates are on */
    if (controls->args->update && !controls->in_init && !controls->sid) {
        controls->sid = g_idle_add_full(G_PRIORITY_LOW, preview_gsource,
                                        controls, NULL);
    }
}

static gboolean
preview_gsource(gpointer user_data)
{
    DepositSynthControls *controls = (DepositSynthControls*)user_data;
    controls->sid = 0;

    preview(controls);

    return FALSE;
}


static void
preview(DepositSynthControls *controls)
{
    DepositSynthArgs *args = controls->args;
    gdouble mag;
    GwySIUnit *siunit;
    GwyDataField *dfield, *surface;
    gchar message[50];
    gint ndata;
    gboolean success;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                                    "/0/data"));

    if (!controls->original || !controls->dims->args->add) {   //no dfield or not to be used
        if (!controls->out) {

            mag = pow10(controls->dims->args->xypow10) * controls->dims->args->measure;
            controls->out = gwy_data_field_new(controls->dims->args->xres, controls->dims->args->yres,
                                        mag*controls->dims->args->xres, mag*controls->dims->args->yres,
                                        TRUE);

            siunit = gwy_data_field_get_si_unit_xy(controls->out);
            gwy_si_unit_set_from_string(siunit, controls->dims->args->xyunits);

            siunit = gwy_data_field_get_si_unit_z(controls->out);
            gwy_si_unit_set_from_string(siunit, controls->dims->args->zunits);

        }
        else if (gwy_data_field_get_xres(controls->out) != controls->dims->args->xres ||
                 gwy_data_field_get_yres(controls->out) != controls->dims->args->yres)
        {
            gwy_data_field_resample(controls->out, 
                                    controls->dims->args->xres,
                                    controls->dims->args->yres,
                                    GWY_INTERPOLATION_NONE);
        }

        mag = pow10(controls->dims->args->xypow10) * controls->dims->args->measure;
        if (gwy_data_field_get_xreal(controls->out) != mag*controls->dims->args->xres ||
            gwy_data_field_get_yreal(controls->out) != mag*controls->dims->args->yres)
        {
            gwy_data_field_set_xreal(controls->out, mag*controls->dims->args->xres);
            gwy_data_field_set_yreal(controls->out, mag*controls->dims->args->yres);
            
        }

        gwy_data_field_fill(controls->out, 0);
    } else {
        if (controls->out && (gwy_data_field_get_xres(controls->original)!=gwy_data_field_get_xres(controls->out) ||
            gwy_data_field_get_yres(controls->original)!=gwy_data_field_get_yres(controls->out)))
        {
            gwy_object_unref(controls->out);
            controls->out = NULL;
        }

        if (!controls->out) {
            controls->out = gwy_data_field_new_alike(controls->original, TRUE);
        }
        if (gwy_data_field_get_xreal(controls->original)!=gwy_data_field_get_xreal(controls->out) ||
            gwy_data_field_get_yreal(controls->original)!=gwy_data_field_get_yreal(controls->out))
        {
            gwy_data_field_set_xreal(controls->out, gwy_data_field_get_xreal(controls->original));
            gwy_data_field_set_yreal(controls->out, gwy_data_field_get_yreal(controls->original));
        }
         
        gwy_data_field_copy(controls->original, controls->out, TRUE);
    }


    surface = surface_for_preview(controls->out, PREVIEW_SIZE);
    gwy_data_field_copy(surface, dfield, FALSE);
    gwy_data_field_data_changed(dfield);

        
    /*check arguments for sure again (see sanitize_args)*/
    args->size = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->size))*pow10(controls->dims->args->xypow10);
    args->width = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->width))*pow10(controls->dims->args->xypow10);
    
    gwy_app_wait_start(GTK_WINDOW(controls->dialog), "Starting...");
    gtk_label_set_text(GTK_LABEL(controls->message), "Running computation...");
    ndata = deposit_synth_do(args, controls->out, dfield, &success);
    gwy_app_wait_finish();

    if (ndata>=0 && success) g_snprintf(message, sizeof(message), "%d particles deposited", ndata);
    else if (ndata>=0 && !success) g_snprintf(message, sizeof(message), "Not all deposited (%d), try revise", ndata);
    else if (ndata==RES_TOO_MANY) g_snprintf(message, sizeof(message), "Error: too many particles.");
    else if (ndata==RES_TOO_FEW) g_snprintf(message, sizeof(message), "Error: no particles.");
    else if (ndata==RES_TOO_LARGE) g_snprintf(message, sizeof(message), "Error: particles too large."); 
    else if (ndata==RES_TOO_SMALL) g_snprintf(message, sizeof(message), "Error: particles too small.");
   
    gtk_label_set_text(GTK_LABEL(controls->message), message);

    if (surface) gwy_object_unref(surface);
    surface = surface_for_preview(controls->out, PREVIEW_SIZE);
    gwy_data_field_copy(surface, dfield, FALSE);
    gwy_data_field_data_changed(dfield);
    controls->data_done = TRUE;

    gwy_data_field_data_changed(controls->out);

    gwy_object_unref(surface);
}

static inline gboolean
gwy_data_field_inside(GwyDataField *data_field, gint i, gint j)
{
        if (i >= 0 && j >= 0 && i < data_field->xres && j < data_field->yres)
                    return TRUE;
            else
                        return FALSE;
}


static void
showit(GwyDataField *lfield, GwyDataField *dfield, gdouble *rdisizes, gdouble *rx, gdouble *ry, gdouble *rz, gint *xdata, gint *ydata, gint ndata,
       gint oxres, gdouble oxreal, gint oyres, gdouble oyreal, gint add, gint xres, gint yres)
{
    gint i, m, n;
    gdouble sum, surface, lsurface, csurface;
    gint disize;
    gdouble rdisize;

    for (i=0; i<ndata; i++)
    {
        xdata[i] = oxres*(rx[i]/oxreal);
        ydata[i] = oyres*(ry[i]/oyreal);

        if (xdata[i]<0 || ydata[i]<0 || xdata[i]>=xres || ydata[i]>=yres) continue;
        if (rz[i]>(gwy_data_field_get_val(lfield, xdata[i], ydata[i])+6*rdisizes[i])) continue;

        csurface = gwy_data_field_get_val(lfield, xdata[i], ydata[i]);
        disize = (gint)((gdouble)oxres*rdisizes[i]/oxreal);
        rdisize = rdisizes[i];

        for (m=(xdata[i]-disize); m<(xdata[i]+disize); m++)

        {
            for (n=(ydata[i]-disize); n<(ydata[i]+disize); n++)
            {
                if (m<0 || n<0 || m>=xres || n>=yres) continue;

                if (m>=add && n>=add && m<(xres-add) && n<(yres-add)) {
                    surface = gwy_data_field_get_val(dfield, m-add, n-add);
                    lsurface = gwy_data_field_get_val(lfield, m, n);

                    if ((sum=(disize*disize - (xdata[i]-m)*(xdata[i]-m) - (ydata[i]-n)*(ydata[i]-n)))>0)
                    {
                        surface = MAX(lsurface, rz[i] + sqrt(sum)*oxreal/(double)oxres);
                        gwy_data_field_set_val(lfield, m, n, surface);
                    }
                }
            }
        }
    }
}


/*lj potential between two particles*/
static gdouble
get_lj_potential_spheres(gdouble ax, gdouble ay, gdouble az, gdouble bx, gdouble by, gdouble bz, gdouble asize, gdouble bsize)
{
    gdouble sigma = 0.82*(asize+bsize); 
    gdouble dist = ((ax-bx)*(ax-bx)
                    + (ay-by)*(ay-by)
                    + (az-bz)*(az-bz));

    if ((asize>0 && bsize>0) && dist > asize/100)
    return (asize)*3e-5*(pow(sigma, 12)/pow(dist, 6) - pow(sigma, 6)/pow(dist, 3)); //corrected for particle size
    else return 0;
}

/*integrate over some volume around particle (ax, ay, az), if there is substrate, add this to potential*/
static gdouble
integrate_lj_substrate(GwyDataField *lfield, gdouble ax, gdouble ay, gdouble az, gdouble size)
{
    /*make l-j only from idealistic substrate now*/
    gdouble zval, sigma, dist;
    sigma = 1.2*size; //empiric

    zval = gwy_data_field_get_val(lfield, CLAMP(gwy_data_field_rtoi(lfield, ax), 0, gwy_data_field_get_xres(lfield)-1),
                                          CLAMP(gwy_data_field_rtoi(lfield, ay), 0, gwy_data_field_get_yres(lfield)-1));

    dist = sqrt((az-zval)*(az-zval));
    
    if (size>0 && dist > size/100)
    return size*1e-3*(pow(sigma, 12)/45.0/pow(dist, 9) - pow(sigma, 6)/6.0/pow(dist, 3)); //corrected for particle size
    else return 0;
}
    


static gint
deposit_synth_do(const DepositSynthArgs *args,
             GwyDataField *dfield, GwyDataField *showfield, gboolean *success)
{
    gint i, ii, m, k;
    GRand *rng;
    GwyDataField *surface=NULL, *lfield, *zlfield, *zdfield; //FIXME all of them?
    gint xres, yres, oxres, oyres, ndata, steps;
    gdouble xreal, yreal, oxreal, oyreal, diff;
    gdouble size, width; 
    gdouble mass=1,  timestep = 1, rxv, ryv, rzv;
    gint mdisize, add, presetval;
    gint *xdata, *ydata;
    gdouble *disizes, *rdisizes;
    gdouble *rx, *ry, *rz;
    gdouble *vx, *vy, *vz;
    gdouble *ax, *ay, *az;
    gdouble *fx, *fy, *fz;
    gdouble disize;
    gint xpos, ypos, too_close, maxsteps = 10000;
    gint nloc, maxloc = 1;
    gint max = 5000000;
    gdouble norm;


    //FIXME renormalize everything for size of field 1x1, including z. Change parameters of potentials.
    norm = 1/gwy_data_field_get_xreal(dfield);

//    printf("do (%d %d) %g %g, norm %g\n", gwy_data_field_get_xres(dfield), gwy_data_field_get_yres(dfield),
//                           gwy_data_field_get_xreal(dfield), gwy_data_field_get_yreal(dfield), norm);
//    printf("size %g width %g, coverage %g, revise %d, datafield real %g x %g, rms %g\n", 
//           args->size, args->width, args->coverage, args->revise, oxreal, oyreal, gwy_data_field_get_rms(dfield)); 

    rng = g_rand_new();
    g_rand_set_seed(rng, args->seed);

    /*normalize all*/
    gwy_data_field_multiply(dfield, norm);
    gwy_data_field_set_xreal(dfield, gwy_data_field_get_xreal(dfield)*norm);
    gwy_data_field_set_yreal(dfield, gwy_data_field_get_yreal(dfield)*norm);
    size = norm*args->size;
    width = norm*args->width;
    /*now everything is normalized to be close to 1*/

    oxres = gwy_data_field_get_xres(dfield);
    oyres = gwy_data_field_get_yres(dfield);
    oxreal = gwy_data_field_get_xreal(dfield);
    oyreal = gwy_data_field_get_yreal(dfield);
    diff = oxreal/oxres/10;

    add = CLAMP(gwy_data_field_rtoi(dfield, size + width), 0, oxres/4);
    mdisize = gwy_data_field_rtoi(dfield, size);
    xres = oxres + 2*add;
    yres = oyres + 2*add;
    xreal = xres*oxreal/(gdouble)oxres;
    yreal = yres*oyreal/(gdouble)oyres;

    presetval = args->coverage/100 * xreal*yreal/(G_PI*size*size);
    if (presetval<=0) return RES_TOO_FEW;
    if (presetval>MAXN) return RES_TOO_MANY;
    if (2*size*xres < xreal) return RES_TOO_SMALL;
    if (4*size > xreal) return RES_TOO_LARGE;

    xdata = (gint *) g_malloc(presetval*sizeof(gint));
    ydata = (gint *) g_malloc(presetval*sizeof(gint));
    disizes = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    rdisizes = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    rx = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    ry = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    rz = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    vx = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    vy = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    vz = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    ax = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    ay = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    az = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    fx = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    fy = (gdouble *) g_malloc(presetval*sizeof(gdouble));
    fz = (gdouble *) g_malloc(presetval*sizeof(gdouble));

   // printf("After normalization size %g width %g, coverage %g, revise %d, datafield real %g x %g, rms %g\n", 
   //        size, width, args->coverage, args->revise, oxreal, oyreal, gwy_data_field_get_rms(dfield)); //assure that width and size are in real coordinates

    /*allocate field with increased size, do all the computation and cut field back, return dfield again*/

    lfield = gwy_data_field_new(xres, yres,
                                xreal, yreal,
                                TRUE);
    gwy_data_field_area_copy(dfield, lfield, 0, 0, oxres, oyres, add, add);

    gwy_data_field_invert(dfield, 1, 0, 0);
    gwy_data_field_area_copy(dfield, lfield, 0, oyres-add-1, oxres, add, add, 0);
    gwy_data_field_area_copy(dfield, lfield, 0, 0, oxres, add, add, yres-add-1);
    gwy_data_field_invert(dfield, 1, 0, 0);

    gwy_data_field_invert(dfield, 0, 1, 0);
    gwy_data_field_area_copy(dfield, lfield, oxres-add-1, 0, add, oyres, 0, add);
    gwy_data_field_area_copy(dfield, lfield, 0, 0, add, oyres, xres-add-1, add);
    gwy_data_field_invert(dfield, 0, 1, 0);

    gwy_data_field_invert(dfield, 1, 1, 0);
    gwy_data_field_area_copy(dfield, lfield, oxres-add-1, oyres-add-1, add, add, 0, 0);
    gwy_data_field_area_copy(dfield, lfield, 0, 0, add, add, xres-add-1, yres-add-1);
    gwy_data_field_area_copy(dfield, lfield, oxres-add-1, 0, add, add, 0, yres-add-1);
    gwy_data_field_area_copy(dfield, lfield, 0, oyres-add-1, add, add, xres-add-1, 0);
    gwy_data_field_invert(dfield, 1, 1, 0);


    zlfield = gwy_data_field_duplicate(lfield);
    zdfield = gwy_data_field_duplicate(dfield);
    gwy_app_wait_set_message("Initial particle set...");

    /*FIXME determine max number of particles and alloc only necessary field sizes*/

    for (i=0; i<presetval; i++)
    {
        ax[i] = ay[i] = az[i] = vx[i] = vy[i] = vz[i] = 0;
    }

    ndata = steps = 0;
    presetval = args->coverage/100 * xreal*yreal/(G_PI*size*size); 
    //printf("%d particles should be deposited\n", presetval);

    while (ndata < presetval && steps<maxsteps)
    {
        size = norm*args->size + rand_gen_gaussian(rng, norm*args->width);
        if (size<args->size/100) size = args->size/100;

        disize = gwy_data_field_rtoi(dfield, size);

        xpos = CLAMP((gint)(disize+(g_rand_double(rng)*(xres-2*(gint)(disize+1))) + 1), 0, xres-1);
        ypos = CLAMP((gint)(disize+(g_rand_double(rng)*(yres-2*(gint)(disize+1))) + 1), 0, yres-1);
        steps++;

        too_close = 0;

        /*sync real to integer positions*/
        for (k=0; k<ndata; k++)
        {
            if (((xpos-xdata[k])*(xpos-xdata[k]) + (ypos-ydata[k])*(ypos-ydata[k]))<(4*disize*disize))
            {
                too_close = 1;
                break;
            }
        }
        if (too_close) continue;
        if (ndata>=presetval) {
            break;
        }

        xdata[ndata] = xpos;
        ydata[ndata] = ypos;
        disizes[ndata] = disize;
        rdisizes[ndata] = size;
        rx[ndata] = (gdouble)xpos*oxreal/(gdouble)oxres; 
        ry[ndata] = (gdouble)ypos*oyreal/(gdouble)oyres;
        rz[ndata] = gwy_data_field_get_val(lfield, xpos, ypos) + rdisizes[ndata]; 
        ndata++;
    };

    gwy_data_field_copy(zlfield, lfield, 0);

    if (showfield) {
        showit(lfield, zdfield, rdisizes, rx, ry, rz, xdata, ydata, ndata,
               oxres, oxreal, oyres, oyreal, add, xres, yres);
        if (surface) gwy_object_unref(surface);
        surface = surface_for_preview(dfield, PREVIEW_SIZE);
        gwy_data_field_copy(surface, showfield, FALSE);
        gwy_data_field_data_changed(showfield);
    }


    gwy_data_field_area_copy(lfield, dfield, add, add, oxres, oyres, 0, 0);
    gwy_data_field_data_changed(dfield);


    /*revise steps*/
    for (i=0; i<(args->revise); i++)
    {
        if (!gwy_app_wait_set_message("Running revise...")) break;

        /*try to add some particles if necessary, do this only for first half of molecular dynamics steps*/
        if (ndata<presetval && i<(10*args->revise)) {
            ii = 0;
            nloc = 0;

            while (ndata < presetval && ii<(max/1000) && nloc<maxloc)
            {
                size = norm*args->size + rand_gen_gaussian(rng, norm*args->width);
                if (size<args->size/100) size = args->size/100;

                disize = gwy_data_field_rtoi(dfield, size);

                xpos = CLAMP(disize+(g_rand_double(rng)*(xres-2*(gint)(disize+1))) + 1, 0, xres-1);
                ypos = CLAMP(disize+(g_rand_double(rng)*(yres-2*(gint)(disize+1))) + 1, 0, yres-1);

                ii++;
                too_close = 0;

                rxv = ((gdouble)xpos*oxreal/(gdouble)oxres); 
                ryv = ((gdouble)ypos*oyreal/(gdouble)oyres);
                rzv = gwy_data_field_get_val(zlfield, xpos, ypos) + 5*size;

                for (k=0; k<ndata; k++)
                {
                    if (((rxv-rx[k])*(rxv-rx[k])
                         + (ryv-ry[k])*(ryv-ry[k])
                         + (rzv-rz[k])*(rzv-rz[k]))<(4.0*size*size))
                    {
                        too_close = 1;
                        break;
                    }
                }
                if (too_close) continue;
                if (ndata>=10000) {
                    break;
                }

                xdata[ndata] = xpos;
                ydata[ndata] = ypos;
                disizes[ndata] = disize;
                rdisizes[ndata] = size;
                rx[ndata] = rxv;
                ry[ndata] = ryv;
                rz[ndata] = rzv;
                vz[ndata] = -0.005;
                ndata++;
                nloc++;

            };
        }

        if (!gwy_app_wait_set_message("Running revise...")) break;

        /*test succesive LJ steps on substrate*/
        for (k=0; k<ndata; k++)
        {
            fx[k] = fy[k] = fz[k] = 0;
            /*calculate forces for all particles on substrate*/

            if (gwy_data_field_rtoi(lfield, rx[k])<0
                || gwy_data_field_rtoj(lfield, ry[k])<0
                || gwy_data_field_rtoi(lfield, rx[k])>=xres
                || gwy_data_field_rtoj(lfield, ry[k])>=yres) {
                continue;
            }

            for (m=0; m<ndata; m++)
            {

                if (m==k) continue;

                fx[k] -= (get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k]+diff, ry[k], rz[k], rdisizes[k], rdisizes[m])
                              -get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k]-diff, ry[k], rz[k], rdisizes[k], rdisizes[m]))/2/diff;
                fy[k] -= (get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k], ry[k]+diff, rz[k], rdisizes[k], rdisizes[m])
                              -get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k], ry[k]-diff, rz[k], rdisizes[k], rdisizes[m]))/2/diff;
                fz[k] -= (get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k], ry[k], rz[k]+diff, rdisizes[k], rdisizes[m])
                              -get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k], ry[k], rz[k]-diff, rdisizes[k], rdisizes[m]))/2/diff;

            }

            fx[k] -= (integrate_lj_substrate(zlfield, rx[k]+diff, ry[k], rz[k], rdisizes[k])
                    - integrate_lj_substrate(zlfield, rx[k]-diff, ry[k], rz[k], rdisizes[k]))/2/diff;
            fy[k] -= (integrate_lj_substrate(zlfield, rx[k], ry[k]-diff, rz[k], rdisizes[k])
                    - integrate_lj_substrate(zlfield, rx[k], ry[k]+diff, rz[k], rdisizes[k]))/2/diff;
            fz[k] -= (integrate_lj_substrate(zlfield, rx[k], ry[k], rz[k]+diff, rdisizes[k])
                    - integrate_lj_substrate(zlfield, rx[k], ry[k], rz[k]-diff, rdisizes[k]))/2/diff;

        }

        if (!gwy_app_wait_set_message("Running revise...")) break;
        //clamp forces to prevent too fast movements at extreme parameters cases
        /*for (k=0; k<ndata; k++)
        {
            fx[k] = CLAMP(fx[k], -100, 100);
            fy[k] = CLAMP(fy[k], -100, 100);
            fz[k] = CLAMP(fz[k], -100, 100);
        }*/

        for (k=0; k<ndata; k++)
        {
          if (gwy_data_field_rtoi(lfield, rx[k])<0
                || gwy_data_field_rtoj(lfield, ry[k])<0
                || gwy_data_field_rtoi(lfield, rx[k])>=xres
                || gwy_data_field_rtoj(lfield, ry[k])>=yres)
                continue;

            /*move all particles*/
            rx[k] += vx[k]*timestep + 0.5*ax[k]*timestep*timestep;
            vx[k] += 0.5*ax[k]*timestep;
            ax[k] = fx[k]/mass;
            vx[k] += 0.5*ax[k]*timestep;
            vx[k] *= 0.9;
            if (fabs(vx[k])>0.01) vx[k] = 0;

            ry[k] += vy[k]*timestep + 0.5*ay[k]*timestep*timestep;
            vy[k] += 0.5*ay[k]*timestep;
            ay[k] = fy[k]/mass;
            vy[k] += 0.5*ay[k]*timestep;
            vy[k] *= 0.9;
            if (fabs(vy[k])>0.01) vy[k] = 0; 

            rz[k] += vz[k]*timestep + 0.5*az[k]*timestep*timestep;
            vz[k] += 0.5*az[k]*timestep;
            az[k] = fz[k]/mass;
            vz[k] += 0.5*az[k]*timestep;
            vz[k] *= 0.9;

            if (fabs(vz[k])>0.01) vz[k] = 0;
            if (rx[k]<rdisizes[k]) rx[k] = rdisizes[k];
            if (ry[k]<rdisizes[k]) ry[k] = rdisizes[k];
            if (rx[k]>(xreal-rdisizes[k])) rx[k] = xreal-rdisizes[k];
            if (ry[k]>(yreal-rdisizes[k])) ry[k] = yreal-rdisizes[k];
            
 //           if (k%10==0) printf("final %d (%g %g %g) (%g %g %g) (%g %g)\n", k, fx[k], fy[k], fz[k], rx[k], ry[k], rz[k], xreal, yreal);
        }


        gwy_data_field_copy(zlfield, lfield, 0);

        if (showfield) {
            showit(lfield, zdfield, rdisizes, rx, ry, rz, xdata, ydata, ndata,
                   oxres, oxreal, oyres, oyreal, add, xres, yres);
            if (surface) gwy_object_unref(surface);
            surface = surface_for_preview(dfield, PREVIEW_SIZE);
            gwy_data_field_copy(surface, showfield, FALSE);
            gwy_data_field_data_changed(showfield);
        }

        gwy_data_field_area_copy(lfield, dfield, add, add, oxres, oyres, 0, 0);
        gwy_data_field_data_changed(dfield);
        while (gtk_events_pending())
                    gtk_main_iteration();


        if (!gwy_app_wait_set_fraction((gdouble)i/(gdouble)args->revise)) break;
    }

    gwy_data_field_copy(zlfield, lfield, 0);

    showit(lfield, zdfield, rdisizes, rx, ry, rz, xdata, ydata, ndata,
           oxres, oxreal, oyres, oyreal, add, xres, yres);
    if (surface) gwy_object_unref(surface);

    if (showfield) {

        surface = surface_for_preview(dfield, PREVIEW_SIZE);
        gwy_data_field_copy(surface, showfield, FALSE);
        gwy_data_field_data_changed(showfield);
    }

    gwy_data_field_area_copy(lfield, dfield, add, add, oxres, oyres, 0, 0);
    gwy_data_field_data_changed(dfield);

    /*denormalize all*/
    gwy_data_field_multiply(dfield, 1/norm);
    gwy_data_field_set_xreal(dfield, gwy_data_field_get_xreal(dfield)/norm);
    gwy_data_field_set_yreal(dfield, gwy_data_field_get_yreal(dfield)/norm);
    /*denormalized*/

    gwy_object_unref(lfield);
    gwy_object_unref(zlfield);
    gwy_object_unref(zdfield);

    g_free(xdata);
    g_free(ydata);
    g_free(disizes);
    g_free(rdisizes);
    g_free(rx);
    g_free(ry);
    g_free(rz);
    g_free(vx);
    g_free(vy);
    g_free(vz);
    g_free(ax);
    g_free(ay);
    g_free(az);
    g_free(fx);
    g_free(fy);
    g_free(fz);

    g_rand_free(rng);

    if (ndata != presetval) *success = FALSE;
    else *success = TRUE;

    return ndata;
}

static const gchar prefix[]           = "/module/deposit_synth";
static const gchar active_page_key[]  = "/module/deposit_synth/active_page";
static const gchar update_key[]       = "/module/deposit_synth/update";
static const gchar randomize_key[]    = "/module/deposit_synth/randomize";
static const gchar seed_key[]         = "/module/deposit_synth/seed";
static const gchar size_key[]         = "/module/deposit_synth/size";
static const gchar width_key[]        = "/module/deposit_synth/width";
static const gchar coverage_key[]     = "/module/deposit_synth/coverage";
static const gchar revise_key[]       = "/module/deposit_synth/revise";

static void
deposit_synth_sanitize_args(DepositSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = !!args->update;
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->size = CLAMP(args->size, 0.0, 100); //FIXME this should be absolute value!
    args->width = CLAMP(args->width, 0.0, 100); //here as well
    args->coverage = CLAMP(args->coverage, 0, 100);
    args->revise = CLAMP(args->revise, 0, 1000);;
}

static void
deposit_synth_load_args(GwyContainer *container,
                    DepositSynthArgs *args,
                    GwyDimensionArgs *dimsargs)
{
    *args = deposit_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_double_by_name(container, size_key, &args->size);
    gwy_container_gis_double_by_name(container, width_key, &args->width);
    gwy_container_gis_double_by_name(container, coverage_key, &args->coverage);
    gwy_container_gis_int32_by_name(container, revise_key, &args->revise);
    deposit_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
deposit_synth_save_args(GwyContainer *container,
                    const DepositSynthArgs *args,
                    const GwyDimensionArgs *dimsargs)
{
    gwy_container_set_int32_by_name(container, active_page_key,
                                    args->active_page);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
    gwy_container_set_double_by_name(container, size_key, args->size);
    gwy_container_set_double_by_name(container, width_key, args->width);
    gwy_container_set_double_by_name(container, coverage_key, args->coverage);
    gwy_container_set_int32_by_name(container, revise_key, args->revise);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
