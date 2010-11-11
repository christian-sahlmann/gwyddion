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
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define PAT_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define DECLARE_PATTERN(name) \
    static gpointer create_gui_##name(PatSynthControls *controls); \
    static void dimensions_changed_##name(PatSynthControls *controls); \
    static void make_pattern_##name(const PatSynthArgs *args, \
                                    const GwyDimensionArgs *dimsargs, \
                                    RandGenSet *rngset, \
                                    GwyDataField *dfield); \
    static gpointer load_args_##name(GwyContainer *settings); \
    static void save_args_##name(gpointer pargs, GwyContainer *settings); \
    static void reset_##name(gpointer pcontrols)

#define PATTERN_FUNCS(name) \
    &create_gui_##name, &dimensions_changed_##name, &reset_##name, \
    &make_pattern_##name, \
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

/* Each pattern has its own set of parameters but many are common so they get
 * the same symbolic name in PatSynthRng for simpliciy. */
typedef enum {
    RNG_FLAT      = 0,
    RNG_TOP       = 0,
    RNG_SLOPE     = 1,
    RNG_BOTTOM    = 2,
    RNG_HEIGHT    = 3,
    RNG_DISPLAC_X = 4,
    RNG_NRNGS
} PatSynthRng;

typedef enum {
    PAT_SYNTH_STEPS  = 0,
    PAT_SYNTH_RIDGES = 1,
    PAT_SYNTH_NTYPES
} PatSynthType;

typedef struct _PatSynthArgs PatSynthArgs;
typedef struct _PatSynthControls PatSynthControls;

typedef struct {
    guint n;
    GRand **rng;
} RandGenSet;

typedef gpointer (*CreateGUIFunc)(PatSynthControls *controls);
typedef void (*DestroyGUIFunc)(gpointer pcontrols);
typedef void (*DimensionsChangedFunc)(PatSynthControls *controls);
typedef void (*ResetFunc)(gpointer pcontrols);
typedef void (*MakePatternFunc)(const PatSynthArgs *args,
                                const GwyDimensionArgs *dimsargs,
                                RandGenSet *rngset,
                                GwyDataField *dfield);
typedef gpointer (*LoadArgsFunc)(GwyContainer *settings);
typedef void (*SaveArgsFunc)(gpointer pargs,
                             GwyContainer *settings);

typedef void (*UpdateValueFunc)(PatSynthControls *controls);

/* This scheme makes the object type list easily reordeable in the GUI without
 * changing the ids.  */
typedef struct {
    PatSynthType type;
    const gchar *name;
    CreateGUIFunc create_gui;
    DimensionsChangedFunc dims_changed;
    ResetFunc reset;
    MakePatternFunc run;
    LoadArgsFunc load_args;
    SaveArgsFunc save_args;
} PatSynthPattern;

struct _PatSynthArgs {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;
    PatSynthType type;
    gpointer pattern_args;
};

struct _PatSynthControls {
    PatSynthArgs *args;
    GwyDimensions *dims;
    const PatSynthPattern *pattern;
    RandGenSet *rngset;
    /* They have different types, known only to the pattern. */
    gpointer pattern_controls;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkTable *table;
    GtkWidget *type;
    GwyContainer *mydata;
    GwyDataField *surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
    gulong sid;
};

static gboolean      module_register      (void);
static void          pat_synth            (GwyContainer *data,
                                           GwyRunType run);
static void          run_noninteractive   (PatSynthArgs *args,
                                           const GwyDimensionArgs *dimsargs,
                                           RandGenSet *rngset,
                                           GwyContainer *data,
                                           GwyDataField *dfield,
                                           gint oldid,
                                           GQuark quark);
static gboolean      pat_synth_dialog     (PatSynthArgs *args,
                                           GwyDimensionArgs *dimsargs,
                                           RandGenSet *rngset,
                                           GwyContainer *data,
                                           GwyDataField *dfield,
                                           gint id);
static GtkWidget*    pattern_selector_new (PatSynthControls *controls);
static void          update_controls      (PatSynthControls *controls,
                                           PatSynthArgs *args);
static void          page_switched        (PatSynthControls *controls,
                                           GtkNotebookPage *page,
                                           gint pagenum);
static void          update_values        (PatSynthControls *controls);
static void          pattern_type_selected(GtkComboBox *combo,
                                           PatSynthControls *controls);
static void          pat_synth_invalidate (PatSynthControls *controls);
static gboolean      preview_gsource      (gpointer user_data);
static void          preview              (PatSynthControls *controls);
static void          pat_synth_do         (const PatSynthArgs *args,
                                           const GwyDimensionArgs *dimsargs,
                                           RandGenSet *rngset,
                                           GwyDataField *dfield);
