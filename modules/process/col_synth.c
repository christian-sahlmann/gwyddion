/*
 *  @(#) $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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

#define COL_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

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

typedef struct _ObjSynthControls ColSynthControls;

typedef struct {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;
    gdouble theta;
    gdouble theta_spread;
    gdouble phi;
    gdouble phi_spread;
    gdouble height;
    gdouble height_noise;
    // TODO: Relaxation algorithm
    gdouble coverage;
} ColSynthArgs;

struct _ObjSynthControls {
    ColSynthArgs *args;
    GwyDimensions *dims;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
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
    GtkWidget *height_init;
    GtkObject *height_noise;
    GwyContainer *mydata;
    GwyDataField *surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
    gulong sid;
};

static gboolean module_register     (void);
static void     col_synth           (GwyContainer *data,
                                     GwyRunType run);
static void     run_noninteractive  (ColSynthArgs *args,
                                     const GwyDimensionArgs *dimsargs,
                                     GwyContainer *data,
                                     GwyDataField *dfield,
                                     gint oldid,
                                     GQuark quark);
static gboolean col_synth_dialog    (ColSynthArgs *args,
                                     GwyDimensionArgs *dimsargs,
                                     GwyContainer *data,
                                     GwyDataField *dfield,
                                     gint id);
static void     update_controls     (ColSynthControls *controls,
                                     ColSynthArgs *args);
static void     page_switched       (ColSynthControls *controls,
                                     GtkNotebookPage *page,
                                     gint pagenum);
static void     update_values       (ColSynthControls *controls);
static void     height_init_clicked (ColSynthControls *controls);
static void     col_synth_invalidate(ColSynthControls *controls);
static gboolean preview_gsource     (gpointer user_data);
static void     preview             (ColSynthControls *controls);
static void     col_synth_do        (const ColSynthArgs *args,
                                     const GwyDimensionArgs *dimsargs,
                                     GwyDataField *dfield);
static gboolean col_synth_trace     (GwyDataField *dfield,
                                     gdouble x,
                                     gdouble y,
                                     gdouble z,
                                     gdouble theta,
                                     gdouble phi,
                                     gdouble size,
                                     gdouble *zmax);
static gdouble  rand_gen_gaussian   (GRand *rng,
                                     gdouble sigma);
static void     col_synth_load_args (GwyContainer *container,
                                     ColSynthArgs *args,
                                     GwyDimensionArgs *dimsargs);
static void     col_synth_save_args (GwyContainer *container,
                                     const ColSynthArgs *args,
                                     const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS ColSynthControls
#define GWY_SYNTH_INVALIDATE(controls) col_synth_invalidate(controls)

#include "synth.h"

static const ColSynthArgs col_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, FALSE,
    0.0, 0.0,
    0.0, 2.0*G_PI,
    1.0, 0.0,
    10.0,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates columnar surfaces by a simple growth algorithm."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("col_synth",
                              (GwyProcessFunc)&col_synth,
                              N_("/S_ynthetic/_Columnar..."),
                              NULL,
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
        col_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);
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

        gwy_app_channel_log_add(data, oldid, oldid, "proc::col_synth", NULL);
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

    col_synth_do(args, dimsargs, dfield);

    if (!replace) {
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
        gwy_app_channel_log_add(data, add ? oldid : -1, newid,
                                "proc::col_synth", NULL);
    }

    g_object_unref(dfield);
}

static gboolean
col_synth_dialog(ColSynthArgs *args,
                  GwyDimensionArgs *dimsargs,
                  GwyContainer *data,
                  GwyDataField *dfield_template,
                  gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook;
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

    gtk_box_pack_start(GTK_BOX(vbox),
                       gwy_synth_instant_updates_new(&controls,
                                                     &controls.update_now,
                                                     &controls.update,
                                                     &args->update),
                       FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(controls.update, TRUE);
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
                                 G_CALLBACK(col_synth_invalidate), &controls);

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

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);
    col_synth_invalidate(&controls);

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
                *args = col_synth_defaults;
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

static void
update_controls(ColSynthControls *controls,
                ColSynthArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->coverage),
                             args->coverage);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height), args->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height_noise),
                             args->height_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->theta), args->theta);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->theta_spread),
                             args->theta_spread);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->phi), args->phi);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->phi_spread),
                             args->phi_spread);
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
height_init_clicked(ColSynthControls *controls)
{
    gdouble mag = pow10(controls->dims->args->zpow10);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height),
                             controls->zscale/mag);
}

static void
col_synth_invalidate(ColSynthControls *controls)
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
    ColSynthControls *controls = (ColSynthControls*)user_data;
    controls->sid = 0;

    preview(controls);

    return FALSE;
}

static void
preview(ColSynthControls *controls)
{
    ColSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, FALSE);
    else
        gwy_data_field_clear(dfield);

    gwy_app_wait_start(GTK_WINDOW(controls->dialog), _("Starting..."));
    col_synth_do(args, controls->dims->args, dfield);
    gwy_app_wait_finish();

    gwy_data_field_data_changed(dfield);
}

static void
col_synth_do(const ColSynthArgs *args,
             const GwyDimensionArgs *dimsargs,
             GwyDataField *dfield)
{
    GwyDataField *workspace;
    gint xres, yres;
    gulong npart, ip;
    gdouble zmax;
    gdouble lasttime = 0.0, lastpreviewtime = 0.0, currtime;
    GTimer *timer;
    GRand *rng;

    rand_gen_gaussian(NULL, 0.0);
    timer = g_timer_new();

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    workspace = gwy_data_field_duplicate(dfield);
    gwy_data_field_add(workspace, -gwy_data_field_get_max(workspace));
    zmax = 0.0;

    npart = args->coverage * xres*yres;
    gwy_app_wait_set_message(_("Depositing particles..."));
    gwy_app_wait_set_fraction(0.0);

    rng = g_rand_new();
    g_rand_set_seed(rng, args->seed);

    for (ip = 0; ip < npart; ip++) {
        gdouble theta, phi, height, x, y, z;

        height = args->height;
        if (args->height_noise)
            height *= exp(rand_gen_gaussian(rng, args->height_noise));

        theta = args->theta;
        if (args->theta_spread) {
            gdouble cth;

            do {
                cth = cos(theta) + rand_gen_gaussian(rng,
                                                     G_PI*args->theta_spread);
            } while (cth < 0.0 || cth > 0.99);

            theta = acos(1.0 - cth);
        }

        phi = args->phi;
        if (args->phi_spread)
            phi += rand_gen_gaussian(rng, 2.0*G_PI*args->phi_spread);

        x = xres*g_rand_double(rng);
        y = yres*g_rand_double(rng);
        /* XXX: Make the starting height also a parameter? */
        z = zmax + 5.0;

        col_synth_trace(workspace, x, y, z, theta, phi, height, &zmax);

        if (ip % 1000 == 0) {
            currtime = g_timer_elapsed(timer, NULL);
            if (currtime - lasttime >= 0.2) {
                if (!gwy_app_wait_set_fraction((gdouble)ip/npart))
                    goto fail;
                lasttime = g_timer_elapsed(timer, NULL);

                /* XXX XXX XXX: Only do this for preview. */
                if (currtime - lastpreviewtime >= 1.0) {
                    gwy_data_field_copy(workspace, dfield, FALSE);
                    gwy_data_field_data_changed(dfield);
                    lastpreviewtime = lasttime;
                }
            }
        }
    }

    gwy_data_field_copy(workspace, dfield, FALSE);

