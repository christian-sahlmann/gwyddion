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
    MAXN         = 10000,
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
    GwyContainer *mydata;
    GwyDataField *surface;
    GwyDataField *original;

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
                                            gint id);
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
static void          update_value_label    (GtkLabel *label,
                                            const GwySIValueFormat *vf,
                                            gdouble value);
static void          deposit_synth_invalidate  (DepositSynthControls *controls);
static gboolean      preview_gsource       (gpointer user_data);
static void          preview               (DepositSynthControls *controls);
static void          deposit_synth_do          (const DepositSynthArgs *args,
                                            GwyDataField *dfield);
static void          deposit_synth_load_args   (GwyContainer *container,
                                            DepositSynthArgs *args,
                                            GwyDimensionArgs *dimsargs);
static void          deposit_synth_save_args   (GwyContainer *container,
                                            const DepositSynthArgs *args,
                                            const GwyDimensionArgs *dimsargs);

static const DepositSynthArgs deposit_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    0, 0,
    10, 10,
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

    if (run == GWY_RUN_IMMEDIATE
        || deposit_synth_dialog(&args, &dimsargs, data, dfield, id))
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);

    if (run == GWY_RUN_INTERACTIVE)
        deposit_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);

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
    gboolean replace = dimsargs->replace && dfield;
    gboolean add = dimsargs->add && dfield;
    gdouble mag; 
    gint newid;

    if (args->randomize)
        args->seed = g_random_int() & 0x7fffffff;

    if (replace) {
        gwy_app_undo_qcheckpointv(data, 1, &quark);
        out = gwy_data_field_new_alike(dfield, FALSE);
    }
    else {
        if (add)
            out = gwy_data_field_new_alike(dfield, FALSE);
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

    deposit_synth_do(args, out);

    if (replace) {
        if (add) {
            printf("ni add\n");
            gwy_data_field_sum_fields(dfield, dfield, out);
        }
        else {
            printf("ni copy\n");
            gwy_data_field_copy(out, dfield, FALSE);

        }

        gwy_data_field_data_changed(dfield);
    }
    else {
        if (add)
            gwy_data_field_sum_fields(out, dfield, out);

        if (data) {
            newid = gwy_app_data_browser_add_data_field(out, data, TRUE);
            gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                    GWY_DATA_ITEM_GRADIENT,
                                    0);
        }
        else {
            newid = 0;
            data = gwy_container_new();
            gwy_container_set_object(data, gwy_app_get_data_key_for_id(newid),
                                     out);
            gwy_app_data_browser_add(data);
            gwy_app_data_browser_reset_visibility(data,
                                                  GWY_VISIBILITY_RESET_SHOW_ALL);
            g_object_unref(data);
        }

        gwy_app_set_data_field_title(data, newid, _("Generated"));
    }
    g_object_unref(out);
}

static gboolean
deposit_synth_dialog(DepositSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *spin;
    DepositSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    controls.pxsize = 1.0;
    dialog = gtk_dialog_new_with_buttons(_("Particle generation"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    controls.original = dfield_template;

 ///////////////////////
    printf("dfield template %d %d %g %g\n", gwy_data_field_get_xres(dfield_template),
                        gwy_data_field_get_yres(dfield_template), gwy_data_field_get_xreal(dfield_template), gwy_data_field_get_yreal(dfield_template));

    ////////////////////


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


    ////                 store a copy, not field itself 
    gwy_container_set_object_by_name(controls.mydata, "/1/data", dfield_template);

    if (data)
        gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                0);
    if (dfield_template) {
        controls.surface = surface_for_preview(controls.original, PREVIEW_SIZE);
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

    controls.size = gtk_adjustment_new(args->size,
                                        0.0001, 10000.0, 0.1, 1.0, 0);
    spin = gwy_table_attach_hscale(table, row, _("_size:"), "",
                                   controls.size, GWY_HSCALE_LOG);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 4);
    controls.size_units = gwy_table_hscale_get_units(controls.size);
    g_signal_connect_swapped(controls.size, "value-changed",
                             G_CALLBACK(size_changed), &controls);
    row++;

    controls.width = gtk_adjustment_new(args->width,
                                        0.0001, 10000.0, 0.1, 1.0, 0);
    spin = gwy_table_attach_hscale(table, row, _("_width:"), "",
                                   controls.width, GWY_HSCALE_LOG);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 4);
    controls.width_units = gwy_table_hscale_get_units(controls.width);
    g_signal_connect_swapped(controls.width, "value-changed",
                             G_CALLBACK(width_changed), &controls);
    row++;

    controls.coverage = gtk_adjustment_new(args->coverage,
                                           0.0, 1000, 0.1, 1, 0);
    gwy_table_attach_hscale(table, row, _("Coverage:"),
                            "%",
                            controls.coverage, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.coverage, "value-changed",
                             G_CALLBACK(coverage_changed), &controls);
    row++;

    controls.revise = gtk_adjustment_new(args->revise,
                                           0.0, 1000, 0.1, 1, 0);
    gwy_table_attach_hscale(table, row, _("Coverage:"),
                            "%",
                            controls.revise, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.revise, "value-changed",
                             G_CALLBACK(revise_changed), &controls);
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
            gtk_widget_destroy(dialog);
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

    if (controls.sid) {
        g_source_remove(controls.sid);
        controls.sid = 0;
    }
    g_object_unref(controls.mydata);
    gwy_dimensions_free(controls.dims);
    gwy_object_unref(controls.surface);

    printf("dialog finished\n");
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
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size), args->size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->width),
                             args->width);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->coverage),
                             args->coverage);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->revise),
                             args->revise);
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

        gtk_label_set_markup(GTK_LABEL(controls->size),
                             dims->xyvf->units);
        gtk_label_set_markup(GTK_LABEL(controls->width),
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
    controls->args->size = gtk_adjustment_get_value(adj);
    deposit_synth_invalidate(controls);

}
static void          
width_changed(DepositSynthControls *controls,
             GtkAdjustment *adj)
{
    controls->args->width = gtk_adjustment_get_value(adj);
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
update_value_label(GtkLabel *label,
                   const GwySIValueFormat *vf,
                   gdouble value)
{
    gchar buf[32];

    g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, value/vf->magnitude);
    gtk_label_set_markup(label, buf);
}

