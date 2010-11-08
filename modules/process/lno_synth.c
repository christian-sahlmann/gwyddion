/*
 *  @(#) $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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
#include <libprocess/filters.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define GWY_SQRT6 2.449489742783178098197284074705

#define LNO_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define DECLARE_LNOISE(name) \
    static gpointer create_gui_##name(LNoSynthControls *controls); \
    static void dimensions_changed_##name(LNoSynthControls *controls); \
    static void make_noise_##name(const LNoSynthArgs *args, \
                                  const GwyDimensionArgs *dimsargs, \
                                  GwyDataField *dfield); \
    static gpointer load_args_##name(GwyContainer *settings); \
    static void save_args_##name(gpointer pargs, GwyContainer *settings); \
    static void reset_##name(gpointer pcontrols)

#define DECLARE_NOISE(name) \
    static gdouble noise_##name##_both(GRand *rng, gdouble sigma); \
    static gdouble noise_##name##_up(GRand *rng, gdouble sigma); \
    static gdouble noise_##name##_down(GRand *rng, gdouble sigma); \
    static gdouble rand_gen_##name(GRand *rng, gdouble sigma)

#define LLNO_FUNCS(name) \
    &create_gui_##name, &dimensions_changed_##name, &reset_##name, \
    &make_noise_##name, \
    &load_args_##name, &save_args_##name

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
    LNO_DISTRIBUTION_GAUSSIAN    = 0,
    LNO_DISTRIBUTION_EXPONENTIAL = 1,
    LNO_DISTRIBUTION_UNIFORM     = 2,
    LNO_DISTRIBUTION_TRIANGULAR  = 3,
    LNO_DISTRIBUTION_NTYPES
} LNoDistributionType;

typedef enum {
    LNO_DIRECTION_BOTH = 0,
    LNO_DIRECTION_UP   = 1,
    LNO_DIRECTION_DOWN = 2,
    LNO_DIRECTION_NTYPES
} LNoDirectionType;

typedef enum {
    LNO_SYNTH_STEPS = 0,
    LNO_SYNTH_SCARS = 1,
    LNO_SYNTH_NTYPES
} LNoSynthNoiseType;

typedef struct _LNoSynthArgs LNoSynthArgs;
typedef struct _LNoSynthControls LNoSynthControls;

typedef gpointer (*CreateGUIFunc)(LNoSynthControls *controls);
typedef void (*DestroyGUIFunc)(gpointer pcontrols);
typedef void (*DimensionsChangedFunc)(LNoSynthControls *controls);
typedef void (*ResetFunc)(gpointer pcontrols);
typedef void (*MakeNoiseFunc)(const LNoSynthArgs *args,
                              const GwyDimensionArgs *dimsargs,
                              GwyDataField *dfield);
typedef gpointer (*LoadArgsFunc)(GwyContainer *settings);
typedef void (*SaveArgsFunc)(gpointer pargs,
                             GwyContainer *settings);

typedef void (*UpdateValueFunc)(LNoSynthControls *controls);

typedef gdouble (*PointNoiseFunc)(GRand *rng, gdouble sigma);

/* This scheme makes the distribution type list easily reordeable in the GUI
 * without changing the ids.  Directions need not be reordered. */
typedef struct {
    LNoDistributionType distribution;
    const gchar *name;
    PointNoiseFunc point_noise[LNO_DIRECTION_NTYPES];
    PointNoiseFunc base_generator;
} LNoSynthGenerator;

/* This scheme makes the object type list easily reordeable in the GUI without
 * changing the ids.  */
typedef struct {
    LNoSynthNoiseType type;
    const gchar *name;
    CreateGUIFunc create_gui;
    DimensionsChangedFunc dims_changed;
    ResetFunc reset;
    MakeNoiseFunc run;
    LoadArgsFunc load_args;
    SaveArgsFunc save_args;
} LNoSynthNoise;

struct _LNoSynthArgs {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;
    LNoDistributionType distribution;
    LNoDirectionType direction;
    gdouble sigma;
    LNoSynthNoiseType type;
    gpointer noise_args;
};

