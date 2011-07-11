/*
 *  @(#) $Id$
 *  Copyright (C) 2009-2011 David Necas (Yeti).
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

#define OBJ_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define DECLARE_OBJECT(name) \
    static void create_##name(ObjSynthObject *feature, gdouble size, \
                              gdouble aspect, gdouble angle); \
    static gdouble getcov_##name(gdouble aspect)

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
    RNG_ID,
    RNG_SIZE,
    RNG_ASPECT,
    RNG_HEIGHT,
    RNG_ANGLE,
    RNG_HTRUNC,
    RNG_NRNGS
} ObjSynthRng;

typedef enum {
    OBJ_SYNTH_SPHERE   = 0,
    OBJ_SYNTH_PYRAMID  = 1,
    OBJ_SYNTH_NUGGET   = 2,
    OBJ_SYNTH_THATCH   = 3,
    OBJ_SYNTH_DOUGHNUT = 4,
    OBJ_SYNTH_4HEDRON  = 5,
    OBJ_SYNTH_BOX      = 6,
    OBJ_SYNTH_CONE     = 7,
    OBJ_SYNTH_TENT     = 8,
    OBJ_SYNTH_DIAMOND  = 9,
    OBJ_SYNTH_GAUSSIAN = 10,
    OBJ_SYNTH_NTYPES
} ObjSynthType;

typedef struct _ObjSynthControls ObjSynthControls;

/* Avoid reallocations by using a single buffer for objects that can only
 * grow. */
typedef struct {
    gint xres;
    gint yres;
    gsize size;
    gdouble *data;
} ObjSynthObject;

typedef void (*CreateFeatureFunc)(ObjSynthObject *feature,
                                  gdouble size,
                                  gdouble aspect,
                                  gdouble angle);
typedef gdouble (*GetCoverageFunc)(gdouble aspect);

/* This scheme makes the object type list easily reordeable in the GUI without
 * changing the ids.  */
typedef struct {
    ObjSynthType type;
    const gchar *name;
    CreateFeatureFunc create;
    GetCoverageFunc get_coverage;
} ObjSynthFeature;

typedef struct {
    guint n;
    GRand **rng;
    gboolean *have_spare;
    gdouble *spare;
} RandGenSet;

typedef struct {
    gint active_page;
    gint seed;
    gboolean randomize;
    gboolean update;
    ObjSynthType type;
    gdouble size;
    gdouble size_noise;
    gdouble aspect;
    gdouble aspect_noise;
    gdouble height;
    gboolean height_bound;
    gdouble height_noise;
    gdouble angle;
    gdouble angle_noise;
    gdouble htrunc;
    gdouble htrunc_noise;
    gdouble coverage;
} ObjSynthArgs;

struct _ObjSynthControls {
    ObjSynthArgs *args;
    GwyDimensions *dims;
    RandGenSet *rngset;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkTable *table;
    GtkWidget *type;
    GtkObject *size;
    GtkWidget *size_value;
    GtkWidget *size_units;
    GtkObject *size_noise;
    GtkObject *aspect;
    GtkObject *aspect_noise;
    GtkObject *height;
    GtkWidget *height_units;
    GtkWidget *height_init;
    GtkWidget *height_bound;
    GtkObject *height_noise;
    GtkObject *angle;
    GtkObject *angle_noise;
    GtkObject *htrunc;
    GtkObject *htrunc_noise;
    GtkObject *coverage;
    GtkWidget *coverage_value;
    GtkWidget *coverage_units;
    GwyContainer *mydata;
    GwyDataField *surface;
    gdouble pxsize;
    gdouble zscale;
    gboolean in_init;
    gulong sid;
};

static gboolean    module_register      (void);
static void        obj_synth            (GwyContainer *data,
                                         GwyRunType run);
static void        run_noninteractive   (ObjSynthArgs *args,
                                         const GwyDimensionArgs *dimsargs,
                                         RandGenSet *rngset,
                                         GwyContainer *data,
                                         GwyDataField *dfield,
                                         gint oldid,
                                         GQuark quark);
static gboolean    obj_synth_dialog     (ObjSynthArgs *args,
                                         GwyDimensionArgs *dimsargs,
                                         RandGenSet *rngset,
                                         GwyContainer *data,
                                         GwyDataField *dfield,
                                         gint id);
static GtkWidget*  feature_selector_new (ObjSynthControls *controls);
static void        update_controls      (ObjSynthControls *controls,
                                         ObjSynthArgs *args);
static void        page_switched        (ObjSynthControls *controls,
                                         GtkNotebookPage *page,
                                         gint pagenum);
static void        update_values        (ObjSynthControls *controls);
static void        feature_type_selected(GtkComboBox *combo,
                                         ObjSynthControls *controls);
static void        height_init_clicked  (ObjSynthControls *controls);
static gint        attach_truncation    (ObjSynthControls *controls,
                                         gint row,
                                         GtkObject **adj,
                                         gdouble *target);
