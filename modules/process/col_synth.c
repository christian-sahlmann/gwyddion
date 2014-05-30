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
#include <libprocess/filters.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define COL_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

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

typedef enum {
    RELAX_WEAK = 0,
    RELAX_STRONG = 1,
} RelaxationType;

typedef struct _ObjSynthControls ColSynthControls;

typedef struct {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;   /* Always false */
    gboolean animated;
    gdouble coverage;
    gdouble theta;
    gdouble theta_spread;
    gdouble phi;
    gdouble phi_spread;
    gdouble height;
    gdouble height_noise;
    RelaxationType relaxation;
} ColSynthArgs;

struct _ObjSynthControls {
    ColSynthArgs *args;
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
    GtkObject *theta;
    GtkObject *theta_spread;
    GtkObject *phi;
    GtkObject *phi_spread;
    GtkObject *height;
    GtkWidget *height_units;
    GtkObject *height_noise;
    GtkWidget *relaxation;
    GwyContainer *mydata;
    GwyDataField *surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
};

static gboolean   module_register         (void);
static void       col_synth               (GwyContainer *data,
                                           GwyRunType run);
static void       run_noninteractive      (ColSynthArgs *args,
                                           const GwyDimensionArgs *dimsargs,
                                           GwyContainer *data,
                                           GwyDataField *dfield,
                                           gint oldid,
                                           GQuark quark);
static gboolean   col_synth_dialog        (ColSynthArgs *args,
                                           GwyDimensionArgs *dimsargs,
                                           GwyContainer *data,
                                           GwyDataField *dfield,
                                           gint id);
static GtkWidget* relaxation_selector_new (ColSynthControls *controls);
static void       update_controls         (ColSynthControls *controls,
                                           ColSynthArgs *args);
static void       page_switched           (ColSynthControls *controls,
                                           GtkNotebookPage *page,
                                           gint pagenum);
static void       update_values           (ColSynthControls *controls);
static void       relaxation_type_selected(GtkComboBox *combo,
                                           ColSynthControls *controls);
static void       col_synth_invalidate    (ColSynthControls *controls);
static void       scale_to_unit_cubes     (GwyDataField *dfield,
                                           gdouble *rx,
                                           gdouble *ry);
static void       scale_from_unit_cubes   (GwyDataField *dfield,
                                           gdouble rx,
                                           gdouble ry);
static void       preview                 (ColSynthControls *controls);
static gboolean   col_synth_do            (const ColSynthArgs *args,
                                           GwyDataField *dfield,
                                           gdouble preview_time);
static gboolean   col_synth_trace         (GwyDataField *dfield,
                                           gdouble x,
                                           gdouble y,
                                           gdouble z,
                                           gdouble theta,
                                           gdouble phi,
                                           gdouble size,
                                           RelaxationType relaxation,
                                           GwyRandGenSet *rngset,
                                           gdouble *zmax);
static void       col_synth_load_args     (GwyContainer *container,
                                           ColSynthArgs *args,
                                           GwyDimensionArgs *dimsargs);
static void       col_synth_save_args     (GwyContainer *container,
                                           const ColSynthArgs *args,
                                           const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS ColSynthControls
#define GWY_SYNTH_INVALIDATE(controls) col_synth_invalidate(controls)

#include "synth.h"

static const ColSynthArgs col_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, FALSE, TRUE,
    10.0,
    0.0, 1.0,
    0.0, 1.0,
    1.0, 0.0,
    RELAX_WEAK,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates columnar surfaces by a simple growth algorithm."),
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("col_synth",
                              (GwyProcessFunc)&col_synth,
                              N_("/S_ynthetic/_Columnar..."),
                              GWY_STOCK_SYNTHETIC_COLUMNAR,
                              COL_SYNTH_RUN_MODES,
                              0,
                              N_("Generate columnar surface"));

    return TRUE;
}

static void
col_synth(GwyContainer *data, GwyRunType run)
{
    ColSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & COL_SYNTH_RUN_MODES);
    col_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || col_synth_dialog(&args, &dimsargs, data, dfield, id)) {
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);
    }

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(ColSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint oldid,
                   GQuark quark)
{
    GwyDataField *newfield;
    GwySIUnit *siunit;
    gboolean replace = dimsargs->replace && dfield;
    gboolean add = dimsargs->add && dfield;
    gdouble rx, ry;
    gint newid;
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

    gwy_app_wait_start(gwy_app_find_window_for_channel(data, oldid),
                       _("Starting..."));
    scale_to_unit_cubes(newfield, &rx, &ry);
    ok = col_synth_do(args, newfield, HUGE_VAL);
    scale_from_unit_cubes(newfield, rx, ry);
    gwy_app_wait_finish();

    if (!ok) {
        g_object_unref(newfield);
        return;
    }

    if (replace) {
        gwy_app_undo_qcheckpointv(data, 1, &quark);
        gwy_container_set_object(data, gwy_app_get_data_key_for_id(oldid),
                                 newfield);
        gwy_app_channel_log_add(data, oldid, oldid, "proc::col_synth", NULL);
        g_object_unref(newfield);
        return;
    }

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
    gwy_app_channel_log_add(data, add ? oldid : -1, newid,
                            "proc::col_synth", NULL);
    g_object_unref(newfield);
}