static void
deposit_synth_invalidate(DepositSynthControls *controls)
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

    printf("starting preview\n");
    if (!controls->original || !controls->dims->args->add) {   //no dfield or not to be used
        if (!controls->out) {
            printf("no out, create it now (%d %d)\n",
                   controls->dims->args->xres, controls->dims->args->yres);

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
            printf("resize out, it was changed obviously to %d %d\n", controls->dims->args->xres, controls->dims->args->yres);
            gwy_data_field_resample(controls->out, 
                                    controls->dims->args->xres,
                                    controls->dims->args->yres,
                                    GWY_INTERPOLATION_NONE);
        }

        mag = pow10(controls->dims->args->xypow10) * controls->dims->args->measure;
        if (gwy_data_field_get_xreal(controls->out) != mag*controls->dims->args->xres ||
            gwy_data_field_get_yreal(controls->out) != mag*controls->dims->args->yres)
        {
            printf("real size changed obviously, change it as well to %g %g\n",
                   mag*controls->dims->args->xres, mag*controls->dims->args->yres);
            gwy_data_field_set_xreal(controls->out, mag*controls->dims->args->xres);
            gwy_data_field_set_yreal(controls->out, mag*controls->dims->args->yres);
            
        }

        gwy_data_field_fill(controls->out, 0);
    } else {
        printf("surface used, copy it for out");
        if (gwy_data_field_get_xres(controls->original)!=gwy_data_field_get_xres(controls->out) ||
            gwy_data_field_get_yres(controls->original)!=gwy_data_field_get_yres(controls->out))
        {
            printf("some strange output size, delete it");
            gwy_object_unref(controls->out);
            controls->out = NULL;
        }

        if (!controls->out) {
            printf("no out for surface copy, create it now\n");
            controls->out = gwy_data_field_new_alike(controls->original, TRUE);
        }
        if (gwy_data_field_get_xreal(controls->original)!=gwy_data_field_get_xreal(controls->out) ||
            gwy_data_field_get_yreal(controls->original)!=gwy_data_field_get_yreal(controls->out))
        {
            printf("wrong output physical size, change it for %g %g\n", gwy_data_field_get_xreal(controls->original),
                    gwy_data_field_get_yreal(controls->original));
            gwy_data_field_set_xreal(controls->out, gwy_data_field_get_xreal(controls->original));
            gwy_data_field_set_yreal(controls->out, gwy_data_field_get_yreal(controls->original));
        }
         
        gwy_data_field_copy(controls->original, controls->out, TRUE);
    }


    printf("preview (dfield %d %d %g %g)\n", gwy_data_field_get_xres(controls->out),
             gwy_data_field_get_yres(controls->out), gwy_data_field_get_xreal(controls->out), gwy_data_field_get_yreal(controls->out));
    
    deposit_synth_do(args, controls->out);


    //this is done for output data; extract "surface" from it to preview

    gwy_data_field_data_changed(controls->out);
}

static void
showit(GwyDataField *lfield, GwyDataField *dfield, gdouble *rdisizes, gdouble *rx, gdouble *ry, gdouble *rz, gint *xdata, gint *ydata, gint ndata,
       gint oxres, gdouble oxreal, gint oyres, gdouble oyreal, gint add, gint xres, gint yres)
{
    gint i, m, n;
    gdouble sum, surface, lsurface, csurface;
    gint disize;
    gdouble rdisize;

    //FIXME, add z also
    //
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
                            //surface = MAX(lsurface, csurface + (sqrt(sum) + disize)*oxreal/(double)oxres);
                            surface = MAX(lsurface, rz[i] + sqrt(sum)*oxreal/(double)oxres);
                            gwy_data_field_set_val(lfield, m, n, surface);
                        }
                    }

                }
            }
    }
}