fail:
    g_object_unref(workspace);
    g_timer_destroy(timer);
    g_rand_free(rng);
}

typedef struct {
    guint i, j;       /* Position in the grid (row, column). */
    gdouble x, y, z;  /* Fractional part of the position. */
    gdouble a, q;     /* dy/dx and dz/dx if dx > 0 is dominant; analogous
                         in other cases.  They are always positive, the
                         actual movement depends on the direction octant. */
    gboolean dominant_edge;
} Particle;

typedef void (*ParticleMoveFunc)(Particle *p);

/* Dominant direction: positive x.  Other direction: positive y. */
static inline void
particle_move_xpyp(Particle *p)
{
    if (p->dominant_edge) {
        if (p->y <= 1.0 - p->a) {
            p->y += p->a;
            p->z += p->q;
            p->j++;
        }
        else {
            gdouble m = (1.0 - p->y)/p->a;
            p->x += m;
            p->y = 0.0;
            p->z += p->q*m;
            p->dominant_edge = FALSE;
            p->i++;
        }
    }
    else {
        gdouble m = (1.0 - p->x);
        p->x = 0.0;
        p->y = p->a*m;
        p->z += p->q*m;
        p->dominant_edge = TRUE;
        p->j++;
    }
}

/* Dominant direction: positive x.  Other direction: negative y. */
static inline void
particle_move_xpym(Particle *p)
{
    if (p->dominant_edge) {
        if (p->y >= p->a) {
            p->y -= p->a;
            p->z += p->q;
            p->j++;
        }
        else {
            gdouble m = p->y/p->a;
            p->x += m;
            p->y = 1.0;
            p->z += p->q*m;
            p->dominant_edge = FALSE;
            p->i--;
        }
    }
    else {
        gdouble m = (1.0 - p->x);
        p->x = 0.0;
        p->y = 1.0 - p->a*m;
        p->z += p->q*m;
        p->dominant_edge = TRUE;
        p->j++;
    }
}

