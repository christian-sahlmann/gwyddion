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
    static void save_args_##name(gpointer pargs, GwyContainer *settings)

#define PATTERN_FUNCS(name) \
    &create_gui_##name, &dimensions_changed_##name, \
    &make_pattern_##name, \
    &load_args_##name, &save_args_##name

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
    RNG_FLAT      = 0,
    RNG_SLOPE     = 1,
    RNG_HEIGHT    = 2,
    RNG_DISPLAC_X = 3,
    RNG_NRNGS
} ObjSynthRng;

typedef enum {
    PAT_SYNTH_STEPS   = 0,
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
    GtkWidget *type;
    GtkTable *table;
    GwyContainer *mydata;
    GwyDataField *surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
    gulong sid;
};

static gboolean    module_register      (void);
static void        pat_synth            (GwyContainer *data,
                                         GwyRunType run);
static void        run_noninteractive   (PatSynthArgs *args,
                                         const GwyDimensionArgs *dimsargs,
                                         RandGenSet *rngset,
                                         GwyContainer *data,
                                         GwyDataField *dfield,
                                         gint oldid,
                                         GQuark quark);
static gboolean    pat_synth_dialog     (PatSynthArgs *args,
                                         GwyDimensionArgs *dimsargs,
                                         RandGenSet *rngset,
                                         GwyContainer *data,
                                         GwyDataField *dfield,
                                         gint id);
static GtkWidget*  pattern_selector_new (PatSynthControls *controls);
static void        update_controls      (PatSynthControls *controls,
                                         PatSynthArgs *args);
static GtkWidget*  random_seed_new      (GtkAdjustment *adj);
static GtkWidget*  randomize_new        (gboolean *randomize);
static GtkWidget*  instant_updates_new  (GtkWidget **update,
                                         GtkWidget **instant,
                                         gboolean *state);
static void        page_switched        (PatSynthControls *controls,
                                         GtkNotebookPage *page,
                                         gint pagenum);
static void        seed_changed         (PatSynthControls *controls,
                                         GtkAdjustment *adj);
static void        randomize_seed       (GtkAdjustment *adj);
static void        pattern_type_selected(GtkComboBox *combo,
                                         PatSynthControls *controls);
static void        update_value_label   (GtkLabel *label,
                                         const GwySIValueFormat *vf,
                                         gdouble value);
static void        pat_synth_invalidate (PatSynthControls *controls);
static gboolean    preview_gsource      (gpointer user_data);
static void        preview              (PatSynthControls *controls);
static void        pat_synth_do         (const PatSynthArgs *args,
                                         const GwyDimensionArgs *dimsargs,
                                         RandGenSet *rngset,
                                         GwyDataField *dfield);
static GwyDataField* make_displacement_map(guint xres, guint yres,
                                           gdouble sigma, gdouble tau,
                                           GRand *rng);
static RandGenSet* rand_gen_set_new     (guint n);
static void        rand_gen_set_init    (RandGenSet *rngset,
                                         guint seed);
static void        rand_gen_set_free    (RandGenSet *rngset);
static void        pat_synth_load_args  (GwyContainer *container,
                                         PatSynthArgs *args,
                                         GwyDimensionArgs *dimsargs);
static void        pat_synth_save_args  (GwyContainer *container,
                                         const PatSynthArgs *args,
                                         const GwyDimensionArgs *dimsargs);

DECLARE_PATTERN(steps);

static const gchar prefix[] = "/module/pat_synth";

static const PatSynthArgs pat_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    PAT_SYNTH_STEPS,
    NULL,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static const PatSynthPattern patterns[] = {
    { PAT_SYNTH_STEPS, N_("Steps"), PATTERN_FUNCS(steps), },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates surfaces representing simple patterns "
       "(steps, trenches, ...)."),
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
                                FALSE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    if (data)
        gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                0);
    if (dfield_template) {
        controls.surface = gwy_data_field_new_resampled(dfield_template,
                                                        PREVIEW_SIZE,
                                                        PREVIEW_SIZE,
                                                        GWY_INTERPOLATION_KEY);
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
                       instant_updates_new(&controls.update_now,
                                           &controls.update, &args->update),
                       FALSE, FALSE, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(pat_synth_invalidate), &controls);
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

    pattern_type_selected(GTK_COMBO_BOX(controls.type), &controls);

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
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
            args->type = pat_synth_defaults.type;
            /* TODO: Reset the pattern args! */
            /* TODO: Update the pattern controls! */
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
page_switched(PatSynthControls *controls,
              G_GNUC_UNUSED GtkNotebookPage *page,
              gint pagenum)
{
    if (controls->in_init)
        return;

    controls->args->active_page = pagenum;

    if (pagenum == PAGE_GENERATOR) {
        GwyDimensions *dims = controls->dims;

        controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
        controls->pattern->dims_changed(controls);
    }
}