static gboolean
col_synth_dialog(ColSynthArgs *args,
                  GwyDimensionArgs *dimsargs,
                  GwyContainer *data,
                  GwyDataField *dfield_template,
                  gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *hbox2, *check;
    ColSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    controls.pxsize = 1.0;
    dialog = gtk_dialog_new_with_buttons(_("Grow Columnar Surface"),
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

    table = gtk_table_new(19 + (dfield_template ? 1 : 0), 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.coverage = gtk_adjustment_new(args->coverage,
                                           0.1, 1000.0, 0.001, 1.0, 0);
    g_object_set_data(G_OBJECT(controls.coverage), "target", &args->coverage);
    gwy_table_attach_hscale(table, row, _("Co_verage:"), NULL,
                            controls.coverage, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.coverage, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Particle Size")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.height = gtk_adjustment_new(args->height, 0.1, 10.0, 0.1, 1.0, 0);
    g_object_set_data(G_OBJECT(controls.height), "target", &args->height);
    gwy_table_attach_hscale(table, row, _("_Height:"), "px",
                            controls.height, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.height, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    row = gwy_synth_attach_variance(&controls, row,
                                    &controls.height_noise,
                                    &args->height_noise);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Incidence")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    row = gwy_synth_attach_angle(&controls, row, &controls.theta, &args->theta,
                                 0.0, 0.99*G_PI/2.0, _("Inclination"));
    row = gwy_synth_attach_variance(&controls, row,
                                    &controls.theta_spread,
                                    &args->theta_spread);

    row = gwy_synth_attach_angle(&controls, row, &controls.phi, &args->phi,
                                 -G_PI, G_PI, _("Direction"));
    row = gwy_synth_attach_variance(&controls, row,
                                    &controls.phi_spread,
                                    &args->phi_spread);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Options")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.relaxation = relaxation_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("Relaxation type:"), NULL,
                            GTK_OBJECT(controls.relaxation), GWY_HSCALE_WIDGET);

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
                *args = col_synth_defaults;
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

    col_synth_save_args(gwy_app_settings_get(), args, dimsargs);

    g_object_unref(controls.mydata);
    gwy_object_unref(controls.surface);
    gwy_dimensions_free(controls.dims);

    return response == GTK_RESPONSE_OK;
}

static GtkWidget*
relaxation_selector_new(ColSynthControls *controls)
{
    static const GwyEnum relaxation_types[] = {
        { N_("Weak"),   RELAX_WEAK   },
        { N_("Strong"), RELAX_STRONG },
    };
    GtkWidget *combo;

    combo = gwy_enum_combo_box_new(relaxation_types,
                                   G_N_ELEMENTS(relaxation_types),
                                   G_CALLBACK(relaxation_type_selected),
                                   controls, controls->args->relaxation, TRUE);
    return combo;
}

static void
update_controls(ColSynthControls *controls,
                ColSynthArgs *args)
{
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
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->theta),
                             args->theta * 180.0/G_PI);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->theta_spread),
                             args->theta_spread);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->phi),
                             args->phi * 180.0/G_PI);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->phi_spread),
                             args->phi_spread);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->relaxation),
                                  args->relaxation);
}