static void        update_coverage_value(ObjSynthControls *controls);
static void        obj_synth_invalidate (ObjSynthControls *controls);
static gboolean    preview_gsource      (gpointer user_data);
static void        preview              (ObjSynthControls *controls);
static void        obj_synth_do         (const ObjSynthArgs *args,
                                         const GwyDimensionArgs *dimsargs,
                                         RandGenSet *rngset,
                                         GwyDataField *dfield);
static void        object_synth_iter    (GwyDataField *surface,
                                         ObjSynthObject *object,
                                         const ObjSynthArgs *args,
                                         const GwyDimensionArgs *dimsargs,
                                         RandGenSet *rngset,
                                         gint nxcells,
                                         gint nycells,
                                         gint xoff,
                                         gint yoff,
                                         gint nobjects,
                                         gint *indices);
static void        place_add_min        (GwyDataField *surface,
                                         ObjSynthObject *object,
                                         gint col,
                                         gint row);
static glong       calculate_n_objects  (const ObjSynthArgs *args,
                                         guint xres,
                                         guint yres);
static RandGenSet* rand_gen_set_new     (guint n);
static void        rand_gen_set_init    (RandGenSet *rngset,
                                         guint seed);
static void        rand_gen_set_free    (RandGenSet *rngset);
static void        obj_synth_load_args  (GwyContainer *container,
                                         ObjSynthArgs *args,
                                         GwyDimensionArgs *dimsargs);
static void        obj_synth_save_args  (GwyContainer *container,
                                         const ObjSynthArgs *args,
                                         const GwyDimensionArgs *dimsargs);

#define GWY_SYNTH_CONTROLS ObjSynthControls
#define GWY_SYNTH_INVALIDATE(controls) \
    update_coverage_value(controls); \
    obj_synth_invalidate(controls)

#include "synth.h"

DECLARE_OBJECT(sphere);
DECLARE_OBJECT(pyramid);
DECLARE_OBJECT(box);
DECLARE_OBJECT(nugget);
DECLARE_OBJECT(thatch);
DECLARE_OBJECT(doughnut);
DECLARE_OBJECT(thedron);
DECLARE_OBJECT(cone);
DECLARE_OBJECT(tent);
DECLARE_OBJECT(diamond);
DECLARE_OBJECT(gaussian);

static const ObjSynthArgs obj_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    OBJ_SYNTH_SPHERE,
    20.0, 0.0,
    1.0, 0.0,
    1.0, TRUE, 0.0,
    0.0, 0.0,
    1.0, 0.0,
    1.0,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static const ObjSynthFeature features[] = {
    { OBJ_SYNTH_SPHERE,   N_("Spheres"),      &create_sphere,   &getcov_sphere,   },
    { OBJ_SYNTH_BOX,      N_("Boxes"),        &create_box,      &getcov_box,      },
    { OBJ_SYNTH_CONE,     N_("Cones"),        &create_cone,     &getcov_cone,     },
    { OBJ_SYNTH_PYRAMID,  N_("Pyramids"),     &create_pyramid,  &getcov_pyramid,  },
    { OBJ_SYNTH_DIAMOND,  N_("Diamonds"),     &create_diamond,  &getcov_diamond,  },
    { OBJ_SYNTH_4HEDRON,  N_("Tetrahedrons"), &create_thedron,  &getcov_thedron,  },
    { OBJ_SYNTH_NUGGET,   N_("Nuggets"),      &create_nugget,   &getcov_nugget,   },
    { OBJ_SYNTH_THATCH,   N_("Thatches"),     &create_thatch,   &getcov_thatch,   },
    { OBJ_SYNTH_TENT,     N_("Tents"),        &create_tent,     &getcov_tent,     },
    { OBJ_SYNTH_GAUSSIAN, N_("Gaussians"),    &create_gaussian, &getcov_gaussian, },
    { OBJ_SYNTH_DOUGHNUT, N_("Dougnuts"),     &create_doughnut, &getcov_doughnut, },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates randomly patterned surfaces by placing objects."),
    "Yeti <yeti@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("obj_synth",
                              (GwyProcessFunc)&obj_synth,
                              N_("/S_ynthetic/_Objects..."),
                              NULL,
                              OBJ_SYNTH_RUN_MODES,
                              0,
                              N_("Generate surface of randomly placed objects"));

    return TRUE;
}

static void
obj_synth(GwyContainer *data, GwyRunType run)
{
    ObjSynthArgs args;
    GwyDimensionArgs dimsargs;
    RandGenSet *rngset;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & OBJ_SYNTH_RUN_MODES);
    obj_synth_load_args(gwy_app_settings_get(), &args, &dimsargs);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);

    rngset = rand_gen_set_new(RNG_NRNGS);
    if (run == GWY_RUN_IMMEDIATE
        || obj_synth_dialog(&args, &dimsargs, rngset, data, dfield, id))
        run_noninteractive(&args, &dimsargs, rngset, data, dfield, id, quark);

    if (run == GWY_RUN_INTERACTIVE)
        obj_synth_save_args(gwy_app_settings_get(), &args, &dimsargs);

    rand_gen_set_free(rngset);
    gwy_dimensions_free_args(&dimsargs);
}

