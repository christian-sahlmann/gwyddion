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
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define DOMAIN_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 320,
};

enum {
    RESPONSE_RESET = 1,
};

enum {
    PAGE_DIMENSIONS = 0,
    PAGE_GENERATOR  = 1,
    PAGE_NPAGES
};

enum {
    QUANTITY_U = 0,
    QUANTITY_V = 1,
    QUANTITY_NTYPES,
    QUANTITY_MASK = (1 << QUANTITY_NTYPES) - 1,
};

typedef struct _ObjSynthControls DomainSynthControls;

typedef struct {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;   /* Always false */
    gboolean animated;
    guint quantity;
    guint preview_quantity;
    guint niters;
    gdouble height;
    gdouble T;
    gdouble J;
    gdouble mu;
    gdouble nu;
    gdouble dt;
} DomainSynthArgs;

struct _ObjSynthControls {
    DomainSynthArgs *args;
    GwyDimensions *dims;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkWidget *animated;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkTable *table;
    GtkObject *niters;
    GtkObject *T;
    GtkObject *J;
    GtkObject *mu;
    GtkObject *nu;
    GtkObject *dt;
    GtkObject *height;
    GtkWidget *height_units;
    GtkWidget *height_init;
    GtkWidget *preview_quantity;
    GtkWidget *quantity[QUANTITY_NTYPES];
    GwyContainer *mydata;
    GwyDataField *surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
};

static gboolean   module_register              (void);
static void       domain_synth                 (GwyContainer *data,
                                                GwyRunType run);
static void       run_noninteractive           (DomainSynthArgs *args,
                                                const GwyDimensionArgs *dimsargs,
                                                GwyContainer *data,
                                                GwyDataField *dfield,
                                                gint oldid,
                                                GQuark quark);
static gboolean   domain_synth_dialog          (DomainSynthArgs *args,
                                                GwyDimensionArgs *dimsargs,
                                                GwyContainer *data,
                                                GwyDataField *dfield,
                                                gint id);
static GtkWidget* preview_quantity_selector_new(DomainSynthControls *controls);
static void       update_controls              (DomainSynthControls *controls,
                                                DomainSynthArgs *args);
static void       page_switched                (DomainSynthControls *controls,
                                                GtkNotebookPage *page,
                                                gint pagenum);
static void       update_values                (DomainSynthControls *controls);
static void       preview_quantity_selected    (GtkComboBox *combo,
                                                DomainSynthControls *controls);
static void       output_quantity_toggled      (GtkToggleButton *check,
                                                DomainSynthControls *controls);
static void       height_init_clicked          (DomainSynthControls *controls);
static void       update_ok_sensitivity        (DomainSynthControls *controls);
static void       domain_synth_invalidate      (DomainSynthControls *controls);
static void       preview                      (DomainSynthControls *controls);
static void       init_ufield_from_surface     (GwyDataField *dfield,
                                                GwyDataField *ufield,
                                                GRand *rng);
static gboolean   domain_synth_do              (const DomainSynthArgs *args,
                                                GwyDataField *ufield,
                                                GwyDataField *vfield,
                                                GRand *rng,
                                                gdouble preview_time);
static void       domain_synth_load_args       (GwyContainer *container,
                                                DomainSynthArgs *args,
                                                GwyDimensionArgs *dimsargs);
static void       domain_synth_save_args       (GwyContainer *container,
                                                const DomainSynthArgs *args,
                                                const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS DomainSynthControls
#define GWY_SYNTH_INVALIDATE(controls) domain_synth_invalidate(controls)

#include "synth.h"

static const DomainSynthArgs domain_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, FALSE, TRUE,
    1 << QUANTITY_U, QUANTITY_U,
    500, 1.0,
    0.8, 1.0, 20.0, 0.0, 5.0,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static const GwyEnum quantity_types[] = {
    { N_("Discrete state"),       QUANTITY_U, },
    { N_("Continuous inhibitor"), QUANTITY_V, },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates domain images using a hybrid Ising model."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("domain_synth",
                              (GwyProcessFunc)&domain_synth,
                              N_("/S_ynthetic/_Domains..."),
                              NULL,
                              DOMAIN_SYNTH_RUN_MODES,
                              0,
                              N_("Generate image with domains"));

    return TRUE;
}

