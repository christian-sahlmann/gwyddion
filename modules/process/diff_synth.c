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
#include <libprocess/grains.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define DIFF_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

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
    PAGE_NPAGES
};

typedef enum {
    GRAPH_VAR = 0,
    GRAPH_NFLAGS,
} GraphFlags;

typedef enum {
    NEIGH_UP        = 0,
    NEIGH_LEFT      = 1,
    NEIGH_RIGHT     = 2,
    NEIGH_DOWN      = 3,
    NEIGH_SCHWOEBEL = 4,   /* Bit offset indicating Schwoebel barrier. */
} ParticleNeighbours;

/* The model permits one movable particle on top of another.  However, we do
 * not keep track which is which and always assume the particle we are
 * processing right now is the top one.  This adds some mobility boost to such
 * particles and breaks these stacks quickly.  No representation of neighbour
 * relations in the z-direction is then necessary. */
typedef struct {
    guint col;
    guint row;
    guint k;
    guint kup;
    guint kleft;
    guint kright;
    guint kdown;
    /* Nehgibours blocking movement. */
    guint nneigh;
    guint neighbours;
} Particle;

typedef struct {
    guint *hfield;
    guint xres;
    guint yres;
    GArray *particles;
    GRand *rng;
    gdouble flux;        /* Linear */
    gdouble schwoebel;   /* Linear */
    gdouble fluxperiter;
    gdouble fluence;
    guint64 iter;
    gboolean use_schwoebel;
    gdouble p_stick[5];
    gdouble p_break[5];
} DiffSynthState;

typedef struct _DiffSynthControls DiffSynthControls;

typedef struct {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;   /* Always false */
    gboolean animated;
    gdouble height;
    gdouble coverage;
    gdouble flux;      /* Log10 */
    gdouble p_stick;
    gdouble p_break;
    gdouble schwoebel; /* Log10 */
    gboolean graph_flags[GRAPH_NFLAGS];
} DiffSynthArgs;

struct _DiffSynthControls {
    DiffSynthArgs *args;
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
    GtkObject *flux;
    GtkObject *p_stick;
    GtkObject *p_break;
    GtkObject *schwoebel;
    GtkObject *height;
    GtkWidget *height_units;
    GtkWidget *height_init;
    GtkWidget *graph_flags[GRAPH_NFLAGS];
    GwyContainer *mydata;
    GwyDataField *surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
};

static gboolean           module_register          (void);
static void               diff_synth               (GwyContainer *data,
                                                    GwyRunType run);
static void               run_noninteractive       (DiffSynthArgs *args,
                                                    const GwyDimensionArgs *dimsargs,
                                                    GwyContainer *data,
                                                    GwyDataField *dfield,
                                                    gint oldid,
                                                    GQuark quark);
static gboolean           diff_synth_dialog        (DiffSynthArgs *args,
                                                    GwyDimensionArgs *dimsargs,
                                                    GwyContainer *data,
                                                    GwyDataField *dfield,
                                                    gint id);
static void               update_controls          (DiffSynthControls *controls,
                                                    DiffSynthArgs *args);
static void               page_switched            (DiffSynthControls *controls,
                                                    GtkNotebookPage *page,
                                                    gint pagenum);
static void               update_values            (DiffSynthControls *controls);
static void               height_init_clicked      (DiffSynthControls *controls);
static void               diff_synth_invalidate    (DiffSynthControls *controls);
static void               preview                  (DiffSynthControls *controls);
static gboolean           diff_synth_do            (DiffSynthArgs *args,
                                                    GwyDataField *dfield,
                                                    GwyGraphCurveModel **gcmodels,
                                                    gdouble preview_time,
                                                    gdouble zscale);
static ParticleNeighbours random_direction         (GRand *rng);
static void               particle_try_move        (Particle *p,
                                                    guint *hfield,
                                                    guint xres,
                                                    guint yres,
                                                    gboolean use_schwoebel,
                                                    gdouble schwoebel,
                                                    GRand *rng,
                                                    const gdouble *p_break);
