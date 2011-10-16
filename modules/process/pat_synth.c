/*
 *  @(#) $Id$
 *  Copyright (C) 2010,2011 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
    PAGE_PLACEMENT  = 2,
    PAGE_NPAGES
};

/* Each pattern has its own set of parameters but many are common so they get
 * the same symbolic name in PatSynthRng for simpliciy. */
typedef enum {
    RNG_FLAT       = 0,
    RNG_TOP        = 0,
    RNG_DISTANCE_X = 0,
    RNG_SLOPE      = 1,
    RNG_BOTTOM     = 2,
    RNG_SIZE       = 2,
    RNG_HEIGHT     = 3,
    RNG_DISPLAC_X  = 4,
    RNG_DISPLAC_Y  = 5,
    RNG_ROUNDNESS  = 6,
    RNG_DISTANCE_Y = 7,
    RNG_NRNGS
} PatSynthRng;

typedef enum {
    PAT_SYNTH_STEPS  = 0,
    PAT_SYNTH_RIDGES = 1,
    PAT_SYNTH_HOLES  = 2,
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
    GtkTable *table_parameters;
    GtkTable *table_placement;
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
DECLARE_PATTERN(holes);

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
    { PAT_SYNTH_HOLES,  N_("Holes"),  PATTERN_FUNCS(holes),  },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates surfaces representing simple patterns "
       "(steps, ridges, ...)."),
    "Yeti <yeti@gwyddion.net>",
    "1.2",
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
    gtk_box_pack_start(GTK_BOX(hbox), notebook, TRUE, TRUE, 4);
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
    controls.table_parameters = GTK_TABLE(table);
    gtk_table_set_row_spacings(controls.table_parameters, 2);
    gtk_table_set_col_spacings(controls.table_parameters, 6);
    gtk_table_set_row_spacing(controls.table_parameters, 0, 0);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.type = pattern_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Pattern:"), NULL,
                            GTK_OBJECT(controls.type), GWY_HSCALE_WIDGET);
    row++;
    g_object_set_data(G_OBJECT(table), "base-rows", GINT_TO_POINTER(row));

    table = gtk_table_new(1, 4, FALSE);
    controls.table_placement = GTK_TABLE(table);
    gtk_table_set_row_spacings(controls.table_placement, 2);
    gtk_table_set_col_spacings(controls.table_placement, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Placement")));
    g_object_set_data(G_OBJECT(table), "base-rows", GINT_TO_POINTER(1));

    controls.table = GTK_TABLE(controls.table_parameters);
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
    GtkTable *table;
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

    table = controls->table_parameters;
    baserows = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(table), "base-rows"));
    gwy_synth_shrink_table(table, baserows);

    table = controls->table_placement;
    baserows = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(table), "base-rows"));
    gwy_synth_shrink_table(table, baserows);

    args->pattern_args = pattern->load_args(gwy_app_settings_get());
    controls->pattern_controls = pattern->create_gui(controls);
    gtk_widget_show_all(GTK_WIDGET(controls->table_parameters));
    gtk_widget_show_all(GTK_WIDGET(controls->table_placement));

    update_values(controls);
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

/* Iterating through rectangles in a growing fashion from the origin to
 * preserve the top left conrer if it's randomly generated.  Field @k holds
 * the current index in the two-dimensional array. */
typedef struct {
    guint n;
    guint i, j, k;
    gboolean horizontal;
} GrowingIter;

static inline void
growing_iter_init(GrowingIter *giter, guint n)
{
    giter->n = n;
    giter->k = giter->j = giter-> i = 0;
    giter->horizontal = FALSE;
}

static inline gboolean
growing_iter_next(GrowingIter *giter)
{
    if (giter->horizontal) {
        if (++giter->j > giter->i) {
            giter->j = 0;
            giter->horizontal = FALSE;
        }
    }
    else {
        if (++giter->j >= giter->i) {
            if (++giter->i == giter->n)
                return FALSE;
            giter->j = 0;
            giter->horizontal = TRUE;
        }
    }

    giter->k = (giter->horizontal
                ? giter->i*giter->n + giter->j
                : giter->j*giter->n + giter->i);
    return TRUE;
}