static void
domain_synth(GwyContainer *data, GwyRunType run)
{
    DomainSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & DOMAIN_SYNTH_RUN_MODES);
    domain_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || domain_synth_dialog(&args, &dimsargs, data, dfield, id)) {
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);
    }

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(DomainSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint oldid,
                   GQuark quark)
{
    GwyDataField *ufield, *vfield;
    GwySIUnit *siunit;
    GRand *rng;
    gboolean replace = dimsargs->replace && dfield;
    gboolean add = dimsargs->add && dfield;
    gint unewid = -1, vnewid = -1;
    gboolean ok, uout, vout;

    uout = (args->quantity & (1 << QUANTITY_U));
    vout = (args->quantity & (1 << QUANTITY_V));

    if (args->randomize)
        args->seed = g_random_int() & 0x7fffffff;

    rng = g_rand_new();
    g_rand_set_seed(rng, args->seed);

    if (add || replace) {
        ufield = gwy_data_field_new_alike(dfield, TRUE);
    }
    else {
        gdouble mag = pow10(dimsargs->xypow10) * dimsargs->measure;
        ufield = gwy_data_field_new(dimsargs->xres, dimsargs->yres,
                                    mag*dimsargs->xres, mag*dimsargs->yres,
                                    TRUE);

        siunit = gwy_data_field_get_si_unit_xy(ufield);
        gwy_si_unit_set_from_string(siunit, dimsargs->xyunits);

        siunit = gwy_data_field_get_si_unit_z(ufield);
        gwy_si_unit_set_from_string(siunit, dimsargs->zunits);
    }
    init_ufield_from_surface(add ? dfield : NULL, ufield, rng);

    gwy_app_wait_start(gwy_app_find_window_for_channel(data, oldid),
                       _("Starting..."));
    vfield = gwy_data_field_new_alike(ufield, FALSE);
    ok = domain_synth_do(args, ufield, vfield, rng, HUGE_VAL);
    gwy_app_wait_finish();

    g_rand_free(rng);

    if (!ok) {
        g_object_unref(ufield);
        g_object_unref(vfield);
        return;
    }

    gwy_data_field_renormalize(ufield,
                               pow10(dimsargs->zpow10) * args->height, 0.0);
    gwy_data_field_renormalize(vfield,
                               pow10(dimsargs->zpow10) * args->height, 0.0);

    if (replace) {
        gwy_app_undo_qcheckpointv(data, 1, &quark);
        if (uout)
            gwy_container_set_object(data, gwy_app_get_data_key_for_id(oldid),
                                     ufield);
        else if (vout)
            gwy_container_set_object(data, gwy_app_get_data_key_for_id(oldid),
                                     vfield);
        else {
            g_assert_not_reached();
        }
        gwy_app_channel_log_add(data, oldid, oldid, "proc::domain_synth", NULL);
        g_object_unref(ufield);
        g_object_unref(vfield);
        return;
    }

    if (data) {
        if (uout)
            unewid = gwy_app_data_browser_add_data_field(ufield, data, TRUE);
        if (vout)
            vnewid = gwy_app_data_browser_add_data_field(vfield, data, TRUE);
    }
    else {
        data = gwy_container_new();
        if (uout) {
            unewid = 0;
            gwy_container_set_object(data, gwy_app_get_data_key_for_id(unewid),
                                     ufield);
        }
        if (vout) {
            vnewid = 1;
            gwy_container_set_object(data, gwy_app_get_data_key_for_id(vnewid),
                                     vfield);
        }
        gwy_app_data_browser_add(data);
        gwy_app_data_browser_reset_visibility(data,
                                              GWY_VISIBILITY_RESET_SHOW_ALL);
        g_object_unref(data);
    }

    if (uout) {
        if (oldid != -1)
            gwy_app_sync_data_items(data, data, oldid, unewid, FALSE,
                                    GWY_DATA_ITEM_GRADIENT,
                                    0);
        gwy_app_set_data_field_title(data, unewid, _("Generated"));
        gwy_app_channel_log_add(data, add ? oldid : -1, unewid,
                                "proc::domain_synth", NULL);
    }
    if (vout) {
        if (oldid != -1)
            gwy_app_sync_data_items(data, data, oldid, vnewid, FALSE,
                                    GWY_DATA_ITEM_GRADIENT,
                                    0);
        gwy_app_set_data_field_title(data, vnewid, _("Generated"));
        gwy_app_channel_log_add(data, add ? oldid : -1, vnewid,
                                "proc::domain_synth", NULL);
    }
    g_object_unref(ufield);
    g_object_unref(vfield);
}