struct _LNoSynthControls {
    LNoSynthArgs *args;
    GwyDimensions *dims;
    const LNoSynthGenerator *generator;
    const LNoSynthNoise *noise;
    /* They have different types, known only to the noise. */
    gpointer noise_controls;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkTable *table;
    GtkWidget *type;
    GtkWidget *distribution;
    GSList *direction;
    GtkObject *sigma;
    GtkWidget *sigma_units;
    GtkWidget *sigma_init;
    GwyContainer *mydata;
    GwyDataField *surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
    gulong sid;
};

static gboolean      module_register           (void);
static void          lno_synth                 (GwyContainer *data,
                                                GwyRunType run);
static void          run_noninteractive        (LNoSynthArgs *args,
                                                const GwyDimensionArgs *dimsargs,
                                                GwyContainer *data,
                                                GwyDataField *dfield,
                                                gint oldid,
                                                GQuark quark);
static gboolean      lno_synth_dialog          (LNoSynthArgs *args,
                                                GwyDimensionArgs *dimsargs,
                                                GwyContainer *data,
                                                GwyDataField *dfield,
                                                gint id);
static GtkWidget*    noise_selector_new        (LNoSynthControls *controls);
static GtkWidget*    distribution_selector_new (LNoSynthControls *controls);
static void          update_controls           (LNoSynthControls *controls,
                                                LNoSynthArgs *args);
static void          page_switched             (LNoSynthControls *controls,
                                                GtkNotebookPage *page,
                                                gint pagenum);
static void          update_values             (LNoSynthControls *controls);
static void          noise_type_selected       (GtkComboBox *combo,
                                                LNoSynthControls *controls);
static void          distribution_type_selected(GtkComboBox *combo,
                                                LNoSynthControls *controls);
static void          direction_type_changed    (GtkWidget *button,
                                                LNoSynthControls *controls);
static void          sigma_init_clicked        (LNoSynthControls *controls);
static void          lno_synth_invalidate      (LNoSynthControls *controls);
static gboolean      preview_gsource           (gpointer user_data);
static void          preview                   (LNoSynthControls *controls);
static void          lno_synth_do              (const LNoSynthArgs *args,
                                                const GwyDimensionArgs *dimsargs,
                                                GwyDataField *dfield);
static void          lno_synth_load_args       (GwyContainer *container,
                                                LNoSynthArgs *args,
                                                GwyDimensionArgs *dimsargs);
static void          lno_synth_save_args       (GwyContainer *container,
                                                const LNoSynthArgs *args,
                                                const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS LNoSynthControls
#define GWY_SYNTH_INVALIDATE(controls) \
    lno_synth_invalidate(controls)

#include "synth.h"

DECLARE_NOISE(gaussian);
DECLARE_NOISE(exp);
DECLARE_NOISE(uniform);
DECLARE_NOISE(triangle);

DECLARE_LNOISE(steps);
DECLARE_LNOISE(scars);

static const gchar prefix[] = "/module/lno_synth";

static const LNoSynthArgs lno_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    LNO_DISTRIBUTION_GAUSSIAN, LNO_DIRECTION_BOTH,
    1.0,
    LNO_SYNTH_STEPS,
    NULL,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static const LNoSynthGenerator generators[] = {
    {
        LNO_DISTRIBUTION_GAUSSIAN,
        N_("distribution|Gaussian"),
        { &noise_gaussian_both, &noise_gaussian_up, &noise_gaussian_down, },
        rand_gen_gaussian,
    },
    {
        LNO_DISTRIBUTION_EXPONENTIAL,
        N_("distribution|Exponential"),
        { &noise_exp_both, &noise_exp_up, &noise_exp_down, },
        rand_gen_exp,
    },
    {
        LNO_DISTRIBUTION_UNIFORM,
        N_("distribution|Uniform"),
        { &noise_uniform_both, &noise_uniform_up, &noise_uniform_down, },
        rand_gen_uniform,
    },
    {
        LNO_DISTRIBUTION_TRIANGULAR,
        N_("distribution|Triangular"),
        { &noise_triangle_both, &noise_triangle_up, &noise_triangle_down, },
        rand_gen_triangle,
    },
};

static const LNoSynthNoise noises[] = {
    { LNO_SYNTH_STEPS, N_("Steps"), LLNO_FUNCS(steps), },
    { LNO_SYNTH_SCARS, N_("Scars"), LLNO_FUNCS(scars), },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates various kinds of line noise."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("lno_synth",
                              (GwyProcessFunc)&lno_synth,
                              N_("/S_ynthetic/_Line Noise..."),
                              NULL,
                              LNO_SYNTH_RUN_MODES,
                              0,
                              N_("Generate line noise"));

    return TRUE;
}