static void
run_noninteractive(ObjSynthArgs *args,
                   const GwyDimensionArgs *dimsargs,
                   RandGenSet *rngset,
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

    obj_synth_do(args, dimsargs, rngset, dfield);

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
obj_synth_dialog(ObjSynthArgs *args,
                 GwyDimensionArgs *dimsargs,
                 RandGenSet *rngset,
                 GwyContainer *data,
                 GwyDataField *dfield_template,
                 gint id)
{
    GtkWidget *dialog, *table, *vbox, *hbox, *notebook;
    ObjSynthControls controls;
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
    dialog = gtk_dialog_new_with_buttons(_("Random Objects"),
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
                                 G_CALLBACK(obj_synth_invalidate), &controls);

    table = gtk_table_new(19 + (dfield_template ? 1 : 0), 4, FALSE);
    controls.table = GTK_TABLE(table);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Generator")));
    row = 0;

    controls.type = feature_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Shape:"), NULL,
                            GTK_OBJECT(controls.type), GWY_HSCALE_WIDGET);
    row++;

    controls.coverage = gtk_adjustment_new(args->coverage,
                                           0.001, 12.0, 0.001, 1.0, 0);
    g_object_set_data(G_OBJECT(controls.coverage), "target", &args->coverage);
    gwy_table_attach_hscale(table, row, _("Co_verage:"), NULL,
                            controls.coverage, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.coverage, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    controls.coverage_value = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.coverage_value), 1.0, 0.5);
    gtk_table_attach(controls.table, controls.coverage_value,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    controls.coverage_units = gtk_label_new(_("obj."));
    gtk_misc_set_alignment(GTK_MISC(controls.coverage_units), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.coverage_units,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Size")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.size = gtk_adjustment_new(args->size, 1.0, 1000.0, 0.1, 10.0, 0);
    row = gwy_synth_attach_lateral(&controls, row, controls.size, &args->size,
                                   _("_Size:"), GWY_HSCALE_LOG,
                                   NULL,
                                   &controls.size_value, &controls.size_units);
    row = gwy_synth_attach_variance(&controls, row,
                                    &controls.size_noise, &args->size_noise);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(controls.table, gwy_label_new_header(_("Aspect Ratio")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.aspect = gtk_adjustment_new(args->aspect,
                                         0.2, 5.0, 0.01, 1.0, 0);
    g_object_set_data(G_OBJECT(controls.aspect), "target", &args->aspect);
    gwy_table_attach_hscale(table, row, _("_Aspect ratio:"), NULL,
                            controls.aspect, GWY_HSCALE_LOG);
    g_signal_connect_swapped(controls.aspect, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), &controls);
    row++;

    row = gwy_synth_attach_variance(&controls, row,
                          &controls.aspect_noise, &args->aspect_noise);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Height")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
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

    controls.height_bound
        = gtk_check_button_new_with_mnemonic(_("Scales _with size"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.height_bound),
                                 args->height_bound);
    g_object_set_data(G_OBJECT(controls.height_bound),
                      "target", &args->height_bound);
    gtk_table_attach(GTK_TABLE(table), controls.height_bound,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.height_bound, "toggled",
                             G_CALLBACK(gwy_synth_boolean_changed), &controls);
    row++;

    row = gwy_synth_attach_variance(&controls, row,
                                    &controls.height_noise,
                                    &args->height_noise);

    row = attach_truncation(&controls, row, &controls.htrunc, &args->htrunc);
    row = gwy_synth_attach_variance(&controls, row,
                                    &controls.htrunc_noise,
                                    &args->htrunc_noise);

    row = gwy_synth_attach_orientation(&controls, row,
                                       &controls.angle, &args->angle);
    row = gwy_synth_attach_variance(&controls, row,
                                    &controls.angle_noise, &args->angle_noise);

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    update_values(&controls);
    obj_synth_invalidate(&controls);

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
                *args = obj_synth_defaults;
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

static const ObjSynthFeature*
get_feature(guint type)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(features); i++) {
        if (features[i].type == type)
            return features + i;
    }
    g_warning("Unknown feature %u\n", type);

    return features + 0;
}

static GtkWidget*
feature_selector_new(ObjSynthControls *controls)
{
    GtkWidget *combo;
    GwyEnum *model;
    guint n, i;

    n = G_N_ELEMENTS(features);
    model = g_new(GwyEnum, n);
    for (i = 0; i < n; i++) {
        model[i].value = features[i].type;
        model[i].name = features[i].name;
    }

    combo = gwy_enum_combo_box_new(model, n,
                                   G_CALLBACK(feature_type_selected), controls,
                                   controls->args->type, TRUE);
    g_object_weak_ref(G_OBJECT(combo), (GWeakNotify)g_free, model);

    return combo;
}

static void
update_controls(ObjSynthControls *controls,
                ObjSynthArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->randomize),
                                 args->randomize);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->coverage),
                             args->coverage);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size), args->size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size_noise),
                             args->size_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->aspect), args->aspect);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->aspect_noise),
                             args->aspect_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height), args->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height_noise),
                             args->height_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->htrunc), args->htrunc);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->htrunc_noise),
                             args->htrunc_noise);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->angle), args->angle);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->angle_noise),
                             args->angle_noise);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->height_bound),
                                 args->height_bound);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->type), args->type);
}