/* Gauss-like distribution with range limited to [1-range, 1+range] */
static inline gdouble
rand_gen_set_mult(RandGenSet *rngset,
                  guint i,
                  gdouble range)
{
    GRand *rng;

    rng = rngset->rng[i];
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

    controls->table = controls->table_parameters;
    row = gwy_synth_extend_table(controls->table, 4+4+4);
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

    controls->table = controls->table_placement;
    row = gwy_synth_extend_table(controls->table, 2+3);

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
    gdouble h, c, s, xoff, range, margin, pitch;

    h = pargs->height * pow10(dimsargs->zpow10);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    pitch = pargs->flat + pargs->slope;
    margin = (hypot(xres, yres) - MAX(xres, yres) + 4*pargs->sigma + 2);
    range = (xres + yres + 8*pargs->sigma + 4)/pitch;
    n = GWY_ROUND(range + 1);
    abscissa = g_new(gdouble, 2*n);
    height = g_new(gdouble, n+1);

    abscissa[0] = -margin;
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
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble v, x, xu, d, q;
            guint kmod;

            xu = xoff + j*c - i*s;
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

    controls->table = controls->table_parameters;
    row = gwy_synth_extend_table(controls->table, 4+4+4+4);
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

    controls->table = controls->table_placement;
    row = gwy_synth_extend_table(controls->table, 2+3);

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
    gdouble h, c, s, xoff, range, margin, pitch;

    h = pargs->height * pow10(dimsargs->zpow10);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    pitch = pargs->top + 2*pargs->slope + pargs->bottom;
    margin = (hypot(xres, yres) - MAX(xres, yres) + 4*pargs->sigma + 2);
    range = (xres + yres + 8*pargs->sigma + 4)/pitch;
    n = GWY_ROUND(range + 1);
    abscissa = g_new(gdouble, 4*n);
    height = g_new(gdouble, n+1);

    abscissa[0] = -margin;
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
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble v, x, xu, d, q;
            guint kmod;

            xu = xoff + j*c - i*s;
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

typedef struct {
    gdouble distance;
    gdouble distance_noise;
    gdouble size;
    gdouble size_noise;
    gdouble slope;
    gdouble slope_noise;
    gdouble height;
    gdouble height_noise;
    gdouble roundness;
    gdouble roundness_noise;
    gdouble angle;
    gdouble sigma;
    gdouble tau;
} PatSynthArgsHoles;

typedef struct {
    PatSynthArgsHoles *args;
    GtkObject *distance;
    GtkWidget *distance_value;
    GtkWidget *distance_units;
    GtkObject *distance_noise;
    GtkObject *size;
    GtkWidget *size_value;
    GtkWidget *size_units;
    GtkObject *size_noise;
    GtkObject *slope;
    GtkWidget *slope_value;
    GtkWidget *slope_units;
    GtkObject *slope_noise;
    GtkObject *height;
    GtkWidget *height_units;
    GtkObject *height_noise;
    GtkObject *roundness;
    GtkWidget *roundness_value;
    GtkObject *roundness_noise;
    GtkObject *angle;
    GtkObject *sigma;
    GtkObject *tau;
    GtkWidget *tau_value;
    GtkWidget *tau_units;
} PatSynthControlsHoles;

static const PatSynthArgsHoles pat_synth_defaults_holes = {
    10.0, 0.0,
    10.0, 0.0,
    1.0, 0.0,
    1.0, 0.0,
    0.0, 0.0,
    0.0,
    0.0, 10.0,
};

static gpointer
create_gui_holes(PatSynthControls *controls)
{
    PatSynthControlsHoles *pcontrols;
    PatSynthArgsHoles *pargs;
    gint row;

    controls->table = controls->table_parameters;
    row = gwy_synth_extend_table(controls->table, 4+4+4+3+4);
    pcontrols = g_new0(PatSynthControlsHoles, 1);
    pargs = pcontrols->args = controls->args->pattern_args;

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Distance")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    pcontrols->distance = gtk_adjustment_new(pargs->distance,
                                             0.1, 1000.0, 0.01, 10.0, 0);
    row = gwy_synth_attach_lateral(controls, row,
                                   pcontrols->distance, &pargs->distance,
                                   _("_Distance:"), GWY_HSCALE_LOG,
                                   NULL,
                                   &pcontrols->distance_value,
                                   &pcontrols->distance_units);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->distance_noise,
                                    &pargs->distance_noise);

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Size")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    pcontrols->size = gtk_adjustment_new(pargs->size,
                                         1.0, 1000.0, 0.01, 10.0, 0);
    row = gwy_synth_attach_lateral(controls, row,
                                   pcontrols->size, &pargs->size,
                                   _("_Size:"), GWY_HSCALE_LOG,
                                   NULL,
                                   &pcontrols->size_value,
                                   &pcontrols->size_units);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->size_noise,
                                    &pargs->size_noise);

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

    row = gwy_synth_attach_roundness(controls, row,
                                     &pcontrols->roundness, &pargs->roundness);
    row = gwy_synth_attach_variance(controls, row,
                                    &pcontrols->roundness_noise,
                                    &pargs->roundness_noise);

    controls->table = controls->table_placement;
    row = gwy_synth_extend_table(controls->table, 2+3);

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
dimensions_changed_holes(PatSynthControls *controls)
{
    PatSynthControlsHoles *pcontrols = controls->pattern_controls;
    GwyDimensions *dims = controls->dims;

    gtk_label_set_markup(GTK_LABEL(pcontrols->distance_units),
                         dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->size_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->slope_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->height_units), dims->zvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->tau_units), dims->xyvf->units);

    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->distance));
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->size));
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->slope));
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(pcontrols->tau));
}

