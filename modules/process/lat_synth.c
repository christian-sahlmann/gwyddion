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

#define LAT_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define DECLARE_LATTICE(name) \
    static void make_lattice_##name(const LatSynthArgs *args, \
                                    const GwyDimensionArgs *dimsargs, \
                                    GwyRandGenSet *rngset, \
                                    GwyDataField *dfield)

enum {
    PREVIEW_SIZE = 400,
};

enum {
    RESPONSE_RESET = 1,
};

enum {
    PAGE_DIMENSIONS = 0,
    PAGE_LATTICE    = 1,
    PAGE_SURFACE    = 2,
    PAGE_NPAGES
};

typedef enum {
    RNG_POINTS     = 0,
    RNG_MISSING    = 1,
    RNG_EXTRA      = 2,
    RNG_VALUE      = 3,
    RNG_DISPLAC_X  = 4,
    RNG_DISPLAC_Y  = 5,
    RNG_NRNGS
} LatSynthRng;

typedef enum {
    LAT_SYNTH_RANDOM     = 0,
    LAT_SYNTH_SQUARE     = 1,
    LAT_SYNTH_HEXAGONAL  = 2,
    LAT_SYNTH_TRIANGULAR = 3,
    LAT_SYNTH_NTYPES
} LatSynthType;

typedef enum {
    LAT_SURFACE_FLAT      = 0,
    LAT_SURFACE_RADIAL    = 1,
    LAT_SURFACE_SEGMENTED = 2,
    LAT_SURFACE_BORDER    = 3,
    LAT_SURFACE_SECOND    = 4,
    LAT_SURFACE_DOTPROD   = 5,
    LAT_SURFACE_NTYPES
} LatSynthSurface;

typedef struct _LatSynthArgs LatSynthArgs;
typedef struct _LatSynthControls LatSynthControls;

/* XXX: This is more complex.  We just create a lattice, the field is rendered
 * later. */
typedef void (*MakeLatticeFunc)(const LatSynthArgs *args,
                                const GwyDimensionArgs *dimsargs,
                                GwyRandGenSet *rngset,
                                GwyDataField *dfield);

/* This scheme makes the object type list easily reordeable in the GUI without
 * changing the ids.  */
typedef struct {
    LatSynthType type;
    const gchar *name;
    MakeLatticeFunc create;
} LatSynthLattice;

struct _LatSynthArgs {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;
    LatSynthType lattice_type;
    gdouble size;
    gdouble angle;
    gdouble sigma;
    gdouble tau;
    gdouble height;
};

struct _LatSynthControls {
    LatSynthArgs *args;
    GwyDimensions *dims;
    const LatSynthLattice *lattice;
    GwyRandGenSet *rngset;
    GwyContainer *mydata;
    GwyDataField *surface;
    /* They have different types, known only to the lattice. */
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkTable *table;   /* Only used in synth.h attach functions. */
    GtkWidget *lattice_type;
    GtkObject *size;
    GtkObject *angle;
    GtkObject *sigma;
    GtkObject *tau;
    GtkObject *surface_type_weight[LAT_SURFACE_NTYPES];
    GtkObject *height;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
    gulong sid;
};

static gboolean      module_register      (void);
static void          lat_synth            (GwyContainer *data,
                                           GwyRunType run);
static void          run_noninteractive   (LatSynthArgs *args,
                                           const GwyDimensionArgs *dimsargs,
                                           GwyRandGenSet *rngset,
                                           GwyContainer *data,
                                           GwyDataField *dfield,
                                           gint oldid,
                                           GQuark quark);
static gboolean      lat_synth_dialog     (LatSynthArgs *args,
                                           GwyDimensionArgs *dimsargs,
                                           GwyRandGenSet *rngset,
                                           GwyContainer *data,
                                           GwyDataField *dfield,
                                           gint id);
static GtkWidget*    lattice_selector_new (LatSynthControls *controls);
static void          update_controls      (LatSynthControls *controls,
                                           LatSynthArgs *args);
static void          page_switched        (LatSynthControls *controls,
                                           GtkNotebookPage *page,
                                           gint pagenum);
static void          update_values        (LatSynthControls *controls);
static void          lattice_type_selected(GtkComboBox *combo,
                                           LatSynthControls *controls);
static void          lat_synth_invalidate (LatSynthControls *controls);
static gboolean      preview_gsource      (gpointer user_data);
static void          preview              (LatSynthControls *controls);
static void          lat_synth_do         (const LatSynthArgs *args,
                                           const GwyDimensionArgs *dimsargs,
                                           GwyRandGenSet *rngset,
                                           GwyDataField *dfield);