static void
seed_changed(PatSynthControls *controls,
             GtkAdjustment *adj)
{
    controls->args->seed = gwy_adjustment_get_int(adj);
    pat_synth_invalidate(controls);
}

static void
randomize_seed(GtkAdjustment *adj)
{
    /* Use the GLib's global PRNG for seeding */
    gtk_adjustment_set_value(adj, g_random_int() & 0x7fffffff);
}

static void
pattern_type_selected(GtkComboBox *combo,
                      PatSynthControls *controls)
{
    const PatSynthPattern *pattern;
    PatSynthArgs *args = controls->args;
    guint cols;

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

    /* Brutally get rid of the unwanted controls. */
    g_object_get(controls->table, "n-columns", &cols, NULL);
    gtk_table_resize(controls->table, 1, cols);

    args->pattern_args = pattern->load_args(gwy_app_settings_get());
    controls->pattern_controls = pattern->create_gui(controls);

    pat_synth_invalidate(controls);
}

static void
double_changed(PatSynthControls *controls,
               GtkAdjustment *adj)
{
    GObject *object = G_OBJECT(adj);
    gdouble *target = g_object_get_data(object, "target");
    UpdateValueFunc update_value = g_object_get_data(object, "update-value");

    g_return_if_fail(target);
    *target = gtk_adjustment_get_value(adj);
    if (update_value)
        update_value(controls);
    pat_synth_invalidate(controls);
}

static void
angle_changed(PatSynthControls *controls,
              GtkAdjustment *adj)
{
    GObject *object = G_OBJECT(adj);
    gdouble *target = g_object_get_data(object, "target");

    g_return_if_fail(target);
    *target = G_PI/180.0 * gtk_adjustment_get_value(adj);
    pat_synth_invalidate(controls);
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

static gint
extend_table(GtkTable *table,
             guint by_rows)
{
    guint rows, cols;

    g_object_get(table, "n-rows", &rows, "n-columns", &cols, NULL);
    gtk_table_resize(table, rows + by_rows, cols);

    return rows;
}

static void
update_lateral(PatSynthControls *controls,
               GtkAdjustment *adj)
{
    GtkWidget *label = g_object_get_data(G_OBJECT(adj), "value-label");

    update_value_label(GTK_LABEL(label),
                       controls->dims->xyvf,
                       gtk_adjustment_get_value(adj) * controls->pxsize);
}

static gint
attach_lateral(PatSynthControls *controls,
               gint row,
               GtkObject *adj,
               gdouble *target,
               const gchar *name,
               GwyHScaleStyle hscale_style,
               GtkWidget **pspin,
               GtkWidget **pvalue,
               GtkWidget **punits)
{
    GtkWidget *spin;

    g_object_set_data(G_OBJECT(adj), "target", target);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, name, "px", adj, hscale_style);
    if (pspin)
        *pspin = spin;
    row++;

    *pvalue = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(*pvalue), 1.0, 0.5);
    gtk_table_attach(controls->table, *pvalue,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_object_set_data(G_OBJECT(adj), "value-label", *pvalue);

    *punits = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(*punits), 0.0, 0.5);
    gtk_table_attach(controls->table, *punits,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    g_signal_connect_swapped(adj, "value-changed",
                             G_CALLBACK(double_changed), controls);
    g_signal_connect_swapped(adj, "value-changed",
                             G_CALLBACK(update_lateral), controls);

    return row;
}

static gint
attach_height(PatSynthControls *controls,
              gint row,
              GtkObject **adj,
              gdouble *target,
              const gchar *name,
              GtkWidget **pspin,
              GtkWidget **punits)
{
    GtkWidget *spin;

    *adj = gtk_adjustment_new(*target, 0.0001, 10000.0, 0.1, 10.0, 0);
    g_object_set_data(G_OBJECT(*adj), "target", target);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, name, "", *adj, GWY_HSCALE_LOG);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 4);
    if (pspin)
        *pspin = spin;

    *punits = gwy_table_hscale_get_units(*adj);
    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(double_changed), controls);
    row++;

    return row;
}