static GwyDataField* make_displacement_map(guint xres,
                                           guint yres,
                                           gdouble sigma,
                                           gdouble tau,
                                           GRand *rng);
static RandGenSet*   rand_gen_set_new     (guint n);
static void          rand_gen_set_init    (RandGenSet *rngset,
                                           guint seed);
static void          rand_gen_set_free    (RandGenSet *rngset);
static void          pat_synth_load_args  (GwyContainer *container,
                                           PatSynthArgs *args,
                                           GwyDimensionArgs *dimsargs);
static void          pat_synth_save_args  (GwyContainer *container,
                                           const PatSynthArgs *args,
                                           const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS PatSynthControls
#define GWY_SYNTH_INVALIDATE(controls) \
    pat_synth_invalidate(controls)

#include "synth.h"

DECLARE_PATTERN(steps);
DECLARE_PATTERN(ridges);

static const gchar prefix[] = "/module/pat_synth";

static const PatSynthArgs pat_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    PAT_SYNTH_STEPS,
    NULL,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static const PatSynthPattern patterns[] = {
    { PAT_SYNTH_STEPS,  N_("Steps"),  PATTERN_FUNCS(steps),  },
    { PAT_SYNTH_RIDGES, N_("Ridges"), PATTERN_FUNCS(ridges), },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates surfaces representing simple patterns "
       "(steps, ridges, ...)."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("pat_synth",
                              (GwyProcessFunc)&pat_synth,
                              N_("/S_ynthetic/_Pattern..."),
                              NULL,
                              PAT_SYNTH_RUN_MODES,
                              0,
                              N_("Generate patterned surface"));

    return TRUE;
}

static void
pat_synth(GwyContainer *data, GwyRunType run)
{
    PatSynthArgs args;
    GwyDimensionArgs dimsargs;
    RandGenSet *rngset;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & PAT_SYNTH_RUN_MODES);
    pat_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    rngset = rand_gen_set_new(RNG_NRNGS);
    if (run == GWY_RUN_IMMEDIATE
        || pat_synth_dialog(&args, &dimsargs, rngset, data, dfield, id))
        run_noninteractive(&args, &dimsargs, rngset, data, dfield, id, quark);

    if (run == GWY_RUN_INTERACTIVE)
        pat_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);

    rand_gen_set_free(rngset);
    gwy_dimensions_free_args(&dimsargs);
}

static const PatSynthPattern*
get_pattern(guint type)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(patterns); i++) {
        if (patterns[i].type == type)
            return patterns + i;
    }
    g_warning("Unknown pattern %u\n", type);

    return patterns + 0;
}

static void
run_noninteractive(PatSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   RandGenSet *rngset,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint oldid,
                   GQuark quark)
{
    const PatSynthPattern *pattern;
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

    pattern = get_pattern(args->type);
    args->pattern_args = pattern->load_args(gwy_app_settings_get());
    pat_synth_do(args, dimsargs, rngset, dfield);
    g_free(args->pattern_args);
    args->pattern_args = NULL;

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
pat_synth_dialog(PatSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 RandGenSet *rngset,
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook;
    PatSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
    controls.rngset = rngset;
    controls.pxsize = 1.0;
    dialog = gtk_dialog_new_with_buttons(_("Pattern"),
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
                       gwy_synth_random_seed_new(&controls,
                                                 &controls.seed, &args->seed),
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
                                 G_CALLBACK(pat_synth_invalidate), &controls);

    table = gtk_table_new(1, 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(controls.table, 2);
    gtk_table_set_col_spacings(controls.table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.type = pattern_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Pattern:"), NULL,
                            GTK_OBJECT(controls.type), GWY_HSCALE_WIDGET);
    row++;

    g_object_set_data(G_OBJECT(controls.table),
                      "base-rows", GINT_TO_POINTER(row));
    pattern_type_selected(GTK_COMBO_BOX(controls.type), &controls);

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);
    pat_synth_invalidate(&controls);

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
            args->seed = pat_synth_defaults.seed;
            args->randomize = pat_synth_defaults.randomize;
            /* Don't reset type either.  It sort of defeats resetting the
             * pattern-specific options. */
            controls.in_init = TRUE;
            controls.pattern->reset(controls.pattern_controls);
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

    pattern_type_selected(NULL, &controls);
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
pattern_selector_new(PatSynthControls *controls)
{
    GtkWidget *combo;
    GwyEnum *model;
    guint n, i;

    n = G_N_ELEMENTS(patterns);
    model = g_new(GwyEnum, n);
    for (i = 0; i < n; i++) {
        model[i].value = patterns[i].type;
        model[i].name = patterns[i].name;
    }

    combo = gwy_enum_combo_box_new(model, n,
                                   G_CALLBACK(pattern_type_selected), controls,
                                   controls->args->type, TRUE);
    g_object_weak_ref(G_OBJECT(combo), (GWeakNotify)g_free, model);

    return combo;
}

static void
update_controls(PatSynthControls *controls,
                PatSynthArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->type), args->type);
}