static void
lno_synth(GwyContainer *data, GwyRunType run)
{
    LNoSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & LNO_SYNTH_RUN_MODES);
    lno_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || lno_synth_dialog(&args, &dimsargs, data, dfield, id))
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);

    if (run == GWY_RUN_INTERACTIVE)
        lno_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);

    gwy_dimensions_free_args(&dimsargs);
}

static const LNoSynthNoise*
get_noise(guint type)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(noises); i++) {
        if (noises[i].type == type)
            return noises + i;
    }
    g_warning("Unknown noise %u\n", type);

    return noises + 0;
}

static void
run_noninteractive(LNoSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint oldid,
                   GQuark quark)
{
    const LNoSynthNoise *noise;
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

    noise = get_noise(args->type);
    args->noise_args = noise->load_args(gwy_app_settings_get());
    lno_synth_do(args, dimsargs, dfield);
    g_free(args->noise_args);
    args->noise_args = NULL;

    if (!replace) {
        if (data) {
            newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
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
    }
    g_object_unref(dfield);
}

static gboolean
lno_synth_dialog(LNoSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *label;
    LNoSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    controls.pxsize = 1.0;
    dialog = gtk_dialog_new_with_buttons(_("Line Noise"),
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
    if (dfield_template) {
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
    g_signal_connect_swapped(controls.update_now, "clicked",
                             G_CALLBACK(preview), &controls);

    gtk_box_pack_start(GTK_BOX(vbox),
                       gwy_synth_random_seed_new(&controls.seed, &args->seed),
                       FALSE, FALSE, 0);

    controls.randomize = gwy_synth_randomize_new(&args->randomize);
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
                                 G_CALLBACK(lno_synth_invalidate), &controls);

    table = gtk_table_new(9 + (dfield_template ? 1 : 0), 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(controls.table, 2);
    gtk_table_set_col_spacings(controls.table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    gtk_table_attach(controls.table, gwy_label_new_header(_("Distribution")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.distribution = distribution_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Distribution:"), NULL,
                            GTK_OBJECT(controls.distribution),
                            GWY_HSCALE_WIDGET);
    row++;

    label = gtk_label_new(_("Direction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 0, 0);
    row++;

    controls.direction
        = gwy_radio_buttons_createl(G_CALLBACK(direction_type_changed),
                                    &controls, args->direction,
                                    _("S_ymmetrical"), LNO_DIRECTION_BOTH,
                                    _("One-sided _positive"), LNO_DIRECTION_UP,
                                    _("One-sided _negative"), LNO_DIRECTION_DOWN,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.direction,
                                            GTK_TABLE(table), 3, row);
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

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(controls.table, gwy_label_new_header(_("Noise Type")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.type = noise_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Noise type:"), NULL,
                            GTK_OBJECT(controls.type), GWY_HSCALE_WIDGET);
    row++;

    g_object_set_data(G_OBJECT(controls.table),
                      "base-rows", GINT_TO_POINTER(row));
    noise_type_selected(GTK_COMBO_BOX(controls.type), &controls);

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);
    lno_synth_invalidate(&controls);

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
            args->seed = lno_synth_defaults.seed;
            args->randomize = lno_synth_defaults.randomize;
            /* Don't reset type either.  It sort of defeats resetting the
             * noise-specific options. */
            controls.in_init = TRUE;
            controls.noise->reset(controls.noise_controls);
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

    noise_type_selected(NULL, &controls);
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
noise_selector_new(LNoSynthControls *controls)
{
    GtkWidget *combo;
    GwyEnum *model;
    guint n, i;

    n = G_N_ELEMENTS(noises);
    model = g_new(GwyEnum, n);
    for (i = 0; i < n; i++) {
        model[i].value = noises[i].type;
        model[i].name = noises[i].name;
    }

    combo = gwy_enum_combo_box_new(model, n,
                                   G_CALLBACK(noise_type_selected), controls,
                                   controls->args->type, TRUE);
    g_object_weak_ref(G_OBJECT(combo), (GWeakNotify)g_free, model);

    return combo;
}

static GtkWidget*
distribution_selector_new(LNoSynthControls *controls)
{
    GtkWidget *combo;
    GwyEnum *model;
    guint n, i;

    n = G_N_ELEMENTS(generators);
    model = g_new(GwyEnum, n);
    for (i = 0; i < n; i++) {
        model[i].value = generators[i].distribution;
        model[i].name = generators[i].name;
    }

    combo = gwy_enum_combo_box_new(model, n,
                                   G_CALLBACK(distribution_type_selected),
                                   controls, controls->args->distribution, TRUE);
    g_object_weak_ref(G_OBJECT(combo), (GWeakNotify)g_free, model);

    return combo;
}

static void
update_controls(LNoSynthControls *controls,
                LNoSynthArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->sigma), args->sigma);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->distribution),
                                  args->distribution);
    gwy_radio_buttons_set_current(controls->direction,
                                  args->direction);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->type), args->type);
}

static void
page_switched(LNoSynthControls *controls,
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
update_values(LNoSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    if (controls->sigma_units)
        gtk_label_set_markup(GTK_LABEL(controls->sigma_units),
                             dims->zvf->units);

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
    controls->noise->dims_changed(controls);
}

static void
noise_type_selected(GtkComboBox *combo,
                    LNoSynthControls *controls)
{
    const LNoSynthNoise *noise;
    LNoSynthArgs *args = controls->args;
    guint baserows;

    if (controls->noise) {
        noise = controls->noise;
        noise->save_args(args->noise_args, gwy_app_settings_get());
        controls->noise = NULL;
        g_free(controls->noise_controls);
        controls->noise_controls = NULL;
        g_free(args->noise_args);
        args->noise_args = NULL;
    }

    /* Just tear-down */
    if (!combo)
        return;

    args->type = gwy_enum_combo_box_get_active(combo);
    noise = controls->noise = get_noise(args->type);

    baserows = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(controls->table),
                                                 "base-rows"));
    gwy_synth_shrink_table(controls->table, baserows);

    args->noise_args = noise->load_args(gwy_app_settings_get());
    controls->noise_controls = noise->create_gui(controls);
    gtk_widget_show_all(GTK_WIDGET(controls->table));

    lno_synth_invalidate(controls);
}

static void
distribution_type_selected(GtkComboBox *combo,
                           LNoSynthControls *controls)
{
    controls->args->distribution = gwy_enum_combo_box_get_active(combo);
    lno_synth_invalidate(controls);
}

static void
direction_type_changed(GtkWidget *button,
                       LNoSynthControls *controls)
{
    controls->args->direction = gwy_radio_button_get_value(button);
    lno_synth_invalidate(controls);
}

static void
sigma_init_clicked(LNoSynthControls *controls)
{
    gdouble mag = pow10(controls->dims->args->zpow10);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->sigma),
                             controls->zscale/mag);
}

