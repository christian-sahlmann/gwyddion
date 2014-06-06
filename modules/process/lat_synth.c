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
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyrandgenset.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define LAT_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define EPS 0.0000001

/* How larger the squarized grid should be (measured in squares). */
#define SQBORDER 2

/* A convience macro to make the source readable.
   Use: VOBJ(p->next)->angle = M_PI2 */
#define VOBJ(x) ((VoronoiObject*)(x)->data)

#define DOTPROD_SS(a, b) ((a).x*(b).x + (a).y*(b).y)
#define DOTPROD_SP(a, b) ((a).x*(b)->x + (a).y*(b)->y)
#define DOTPROD_PS(a, b) ((a)->x*(b).x + (a)->y*(b).y)

#define DECLARE_SURFACE(x) \
    static gdouble surface_##x(const VoronoiCoords *point, \
                               const VoronoiObject *owner, \
                               const LatSynthArgs *args)

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

/* The random grid uses the generators differently so there are aliases. */
typedef enum {
    RNG_POINTS     = 0,
    RNG_MISSING    = 0,
    RNG_EXTRA      = 1,
    RNG_DISPLAC_X  = 2,
    RNG_DISPLAC_Y  = 3,
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
    LAT_SURFACE_FLAT       = 0,
    LAT_SURFACE_LINEAR     = 1,
    LAT_SURFACE_BUMPY      = 2,
    LAT_SURFACE_RADIAL     = 3,
    LAT_SURFACE_SEGMENTED  = 4,
    LAT_SURFACE_ZSEGMENTED = 5,
    LAT_SURFACE_BORDER     = 6,
    LAT_SURFACE_ZBORDER    = 7,
    LAT_SURFACE_SECOND     = 8,
    LAT_SURFACE_NTYPES
} LatSynthSurface;

typedef struct _LatSynthArgs LatSynthArgs;
typedef struct _LatSynthControls LatSynthControls;

typedef struct {
    gdouble x, y;
} VoronoiCoords;

typedef struct {
    VoronoiCoords v; /* line equation: v*r == d */
    gdouble d;
} VoronoiLine;

typedef struct {
    VoronoiCoords pos; /* coordinates */
    VoronoiLine rel; /* precomputed coordinates relative to currently processed
                        object and their norm */
    gdouble angle; /* precomputed angle relative to currently processed object
                      (similar as rel) */
    gdouble random;  /* a random number in [0,1], generated to be always the
                        same for the same grid size */
    GSList *ne; /* neighbour list */
} VoronoiObject;

typedef struct {
    GwyRandGenSet *rngset;
    GSList **squares; /* (hsq+2*SQBORDER)*(wsq+2*SQBORDER) VoronoiObject list */
    gint wsq;         /* width in squares (unextended) */
    gint hsq;         /* height in squares (unextended) */
    gdouble scale;    /* ratio of square side to the average cell size */
} VoronoiState;

typedef gdouble (*RenderFunc)(const VoronoiCoords *point,
                              const VoronoiObject *owner,
                              const LatSynthArgs *args);

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
    gboolean enabled[LAT_SURFACE_NTYPES];
    gdouble weight[LAT_SURFACE_NTYPES];
    gdouble lower[LAT_SURFACE_NTYPES];
    gdouble upper[LAT_SURFACE_NTYPES];
    /* Cache vstate.scale here */
    gdouble scale;
};

struct _LatSynthControls {
    LatSynthArgs *args;
    GwyDimensions *dims;
    GwyContainer *mydata;
    GwyDataField *surface;
    VoronoiState *vstate;
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
    GtkWidget *size_value;
    GtkWidget *size_units;
    GtkObject *angle;
    GtkObject *sigma;
    GtkObject *tau;
    GtkWidget *surfaces;
    GtkWidget *enabled;
    GtkWidget *surface_header;
    GtkObject *weight;
    GtkObject *lower;
    GtkObject *upper;
    GtkObject *height;
    LatSynthSurface selected_surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
    gulong sid;
};

static gboolean       module_register             (void);
static void           lat_synth                   (GwyContainer *data,
                                                   GwyRunType run);
static void           run_noninteractive          (LatSynthArgs *args,
                                                   const GwyDimensionArgs *dimsargs,
                                                   GwyContainer *data,
                                                   GwyDataField *dfield,
                                                   gint oldid,
                                                   GQuark quark);
static gboolean       lat_synth_dialog            (LatSynthArgs *args,
                                                   GwyDimensionArgs *dimsargs,
                                                   GwyContainer *data,
                                                   GwyDataField *dfield,
                                                   gint id);
static void           update_controls             (LatSynthControls *controls,
                                                   LatSynthArgs *args);
static void           page_switched               (LatSynthControls *controls,
                                                   GtkNotebookPage *page,
                                                   gint pagenum);
static void           update_values               (LatSynthControls *controls);
static void           create_surface_treeview     (LatSynthControls *controls);
static void           select_surface              (LatSynthControls *controls,
                                                   LatSynthSurface i);
static void           enabled_toggled             (LatSynthControls *controls,
                                                   const gchar *strpath,
                                                   GtkCellRendererToggle *toggle);
static void           render_enabled              (GtkTreeViewColumn *column,
                                                   GtkCellRenderer *renderer,
                                                   GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   gpointer user_data);
static void           render_name                 (GtkTreeViewColumn *column,
                                                   GtkCellRenderer *renderer,
                                                   GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   gpointer user_data);
static void           render_weight               (GtkTreeViewColumn *column,
                                                   GtkCellRenderer *renderer,
                                                   GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   gpointer user_data);
static void           surface_selected            (LatSynthControls *controls,
                                                   GtkTreeSelection *selection);
static void           lattice_type_selected       (GtkComboBox *combo,
                                                   LatSynthControls *controls);
static void           weight_changed              (LatSynthControls *controls,
                                                   GtkAdjustment *adj);
static void           lower_changed               (LatSynthControls *controls,
                                                   GtkAdjustment *adj);
static void           upper_changed               (LatSynthControls *controls,
                                                   GtkAdjustment *adj);
static void           invalidate_lattice          (LatSynthControls *controls);
static void           lat_synth_invalidate        (LatSynthControls *controls);
static gboolean       preview_gsource             (gpointer user_data);
static void           preview                     (LatSynthControls *controls);
static VoronoiState*  lat_synth_do                (LatSynthArgs *args,
                                                   const GwyDimensionArgs *dimsargs,
                                                   VoronoiState *vstate,
                                                   GwyDataField *dfield);
static void           construct_surface           (LatSynthArgs *args,
                                                   const GwyDimensionArgs *dimsargs,
                                                   VoronoiState *vstate,
                                                   GwyDataField *dfield);
static VoronoiState*  make_randomized_grid        (const LatSynthArgs *args,
                                                   guint xres,
                                                   guint yres);
static void           random_squarized_points     (GSList **squares,
                                                   guint extwsq,
                                                   guint exthsq,
                                                   guint npts,
                                                   GRand *rng);