static gboolean
domain_synth_dialog(DomainSynthArgs *args,
                    GwyDimensionArgs *dimsargs,
                    GwyContainer *data,
                    GwyDataField *dfield_template,
                    gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *hbox2, *check, *label;
    DomainSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row, i;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    controls.pxsize = 1.0;
    dialog = gtk_dialog_new_with_buttons(_("Domains"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
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
                                TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);

    dfield = gwy_data_field_new(PREVIEW_SIZE, PREVIEW_SIZE,
                                dimsargs->measure*PREVIEW_SIZE,
                                dimsargs->measure*PREVIEW_SIZE,
                                TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/1/data", dfield);

    if (dfield_template) {
        gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                0);
        gwy_app_sync_data_items(data, controls.mydata, id, 1, FALSE,
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

    table = gtk_table_new(12 + (dfield_template ? 1 : 0), 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.preview_quantity = preview_quantity_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Preview quantity:"), NULL,
                            GTK_OBJECT(controls.preview_quantity),
                            GWY_HSCALE_WIDGET);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Simulation Parameters")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.niters = gtk_adjustment_new(args->niters, 1, 10000, 1, 10, 0);
    g_object_set_data(G_OBJECT(controls.niters), "target", &args->niters);
    gwy_table_attach_hscale(table, row, _("_Number of iterations:"), NULL,
                            GTK_OBJECT(controls.niters), GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.niters, "value-changed",
                             G_CALLBACK(gwy_synth_int_changed), &controls);
    row++;

    controls.T = gtk_adjustment_new(args->T, 0.001, 2.0, 0.001, 0.1, 0);
    g_object_set_data(G_OBJECT(controls.T), "target", &args->T);
    gwy_table_attach_hscale(table, row, _("_Temperature:"), NULL,
                            GTK_OBJECT(controls.T), GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.T, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    controls.J = gtk_adjustment_new(args->J, 0.001, 100.0, 0.1, 10, 0);
    g_object_set_data(G_OBJECT(controls.J), "target", &args->J);
    gwy_table_attach_hscale(table, row, _("_Inhibitor strength:"), NULL,
                            GTK_OBJECT(controls.J), GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.J, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    controls.mu = gtk_adjustment_new(args->mu, 0.001, 100.0, 0.1, 10, 0);
    g_object_set_data(G_OBJECT(controls.mu), "target", &args->mu);
    gwy_table_attach_hscale(table, row, _("In_hibitor coupling:"), NULL,
                            GTK_OBJECT(controls.mu), GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.mu, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    controls.nu = gtk_adjustment_new(args->nu, -5.0, 5.0, 0.001, 0.1, 0);
    g_object_set_data(G_OBJECT(controls.nu), "target", &args->nu);
    gwy_table_attach_hscale(table, row, _("_Bias:"), NULL,
                            GTK_OBJECT(controls.nu), GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.nu, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    controls.dt = gtk_adjustment_new(args->dt, 0.001, 1000.0, 0.001, 1.0, 0);
    g_object_set_data(G_OBJECT(controls.dt), "target", &args->dt);
    gwy_table_attach_hscale(table, row, _("_Monte-Carlo time step:"), 
                            "×10<sup>-3</sup>",
                            GTK_OBJECT(controls.dt), GWY_HSCALE_LOG);
    g_signal_connect_swapped(controls.dt, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Output Options")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
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

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new(_("Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    for (i = 0; i < QUANTITY_NTYPES; i++) {
        controls.quantity[i]
            = gtk_check_button_new_with_label(_(quantity_types[i].name));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.quantity[i]),
                                     args->quantity & (1 << i));
        g_object_set_data(G_OBJECT(controls.quantity[i]), "value",
                          GUINT_TO_POINTER(i));
        gtk_table_attach(GTK_TABLE(table), controls.quantity[i],
                         0, 3, row, row+1, GTK_FILL, 0, 0, 0);
        g_signal_connect(controls.quantity[i], "toggled",
                         G_CALLBACK(output_quantity_toggled), &controls);
        row++;
    }
    update_ok_sensitivity(&controls);

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);
    preview_quantity_selected(GTK_COMBO_BOX(controls.preview_quantity),
                              &controls);

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
                *args = domain_synth_defaults;
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

    domain_synth_save_args(gwy_app_settings_get(), args, dimsargs);

    g_object_unref(controls.mydata);
    gwy_object_unref(controls.surface);
    gwy_dimensions_free(controls.dims);

    return response == GTK_RESPONSE_OK;
}

static GtkWidget*
preview_quantity_selector_new(DomainSynthControls *controls)
{
    GtkWidget *combo;

    combo = gwy_enum_combo_box_new(quantity_types,
                                   G_N_ELEMENTS(quantity_types),
                                   G_CALLBACK(preview_quantity_selected),
                                   controls, controls->args->preview_quantity,
                                   TRUE);
    return combo;
}

static void
update_controls(DomainSynthControls *controls,
                DomainSynthArgs *args)
{
    guint i;

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->animated),
                                 args->animated);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->niters), args->niters);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->T), args->T);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->J), args->J);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->mu), args->mu);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->nu), args->nu);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->dt), args->dt);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height), args->height);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->preview_quantity),
                                  args->preview_quantity);

    for (i = 0; i < QUANTITY_NTYPES; i++)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->quantity[i]),
                                     args->quantity & (1 << i));
}