static void
lno_synth_invalidate(LNoSynthControls *controls)
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
    LNoSynthControls *controls = (LNoSynthControls*)user_data;
    controls->sid = 0;

    preview(controls);

    return FALSE;
}

static void
preview(LNoSynthControls *controls)
{
    LNoSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, FALSE);
    else
        gwy_data_field_clear(dfield);

    lno_synth_do(args, controls->dims->args, dfield);
}

static void
lno_synth_do(const LNoSynthArgs *args,
             const GwyDimensionArgs *dimsargs,
             GwyDataField *dfield)
{
    const LNoSynthNoise *noise = get_noise(args->type);

    noise->run(args, dimsargs, dfield);
    gwy_data_field_data_changed(dfield);
}

static const LNoSynthGenerator*
get_point_noise_generator(LNoDistributionType distribution)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(generators); i++) {
        if (generators[i].distribution == distribution) {
            return generators + i;
        }
    }
    g_warning("Unknown distribution %u\n", distribution);

    return generators + 0;
}

typedef struct {
    gdouble density;
    gdouble lineprob;
    gboolean cumulative;
} LNoSynthArgsSteps;

typedef struct {
    LNoSynthArgsSteps *args;
    GtkObject *density;
    GtkObject *lineprob;
    GtkWidget *cumulative;
} LNoSynthControlsSteps;