static GwyDataField*  make_displacement_map       (guint xres,
                                                   guint yres,
                                                   gdouble sigma,
                                                   gdouble tau,
                                                   GRand *rng);
static void           find_voronoi_neighbours_iter(VoronoiState *vstate,
                                                   gint iter);
static VoronoiObject* find_owner                  (VoronoiState *vstate,
                                                   const VoronoiCoords *point);
static void           neighbourize                (GSList *ne0,
                                                   const VoronoiCoords *center);
static void           compute_segment_angles      (GSList *ne0);
static VoronoiObject* move_along_line             (const VoronoiObject *owner,
                                                   const VoronoiCoords *start,
                                                   const VoronoiCoords *end,
                                                   gint *next_safe);
static void           voronoi_state_free          (VoronoiState *vstate);
static void           lat_synth_load_args         (GwyContainer *container,
                                                   LatSynthArgs *args,
                                                   GwyDimensionArgs *dimsargs);
static void           lat_synth_save_args         (GwyContainer *container,
                                                   const LatSynthArgs *args,
                                                   const GwyDimensionArgs *dimsargs);

DECLARE_SURFACE(flat);
DECLARE_SURFACE(linear);
DECLARE_SURFACE(bumpy);
DECLARE_SURFACE(radial);
DECLARE_SURFACE(segmented);
DECLARE_SURFACE(zsegmented);
DECLARE_SURFACE(border);
DECLARE_SURFACE(zborder);
DECLARE_SURFACE(second);

#define GWY_SYNTH_CONTROLS LatSynthControls
#define GWY_SYNTH_INVALIDATE(controls) \
    lat_synth_invalidate(controls)

#include "synth.h"

static const RenderFunc surface_functions[LAT_SURFACE_NTYPES] = {
    surface_flat,
    surface_linear,
    surface_bumpy,
    surface_radial,
    surface_segmented,
    surface_zsegmented,
    surface_border,
    surface_zborder,
    surface_second,
};

static const gchar* surface_names[LAT_SURFACE_NTYPES] = {
    N_("Random constant"),
    N_("Random linear"),
    N_("Random bumpy"),
    N_("Radial distance"),
    N_("Segmented distance"),
    N_("Segmented random"),
    N_("Border distance"),
    N_("Border random"),
    N_("Second nearest distance"),
};

static const LatSynthArgs lat_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    LAT_SYNTH_RANDOM,
    40.0,
    0.0,
    0.0, 0.0,
    0.0,
    { FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, },
    { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, },
    { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, },
    0.0,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

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
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & LAT_SYNTH_RUN_MODES);
    lat_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    if (run == GWY_RUN_IMMEDIATE
        || lat_synth_dialog(&args, &dimsargs, data, dfield, id)) {
        lat_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);
        run_noninteractive(&args, &dimsargs, data, dfield, id, quark);
    }

    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(LatSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   gint oldid,
                   GQuark quark)
{
    GwySIUnit *siunit;
    VoronoiState *vstate;
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

    vstate = lat_synth_do(args, dimsargs, NULL, dfield);
    voronoi_state_free(vstate);

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
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    static const GwyEnum lattice_types[LAT_SYNTH_NTYPES] = {
        { N_("lattice|Random"),     LAT_SYNTH_RANDOM,     },
        { N_("lattice|Square"),     LAT_SYNTH_SQUARE,     },
        { N_("lattice|Hexagonal"),  LAT_SYNTH_HEXAGONAL,  },
        { N_("lattice|Triangular"), LAT_SYNTH_TRIANGULAR, },
    };

    GtkWidget *dialog, *table, *vbox, *hbox, *notebook, *scwin;
    LatSynthControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    gboolean finished;
    gint response, row;
    guint i;

    gwy_clear(&controls, 1);
    controls.in_init = TRUE;
    controls.args = args;
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
    g_signal_connect_swapped(controls.seed, "value-changed",
                             G_CALLBACK(invalidate_lattice), &controls);

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

    controls.lattice_type
        = gwy_enum_combo_box_new(lattice_types, G_N_ELEMENTS(lattice_types),
                                 G_CALLBACK(lattice_type_selected), &controls,
                                 args->lattice_type, TRUE);
    gwy_table_attach_hscale(table, row, _("_Lattice:"), NULL,
                            GTK_OBJECT(controls.lattice_type),
                            GWY_HSCALE_WIDGET);
    row++;

    controls.size = gtk_adjustment_new(args->size, 4.0, 1000.0, 0.1, 10.0, 0);
    row = gwy_synth_attach_lateral(&controls, row, controls.size, &args->size,
                                   _("_Size:"), GWY_HSCALE_LOG,
                                   NULL,
                                   &controls.size_value, &controls.size_units);
    g_signal_connect_swapped(controls.size, "value-changed",
                             G_CALLBACK(invalidate_lattice), &controls);
    row++;

    table = gtk_table_new(1, 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Surface")));
    row = 0;

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scwin, -1, 240);
    gtk_table_attach(GTK_TABLE(table), scwin, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    create_surface_treeview(&controls);
    gtk_container_add(GTK_CONTAINER(scwin), controls.surfaces);
    row++;

    controls.surface_header = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.surface_header), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.surface_header,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.weight = gtk_adjustment_new(0.0, -1.0, 1.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("_Weight:"), NULL,
                            controls.weight, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.weight, "value-changed",
                             G_CALLBACK(weight_changed), &controls);
    row++;

    controls.lower = gtk_adjustment_new(0.0, 0.0, 1.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("Lower threshold:"), NULL,
                            controls.lower, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.lower, "value-changed",
                             G_CALLBACK(lower_changed), &controls);
    row++;

    controls.upper = gtk_adjustment_new(0.0, 0.0, 1.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("Upper threshold:"), NULL,
                            controls.upper, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.upper, "value-changed",
                             G_CALLBACK(upper_changed), &controls);
    row++;

    gtk_widget_show_all(dialog);
    controls.table = NULL;
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);
    for (i = 0; i < LAT_SURFACE_NTYPES; i++) {
        if (args->enabled[i])
            break;
    }
    select_surface(&controls, i % LAT_SURFACE_NTYPES);

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

    lat_synth_save_args(gwy_app_settings_get(), args, dimsargs);

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
    gtk_label_set_markup(GTK_LABEL(controls->size_units), dims->xyvf->units);
    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(controls->size));
}

static void
create_surface_treeview(LatSynthControls *controls)
{
    GtkTreeModel *model;
    GtkWidget *treeview;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;

    model = GTK_TREE_MODEL(gwy_null_store_new(LAT_SURFACE_NTYPES));
    treeview = gtk_tree_view_new_with_model(model);
    controls->surfaces = treeview;
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    g_object_unref(model);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(column, FALSE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(renderer, "activatable", TRUE, NULL);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_enabled, controls, NULL);
    g_signal_connect_swapped(renderer, "toggled",
                             G_CALLBACK(enabled_toggled), controls);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_name, controls, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(column, FALSE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "width-chars", 7, "xalign", 1.0, NULL);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_weight, controls, NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(surface_selected), controls);
}