static void
page_switched(PatSynthControls *controls,
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
update_values(PatSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
    controls->pattern->dims_changed(controls);
}

static void
pattern_type_selected(GtkComboBox *combo,
                      PatSynthControls *controls)
{
    const PatSynthPattern *pattern;
    PatSynthArgs *args = controls->args;
    guint baserows;

    if (controls->pattern) {
        pattern = controls->pattern;
        pattern->save_args(args->pattern_args, gwy_app_settings_get());
        controls->pattern = NULL;
        g_free(controls->pattern_controls);
        controls->pattern_controls = NULL;
        g_free(args->pattern_args);
        args->pattern_args = NULL;
    }

    /* Just tear-down */
    if (!combo)
        return;

    args->type = gwy_enum_combo_box_get_active(combo);
    pattern = controls->pattern = get_pattern(args->type);

    baserows = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(controls->table),
                                                 "base-rows"));
    gwy_synth_shrink_table(controls->table, baserows);

    args->pattern_args = pattern->load_args(gwy_app_settings_get());
    controls->pattern_controls = pattern->create_gui(controls);
    gtk_widget_show_all(GTK_WIDGET(controls->table));

    pat_synth_invalidate(controls);
}

static void
pat_synth_invalidate(PatSynthControls *controls)
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
    PatSynthControls *controls = (PatSynthControls*)user_data;
    controls->sid = 0;

    preview(controls);

    return FALSE;
}

static void
preview(PatSynthControls *controls)
{
    PatSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, FALSE);
    else
        gwy_data_field_clear(dfield);

    pat_synth_do(args, controls->dims->args, controls->rngset, dfield);
}

static void
pat_synth_do(const PatSynthArgs *args,
             const GwyDimensionArgs *dimsargs,
             RandGenSet *rngset,
             GwyDataField *dfield)
{
    const PatSynthPattern *pattern = get_pattern(args->type);

    rand_gen_set_init(rngset, args->seed);
    pattern->run(args, dimsargs, rngset, dfield);
    gwy_data_field_data_changed(dfield);
}

/* Gauss-like distribution with range limited to [1-range, 1+range] */
static inline gdouble
rand_gen_set_mult(RandGenSet *rngset,
                  guint i,
                  gdouble range)
{
    GRand *rng;

    rng = rngset->rng[i];

    /*
    return 1.0 + range*(g_rand_double(rng) - g_rand_double(rng)
                        + g_rand_double(rng) - g_rand_double(rng))/2.0;
                        */
    return 1.0 + range*(g_rand_double(rng) - g_rand_double(rng));
}

static guint
bisect_lower(const gdouble *a, guint n, gdouble x)
{
    guint lo = 0, hi = n-1;

    if (G_UNLIKELY(x < a[lo]))
        return 0;
    if (G_UNLIKELY(x >= a[hi]))
        return n-1;

    while (hi - lo > 1) {
        guint mid = (hi + lo)/2;

        if (x < a[mid])
            hi = mid;
        else
            lo = mid;
    }

    return lo;
}

static inline void
generate(gdouble *a, gdouble base, gdouble noise,
         RandGenSet *rngset, gint id)
{
    gdouble v = base;

    if (base && noise)
        v *= rand_gen_set_mult(rngset, id, noise);
    *a = v;
}

static inline void
accumulate(gdouble *a, gdouble base, gdouble noise,
           RandGenSet *rngset, gint id)
{
    gdouble v = base;

    if (base && noise)
        v *= rand_gen_set_mult(rngset, id, noise);
    *a = v + *(a - 1);
}

typedef struct {
    gdouble flat;
    gdouble flat_noise;
    gdouble slope;
    gdouble slope_noise;
    gdouble height;
    gdouble height_noise;
    gdouble angle;
    gdouble sigma;
    gdouble tau;
} PatSynthArgsSteps;

typedef struct {
    PatSynthArgsSteps *args;
    GtkObject *flat;
    GtkWidget *flat_value;
    GtkWidget *flat_units;
    GtkObject *flat_noise;
    GtkObject *slope;
    GtkWidget *slope_value;
    GtkWidget *slope_units;
    GtkObject *slope_noise;
    GtkObject *height;
    GtkWidget *height_units;
    GtkObject *height_noise;
    GtkObject *angle;
    GtkObject *sigma;
    GtkObject *tau;
    GtkWidget *tau_value;
    GtkWidget *tau_units;
} PatSynthControlsSteps;