static const LNoSynthArgsSteps lno_synth_defaults_steps = {
    1.0, 0.0, FALSE,
};

static gpointer
create_gui_steps(LNoSynthControls *controls)
{
    LNoSynthControlsSteps *pcontrols;
    LNoSynthArgsSteps *pargs;
    gint row;

    row = gwy_synth_extend_table(controls->table, 4);
    pcontrols = g_new0(LNoSynthControlsSteps, 1);
    pargs = pcontrols->args = controls->args->noise_args;

    pcontrols->density = gtk_adjustment_new(pargs->density,
                                            0.001, 100.0, 0.001, 1.0, 0);
    g_object_set_data(G_OBJECT(pcontrols->density),
                      "target", &pargs->density);
    gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                            row, _("Densi_ty:"), NULL, pcontrols->density,
                            GWY_HSCALE_LOG);
    g_signal_connect_swapped(pcontrols->density, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), controls);
    row++;

    pcontrols->lineprob = gtk_adjustment_new(pargs->lineprob,
                                             0.0, 1.0, 0.001, 0.1, 0);
    g_object_set_data(G_OBJECT(pcontrols->lineprob),
                      "target", &pargs->lineprob);
    gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                            row, _("_Within line:"), NULL, pcontrols->lineprob,
                            GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(pcontrols->lineprob, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), controls);
    row++;

    pcontrols->cumulative
        = gtk_check_button_new_with_mnemonic(_("C_umulative"));
    g_object_set_data(G_OBJECT(pcontrols->cumulative),
                      "target", &pargs->cumulative);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pcontrols->cumulative),
                                 pargs->cumulative);
    gtk_table_attach(controls->table, pcontrols->cumulative,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(pcontrols->cumulative, "toggled",
                             G_CALLBACK(gwy_synth_boolean_changed), controls);
    row++;

    return pcontrols;
}

static void
dimensions_changed_steps(G_GNUC_UNUSED LNoSynthControls *controls)
{
}

static void
make_noise_steps(const LNoSynthArgs *args,
                 const GwyDimensionArgs *dimsargs,
                 GwyDataField *dfield)
{
    enum { BATCH_SIZE = 64 };

    const LNoSynthArgsSteps *pargs = args->noise_args;
    const LNoSynthGenerator *generator;
    PointNoiseFunc point_noise;
    gdouble *steps, *data;
    GRand *rng;
    guint xres, yres, nbatches, nsteps, n, i, j, ib, is;
    gdouble q, h;

    rng = g_rand_new();

    q = args->sigma * pow10(dimsargs->zpow10);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    n = xres*yres;

    nsteps = GWY_ROUND(yres*pargs->density);
    nsteps = MAX(nsteps, 1);
    steps = g_new(gdouble, nsteps + 1);

    /* Generate the steps in batches because (a) it speeds up sorting (b)
     * it makes them more uniform. */
    nbatches = (nsteps + BATCH_SIZE-1)/BATCH_SIZE;

    g_rand_set_seed(rng, args->seed);
    for (ib = 0; ib < nbatches; ib++) {
        guint base = ib*nsteps/nbatches, nextbase = (ib + 1)*nsteps/nbatches;
        gdouble min = base/(gdouble)nsteps, max = nextbase/(gdouble)nsteps;

        for (i = base; i < nextbase; i++)
            steps[i] = g_rand_double_range(rng, min, max);

        gwy_math_sort(nextbase - base, steps + base);
    }
    /* Sentinel */
    steps[nsteps] = 1.01;

    g_rand_set_seed(rng, args->seed + 1);
    generator = get_point_noise_generator(args->distribution);
    point_noise = generator->point_noise[args->direction];
    /* Clear spare values possibly saved in the base generator */
    generator->base_generator(NULL, 0.0);

    data = gwy_data_field_get_data(dfield);
    is = 0;
    h = 0.0;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble x = (pargs->lineprob*(j + 0.5)/xres + i)/yres;

            while (x > steps[is]) {
                if (pargs->cumulative)
                    h += point_noise(rng, q);
                else
                    h = point_noise(rng, q);
                is++;
            }
            data[i*xres + j] += h;
        }
    }

    g_free(steps);
    g_rand_free(rng);
}

