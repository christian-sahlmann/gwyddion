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
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define GWY_SQRT6 2.449489742783178098197284074705

#define NOISE_DISTRIBUTION_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define DECLARE_NOISE(name) \
    static gdouble noise_##name##_both(GRand *rng, gdouble sigma); \
    static gdouble noise_##name##_up(GRand *rng, gdouble sigma); \
    static gdouble noise_##name##_down(GRand *rng, gdouble sigma); \
    static gdouble rand_gen_##name(GRand *rng, gdouble sigma)

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
    NOISE_DISTRIBUTION_GAUSSIAN    = 0,
    NOISE_DISTRIBUTION_EXPONENTIAL = 1,
    NOISE_DISTRIBUTION_UNIFORM     = 2,
    NOISE_DISTRIBUTION_TRIANGULAR  = 3,
    NOISE_DISTRIBUTION_NTYPES
} NoiseDistributionType;

typedef enum {
    NOISE_DIRECTION_BOTH = 0,
    NOISE_DIRECTION_UP   = 1,
    NOISE_DIRECTION_DOWN = 2,
    NOISE_DIRECTION_NTYPES
} NoiseDirectionType;

typedef gdouble (*PointNoiseFunc)(GRand *rng, gdouble sigma);

/* This scheme makes the distribution type list easily reordeable in the GUI
 * without changing the ids.  Directions need not be reordered. */
typedef struct {
    NoiseDistributionType distribution;
    const gchar *name;
    PointNoiseFunc point_noise[NOISE_DIRECTION_NTYPES];
    PointNoiseFunc base_generator;
} NoiseSynthGenerator;

typedef struct {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;
    NoiseDistributionType distribution;
    NoiseDirectionType direction;
    gdouble sigma;
} NoiseSynthArgs;

typedef struct {
    NoiseSynthArgs *args;
    GwyDimensions *dims;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkTable *table;
    GtkWidget *randomize;
    GtkWidget *distribution;
    GSList *direction;
    GtkObject *sigma;
    GtkWidget *sigma_units;
    GtkWidget *sigma_init;
    GwyContainer *mydata;
    GwyDataField *surface;
    GwyDataField *noise;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
    gulong sid;
} NoiseSynthControls;

static gboolean      module_register           (void);
static void          noise_synth               (GwyContainer *data,
                                                GwyRunType run);
static void          run_noninteractive        (NoiseSynthArgs *args,
                                                const GwyDimensionArgs *dimsargs,
                                                GwyContainer *data,
                                                GwyDataField *dfield,
                                                gint oldid,
                                                GQuark quark);
static gboolean      noise_synth_dialog        (NoiseSynthArgs *args,
                                                GwyDimensionArgs *dimsargs,
                                                GwyContainer *data,
                                                GwyDataField *dfield,
                                                gint id);
static GtkWidget*    distribution_selector_new (NoiseSynthControls *controls);
static void          update_controls           (NoiseSynthControls *controls,
                                                NoiseSynthArgs *args);
static void          page_switched             (NoiseSynthControls *controls,
                                                GtkNotebookPage *page,
                                                gint pagenum);
static void          update_values             (NoiseSynthControls *controls);
static void          distribution_type_selected(GtkComboBox *combo,
                                                NoiseSynthControls *controls);
static void          direction_type_changed    (GtkWidget *button,
                                                NoiseSynthControls *controls);
static void          sigma_init_clicked        (NoiseSynthControls *controls);
static void          noise_synth_invalidate    (NoiseSynthControls *controls);
static gboolean      preview_gsource           (gpointer user_data);
static void          preview                   (NoiseSynthControls *controls);
static void          noise_synth_do            (const NoiseSynthArgs *args,
                                                const GwyDimensionArgs *dimsargs,
                                                GwyDataField *dfield);
static void          noise_synth_load_args     (GwyContainer *container,
                                                NoiseSynthArgs *args,
                                                GwyDimensionArgs *dimsargs);
static void          noise_synth_save_args     (GwyContainer *container,
                                                const NoiseSynthArgs *args,
                                                const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS NoiseSynthControls
#define GWY_SYNTH_INVALIDATE(controls) \
    noise_synth_invalidate(controls)

#include "synth.h"

DECLARE_NOISE(gaussian);
DECLARE_NOISE(exp);
DECLARE_NOISE(uniform);
DECLARE_NOISE(triangle);

static const NoiseSynthArgs noise_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    NOISE_DISTRIBUTION_GAUSSIAN, NOISE_DIRECTION_BOTH,
    1.0,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static const NoiseSynthGenerator generators[] = {
    {
        NOISE_DISTRIBUTION_GAUSSIAN,
        N_("distribution|Gaussian"),
        { &noise_gaussian_both, &noise_gaussian_up, &noise_gaussian_down, },
        rand_gen_gaussian,
    },
    {
        NOISE_DISTRIBUTION_EXPONENTIAL,
        N_("distribution|Exponential"),
        { &noise_exp_both, &noise_exp_up, &noise_exp_down, },
        rand_gen_exp,
    },
    {
        NOISE_DISTRIBUTION_UNIFORM,
        N_("distribution|Uniform"),
        { &noise_uniform_both, &noise_uniform_up, &noise_uniform_down, },
        rand_gen_uniform,
    },
    {
        NOISE_DISTRIBUTION_TRIANGULAR,
        N_("distribution|Triangular"),
        { &noise_triangle_both, &noise_triangle_up, &noise_triangle_down, },
        rand_gen_triangle,
    },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates uncorrelated random noise."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("noise_synth",
                              (GwyProcessFunc)&noise_synth,
                              N_("/S_ynthetic/_Noise..."),
                              NULL,
                              NOISE_DISTRIBUTION_RUN_MODES,
                              0,
                              N_("Generate surface of uncorrelated noise"));

    return TRUE;
}

