/*
 *  @(#) $Id$
 *  Copyright (C) 2007,2009 David Necas (Yeti).
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
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#include "dimensions.h"

#define OBJ_SYNTH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 320,
};

enum {
    RESPONSE_RESET = 1,
};

enum {
    PAGE_DIMENSIONS = 0,
    PAGE_SHAPE      = 1,
    PAGE_PLACEMENT  = 2,
};

typedef enum {
    OBJ_SYNTH_SPHERE   = 0,
    OBJ_SYNTH_PYRAMID  = 1,
    OBJ_SYNTH_NUGGET   = 2,
    OBJ_SYNTH_THATCH   = 3,
    OBJ_SYNTH_DOUGHNUT = 4,
    OBJ_SYNTH_NTYPES
} ObjSynthType;

typedef void (*MakeFeatureFunc)(GwyDataField *feature,
                                gdouble size,
                                gdouble aspect,
                                gdouble height,
                                gdouble angle);

typedef struct {
    ObjSynthType type;
    const gchar *name;
    MakeFeatureFunc create;
} ObjSynthFeature;

typedef struct {
    GRand *id;
    GRand *size;
    GRand *aspect;
    GRand *height;
    GRand *angle;
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
    gdouble height_noise;
    gdouble angle;
    gdouble angle_noise;
} ObjSynthArgs;

typedef struct {
    ObjSynthArgs *args;
    GwyDimensions *dims;
    RandGenSet *rngset;
    gdouble pxsize;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *update;
    GtkWidget *update_now;
    GtkObject *seed;
    GtkWidget *randomize;
    GtkWidget *type;
    GtkObject *size;
    GtkWidget *size_value;
    GtkWidget *size_units;
    GtkObject *size_noise;
    GtkObject *aspect;
    GtkObject *aspect_noise;
    GtkObject *height;
    GtkWidget *height_units;
    GtkObject *height_noise;
    GtkObject *angle;
    GtkObject *angle_noise;
    GwyContainer *mydata;
    gboolean in_init;
} ObjSynthControls;

static gboolean    module_register     (void);
static void        obj_synth           (GwyContainer *data,
                                        GwyRunType run);
static void        run_noninteractive  (ObjSynthArgs *args,
                                        const GwyDimensionArgs *dimsargs,
                                        RandGenSet *rngset,
                                        GwyContainer *data,
                                        GwyDataField *dfield,
                                        gint oldid,
                                        GQuark quark);
static gboolean    obj_synth_dialog    (ObjSynthArgs *args,
                                        GwyDimensionArgs *dimsargs,
                                        RandGenSet *rngset,
                                        GwyContainer *data,
                                        GwyDataField *dfield,
                                        gint id);
static GtkWidget*  feature_selector_new(ObjSynthControls *controls);
static void        update_controls     (ObjSynthControls *controls,
                                        ObjSynthArgs *args);
static GtkWidget*  random_seed_new     (GtkAdjustment *adj);
static GtkWidget*  randomize_new       (gboolean *randomize);
static GtkWidget*  instant_updates_new (GtkWidget **update,
                                        GtkWidget **instant,
                                        gboolean *state);
static void        page_switched       (ObjSynthControls *controls,
                                        GtkNotebookPage *page,
                                        gint pagenum);
static void        seed_changed        (ObjSynthControls *controls,
                                        GtkAdjustment *adj);
static void        randomize_seed      (GtkAdjustment *adj);
static void        size_changed        (ObjSynthControls *controls,
                                        GtkAdjustment *adj);
static void        feature_type_selected(GtkComboBox *combo,
                                         ObjSynthControls *controls);
static void        update_size_value   (ObjSynthControls *controls);
static void        size_noise_changed  (ObjSynthControls *controls,
                                        GtkAdjustment *adj);
static void        aspect_changed      (ObjSynthControls *controls,
                                        GtkAdjustment *adj);
static void        aspect_noise_changed(ObjSynthControls *controls,
                                        GtkAdjustment *adj);
static void        height_changed      (ObjSynthControls *controls,
                                        GtkAdjustment *adj);
static void        height_noise_changed(ObjSynthControls *controls,
                                        GtkAdjustment *adj);
static void        angle_changed       (ObjSynthControls *controls,
                                        GtkAdjustment *adj);
static void        angle_noise_changed (ObjSynthControls *controls,
                                        GtkAdjustment *adj);
static void        update_value_label  (GtkLabel *label,
                                        const GwySIValueFormat *vf,
                                        gdouble value);
static void        obj_synth_invalidate(ObjSynthControls *controls);
static void        preview             (ObjSynthControls *controls);
static void        obj_synth_do        (const ObjSynthArgs *args,
                                        RandGenSet *rngset,
                                        GwyDataField *dfield);
static void        object_synth_iter   (GwyDataField *surface,
                                        GwyDataField *object,
                                        const ObjSynthArgs *args,
                                        RandGenSet *rngset,
                                        gint nxcells,
                                        gint nycells,
                                        gint xoff,
                                        gint yoff,
                                        gint nobjects,
                                        gint *indices);
static void        make_sphere         (GwyDataField *feature,
                                        gdouble size,
                                        gdouble aspect,
                                        gdouble height,
                                        gdouble angle);
static void        make_pyramid        (GwyDataField *feature,
                                        gdouble size,
                                        gdouble aspect,
                                        gdouble height,
                                        gdouble angle);
static void        make_nugget         (GwyDataField *feature,
                                        gdouble size,
                                        gdouble aspect,
                                        gdouble height,
                                        gdouble angle);
static void        make_thatch         (GwyDataField *feature,
                                        gdouble size,
                                        gdouble aspect,
                                        gdouble height,
                                        gdouble angle);
static void        make_doughnut       (GwyDataField *feature,
                                        gdouble size,
                                        gdouble aspect,
                                        gdouble height,
                                        gdouble angle);
static void        place_add_min       (GwyDataField *surface,
                                        GwyDataField *object,
                                        gint col,
                                        gint row);
static RandGenSet* rand_gen_set_new    (void);
static void        rand_gen_set_init   (RandGenSet *rngset,
                                        guint seed);
static void        rand_gen_set_free   (RandGenSet *rngset);
static void        obj_synth_load_args (GwyContainer *container,
                                        ObjSynthArgs *args,
                                        GwyDimensionArgs *dimsargs);
static void        obj_synth_save_args (GwyContainer *container,
                                        const ObjSynthArgs *args,
                                        const GwyDimensionArgs *dimsargs);

static const ObjSynthArgs obj_synth_defaults = {
    PAGE_DIMENSIONS,
    42, TRUE, TRUE,
    OBJ_SYNTH_SPHERE,
    20.0, 0.0,
    1.0, 0.0,
    1.0, 0.0,
    0.0, 0.0,
};

static const GwyDimensionArgs dims_defaults = GWY_DIMENSION_ARGS_INIT;

static const ObjSynthFeature features[] = {
    { OBJ_SYNTH_SPHERE,   N_("Spheres"),  &make_sphere,   },
    { OBJ_SYNTH_PYRAMID,  N_("Pyramids"), &make_pyramid,  },
    { OBJ_SYNTH_NUGGET,   N_("Nuggets"),  &make_nugget,   },
    { OBJ_SYNTH_THATCH,   N_("Thatches"), &make_thatch,   },
    { OBJ_SYNTH_DOUGHNUT, N_("Dougnuts"), &make_doughnut, },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generates randomly patterned surfaces by placing objects."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
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
                              GWY_MENU_FLAG_DATA,
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
    g_return_if_fail(dfield);

    rngset = rand_gen_set_new();
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
    gdouble mag;
    gint newid;

    if (args->randomize)
        args->seed = g_random_int() & 0x7fffffff;

    mag = pow10(dimsargs->xypow10) * dimsargs->measure;
    if (replace)
        gwy_app_undo_qcheckpointv(data, 1, &quark);
    else
        dfield = gwy_data_field_new(dimsargs->xres, dimsargs->yres,
                                    mag*dimsargs->xres, mag*dimsargs->yres,
                                    FALSE);
    obj_synth_do(args, rngset, dfield);

    mag = gwy_data_field_get_rms(dfield);
    if (mag)
        gwy_data_field_multiply(dfield,
                                pow10(dimsargs->zpow10)*args->height/mag);

    if (dimsargs->replace)
        gwy_data_field_data_changed(dfield);
    else {
        siunit = gwy_data_field_get_si_unit_xy(dfield);
        gwy_si_unit_set_from_string(siunit, dimsargs->xyunits);

        siunit = gwy_data_field_get_si_unit_z(dfield);
        gwy_si_unit_set_from_string(siunit, dimsargs->zunits);

        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                0);
        gwy_app_set_data_field_title(data, newid, _("Generated"));
        g_object_unref(dfield);
    }
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
    gint response;
    GwyPixmapLayer *layer;
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
                                FALSE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            0);
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
                             G_CALLBACK(obj_synth_invalidate), &controls);
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

    table = gtk_table_new(15, 5, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new(_("Shape")));
    row = 0;

    controls.type = feature_selector_new(&controls);
    gwy_table_attach_hscale(table, row, _("_Shape:"), NULL,
                            GTK_OBJECT(controls.type), GWY_HSCALE_WIDGET);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Size")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.size = gtk_adjustment_new(args->size, 1.0, 1000.0, 0.1, 10.0, 0);
    gwy_table_attach_hscale(table, row, _("_Size:"), "px",
                            controls.size, GWY_HSCALE_LOG);
    g_signal_connect_swapped(controls.size, "value-changed",
                             G_CALLBACK(size_changed), &controls);
    row++;

    controls.size_value = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.size_value), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.size_value,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    controls.size_units = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.size_units), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.size_units,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.size_noise = gtk_adjustment_new(args->size_noise,
                                             0.0, 1.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("Variance:"), NULL,
                            controls.size_noise, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.size_noise, "value-changed",
                             G_CALLBACK(size_noise_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 12);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Aspect Ratio")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.aspect = gtk_adjustment_new(args->aspect,
                                         0.2, 5.0, 0.01, 1.0, 0);
    gwy_table_attach_hscale(table, row, _("_Aspect ratio:"), NULL,
                            controls.aspect, GWY_HSCALE_LOG);
    g_signal_connect_swapped(controls.aspect, "value-changed",
                             G_CALLBACK(aspect_changed), &controls);
    row++;

    controls.aspect_noise = gtk_adjustment_new(args->aspect_noise,
                                             0.0, 1.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("Variance:"), NULL,
                            controls.aspect_noise, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.aspect_noise, "value-changed",
                             G_CALLBACK(aspect_noise_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 12);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Height")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.height = gtk_adjustment_new(args->height,
                                        0.0001, 10000.0, 0.0001, 1.0, 0);
    gwy_table_attach_hscale(table, row, _("_Height:"), "",
                            controls.height, GWY_HSCALE_LOG);
    controls.height_units = gwy_table_hscale_get_units(controls.height);
    g_signal_connect_swapped(controls.height, "value-changed",
                             G_CALLBACK(height_changed), &controls);
    row++;

    controls.height_noise = gtk_adjustment_new(args->height_noise,
                                             0.0, 1.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("Variance:"), NULL,
                            controls.height_noise, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.height_noise, "value-changed",
                             G_CALLBACK(height_noise_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 12);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Orientation")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.angle = gtk_adjustment_new(args->angle*180.0/G_PI,
                                         -180.0, 180.0, 0.1, 10.0, 0);
    gwy_table_attach_hscale(table, row, _("Orien_tation:"), "deg",
                            controls.angle, 0);
    g_signal_connect_swapped(controls.angle, "value-changed",
                             G_CALLBACK(angle_changed), &controls);
    row++;

    controls.angle_noise = gtk_adjustment_new(args->angle_noise,
                                             0.0, 1.0, 0.001, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("Variance:"), NULL,
                            controls.angle_noise, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.angle_noise, "value-changed",
                             G_CALLBACK(angle_noise_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 12);
    row++;

    gtk_widget_show_all(dialog);
    controls.in_init = FALSE;
    /* Must be done when widgets are shown, see GtkNotebook docs */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), args->active_page);
    obj_synth_invalidate(&controls);

    while (TRUE) {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_OK:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            gwy_dimensions_free(controls.dims);
            return response == GTK_RESPONSE_OK;
            break;

            case RESPONSE_RESET:
            {
                gboolean temp = args->update;
                *args = obj_synth_defaults;
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
    /*
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->sigma), args->sigma);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->freq_min),
                             args->freq_min);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->freq_max),
                             args->freq_max);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->gauss_tau),
                             args->gauss_tau);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->power_p), args->power_p);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->seed), args->seed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->gauss_enable),
                                 args->gauss_enable);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->power_enable),
                                 args->power_enable);
                                 */
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

    button = gtk_button_new_with_mnemonic(_("_New"));
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
page_switched(ObjSynthControls *controls,
              G_GNUC_UNUSED GtkNotebookPage *page,
              gint pagenum)
{
    if (controls->in_init)
        return;

    controls->args->active_page = pagenum;

    if (pagenum == PAGE_SHAPE) {
        GwyDimensions *dims = controls->dims;

        if (controls->height_units)
            gtk_label_set_markup(GTK_LABEL(controls->height_units),
                                 dims->zvf->units);
        controls->pxsize = dims->args->measure * pow10(dims->args->xypow10);
        gtk_label_set_markup(GTK_LABEL(controls->size_units),
                             dims->xyvf->units);

        update_size_value(controls);
    }
}

static void
seed_changed(ObjSynthControls *controls,
             GtkAdjustment *adj)
{
    controls->args->seed = gwy_adjustment_get_int(adj);
    obj_synth_invalidate(controls);
}

static void
randomize_seed(GtkAdjustment *adj)
{
    /* Use the GLib's global PRNG for seeding */
    gtk_adjustment_set_value(adj, g_random_int() & 0x7fffffff);
}

static void
feature_type_selected(GtkComboBox *combo,
                      ObjSynthControls *controls)
{
    controls->args->type = gwy_enum_combo_box_get_active(combo);
    obj_synth_invalidate(controls);
}

static void
size_changed(ObjSynthControls *controls,
             GtkAdjustment *adj)
{
    controls->args->size = gtk_adjustment_get_value(adj);
    update_size_value(controls);
    obj_synth_invalidate(controls);
}

static void
update_size_value(ObjSynthControls *controls)
{
    update_value_label(GTK_LABEL(controls->size_value),
                       controls->dims->xyvf,
                       controls->args->size*controls->pxsize);
}

static void
size_noise_changed(ObjSynthControls *controls,
                   GtkAdjustment *adj)
{
    controls->args->size_noise = gtk_adjustment_get_value(adj);
    obj_synth_invalidate(controls);
}

static void
aspect_changed(ObjSynthControls *controls,
               GtkAdjustment *adj)
{
    controls->args->aspect = gtk_adjustment_get_value(adj);
    obj_synth_invalidate(controls);
}

static void
aspect_noise_changed(ObjSynthControls *controls,
                     GtkAdjustment *adj)
{
    controls->args->aspect_noise = gtk_adjustment_get_value(adj);
    obj_synth_invalidate(controls);
}

static void
height_changed(ObjSynthControls *controls,
               GtkAdjustment *adj)
{
    controls->args->height = gtk_adjustment_get_value(adj);
    /* Height is not observable on the preview, don't do anything. */
}

static void
height_noise_changed(ObjSynthControls *controls,
                     GtkAdjustment *adj)
{
    controls->args->height_noise = gtk_adjustment_get_value(adj);
    obj_synth_invalidate(controls);
}

static void
angle_changed(ObjSynthControls *controls,
              GtkAdjustment *adj)
{
    controls->args->angle = G_PI/180.0*gtk_adjustment_get_value(adj);
    obj_synth_invalidate(controls);
}

static void
angle_noise_changed(ObjSynthControls *controls,
                    GtkAdjustment *adj)
{
    controls->args->angle_noise = gtk_adjustment_get_value(adj);
    obj_synth_invalidate(controls);
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
obj_synth_invalidate(ObjSynthControls *controls)
{
    /* create preview if instant updates are on */
    if (controls->args->update && !controls->in_init)
        preview(controls);
}

static void
preview(ObjSynthControls *controls)
{
    const ObjSynthArgs *args = controls->args;
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    obj_synth_do(args, controls->rngset, dfield);
}

static void
obj_synth_do(const ObjSynthArgs *args,
             RandGenSet *rngset,
             GwyDataField *dfield)
{
    GwyDataField *object;
    gint *indices;
    gint i;

    rand_gen_set_init(rngset, args->seed);
    indices = g_new(gint, 100);
    object = gwy_data_field_new(1, 1, 1.0, 1.0, FALSE);

    gwy_data_field_clear(dfield);
    for (i = 0; i < 5; i++) {
        object_synth_iter(dfield, object, args, rngset,
                          10, 10, 0, 0, 100, indices);
    }

    g_object_unref(object);
    g_free(indices);

    gwy_data_field_data_changed(dfield);
}

static inline gdouble
randsymm(GRand *rng, gdouble mag)
{
    gdouble x;

    do {
       x = g_rand_double(rng);
    } while (G_UNLIKELY(x == 0.0));

    return mag*(2.0*x - 1.0);
}

static void
object_synth_iter(GwyDataField *surface,
                  GwyDataField *object,
                  const ObjSynthArgs *args,
                  RandGenSet *rngset,
                  gint nxcells,
                  gint nycells,
                  gint xoff,
                  gint yoff,
                  gint nobjects,
                  gint *indices)
{
    gint xres, yres, ncells, k;

    g_return_if_fail(nobjects <= nxcells*nycells);

    xres = gwy_data_field_get_xres(surface);
    yres = gwy_data_field_get_yres(surface);
    ncells = nxcells*nycells;

    for (k = 0; k < ncells; k++)
        indices[k] = k;

    for (k = 0; k < nobjects; k++) {
        gdouble size, aspect, height, angle;
        gint id, i, j, from, to;

        id = g_rand_int_range(rngset->id, 0, ncells - k);
        i = indices[id]/nycells;
        j = indices[id] % nycells;
        indices[id] = indices[ncells-1 - k];

        size = args->size;
        if (args->size_noise)
            size *= 1.0 + randsymm(rngset->size, args->size_noise);

        aspect = args->aspect;
        if (args->aspect_noise)
            aspect *= 1.0 + randsymm(rngset->aspect, args->aspect_noise);

        height = args->height;
        if (args->height_noise)
            height *= 1.0 + randsymm(rngset->height, args->height_noise);

        angle = args->angle;
        if (args->angle_noise)
            angle += randsymm(rngset->height, G_PI*args->angle_noise);

        features[args->type].create(object, size, aspect, height, angle);

        from = (j*xres + nxcells/2)/nxcells;
        to = (j*xres + xres + nxcells/2)/nxcells;
        to = MIN(to, xres);
        j = from + xoff + g_rand_int_range(rngset->id, 0, to - from);

        from = (i*yres + nycells/2)/nycells;
        to = (i*yres + yres + nycells/2)/nycells;
        to = MIN(to, yres);
        i = from + yoff + g_rand_int_range(rngset->id, 0, to - from);

        place_add_min(surface, object, j, i);
    }
}

static void
make_sphere(GwyDataField *feature,
            gdouble size,
            gdouble aspect,
            gdouble height,
            gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size/sqrt(aspect);
    b = size*sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*hypot(a*c, b*s) + 1) | 1;
    yres = (gint)ceil(2*hypot(a*s, b*c) + 1) | 1;

    gwy_data_field_resample(feature, xres, yres, GWY_INTERPOLATION_NONE);
    z = gwy_data_field_get_data(feature);
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = 1.0 - xc*xc - yc*yc;
            z[i*xres + j] = (r > 0.0) ? height*sqrt(r) : 0.0;
        }
    }
}