static gint
attach_orientation(PatSynthControls *controls,
                   gint row,
                   GtkObject **adj,
                   gdouble *target)
{
    GtkWidget *spin;

    gtk_table_set_row_spacing(controls->table, row-1, 12);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Orientation")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    *adj = gtk_adjustment_new(*target * 180.0/G_PI,
                              -180.0, 180.0, 1.0, 10.0, 0);
    g_object_set_data(G_OBJECT(*adj), "target", target);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, _("Orien_tation:"), "deg", *adj,
                                   GWY_HSCALE_DEFAULT);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(angle_changed), controls);

    row++;

    return row;
}

static gint
attach_variance(PatSynthControls *controls,
                gint row,
                GtkObject **adj,
                gdouble *target)
{
    GtkWidget *spin;

    *adj = gtk_adjustment_new(*target, 0.0, 1.0, 0.01, 0.1, 0);
    g_object_set_data(G_OBJECT(*adj), "target", target);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, _("Variance:"), NULL, *adj,
                                   GWY_HSCALE_SQRT);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    row++;

    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(double_changed), controls);

    return row;
}

static gint
attach_deformation(PatSynthControls *controls,
                   gint row,
                   GtkObject **adj_sigma,
                   gdouble *target_sigma,
                   GtkObject **adj_tau,
                   gdouble *target_tau,
                   GtkWidget **pvalue_tau,
                   GtkWidget **punits_tau)
{
    GtkWidget *spin;

    gtk_table_set_row_spacing(controls->table, row-1, 12);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Deformation")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    *adj_sigma = gtk_adjustment_new(*target_sigma, 0.0, 100.0, 0.01, 1.0, 0);
    g_object_set_data(G_OBJECT(*adj_sigma), "target", target_sigma);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, _("_Amplitude:"), NULL, *adj_sigma,
                                   GWY_HSCALE_LOG);
    row++;

    *adj_tau = gtk_adjustment_new(*target_tau, 0.0, 100.0, 0.01, 1.0, 0);
    g_object_set_data(G_OBJECT(*adj_tau), "target", target_tau);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, _("_Lateral scale:"), "px", *adj_tau,
                                   GWY_HSCALE_LOG);
    row++;

    *pvalue_tau = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(*pvalue_tau), 1.0, 0.5);
    gtk_table_attach(controls->table, *pvalue_tau,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_object_set_data(G_OBJECT(*adj_tau), "value-label", *pvalue_tau);

    *punits_tau = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(*punits_tau), 0.0, 0.5);
    gtk_table_attach(controls->table, *punits_tau,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    g_signal_connect_swapped(*adj_sigma, "value-changed",
                             G_CALLBACK(double_changed), controls);
    g_signal_connect_swapped(*adj_tau, "value-changed",
                             G_CALLBACK(double_changed), controls);
    g_signal_connect_swapped(*adj_tau, "value-changed",
                             G_CALLBACK(update_lateral), controls);

    return row;
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

static gpointer
create_gui_steps(PatSynthControls *controls)
{
    PatSynthControlsSteps *pcontrols;
    PatSynthArgsSteps *pargs;
    gint row;

    row = extend_table(controls->table, 4+4+3+2+4);
    pcontrols = g_new0(PatSynthControlsSteps, 1);
    pargs = pcontrols->args = controls->args->pattern_args;

    gtk_table_set_row_spacing(controls->table, row-1, 12);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Flat")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    pcontrols->flat = gtk_adjustment_new(pargs->flat,
                                         1.0, 1000.0, 0.01, 10.0, 0);
    row = attach_lateral(controls, row, pcontrols->flat, &pargs->flat,
                         _("_Flat length:"), GWY_HSCALE_LOG,
                         NULL,
                         &pcontrols->flat_value,
                         &pcontrols->flat_units);
    row = attach_variance(controls, row,
                          &pcontrols->flat_noise, &pargs->flat_noise);

    gtk_table_set_row_spacing(controls->table, row-1, 12);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Slope")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    pcontrols->slope = gtk_adjustment_new(pargs->slope,
                                          0.0, 1000.0, 0.01, 10.0, 0);
    row = attach_lateral(controls, row, pcontrols->slope, &pargs->slope,
                         _("_Slope length:"), GWY_HSCALE_SQRT,
                         NULL,
                         &pcontrols->slope_value,
                         &pcontrols->slope_units);
    row = attach_variance(controls, row,
                          &pcontrols->slope_noise, &pargs->slope_noise);

    gtk_table_set_row_spacing(controls->table, row-1, 12);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Height")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    row = attach_height(controls, row,
                        &pcontrols->height, &pargs->height,
                        _("_Height:"), NULL, &pcontrols->height_units);
    row = attach_variance(controls, row,
                          &pcontrols->height_noise, &pargs->height_noise);

    row = attach_orientation(controls, row,
                             &pcontrols->angle, &pargs->angle);

    row = attach_deformation(controls, row,
                             &pcontrols->sigma, &pargs->sigma,
                             &pcontrols->tau, &pargs->tau,
                             &pcontrols->tau_value, &pcontrols->tau_units);

    return pcontrols;
}