static GwyDataField* make_displacement_map(guint xres,
                                           guint yres,
                                           gdouble sigma,
                                           gdouble tau,
                                           GRand *rng);
static void          lat_synth_load_args  (GwyContainer *container,
                                           LatSynthArgs *args,
                                           GwyDimensionArgs *dimsargs);
static void          lat_synth_save_args  (GwyContainer *container,
                                           const LatSynthArgs *args,
                                           const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS LatSynthControls
#define GWY_SYNTH_INVALIDATE(controls) \
    lat_synth_invalidate(controls)

#include "synth.h"

DECLARE_LATTICE(random);
DECLARE_LATTICE(square);
DECLARE_LATTICE(hexagonal);
DECLARE_LATTICE(triangular);

static const LatSynthArgs lat_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    LAT_SYNTH_RANDOM,
    20.0,
    0.0,
    0.0, 0.0,
    0.0,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static const LatSynthLattice lattices[] = {
    { LAT_SYNTH_RANDOM,     N_("lattice|Random"),     make_lattice_random,     },
    { LAT_SYNTH_SQUARE,     N_("lattice|Square"),     make_lattice_square,     },
    { LAT_SYNTH_HEXAGONAL,  N_("lattice|Hexagonal"),  make_lattice_hexagonal,  },
    { LAT_SYNTH_TRIANGULAR, N_("lattice|Triangular"), make_lattice_triangular, },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates surfaces based on regular or random lattices."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("lat_synth",
                              (GwyProcessFunc)&lat_synth,
                              N_("/S_ynthetic/_Lattice..."),
                              NULL,
                              LAT_SYNTH_RUN_MODES,
                              0,
                              N_("Generate lattice based surface"));

    return TRUE;
}

static void
lat_synth(GwyContainer *data, GwyRunType run)
{
    LatSynthArgs args;
    GwyDimensionArgs dimsargs;
    GwyRandGenSet *rngset;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & LAT_SYNTH_RUN_MODES);
    lat_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    rngset = gwy_rand_gen_set_new(RNG_NRNGS);
    if (run == GWY_RUN_IMMEDIATE
        || lat_synth_dialog(&args, &dimsargs, rngset, data, dfield, id)) {
        lat_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);
        run_noninteractive(&args, &dimsargs, rngset, data, dfield, id, quark);
    }

    gwy_rand_gen_set_free(rngset);
    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(LatSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   GwyRandGenSet *rngset,
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

        gwy_app_channel_log_add(data, oldid, oldid, "proc::lat_synth", NULL);
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

    lat_synth_do(args, dimsargs, rngset, dfield);

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
                                "proc::lat_synth", NULL);
    }
    g_object_unref(dfield);
}

static gboolean
lat_synth_dialog(LatSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 GwyRandGenSet *rngset,
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook;
    LatSynthControls controls;
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
    dialog = gtk_dialog_new_with_buttons(_("Lattice"),
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
                                 G_CALLBACK(lat_synth_invalidate), &controls);

    table = gtk_table_new(5, 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Lattice")));
    row = 0;

    controls.lattice_type = lattice_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Lattice:"), NULL,
                            GTK_OBJECT(controls.lattice_type),
                            GWY_HSCALE_WIDGET);
    row++;

    table = gtk_table_new(1, 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Surface")));

    gtk_widget_show_all(dialog);
    controls.table = NULL;
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);
    lat_synth_invalidate(&controls);

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
            args->seed = lat_synth_defaults.seed;
            args->randomize = lat_synth_defaults.randomize;
            /* TODO */
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
lattice_selector_new(LatSynthControls *controls)
{
    GtkWidget *combo;
    GwyEnum *model;
    guint n, i;

    n = G_N_ELEMENTS(lattices);
    model = g_new(GwyEnum, n);
    for (i = 0; i < n; i++) {
        model[i].value = lattices[i].type;
        model[i].name = lattices[i].name;
    }

    combo = gwy_enum_combo_box_new(model, n,
                                   G_CALLBACK(lattice_type_selected), controls,
                                   controls->args->lattice_type, TRUE);
    g_object_weak_ref(G_OBJECT(combo), (GWeakNotify)g_free, model);

    return combo;
}

static void
update_controls(LatSynthControls *controls,
                LatSynthArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->lattice_type),
                                  args->lattice_type);
    /* TODO */
}

static void
page_switched(LatSynthControls *controls,
              G_GNUC_UNUSED GtkNotebookPage *page,
              gint pagenum)
{
    if (controls->in_init)
        return;

    controls->args->active_page = pagenum;
    if (pagenum == PAGE_LATTICE)
        update_values(controls);
}