static const PatSynthArgsSteps pat_synth_defaults_steps = {
    10.0, 0.0,
    1.0, 0.0,
    1.0, 0.0,
    0.0,
    0.0, 10.0,
};

static gpointer
create_gui_steps(PatSynthControls *controls)
{
    PatSynthControlsSteps *pcontrols;
    PatSynthArgsSteps *pargs;
    gint row;

    row = gwy_synth_extend_table(controls->table, 4+4+3+2+4);
    pcontrols = g_new0(PatSynthControlsSteps, 1);
    pargs = pcontrols->args = controls->args->pattern_args;

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Flat")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    pcontrols->flat = gtk_adjustment_new(pargs->flat,
                                         0.1, 1000.0, 0.01, 10.0, 0);
    row = gwy_synth_attach_lateral(controls, row,
                                   pcontrols->flat, &pargs->flat,
                                   _("_Flat width:"), GWY_HSCALE_LOG,
                                   NULL,
                                   &pcontrols->flat_value,
                                   &pcontrols->flat_units);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->flat_noise,
                                    &pargs->flat_noise);

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Slope")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    pcontrols->slope = gtk_adjustment_new(pargs->slope,
                                          0.0, 1000.0, 0.01, 10.0, 0);
    row = gwy_synth_attach_lateral(controls, row,
                                   pcontrols->slope, &pargs->slope,
                                   _("_Slope width:"), GWY_HSCALE_SQRT,
                                   NULL,
                                   &pcontrols->slope_value,
                                   &pcontrols->slope_units);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->slope_noise,
                                    &pargs->slope_noise);

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Height")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    row = gwy_synth_attach_height(controls, row,
                                  &pcontrols->height, &pargs->height,
                                  _("_Height:"),
                                  NULL, &pcontrols->height_units);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->height_noise,
                                    &pargs->height_noise);

    row = gwy_synth_attach_orientation(controls, row,
                                       &pcontrols->angle, &pargs->angle);

    row = gwy_synth_attach_deformation(controls, row,
                                       &pcontrols->sigma, &pargs->sigma,
                                       &pcontrols->tau, &pargs->tau,
                                       &pcontrols->tau_value,
                                       &pcontrols->tau_units);

    return pcontrols;
}

static void
dimensions_changed_steps(PatSynthControls *controls)
{
    PatSynthControlsSteps *pcontrols = controls->pattern_controls;
    GwyDimensions *dims = controls->dims;

    gtk_label_set_markup(GTK_LABEL(pcontrols->flat_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->slope_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->height_units), dims->zvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->tau_units), dims->xyvf->units);

    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->flat));
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->slope));
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->tau));
}

static void
make_pattern_steps(const PatSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   RandGenSet *rngset,
                   GwyDataField *dfield)
{
    const PatSynthArgsSteps *pargs = args->pattern_args;
    GwyDataField *displacement_x;
    guint n, i, j, k, xres, yres;
    gdouble *abscissa, *height, *data, *dx_data;
    gdouble h, c, s, xoff, yoff, range;

    h = pargs->height * pow10(dimsargs->zpow10);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    range = (hypot(xres, yres) + pargs->sigma)/(pargs->flat + pargs->slope);
    n = GWY_ROUND(2.0*range + 16);
    abscissa = g_new(gdouble, 2*n);
    height = g_new(gdouble, n+1);

    abscissa[0] = -range - 8;
    accumulate(abscissa + 1, pargs->slope, pargs->slope_noise,
               rngset, RNG_SLOPE);
    height[0] = 0.0;

    for (k = 1; k < n; k++) {
        accumulate(abscissa + 2*k, pargs->flat, pargs->flat_noise,
                   rngset, RNG_FLAT);
        accumulate(abscissa + 2*k + 1, pargs->slope, pargs->slope_noise,
                   rngset, RNG_SLOPE);
        accumulate(height + k, h, pargs->height_noise,
                   rngset, RNG_HEIGHT);
    }
    accumulate(height + n, h, pargs->height_noise,
               rngset, RNG_HEIGHT);

    displacement_x = make_displacement_map(xres, yres,
                                           pargs->sigma, pargs->tau,
                                           rngset->rng[RNG_DISPLAC_X]);
    dx_data = gwy_data_field_get_data(displacement_x);

    c = cos(pargs->angle);
    s = sin(pargs->angle);
    xoff = 0.5*((1.0 - c)*xres + s*yres);
    yoff = 0.5*(-s*yres + (1.0 - c)*yres);
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble v, x, xu, yu, d, q;
            guint kmod;

            xu = xoff + j*c - i*s;
            yu = yoff + j*s + i*c;
            x = xu + dx_data[i*xres + j];
            k = bisect_lower(abscissa, 2*n, x);
            kmod = k % 2;
            if (!kmod) {
                d = abscissa[k+1] - abscissa[k];
                q = G_LIKELY(d) ? (x - abscissa[k])/d : 0.5;
                v = (1.0 - q)*height[k/2] + q*height[k/2 + 1];
            }
            else
                v = height[k/2 + 1];
            data[i*xres + j] += v;
        }
    }

    g_free(height);
    g_free(abscissa);
    g_object_unref(displacement_x);
}