static void
select_surface(LatSynthControls *controls,
               LatSynthSurface i)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->surfaces));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->surfaces));
    gtk_tree_model_iter_nth_child(model, &iter, NULL, i);
    gtk_tree_selection_select_iter(selection, &iter);
}

static void
enabled_toggled(LatSynthControls *controls,
                const gchar *strpath,
                G_GNUC_UNUSED GtkCellRendererToggle *toggle)
{
    GtkTreeView *treeview = GTK_TREE_VIEW(controls->surfaces);
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreePath *path;
    GtkTreeIter iter;
    guint i;

    path = gtk_tree_path_new_from_string(strpath);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);
    gtk_tree_model_get(model, &iter, 0, &i, -1);
    controls->args->enabled[i] = !controls->args->enabled[i];
    gwy_null_store_row_changed(GWY_NULL_STORE(model), i);
    lat_synth_invalidate(controls);
}

static void
render_enabled(G_GNUC_UNUSED GtkTreeViewColumn *column,
               GtkCellRenderer *renderer,
               GtkTreeModel *model,
               GtkTreeIter *iter,
               gpointer user_data)
{
    LatSynthControls *controls = (LatSynthControls*)user_data;
    guint i;

    gtk_tree_model_get(model, iter, 0, &i, -1);
    g_object_set(renderer, "active", controls->args->enabled[i], NULL);
}

static void
render_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer user_data)
{
    guint i;

    gtk_tree_model_get(model, iter, 0, &i, -1);
    g_object_set(renderer, "text", gettext(surface_names[i]), NULL);
}

static void
render_weight(G_GNUC_UNUSED GtkTreeViewColumn *column,
              GtkCellRenderer *renderer,
              GtkTreeModel *model,
              GtkTreeIter *iter,
              gpointer user_data)
{
    LatSynthControls *controls = (LatSynthControls*)user_data;
    gchar buf[12];
    guint i;

    gtk_tree_model_get(model, iter, 0, &i, -1);
    g_snprintf(buf, sizeof(buf), "%.03f", controls->args->weight[i]);
    g_object_set(renderer, "text", buf, NULL);
}

static void
surface_selected(LatSynthControls *controls, GtkTreeSelection *selection)
{
    LatSynthArgs *args = controls->args;
    GtkTreeModel *model;
    GtkTreeIter iter;
    guint i;
    gchar *s;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, 0, &i, -1);
    controls->selected_surface = i;

    controls->in_init = TRUE;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->weight), args->weight[i]);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->lower), args->lower[i]);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->upper), args->upper[i]);
    controls->in_init = FALSE;

    s = g_strconcat("<b>", surface_names[i], "</b>", NULL);
    gtk_label_set_markup(GTK_LABEL(controls->surface_header), s);
    g_free(s);
}

static void
lattice_type_selected(GtkComboBox *combo, LatSynthControls *controls)
{
    LatSynthArgs *args = controls->args;
    args->lattice_type = gwy_enum_combo_box_get_active(combo);
    invalidate_lattice(controls);
    lat_synth_invalidate(controls);
}

static void
weight_changed(LatSynthControls *controls, GtkAdjustment *adj)
{
    GtkTreeModel *model;
    controls->args->weight[controls->selected_surface]
        = gtk_adjustment_get_value(adj);
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->surfaces));
    gwy_null_store_row_changed(GWY_NULL_STORE(model),
                               controls->selected_surface);
    lat_synth_invalidate(controls);
}

static void
lower_changed(LatSynthControls *controls, GtkAdjustment *adj)
{
    controls->args->lower[controls->selected_surface]
        = gtk_adjustment_get_value(adj);
    lat_synth_invalidate(controls);
}

static void
upper_changed(LatSynthControls *controls, GtkAdjustment *adj)
{
    controls->args->upper[controls->selected_surface]
        = gtk_adjustment_get_value(adj);
    lat_synth_invalidate(controls);
}

/* This extra callback is only invoked on changed that influence the lattice.
 * Visualisation style changes do not need to costly recompute it. */
static void
invalidate_lattice(LatSynthControls *controls)
{
    if (controls->vstate) {
        voronoi_state_free(controls->vstate);
        controls->vstate = NULL;
    }
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

    controls->vstate = lat_synth_do(args, controls->dims->args,
                                    controls->vstate, dfield);
}

static VoronoiState*
lat_synth_do(LatSynthArgs *args,
             const GwyDimensionArgs *dimsargs,
             VoronoiState *vstate,
             GwyDataField *dfield)
{
    guint xres = dfield->xres, yres = dfield->yres;
    guint iter, niter;
    GTimer *timer = g_timer_new();

    if (!vstate) {
        vstate = make_randomized_grid(args, xres, yres);

        gwy_debug("Lattice creation: %g ms",
                  1000.0*g_timer_elapsed(timer, NULL));
        g_timer_start(timer);

        niter = (vstate->wsq + 2*SQBORDER)*(vstate->hsq + 2*SQBORDER);
        for (iter = 0; iter < niter; iter++) {
            find_voronoi_neighbours_iter(vstate, iter);
            /* TODO: Update progress bar, if necessary. */
        }
        gwy_debug("Triangulation: %g ms", 1000.0*g_timer_elapsed(timer, NULL));
        g_timer_start(timer);
    }

    construct_surface(args, dimsargs, vstate, dfield);
    gwy_debug("Rendering: %g ms", 1000.0*g_timer_elapsed(timer, NULL));
    g_timer_destroy(timer);

    gwy_data_field_data_changed(dfield);

    return vstate;
}

