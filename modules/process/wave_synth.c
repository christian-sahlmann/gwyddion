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
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define WAVE_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

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
    WAVE_TYPE_SINE            = 0,
    WAVE_TYPE_INVCOSH         = 1,
    WAVE_TYPE_NTYPES
} WaveTypeType;

typedef enum {
    WAVE_QUANTITY_DISPLACEMENT = 0,
    WAVE_QUANTITY_INTENSITY    = 1,
    WAVE_QUANTITY_PHASE        = 2,
    WAVE_QUANTITY_NTYPES
} WaveQuantityType;

typedef struct _WaveSynthArgs WaveSynthArgs;
typedef struct _WaveSynthControls WaveSynthControls;

struct _WaveSynthArgs {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;
    WaveTypeType type;
    WaveQuantityType quantity;
    guint nwaves;
    gdouble x;
    gdouble x_noise;
    gdouble y;
    gdouble y_noise;
    gdouble amplitude;
    gdouble amplitude_noise;
    gdouble k;
    gdouble k_noise;
};

struct _WaveSynthControls {
    WaveSynthArgs *args;
    GwyDimensions *dims;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkTable *table;
    GtkWidget *type;
    GtkWidget *quantity;
    GtkObject *nwaves;
    GtkObject *x;
    GtkWidget *x_units;
    GtkObject *x_noise;
    GtkObject *y;
    GtkWidget *y_units;
    GtkObject *y_noise;
    GtkObject *amplitude;
    GtkWidget *amplitude_units;
    GtkObject *amplitude_noise;
    GtkWidget *amplitude_init;
    GtkObject *k;
    GtkWidget *k_units;
    GtkObject *k_noise;
    GwyContainer *mydata;
    GwyDataField *surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
    gulong sid;
};

static gboolean   module_register       (void);
static void       wave_synth            (GwyContainer *data,
                                         GwyRunType run);
static void       run_noninteractive    (WaveSynthArgs *args,
                                         const GwyDimensionArgs *dimsargs,
                                         GwyContainer *data,
                                         GwyDataField *dfield,
                                         gint oldid,
                                         GQuark quark);
static gboolean   wave_synth_dialog     (WaveSynthArgs *args,
                                         GwyDimensionArgs *dimsargs,
                                         GwyContainer *data,
                                         GwyDataField *dfield,
                                         gint id);
static GtkWidget* type_selector_new     (WaveSynthControls *controls);
static GtkWidget* quantity_selector_new (WaveSynthControls *controls);
static void       update_controls       (WaveSynthControls *controls,
                                         WaveSynthArgs *args);
static void       page_switched         (WaveSynthControls *controls,
                                         GtkNotebookPage *page,
                                         gint pagenum);
static void       update_values         (WaveSynthControls *controls);
static void       wave_type_selected    (GtkComboBox *combo,
                                         WaveSynthControls *controls);
static void       quantity_type_selected(GtkComboBox *combo,
                                         WaveSynthControls *controls);
static void       amplitude_init_clicked(WaveSynthControls *controls);
static void       wave_synth_invalidate (WaveSynthControls *controls);
static gboolean   preview_gsource       (gpointer user_data);
static void       preview               (WaveSynthControls *controls);
static void       wave_synth_do         (const WaveSynthArgs *args,
                                         const GwyDimensionArgs *dimsargs,
                                         GwyDataField *dfield);
static void       wave_synth_load_args  (GwyContainer *container,
                                         WaveSynthArgs *args,
                                         GwyDimensionArgs *dimsargs);
static void       wave_synth_save_args  (GwyContainer *container,
                                         const WaveSynthArgs *args,
                                         const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS WaveSynthControls
#define GWY_SYNTH_INVALIDATE(controls) wave_synth_invalidate(controls)

#include "synth.h"

static const gchar prefix[] = "/module/wave_synth";

static const WaveSynthArgs wave_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    WAVE_TYPE_SINE, WAVE_QUANTITY_INTENSITY,
    100,
    0.0, 1.0,
    0.0, 1.0,
    1.0, 0.0,
    20.0, 0.1,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates various kinds of waves."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("wave_synth",
                              (GwyProcessFunc)&wave_synth,
                              N_("/S_ynthetic/_Waves..."),
                              NULL,
                              WAVE_SYNTH_RUN_MODES,
                              0,
                              N_("Generate waves"));

    return TRUE;
}

static void
wave_synth(GwyContainer *data, GwyRunType run)
{
    WaveSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & WAVE_SYNTH_RUN_MODES);
    wave_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || wave_synth_dialog(&args, &dimsargs, data, dfield, id)) {
        wave_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);
    }

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(WaveSynthArgs *args,
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

        gwy_app_channel_log_add(data, oldid, oldid, "proc::wave_synth", NULL);
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

    wave_synth_do(args, dimsargs, dfield);

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
                                "proc::wave_synth", NULL);
    }
    g_object_unref(dfield);
}