static gpointer
load_args_steps(GwyContainer *settings)
{
    PatSynthArgsSteps *pargs;
    GString *key;

    pargs = g_memdup(&pat_synth_defaults_steps, sizeof(PatSynthArgsSteps));
    key = g_string_new(prefix);
    g_string_append(key, "/steps/");
    gwy_synth_load_arg_double(settings, key, "flat", 0.1, 1000.0,
                              &pargs->flat);
    gwy_synth_load_arg_double(settings, key, "flat_noise", 0.0, 1.0,
                              &pargs->flat_noise);
    gwy_synth_load_arg_double(settings, key, "slope", 0.0, 1000.0,
                              &pargs->slope);
    gwy_synth_load_arg_double(settings, key, "slope_noise", 0.0, 1.0,
                              &pargs->slope_noise);
    gwy_synth_load_arg_double(settings, key, "height", 0.0001, 10000.0,
                              &pargs->height);
    gwy_synth_load_arg_double(settings, key, "height_noise", 0.0, 1.0,
                              &pargs->height_noise);
    gwy_synth_load_arg_double(settings, key, "angle", -G_PI, G_PI,
                              &pargs->angle);
    gwy_synth_load_arg_double(settings, key, "sigma", 0.0, 100.0,
                              &pargs->sigma);
    gwy_synth_load_arg_double(settings, key, "tau", 0.1, 1000.0,
                              &pargs->tau);
    g_string_free(key, TRUE);

    return pargs;
}

static void
save_args_steps(gpointer p,
                GwyContainer *settings)
{
    PatSynthArgsSteps *pargs = p;
    GString *key;

    key = g_string_new(prefix);
    g_string_append(key, "/steps/");
    gwy_synth_save_arg_double(settings, key, "flat", pargs->flat);
    gwy_synth_save_arg_double(settings, key, "flat_noise", pargs->flat_noise);
    gwy_synth_save_arg_double(settings, key, "slope", pargs->slope);
    gwy_synth_save_arg_double(settings, key, "slope_noise", pargs->slope_noise);
    gwy_synth_save_arg_double(settings, key, "height", pargs->height);
    gwy_synth_save_arg_double(settings, key, "height_noise", pargs->height_noise);
    gwy_synth_save_arg_double(settings, key, "angle", pargs->angle);
    gwy_synth_save_arg_double(settings, key, "sigma", pargs->sigma);
    gwy_synth_save_arg_double(settings, key, "tau", pargs->tau);
    g_string_free(key, TRUE);
}

static void
reset_steps(gpointer p)
{
    PatSynthControlsSteps *pcontrols = p;
    PatSynthArgsSteps *pargs = pcontrols->args;

    *pargs = pat_synth_defaults_steps;

    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->flat), pargs->flat);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->flat_noise),
                             pargs->flat_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->slope), pargs->slope);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->slope_noise),
                             pargs->slope_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->height), pargs->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->height_noise),
                             pargs->height_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->angle), pargs->angle);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->sigma), pargs->sigma);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->tau), pargs->tau);
}

typedef struct {
    gdouble top;
    gdouble top_noise;
    gdouble bottom;
    gdouble bottom_noise;
    gdouble slope;
    gdouble slope_noise;
    gdouble height;
    gdouble height_noise;
    gdouble angle;
    gdouble sigma;
    gdouble tau;
} PatSynthArgsRidges;

typedef struct {
    PatSynthArgsRidges *args;
    GtkObject *top;
    GtkWidget *top_value;
    GtkWidget *top_units;
    GtkObject *top_noise;
    GtkObject *bottom;
    GtkWidget *bottom_value;
    GtkWidget *bottom_units;
    GtkObject *bottom_noise;
    GtkObject *slope;
    GtkWidget *slope_value;
    GtkWidget *slope_units;
    GtkObject *slope_noise;
    GtkObject *height;
    GtkWidget *height_units;
    GtkObject *height_noise;
    GtkObject *angle;
    GtkObject *sigma;
    GtkObject *tau;
    GtkWidget *tau_value;
    GtkWidget *tau_units;
} PatSynthControlsRidges;