static inline gdouble
hole_radial_intersection(gdouble q, gdouble A, gdouble R)
{
    gdouble A1q = A*(1.0 - q);
    gdouble q21 = 1.0 + q*q;
    gdouble D = R*R*q21 - A1q*A1q;
    gdouble sqrtD = sqrt(MAX(D, 0.0));
    gdouble x = ((1.0 + q)*A + sqrtD)/q21;
    return hypot(x, q*x);
}

static gdouble
hole_shape(gdouble x, gdouble y, gdouble size, gdouble slope, gdouble roundness)
{
    gdouble rx, ry, r, rr, rsz;
    gdouble v = 0.0;

    if (roundness) {
        rsz = roundness*size;
        rx = fabs(x) - (size - rsz);
        ry = fabs(y) - (size - rsz);
        r = MAX(rx, ry);
        rr = MIN(rx, ry);
        if (r <= 0.0 || (r <= rsz && rr <= 0.0) || hypot(rx, ry) <= rsz)
            v = -1.0;
        else if (slope) {
            gdouble ss = size + slope;
            rsz = roundness*ss;
            rx = fabs(x) - (ss - rsz);
            ry = fabs(y) - (ss - rsz);
            r = MAX(rx, ry);
            rr = MIN(rx, ry);
            if (r <= 0.0 || (r <= rsz && rr <= 0.0)
                || hypot(rx, ry) <= rsz) {
                gdouble q = (rr + ss - rsz)/(r + ss - rsz);
                if (q <= 1.0 - roundness)
                    v = (r - rsz)/slope;
                else {
                    r = hole_radial_intersection(q, ss - rsz, rsz);
                    rr = hole_radial_intersection(q, size - roundness*size,
                                                  roundness*size);
                    v = (hypot(x, y) - r)/(r - rr);
                }
            }
        }
    }
    else {
        rx = fabs(x) - size;
        ry = fabs(y) - size;
        r = MAX(rx, ry);
        if (r <= 0.0)
            v = -1.0;
        else if (r < slope)
            v = (r - slope)/slope;
    }

    return v;
}