static gboolean
wave_synth_dialog(WaveSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *label;
    WaveSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    controls.pxsize = 1.0;
    dialog = gtk_dialog_new_with_buttons(_("Waves"),
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
                                 G_CALLBACK(wave_synth_invalidate), &controls);

    table = gtk_table_new(16, 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(controls.table, 2);
    gtk_table_set_col_spacings(controls.table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.quantity = quantity_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Quantity:"), NULL,
                            GTK_OBJECT(controls.quantity),
                            GWY_HSCALE_WIDGET);
    row++;

    controls.nwaves = gtk_adjustment_new(args->nwaves, 1, 10000, 1, 10, 0);
    g_object_set_data(G_OBJECT(controls.nwaves), "target", &args->nwaves);
    gwy_table_attach_hscale(table, row, _("_Number of waves:"), NULL,
                            GTK_OBJECT(controls.nwaves), GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.nwaves, "changed",
                             G_CALLBACK(gwy_synth_int_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Amplitude")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.type = type_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Wave form:"), NULL,
                            GTK_OBJECT(controls.type),
                            GWY_HSCALE_WIDGET);
    row++;

    row = gwy_synth_attach_height(&controls, row,
                                  &controls.amplitude, &args->amplitude,
                                  _("Amplitude:"),
                                  NULL, &controls.amplitude_units);

    if (dfield_template) {
        controls.amplitude_init
            = gtk_button_new_with_mnemonic(_("_Like Current Channel"));
        g_signal_connect_swapped(controls.amplitude_init, "clicked",
                                 G_CALLBACK(amplitude_init_clicked), &controls);
        gtk_table_attach(GTK_TABLE(table), controls.amplitude_init,
                         1, 3, row, row+1, GTK_FILL, 0, 0, 0);
        row++;
    }

    row = gwy_synth_attach_variance(&controls, row,
                                    &controls.amplitude_noise,
                                    &args->amplitude_noise);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Frequency")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Sources")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);
    wave_synth_invalidate(&controls);

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
            args->seed = wave_synth_defaults.seed;
            args->randomize = wave_synth_defaults.randomize;
            /* Don't reset type either.  It sort of defeats resetting the
             * noise-specific options. */
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

static GtkWidget*
type_selector_new(WaveSynthControls *controls)
{
    static const GwyEnum wave_types[] = {
        { N_("Sine"),         WAVE_TYPE_SINE   },
        { N_("Inverse cosh"), WAVE_TYPE_INVCOSH },
    };
    GtkWidget *combo;

    combo = gwy_enum_combo_box_new(wave_types, G_N_ELEMENTS(wave_types),
                                   G_CALLBACK(wave_type_selected),
                                   controls, controls->args->type, TRUE);
    return combo;
}

static GtkWidget*
quantity_selector_new(WaveSynthControls *controls)
{
    static const GwyEnum quantity_types[] = {
        { N_("Displacement"), WAVE_QUANTITY_DISPLACEMENT },
        { N_("Intensity"),    WAVE_QUANTITY_INTENSITY    },
        { N_("Phase"),        WAVE_QUANTITY_PHASE        },
    };
    GtkWidget *combo;

    combo = gwy_enum_combo_box_new(quantity_types, G_N_ELEMENTS(quantity_types),
                                   G_CALLBACK(quantity_type_selected),
                                   controls, controls->args->quantity, TRUE);
    return combo;
}

static void
update_controls(WaveSynthControls *controls,
                WaveSynthArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->quantity),
                                  args->quantity);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->type), args->type);
    /* TODO */
}

static void
page_switched(WaveSynthControls *controls,
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
update_values(WaveSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    if (controls->amplitude_units)
        gtk_label_set_markup(GTK_LABEL(controls->amplitude_units),
                             dims->zvf->units);

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
}

static void
wave_type_selected(GtkComboBox *combo,
                   WaveSynthControls *controls)
{
    controls->args->type = gwy_enum_combo_box_get_active(combo);
    wave_synth_invalidate(controls);
}

static void
quantity_type_selected(GtkComboBox *combo,
                           WaveSynthControls *controls)
{
    controls->args->quantity = gwy_enum_combo_box_get_active(combo);
    wave_synth_invalidate(controls);
}

static void
amplitude_init_clicked(WaveSynthControls *controls)
{
    gdouble mag = pow10(controls->dims->args->zpow10);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->amplitude),
                             controls->zscale/mag);
}