static const PatSynthArgsRidges pat_synth_defaults_ridges = {
    10.0, 0.0,
    10.0, 0.0,
    1.0, 0.0,
    1.0, 0.0,
    0.0,
    0.0, 10.0,
};

static gpointer
create_gui_ridges(PatSynthControls *controls)
{
    PatSynthControlsRidges *pcontrols;
    PatSynthArgsRidges *pargs;
    gint row;

    row = gwy_synth_extend_table(controls->table, 4+4+4+3+2+4);
    pcontrols = g_new0(PatSynthControlsRidges, 1);
    pargs = pcontrols->args = controls->args->pattern_args;

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Top")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    pcontrols->top = gtk_adjustment_new(pargs->top,
                                        0.1, 1000.0, 0.01, 10.0, 0);
    row = gwy_synth_attach_lateral(controls, row, pcontrols->top, &pargs->top,
                                   _("Flat _top width:"), GWY_HSCALE_LOG,
                                   NULL,
                                   &pcontrols->top_value,
                                   &pcontrols->top_units);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->top_noise, &pargs->top_noise);

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Bottom")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    pcontrols->bottom = gtk_adjustment_new(pargs->bottom,
                                           0.1, 1000.0, 0.01, 10.0, 0);
    row = gwy_synth_attach_lateral(controls, row,
                                   pcontrols->bottom, &pargs->bottom,
                                   _("Flat _bottom width:"), GWY_HSCALE_LOG,
                                   NULL,
                                   &pcontrols->bottom_value,
                                   &pcontrols->bottom_units);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->bottom_noise,
                                    &pargs->bottom_noise);

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Slope")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    pcontrols->slope = gtk_adjustment_new(pargs->slope,
                                          0.0, 1000.0, 0.01, 10.0, 0);
    row = gwy_synth_attach_lateral(controls, row,
                                   pcontrols->slope, &pargs->slope,
                                   _("_Slope width:"), GWY_HSCALE_SQRT,
                                   NULL,
                                   &pcontrols->slope_value,
                                   &pcontrols->slope_units);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->slope_noise,
                                    &pargs->slope_noise);

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Height")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    row = gwy_synth_attach_height(controls, row,
                                  &pcontrols->height, &pargs->height,
                                  _("_Height:"),
                                  NULL, &pcontrols->height_units);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->height_noise,
                                    &pargs->height_noise);

    row = gwy_synth_attach_orientation(controls, row,
                                       &pcontrols->angle, &pargs->angle);

    row = gwy_synth_attach_deformation(controls, row,
                                       &pcontrols->sigma, &pargs->sigma,
                                       &pcontrols->tau, &pargs->tau,
                                       &pcontrols->tau_value,
                                       &pcontrols->tau_units);

    return pcontrols;
}

static void
dimensions_changed_ridges(PatSynthControls *controls)
{
    PatSynthControlsRidges *pcontrols = controls->pattern_controls;
    GwyDimensions *dims = controls->dims;

    gtk_label_set_markup(GTK_LABEL(pcontrols->top_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->bottom_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->slope_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->height_units), dims->zvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->tau_units), dims->xyvf->units);

    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->top));
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->bottom));
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->slope));
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->tau));
}