static void
make_pyramid(GwyDataField *feature,
             gdouble size,
             gdouble aspect,
             gdouble height,
             gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size/sqrt(aspect);
    b = size*sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*(a*fabs(c) + b*fabs(s)) + 1) | 1;
    yres = (gint)ceil(2*(a*fabs(s) + b*fabs(c)) + 1) | 1;

    gwy_data_field_resample(feature, xres, yres, GWY_INTERPOLATION_NONE);
    z = gwy_data_field_get_data(feature);
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = 1.0 - MAX(fabs(xc), fabs(yc));
            z[i*xres + j] = (r > 0.0) ? height*r : 0.0;
        }
    }
}

static void
make_nugget(GwyDataField *feature,
            gdouble size,
            gdouble aspect,
            gdouble height,
            gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc, excess;
    gint xres, yres, i, j;
    gdouble *z;

    if (aspect == 1.0) {
        make_sphere(feature, size, aspect, height, angle);
        return;
    }

    /* Ensure a > b */
    if (aspect >= 1.0) {
        angle += G_PI/2.0;
        aspect = 1.0/aspect;
    }

    a = size/sqrt(aspect);
    b = size*sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    excess = 1.0/aspect - 1.0;
    /* FIXME: The rotated rectangular bounding box, this is a bit wasteful. */
    xres = (gint)ceil(2*(a*fabs(c) + b*fabs(s)) + 1) | 1;
    yres = (gint)ceil(2*(a*fabs(s) + b*fabs(c)) + 1) | 1;

    gwy_data_field_resample(feature, xres, yres, GWY_INTERPOLATION_NONE);
    z = gwy_data_field_get_data(feature);
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
            z[i*xres + j] = (r > 0.0) ? height*sqrt(r) : 0.0;
        }
    }
}