/* Dominant direction: negative x.  Other direction: positive y. */
static inline void
particle_move_xmyp(Particle *p)
{
    if (p->dominant_edge) {
        if (p->y <= 1.0 - p->a) {
            p->y += p->a;
            p->z += p->q;
            p->j--;
        }
        else {
            gdouble m = (1.0 - p->y)/p->a;
            p->x -= m;
            p->y = 0.0;
            p->z += p->q*m;
            p->dominant_edge = FALSE;
            p->i++;
        }
    }
    else {
        gdouble m = p->x;
        p->x = 1.0;
        p->y = p->a*m;
        p->z += p->q*m;
        p->dominant_edge = TRUE;
        p->j--;
    }
}

/* Dominant direction: negative x.  Other direction: negative y. */
static inline void
particle_move_xmym(Particle *p)
{
    if (p->dominant_edge) {
        if (p->y >= p->a) {
            p->y -= p->a;
            p->z += p->q;
            p->j--;
        }
        else {
            gdouble m = p->y/p->a;
            p->x -= m;
            p->y = 1.0;
            p->z += p->q*m;
            p->dominant_edge = FALSE;
            p->i--;
        }
    }
    else {
        gdouble m = p->x;
        p->x = 1.0;
        p->y = 1.0 - p->a*m;
        p->z += p->q*m;
        p->dominant_edge = TRUE;
        p->j--;
    }
}

/* Dominant direction: positive y.  Other direction: positive x. */
static inline void
particle_move_ypxp(Particle *p)
{
    if (p->dominant_edge) {
        if (p->x <= 1.0 - p->a) {
            p->x += p->a;
            p->z += p->q;
            p->i++;
        }
        else {
            gdouble m = (1.0 - p->x)/p->a;
            p->y += m;
            p->x = 0.0;
            p->z += p->q*m;
            p->dominant_edge = FALSE;
            p->j++;
        }
    }
    else {
        gdouble m = (1.0 - p->y);
        p->y = 0.0;
        p->x = p->a*m;
        p->z += p->q*m;
        p->dominant_edge = TRUE;
        p->i++;
    }
}

/* Dominant direction: positive y.  Other direction: negative x. */
static inline void
particle_move_ypxm(Particle *p)
{
    if (p->dominant_edge) {
        if (p->x >= p->a) {
            p->x -= p->a;
            p->z += p->q;
            p->i++;
        }
        else {
            gdouble m = p->x/p->a;
            p->y += m;
            p->x = 1.0;
            p->z += p->q*m;
            p->dominant_edge = FALSE;
            p->j--;
        }
    }
    else {
        gdouble m = (1.0 - p->y);
        p->y = 0.0;
        p->x = 1.0 - p->a*m;
        p->z += p->q*m;
        p->dominant_edge = TRUE;
        p->i++;
    }
}

/* Dominant direction: negative y.  Other direction: positive x. */
static inline void
particle_move_ymxp(Particle *p)
{
    if (p->dominant_edge) {
        if (p->x <= 1.0 - p->a) {
            p->x += p->a;
            p->z += p->q;
            p->i--;
        }
        else {
            gdouble m = (1.0 - p->x)/p->a;
            p->y -= m;
            p->x = 0.0;
            p->z += p->q*m;
            p->dominant_edge = FALSE;
            p->j++;
        }
    }
    else {
        gdouble m = p->y;
        p->y = 1.0;
        p->x = p->a*m;
        p->z += p->q*m;
        p->dominant_edge = TRUE;
        p->i--;
    }
}

/* Dominant direction: negative y.  Other direction: negative x. */
static inline void
particle_move_ymxm(Particle *p)
{
    if (p->dominant_edge) {
        if (p->x >= p->a) {
            p->x -= p->a;
            p->z += p->q;
            p->i--;
        }
        else {
            gdouble m = p->x/p->a;
            p->y -= m;
            p->x = 1.0;
            p->z += p->q*m;
            p->dominant_edge = FALSE;
            p->j--;
        }
    }
    else {
        gdouble m = p->y;
        p->y = 1.0;
        p->x = 1.0 - p->a*m;
        p->z += p->q*m;
        p->dominant_edge = TRUE;
        p->i--;
    }
}