static void
page_switched(ColSynthControls *controls,
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
update_values(ColSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
    if (controls->height_units)
        gtk_label_set_markup(GTK_LABEL(controls->height_units),
                             dims->zvf->units);
}

static void
relaxation_type_selected(GtkComboBox *combo,
                         ColSynthControls *controls)
{
    controls->args->relaxation = gwy_enum_combo_box_get_active(combo);
}

static void
col_synth_invalidate(G_GNUC_UNUSED ColSynthControls *controls)
{
}

static void
scale_to_unit_cubes(GwyDataField *dfield,
                    gdouble *rx, gdouble *ry)
{
    gdouble r;

    *rx = 1.0/gwy_data_field_get_xmeasure(dfield);
    *ry = 1.0/gwy_data_field_get_ymeasure(dfield);
    gwy_data_field_set_xreal(dfield, dfield->xres);
    gwy_data_field_set_yreal(dfield, dfield->yres);
    r = sqrt((*rx)*(*ry));
    gwy_data_field_multiply(dfield, r);
}

static void
scale_from_unit_cubes(GwyDataField *dfield,
                      gdouble rx, gdouble ry)
{
    gwy_data_field_multiply(dfield, 1.0/sqrt(rx*ry));
    gwy_data_field_set_xreal(dfield, dfield->xres/rx);
    gwy_data_field_set_yreal(dfield, dfield->yres/ry);
}

static void
preview(ColSynthControls *controls)
{
    ColSynthArgs *args = controls->args;
    GwyDataField *dfield;
    gdouble rx, ry;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, TRUE);
    else
        gwy_data_field_clear(dfield);

    scale_to_unit_cubes(dfield, &rx, &ry);

    gwy_app_wait_start(GTK_WINDOW(controls->dialog), _("Starting..."));
    col_synth_do(args, dfield, 1.25);
    gwy_app_wait_finish();

    scale_from_unit_cubes(dfield,rx, ry);
    gwy_data_field_data_changed(dfield);
}

static gboolean
col_synth_do(const ColSynthArgs *args,
             GwyDataField *dfield,
             gdouble preview_time)
{
    RelaxationType relaxation;
    gint xres, yres;
    gulong npart, ip;
    gdouble zmax;
    gdouble lasttime = 0.0, lastpreviewtime = 0.0, currtime;
    GTimer *timer;
    GwyRandGenSet *rngset;
    gboolean finished = FALSE;

    timer = g_timer_new();

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    relaxation = args->relaxation;

    gwy_data_field_add(dfield, -gwy_data_field_get_max(dfield));
    zmax = 0.0;

    npart = args->coverage * xres*yres;
    gwy_app_wait_set_message(_("Depositing particles..."));
    gwy_app_wait_set_fraction(0.0);

    rngset = gwy_rand_gen_set_new(1);
    gwy_rand_gen_set_init(rngset, args->seed);

    for (ip = 0; ip < npart; ip++) {
        gdouble theta, phi, height, x, y, z;

        height = args->height;
        if (args->height_noise)
            height *= exp(gwy_rand_gen_set_gaussian(rngset, 0,
                                                    args->height_noise));

        theta = args->theta;
        if (args->theta_spread) {
            gdouble cth;

            do {
                cth = (cos(theta)
                       + (gwy_rand_gen_set_gaussian(rngset, 0,
                                                    G_PI*args->theta_spread)));
            } while (cth < 0.0 || cth > 0.99);

            theta = acos(1.0 - cth);
        }

        phi = args->phi;
        if (args->phi_spread)
            phi += gwy_rand_gen_set_gaussian(rngset, 0,
                                             2.0*G_PI*args->phi_spread);

        x = xres*gwy_rand_gen_set_double(rngset, 0);
        y = yres*gwy_rand_gen_set_double(rngset, 0);
        /* XXX: Make the starting height also a parameter? */
        z = zmax + 5.0;

        col_synth_trace(dfield, x, y, z, theta, phi, height,
                        relaxation, rngset, &zmax);

        if (ip % 1000 == 0) {
            currtime = g_timer_elapsed(timer, NULL);
            if (currtime - lasttime >= 0.25) {
                if (!gwy_app_wait_set_fraction((gdouble)ip/npart))
                    goto fail;
                lasttime = currtime;

                if (args->animated
                    && currtime - lastpreviewtime >= preview_time) {
                    gwy_data_field_data_changed(dfield);
                    lastpreviewtime = lasttime;
                }
            }
        }
    }

    finished = TRUE;

fail:
    g_timer_destroy(timer);
    gwy_rand_gen_set_free(rngset);

    return finished;
}