static void
make_thatch(GwyDataField *feature,
            gdouble size,
            gdouble aspect,
            gdouble height,
            gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size/sqrt(aspect);
    b = size*sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    /* FIXME: The rotated rectangular bounding box, this is a bit wasteful. */
    xres = (gint)ceil(2*(a*fabs(c) + b*fabs(s)) + 1) | 1;
    yres = (gint)ceil(2*(a*fabs(s) + b*fabs(c)) + 1) | 1;

    gwy_data_field_resample(feature, xres, yres, GWY_INTERPOLATION_NONE);
    z = gwy_data_field_get_data(feature);
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = 0.5 - 0.5*xc;
            if (r >= 0.0 && r <= 1.0)
                z[i*xres + j] = (fabs(yc) <= r*aspect) ? height*(1.0 - r) : 0.0;
            else
                z[i*xres + j] = 0.0;
        }
    }
}

static void
place_add_min(GwyDataField *surface,
              GwyDataField *object,
              gint col,
              gint row)
{
    gint xres, yres, kxres, kyres;
    gint ioff, joff;
    gint i, j, l;
    gdouble min;
    const gdouble *k, *krow;
    gdouble *d, *drow;

    xres = gwy_data_field_get_xres(surface);
    yres = gwy_data_field_get_yres(surface);
    kxres = gwy_data_field_get_xres(object);
    kyres = gwy_data_field_get_yres(object);

    joff = (col - kxres/2 + 2*xres) % xres;
    ioff = (row - kyres/2 + 2*yres) % yres;
    g_return_if_fail(joff >= 0);
    g_return_if_fail(ioff >= 0);

    d = gwy_data_field_get_data(surface);
    k = gwy_data_field_get_data_const(object);

    min = G_MAXDOUBLE;
    for (i = 0; i < kyres; i++) {
        drow = d + ((ioff + i) % yres)*xres;
        krow = k + i*kxres;
        for (j = 0; j < kxres; j++) {
            if (krow[j]) {
                l = (joff + j) % xres;
                if (drow[l] < min)
                    min = drow[l];
            }
        }
    }
    for (i = 0; i < kyres; i++) {
        drow = d + ((ioff + i) % yres)*xres;
        krow = k + i*kxres;
        for (j = 0; j < kxres; j++) {
            if (krow[j]) {
                l = (joff + j) % xres;
                drow[l] = MAX(drow[l], min + krow[j]);
            }
        }
    }
}