static void               one_iteration            (DiffSynthState *dstate);
static void               add_particle             (DiffSynthState *dstate);
static void               finalize_moving_particles(DiffSynthState *dstate);
static DiffSynthState*    diff_synth_state_new     (guint xres,
                                                    guint yres,
                                                    const DiffSynthArgs *args);
static void               diff_synth_state_free    (DiffSynthState *dstate);
static void               diff_synth_load_args     (GwyContainer *container,
                                                    DiffSynthArgs *args,
                                                    GwyDimensionArgs *dimsargs);
static void               diff_synth_save_args     (GwyContainer *container,
                                                    const DiffSynthArgs *args,
                                                    const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS DiffSynthControls
#define GWY_SYNTH_INVALIDATE(controls) diff_synth_invalidate(controls)

#include "synth.h"

static const gchar* graph_flags[GRAPH_NFLAGS + 1/*unused*/] = {
    N_("Variation"),
    N_("Number of islands"),
};

static const DiffSynthArgs diff_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, FALSE, TRUE,
    1.0,
    0.25, -10.0, 0.1, 0.01, 0.0,
    { FALSE, },
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates surfaces by diffusion limited aggregation."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("diff_synth",
                              (GwyProcessFunc)&diff_synth,
                              N_("/S_ynthetic/_Diffusion..."),
                              GWY_STOCK_SYNTHETIC_DIFFUSION,
                              DIFF_SYNTH_RUN_MODES,
                              0,
                              N_("Generate surface by diffusion limited "
                                 "aggregation"));

    return TRUE;
}

static void
diff_synth(GwyContainer *data, GwyRunType run)
{
    DiffSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & DIFF_SYNTH_RUN_MODES);
    diff_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || diff_synth_dialog(&args, &dimsargs, data, dfield, id)) {
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);
    }

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(DiffSynthArgs *args,
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
    ok = diff_synth_do(args, newfield, gcmodels, HUGE_VAL, zscale);
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
                     "axis-label-bottom", _("Mean height"),
                     "axis-label-left", _(graph_flags[i]),
                     NULL);
        g_free(title);

        unit = gwy_data_field_get_si_unit_z(newfield);
        unit = gwy_si_unit_duplicate(unit);
        g_object_set(gmodel, "si-unit-x", unit, NULL);
        g_object_unref(unit);

        if (i == GRAPH_VAR) {
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
diff_synth_dialog(DiffSynthArgs *args,
                  GwyDimensionArgs *dimsargs,
                  GwyContainer *data,
                  GwyDataField *dfield_template,
                  gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *hbox2, *check, *label;
    DiffSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row, i;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    controls.pxsize = 1.0;
    dialog = gtk_dialog_new_with_buttons(_("Diffusion Limited Aggregation"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_proc_dialog_button(GTK_DIALOG(dialog));
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
    /* This is used only for synt.h helpers. */
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.coverage = gtk_adjustment_new(args->coverage,
                                           0.0, 16.0, 0.001, 1.0, 0);
    g_object_set_data(G_OBJECT(controls.coverage), "target", &args->coverage);
    gwy_table_attach_hscale(table, row, _("Co_verage:"), NULL,
                            controls.coverage, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.coverage, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    controls.flux = gtk_adjustment_new(args->flux, -13.0, -3.0, 0.01, 1.0, 0);
    g_object_set_data(G_OBJECT(controls.flux), "target", &args->flux);
    gwy_table_attach_hscale(table, row, _("_Flux:"), "log<sub>10</sub>",
                            controls.flux, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.flux, "value-changed",
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

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gwy_label_new_header(_("Probabilities"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.p_stick = gtk_adjustment_new(args->p_stick,
                                          0.0, 1.0, 0.0001, 0.1, 0);
    g_object_set_data(G_OBJECT(controls.p_stick), "target", &args->p_stick);
    gwy_table_attach_hscale(table, row, _("_Sticking:"), NULL,
                            controls.p_stick, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.p_stick, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    controls.p_break = gtk_adjustment_new(args->p_break,
                                          0.0, 1.0, 0.0001, 0.1, 0);
    g_object_set_data(G_OBJECT(controls.p_break), "target", &args->p_break);
    gwy_table_attach_hscale(table, row, _("_Activation:"), NULL,
                            controls.p_break, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.p_break, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    controls.schwoebel = gtk_adjustment_new(args->schwoebel,
                                            -12.0, 0.0, 0.01, 1.0, 0);
    g_object_set_data(G_OBJECT(controls.schwoebel), "target", &args->schwoebel);
    gwy_table_attach_hscale(table, row, _("Passing Sch_woebel:"),
                            "log<sub>10</sub>",
                            controls.schwoebel, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.schwoebel, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gwy_label_new_header(_("Options"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

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
                *args = diff_synth_defaults;
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

    diff_synth_save_args(gwy_app_settings_get(), args, dimsargs);

    g_object_unref(controls.mydata);
    gwy_object_unref(controls.surface);
    gwy_dimensions_free(controls.dims);

    return response == GTK_RESPONSE_OK;
}

static void
update_controls(DiffSynthControls *controls,
                DiffSynthArgs *args)
{
    guint i;

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->animated),
                                 args->animated);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height), args->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->coverage),
                             args->coverage);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->flux), args->flux);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->p_break), args->p_break);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->p_stick), args->p_stick);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->schwoebel),
                             args->schwoebel);
    for (i = 0; i < GRAPH_NFLAGS; i++) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->graph_flags[i]),
                                     args->graph_flags[i]);
    }
}