static void
page_switched(DomainSynthControls *controls,
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
update_values(DomainSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
    if (controls->height_units)
        gtk_label_set_markup(GTK_LABEL(controls->height_units),
                             dims->zvf->units);
}

static void
preview_quantity_selected(GtkComboBox *combo,
                          DomainSynthControls *controls)
{
    DomainSynthArgs *args = controls->args;
    GwyPixmapLayer *layer;

    args->preview_quantity = gwy_enum_combo_box_get_active(combo);

    layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));

    if (args->preview_quantity == QUANTITY_U)
        g_object_set(layer, "data-key", "/0/data", NULL);
    else if (args->preview_quantity == QUANTITY_V)
        g_object_set(layer, "data-key", "/1/data", NULL);
    else {
        g_return_if_reached();
    }
}

static void
output_quantity_toggled(GtkToggleButton *check,
                        DomainSynthControls *controls)
{
    DomainSynthArgs *args = controls->args;
    guint value = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(check), "value"));
    gboolean checked = gtk_toggle_button_get_active(check);

    if (checked)
        args->quantity |= (1 << value);
    else
        args->quantity &= ~(1 << value);

    update_ok_sensitivity(controls);
}

static void
height_init_clicked(DomainSynthControls *controls)
{
    gdouble mag = pow10(controls->dims->args->zpow10);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height),
                             controls->zscale/mag);
}

static void
update_ok_sensitivity(DomainSynthControls *controls)
{
    gboolean have_output = !!controls->args->quantity;
    gboolean sensitive;

    if (!have_output)
        sensitive = FALSE;
    else if (!controls->dims->args->replace)
        sensitive = TRUE;
    else {
        guint i, count = 0;

        for (i = 0; i < QUANTITY_NTYPES; i++)
            count += !!(controls->args->quantity & (1 << i));

        sensitive = (count == 1);
    }

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      GTK_RESPONSE_OK, sensitive);
}

static void
domain_synth_invalidate(G_GNUC_UNUSED DomainSynthControls *controls)
{
}

