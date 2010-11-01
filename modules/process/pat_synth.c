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
    static void destroy_gui_##name(gpointer pcontrols); \
    static void dimensions_changed_##name(PatSynthControls *controls); \
    static void make_pattern_##name(PatSynthArgs *args, \
                                    GwyDimensionArgs *dimsargs, \
                                    gpointer pargs, \
                                    GwyDataField *dfield); \
    static gpointer load_args_##name(GwyContainer *settings); \
    static void save_args_##name(gpointer pargs, GwyContainer *settings)

#define PATTERN_FUNCS(name) \
    &create_gui_##name, &destroy_gui_##name, &dimensions_changed_##name, \
    &make_pattern_##name, &load_args_##name, &save_args_##name

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
    PAT_SYNTH_STEPS   = 0,
    PAT_SYNTH_NTYPES
} PatSynthType;

typedef struct _PatSynthArgs PatSynthArgs;
typedef struct _PatSynthControls PatSynthControls;

typedef gpointer (*CreateGUIFunc)(PatSynthControls *controls);
typedef void (*DestroyGUIFunc)(gpointer pcontrols);
typedef void (*DimensionsChangedFunc)(PatSynthControls *controls);
typedef void (*MakePatternFunc)(PatSynthArgs *args,
                                GwyDimensionArgs *dimsargs,
                                gpointer pargs,
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
    DestroyGUIFunc destroy_gui;
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
};

struct _PatSynthControls {
    PatSynthArgs *args;
    GwyDimensions *dims;
    /* They have different types, known only to the pattern. */
    gpointer pattern_args;
    gpointer pattern_controls;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkWidget *type;
    GtkWidget *table;
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
                                         GwyContainer *data,
                                         GwyDataField *dfield,
                                         gint oldid,
                                         GQuark quark);
static gboolean    pat_synth_dialog     (PatSynthArgs *args,
                                         GwyDimensionArgs *dimsargs,
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
                                         GwyDataField *dfield);
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
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & PAT_SYNTH_RUN_MODES);
    pat_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || pat_synth_dialog(&args, &dimsargs, data, dfield, id))
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);

    if (run == GWY_RUN_INTERACTIVE)
        pat_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(PatSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint oldid,
                   GQuark quark)
{
    GwySIUnit *siunit;
    gboolean replace = dimsargs->replace && dfield;
    gboolean add = dimsargs->add && dfield;
    gdouble mag;
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
            mag = pow10(dimsargs->xypow10) * dimsargs->measure;
            dfield = gwy_data_field_new(dimsargs->xres, dimsargs->yres,
                                        mag*dimsargs->xres, mag*dimsargs->yres,
                                        FALSE);

            siunit = gwy_data_field_get_si_unit_xy(dfield);
            gwy_si_unit_set_from_string(siunit, dimsargs->xyunits);

            siunit = gwy_data_field_get_si_unit_z(dfield);
            gwy_si_unit_set_from_string(siunit, dimsargs->zunits);
        }
    }

    mag = pow10(dimsargs->zpow10);
    //args->height *= mag;
    pat_synth_do(args, dfield);
    //args->height /= mag;

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
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *spin;
    PatSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response;
    gint row;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
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
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.type = pattern_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Pattern:"), NULL,
                            GTK_OBJECT(controls.type), GWY_HSCALE_WIDGET);
    row++;



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
            {
                gboolean temp = args->update;
                gint temp2 = args->active_page;
                *args = pat_synth_defaults;
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
        const PatSynthPattern *pattern = get_pattern(controls->args->type);
        GwyDimensions *dims = controls->dims;

        controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
        pattern->dims_changed(controls);
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
    controls->args->type = gwy_enum_combo_box_get_active(combo);
    /* TODO: switch pattern */
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
    gdouble mag = pow10(controls->dims->args->zpow10);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, FALSE);
    else
        gwy_data_field_clear(dfield);

    //args->height *= mag;
    pat_synth_do(args, dfield);
    //args->height /= mag;
}