static void
page_switched(DiffSynthControls *controls,
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
update_values(DiffSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
    if (controls->height_units)
        gtk_label_set_markup(GTK_LABEL(controls->height_units),
                             dims->zvf->units);
}

static void
height_init_clicked(DiffSynthControls *controls)
{
    gdouble mag = pow10(controls->dims->args->zpow10);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height),
                             controls->zscale/mag);
}

static void
diff_synth_invalidate(G_GNUC_UNUSED DiffSynthControls *controls)
{
}

static void
preview(DiffSynthControls *controls)
{
    DiffSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, TRUE);
    else
        gwy_data_field_clear(dfield);

    gwy_app_wait_start(GTK_WINDOW(controls->dialog), _("Starting..."));
    diff_synth_do(args, dfield, NULL, 1.25, 1.0);
    gwy_app_wait_finish();

    gwy_data_field_data_changed(dfield);
}

static gboolean
diff_synth_do(DiffSynthArgs *args,
              GwyDataField *dfield,
              GwyGraphCurveModel **gcmodels,
              gdouble preview_time,
              gdouble zscale)
{
    gint xres, yres, k;
    guint i;
    gdouble nextgraphx;
    gdouble lasttime = 0.0, lastpreviewtime = 0.0, currtime;
    gdouble height, threshold;
    GTimer *timer;
    guint64 workdone, niter, iter;
    DiffSynthState *dstate;
    GArray *particles;
    GArray **evolution = NULL;
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

    threshold = gwy_data_field_otsu_threshold(dfield);
    gwy_data_field_threshold(dfield, threshold, 0.0, 1.0);
    nextgraphx = 0.0;

    gwy_app_wait_set_message(_("Depositing particles..."));
    gwy_app_wait_set_fraction(0.0);

    random_direction(NULL);

    dstate = diff_synth_state_new(xres, yres, args);
    for (k = 0; k < xres*yres; k++)
        dstate->hfield[k] = dfield->data[k];

    particles = dstate->particles;
    workdone = 0.0;
    niter = (guint64)(args->coverage/dstate->flux + 0.5);
    iter = 0;
    dstate->fluxperiter = xres*yres * dstate->flux;

    while (iter < niter) {
        workdone += particles->len;
        one_iteration(dstate);
        /* Optimize the low-flux case when there may be no free particles at
         * all for relatively long time periods and skip to the time when
         * another particle arrives. */
        if (particles->len)
            iter++;
        else {
            add_particle(dstate);
            iter += (guint64)((1.0 - dstate->fluence)/dstate->fluxperiter
                              + 0.5);
            dstate->fluence = 0.0;
        }

        if (workdone >= WORK_UPDATE_CHECK) {
            currtime = g_timer_elapsed(timer, NULL);
            if (currtime - lasttime >= 0.25) {
                if (!gwy_app_wait_set_fraction((gdouble)iter/niter))
                    goto fail;
                lasttime = currtime;

                if (args->animated
                    && currtime - lastpreviewtime >= preview_time) {
                    for (k = 0; k < xres*yres; k++)
                        dfield->data[k] = dstate->hfield[k];
                    gwy_data_field_invalidate(dfield);
                    gwy_data_field_data_changed(dfield);
                    lastpreviewtime = lasttime;
                }
            }
            workdone -= WORK_UPDATE_CHECK;
        }

        if (any_graphs && iter >= nextgraphx) {
            for (k = 0; k < xres*yres; k++)
                dfield->data[k] = dstate->hfield[k] * zscale;
            gwy_data_field_invalidate(dfield);

            height = iter*dstate->fluxperiter * zscale;
            g_array_append_val(evolution[GRAPH_NFLAGS], height);
            if (evolution[GRAPH_VAR]) {
                gdouble var = gwy_data_field_get_variation(dfield);
                g_array_append_val(evolution[GRAPH_VAR], var);
            }

            nextgraphx += 0.0001/dstate->flux + MIN(0.2*nextgraphx,
                                                    0.08/dstate->flux);
        }
    }
    finalize_moving_particles(dstate);

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

    for (k = 0; k < xres*yres; k++)
        dfield->data[k] = dstate->hfield[k];

    gwy_data_field_invalidate(dfield);
    finished = TRUE;

fail:
    g_timer_destroy(timer);
    diff_synth_state_free(dstate);
    for (i = 0; i <= GRAPH_NFLAGS; i++) {
        if (evolution[i])
            g_array_free(evolution[i], TRUE);
    }
    g_free(evolution);

    return finished;
}