static void
dimensions_changed_steps(PatSynthControls *controls)
{
    PatSynthControlsSteps *pcontrols = controls->pattern_controls;
    GwyDimensions *dims = controls->dims;

    gtk_label_set_markup(GTK_LABEL(pcontrols->flat_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->slope_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->tau_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->height_units), dims->zvf->units);

    update_lateral(controls, GTK_ADJUSTMENT(pcontrols->flat));
    update_lateral(controls, GTK_ADJUSTMENT(pcontrols->slope));
    update_lateral(controls, GTK_ADJUSTMENT(pcontrols->tau));
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
    gdouble h, c, s, x, xu, yu, xoff, yoff;

    h = pargs->height * pow10(dimsargs->zpow10);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    n = GWY_ROUND(1.2*hypot(xres, yres)/(pargs->flat + pargs->slope) + 15);
    abscissa = g_new(gdouble, 2*n);
    height = g_new(gdouble, n);

    abscissa[0] = -0.6*hypot(xres, yres) - 7;
    abscissa[1] = pargs->flat;
    if (pargs->flat && pargs->flat_noise)
        abscissa[1] *= rand_gen_set_mult(rngset, RNG_FLAT,
                                         pargs->flat_noise);
    abscissa[1] += abscissa[0];
    height[0] = 0.0;

    for (k = 1; k < n; k++) {
        abscissa[2*k] = pargs->slope;
        if (pargs->slope && pargs->slope_noise)
            abscissa[2*k] *= rand_gen_set_mult(rngset, RNG_SLOPE,
                                               pargs->slope_noise);
        abscissa[2*k] += abscissa[2*k - 1];

        abscissa[2*k + 1] = pargs->flat;
        if (pargs->flat && pargs->flat_noise)
            abscissa[2*k + 1] *= rand_gen_set_mult(rngset, RNG_FLAT,
                                                   pargs->flat_noise);
        abscissa[2*k + 1] += abscissa[2*k];

        height[k] = h;
        if (pargs->height && pargs->height_noise)
            height[k] *= rand_gen_set_mult(rngset, RNG_HEIGHT,
                                           pargs->height_noise);
        height[k] += height[k-1];
    }

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
            gdouble v;

            xu = xoff + j*c - i*s;
            yu = yoff + j*s + i*c;
            x = xu + dx_data[i*xres + j];
            k = bisect_lower(abscissa, 2*n, x);
            if (k % 2 == 0)
                v = height[k/2];
            else {
                if (G_UNLIKELY(k == 2*n - 1))
                    v = height[k/2];
                else {
                    gdouble d = abscissa[k+1] - abscissa[k];
                    gdouble q = G_LIKELY(d) ? (x - abscissa[k])/d : 0.5;

                    v = (1.0 - q)*height[k/2] + q*height[k/2 + 1];
                }
            }
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
    guint len;

    key = g_string_new(prefix);
    g_string_append(key, "/steps/");
    len = key->len;

    pargs = g_new0(PatSynthArgsSteps, 1);

    g_string_append(g_string_truncate(key, len), "flat");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->flat);
    pargs->flat = CLAMP(pargs->flat, 1.0, 1000.0);

    g_string_append(g_string_truncate(key, len), "flat_noise");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->flat_noise);
    pargs->flat_noise = CLAMP(pargs->flat_noise, 0.0, 1.0);

    g_string_append(g_string_truncate(key, len), "slope");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->slope);
    pargs->slope = CLAMP(pargs->slope, 0.0, 1000.0);

    g_string_append(g_string_truncate(key, len), "slope_noise");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->slope_noise);
    pargs->slope_noise = CLAMP(pargs->slope_noise, 0.0, 1.0);

    g_string_append(g_string_truncate(key, len), "height");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->height);
    pargs->height = CLAMP(pargs->height, 0.0001, 10000.0);

    g_string_append(g_string_truncate(key, len), "height_noise");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->height_noise);
    pargs->height_noise = CLAMP(pargs->height_noise, 0.0, 1.0);

    g_string_append(g_string_truncate(key, len), "angle");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->angle);
    pargs->angle = CLAMP(pargs->angle, -G_PI, G_PI);

    g_string_append(g_string_truncate(key, len), "sigma");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->sigma);
    pargs->sigma = CLAMP(pargs->sigma, 0.0, 100.0);

    g_string_append(g_string_truncate(key, len), "tau");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->tau);
    pargs->tau = CLAMP(pargs->tau, 0.1, 1000.0);

    g_string_free(key, TRUE);

    return pargs;
}