static void
page_switched(ObjSynthControls *controls,
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
update_values(ObjSynthControls *controls)
{
    GwyDimensions *dims = controls->dims;

    controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
    if (controls->height_units)
        gtk_label_set_markup(GTK_LABEL(controls->height_units),
                             dims->zvf->units);
    gtk_label_set_markup(GTK_LABEL(controls->size_units),
                         dims->xyvf->units);

    gwy_synth_update_lateral(controls, GTK_ADJUSTMENT(controls->size));
    update_coverage_value(controls);
}

static void
feature_type_selected(GtkComboBox *combo,
                      ObjSynthControls *controls)
{
    controls->args->type = gwy_enum_combo_box_get_active(combo);
    update_coverage_value(controls);
    obj_synth_invalidate(controls);
}

static void
height_init_clicked(ObjSynthControls *controls)
{
    gdouble mag = pow10(controls->dims->args->zpow10);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height),
                             controls->zscale/mag);
}

static gint
attach_truncation(ObjSynthControls *controls,
                  gint row,
                  GtkObject **adj,
                  gdouble *target)
{
    GtkWidget *spin;

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    *adj = gtk_adjustment_new(*target,
                              0.001, 1.0, 0.001, 0.1, 0);
    g_object_set_data(G_OBJECT(*adj), "target", target);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, _("_Truncate:"), NULL, *adj,
                                   GWY_HSCALE_DEFAULT);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), controls);

    row++;

    return row;
}

static void
update_coverage_value(ObjSynthControls *controls)
{
    glong nobjects;
    guchar buf[32];

    if (controls->in_init)
        return;

    nobjects = calculate_n_objects(controls->args,
                                   controls->dims->args->xres,
                                   controls->dims->args->yres);
    g_snprintf(buf, sizeof(buf), "%ld", nobjects);
    gtk_label_set_text(GTK_LABEL(controls->coverage_value), buf);
}

static void
obj_synth_invalidate(ObjSynthControls *controls)
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
    ObjSynthControls *controls = (ObjSynthControls*)user_data;
    controls->sid = 0;

    preview(controls);

    return FALSE;
}

static void
preview(ObjSynthControls *controls)
{
    ObjSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    if (controls->dims->args->add && controls->surface)
        gwy_data_field_copy(controls->surface, dfield, FALSE);
    else
        gwy_data_field_clear(dfield);

    obj_synth_do(args, controls->dims->args, controls->rngset, dfield);
}

static void
obj_synth_do(const ObjSynthArgs *args,
             const GwyDimensionArgs *dimsargs,
             RandGenSet *rngset,
             GwyDataField *dfield)
{
    ObjSynthObject object = { 0, 0, 0, NULL };
    gint *indices;
    glong nobjects;
    gint xres, yres, nxcells, nycells, ncells, cellside, niters, i;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    cellside = sqrt(sqrt(xres*yres));
    nxcells = (xres + cellside-1)/cellside;
    nycells = (yres + cellside-1)/cellside;
    ncells = nxcells*nycells;
    nobjects = calculate_n_objects(args, xres, yres);
    niters = nobjects/ncells;

    rand_gen_set_init(rngset, args->seed);
    indices = g_new(gint, ncells);

    for (i = 0; i < niters; i++) {
        object_synth_iter(dfield, &object, args, dimsargs, rngset,
                          nxcells, nycells, i+1, i+1, ncells, indices);
    }
    object_synth_iter(dfield, &object, args, dimsargs, rngset,
                      nxcells, nycells, 0, 0, nobjects % ncells, indices);

    g_free(object.data);
    g_free(indices);

    gwy_data_field_data_changed(dfield);
}

static inline gdouble
rand_gen_set_gauss(RandGenSet *rngset,
                   guint i,
                   gdouble sigma)
{
    GRand *rng;
    gdouble x, y, w;

    if (rngset->have_spare[i]) {
        rngset->have_spare[i] = FALSE;
        return sigma*rngset->spare[i];
    }

    rng = rngset->rng[i];
    do {
        x = -1.0 + 2.0*g_rand_double(rng);
        y = -1.0 + 2.0*g_rand_double(rng);
        w = x*x + y*y;
    } while (w >= 1.0 || w == 0);

    w = sqrt(-2.0*log(w)/w);
    rngset->spare[i] = y*w;
    rngset->have_spare[i] = TRUE;

    return sigma*x*w;
}