static void
pat_synth_do(const PatSynthArgs *args,
             GwyDataField *dfield)
{
    const PatSynthPattern *pattern;

    pattern = get_pattern(args->type);

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

static gint
attach_scaled_value(PatSynthControls *controls,
                    gint row,
                    GtkObject *adj,
                    gdouble *target,
                    const gchar *name,
                    const gchar *units,
                    GwyHScaleStyle hscale_style,
                    GtkWidget **pspin,
                    GtkWidget **pvalue,
                    GtkWidget **punits)
{
    GtkTable *table = GTK_TABLE(controls->table);
    GtkWidget *spin;

    g_object_set_data(G_OBJECT(adj), "target", target);

    spin = gwy_table_attach_hscale(GTK_WIDGET(table),
                                   row, name, units, adj, hscale_style);
    if (pspin)
        *pspin = spin;
    row++;

    *pvalue = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(*pvalue), 1.0, 0.5);
    gtk_table_attach(table, *pvalue, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    *punits = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(*punits), 0.0, 0.5);
    gtk_table_attach(table, *punits, 3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    g_signal_connect_swapped(adj, "value-changed",
                             G_CALLBACK(double_changed), controls);

    return row;
}

typedef struct {
    gdouble pitch;
    gdouble slope;
} PatSynthArgsSteps;

typedef struct {
    PatSynthArgsSteps *args;
    GtkObject *pitch;
    GtkWidget *pitch_value;
    GtkWidget *pitch_units;
    GtkObject *slope;
    GtkWidget *slope_value;
    GtkWidget *slope_units;
} PatSynthControlsSteps;

static void
steps_pitch_changed(PatSynthControls *controls)
{
    PatSynthControlsSteps *pcontrols = controls->pattern_controls;
    PatSynthArgsSteps *pargs = controls->pattern_args;

    update_value_label(GTK_LABEL(pcontrols->pitch),
                       controls->dims->xyvf,
                       pargs->pitch * controls->pxsize);
    update_value_label(GTK_LABEL(pcontrols->slope),
                       controls->dims->xyvf,
                       pargs->pitch * pargs->slope/100.0 * controls->pxsize);
}

static void
steps_slope_changed(PatSynthControls *controls)
{
    PatSynthControlsSteps *pcontrols = controls->pattern_controls;
    PatSynthArgsSteps *pargs = controls->pattern_args;

    update_value_label(GTK_LABEL(pcontrols->slope),
                       controls->dims->xyvf,
                       pargs->pitch * pargs->slope/100.0 * controls->pxsize);
}

static gpointer
create_gui_steps(PatSynthControls *controls)
{
    PatSynthControlsSteps *pcontrols;
    PatSynthArgsSteps *pargs;
    GtkTable *table;
    gint row;

    table = GTK_TABLE(controls->table);
    row = extend_table(table, 7);
    pcontrols = g_new0(PatSynthControlsSteps, 1);
    pargs = pcontrols->args = controls->pattern_args;

    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Ideal Geometry")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    pcontrols->pitch = gtk_adjustment_new(pargs->pitch,
                                          1.0, 1000.0, 0.1, 10.0, 0);
    row = attach_scaled_value(controls, row,
                              pcontrols->pitch, &pargs->pitch,
                              _("Pitch:"), "px", GWY_HSCALE_SQRT,
                              NULL,
                              &pcontrols->pitch_value,
                              &pcontrols->pitch_units);

    pcontrols->slope = gtk_adjustment_new(pargs->slope,
                                          0.0, 100.0, 0.1, 1.0, 0);
    row = attach_scaled_value(controls, row,
                              pcontrols->pitch, &pargs->pitch,
                              _("Sloping part:"), "%", GWY_HSCALE_DEFAULT,
                              NULL,
                              &pcontrols->slope_value,
                              &pcontrols->slope_units);

    g_signal_connect_swapped(pcontrols->pitch, "value-changed",
                             G_CALLBACK(steps_pitch_changed), controls);
    g_signal_connect_swapped(pcontrols->slope, "value-changed",
                             G_CALLBACK(steps_slope_changed), controls);

    return pcontrols;
}

static void
destroy_gui_steps(gpointer p)
{
    PatSynthControlsSteps *pcontrols = p;

    g_free(pcontrols->args);
    g_free(pcontrols);
}

static void
dimensions_changed_steps(PatSynthControls *controls)
{
    PatSynthControlsSteps *pcontrols = controls->pattern_controls;
    GwyDimensions *dims = controls->dims;

    gtk_label_set_markup(GTK_LABEL(pcontrols->pitch_units),
                         dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(pcontrols->slope_units),
                         dims->xyvf->units);
    /* Updates also subordinate values */
    steps_pitch_changed(controls);
}

static void
make_pattern_steps(PatSynthArgs *args,
                   GwyDimensionArgs *dimsargs,
                   gpointer pargs,
                   GwyDataField *dfield)
{
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

    g_string_append(g_string_truncate(key, len), "pitch");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->pitch);

    g_string_append(g_string_truncate(key, len), "slope");
    gwy_container_gis_double_by_name(settings, key->str, &pargs->slope);

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

    g_string_append(g_string_truncate(key, len), "pitch");
    gwy_container_set_double_by_name(settings, key->str, pargs->pitch);

    g_string_append(g_string_truncate(key, len), "slope");
    gwy_container_set_double_by_name(settings, key->str, pargs->slope);

    g_string_free(key, TRUE);
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