static void
deposit_synth_do(const DepositSynthArgs *args,
             GwyDataField *dfield)
{
    gint i, j, k;
    GRand *rng;
    GwyDataField *lfield, *zlfield, *zdfield; //FIXME all of them?
    gint xres, yres, oxres, oyres, ndata, steps;
    gdouble xreal, yreal, oxreal, oyreal, diff;
    gdouble size, width;
    gint mdisize, add, presetval;
    gint xdata[MAXN];
    gint ydata[MAXN];
    gdouble disizes[MAXN];
    gdouble rdisizes[MAXN];
    gdouble rx[MAXN];
    gdouble ry[MAXN];
    gdouble rz[MAXN];
    gdouble ax[MAXN];
    gdouble ay[MAXN];
    gdouble az[MAXN];
    gdouble vx[MAXN];
    gdouble vy[MAXN];
    gdouble vz[MAXN];
    gdouble fx[MAXN];
    gdouble fy[MAXN];
    gdouble fz[MAXN];
    gdouble disize;
    gint xpos, ypos, too_close, maxsteps = 1000;

    printf("do (%d %d) %g %g \n", gwy_data_field_get_xres(dfield), gwy_data_field_get_yres(dfield),
                           gwy_data_field_get_xreal(dfield), gwy_data_field_get_yreal(dfield));
    rng = g_rand_new();
    g_rand_set_seed(rng, args->seed);

    oxres = gwy_data_field_get_xres(dfield);
    oyres = gwy_data_field_get_yres(dfield);
    oxreal = gwy_data_field_get_xreal(dfield);
    oyreal = gwy_data_field_get_yreal(dfield);
    diff = oxreal/oxres/10;


    printf("size %g width %g, coverage %g, revise %d, datafield real %g x %g\n", 
           args->size, args->width, args->coverage, args->revise, oxreal, oyreal); //assure that width and size are in real coordinates

    size = 5e-8; //FIXME
    width = 2*size;
    add = gwy_data_field_rtoi(dfield, size + width);
    mdisize = gwy_data_field_rtoi(dfield, size);
    xres = oxres + 2*add;
    yres = oyres + 2*add;
    xreal = oxreal + 2*(size+width);
    yreal = oyreal + 2*(size+width);


    /*allocate field with increased size, do all the computation and cut field back, return dfield again*/

    printf("new size: %d %d\n", xres, yres);
    lfield = gwy_data_field_new(xres, yres,
                                gwy_data_field_itor(dfield, xres),
                                gwy_data_field_jtor(dfield, yres),
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
    printf("fields prepared for computation\n");

    /*FIXME determine max number of particles and alloc only necessary field sizes*/

    for (i=0; i<MAXN; i++)
    {
        ax[i] = ay[i] = az[i] = vx[i] = vy[i] = vz[i] = 0;
    }

    ndata = steps = 0;
    presetval = args->coverage; //FIXME
    printf("%d particles should be deposited\n", presetval);

    while (ndata < presetval && steps<maxsteps)
    {
        //disize = mdisize*(0.8+(double)(rand()%20)/40.0);   
        disize = mdisize;

        xpos = disize+(g_rand_double(rng)*(xres-2*(gint)(disize+1))) + 1;
        ypos = disize+(g_rand_double(rng)*(yres-2*(gint)(disize+1))) + 1;
        steps++;

        {
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
            if (ndata>=MAXN) {
                break;
            }

            xdata[ndata] = xpos;
            ydata[ndata] = ypos;
            disizes[ndata] = disize;
            rdisizes[ndata] = size;
            rx[ndata] = (gdouble)xpos*oxreal/(gdouble)oxres;
            ry[ndata] = (gdouble)ypos*oyreal/(gdouble)oyres;
            //printf("surface at %g, particle size %g\n", gwy_data_field_get_val(lfield, xpos, ypos), rdisizes[ndata]);
            rz[ndata] = 1.0*gwy_data_field_get_val(lfield, xpos, ypos) + rdisizes[ndata]; //2
            ndata++;
        }
    };
    printf("%d particles deposited in %d steps\n", ndata, steps);

    gwy_data_field_copy(zlfield, lfield, 0);
    showit(lfield, zdfield, rdisizes, rx, ry, rz, xdata, ydata, ndata,
                  oxres, oxreal, oyres, oyreal, add, xres, yres);


    gwy_data_field_area_copy(lfield, dfield, add, add, oxres, oyres, 0, 0);
    gwy_data_field_data_changed(dfield);




    printf("copying center back\n");
    gwy_data_field_area_copy(lfield, dfield, add, add, oxres, oyres, 0, 0);
    gwy_data_field_data_changed(dfield);
    gwy_object_unref(lfield);
    gwy_object_unref(zlfield);
    gwy_object_unref(zdfield);

    g_rand_free(rng);
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
    args->size = CLAMP(args->size, 0.0, 10000);
    args->width = CLAMP(args->width, 0.0, 10000);
    args->coverage = CLAMP(args->coverage, 0, 1000);
    args->revise = CLAMP(args->revise, 0, 10000);;
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