static void
object_synth_iter(GwyDataField *surface,
                  ObjSynthObject *object,
                  const ObjSynthArgs *args,
                  const GwyDimensionArgs *dimsargs,
                  RandGenSet *rngset,
                  gint nxcells,
                  gint nycells,
                  gint xoff,
                  gint yoff,
                  gint nobjects,
                  gint *indices)
{
    gint xres, yres, ncells, k, l;
    const ObjSynthFeature *feature;

    g_return_if_fail(nobjects <= nxcells*nycells);

    feature = get_feature(args->type);
    xres = gwy_data_field_get_xres(surface);
    yres = gwy_data_field_get_yres(surface);
    ncells = nxcells*nycells;

    for (k = 0; k < ncells; k++)
        indices[k] = k;

    for (k = 0; k < nobjects; k++) {
        gdouble size, aspect, height, angle, htrunc;
        gint id, i, j, from, to;
        gdouble *p;

        id = g_rand_int_range(rngset->rng[RNG_ID], 0, ncells - k);
        i = indices[id]/nycells;
        j = indices[id] % nycells;
        indices[id] = indices[ncells-1 - k];

        size = args->size;
        if (args->size_noise)
            size *= exp(rand_gen_set_gauss(rngset, RNG_SIZE,
                                           args->size_noise));

        aspect = args->aspect;
        if (args->aspect_noise)
            aspect *= exp(rand_gen_set_gauss(rngset, RNG_ASPECT,
                                             args->aspect_noise));

        height = args->height * pow10(dimsargs->zpow10);
        if (args->height_bound)
            height *= size/args->size;
        if (args->height_noise)
            height *= exp(rand_gen_set_gauss(rngset, RNG_HEIGHT,
                                             args->height_noise));

        angle = args->angle;
        if (args->angle_noise)
            angle += rand_gen_set_gauss(rngset, RNG_ANGLE,
                                        2*args->angle_noise);

        // Use a specific distribution for htrunc.
        if (args->htrunc_noise) {
            gdouble q = exp(rand_gen_set_gauss(rngset, RNG_HTRUNC,
                                               args->htrunc_noise));
            htrunc = q/(q + 1.0/args->htrunc - 1.0);
        }
        else
            htrunc = args->htrunc;

        feature->create(object, size, aspect, angle);
        p = object->data;

        if (htrunc < 1.0) {
            for (l = object->xres*object->yres; l; l--, p++)
                *p = height*((*p < htrunc) ? *p : htrunc);
        }
        else {
            for (l = object->xres*object->yres; l; l--, p++)
                *p *= height;
        }

        from = (j*xres + nxcells/2)/nxcells;
        to = (j*xres + xres + nxcells/2)/nxcells;
        to = MIN(to, xres);
        j = from + xoff + g_rand_int_range(rngset->rng[RNG_ID], 0, to - from);

        from = (i*yres + nycells/2)/nycells;
        to = (i*yres + yres + nycells/2)/nycells;
        to = MIN(to, yres);
        i = from + yoff + g_rand_int_range(rngset->rng[RNG_ID], 0, to - from);

        place_add_min(surface, object, j, i);
    }
}

static inline void
obj_synth_object_resize(ObjSynthObject *object,
                        gint xres, gint yres)
{
    object->xres = xres;
    object->yres = yres;
    if ((guint)(xres*yres) > object->size) {
        g_free(object->data);
        object->data = g_new(gdouble, xres*yres);
        object->size = xres*yres;
    }
}

static void
create_sphere(ObjSynthObject *feature,
              gdouble size,
              gdouble aspect,
              gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size*sqrt(aspect);
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*hypot(a*c, b*s) + 1) | 1;
    yres = (gint)ceil(2*hypot(a*s, b*c) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = 1.0 - xc*xc - yc*yc;
            z[i*xres + j] = (r > 0.0) ? sqrt(r) : 0.0;
        }
    }
}

static void
create_pyramid(ObjSynthObject *feature,
               gdouble size,
               gdouble aspect,
               gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size*sqrt(aspect);
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*(a*fabs(c) + b*fabs(s)) + 1) | 1;
    yres = (gint)ceil(2*(a*fabs(s) + b*fabs(c)) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = 1.0 - MAX(fabs(xc), fabs(yc));
            z[i*xres + j] = MAX(r, 0.0);
        }
    }
}

static void
create_diamond(ObjSynthObject *feature,
               gdouble size,
               gdouble aspect,
               gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size*sqrt(aspect);
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*MAX(a*fabs(c), b*fabs(s)) + 1) | 1;
    yres = (gint)ceil(2*MAX(a*fabs(s), b*fabs(c)) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = 1.0 - (fabs(xc) + fabs(yc));
            z[i*xres + j] = MAX(r, 0.0);
        }
    }
}