static gpointer
load_args_steps(GwyContainer *settings)
{
    LNoSynthArgsSteps *pargs;
    GString *key;

    pargs = g_memdup(&lno_synth_defaults_steps, sizeof(LNoSynthArgsSteps));
    key = g_string_new(prefix);
    g_string_append(key, "/steps/");
    gwy_synth_load_arg_double(settings, key, "density", 0.001, 100.0,
                              &pargs->density);
    gwy_synth_load_arg_double(settings, key, "lineprob", 0.0, 1.0,
                              &pargs->lineprob);
    gwy_synth_load_arg_boolean(settings, key, "cumulative",
                               &pargs->cumulative);
    g_string_free(key, TRUE);

    return pargs;
}

static void
save_args_steps(gpointer p,
                GwyContainer *settings)
{
    LNoSynthArgsSteps *pargs = p;
    GString *key;

    key = g_string_new(prefix);
    g_string_append(key, "/steps/");
    gwy_synth_save_arg_double(settings, key, "density", pargs->density);
    gwy_synth_save_arg_double(settings, key, "lineprob", pargs->lineprob);
    gwy_synth_save_arg_boolean(settings, key, "cumulative", pargs->cumulative);
    g_string_free(key, TRUE);
}

static void
reset_steps(gpointer p)
{
    LNoSynthControlsSteps *pcontrols = p;
    LNoSynthArgsSteps *pargs = pcontrols->args;

    *pargs = lno_synth_defaults_steps;

    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->density),
                             pargs->density);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->lineprob),
                             pargs->lineprob);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pcontrols->cumulative),
                                 pargs->cumulative);
}

typedef struct {
    gdouble coverage;
    gdouble length;
    gdouble length_noise;
} LNoSynthArgsScars;

typedef struct {
    LNoSynthArgsScars *args;
    GtkObject *coverage;
    GtkObject *length;
    GtkWidget *length_value;
    GtkWidget *length_units;
    GtkObject *length_noise;
} LNoSynthControlsScars;

static const LNoSynthArgsScars lno_synth_defaults_scars = {
    0.01, 10.0, 0.0
};

static gpointer
create_gui_scars(LNoSynthControls *controls)
{
    LNoSynthControlsScars *pcontrols;
    LNoSynthArgsScars *pargs;
    gint row;

    row = gwy_synth_extend_table(controls->table, 4);
    pcontrols = g_new0(LNoSynthControlsScars, 1);
    pargs = pcontrols->args = controls->args->noise_args;

    pcontrols->coverage = gtk_adjustment_new(pargs->coverage,
                                             0.0001, 10.0, 0.0001, 0.1, 0);
    gwy_table_attach_hscale(GTK_WIDGET(controls->table), row, _("Co_verage:"),
                            NULL, pcontrols->coverage, GWY_HSCALE_SQRT);
    g_object_set_data(G_OBJECT(pcontrols->coverage),
                      "target", &pargs->coverage);
    g_signal_connect_swapped(pcontrols->coverage, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), controls);
    row++;

    pcontrols->length = gtk_adjustment_new(pargs->length,
                                           1.0, 10000.0, 1.0, 10.0, 0);
    row = gwy_synth_attach_lateral(controls, row,
                                   pcontrols->length, &pargs->length,
                                   _("_Length:"), GWY_HSCALE_LOG,
                                   NULL,
                                   &pcontrols->length_value,
                                   &pcontrols->length_units);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->length_noise,
                                    &pargs->length_noise);

    return pcontrols;
}

static void
dimensions_changed_scars(LNoSynthControls *controls)
{
    LNoSynthControlsScars *pcontrols = controls->noise_controls;
    GwyDimensions *dims = controls->dims;

    gtk_label_set_markup(GTK_LABEL(pcontrols->length_units), dims->xyvf->units);
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->length));
}