static void
preview(DomainSynthControls *controls)
{
    DomainSynthArgs *args = controls->args;
    GwyDataField *ufield, *vfield;
    GRand *rng;

    rng = g_rand_new();
    g_rand_set_seed(rng, args->seed);

    ufield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    vfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/1/data"));

    if (controls->dims->args->add && controls->surface)
        init_ufield_from_surface(controls->surface, ufield, rng);
    else
        init_ufield_from_surface(NULL, ufield, rng);

    gwy_app_wait_start(GTK_WINDOW(controls->dialog), _("Starting..."));
    domain_synth_do(args, ufield, vfield, rng, 1.25);
    gwy_app_wait_finish();

    g_rand_free(rng);
}

static void
init_ufield_from_surface(GwyDataField *dfield, GwyDataField *ufield,
                         GRand *rng)
{
    guint xres = ufield->xres, yres = ufield->yres, k;
    gdouble *u = gwy_data_field_get_data(ufield);

    if (dfield) {
        gdouble med = gwy_data_field_get_median(dfield);
        const gdouble *d = dfield->data;

        for (k = xres*yres; k; k--, d++, u++)
            *u = (*d <= med) ? -1 : 1;
    }
    else {
        for (k = xres*yres; k; k--, u++)
            *u = g_rand_boolean(rng) ? 1 : -1;
    }
}

static inline gint
mc_step8(gint u,
         gint u1, gint u2, gint u3, gint u4,
         gint u5, gint u6, gint u7, gint u8,
         GRand *rng, gdouble T, gdouble J, gdouble v)
{
    gint s1 = (u == u1) + (u == u2) + (u == u3) + (u == u4);
    gint s2 = (u == u5) + (u == u6) + (u == u7) + (u == u8);
    gdouble E = 6.0 - s1 - 0.5*s2 + J*u*v;
    gdouble Enew = s1 + 0.5*s2 - J*u*v;
    if (Enew < E || g_rand_double(rng) < exp((E - Enew)/T))
        return -u;
    return u;
}

static void
field_mc_step8(const GwyDataField *vfield, const gint *u, gint *unew,
               const DomainSynthArgs *args,
               GRand *rng)
{
    gdouble T = args->T, J = args->J;
    guint xres = vfield->xres, yres = vfield->yres, n = xres*yres;
    const gdouble *v = vfield->data;
    guint i, j;

    /* Top row. */
    unew[0] = mc_step8(u[0],
                       u[1], u[xres-1], u[xres], u[n-xres],
                       u[xres+1], u[2*xres-1], u[n-xres+1], u[n-1],
                       rng, T, J, v[0]);

    for (j = 1; j < xres-1; j++) {
        unew[j] = mc_step8(u[j],
                           u[j-1], u[j+1], u[j+xres], u[j + n-xres],
                           u[j+xres-1], u[j+xres+1], u[j-1 + n-xres], u[j+1 + n-xres],
                           rng, T, J, v[j]);
    }

    j = xres-1;
    unew[j] = mc_step8(u[j],
                       u[0], u[j+xres], u[j-1], u[n-1],
                       u[2*xres-2],  u[xres], u[n-2], u[n-xres],
                       rng, T, J, v[j]);

    /* Inner rows. */
    for (i = 1; i < yres-1; i++) {
        gint *unewrow = unew + i*xres;
        const gint *urow = u + i*xres;
        const gint *uprevrow = u + (i - 1)*xres;
        const gint *unextrow = u + (i + 1)*xres;
        const gdouble *vrow = v + i*xres;

        unewrow[0] = mc_step8(urow[0],
                              uprevrow[0], urow[1], unextrow[0], urow[xres-1],
                              uprevrow[1], uprevrow[xres-1], unextrow[1], unextrow[xres-1],
                              rng, T, J, vrow[0]);

        for (j = 1; j < xres-1; j++) {
            unewrow[j] = mc_step8(urow[j],
                                  uprevrow[j], urow[j-1], urow[j+1], unextrow[j],
                                  uprevrow[j-1], uprevrow[j+1], unextrow[j-1], unextrow[j+1],
                                  rng, T, J, vrow[j]);
        }

        j = xres-1;
        unewrow[j] = mc_step8(urow[j],
                              uprevrow[j], urow[0], urow[xres-2], unextrow[j],
                              uprevrow[0], uprevrow[xres-2], unextrow[0], unextrow[xres-2],
                              rng, T, J, vrow[j]);
    }

    /* Bottom row. */
    j = i = n-xres;
    unew[j] = mc_step8(u[j],
                       u[j+1], u[0], u[n-1], u[j-xres],
                       u[j - xres-1], u[j - xres+1], u[1], u[xres-1],
                       rng, T, J, v[j]);

    for (j = 1; j < xres-1; j++) {
        unew[i + j] = mc_step8(u[i + j],
                               u[i + j-1], u[i + j+1], u[i + j-xres], u[j],
                               u[i + j-xres-1], u[i + j-xres+1], u[j-1], u[j+1],
                               rng, T, J, v[i + j]);
    }

    j = n-1;
    unew[j] = mc_step8(u[j],
                       u[i], u[j-xres], u[xres-1], u[j-1],
                       u[0], u[xres-2], u[i-2], u[i-xres],
                       rng, T, J, v[j]);
}