static void
create_box(ObjSynthObject *feature,
           gdouble size,
           gdouble aspect,
           gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size*sqrt(aspect);
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*(a*fabs(c) + b*fabs(s)) + 1) | 1;
    yres = (gint)ceil(2*(a*fabs(s) + b*fabs(c)) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = 1.0 - MAX(fabs(xc), fabs(yc));
            z[i*xres + j] = (r >= 0.0) ? 1.0 : 0.0;
        }
    }
}

static void
create_tent(ObjSynthObject *feature,
            gdouble size,
            gdouble aspect,
            gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size*sqrt(aspect);
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*(a*fabs(c) + b*fabs(s)) + 1) | 1;
    yres = (gint)ceil(2*(a*fabs(s) + b*fabs(c)) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = 1.0 - fabs(yc);
            z[i*xres + j] = (fabs(xc) <= 1.0 && r > 0.0) ? r : 0.0;
        }
    }
}

static void
create_cone(ObjSynthObject *feature,
            gdouble size,
            gdouble aspect,
            gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size*sqrt(aspect);
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*hypot(a*c, b*s) + 1) | 1;
    yres = (gint)ceil(2*hypot(a*s, b*c) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = 1.0 - hypot(xc, yc);
            z[i*xres + j] = MAX(r, 0.0);
        }
    }
}

static void
create_nugget(ObjSynthObject *feature,
              gdouble size,
              gdouble aspect,
              gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc, excess;
    gint xres, yres, i, j;
    gdouble *z;

    if (aspect == 1.0) {
        create_sphere(feature, size, aspect, angle);
        return;
    }

    /* Ensure a > b */
    if (aspect < 1.0) {
        angle += G_PI/2.0;
        aspect = 1.0/aspect;
    }

    a = size*sqrt(aspect);
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    excess = aspect - 1.0;
    xres = (gint)ceil(2*((a - b)*fabs(c) + b) + 1) | 1;
    yres = (gint)ceil(2*((a - b)*fabs(s) + b) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/b;
            yc = (x*s + y*c)/b;
            xc = fabs(xc) - excess;
            if (xc < 0.0)
                xc = 0.0;
            r = 1.0 - xc*xc - yc*yc;
            z[i*xres + j] = (r > 0.0) ? sqrt(r) : 0.0;
        }
    }
}

static void
create_thatch(ObjSynthObject *feature,
              gdouble size,
              gdouble aspect,
              gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size*sqrt(aspect);
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*(a*fabs(c) + b*fabs(s)) + 1) | 1;
    yres = (gint)ceil(2*(a*fabs(s) + b*fabs(c)) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = ((x*c - y*s) - 0.3)/a;
            yc = (x*s + y*c)/b;
            r = 0.5 - 0.5*xc;
            if (r >= 0.0 && r <= 1.0)
                z[i*xres + j] = (fabs(yc) <= r) ? (1.0 - r) : 0.0;
            else
                z[i*xres + j] = 0.0;
        }
    }
}

static void
create_doughnut(ObjSynthObject *feature,
                gdouble size,
                gdouble aspect,
                gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size*sqrt(aspect);
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*hypot(a*c, b*s) + 1) | 1;
    yres = (gint)ceil(2*hypot(a*s, b*c) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = hypot(xc, yc) - 0.6;
            r = 1.0 - r*r/0.16;
            z[i*xres + j] = (r > 0.0) ? sqrt(r) : 0.0;
        }
    }
}

static void
create_gaussian(ObjSynthObject *feature,
                gdouble size,
                gdouble aspect,
                gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size*sqrt(aspect);
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(8*hypot(a*c, b*s) + 1) | 1;
    yres = (gint)ceil(8*hypot(a*s, b*c) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = exp(-4.0*(xc*xc + yc*yc));
            z[i*xres + j] = r;
        }
    }
}

static void
create_thedron(ObjSynthObject *feature,
               gdouble size,
               gdouble aspect,
               gdouble angle)
{
    gdouble a, b, c, s, r, xp, xm, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size*sqrt(aspect)*GWY_SQRT3/2.0;
    b = size/sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*(a*fabs(c) + b*fabs(s)) + 1) | 1;
    yres = (gint)ceil(2*(a*fabs(s) + b*fabs(c)) + 1) | 1;

    obj_synth_object_resize(feature, xres, yres);
    z = feature->data;
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a*GWY_SQRT3/2.0 + GWY_SQRT3/6.0;
            yc = (x*s + y*c)/b;
            xp = 0.5*xc + GWY_SQRT3/2.0*yc;
            xm = 0.5*xc - GWY_SQRT3/2.0*yc;
            r = MAX(-xc, xp);
            r = MAX(r, xm);
            r = 1.0 - GWY_SQRT3*r;
            z[i*xres + j] = MAX(r, 0.0);
        }
    }
}