static void
make_noise_scars(const LNoSynthArgs *args,
                 const GwyDimensionArgs *dimsargs,
                 GwyDataField *dfield)
{
    const LNoSynthArgsScars *pargs = args->noise_args;
    const LNoSynthGenerator *generator;
    PointNoiseFunc point_noise;
    gdouble *data, *row;
    GRand *rng;
    guint xres, yres, is, n, nscars, i, j, length, from, to;
    gdouble noise_corr, q, h;

    rng = g_rand_new();

    q = args->sigma * pow10(dimsargs->zpow10);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    n = xres*yres;

    noise_corr = exp(pargs->length_noise*pargs->length_noise);
    /* FIXME: Must compensate for scars sticking from of the line to get the
     * correct coverage. */
    nscars = GWY_ROUND(pargs->coverage*n/(pargs->length*noise_corr));
    nscars = MAX(nscars, 1);

    generator = get_point_noise_generator(args->distribution);
    point_noise = generator->point_noise[args->direction];
    /* Clear spare values possibly saved in the base generator */
    generator->base_generator(NULL, 0.0);
    rand_gen_gaussian(NULL, 0.0);
    g_rand_set_seed(rng, args->seed);

    data = gwy_data_field_get_data(dfield);
    for (is = 0; is < nscars; is++) {
        i = g_rand_int_range(rng, 0, n);
        h = point_noise(rng, q);
        length = GWY_ROUND(pargs->length
                           *exp(rand_gen_gaussian(rng, pargs->length_noise)));

        row = data + (i/xres)*xres;
        j = i % xres;
        from = (length/2 > j) ? 0 : j - length/2;
        to = (length - length/2 > xres - j) ? xres : j+length - length/2;
        for (j = from; j <= to; j++)
            row[j] += h;
    }

    g_rand_free(rng);
}

static gpointer
load_args_scars(GwyContainer *settings)
{
    LNoSynthArgsScars *pargs;
    GString *key;

    pargs = g_memdup(&lno_synth_defaults_scars, sizeof(LNoSynthArgsScars));
    key = g_string_new(prefix);
    g_string_append(key, "/scars/");
    gwy_synth_load_arg_double(settings, key, "coverage", 0.0001, 10.0,
                              &pargs->coverage);
    gwy_synth_load_arg_double(settings, key, "length", 1.0, 10000.0,
                              &pargs->length);
    gwy_synth_load_arg_double(settings, key, "length_noise", 0.0, 1.0,
                              &pargs->length_noise);
    g_string_free(key, TRUE);

    return pargs;
}

static void
save_args_scars(gpointer p,
                GwyContainer *settings)
{
    LNoSynthArgsScars *pargs = p;
    GString *key;

    key = g_string_new(prefix);
    g_string_append(key, "/scars/");
    gwy_synth_save_arg_double(settings, key, "coverage", pargs->coverage);
    gwy_synth_save_arg_double(settings, key, "length", pargs->length);
    gwy_synth_save_arg_double(settings, key, "length_noise", pargs->length_noise);
    g_string_free(key, TRUE);
}

static void
reset_scars(gpointer p)
{
    LNoSynthControlsScars *pcontrols = p;
    LNoSynthArgsScars *pargs = pcontrols->args;

    *pargs = lno_synth_defaults_scars;

    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->coverage),
                             pargs->coverage);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->length),
                             pargs->length);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->length_noise),
                             pargs->length_noise);
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

static gdouble
rand_gen_exp(GRand *rng,
             gdouble sigma)
{
    static guint spare_bits = 0;
    static guint32 spare;

    gdouble x;
    gboolean sign;

    /* Calling with NULL rng just clears the spare random value. */
    if (G_UNLIKELY(!rng)) {
        spare_bits = 0;
        return 0.0;
    }

    x = g_rand_double(rng);
    /* This is how we get exact 0.0 at least sometimes */
    if (G_UNLIKELY(x == 0.0))
        return 0.0;

    if (!spare_bits) {
        spare = g_rand_int(rng);
        spare_bits = 32;
    }

    sign = spare & 1;
    spare >>= 1;
    spare_bits--;

    if (sign)
        return -sigma/G_SQRT2*log(x);
    else
        return sigma/G_SQRT2*log(x);
}

static gdouble
rand_gen_uniform(GRand *rng,
                 gdouble sigma)
{
    gdouble x;

    if (G_UNLIKELY(!rng))
        return 0.0;

    do {
        x = g_rand_double(rng);
    } while (G_UNLIKELY(x == 0.0));

    return (2.0*x - 1.0)*GWY_SQRT3*sigma;
}

static gdouble
rand_gen_triangle(GRand *rng,
                  gdouble sigma)
{
    gdouble x;

    if (G_UNLIKELY(!rng))
        return 0.0;

    do {
        x = g_rand_double(rng);
    } while (G_UNLIKELY(x == 0.0));

    return (x <= 0.5 ? sqrt(2.0*x) - 1.0 : 1.0 - sqrt(2.0*(1.0 - x)))
           *sigma*GWY_SQRT6;
}