static void
make_pattern_holes(const PatSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   RandGenSet *rngset,
                   GwyDataField *dfield)
{
    enum {
        HOLE_OFFSET_X,
        HOLE_OFFSET_Y,
        HOLE_SIZE,
        HOLE_SLOPE,
        HOLE_HEIGHT,
        HOLE_ROUNDNESS,
        HOLE_NPARAMS
    };

    const PatSynthArgsHoles *pargs = args->pattern_args;
    GwyDataField *displacement_x, *displacement_y;
    guint n, i, j, xres, yres;
    gdouble *params, *data, *dx_data, *dy_data;
    gdouble h, c, s, xoff, yoff, pitch, range, margin;
    GrowingIter giter;

    h = pargs->height * pow10(dimsargs->zpow10);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    pitch = pargs->size + 2*pargs->slope + pargs->distance;
    margin = (hypot(xres, yres) - MAX(xres, yres) + 4*pargs->sigma + 2);
    range = (xres + yres + 8*pargs->sigma + 4)/pitch;
    n = GWY_ROUND(range + 1);
    params = g_new(gdouble, HOLE_NPARAMS*n*n);

    growing_iter_init(&giter, n);
    do {
        gdouble *p = params + giter.k*HOLE_NPARAMS;
        generate(p + HOLE_OFFSET_X, pargs->distance, pargs->distance_noise,
                 rngset, RNG_DISTANCE_X);
        generate(p + HOLE_OFFSET_Y, pargs->distance, pargs->distance_noise,
                 rngset, RNG_DISTANCE_Y);
        generate(p + HOLE_SIZE, pargs->size, pargs->size_noise,
                 rngset, RNG_SIZE);
        generate(p + HOLE_HEIGHT, h, pargs->height_noise,
                 rngset, RNG_HEIGHT);
        generate(p + HOLE_SLOPE, pargs->slope, pargs->slope_noise,
                 rngset, RNG_SLOPE);
        generate(p + HOLE_ROUNDNESS, pargs->roundness, pargs->roundness_noise,
                 rngset, RNG_ROUNDNESS);
    } while (growing_iter_next(&giter));

    displacement_x = make_displacement_map(xres, yres,
                                           pargs->sigma, pargs->tau,
                                           rngset->rng[RNG_DISPLAC_X]);
    displacement_y = make_displacement_map(xres, yres,
                                           pargs->sigma, pargs->tau,
                                           rngset->rng[RNG_DISPLAC_Y]);
    dx_data = gwy_data_field_get_data(displacement_x);
    dy_data = gwy_data_field_get_data(displacement_y);

    c = cos(pargs->angle);
    s = sin(pargs->angle);
    xoff = 0.5*((1.0 - c)*xres + s*yres);
    yoff = 0.5*(-s*yres + (1.0 - c)*yres);
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble v[4], x, y, xu, yu, xx, yy;
            gint kx, ky, kxx, kyy;
            gdouble *p;

            xu = xoff + j*c - i*s;
            yu = yoff + j*s + i*c;
            x = xu + dx_data[i*xres + j] + margin;
            y = yu + dy_data[i*xres + j] + margin;
            kx = (gint)floor(x/pitch);
            ky = (gint)floor(y/pitch);
            x -= pitch*(kx + 0.5);
            y -= pitch*(ky + 0.5);
            /* Better repeated boundary features than buffer overflow... */
            kx = CLAMP(kx, 0, n-1);
            ky = CLAMP(ky, 0, n-1);
            /* The cell itself */
            p = params + (n*ky + kx)*HOLE_NPARAMS;
            xx = x - 0.5*p[HOLE_OFFSET_X];
            yy = y - 0.5*p[HOLE_OFFSET_Y];
            v[0] = hole_shape(xx, yy,
                              0.5*p[HOLE_SIZE], p[HOLE_SLOPE],
                              p[HOLE_ROUNDNESS]) * p[HOLE_HEIGHT];
            /* Left/right neighbour. */
            kxx = kx + (x >= 0 ? 1 : -1);
            kxx = CLAMP(kxx, 0, n-1);
            p = params + (n*ky + kxx)*HOLE_NPARAMS;
            xx = x - 0.5*p[HOLE_OFFSET_X] - (x >= 0 ? pitch : -pitch);
            yy = y - 0.5*p[HOLE_OFFSET_Y];
            v[1] = hole_shape(xx, yy,
                              0.5*p[HOLE_SIZE], p[HOLE_SLOPE],
                              p[HOLE_ROUNDNESS]) * p[HOLE_HEIGHT];
            /* Upper/lower neighbour. */
            kyy = ky + (y >= 0 ? 1 : -1);
            kyy = CLAMP(kyy, 0, n-1);
            p = params + (n*kyy + kx)*HOLE_NPARAMS;
            xx = x - 0.5*p[HOLE_OFFSET_X];
            yy = y - 0.5*p[HOLE_OFFSET_Y] - (y >= 0 ? pitch : -pitch);
            v[2] = hole_shape(xx, yy,
                              0.5*p[HOLE_SIZE], p[HOLE_SLOPE],
                              p[HOLE_ROUNDNESS]) * p[HOLE_HEIGHT];
            /* Diagonal neighbour */
            p = params + (n*kyy + kxx)*HOLE_NPARAMS;
            xx = x - 0.5*p[HOLE_OFFSET_X] - (x >= 0 ? pitch : -pitch);
            yy = y - 0.5*p[HOLE_OFFSET_Y] - (y >= 0 ? pitch : -pitch);
            v[3] = hole_shape(xx, yy,
                              0.5*p[HOLE_SIZE], p[HOLE_SLOPE],
                              p[HOLE_ROUNDNESS]) * p[HOLE_HEIGHT];

            data[i*xres + j] += MIN(MIN(v[0], v[1]), MIN(v[2], v[3]));
        }
    }

    g_object_unref(displacement_y);
    g_object_unref(displacement_x);
    g_free(params);
}