static void
place_add_min(GwyDataField *surface,
              ObjSynthObject *object,
              gint col,
              gint row)
{
    gint xres, yres, kxres, kyres;
    gint ioff, joff;
    gint i, j, l;
    gdouble min;
    const gdouble *z;
    gdouble *d, *drow;

    xres = gwy_data_field_get_xres(surface);
    yres = gwy_data_field_get_yres(surface);
    kxres = object->xres;
    kyres = object->yres;

    joff = (col - kxres/2 + 16384*xres) % xres;
    ioff = (row - kyres/2 + 16384*yres) % yres;
    g_return_if_fail(joff >= 0);
    g_return_if_fail(ioff >= 0);

    d = gwy_data_field_get_data(surface);

    min = G_MAXDOUBLE;
    z = object->data;
    for (i = 0; i < kyres; i++) {
        drow = d + ((ioff + i) % yres)*xres;
        for (j = 0; j < kxres; j++, z++) {
            if (*z) {
                l = (joff + j) % xres;
                if (drow[l] < min)
                    min = drow[l];
            }
        }
    }

    z = object->data;
    for (i = 0; i < kyres; i++) {
        drow = d + ((ioff + i) % yres)*xres;
        for (j = 0; j < kxres; j++, z++) {
            if (*z) {
                l = (joff + j) % xres;
                drow[l] = MAX(drow[l], min + *z);
            }
        }
    }
}

static glong
calculate_n_objects(const ObjSynthArgs *args,
                    guint xres, guint yres)
{
    /* The distribution of area differs from the distribution of size. */
    const ObjSynthFeature *feature = get_feature(args->type);
    gdouble noise_corr = exp(2.0*args->size_noise*args->size_noise);
    gdouble area_ratio = feature->get_coverage(args->aspect);
    gdouble mean_obj_area = args->size*args->size * area_ratio * noise_corr;
    gdouble must_cover = args->coverage*xres*yres;
    return (glong)ceil(must_cover/mean_obj_area);
}

static gdouble
getcov_sphere(G_GNUC_UNUSED gdouble aspect)
{
    return G_PI/4.0;
}

static gdouble
getcov_pyramid(G_GNUC_UNUSED gdouble aspect)
{
    return 1.0;
}

static gdouble
getcov_diamond(G_GNUC_UNUSED gdouble aspect)
{
    return 0.5;
}

static gdouble
getcov_box(G_GNUC_UNUSED gdouble aspect)
{
    return 1.0;
}

static gdouble
getcov_tent(G_GNUC_UNUSED gdouble aspect)
{
    return 1.0;
}

static gdouble
getcov_cone(G_GNUC_UNUSED gdouble aspect)
{
    return G_PI/4.0;
}

static gdouble
getcov_nugget(G_GNUC_UNUSED gdouble aspect)
{
    return 1.0 - (1.0 - G_PI/4.0)/MAX(aspect, 1.0/aspect);
}

static gdouble
getcov_thatch(G_GNUC_UNUSED gdouble aspect)
{
    return 0.5;
}

static gdouble
getcov_doughnut(G_GNUC_UNUSED gdouble aspect)
{
    return G_PI/4.0 * 24.0/25.0;
}

static gdouble
getcov_gaussian(G_GNUC_UNUSED gdouble aspect)
{
    /* Just an `effective' value estimate, returning 1 would make the gaussians
     * too tiny wrt to other objects */
    return G_PI/8.0;
}

static gdouble
getcov_thedron(G_GNUC_UNUSED gdouble aspect)
{
    return GWY_SQRT3/4.0;
}

static RandGenSet*
rand_gen_set_new(guint n)
{
    RandGenSet *rngset;
    guint i;

    rngset = g_new(RandGenSet, 1);
    rngset->rng = g_new(GRand*, n);
    rngset->have_spare = g_new0(gboolean, n);
    rngset->spare = g_new0(gdouble, n);
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

    for (i = 0; i < rngset->n; i++) {
        g_rand_set_seed(rngset->rng[i], seed + i);
        rngset->have_spare[i] = FALSE;
    }
}

static void
rand_gen_set_free(RandGenSet *rngset)
{
    guint i;

    for (i = 0; i < rngset->n; i++)
        g_rand_free(rngset->rng[i]);
    g_free(rngset->rng);
    g_free(rngset->have_spare);
    g_free(rngset->spare);
    g_free(rngset);
}

static const gchar prefix[]           = "/module/obj_synth";
static const gchar active_page_key[]  = "/module/obj_synth/active_page";
static const gchar update_key[]       = "/module/obj_synth/update";
static const gchar randomize_key[]    = "/module/obj_synth/randomize";
static const gchar seed_key[]         = "/module/obj_synth/seed";
static const gchar type_key[]         = "/module/obj_synth/type";
static const gchar size_key[]         = "/module/obj_synth/size";
static const gchar size_noise_key[]   = "/module/obj_synth/size_noise";
static const gchar aspect_key[]       = "/module/obj_synth/aspect";
static const gchar aspect_noise_key[] = "/module/obj_synth/aspect_noise";
static const gchar height_key[]       = "/module/obj_synth/height";
static const gchar height_noise_key[] = "/module/obj_synth/height_noise";
static const gchar height_bound_key[] = "/module/obj_synth/height_bound";
static const gchar htrunc_key[]       = "/module/obj_synth/htrunc";
static const gchar htrunc_noise_key[] = "/module/obj_synth/htrunc_noise";
static const gchar angle_key[]        = "/module/obj_synth/angle";
static const gchar angle_noise_key[]  = "/module/obj_synth/angle_noise";
static const gchar coverage_key[]     = "/module/obj_synth/coverage";