static void
calc_neighbour_pos(guint col, guint row, guint k, guint xres, guint yres,
                   guint *kup, guint *kleft, guint *kright, guint *kdown)
{
    *kup = G_LIKELY(row) ? k - xres : k + xres*(yres - 1);
    *kleft = G_LIKELY(col) ? k - 1 : k + xres - 1;
    *kright = G_LIKELY(col < xres-1) ? k + 1 : k - (xres - 1);
    *kdown = G_LIKELY(row < yres-1) ? k + xres : k - xres*(yres - 1);
}

static void
particle_update_neighbours(Particle *p, const guint *hfield,
                           gboolean use_schwoebel)
{
    guint h = hfield[p->k];
    guint neighbours = 0, nneigh = 0;

    if (hfield[p->kup] >= h) {
        neighbours |= (1 << NEIGH_UP);
        nneigh++;
    }
    if (hfield[p->kleft] >= h) {
        neighbours |= (1 << NEIGH_LEFT);
        nneigh++;
    }
    if (hfield[p->kright] >= h) {
        neighbours |= (1 << NEIGH_RIGHT);
        nneigh++;
    }
    if (hfield[p->kdown] >= h) {
        neighbours |= (1 << NEIGH_DOWN);
        nneigh++;
    }
    if (use_schwoebel) {
        if (hfield[p->kup] + 1 < h)
            neighbours |= (1 << (NEIGH_UP + NEIGH_SCHWOEBEL));
        if (hfield[p->kleft] + 1 < h)
            neighbours |= (1 << (NEIGH_LEFT + NEIGH_SCHWOEBEL));
        if (hfield[p->kright] + 1 < h)
            neighbours |= (1 << (NEIGH_RIGHT + NEIGH_SCHWOEBEL));
        if (hfield[p->kdown] + 1 < h)
            neighbours |= (1 << (NEIGH_DOWN + NEIGH_SCHWOEBEL));
    }
    p->neighbours = neighbours;
    p->nneigh = nneigh;
}

static inline ParticleNeighbours
random_direction(GRand *rng)
{
    static guint32 spare = 0;
    static guint nspare = 0;

    ParticleNeighbours direction;

    if (G_UNLIKELY(!rng)) {
        nspare = 0;
        return 0;
    }

    if (nspare) {
        direction = spare & 0x3;
        spare >>= 2;
        nspare--;
    }
    else {
        spare = g_rand_int(rng);
        direction = spare & 0x3;
        spare >>= 2;
        nspare = 7;
    }

    return direction;
}