static gpointer
load_args_holes(GwyContainer *settings)
{
    PatSynthArgsHoles *pargs;
    GString *key;

    pargs = g_memdup(&pat_synth_defaults_holes, sizeof(PatSynthArgsHoles));
    key = g_string_new(prefix);
    g_string_append(key, "/holes/");
    gwy_synth_load_arg_double(settings, key, "distance", 0.1, 1000.0,
                              &pargs->distance);
    gwy_synth_load_arg_double(settings, key, "distance_noise", 0.0, 1.0,
                              &pargs->distance_noise);
    gwy_synth_load_arg_double(settings, key, "size", 1.0, 1000.0,
                              &pargs->size);
    gwy_synth_load_arg_double(settings, key, "size_noise", 0.0, 1.0,
                              &pargs->size_noise);
    gwy_synth_load_arg_double(settings, key, "slope", 0.0, 1000.0,
                              &pargs->slope);
    gwy_synth_load_arg_double(settings, key, "slope_noise", 0.0, 1.0,
                              &pargs->slope_noise);
    gwy_synth_load_arg_double(settings, key, "height", 0.0001, 10000.0,
                              &pargs->height);
    gwy_synth_load_arg_double(settings, key, "height_noise", 0.0, 1.0,
                              &pargs->height_noise);
    gwy_synth_load_arg_double(settings, key, "roundness", 0.0, 1.0,
                              &pargs->roundness);
    gwy_synth_load_arg_double(settings, key, "roundness_noise", 0.0, 1.0,
                              &pargs->roundness_noise);
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
save_args_holes(gpointer p,
                GwyContainer *settings)
{
    PatSynthArgsHoles *pargs = p;
    GString *key;

    key = g_string_new(prefix);
    g_string_append(key, "/holes/");
    gwy_synth_save_arg_double(settings, key, "distance", pargs->distance);
    gwy_synth_save_arg_double(settings, key, "distance_noise",
                              pargs->distance_noise);
    gwy_synth_save_arg_double(settings, key, "size", pargs->size);
    gwy_synth_save_arg_double(settings, key, "size_noise", pargs->size_noise);
    gwy_synth_save_arg_double(settings, key, "slope", pargs->slope);
    gwy_synth_save_arg_double(settings, key, "slope_noise", pargs->slope_noise);
    gwy_synth_save_arg_double(settings, key, "height", pargs->height);
    gwy_synth_save_arg_double(settings, key, "height_noise",
                              pargs->height_noise);
    gwy_synth_save_arg_double(settings, key, "roundness", pargs->roundness);
    gwy_synth_save_arg_double(settings, key, "roundness_noise",
                              pargs->roundness_noise);
    gwy_synth_save_arg_double(settings, key, "angle", pargs->angle);
    gwy_synth_save_arg_double(settings, key, "sigma", pargs->sigma);
    gwy_synth_save_arg_double(settings, key, "tau", pargs->tau);
    g_string_free(key, TRUE);
}

static void
reset_holes(gpointer p)
{
    PatSynthControlsHoles *pcontrols = p;
    PatSynthArgsHoles *pargs = pcontrols->args;

    *pargs = pat_synth_defaults_holes;

    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->distance),
                             pargs->distance);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->distance_noise),
                             pargs->distance_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->size), pargs->size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->size_noise),
                             pargs->size_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->slope), pargs->slope);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->slope_noise),
                             pargs->slope_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->height), pargs->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->height_noise),
                             pargs->height_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->roundness),
                             pargs->roundness);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->roundness_noise),
                             pargs->roundness_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->angle), pargs->angle);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->sigma), pargs->sigma);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->tau), pargs->tau);
}

/* Fill a data field with uncorrelated random numbers in a growing fashion to
 * preserve the character of the noise even if the dimensions change */
static void
fill_displacement_map(GwyDataField *dfield,
                      GRand *rng,
                      gdouble q)
{
    GrowingIter giter;
    guint xres, yres;
    gdouble *data;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    g_return_if_fail(xres == yres);
    data = gwy_data_field_get_data(dfield);
    growing_iter_init(&giter, xres);

    do {
        data[giter.k] = q*(g_rand_double(rng) - 0.5);
    } while (growing_iter_next(&giter));
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