static void
save_args_steps(gpointer p,
                GwyContainer *settings)
{
    PatSynthArgsSteps *pargs = p;
    GString *key;
    guint len;

    key = g_string_new(prefix);
    g_string_append(key, "/steps/");
    len = key->len;

    g_string_append(g_string_truncate(key, len), "flat");
    gwy_container_set_double_by_name(settings, key->str, pargs->flat);

    g_string_append(g_string_truncate(key, len), "flat_noise");
    gwy_container_set_double_by_name(settings, key->str, pargs->flat_noise);

    g_string_append(g_string_truncate(key, len), "slope");
    gwy_container_set_double_by_name(settings, key->str, pargs->slope);

    g_string_append(g_string_truncate(key, len), "slope_noise");
    gwy_container_set_double_by_name(settings, key->str, pargs->slope_noise);

    g_string_append(g_string_truncate(key, len), "height");
    gwy_container_set_double_by_name(settings, key->str, pargs->height);

    g_string_append(g_string_truncate(key, len), "height_noise");
    gwy_container_set_double_by_name(settings, key->str, pargs->height_noise);

    g_string_append(g_string_truncate(key, len), "angle");
    gwy_container_set_double_by_name(settings, key->str, pargs->angle);

    g_string_append(g_string_truncate(key, len), "sigma");
    gwy_container_set_double_by_name(settings, key->str, pargs->sigma);

    g_string_append(g_string_truncate(key, len), "tau");
    gwy_container_set_double_by_name(settings, key->str, pargs->tau);

    g_string_free(key, TRUE);
}

static GwyDataField*
make_displacement_map(guint xres, guint yres,
                      gdouble sigma, gdouble tau,
                      GRand *rng)
{
    GwyDataField *grid, *dfield;
    gint i, j;
    gdouble q;
    gdouble *data;
    gint n = 401;

    if (!sigma)
        return gwy_data_field_new(xres, yres, 1.0, 1.0, TRUE);

    grid = gwy_data_field_new(n, n, 1.0, 1.0, FALSE);
    data = gwy_data_field_get_data(grid);
    q = 2.0*sigma*tau;

    data[n/2*(n + 1)] = q*(g_rand_double(rng) - 0.5);
    for (i = 1; i <= n/2; i++) {
        for (j = 0; j < 2*i; j++) {
            data[(n/2 - i + j)*n + n/2 - i] = q*(g_rand_double(rng) - 0.5);
            data[(n/2 + i)*n + n/2 - i + j] = q*(g_rand_double(rng) - 0.5);
            data[(n/2 + i - j)*n + n/2 + i] = q*(g_rand_double(rng) - 0.5);
            data[(n/2 - i)*n + n/2 + i - j] = q*(g_rand_double(rng) - 0.5);
        }
    }
    gwy_data_field_filter_gaussian(grid, tau);

    dfield = gwy_data_field_new_resampled(grid, xres, yres,
                                          GWY_INTERPOLATION_KEY);
    g_object_unref(grid);

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
        g_rand_set_seed(rngset->rng[i], seed);
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