static void
update_values(LatSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
    /*
    gtk_label_set_markup(GTK_LABEL(controls->flat_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(controls->slope_units), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(controls->height_units), dims->zvf->units);
    gtk_label_set_markup(GTK_LABEL(controls->tau_units), dims->xyvf->units);

    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(controls->flat));
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(controls->slope));
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(controls->tau));
    */
}

static void
lattice_type_selected(GtkComboBox *combo,
                      LatSynthControls *controls)
{
    LatSynthArgs *args = controls->args;
    args->lattice_type = gwy_enum_combo_box_get_active(combo);
    lat_synth_invalidate(controls);
}

static void
lat_synth_invalidate(LatSynthControls *controls)
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
    LatSynthControls *controls = (LatSynthControls*)user_data;
    controls->sid = 0;

    preview(controls);

    return FALSE;
}

static void
preview(LatSynthControls *controls)
{
    LatSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, FALSE);
    else
        gwy_data_field_clear(dfield);

    lat_synth_do(args, controls->dims->args, controls->rngset, dfield);
}

static void
lat_synth_do(const LatSynthArgs *args,
             const GwyDimensionArgs *dimsargs,
             GwyRandGenSet *rngset,
             GwyDataField *dfield)
{
    gwy_rand_gen_set_init(rngset, args->seed);
    /* TODO */
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
    giter->k = giter->j = giter->i = 0;
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

static void
make_lattice_random(const LatSynthArgs *args,
                    const GwyDimensionArgs *dimsargs,
                    GwyRandGenSet *rngset,
                    GwyDataField *dfield)
{
}

static void
make_lattice_square(const LatSynthArgs *args,
                    const GwyDimensionArgs *dimsargs,
                    GwyRandGenSet *rngset,
                    GwyDataField *dfield)
{
}

static void
make_lattice_hexagonal(const LatSynthArgs *args,
                       const GwyDimensionArgs *dimsargs,
                       GwyRandGenSet *rngset,
                       GwyDataField *dfield)
{
}

static void
make_lattice_triangular(const LatSynthArgs *args,
                        const GwyDimensionArgs *dimsargs,
                        GwyRandGenSet *rngset,
                        GwyDataField *dfield)
{
}

static const gchar prefix[]           = "/module/lat_synth";
static const gchar active_page_key[]  = "/module/lat_synth/active_page";
static const gchar update_key[]       = "/module/lat_synth/update";
static const gchar randomize_key[]    = "/module/lat_synth/randomize";
static const gchar seed_key[]         = "/module/lat_synth/seed";
static const gchar lattice_type_key[] = "/module/lat_synth/lattice_type";
static const gchar size_key[]         = "/module/lat_synth/size";
static const gchar angle_key[]        = "/module/lat_synth/angle";
static const gchar sigma_key[]        = "/module/lat_synth/sigma";
static const gchar tau_key[]          = "/module/lat_synth/tau";
static const gchar height_key[]       = "/module/lat_synth/height";

static void
lat_synth_sanitize_args(LatSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = !!args->update;
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->lattice_type = MIN(args->lattice_type, LAT_SYNTH_NTYPES-1);
    args->size = CLAMP(args->size, 1.0, 1000.0);
    args->angle = CLAMP(args->angle, -G_PI, G_PI);
    args->sigma = CLAMP(args->sigma, 0.0, 100.0);
    args->tau = CLAMP(args->sigma, 0.1, 1000.0);
}

static void
lat_synth_load_args(GwyContainer *container,
                    LatSynthArgs *args,
                    GwyDimensionArgs *dimsargs)
{
    *args = lat_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_enum_by_name(container, lattice_type_key,
                                   &args->lattice_type);
    gwy_container_gis_double_by_name(container, size_key, &args->size);
    gwy_container_gis_double_by_name(container, angle_key, &args->angle);
    gwy_container_gis_double_by_name(container, sigma_key, &args->sigma);
    gwy_container_gis_double_by_name(container, tau_key, &args->tau);
    lat_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
lat_synth_save_args(GwyContainer *container,
                    const LatSynthArgs *args,
                    const GwyDimensionArgs *dimsargs)
{
    gwy_container_set_int32_by_name(container, active_page_key,
                                    args->active_page);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
    gwy_container_set_enum_by_name(container, lattice_type_key,
                                   args->lattice_type);
    gwy_container_set_double_by_name(container, size_key, args->size);
    gwy_container_set_double_by_name(container, angle_key, args->angle);
    gwy_container_set_double_by_name(container, sigma_key, args->sigma);
    gwy_container_set_double_by_name(container, tau_key, args->tau);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