/* This must be called with nehgibours information updated. */
static void
particle_try_move(Particle *p,
                  guint *hfield, guint xres, guint yres,
                  gboolean use_schwoebel, const gdouble schwoebel,
                  GRand *rng, const gdouble *p_break)
{
    ParticleNeighbours direction = random_direction(rng);

    if (p->neighbours & (1 << direction))
        return;

    if (use_schwoebel
        && (p->neighbours & (1 << (direction + NEIGH_SCHWOEBEL)))
        && g_rand_double(rng) >= schwoebel)
        return;

    if (g_rand_double(rng) >= p_break[p->nneigh])
        return;

    hfield[p->k]--;
    if (direction == NEIGH_UP)
        p->row = G_LIKELY(p->row) ? p->row - 1 : xres - 1;
    else if (direction == NEIGH_LEFT)
        p->col = G_LIKELY(p->col) ? p->col - 1 : yres - 1;
    else if (direction == NEIGH_RIGHT)
        p->col = G_LIKELY(p->col < xres - 1) ? p->col + 1 : 0;
    else
        p->row = G_LIKELY(p->row < yres - 1) ? p->row + 1 : 0;

    p->k = p->row*xres + p->col;
    calc_neighbour_pos(p->col, p->row, p->k, xres, yres,
                       &p->kup, &p->kleft, &p->kright, &p->kdown);
    hfield[p->k]++;
}

static void
add_particle(DiffSynthState *dstate)
{
    Particle p;
    p.col = g_rand_int_range(dstate->rng, 0, dstate->xres);
    p.row = g_rand_int_range(dstate->rng, 0, dstate->yres);
    p.k = p.row*dstate->xres + p.col;
    calc_neighbour_pos(p.col, p.row, p.k, dstate->xres, dstate->yres,
                       &p.kup, &p.kleft, &p.kright, &p.kdown);
    g_array_append_val(dstate->particles, p);
    dstate->hfield[p.k]++;
}

static void
one_iteration(DiffSynthState *dstate)
{
    GArray *particles = dstate->particles;
    guint xres = dstate->xres, yres = dstate->yres;
    guint *hfield = dstate->hfield;
    const gdouble *p_break = dstate->p_break;
    const gdouble *p_stick = dstate->p_stick;
    gboolean use_schwoebel = dstate->use_schwoebel;
    gdouble schwoebel = dstate->schwoebel;
    GRand *rng = dstate->rng;
    guint i = 0;

    while (i < particles->len) {
        Particle *p = &g_array_index(particles, Particle, i);
        gdouble ps;

        particle_update_neighbours(p, hfield, use_schwoebel);
        ps = p_stick[p->nneigh];
        if (ps == 1.0 || (ps && g_rand_double(rng) < ps))
            g_array_remove_index_fast(particles, i);
        else {
            // XXX: We may also consider desorption here.
            particle_try_move(p, hfield, xres, yres, use_schwoebel, schwoebel,
                              rng, p_break);
            i++;
        }
    }

    dstate->fluence += dstate->fluxperiter;
    while (dstate->fluence >= 1.0) {
        add_particle(dstate);
        dstate->fluence -= 1.0;
    }
}

static void
finalize_moving_particles(DiffSynthState *dstate)
{
    GArray *particles = dstate->particles;
    guint *hfield = dstate->hfield;
    GRand *rng = dstate->rng;
    const gdouble *p_stick = dstate->p_stick;
    guint i = 0;

    while (i < particles->len) {
        Particle *p = &g_array_index(particles, Particle, i);
        gdouble ps;

        particle_update_neighbours(p, hfield, FALSE);
        ps = p_stick[p->nneigh];
        if (ps == 1.0 || (ps && g_rand_double(rng) < ps))
            g_array_remove_index_fast(particles, i);
        else {
            hfield[p->k]--;
            i++;
        }
    }
}