static inline gdouble
v_rk4_step(gdouble v, gint u, gdouble mu, gdouble nu, gdouble dt)
{
    gdouble p = (mu*u - v - nu)*dt;
    return v + p*(1.0 - p*(0.5 - p*(1.0/6.0 - p/24.0)));
}

static void
field_rk4_step(GwyDataField *vfield, const gint *u,
               const DomainSynthArgs *args)
{
    gdouble mu = args->mu, nu = args->nu, dt = args->dt * 1e-3;
    guint xres = vfield->xres, yres = vfield->yres, n = xres*yres;
    gdouble *v = vfield->data;
    guint k;

    for (k = 0; k < n; k++)
        v[k] = v_rk4_step(v[k], u[k], mu, nu, dt);
}

static void
ufield_to_data_field(const gint *u, const gint *ubuf,
                     GwyDataField *dfield)
{
    guint xres = gwy_data_field_get_xres(dfield);
    guint yres = gwy_data_field_get_yres(dfield);
    guint k;

    for (k = 0; k < xres*yres; k++)
        dfield->data[k] = 0.5*(u[k] + ubuf[k]);

    gwy_data_field_invalidate(dfield);
    gwy_data_field_data_changed(dfield);
}

static gboolean
domain_synth_do(const DomainSynthArgs *args,
                GwyDataField *ufield,
                GwyDataField *vfield,
                GRand *rng,
                gdouble preview_time)
{
    gint xres, yres;
    gulong i;
    gdouble lasttime = 0.0, lastpreviewtime = 0.0, currtime;
    GTimer *timer;
    gint *u, *ubuf;
    guint k;
    gboolean finished = FALSE;

    timer = g_timer_new();

    xres = gwy_data_field_get_xres(ufield);
    yres = gwy_data_field_get_yres(ufield);

    gwy_app_wait_set_message(_("Running computation..."));
    gwy_app_wait_set_fraction(0.0);

    gwy_data_field_clear(vfield);
    u = g_new(gint, xres*yres);
    ubuf = g_new(gint, xres*yres);
    for (k = 0; k < xres*yres; k++)
        u[k] = (gint)(ufield->data[k]);

    for (i = 0; i < args->niters; i++) {
        field_mc_step8(vfield, u, ubuf, args, rng);
        field_rk4_step(vfield, ubuf, args);
        field_mc_step8(vfield, ubuf, u, args, rng);
        field_rk4_step(vfield, u, args);

        if (i % 20 == 0) {
            currtime = g_timer_elapsed(timer, NULL);
            if (currtime - lasttime >= 0.25) {
                if (!gwy_app_wait_set_fraction((gdouble)i/args->niters))
                    goto fail;
                lasttime = currtime;

                if (args->animated
                    && currtime - lastpreviewtime >= preview_time) {
                    ufield_to_data_field(u, ubuf, ufield);
                    gwy_data_field_invalidate(vfield);
                    gwy_data_field_data_changed(vfield);
                    lastpreviewtime = lasttime;
                }
            }
        }
    }

    ufield_to_data_field(u, ubuf, ufield);
    gwy_data_field_invalidate(vfield);
    gwy_data_field_data_changed(vfield);
    finished = TRUE;

fail:
    g_timer_destroy(timer);
    g_free(u);
    g_free(ubuf);

    return finished;
}