static gboolean
col_synth_trace2(GwyDataField *dfield,
                gdouble x, gdouble y, gdouble z,
                gdouble theta, gdouble phi, gdouble size,
                gdouble *zmax)
{
    ParticleMoveFunc move, move_back;
    Particle p;
    gdouble kx, ky, kz;
    guint k, xres, yres;
    gdouble *data;

    kx = cos(phi);
    ky = sin(phi);
    kz = -1.0/tan(theta);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    p.j = floor(x);
    p.i = floor(y);
    p.x = x - p.j;
    p.y = y - p.i;
    p.z = z;
    p.dominant_edge = TRUE;
    p.a = fmin(fabs(kx), fabs(ky))/fmax(fabs(kx), fabs(ky));
    p.q = kz/fmax(fabs(kx), fabs(ky));

    if (kx >= 0.0) {
        if (ky >= 0.0) {
            if (kx >= ky) {
                move = &particle_move_xpyp;
                move_back = &particle_move_xmym;
            }
            else {
                move = &particle_move_ypxp;
                move_back = &particle_move_ymxm;
            }
        }
        else {
            if (kx >= -ky) {
                move = &particle_move_xpym;
                move_back = &particle_move_xmyp;
            }
            else {
                move = &particle_move_ymxp;
                move_back = &particle_move_ypxm;
            }
        }
    }
    else {
        if (ky >= 0.0) {
            if (-kx >= ky) {
                move = &particle_move_xmyp;
                move_back = &particle_move_xpym;
            }
            else {
                move = &particle_move_ypxm;
                move_back = &particle_move_ymxp;
            }
        }
        else {
            if (-kx >= -ky) {
                move = &particle_move_xmym;
                move_back = &particle_move_xpyp;
            }
            else {
                move = &particle_move_ymxm;
                move_back = &particle_move_ypxp;
            }
        }
    }

    do {
        move(&p);
        if (p.j >= xres || p.i >= yres)
            return FALSE;
        k = p.i*xres + p.j;
    } while (p.z > data[k]);

    move_back(&p);
    /* Rounding error? */
    if (p.j >= xres || p.i >= yres) {
        //g_warning("Move back went astray.");
        return FALSE;
    }
    k = p.i*xres + p.j;
    data[k] += size;
    if (data[k] > *zmax)
        *zmax = data[k];

    return TRUE;
}

static gboolean
col_synth_trace(GwyDataField *dfield,
                gdouble x, gdouble y, gdouble z,
                gdouble theta, gdouble phi, gdouble size,
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
    if (data[k2] < data[k1])
        k = k2;
    else
        k = k1;

    data[k] += size;
    if (data[k] > *zmax)
        *zmax = data[k];

    return TRUE;
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

static const gchar prefix[]           = "/module/col_synth";
static const gchar active_page_key[]  = "/module/col_synth/active_page";
static const gchar update_key[]       = "/module/col_synth/update";
static const gchar randomize_key[]    = "/module/col_synth/randomize";
static const gchar seed_key[]         = "/module/col_synth/seed";
static const gchar height_key[]       = "/module/col_synth/height";
static const gchar height_noise_key[] = "/module/col_synth/height_noise";
static const gchar theta_key[]        = "/module/col_synth/theta";
static const gchar theta_spread_key[] = "/module/col_synth/theta_spread";
static const gchar phi_key[]          = "/module/col_synth/phi";
static const gchar phi_spread_key[]   = "/module/col_synth/phi_spread";
static const gchar coverage_key[]     = "/module/col_synth/coverage";

static void
col_synth_sanitize_args(ColSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = FALSE;  /* Never switch it on. */
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->height = CLAMP(args->height, 0.001, 10000.0);
    args->height_noise = CLAMP(args->height_noise, 0.0, 1.0);
    args->theta = CLAMP(args->theta, 0, G_PI/2.0);
    args->theta_spread = CLAMP(args->theta_spread, 0.0, 1.0);
    args->phi = CLAMP(args->phi, -G_PI, G_PI);
    args->phi_spread = CLAMP(args->phi_spread, 0.0, 1.0);
    args->coverage = CLAMP(args->coverage, 0.1, 1000.0);
}

static void
col_synth_load_args(GwyContainer *container,
                    ColSynthArgs *args,
                    GwyDimensionArgs *dimsargs)
{
    *args = col_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
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
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
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

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