static gboolean
col_synth_trace(GwyDataField *dfield,
                gdouble x, gdouble y, gdouble z,
                gdouble theta, gdouble phi, gdouble size,
                RelaxationType relaxation,
                GwyRandGenSet *rngset,
                gdouble *zmax)
{
    gdouble kx, ky, kz;
    gint i, j, k1, k2, k, xres, yres;
    gdouble xnew, ynew, znew;
    gint inew, jnew, iold, jold;
    gdouble *data;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    kx = cos(phi);
    ky = sin(phi);
    kz = -1.0/tan(fmax(theta, 1e-18));

    jnew = jold = (gint)floor(x);
    inew = iold = (gint)floor(y);

    do {
        i = inew;
        j = jnew;
        xnew = x + 0.4*kx;
        ynew = y + 0.4*ky;
        znew = z + 0.4*kz;
        jnew = (gint)floor(xnew);
        inew = (gint)floor(ynew);
        if (jnew != j || inew != i) {
            if (jnew < 0) {
                xnew += xres;
                jnew += xres;
            }
            else if (jnew >= xres) {
                xnew -= xres;
                jnew -= xres;
            }

            if (inew < 0) {
                ynew += yres;
                inew += yres;
            }
            else if (inew >= yres) {
                ynew -= yres;
                inew -= yres;
            }

            jold = j;
            iold = i;
        }

        x = xnew;
        y = ynew;
        z = znew;
    } while (z > data[inew*xres + jnew]);

    /* Relaxation.  This is important as it prevents exponential growth of
     * spikes with periodic boundary conditions. */
    k1 = iold*xres + jold;
    k2 = inew*xres + jnew;
    if (relaxation == RELAX_STRONG) {
        for (i = -1; i <= 1; i++) {
            for (j = -1; j <= 1; j++) {
                if (!j && !i)
                    continue;

                k = ((inew + yres + i) % yres)*xres + (jnew + xres + j) % xres;
                if (data[k] < data[k2]) {
                    if (gwy_rand_gen_set_double(rngset, 0) < 0.5/(i*i + j*j))
                        k2 = k;
                }
            }
        }
    }

    k = (data[k2] < data[k1]) ? k2 : k1;
    data[k] += size;
    if (data[k] > *zmax)
        *zmax = data[k];

    return TRUE;
}

static const gchar prefix[]           = "/module/col_synth";
static const gchar coverage_key[]     = "/module/col_synth/coverage";
static const gchar active_page_key[]  = "/module/col_synth/active_page";
static const gchar randomize_key[]    = "/module/col_synth/randomize";
static const gchar seed_key[]         = "/module/col_synth/seed";
static const gchar animated_key[]     = "/module/col_synth/animated";
static const gchar height_key[]       = "/module/col_synth/height";
static const gchar height_noise_key[] = "/module/col_synth/height_noise";
static const gchar theta_key[]        = "/module/col_synth/theta";
static const gchar theta_spread_key[] = "/module/col_synth/theta_spread";
static const gchar phi_key[]          = "/module/col_synth/phi";
static const gchar phi_spread_key[]   = "/module/col_synth/phi_spread";
static const gchar relaxation_key[]   = "/module/col_synth/relaxation";

static void
col_synth_sanitize_args(ColSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->animated = !!args->animated;
    args->coverage = CLAMP(args->coverage, 0.1, 1000.0);
    args->height = CLAMP(args->height, 0.001, 10000.0);
    args->height_noise = CLAMP(args->height_noise, 0.0, 1.0);
    args->theta = CLAMP(args->theta, 0, G_PI/2.0);
    args->theta_spread = CLAMP(args->theta_spread, 0.0, 1.0);
    args->phi = CLAMP(args->phi, -G_PI, G_PI);
    args->phi_spread = CLAMP(args->phi_spread, 0.0, 1.0);
    args->relaxation = MIN(args->relaxation, RELAX_STRONG);
}

static void
col_synth_load_args(GwyContainer *container,
                    ColSynthArgs *args,
                    GwyDimensionArgs *dimsargs)
{
    *args = col_synth_defaults;

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
    gwy_container_gis_double_by_name(container, theta_key, &args->theta);
    gwy_container_gis_double_by_name(container, theta_spread_key,
                                     &args->theta_spread);
    gwy_container_gis_double_by_name(container, phi_key, &args->phi);
    gwy_container_gis_double_by_name(container, phi_spread_key,
                                     &args->phi_spread);
    gwy_container_gis_double_by_name(container, coverage_key, &args->coverage);
    gwy_container_gis_enum_by_name(container, relaxation_key, &args->relaxation);
    col_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
col_synth_save_args(GwyContainer *container,
                    const ColSynthArgs *args,
                    const GwyDimensionArgs *dimsargs)
{
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
    gwy_container_set_double_by_name(container, theta_key, args->theta);
    gwy_container_set_double_by_name(container, theta_spread_key,
                                     args->theta_spread);
    gwy_container_set_double_by_name(container, phi_key, args->phi);
    gwy_container_set_double_by_name(container, phi_spread_key,
                                     args->phi_spread);
    gwy_container_set_double_by_name(container, coverage_key, args->coverage);
    gwy_container_set_enum_by_name(container, relaxation_key, args->relaxation);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