static const gchar prefix[]               = "/module/domain_synth";
static const gchar active_page_key[]      = "/module/domain_synth/active_page";
static const gchar randomize_key[]        = "/module/domain_synth/randomize";
static const gchar seed_key[]             = "/module/domain_synth/seed";
static const gchar animated_key[]         = "/module/domain_synth/animated";
static const gchar T_key[]                = "/module/domain_synth/T";
static const gchar J_key[]                = "/module/domain_synth/J";
static const gchar mu_key[]               = "/module/domain_synth/mu";
static const gchar nu_key[]               = "/module/domain_synth/nu";
static const gchar dt_key[]               = "/module/domain_synth/dt";
static const gchar quantity_key[]         = "/module/domain_synth/quantity";
static const gchar preview_quantity_key[] = "/module/domain_synth/preview_quantity";
static const gchar niters_key[]           = "/module/domain_synth/niters";
static const gchar height_key[]           = "/module/domain_synth/height";

static void
domain_synth_sanitize_args(DomainSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->animated = !!args->animated;
    args->niters = MIN(args->niters, 10000);
    args->T = CLAMP(args->T, 0.001, 2.0);
    args->J = CLAMP(args->J, 0.001, 100.0);
    args->mu = CLAMP(args->mu, 0.001, 100.0);
    args->nu = CLAMP(args->nu, -1.0, 1.0);
    args->dt = CLAMP(args->dt, 0.001, 100.0);
    args->height = CLAMP(args->height, 0.001, 10000.0);
    args->quantity &= QUANTITY_MASK;
    args->preview_quantity = MIN(args->preview_quantity, QUANTITY_NTYPES-1);
}

static void
domain_synth_load_args(GwyContainer *container,
                       DomainSynthArgs *args,
                       GwyDimensionArgs *dimsargs)
{
    *args = domain_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_boolean_by_name(container, animated_key,
                                      &args->animated);
    gwy_container_gis_int32_by_name(container, niters_key, &args->niters);
    gwy_container_gis_double_by_name(container, T_key, &args->T);
    gwy_container_gis_double_by_name(container, J_key, &args->J);
    gwy_container_gis_double_by_name(container, mu_key, &args->mu);
    gwy_container_gis_double_by_name(container, nu_key, &args->nu);
    gwy_container_gis_double_by_name(container, dt_key, &args->dt);
    gwy_container_gis_enum_by_name(container, quantity_key, &args->quantity);
    gwy_container_gis_enum_by_name(container, preview_quantity_key,
                                   &args->preview_quantity);
    gwy_container_gis_double_by_name(container, height_key, &args->height);
    domain_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
domain_synth_save_args(GwyContainer *container,
                       const DomainSynthArgs *args,
                       const GwyDimensionArgs *dimsargs)
{
    gwy_container_set_int32_by_name(container, active_page_key,
                                    args->active_page);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
    gwy_container_set_boolean_by_name(container, animated_key,
                                      args->animated);
    gwy_container_set_int32_by_name(container, niters_key, args->niters);
    gwy_container_set_double_by_name(container, T_key, args->T);
    gwy_container_set_double_by_name(container, J_key, args->J);
    gwy_container_set_double_by_name(container, mu_key, args->mu);
    gwy_container_set_double_by_name(container, nu_key, args->nu);
    gwy_container_set_double_by_name(container, dt_key, args->dt);
    gwy_container_set_enum_by_name(container, quantity_key, args->quantity);
    gwy_container_set_enum_by_name(container, preview_quantity_key,
                                   args->preview_quantity);
    gwy_container_set_double_by_name(container, height_key, args->height);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