static void
make_pattern_ridges(const PatSynthArgs *args,
                    const GwyDimensionArgs *dimsargs,
                    RandGenSet *rngset,
                    GwyDataField *dfield)
{
    const PatSynthArgsRidges *pargs = args->pattern_args;
    GwyDataField *displacement_x;
    guint n, i, j, k, xres, yres;
    gdouble *abscissa, *height, *data, *dx_data;
    gdouble h, c, s, xoff, yoff, range;

    h = pargs->height * pow10(dimsargs->zpow10);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    range = (hypot(xres, yres) + pargs->sigma)/(pargs->top + 2*pargs->slope
                                                + pargs->bottom);
    n = GWY_ROUND(2.0*range + 16);
    abscissa = g_new(gdouble, 4*n);
    height = g_new(gdouble, n+1);

    abscissa[0] = -range - 8;
    accumulate(abscissa + 1, pargs->slope, pargs->slope_noise,
               rngset, RNG_SLOPE);
    accumulate(abscissa + 2, pargs->bottom, pargs->bottom_noise,
               rngset, RNG_BOTTOM);
    accumulate(abscissa + 3, pargs->slope, pargs->slope_noise,
               rngset, RNG_SLOPE);
    height[0] = 0.0;

    for (k = 1; k < n; k++) {
        accumulate(abscissa + 4*k, pargs->top, pargs->top_noise,
                   rngset, RNG_TOP);
        accumulate(abscissa + 4*k + 1, pargs->slope, pargs->slope_noise,
                   rngset, RNG_SLOPE);
        accumulate(abscissa + 4*k + 2, pargs->bottom, pargs->bottom_noise,
                   rngset, RNG_BOTTOM);
        accumulate(abscissa + 4*k + 3, pargs->slope, pargs->slope_noise,
                   rngset, RNG_SLOPE);
        generate(height + k, h, pargs->height_noise,
                 rngset, RNG_HEIGHT);
    }
    generate(height + n, h, pargs->height_noise,
             rngset, RNG_HEIGHT);

    displacement_x = make_displacement_map(xres, yres,
                                           pargs->sigma, pargs->tau,
                                           rngset->rng[RNG_DISPLAC_X]);
    dx_data = gwy_data_field_get_data(displacement_x);

    c = cos(pargs->angle);
    s = sin(pargs->angle);
    xoff = 0.5*((1.0 - c)*xres + s*yres);
    yoff = 0.5*(-s*yres + (1.0 - c)*yres);
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble v, x, xu, yu, d, q;
            guint kmod;

            xu = xoff + j*c - i*s;
            yu = yoff + j*s + i*c;
            x = xu + dx_data[i*xres + j];
            k = bisect_lower(abscissa, 4*n, x);
            kmod = k % 4;
            if (kmod == 0) {
                d = abscissa[k+1] - abscissa[k];
                q = G_LIKELY(d) ? (x - abscissa[k])/d : 0.5;
                v = (1.0 - q)*height[k/4];
            }
            else if (kmod == 1)
                v = 0.0;
            else if (kmod == 2) {
                d = abscissa[k+1] - abscissa[k];
                q = G_LIKELY(d) ? (x - abscissa[k])/d : 0.5;
                v = q*height[k/4 + 1];
            }
            else
                v = height[k/4 + 1];
            data[i*xres + j] += v;
        }
    }

    g_free(height);
    g_free(abscissa);
    g_object_unref(displacement_x);
}

static gpointer
load_args_ridges(GwyContainer *settings)
{
    PatSynthArgsRidges *pargs;
    GString *key;

    pargs = g_memdup(&pat_synth_defaults_ridges, sizeof(PatSynthArgsRidges));
    key = g_string_new(prefix);
    g_string_append(key, "/ridges/");
    gwy_synth_load_arg_double(settings, key, "top", 0.1, 1000.0,
                              &pargs->top);
    gwy_synth_load_arg_double(settings, key, "top_noise", 0.0, 1.0,
                              &pargs->top_noise);
    gwy_synth_load_arg_double(settings, key, "bottom", 0.1, 1000.0,
                              &pargs->bottom);
    gwy_synth_load_arg_double(settings, key, "bottom_noise", 0.0, 1.0,
                              &pargs->bottom_noise);
    gwy_synth_load_arg_double(settings, key, "slope", 0.0, 1000.0,
                              &pargs->slope);
    gwy_synth_load_arg_double(settings, key, "slope_noise", 0.0, 1.0,
                              &pargs->slope_noise);
    gwy_synth_load_arg_double(settings, key, "height", 0.0001, 10000.0,
                              &pargs->height);
    gwy_synth_load_arg_double(settings, key, "height_noise", 0.0, 1.0,
                              &pargs->height_noise);
    gwy_synth_load_arg_double(settings, key, "angle", -G_PI, G_PI,
                              &pargs->angle);
    gwy_synth_load_arg_double(settings, key, "sigma", 0.0, 100.0,
                              &pargs->sigma);
    gwy_synth_load_arg_double(settings, key, "tau", 0.1, 1000.0,
                              &pargs->tau);
    g_string_free(key, TRUE);

    return pargs;
}

static void
save_args_ridges(gpointer p,
                 GwyContainer *settings)
{
    PatSynthArgsRidges *pargs = p;
    GString *key;

    key = g_string_new(prefix);
    g_string_append(key, "/ridges/");
    gwy_synth_save_arg_double(settings, key, "top", pargs->top);
    gwy_synth_save_arg_double(settings, key, "top_noise", pargs->top_noise);
    gwy_synth_save_arg_double(settings, key, "bottom", pargs->bottom);
    gwy_synth_save_arg_double(settings, key, "bottom_noise", pargs->bottom_noise);
    gwy_synth_save_arg_double(settings, key, "slope", pargs->slope);
    gwy_synth_save_arg_double(settings, key, "slope_noise", pargs->slope_noise);
    gwy_synth_save_arg_double(settings, key, "height", pargs->height);
    gwy_synth_save_arg_double(settings, key, "height_noise", pargs->height_noise);
    gwy_synth_save_arg_double(settings, key, "angle", pargs->angle);
    gwy_synth_save_arg_double(settings, key, "sigma", pargs->sigma);
    gwy_synth_save_arg_double(settings, key, "tau", pargs->tau);
    g_string_free(key, TRUE);
}