static void
obj_synth_sanitize_args(ObjSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page,
                              PAGE_DIMENSIONS, PAGE_NPAGES-1);
    args->update = !!args->update;
    args->seed = MAX(0, args->seed);
    args->randomize = !!args->randomize;
    args->type = MIN(args->type, OBJ_SYNTH_NTYPES-1);
    args->size = CLAMP(args->size, 1.0, 1000.0);
    args->size_noise = CLAMP(args->size_noise, 0.0, 1.0);
    args->aspect = CLAMP(args->aspect, 0.2, 5.0);
    args->aspect_noise = CLAMP(args->aspect_noise, 0.0, 1.0);
    args->height = CLAMP(args->height, 0.001, 10000.0);
    args->height_noise = CLAMP(args->height_noise, 0.0, 1.0);
    args->height_bound = !!args->height_bound;
    args->htrunc = CLAMP(args->htrunc, 0.001, 1.0);
    args->htrunc_noise = CLAMP(args->htrunc_noise, 0.0, 1.0);
    args->angle = CLAMP(args->angle, -G_PI, G_PI);
    args->angle_noise = CLAMP(args->angle_noise, 0.0, 1.0);
    args->coverage = CLAMP(args->coverage, 0.001, 12.0);
}

static void
obj_synth_load_args(GwyContainer *container,
                    ObjSynthArgs *args,
                    GwyDimensionArgs *dimsargs)
{
    *args = obj_synth_defaults;

    gwy_container_gis_int32_by_name(container, active_page_key,
                                    &args->active_page);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, seed_key, &args->seed);
    gwy_container_gis_boolean_by_name(container, randomize_key,
                                      &args->randomize);
    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    gwy_container_gis_double_by_name(container, size_key, &args->size);
    gwy_container_gis_double_by_name(container, size_noise_key,
                                     &args->size_noise);
    gwy_container_gis_double_by_name(container, aspect_key, &args->aspect);
    gwy_container_gis_double_by_name(container, aspect_noise_key,
                                     &args->aspect_noise);
    gwy_container_gis_double_by_name(container, height_key, &args->height);
    gwy_container_gis_double_by_name(container, height_noise_key,
                                     &args->height_noise);
    gwy_container_gis_boolean_by_name(container, height_bound_key,
                                      &args->height_bound);
    gwy_container_gis_double_by_name(container, htrunc_key, &args->htrunc);
    gwy_container_gis_double_by_name(container, htrunc_noise_key,
                                     &args->htrunc_noise);
    gwy_container_gis_double_by_name(container, angle_key, &args->angle);
    gwy_container_gis_double_by_name(container, angle_noise_key,
                                     &args->angle_noise);
    gwy_container_gis_double_by_name(container, coverage_key, &args->coverage);
    obj_synth_sanitize_args(args);

    gwy_clear(dimsargs, 1);
    gwy_dimensions_copy_args(&dims_defaults, dimsargs);
    gwy_dimensions_load_args(dimsargs, container, prefix);
}

static void
obj_synth_save_args(GwyContainer *container,
                    const ObjSynthArgs *args,
                    const GwyDimensionArgs *dimsargs)
{
    gwy_container_set_int32_by_name(container, active_page_key,
                                    args->active_page);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, seed_key, args->seed);
    gwy_container_set_boolean_by_name(container, randomize_key,
                                      args->randomize);
    gwy_container_set_enum_by_name(container, type_key, args->type);
    gwy_container_set_double_by_name(container, size_key, args->size);
    gwy_container_set_double_by_name(container, size_noise_key,
                                     args->size_noise);
    gwy_container_set_double_by_name(container, aspect_key, args->aspect);
    gwy_container_set_double_by_name(container, aspect_noise_key,
                                     args->aspect_noise);
    gwy_container_set_double_by_name(container, height_key, args->height);
    gwy_container_set_double_by_name(container, height_noise_key,
                                     args->height_noise);
    gwy_container_set_boolean_by_name(container, height_bound_key,
                                      args->height_bound);
    gwy_container_set_double_by_name(container, htrunc_key, args->htrunc);
    gwy_container_set_double_by_name(container, htrunc_noise_key,
                                     args->htrunc_noise);
    gwy_container_set_double_by_name(container, angle_key, args->angle);
    gwy_container_set_double_by_name(container, angle_noise_key,
                                     args->angle_noise);
    gwy_container_set_double_by_name(container, coverage_key, args->coverage);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