static void
wave_synth_invalidate(WaveSynthControls *controls)
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
    WaveSynthControls *controls = (WaveSynthControls*)user_data;
    controls->sid = 0;

    preview(controls);

    return FALSE;
}

static void
preview(WaveSynthControls *controls)
{
    WaveSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, FALSE);
    else
        gwy_data_field_clear(dfield);

    wave_synth_do(args, controls->dims->args, dfield);
}

static void
wave_synth_do(const WaveSynthArgs *args,
             const GwyDimensionArgs *dimsargs,
             GwyDataField *dfield)
{
    /* TODO */
    gwy_data_field_data_changed(dfield);
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

static const gchar active_page_key[]     = "/module/wave_synth/active_page";
static const gchar update_key[]          = "/module/wave_synth/update";
static const gchar randomize_key[]       = "/module/wave_synth/randomize";
static const gchar seed_key[]            = "/module/wave_synth/seed";
static const gchar type_key[]            = "/module/wave_synth/type";
static const gchar quantity_key[]        = "/module/wave_synth/quantity";
static const gchar nwaves_key[]          = "/module/wave_synth/nwaves";
static const gchar x_key[]               = "/module/wave_synth/x";
static const gchar x_noise_key[]         = "/module/wave_synth/x_noise";
static const gchar y_key[]               = "/module/wave_synth/y";
static const gchar y_noise_key[]         = "/module/wave_synth/y_noise";
static const gchar amplitude_key[]       = "/module/wave_synth/amplitude";
static const gchar amplitude_noise_key[] = "/module/wave_synth/amplitude_noise";
static const gchar k_key[]               = "/module/wave_synth/k";
static const gchar k_noise_key[]         = "/module/wave_synth/k_noise";

static void
wave_synth_sanitize_args(WaveSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = !!args->update;
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->type = MIN(args->type, WAVE_TYPE_NTYPES-1);
    args->quantity = MIN(args->quantity, WAVE_QUANTITY_NTYPES-1);
    args->nwaves = CLAMP(args->nwaves, 1, 10000);
    args->x = CLAMP(args->x, -1000.0, 1000.0);
    args->x_noise = CLAMP(args->x_noise, 0.0, 1.0);
    args->y = CLAMP(args->y, -1000.0, 1000.0);
    args->y_noise = CLAMP(args->y_noise, 0.0, 1.0);
    args->amplitude = CLAMP(args->amplitude, 0.001, 10000.0);
    args->amplitude_noise = CLAMP(args->amplitude_noise, 0.0, 1.0);
    args->k = CLAMP(args->k, 0.1, 1000.0);
    args->k_noise = CLAMP(args->k_noise, 0.0, 1.0);
}

static void
wave_synth_load_args(GwyContainer *container,
                    WaveSynthArgs *args,
                    GwyDimensionArgs *dimsargs)
{
    *args = wave_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    gwy_container_gis_enum_by_name(container, quantity_key, &args->quantity);
    gwy_container_gis_int32_by_name(container, nwaves_key, &args->nwaves);
    gwy_container_gis_double_by_name(container, x_key, &args->x);
    gwy_container_gis_double_by_name(container, x_noise_key, &args->x_noise);
    gwy_container_gis_double_by_name(container, y_key, &args->y);
    gwy_container_gis_double_by_name(container, y_noise_key, &args->y_noise);
    gwy_container_gis_double_by_name(container, amplitude_key,
                                     &args->amplitude);
    gwy_container_gis_double_by_name(container, amplitude_noise_key,
                                     &args->amplitude_noise);
    gwy_container_gis_double_by_name(container, k_key, &args->k);
    gwy_container_gis_double_by_name(container, k_noise_key, &args->k_noise);
    wave_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
wave_synth_save_args(GwyContainer *container,
                    const WaveSynthArgs *args,
                    const GwyDimensionArgs *dimsargs)
{
    gwy_container_set_int32_by_name(container, active_page_key,
                                    args->active_page);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
    gwy_container_set_enum_by_name(container, type_key, args->type);
    gwy_container_set_enum_by_name(container, quantity_key,
                                   args->quantity);
    gwy_container_set_int32_by_name(container, nwaves_key, args->nwaves);
    gwy_container_set_double_by_name(container, x_key, args->x);
    gwy_container_set_double_by_name(container, x_noise_key, args->x_noise);
    gwy_container_set_double_by_name(container, y_key, args->y);
    gwy_container_set_double_by_name(container, y_noise_key, args->y_noise);
    gwy_container_set_double_by_name(container, amplitude_key,
                                     args->amplitude);
    gwy_container_set_double_by_name(container, amplitude_noise_key,
                                     args->amplitude_noise);
    gwy_container_set_double_by_name(container, k_key, args->k);
    gwy_container_set_double_by_name(container, k_noise_key, args->k_noise);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