static DiffSynthState*
diff_synth_state_new(guint xres, guint yres, const DiffSynthArgs *args)
{
    DiffSynthState *dstate = g_new0(DiffSynthState, 1);
    guint i;

    dstate->rng = g_rand_new_with_seed(args->seed);
    dstate->xres = xres;
    dstate->yres = yres;
    dstate->hfield = g_new0(guint, dstate->xres*dstate->yres);
    dstate->particles = g_array_new(FALSE, FALSE, sizeof(Particle));
    dstate->fluence = 0.0;

    dstate->p_break[0] = 1.0;
    for (i = 1; i < 5; i++)
        dstate->p_break[i] = args->p_break * dstate->p_break[i-1];

    dstate->p_stick[0] = 0.0;
    dstate->p_stick[1] = args->p_stick;
    for (i = 2; i < 4; i++) {
        dstate->p_stick[i] = 1.0 - pow(1.0 - dstate->p_stick[i-1], 2.0);
        dstate->p_stick[i] = CLAMP(dstate->p_stick[i], 0.0, 1.0);
    }
    dstate->p_stick[4] = 1.0;

    dstate->flux = pow10(args->flux);
    dstate->schwoebel = pow10(args->schwoebel);
    dstate->use_schwoebel = (dstate->schwoebel < 0.999);

    return dstate;
}

static void
diff_synth_state_free(DiffSynthState *dstate)
{
    g_free(dstate->hfield);
    g_rand_free(dstate->rng);
    g_array_free(dstate->particles, TRUE);
    g_free(dstate);
}

static const gchar prefix[]          = "/module/diff_synth";
static const gchar active_page_key[] = "/module/diff_synth/active_page";
static const gchar randomize_key[]   = "/module/diff_synth/randomize";
static const gchar seed_key[]        = "/module/diff_synth/seed";
static const gchar animated_key[]    = "/module/diff_synth/animated";
static const gchar height_key[]      = "/module/diff_synth/height";
static const gchar coverage_key[]    = "/module/diff_synth/coverage";
static const gchar flux_key[]        = "/module/diff_synth/flux";
static const gchar p_break_key[]     = "/module/diff_synth/p_break";
static const gchar p_stick_key[]     = "/module/diff_synth/p_stick";
static const gchar schwoebel_key[]   = "/module/diff_synth/schwoebel";
static const gchar graph_flags_key[] = "/module/diff_synth/graph_flags";

static void
diff_synth_sanitize_args(DiffSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->animated = !!args->animated;
    args->height = CLAMP(args->height, 0.001, 10000.0);
    args->coverage = CLAMP(args->coverage, 0.0, 16.0);
    args->flux = CLAMP(args->flux, -13.0, -3.0);
    args->p_stick = CLAMP(args->p_stick, 0.0, 1.0);
    args->p_break = CLAMP(args->p_break, 0.0, 1.0);
    args->schwoebel = CLAMP(args->schwoebel, -12.0, 0.0);
}

static void
diff_synth_load_args(GwyContainer *container,
                     DiffSynthArgs *args,
                     GwyDimensionArgs *dimsargs)
{
    guint i;
    gint gflags = 0;

    *args = diff_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_boolean_by_name(container, animated_key,
                                      &args->animated);
    gwy_container_gis_double_by_name(container, height_key, &args->height);
    gwy_container_gis_double_by_name(container, coverage_key, &args->coverage);
    gwy_container_gis_double_by_name(container, flux_key, &args->flux);
    gwy_container_gis_double_by_name(container, p_stick_key, &args->p_stick);
    gwy_container_gis_double_by_name(container, p_break_key, &args->p_break);
    gwy_container_gis_double_by_name(container, schwoebel_key, &args->schwoebel);
    gwy_container_gis_int32_by_name(container, graph_flags_key, &gflags);
    for (i = 0; i < GRAPH_NFLAGS; i++)
        args->graph_flags[i] = !!(gflags & (1 << i));

    diff_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
diff_synth_save_args(GwyContainer *container,
                    const DiffSynthArgs *args,
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
    gwy_container_set_double_by_name(container, coverage_key, args->coverage);
    gwy_container_set_double_by_name(container, flux_key, args->flux);
    gwy_container_set_double_by_name(container, p_stick_key, args->p_stick);
    gwy_container_set_double_by_name(container, p_break_key, args->p_break);
    gwy_container_set_double_by_name(container, schwoebel_key, args->schwoebel);
    gwy_container_set_int32_by_name(container, graph_flags_key, gflags);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