static void
construct_surface(LatSynthArgs *args,
                  G_GNUC_UNUSED const GwyDimensionArgs *dimsargs,
                  VoronoiState *vstate,
                  GwyDataField *dfield)
{
    GwyDataField *tmpfield = gwy_data_field_new_alike(dfield, FALSE);
    VoronoiObject *owner, *line_start;
    VoronoiCoords z, zline, tmp;
    guint xres = dfield->xres, yres = dfield->yres;
    gint hsafe, vsafe;
    guint x, y, i;
    gdouble q, xoff, yoff;
    gdouble *data;

    if (xres <= yres) {
        q = (gdouble)vstate->wsq/xres;
        xoff = SQBORDER;
        yoff = SQBORDER + 0.5*(q*yres - vstate->hsq);
    }
    else {
        q = (gdouble)vstate->hsq/yres;
        xoff = SQBORDER + 0.5*(q*xres - vstate->wsq);
        yoff = SQBORDER;
    }

    args->scale = vstate->scale;
    data = gwy_data_field_get_data(tmpfield);
    for (i = 0; i < LAT_SURFACE_NTYPES; i++) {
        if (!args->enabled[i] || !args->weight[i]
            || args->lower[i] >= args->upper[i])
            continue;

        gwy_data_field_clear(tmpfield);
        zline.x = xoff;
        zline.y = yoff;
        line_start = find_owner(vstate, &zline);
        vsafe = 0;

        for (y = 0; y < yres; ) {
            hsafe = 0;
            z = zline;
            owner = line_start;

            neighbourize(owner->ne, &owner->pos);
            compute_segment_angles(owner->ne);

            tmp.y = zline.y;

            for (x = 0; x < xres; ) {
                data[y*xres + x] = surface_functions[i](&z, owner, args);

                /* Move right. */
                x++;
                if (hsafe-- == 0) {
                    tmp.x = q*x + xoff;
                    owner = move_along_line(owner, &z, &tmp, &hsafe);
                    neighbourize(owner->ne, &owner->pos);
                    compute_segment_angles(owner->ne);
                    z.x = tmp.x;
                }
                else
                    z.x = q*x + xoff;

            }

            /* Move down. */
            y++;
            if (vsafe-- == 0) {
                tmp.x = xoff;
                tmp.y = q*y + yoff;
                line_start = move_along_line(line_start, &zline, &tmp, &vsafe);
                zline.y = tmp.y;
            }
            else
                zline.y = q*y + yoff;
        }

        gwy_data_field_invalidate(tmpfield);
        gwy_data_field_normalize(tmpfield);
        if (args->lower[i] > 0.0 || args->upper[i] < 1.0)
            gwy_data_field_clamp(tmpfield, args->lower[i], args->upper[i]);
        if (args->weight[i] != 1.0)
            gwy_data_field_multiply(tmpfield, args->weight[i]);
        gwy_data_field_sum_fields(dfield, dfield, tmpfield);
    }

    g_object_unref(tmpfield);
}

static VoronoiState*
make_randomized_grid(const LatSynthArgs *args,
                     guint xres, guint yres)
{
    VoronoiState *vstate;
    gdouble scale, a;
    guint wsq, hsq, extwsq, exthsq;
    guint npts;

    /* Compute square size trying to get density per square around 7. The
     * shorter side of the field will be divided to squares exactly, the
     * longer side may have more squares, i.e. slightly wider border around
     * the field than SQBORDER. */
    gwy_debug("Field: %ux%u, size %g", xres, yres, args->size);
    if (xres <= yres) {
        wsq = (gint)ceil(xres/(sqrt(7.0)*args->size));
        a = xres/(gdouble)wsq;
        hsq = (gint)ceil((1.0 - EPS)*yres/a);
    }
    else {
        hsq = (gint)ceil(yres/(sqrt(7.0)*args->size));
        a = yres/(gdouble)hsq;
        wsq = (gint)ceil((1.0 - EPS)*xres/a);
    }
    gwy_debug("Squares: %ux%u", wsq, hsq);
    scale = a/args->size;
    gwy_debug("Scale: %g, Density: %g", scale, scale*scale);
    extwsq = wsq + 2*SQBORDER;
    exthsq = hsq + 2*SQBORDER;
    npts = ceil(exthsq*extwsq*scale*scale);
    if (npts < exthsq*extwsq) {
        /* XXX: This means we have only a handful of points in the image.
         * The result does not worth much anyway then. */
        npts = exthsq*extwsq;
    }

    vstate = g_new(VoronoiState, 1);
    vstate->squares = g_new0(GSList*, extwsq*exthsq);
    vstate->hsq = hsq;
    vstate->wsq = wsq;
    vstate->scale = scale;
    vstate->rngset = gwy_rand_gen_set_new(RNG_NRNGS);
    gwy_rand_gen_set_init(vstate->rngset, args->seed);

    if (TRUE || args->lattice_type == LAT_SYNTH_RANDOM) {
        GRand *rng = gwy_rand_gen_set_rng(vstate->rngset, RNG_POINTS);
        random_squarized_points(vstate->squares, extwsq, exthsq, npts, rng);
        return vstate;
    }

#if 0
    /* compute cell sizes, must take tileability into account
     * ncell is the number of *rectangular* cells */
    ncell.x = width/side*sqrt(shape_area_factor[gridtype])
        /rect_cell_to_axy_ratio[gridtype].x;
    ncell.y = height/side*sqrt(shape_area_factor[gridtype])
        /rect_cell_to_axy_ratio[gridtype].y;

    a.x = width/ncell.x/rect_cell_to_axy_ratio[gridtype].x;
    a.y = height/ncell.y/rect_cell_to_axy_ratio[gridtype].y;

    /* compensate for square deformation */
    a.x *= wsq/(gdouble)width;
    a.y *= hsq/(gdouble)height;
    radial_factor = 1.0/sqrt(a.x*a.y*grid_cell_center_distance[gridtype]);

    /*fprintf(stderr, "lambda = %f\n", lambda);*/
    wmap = MAX(1.2*rect_cell_to_axy_ratio[gridtype].x*ncell.x, 6);
    hmap = MAX(1.2*rect_cell_to_axy_ratio[gridtype].y*ncell.y, 6);
    defmapx = g_new(gdouble, wmap*hmap);
    defmapy = g_new(gdouble, wmap*hmap);
    slambda = lambda*sqrt(wmap*hmap/(gdouble)n);
    /*
       fprintf(stderr, "ncell = (%f, %f)\n", ncell.x, ncell.y);
       fprintf(stderr, "a = (%f, %f)\n", a.x, a.y);
       fprintf(stderr, "wmap = %d, hmap = %d, slambda = %f\n", wmap, hmap, slambda);
       */

    defsigma.x = a.x*sigma*grid_cell_center_distance[gridtype];
    defsigma.y = a.y*sigma*grid_cell_center_distance[gridtype];
    /*fprintf(stderr, "sigma = (%f, %f)\n", defsigma.x, defsigma.y);*/
    n = (extwsq*exthsq)/(a.x*a.y*shape_area_factor[gridtype]);
    iter = 0;
    do {
        grid = create_regular_grid(gridtype, &a, wsq, hsq);
        compute_deformation_map(defmapx, wmap, hmap, slambda);
        compute_deformation_map(defmapy, wmap, hmap, slambda);
        apply_deformation_map(grid, wsq, hsq,
                              defmapx, defmapy, defsigma, wmap, hmap);
        squarize_grid(grid, squares, wsq, hsq);
        add_dislocations(squares, wsq, hsq, n*interstit, n*vacancies);
        make_grid_tilable(squares, wsq, hsq, htil, vtil);
        if (iter++ == 20) {
            g_error("Cannot create grid. If you can reproduce this, report bug "
                    "to <yeti@physics.muni.cz>, please include grid generator "
                    "settings.");
        }
    } while (empty_squares(squares, wsq, hsq));

    g_free(defmapx);
    g_free(defmapy);
#endif

    return vstate;
}