static void
make_doughnut(GwyDataField *feature,
              gdouble size,
              gdouble aspect,
              gdouble height,
              gdouble angle)
{
    gdouble a, b, c, s, r, x, y, xc, yc;
    gint xres, yres, i, j;
    gdouble *z;

    a = size/sqrt(aspect);
    b = size*sqrt(aspect);
    c = cos(angle);
    s = sin(angle);
    xres = (gint)ceil(2*hypot(a*c, b*s) + 1) | 1;
    yres = (gint)ceil(2*hypot(a*s, b*c) + 1) | 1;

    gwy_data_field_resample(feature, xres, yres, GWY_INTERPOLATION_NONE);
    z = gwy_data_field_get_data(feature);
    for (i = 0; i < yres; i++) {
        y = i - yres/2;
        for (j = 0; j < xres; j++) {
            x = j - xres/2;

            xc = (x*c - y*s)/a;
            yc = (x*s + y*c)/b;
            r = hypot(xc, yc) - 0.6;
            r = 1.0 - r*r/0.16;
            z[i*xres + j] = (r > 0.0) ? height*sqrt(r) : 0.0;
        }
    }
}

static RandGenSet*
rand_gen_set_new(void)
{
    RandGenSet *rngset;

    rngset = g_new(RandGenSet, 1);
    rngset->id = g_rand_new();
    rngset->size = g_rand_new();
    rngset->aspect = g_rand_new();
    rngset->height = g_rand_new();
    rngset->angle = g_rand_new();

    return rngset;
}