static void
reset_ridges(gpointer p)
{
    PatSynthControlsRidges *pcontrols = p;
    PatSynthArgsRidges *pargs = pcontrols->args;

    *pargs = pat_synth_defaults_ridges;

    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->top), pargs->top);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->top_noise),
                             pargs->top_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->top), pargs->top);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->top_noise),
                             pargs->top_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->slope), pargs->slope);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->slope_noise),
                             pargs->slope_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->height), pargs->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->height_noise),
                             pargs->height_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->angle), pargs->angle);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->sigma), pargs->sigma);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->tau), pargs->tau);
}

/* Fill a data field with uncorrelated random numbers in a growin fashion to
 * preserve character of the noise even if the dimensions change */
static void
fill_displacement_map(GwyDataField *dfield,
                      GRand *rng,
                      gdouble q)
{
    guint xres, yres, n, i, j;
    gdouble *data;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    g_return_if_fail(xres == yres);
    data = gwy_data_field_get_data(dfield);
    n = xres;

    data[0] = q*(g_rand_double(rng) - 0.5);
    for (i = 1; i < n; i++) {
        for (j = 0; j <= i; j++)
            data[i*n + j] = q*(g_rand_double(rng) - 0.5);
        for (j = 0; j < i; j++)
            data[j*n + i] = q*(g_rand_double(rng) - 0.5);
    }
}

static GwyDataField*
make_displacement_map(guint xres, guint yres,
                      gdouble sigma, gdouble tau,
                      GRand *rng)
{
    GwyDataField *grid, *dfield;
    guint gn, n;
    gdouble q, r;

    n = MAX(xres, yres);
    q = 2.0*sigma*tau;
    if (!q)
        return gwy_data_field_new(xres, yres, 1.0, 1.0, TRUE);

    if (tau <= 1.0) {
        grid = gwy_data_field_new(n, n, 1.0, 1.0, FALSE);
        fill_displacement_map(grid, rng, q);
        gwy_data_field_filter_gaussian(grid, tau);
        if (xres == yres)
            return grid;

        dfield = gwy_data_field_area_extract(grid, 0, 0, xres, yres);
        g_object_unref(grid);
        return dfield;
    }

    gn = GWY_ROUND(1.0/tau*n);
    gn = MAX(gn, 2);
    r = (gdouble)gn/n;
    grid = gwy_data_field_new(gn, gn, 1.0, 1.0, FALSE);
    fill_displacement_map(grid, rng, q*r);
    gwy_data_field_filter_gaussian(grid, r*tau);
    dfield = gwy_data_field_new_resampled(grid, n, n, GWY_INTERPOLATION_KEY);
    g_object_unref(grid);

    if (xres != yres) {
        grid = dfield;
        dfield = gwy_data_field_area_extract(grid, 0, 0, xres, yres);
        g_object_unref(grid);
    }

    return dfield;
}

static RandGenSet*
rand_gen_set_new(guint n)
{
    RandGenSet *rngset;
    guint i;

    rngset = g_new(RandGenSet, 1);
    rngset->rng = g_new(GRand*, n);
    rngset->n = n;
    for (i = 0; i < n; i++)
        rngset->rng[i] = g_rand_new();

    return rngset;
}

static void
rand_gen_set_init(RandGenSet *rngset,
                  guint seed)
{
    guint i;

    for (i = 0; i < rngset->n; i++)
        g_rand_set_seed(rngset->rng[i], seed + i);
}

static void
rand_gen_set_free(RandGenSet *rngset)
{
    guint i;

    for (i = 0; i < rngset->n; i++)
        g_rand_free(rngset->rng[i]);
    g_free(rngset->rng);
    g_free(rngset);
}

static const gchar active_page_key[] = "/module/pat_synth/active_page";
static const gchar update_key[]      = "/module/pat_synth/update";
static const gchar randomize_key[]   = "/module/pat_synth/randomize";
static const gchar seed_key[]        = "/module/pat_synth/seed";
static const gchar type_key[]        = "/module/pat_synth/type";

static void
pat_synth_sanitize_args(PatSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = !!args->update;
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->type = MIN(args->type, PAT_SYNTH_NTYPES-1);
}

static void
pat_synth_load_args(GwyContainer *container,
                    PatSynthArgs *args,
                    GwyDimensionArgs *dimsargs)
{
    *args = pat_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    pat_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
pat_synth_save_args(GwyContainer *container,
                    const PatSynthArgs *args,
                    const GwyDimensionArgs *dimsargs)
{
    gwy_container_set_int32_by_name(container, active_page_key,
                                    args->active_page);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
    gwy_container_set_enum_by_name(container, type_key, args->type);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