static void
random_squarized_points(GSList **squares,
                        guint extwsq, guint exthsq, guint npts,
                        GRand *rng)
{
    VoronoiObject *obj;
    guint i, j, k, nsq, nempty, nrem;

    nsq = extwsq*exthsq;
    g_assert(npts >= nsq);
    nempty = nsq;
    nrem = npts;

    /* First place points randomly to the entire area.  For preiew, this part
     * does not depend on the mean cell size which is good because the radnom
     * lattice changes more or less smoothly with size then. */
    while (nrem > nempty) {
        obj = g_slice_new0(VoronoiObject);
        obj->pos.x = g_rand_double(rng)*(extwsq - 2.0*EPS) + EPS;
        obj->pos.y = g_rand_double(rng)*(exthsq - 2.0*EPS) + EPS;
        obj->random = g_rand_double(rng);
        j = (guint)floor(obj->pos.x);
        i = (guint)floor(obj->pos.y);
        k = extwsq*i + j;
        if (!squares[k])
            nempty--;

        squares[k] = g_slist_prepend(squares[k], obj);
        nrem--;
    }

    gwy_debug("Placed %u points into %u squares, %u empty squares left.",
              npts, nsq, nrem);

    if (!nrem)
        return;

    /* We still have some empty squares.  Must place a point to each.  This
     * depends strongly on the mean cell size but influences only a tiny
     * fraction (≈ 10⁻⁴) of points. */
    for (i = 0; i < exthsq; i++) {
        for (j = 0; j < extwsq; j++) {
            k = extwsq*i + j;
            if (squares[k])
                continue;

            obj = g_slice_new0(VoronoiObject);
            obj->pos.x = (1.0 - 2.0*EPS)*g_rand_double(rng) + EPS + j;
            obj->pos.y = (1.0 - 2.0*EPS)*g_rand_double(rng) + EPS + i;
            obj->random = g_rand_double(rng);
            squares[k] = g_slist_prepend(NULL, obj);
        }
    }
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

static inline VoronoiCoords
coords_minus(const VoronoiCoords *a, const VoronoiCoords *b)
{
    VoronoiCoords z;

    z.x = a->x - b->x;
    z.y = a->y - b->y;

    return z;
}

static inline VoronoiCoords
coords_plus(const VoronoiCoords *a, const VoronoiCoords *b)
{
    VoronoiCoords z;

    z.x = a->x + b->x;
    z.y = a->y + b->y;

    return z;
}

static inline gdouble
angle(const VoronoiCoords *r)
{
    return atan2(r->y, r->x);
}

static gint
vobj_angle_compare(gconstpointer x, gconstpointer y)
{
    gdouble xangle, yangle;

    xangle = ((const VoronoiObject*)x)->angle;
    yangle = ((const VoronoiObject*)y)->angle;

    if (xangle < yangle)
        return -1;
    if (xangle > yangle)
        return 1;
    return 0;
}

/* owner->ne requirements: NONE */
static gdouble
surface_flat(G_GNUC_UNUSED const VoronoiCoords *point,
             const VoronoiObject *owner,
             G_GNUC_UNUSED const LatSynthArgs *args)
{
    gdouble r;

    r = owner->random;

    return r;
}

/* Returns TRUE if owner does not change and we can assume everything is
 * neighbourised.  FALSE is returned if we moved to another cell. */
static gboolean
find_delaunay_triangle(const VoronoiCoords *point,
                       const VoronoiObject **owner,
                       const VoronoiObject **neigh1,
                       const VoronoiObject **neigh2)
{
    VoronoiCoords dist;
    const VoronoiCoords *v1, *v2, *v;
    const VoronoiObject *pivot;
    GSList *ne1, *ne2, *ne;
    gdouble cp1, cp2;
    guint iter = 0;

    /* Find the two neighbours that bracket the direction to the point. */
    dist = coords_minus(point, &((*owner)->pos));
    ne1 = (*owner)->ne;
    ne2 = ne1->next;
    while (TRUE) {
        v1 = &VOBJ(ne1)->rel.v;
        v2 = &VOBJ(ne2)->rel.v;
        if ((cp1 = v1->x*dist.y - v1->y*dist.x) >= 0
            && (cp2 = dist.x*v2->y - dist.y*v2->x) >= 0)
            break;
        ne1 = ne2;
        ne2 = ne2->next;
    }

    if (v1->x*v2->y - v1->y*v2->x - cp1 - cp2 >= 0.0) {
        /* OK, we are inside the right Delaunay triangle. */
        *neigh1 = VOBJ(ne1);
        *neigh2 = VOBJ(ne2);
        return TRUE;
    }

    /* We are not.  The somewhat slower path is to check the opposite cell that
     * also has ne1 and ne2 neighbours. */
    while (TRUE) {
        VoronoiCoords tdist;
        gdouble a1, a2, a12;

        /* Find ne1 and the third point (ne) in the neighbour list of ne2. */
        pivot = VOBJ(ne2);
        for (ne = pivot->ne;
             (const VoronoiObject*)VOBJ(ne) != VOBJ(ne1);
             ne = ne->next)
            ;
        ne1 = ne;
        ne2 = ne->next;

        v = &pivot->pos;
        v1 = &VOBJ(ne1)->pos;
        v2 = &VOBJ(ne2)->pos;

        dist = coords_minus(point, v);
        tdist = coords_minus(v1, v);
        a1 = tdist.x*dist.y - tdist.y*dist.x;
        /* Are both sides of the line the wrong side?  Well...  Probably we are
         * almost exactly on that line so nothing bad will happen if we just
         * give up.  Seems very rare in practice. */
        if (G_UNLIKELY(a1 < 0))
            break;

        dist = coords_minus(point, v1);
        tdist = coords_minus(v2, v1);
        a12 = tdist.x*dist.y - tdist.y*dist.x;

        dist = coords_minus(point, v2);
        tdist = coords_minus(v, v2);
        a2 = tdist.x*dist.y - tdist.y*dist.x;

        if (a2 >= 0.0 && a12 >= 0.0)
            break;

        if (a12 >= 0.0) {
            ne1 = ne2;
            ne2 = ne;
        }
        else if (a2 >= 0.0) {
            /* ne1 and ne2 are already set as expected */
        }
        else {
            /* Is out point really in the shadow of ne2?  Well...  Just move
             * on.  A more sophisticated decision method could be used here but
             * this again is almost impossible to trigger. */
            if (a2 < a1) {
                ne1 = ne2;
                ne2 = ne;
            }
        }

        /* Safety measure.  Seems very rare in practice. */
        if (++iter == 8)
            break;
    }

    *owner = pivot;   /* Does not mean anything, just the third vertex. */
    *neigh1 = VOBJ(ne1);
    *neigh2 = VOBJ(ne2);

    return FALSE;
}

/* owner->ne requirements: cyclic, neighbourized, segment angles */
static gdouble
surface_linear(const VoronoiCoords *point, const VoronoiObject *owner,
               G_GNUC_UNUSED const LatSynthArgs *args)
{
    const VoronoiObject *neigh1, *neigh2;
    VoronoiCoords dist, v1, v2;
    gdouble r, D, c1, c2, c;

    if (find_delaunay_triangle(point, &owner, &neigh1, &neigh2)) {
        v1 = neigh1->rel.v;
        v2 = neigh2->rel.v;
    }
    else {
        v1 = coords_minus(&neigh1->pos, &owner->pos);
        v2 = coords_minus(&neigh2->pos, &owner->pos);
    }

    dist = coords_minus(point, &owner->pos);
    D = v1.x*v2.y - v1.y*v2.x;
    c1 = -(v2.x*dist.y - v2.y*dist.x)/D;
    c2 = (v1.x*dist.y - v1.y*dist.x)/D;
    c = 1.0 - (c2 + c1);
    r = c*owner->random + c1*neigh1->random + c2*neigh2->random;

    return r;
}

/* owner->ne requirements: cyclic, neighbourized, segment angles */
static gdouble
surface_bumpy(const VoronoiCoords *point, const VoronoiObject *owner,
              G_GNUC_UNUSED const LatSynthArgs *args)
{
    const VoronoiObject *neigh1, *neigh2;
    VoronoiCoords dist, v1, v2;
    gdouble r, D, c1, c2, c, cs;

    if (find_delaunay_triangle(point, &owner, &neigh1, &neigh2)) {
        v1 = neigh1->rel.v;
        v2 = neigh2->rel.v;
    }
    else {
        v1 = coords_minus(&neigh1->pos, &owner->pos);
        v2 = coords_minus(&neigh2->pos, &owner->pos);
    }

    dist = coords_minus(point, &owner->pos);
    D = v1.x*v2.y - v1.y*v2.x;
    c1 = -(v2.x*dist.y - v2.y*dist.x)/D;
    c2 = (v1.x*dist.y - v1.y*dist.x)/D;
    c = 1.0 - (c2 + c1);
    c1 *= c1*c1;
    c2 *= c2*c2;
    c *= c*c;
    cs = c + c1 + c2;
    r = (c*owner->random + c1*neigh1->random + c2*neigh2->random)/cs;

    return r;
}

/* owner->ne requirements: NONE */
static gdouble
surface_radial(const VoronoiCoords *point, const VoronoiObject *owner,
               const LatSynthArgs *args)
{
    VoronoiCoords dist;
    gdouble r;

    dist = coords_minus(point, &owner->pos);
    r = args->scale*sqrt(DOTPROD_SS(dist, dist));

    return r;
}

/* owner->ne requirements: cyclic, neighbourized, segment angles */
static gdouble
surface_segmented(const VoronoiCoords *point, const VoronoiObject *owner,
                  G_GNUC_UNUSED const LatSynthArgs *args)
{
    VoronoiCoords dist;
    gdouble r, phi;
    GSList *ne;

    ne = owner->ne;
    dist = coords_minus(point, &owner->pos);
    phi = angle(&dist);

    while ((phi >= VOBJ(ne)->angle)
           + (phi < VOBJ(ne->next)->angle)
           + (VOBJ(ne)->angle > VOBJ(ne->next)->angle) < 2)
        ne = ne->next;

    r = 2*DOTPROD_SS(dist, VOBJ(ne)->rel.v)/VOBJ(ne)->rel.d;

    return r;
}

/* owner->ne requirements: cyclic, neighbourized, segment angles */
static gdouble
surface_zsegmented(const VoronoiCoords *point, const VoronoiObject *owner,
                   G_GNUC_UNUSED const LatSynthArgs *args)
{
    VoronoiCoords dist;
    gdouble r, phi;
    GSList *ne;

    ne = owner->ne;
    dist = coords_minus(point, &owner->pos);
    phi = angle(&dist);

    while ((phi >= VOBJ(ne)->angle)
           + (phi < VOBJ(ne->next)->angle)
           + (VOBJ(ne)->angle > VOBJ(ne->next)->angle) < 2)
        ne = ne->next;

    r = owner->random*(2*DOTPROD_SS(dist, VOBJ(ne)->rel.v)/VOBJ(ne)->rel.d - 1);

    return r;
}

/* owner->ne requirements: neighbourized */
static gdouble
surface_border(const VoronoiCoords *point, const VoronoiObject *owner,
               const LatSynthArgs *args)
{
    VoronoiCoords dist;
    gdouble r, r_min;
    GSList *ne;

    dist = coords_minus(point, &owner->pos);
    r_min = HUGE_VAL;

    for (ne = owner->ne; ; ne = ne->next) {
        r = fabs(VOBJ(ne)->rel.d/2 - DOTPROD_SS(dist, VOBJ(ne)->rel.v))
            /sqrt(VOBJ(ne)->rel.d);
        if (r < r_min)
            r_min = r;
        if (ne->next == owner->ne)
            break;
    }

    r = 1 - 2*r_min*args->scale;

    return r;
}

/* owner->ne requirements: neighbourized */
static gdouble
surface_zborder(const VoronoiCoords *point, const VoronoiObject *owner,
                const LatSynthArgs *args)
{
    VoronoiCoords dist;
    gdouble r, r_min;
    GSList *ne;

    dist = coords_minus(point, &owner->pos);
    r_min = HUGE_VAL;

    for (ne = owner->ne; ; ne = ne->next) {
        r = fabs(VOBJ(ne)->rel.d/2 - DOTPROD_SS(dist, VOBJ(ne)->rel.v))
            /sqrt(VOBJ(ne)->rel.d);
        if (r < r_min)
            r_min = r;
        if (ne->next == owner->ne)
            break;
    }

    r = 1 - 2*r_min*args->scale*owner->random;

    return r;
}

/* owner->ne requirements: NONE */
static gdouble
surface_second(const VoronoiCoords *point, const VoronoiObject *owner,
               const LatSynthArgs *args)
{
    VoronoiCoords dist;
    gdouble r, r_min;
    GSList *ne;

    r_min = HUGE_VAL;

    for (ne = owner->ne; ; ne = ne->next) {
        dist = coords_minus(point, &VOBJ(ne)->pos);
        r = DOTPROD_SS(dist, dist);
        if (r < r_min)
            r_min = r;
        if (ne->next == owner->ne)
            break;
    }

    r = 1 - sqrt(r_min)*args->scale;

    return r;
}

/* compute segment angles
 * more precisely, VOBJ(ne)->angle will be set to start angle for segment
 * from ne to ne->next (so end angle is in ne->next)
 *
 * ne0 requirements: cyclic and neighbourized */
static void
compute_segment_angles(GSList *ne0)
{
    GSList *ne;
    VoronoiObject *p, *q;
    VoronoiCoords z;

    ne = ne0;
    do {
        p = VOBJ(ne);
        q = VOBJ(ne->next);
        z.x = p->rel.d * q->rel.v.y - q->rel.d * p->rel.v.y;
        z.y = q->rel.d * p->rel.v.x - p->rel.d * q->rel.v.x;
        q->angle = angle(&z);
        ne = g_slist_next(ne);
    } while (ne != ne0);
}

/* calculate intersection time t for intersection of lines:
 *
 * r = linevec*t + start
 * |r - a| = |r - b|
 */
static inline gdouble
intersection_time(const VoronoiCoords *a, const VoronoiCoords *b,
                  const VoronoiCoords *linevec, const VoronoiCoords *start)
{
    VoronoiCoords p, q;
    gdouble s;

    /* line dividing a-neighbourhood and b-neighbourhood */
    q = coords_minus(b, a);
    p = coords_plus(b, a);

    /* XXX: can be numerically unstable */
    s = DOTPROD_SP(q, linevec);
    if (fabs(s) < 1e-14)
        s = 1e-14; /* better than nothing */
    return (DOTPROD_SS(q, p)/2 - DOTPROD_SP(q, start))/s;
}

/* being in point start owned by owner (XXX: this condition MUST be true)
 * we want to get to point end and know our new owner
 * returns the new owner; in addition, when next_safe is not NULL it stores
 * there number of times we can repeat move along (end - start) vector still
 * remaining in the new owner */
static VoronoiObject*
move_along_line(const VoronoiObject *owner,
                const VoronoiCoords *start,
                const VoronoiCoords *end, gint *next_safe)
{
    VoronoiCoords linevec;
    VoronoiObject *ow;
    GSList *ne, *nearest = NULL;
    gdouble t, t_min, t_back;

    ow = (VoronoiObject*)owner;
    linevec = coords_minus(end, start);
    t_back = 0;
    /* XXX: start must be owned by owner, or else strange things will happen */
    while (TRUE) {
        t_min = HUGE_VAL;
        ne = ow->ne;
        do {
            /* find intersection with border line between ow and ne
             * FIXME: there apparently exist values t > t_back && t_back > t */
            t = intersection_time(&ow->pos, &VOBJ(ne)->pos, &linevec, start);
            if (t - t_back >= EPS && t < t_min) {
                t_min = t;
                nearest = ne;
            }
            ne = ne->next;
        } while (ne != ow->ne);

        /* no intersection inside the abscissa? then we are finished and can
           compute how many steps the same direction will remain in ow's
           neighbourhood */
        if (t_min > 1) {
            if (next_safe == NULL)
                return ow;
            if (t_min == HUGE_VAL)
                *next_safe = G_MAXINT;
            else
                *next_safe = floor(t_min) - 1;
            return ow;
        }

        /* otherwise nearest intersection determines a new owner */
        ow = VOBJ(nearest);
        t_back = t_min; /* time value showing we are going back */
    }
}

/* find and return the owner of a point
 * NB: this is crude and should not be used for anything else than initial
 * grip, use move_along_line() then
 * works for both cyclic and noncyclic ne-> */
static VoronoiObject*
find_owner(VoronoiState *vstate, const VoronoiCoords *point)
{
    GSList *ne, **squares = vstate->squares;
    VoronoiObject *owner = NULL;
    VoronoiCoords dist;
    gint jx, jy;
    gint ix, iy;
    gint wsq = vstate->wsq, hsq = vstate->hsq;
    gint extwsq = wsq + 2*SQBORDER;
    gdouble norm_min;

    jx = floor(point->x);
    jy = floor(point->y);

    /* These might be slightly non-true due to rounding errors.  Use clamps
     * in production code. */
#ifdef DEBUG
    g_return_val_if_fail(jx >= SQBORDER, NULL);
    g_return_val_if_fail(jy >= SQBORDER, NULL);
    g_return_val_if_fail(jx < wsq + SQBORDER, NULL);
    g_return_val_if_fail(jy < hsq + SQBORDER, NULL);
#endif
    jx = CLAMP(jx, SQBORDER, wsq + SQBORDER-1);
    jy = CLAMP(jy, SQBORDER, hsq + SQBORDER-1);

    /* scan the 25-neighbourhood */
    norm_min = HUGE_VAL;
    for (ix = -SQBORDER; ix <= SQBORDER; ix++) {
        gint x = jx + ix;
        for (iy = -SQBORDER; iy <= SQBORDER; iy++) {
            gint y = jy + iy;
            gint k = y*extwsq + x;
            for (ne = squares[k]; ne != NULL; ne = ne->next) {
                dist = coords_minus(&VOBJ(ne)->pos, point);
                if (DOTPROD_SS(dist, dist) < norm_min) {
                    norm_min = DOTPROD_SS(dist, dist);
                    owner = VOBJ(ne);
                }
                if (ne->next == squares[k])
                    break;
            }
        }
    }

    return owner;
}

/* compute angles from rel.v relative coordinates
 *
 * ne0 requirements: neighbourized */
static void
compute_straight_angles(GSList *ne0)
{
    GSList *ne;
    VoronoiObject *p;

    for (ne = ne0; ne; ne = g_slist_next(ne)) {
        p = VOBJ(ne);
        p->angle = angle(&p->rel.v);
        if (ne->next == ne0)
            return;
    }
}

/* compute relative positions and norms to center center
 *
 * ne0 requirements: NONE */
static void
neighbourize(GSList *ne0, const VoronoiCoords *center)
{
    GSList *ne;

    for (ne = ne0; ne; ne = g_slist_next(ne)) {
        VoronoiObject *p = VOBJ(ne);

        p->rel.v = coords_minus(&p->pos, center);
        p->rel.d = DOTPROD_SS(p->rel.v, p->rel.v);
        if (ne->next == ne0)
            return;
    }
}

/* return true iff point z (given as VoronoiLine) is shadowed by points a and b
 * (XXX: all coordiantes are relative) */
static inline gboolean
in_shadow(const VoronoiLine *a, const VoronoiLine *b, const VoronoiCoords *z)
{
    VoronoiCoords r, oa, ob, rz;
    gdouble s;

    /* Artifical fix for periodic grids, because in Real World This Just Does
     * Not Happen; also mitigates the s == 0 case below, as the offending point
     * would be probably removed here. */
    if (DOTPROD_SP(a->v, z) > 1.01*a->d
        && fabs(a->v.x * z->y - z->x * a->v.y) < 1e-12)
        return TRUE;
    if (DOTPROD_SP(b->v, z) > 1.01*b->d
        && fabs(b->v.x * z->y - z->x * b->v.y) < 1e-12)
        return TRUE;

    s = 2*(a->v.x * b->v.y - b->v.x * a->v.y);
    /* FIXME: what to do when s == 0 (or very near)??? */
    r.x = (a->d * b->v.y - b->d * a->v.y)/s;
    r.y = (b->d * a->v.x - a->d * b->v.x)/s;
    oa.x = -a->v.y;
    oa.y = a->v.x;
    ob.x = -b->v.y;
    ob.y = b->v.x;
    rz = coords_minus(z, &r);
    return (DOTPROD_SS(rz, rz) > DOTPROD_SS(r, r)
            && DOTPROD_PS(z, oa)*DOTPROD_SS(b->v, oa) > 0
            && DOTPROD_PS(z, ob)*DOTPROD_SS(a->v, ob) > 0);
}

static GSList*
extract_neighbourhood(GSList **squares,
                      gint wsq, gint hsq,
                      VoronoiObject *p)
{
    GSList *ne = NULL;
    gint jx, jy;
    gint ix, iy;
    gint xwsq, xhsq;

    xwsq = wsq + 2*SQBORDER;
    xhsq = hsq + 2*SQBORDER;

    jx = floor(p->pos.x);
    jy = floor(p->pos.y);

    /* construct the 37-neighbourhood list */
    for (ix = -3; ix <= 3; ix++) {
        gint x = jx + ix;
        if (x < 0 || x >= xwsq)
            continue;
        for (iy = -3; iy <= 3; iy++) {
            gint y = jy + iy;
            if ((ix == 3 || ix == -3) && (iy == 3 || iy == -3))
                continue;
            if (y < 0 || y >= xhsq)
                continue;
            ne = g_slist_concat(g_slist_copy(squares[y*xwsq + x]), ne);
            if (ix == 0 && iy == 0)
                ne = g_slist_remove(ne, p);
        }
    }

    g_assert(ne != NULL);

    /* compute relative coordinates and angles */
    neighbourize(ne, &p->pos);
    compute_straight_angles(ne);

    return ne;
}

static GSList*
shadow_filter(GSList *ne)
{
    GSList *ne1, *ne2;
    gint notremoved;
    gint len;

    if (ne == NULL)
        return ne;

    /* make the list cyclic if it isn't already
     * (we have to unlink elements ourself then) */
    len = 1;
    for (ne2 = ne; ne2->next && ne2->next != ne; ne2 = g_slist_next(ne2))
        len++;
    if (len < 3)
        return ne;
    ne2->next = ne;

    /* remove objects shadowed by their ancestors and successors
     * XXX: in non-degenerate case this is O(n*log(n)), but can be O(n*n) */
    ne1 = ne;
    notremoved = 0;
    do {
        ne2 = ne1->next;
        if (in_shadow(&VOBJ(ne1)->rel,
                      &VOBJ(ne2->next)->rel,
                      &VOBJ(ne2)->rel.v)) {
            ne1->next = ne2->next;
            g_slist_free_1(ne2);
            notremoved = 0;
            len--;
        }
        else {
            ne1 = ne2;
            notremoved++;
        }
    } while (notremoved < len && len > 2);

    return ne1; /* return cyclic list */
}

static void
find_voronoi_neighbours_iter(VoronoiState *vstate, gint iter)
{
    GSList *this;

    for (this = vstate->squares[iter]; this; this = g_slist_next(this)) {
        VoronoiObject *obj = VOBJ(this);

        obj->ne = extract_neighbourhood(vstate->squares,
                                        vstate->wsq, vstate->hsq, obj);
        obj->ne = g_slist_sort(obj->ne, &vobj_angle_compare);
        obj->ne = shadow_filter(obj->ne);
    }
}

static void
voronoi_state_free(VoronoiState *vstate)
{
    GSList *l;
    guint extwsq, exthsq, i;

    gwy_rand_gen_set_free(vstate->rngset);

    extwsq = vstate->wsq + 2*SQBORDER;
    exthsq = vstate->hsq + 2*SQBORDER;

    /* Neighbourhoods. */
    for (i = 0; i < extwsq*exthsq; i++) {
        for (l = vstate->squares[i]; l; l = g_slist_next(l)) {
            if (l && l->data && VOBJ(l)->ne) {
                GSList *ne = VOBJ(l)->ne->next;
                VOBJ(l)->ne->next = NULL; /* break cycles */
                g_slist_free(ne);
            }
        }
    }

    /* Grid contents. */
    for (i = 0; i < extwsq*exthsq; i++) {
        for (l = vstate->squares[i]; l; l = g_slist_next(l))
            g_slice_free(VoronoiObject, l->data);
        g_slist_free(vstate->squares[i]);
    }
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
static const gchar enabled_key[]      = "enabled";
static const gchar weight_key[]       = "weight";
static const gchar lower_key[]        = "lower";
static const gchar upper_key[]        = "upper";

static const gchar *surface_keys[] = {
    "flat", "linear", "bumpy", "radial",
    "segmented", "zsegmented", "border", "zborder",
    "second",
};

static void
lat_synth_sanitize_args(LatSynthArgs *args)
{
    guint i;

    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = !!args->update;
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->lattice_type = MIN(args->lattice_type, LAT_SYNTH_NTYPES-1);
    args->size = CLAMP(args->size, 4.0, 1000.0);
    args->angle = CLAMP(args->angle, -G_PI, G_PI);
    args->sigma = CLAMP(args->sigma, 0.0, 100.0);
    args->tau = CLAMP(args->sigma, 0.1, 1000.0);
    for (i = 0; i < G_N_ELEMENTS(args->weight); i++) {
        args->weight[i]= CLAMP(args->weight[i], -1.0, 1.0);
        args->enabled[i] = !!args->enabled[i];
        args->lower[i] = CLAMP(args->lower[i], 0.0, 1.0);
        args->upper[i] = CLAMP(args->upper[i], 0.0, 1.0);
    }
}

static void
lat_synth_load_args(GwyContainer *container,
                    LatSynthArgs *args,
                    GwyDimensionArgs *dimsargs)
{
    GString *str;
    guint i;

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
    str = g_string_new(NULL);
    for (i = 0; i < G_N_ELEMENTS(args->weight); i++) {
        guint len;

        g_string_assign(str, prefix);
        g_string_append_c(str, '/');
        g_string_append(str, surface_keys[i]);
        g_string_append_c(str, '/');
        len = str->len;

        g_string_truncate(str, len);
        g_string_append(str, enabled_key);
        gwy_container_gis_boolean_by_name(container, str->str, args->enabled + i);

        g_string_truncate(str, len);
        g_string_append(str, weight_key);
        gwy_container_gis_double_by_name(container, str->str, args->weight + i);

        g_string_truncate(str, len);
        g_string_append(str, lower_key);
        gwy_container_gis_double_by_name(container, str->str, args->lower + i);

        g_string_truncate(str, len);
        g_string_append(str, upper_key);
        gwy_container_gis_double_by_name(container, str->str, args->upper + i);
    }
    g_string_free(str, TRUE);
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
    GString *str;
    guint i;

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
    str = g_string_new(NULL);
    for (i = 0; i < G_N_ELEMENTS(args->weight); i++) {
        guint len;
        g_string_assign(str, prefix);
        g_string_append_c(str, '/');
        g_string_append(str, surface_keys[i]);
        g_string_append_c(str, '/');
        len = str->len;

        g_string_truncate(str, len);
        g_string_append(str, enabled_key);
        gwy_container_set_boolean_by_name(container, str->str, args->enabled[i]);

        g_string_truncate(str, len);
        g_string_append(str, weight_key);
        gwy_container_set_double_by_name(container, str->str, args->weight[i]);

        g_string_truncate(str, len);
        g_string_append(str, lower_key);
        gwy_container_set_double_by_name(container, str->str, args->lower[i]);

        g_string_truncate(str, len);
        g_string_append(str, upper_key);
        gwy_container_set_double_by_name(container, str->str, args->upper[i]);
    }
    g_string_free(str, TRUE);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