static void
rand_gen_set_init(RandGenSet *rngset,
                  guint seed)
{
    g_rand_set_seed(rngset->id, seed);
    g_rand_set_seed(rngset->size, seed);
    g_rand_set_seed(rngset->aspect, seed);
    g_rand_set_seed(rngset->height, seed);
    g_rand_set_seed(rngset->angle, seed);
}

static void
rand_gen_set_free(RandGenSet *rngset)
{
    g_rand_free(rngset->id);
    g_rand_free(rngset->size);
    g_rand_free(rngset->aspect);
    g_rand_free(rngset->height);
    g_rand_free(rngset->angle);
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
static const gchar angle_key[]        = "/module/obj_synth/angle";
static const gchar angle_noise_key[]  = "/module/obj_synth/angle_noise";

static void
obj_synth_sanitize_args(ObjSynthArgs *args)
{
    args->active_page = CLAMP(args->active_page, PAGE_DIMENSIONS, PAGE_SHAPE);
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
    args->angle = CLAMP(args->angle, -G_PI, G_PI);
    args->angle_noise = CLAMP(args->angle_noise, 0.0, 1.0);
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
    gwy_container_gis_double_by_name(container, angle_key, &args->angle);
    gwy_container_gis_double_by_name(container, angle_noise_key,
                                     &args->angle_noise);
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
    gwy_container_set_double_by_name(container, angle_key, args->angle);
    gwy_container_set_double_by_name(container, angle_noise_key,
                                     args->angle_noise);

    gwy_dimensions_save_args(dimsargs, container, prefix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