/* XXX: Sometimes the generators seem unnecessarily complicated; this is to
 * make the positive and negative noise related to the symmetrical one. */

static gdouble
noise_gaussian_both(GRand *rng, gdouble sigma)
{
    return rand_gen_gaussian(rng, sigma);
}

static gdouble
noise_gaussian_up(GRand *rng, gdouble sigma)
{
    return fabs(rand_gen_gaussian(rng, sigma));
}

static gdouble
noise_gaussian_down(GRand *rng, gdouble sigma)
{
    return -fabs(rand_gen_gaussian(rng, sigma));
}

static gdouble
noise_exp_both(GRand *rng, gdouble sigma)
{
    return rand_gen_exp(rng, sigma);
}

static gdouble
noise_exp_up(GRand *rng, gdouble sigma)
{
    gdouble x = g_rand_double(rng);

    if (G_UNLIKELY(x == 0.0))
        return 0.0;

    return -sigma/G_SQRT2*log(x);
}

static gdouble
noise_exp_down(GRand *rng, gdouble sigma)
{
    gdouble x = g_rand_double(rng);

    if (G_UNLIKELY(x == 0.0))
        return 0.0;

    return sigma/G_SQRT2*log(x);
}

static gdouble
noise_uniform_both(GRand *rng, gdouble sigma)
{
    return rand_gen_uniform(rng, sigma);
}

static gdouble
noise_uniform_up(GRand *rng, gdouble sigma)
{
    return fabs(rand_gen_uniform(rng, sigma));
}

static gdouble
noise_uniform_down(GRand *rng, gdouble sigma)
{
    return -fabs(rand_gen_uniform(rng, sigma));
}

static gdouble
noise_triangle_both(GRand *rng, gdouble sigma)
{
    return rand_gen_triangle(rng, sigma);
}

static gdouble
noise_triangle_up(GRand *rng, gdouble sigma)
{
    return fabs(rand_gen_triangle(rng, sigma));
}

static gdouble
noise_triangle_down(GRand *rng, gdouble sigma)
{
    return -fabs(rand_gen_triangle(rng, sigma));
}

static const gchar active_page_key[]  = "/module/lno_synth/active_page";
static const gchar update_key[]       = "/module/lno_synth/update";
static const gchar randomize_key[]    = "/module/lno_synth/randomize";
static const gchar seed_key[]         = "/module/lno_synth/seed";
static const gchar distribution_key[] = "/module/lno_synth/distribution";
static const gchar direction_key[]    = "/module/lno_synth/direction";
static const gchar sigma_key[]        = "/module/lno_synth/sigma";
static const gchar type_key[]         = "/module/lno_synth/type";

static void
lno_synth_sanitize_args(LNoSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = !!args->update;
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->distribution = MIN(args->distribution, LNO_DISTRIBUTION_NTYPES-1);
    args->direction = MIN(args->direction, LNO_DIRECTION_NTYPES-1);
    args->sigma = CLAMP(args->sigma, 0.001, 10000.0);
    args->type = MIN(args->type, LNO_SYNTH_NTYPES-1);
}

static void
lno_synth_load_args(GwyContainer *container,
                    LNoSynthArgs *args,
                    GwyDimensionArgs *dimsargs)
{
    *args = lno_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_enum_by_name(container, distribution_key,
                                   &args->distribution);
    gwy_container_gis_enum_by_name(container, direction_key, &args->direction);
    gwy_container_gis_double_by_name(container, sigma_key, &args->sigma);
    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    lno_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
lno_synth_save_args(GwyContainer *container,
                    const LNoSynthArgs *args,
                    const GwyDimensionArgs *dimsargs)
{
    gwy_container_set_int32_by_name(container, active_page_key,
                                    args->active_page);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
    gwy_container_set_enum_by_name(container, distribution_key,
                                   args->distribution);
    gwy_container_set_enum_by_name(container, direction_key, args->direction);
    gwy_container_set_double_by_name(container, sigma_key, args->sigma);
    gwy_container_set_enum_by_name(container, type_key, args->type);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