static void
noise_synth(GwyContainer *data, GwyRunType run)
{
    NoiseSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & NOISE_DISTRIBUTION_RUN_MODES);
    noise_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || noise_synth_dialog(&args, &dimsargs, data, dfield, id))
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);

    if (run == GWY_RUN_INTERACTIVE)
        noise_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(NoiseSynthArgs *args,
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

    noise_synth_do(args, dimsargs, dfield);

    if (replace)
        gwy_data_field_data_changed(dfield);
    else {
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
noise_synth_dialog(NoiseSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *label;
    NoiseSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Random Noise"),
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
                                TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    controls.noise = gwy_data_field_new_alike(dfield, FALSE);
    if (data)
        gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                0);
    if (dfield_template) {
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
                                 G_CALLBACK(noise_synth_invalidate), &controls);

    table = gtk_table_new(6 + (dfield_template ? 1 : 0), 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(controls.table, 2);
    gtk_table_set_col_spacings(controls.table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

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
                                    _("S_ymmetrical"), NOISE_DIRECTION_BOTH,
                                    _("One-sided _positive"), NOISE_DIRECTION_UP,
                                    _("One-sided _negative"), NOISE_DIRECTION_DOWN,
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

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);
    noise_synth_invalidate(&controls);

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
                *args = noise_synth_defaults;
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
    gwy_object_unref(controls.noise);
    gwy_dimensions_free(controls.dims);

    return response == GTK_RESPONSE_OK;
}

static const NoiseSynthGenerator*
get_point_noise_generator(NoiseDistributionType distribution)
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

static GtkWidget*
distribution_selector_new(NoiseSynthControls *controls)
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
update_controls(NoiseSynthControls *controls,
                NoiseSynthArgs *args)
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
}

static void
page_switched(NoiseSynthControls *controls,
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
update_values(NoiseSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
    if (controls->sigma_units)
        gtk_label_set_markup(GTK_LABEL(controls->sigma_units),
                             dims->zvf->units);
}

static void
distribution_type_selected(GtkComboBox *combo,
                           NoiseSynthControls *controls)
{
    controls->args->distribution = gwy_enum_combo_box_get_active(combo);
    noise_synth_invalidate(controls);
}

static void
direction_type_changed(GtkWidget *button,
                       NoiseSynthControls *controls)
{
    controls->args->direction = gwy_radio_button_get_value(button);
    noise_synth_invalidate(controls);
}

static void
sigma_init_clicked(NoiseSynthControls *controls)
{
    gdouble mag = pow10(controls->dims->args->zpow10);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->sigma),
                             controls->zscale/mag);
}

static void
noise_synth_invalidate(NoiseSynthControls *controls)
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
    NoiseSynthControls *controls = (NoiseSynthControls*)user_data;
    controls->sid = 0;

    preview(controls);

    return FALSE;
}

static void
preview(NoiseSynthControls *controls)
{
    NoiseSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, FALSE);
    else
        gwy_data_field_clear(dfield);

    noise_synth_do(args, controls->dims->args, dfield);

    gwy_data_field_data_changed(dfield);
}

static void
noise_synth_do(const NoiseSynthArgs *args,
               const GwyDimensionArgs *dimsargs,
               GwyDataField *dfield)
{
    const NoiseSynthGenerator *generator;
    PointNoiseFunc point_noise;
    GRand *rng;
    gdouble *data;
    gdouble sigma;
    gint xres, yres, i;

    generator = get_point_noise_generator(args->distribution);
    point_noise = generator->point_noise[args->direction];
    /* Clear spare values possibly saved in the base generator */
    generator->base_generator(NULL, 0.0);

    rng = g_rand_new();
    g_rand_set_seed(rng, args->seed);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);
    sigma = args->sigma * pow10(dimsargs->zpow10);

    for (i = 0; i < xres*yres; i++)
        data[i] += point_noise(rng, sigma);

    g_rand_free(rng);
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

static const gchar prefix[]           = "/module/noise_synth";
static const gchar active_page_key[]  = "/module/noise_synth/active_page";
static const gchar update_key[]       = "/module/noise_synth/update";
static const gchar randomize_key[]    = "/module/noise_synth/randomize";
static const gchar seed_key[]         = "/module/noise_synth/seed";
static const gchar distribution_key[] = "/module/noise_synth/distribution";
static const gchar direction_key[]    = "/module/noise_synth/direction";
static const gchar sigma_key[]        = "/module/noise_synth/sigma";

static void
noise_synth_sanitize_args(NoiseSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = !!args->update;
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->distribution = MIN(args->distribution, NOISE_DISTRIBUTION_NTYPES-1);
    args->direction = MIN(args->direction, NOISE_DIRECTION_NTYPES-1);
    args->sigma = CLAMP(args->sigma, 0.001, 10000.0);
}

static void
noise_synth_load_args(GwyContainer *container,
                      NoiseSynthArgs *args,
                      GwyDimensionArgs *dimsargs)
{
    *args = noise_synth_defaults;

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
    noise_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
noise_synth_save_args(GwyContainer *container,
                      const NoiseSynthArgs *args,
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

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
