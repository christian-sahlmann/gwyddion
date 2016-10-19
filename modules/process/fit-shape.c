/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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

/* TODO:
 * - Gradient adaptation only works for full-range mapping.  Simply enforce
 *   full-range mapping with adapted gradients?
 * - Align parameter table properly (with UTF-8 string lengths).
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyrandgenset.h>
#include <libgwyddion/gwynlfit.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <libprocess/arithmetic.h>
#include <libprocess/peaks.h>
#include <libprocess/gwyshapefitpreset.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwymodule/gwymodule-xyz.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>
#include "preview.h"

#define FIT_SHAPE_RUN_MODES GWY_RUN_INTERACTIVE

#define FIT_GRADIENT_NAME "__GwyFitDiffGradient"

/* Lower symmetric part indexing */
/* i MUST be greater or equal than j */
#define SLi(a, i, j) a[(i)*((i) + 1)/2 + (j)]

enum { NREDLIM = 4096 };

typedef enum {
    FIT_SHAPE_DISPLAY_DATA   = 0,
    FIT_SHAPE_DISPLAY_RESULT = 1,
    FIT_SHAPE_DISPLAY_DIFF   = 2
} FitShapeDisplayType;

typedef enum {
    FIT_SHAPE_OUTPUT_FIT  = 0,
    FIT_SHAPE_OUTPUT_DIFF = 1,
    FIT_SHAPE_OUTPUT_BOTH = 2,
} FitShapeOutputType;

typedef enum {
    FIT_SHAPE_INITIALISED      = 0,
    FIT_SHAPE_ESTIMATED        = 1,
    FIT_SHAPE_QUICK_FITTED     = 2,
    FIT_SHAPE_FITTED           = 3,
    FIT_SHAPE_USER             = 4,
    FIT_SHAPE_ESTIMATE_FAILED  = 5,
    FIT_SHAPE_QUICK_FIT_FAILED = 6,
    FIT_SHAPE_FIT_FAILED       = 7,
    FIT_SHAPE_FIT_CANCELLED    = 8,
} FitShapeState;

typedef struct {
    const gchar *function;
    GwyMaskingType masking;
    FitShapeDisplayType display;
    FitShapeOutputType output;
    gboolean diff_colourmap;
    gboolean diff_excluded;
} FitShapeArgs;

/* XXX XXX XXX XXX XXX */
typedef struct {
    guint nparam;
    gboolean *param_fixed;

    GwySurface *surface;     /* Either ref(surface), or created from image. */
    guint n;
    const GwyXYZ *xyz;       /* Data */
    gdouble *f;              /* Function values. */
} FitShapeContext;

typedef struct {
    GtkWidget *fix;          /* Unused for secondary */
    GtkWidget *name;
    GtkWidget *equals;
    GtkWidget *value;
    GtkWidget *value_unit;
    GtkWidget *pm;
    GtkWidget *error;
    GtkWidget *error_unit;
    gdouble magnitude;       /* Unused for secondary */
} FitParamControl;

typedef struct {
    FitShapeArgs *args;
    /* These are actually non-GUI and could be separated for some
     * non-interactive use. */
    FitShapeContext *ctx;
    FitShapeState state;
    GwyAppPage pageno;
    gint id;
    gchar *title;
    GwyShapeFitPreset *preset;
    gdouble *param;
    gdouble *alt_param;
    gdouble *param_err;
    gdouble *correl;
    gdouble *secondary;
    gdouble *secondary_err;
    gdouble rss;
    /* This is GUI but we use the fields in mydata. */
    GwyContainer *mydata;
    GwyGradient *diff_gradient;
    GtkWidget *dialogue;
    GtkWidget *view;
    GtkWidget *function;
    GtkWidget *diff_colourmap;
    GtkWidget *diff_excluded;
    GtkWidget *output;
    GSList *display;
    GSList *masking;
    GtkWidget *rss_label;
    GtkWidget *fit_message;
    GtkWidget *revert;
    GtkWidget *recalculate;
    GtkWidget *param_table;
    GtkWidget *correl_table;
    GArray *param_controls;
    GPtrArray *correl_values;
    GPtrArray *correl_hlabels;
    GPtrArray *correl_vlabels;
    GtkWidget *secondary_table;
    GArray *secondary_controls;
} FitShapeControls;

static gboolean      module_register             (void);
static void          fit_shape                   (GwyContainer *data,
                                                  GwyRunType run);
static void          fit_shape_xyz               (GwyContainer *data,
                                                  GwyRunType run);
static void          fit_shape_dialogue          (FitShapeArgs *args,
                                                  GwyContainer *data,
                                                  gint id,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield,
                                                  GwySurface *surface);
static void          create_output_fields        (FitShapeControls *controls,
                                                  GwyContainer *data);
static void          create_output_xyz           (FitShapeControls *controls,
                                                  GwyContainer *data);
static GtkWidget*    basic_tab_new               (FitShapeControls *controls,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield);
static gint          basic_tab_add_masking       (FitShapeControls *controls,
                                                  GtkWidget *table,
                                                  gint row);
static GtkWidget*    parameters_tab_new          (FitShapeControls *controls);
static void          fit_param_table_resize      (FitShapeControls *controls);
static GtkWidget*    correl_tab_new              (FitShapeControls *controls);
static void          fit_correl_table_resize     (FitShapeControls *controls);
static GtkWidget*    secondary_tab_new           (FitShapeControls *controls);
static void          fit_secondary_table_resize  (FitShapeControls *controls);
static GtkWidget*    function_menu_new           (const gchar *name,
                                                  GwyDataField *dfield,
                                                  FitShapeControls *controls);
static void          function_changed            (GtkComboBox *combo,
                                                  FitShapeControls *controls);
static void          display_changed             (GtkToggleButton *toggle,
                                                  FitShapeControls *controls);
static void          diff_colourmap_changed      (GtkToggleButton *toggle,
                                                  FitShapeControls *controls);
static void          diff_excluded_changed       (GtkToggleButton *toggle,
                                                  FitShapeControls *controls);
static void          output_changed              (GtkComboBox *combo,
                                                  FitShapeControls *controls);
static void          masking_changed             (GtkToggleButton *toggle,
                                                  FitShapeControls *controls);
static void          update_colourmap_key        (FitShapeControls *controls);
static void          fix_changed                 (GtkToggleButton *button,
                                                  FitShapeControls *controls);
static void          param_value_activate        (GtkEntry *entry,
                                                  FitShapeControls *controls);
static void          update_all_param_values     (FitShapeControls *controls);
static void          revert_params               (FitShapeControls *controls);
static void          calculate_secondary_params  (FitShapeControls *controls);
static void          recalculate_image           (FitShapeControls *controls);
static void          update_param_table          (FitShapeControls *controls,
                                                  const gdouble *param,
                                                  const gdouble *param_err);
static void          update_correl_table         (FitShapeControls *controls,
                                                  GwyNLFitter *fitter);
static void          update_secondary_table      (FitShapeControls *controls);
static void          fit_shape_estimate          (FitShapeControls *controls);
static void          fit_shape_reduced_fit       (FitShapeControls *controls);
static void          fit_shape_full_fit          (FitShapeControls *controls);
static void          fit_copy_correl_matrix      (FitShapeControls *controls,
                                                  GwyNLFitter *fitter);
static void          update_fields               (FitShapeControls *controls);
static void          update_diff_gradient        (FitShapeControls *controls);
static void          update_fit_state            (FitShapeControls *controls);
static void          update_fit_results          (FitShapeControls *controls,
                                                  GwyNLFitter *fitter);
static void          update_context_data         (FitShapeControls *controls);
static void          fit_context_resize_params   (FitShapeContext *ctx,
                                                  guint n_param);
static void          fit_context_free            (FitShapeContext *ctx);
static GwyNLFitter*  fit                         (GwyShapeFitPreset *preset,
                                                  const FitShapeContext *ctx,
                                                  guint maxiter,
                                                  gdouble *param,
                                                  gdouble *rss,
                                                  GwySetFractionFunc set_fraction,
                                                  GwySetMessageFunc set_message);
static GwyNLFitter*  fit_reduced                 (GwyShapeFitPreset *preset,
                                                  const FitShapeContext *ctx,
                                                  gdouble *param,
                                                  gdouble *rss);
static void          calculate_field             (GwyShapeFitPreset *preset,
                                                  const gdouble *params,
                                                  GwyDataField *dfield);
static void          reduce_data_size            (const GwyXYZ *xyzsrc,
                                                  guint nsrc,
                                                  GwySurface *dest);
static GString*      create_fit_report           (FitShapeControls *controls);
static void          fit_shape_load_args         (GwyContainer *container,
                                                  FitShapeArgs *args);
static void          fit_shape_save_args         (GwyContainer *container,
                                                  FitShapeArgs *args);

/* NB: The default must not require same units because then we could not fall
 * back to it in all cases. */
static const FitShapeArgs fit_shape_defaults = {
    "Grating (simple)", GWY_MASK_IGNORE,
    FIT_SHAPE_DISPLAY_RESULT, FIT_SHAPE_OUTPUT_FIT,
    TRUE, TRUE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Fits predefined geometrical shapes to data."),
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fit_shape",
                              (GwyProcessFunc)&fit_shape,
                              N_("/_Level/_Fit Shape..."),
                              NULL,
                              FIT_SHAPE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Fit geometrical shapes"));
    gwy_xyz_func_register("xyz_fit_shape",
                          (GwyXYZFunc)&fit_shape_xyz,
                          N_("/_Fit Shape..."),
                          NULL,
                          FIT_SHAPE_RUN_MODES,
                          GWY_MENU_FLAG_XYZ,
                          N_("Fit geometrical shapes"));

    return TRUE;
}

static void
fit_shape(GwyContainer *data, GwyRunType run)
{
    FitShapeArgs args;
    GwyDataField *dfield, *mfield;
    gint id;

    g_return_if_fail(run & FIT_SHAPE_RUN_MODES);

    fit_shape_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    fit_shape_dialogue(&args, data, id, dfield, mfield, NULL);

    fit_shape_save_args(gwy_app_settings_get(), &args);

}

static void
fit_shape_xyz(GwyContainer *data, GwyRunType run)
{
    FitShapeArgs args;
    GwySurface *surface;
    gint id;

    g_return_if_fail(run & FIT_SHAPE_RUN_MODES);

    fit_shape_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_SURFACE, &surface,
                                     GWY_APP_SURFACE_ID, &id,
                                     0);
    g_return_if_fail(surface);

    fit_shape_dialogue(&args, data, id, NULL, NULL, surface);

    fit_shape_save_args(gwy_app_settings_get(), &args);

}

static void
fit_shape_dialogue(FitShapeArgs *args,
                   GwyContainer *data, gint id,
                   GwyDataField *dfield, GwyDataField *mfield,
                   GwySurface *surface)
{
    GtkWidget *dialogue, *notebook, *widget, *vbox, *hbox, *alignment,
              *hbox2, *label;
    FitShapeControls controls;
    FitShapeContext ctx;
    GwyDataField *mydfield = NULL;
    GString *report;
    gint response;

    gwy_clear(&ctx, 1);
    gwy_clear(&controls, 1);
    controls.args = args;
    controls.ctx = &ctx;
    controls.id = id;

    if (surface) {
        controls.pageno = GWY_PAGE_XYZS;
        controls.title = gwy_app_get_surface_title(data, id);
        mydfield = gwy_data_field_new(1, 1, 1.0, 1.0, FALSE);
        gwy_preview_surface_to_datafield(surface, mydfield,
                                         PREVIEW_SIZE, PREVIEW_SIZE,
                                         GWY_PREVIEW_SURFACE_FILL);
        dfield = mydfield;
    }
    else if (dfield) {
        controls.pageno = GWY_PAGE_CHANNELS;
        controls.title = gwy_app_get_data_field_title(data, id);
    }
    else {
        g_return_if_reached();
    }

    controls.diff_gradient = gwy_inventory_new_item(gwy_gradients(),
                                                    GWY_GRADIENT_DEFAULT,
                                                    FIT_GRADIENT_NAME);
    gwy_resource_use(GWY_RESOURCE(controls.diff_gradient));

    dialogue = gtk_dialog_new_with_buttons(_("Fit Shape"), NULL, 0,
                                           GTK_STOCK_SAVE,
                                           RESPONSE_SAVE,
                                           gwy_sgettext("verb|_Fit"),
                                           RESPONSE_REFINE,
                                           gwy_sgettext("verb|_Quick Fit"),
                                           RESPONSE_CALCULATE,
                                           gwy_sgettext("verb|_Estimate"),
                                           RESPONSE_ESTIMATE,
                                           GTK_STOCK_CANCEL,
                                           GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_OK,
                                           GTK_RESPONSE_OK,
                                           NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialogue), GTK_RESPONSE_OK);
    if (controls.pageno == GWY_PAGE_XYZS)
        gwy_help_add_to_xyz_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);
    else
        gwy_help_add_to_proc_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);
    controls.dialogue = dialogue;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialogue)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    if (surface) {
        gwy_container_set_object_by_name(controls.mydata,
                                         "/surface/0", surface);
    }
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    if (mfield)
        gwy_container_set_object_by_name(controls.mydata, "/0/mask", mfield);
    dfield = gwy_data_field_duplicate(dfield);
    gwy_container_set_object_by_name(controls.mydata, "/1/data", dfield);
    g_object_unref(dfield);
    dfield = gwy_data_field_duplicate(dfield);
    gwy_container_set_object_by_name(controls.mydata, "/2/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_const_string_by_name(controls.mydata, "/2/base/palette",
                                           FIT_GRADIENT_NAME);
    if (surface) {
        GQuark quark = gwy_app_get_surface_palette_key_for_id(id);
        const guchar *gradient;
        if (gwy_container_gis_string(data, quark, &gradient)) {
            gwy_container_set_const_string_by_name(controls.mydata,
                                                   "/0/base/palette",
                                                   gradient);
        }
    }
    else {
        gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_RANGE_TYPE,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                0);
    }
    controls.view = create_preview(controls.mydata, 0, PREVIEW_SIZE, FALSE);
    alignment = GTK_WIDGET(gtk_alignment_new(0.5, 0, 0, 0));
    gtk_container_add(GTK_CONTAINER(alignment), controls.view);
    gtk_box_pack_start(GTK_BOX(hbox), alignment, FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    widget = basic_tab_new(&controls, dfield, mfield);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
                             gtk_label_new(gwy_sgettext("adjective|Basic")));

    widget = parameters_tab_new(&controls);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
                             gtk_label_new(_("Parameters")));

    widget = correl_tab_new(&controls);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
                             gtk_label_new(_("Correlation Matrix")));

    widget = secondary_tab_new(&controls);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
                             gtk_label_new(_("Derived Quantities")));

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    label = gtk_label_new(_("Mean square difference:"));
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 2);

    controls.rss_label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.rss_label, FALSE, FALSE, 2);

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    controls.fit_message = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.fit_message, FALSE, FALSE, 2);

    update_context_data(&controls);
    function_changed(GTK_COMBO_BOX(controls.function), &controls);
    display_changed(NULL, &controls);

    gtk_widget_show_all(dialogue);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialogue));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialogue);
            case GTK_RESPONSE_NONE:
            goto finalise;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_REFINE:
            fit_shape_full_fit(&controls);
            break;

            case RESPONSE_CALCULATE:
            fit_shape_reduced_fit(&controls);
            break;

            case RESPONSE_ESTIMATE:
            fit_shape_estimate(&controls);
            break;

            case RESPONSE_SAVE:
            report = create_fit_report(&controls);
            gwy_save_auxiliary_data(_("Save Fit Report"), GTK_WINDOW(dialogue),
                                    -1, report->str);
            g_string_free(report, TRUE);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    if (controls.pageno == GWY_PAGE_XYZS)
        create_output_xyz(&controls, data);
    else
        create_output_fields(&controls, data);

    gtk_widget_destroy(dialogue);

finalise:
    gwy_resource_release(GWY_RESOURCE(controls.diff_gradient));
    gwy_inventory_delete_item(gwy_gradients(), FIT_GRADIENT_NAME);
    g_object_unref(controls.mydata);
    GWY_OBJECT_UNREF(mydfield);
    g_free(controls.param);
    g_free(controls.alt_param);
    g_free(controls.param_err);
    g_free(controls.secondary);
    g_free(controls.secondary_err);
    g_free(controls.correl);
    g_free(controls.title);
    g_array_free(controls.param_controls, TRUE);
    g_ptr_array_free(controls.correl_values, TRUE);
    g_ptr_array_free(controls.correl_hlabels, TRUE);
    g_ptr_array_free(controls.correl_vlabels, TRUE);
    g_array_free(controls.secondary_controls, TRUE);
    fit_context_free(controls.ctx);
}

/* NB: We reuse fields from mydata.  It is possible only because they are
 * newly created and we are going to destroy mydata anyway. */
static void
create_output_fields(FitShapeControls *controls, GwyContainer *data)
{
    FitShapeArgs *args = controls->args;
    GwyDataField *dfield;
    gint id = controls->id, newid;

    if (args->output == FIT_SHAPE_OUTPUT_FIT
        || args->output == FIT_SHAPE_OUTPUT_BOTH) {
        dfield = gwy_container_get_object_by_name(controls->mydata, "/1/data");
        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                GWY_DATA_ITEM_SELECTIONS,
                                0);
        gwy_app_channel_log_add_proc(data, id, newid);
        gwy_app_set_data_field_title(data, newid, _("Fitted shape"));
    }

    if (args->output == FIT_SHAPE_OUTPUT_DIFF
        || args->output == FIT_SHAPE_OUTPUT_BOTH) {
        dfield = gwy_container_get_object_by_name(controls->mydata, "/2/data");
        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                GWY_DATA_ITEM_SELECTIONS,
                                0);
        gwy_app_channel_log_add_proc(data, id, newid);
        gwy_app_set_data_field_title(data, newid, _("Difference"));
    }
}

static void
create_output_xyz(FitShapeControls *controls, GwyContainer *data)
{
    FitShapeArgs *args = controls->args;
    FitShapeContext *ctx = controls->ctx;
    GwySurface *surface, *newsurface;
    gint id = controls->id, newid;
    GQuark quark = gwy_app_get_surface_palette_key_for_id(id);
    const guchar *gradient = NULL;
    const GwyXYZ *sxyz;
    GwyXYZ *xyz;
    guint n, i;

    gwy_container_gis_string(data, quark, &gradient);
    surface = gwy_container_get_object_by_name(controls->mydata, "/surface/0");
    g_return_if_fail(gwy_surface_get_npoints(surface) == ctx->n);
    sxyz = gwy_surface_get_data_const(surface);
    n = ctx->n;

    if (args->output == FIT_SHAPE_OUTPUT_FIT
        || args->output == FIT_SHAPE_OUTPUT_BOTH) {
        newsurface = gwy_surface_duplicate(surface);
        xyz = gwy_surface_get_data(newsurface);
        for (i = 0; i < n; i++)
            xyz[i].z = ctx->f[i];

        newid = gwy_app_data_browser_add_surface(newsurface, data, TRUE);
        gwy_app_xyz_log_add_xyz(data, id, newid);
        gwy_app_set_surface_title(data, newid, _("Fitted shape"));
        g_object_unref(newsurface);
        if (gradient) {
            quark = gwy_app_get_surface_palette_key_for_id(newid);
            gwy_container_set_const_string(data, quark, gradient);
        }
    }

    if (args->output == FIT_SHAPE_OUTPUT_DIFF
        || args->output == FIT_SHAPE_OUTPUT_BOTH) {
        newsurface = gwy_surface_duplicate(surface);
        xyz = gwy_surface_get_data(newsurface);
        for (i = 0; i < n; i++)
            xyz[i].z = sxyz[i].z - ctx->f[i];

        newid = gwy_app_data_browser_add_surface(newsurface, data, TRUE);
        gwy_app_xyz_log_add_xyz(data, id, newid);
        gwy_app_set_surface_title(data, newid, _("Difference"));
        g_object_unref(newsurface);
        if (gradient) {
            quark = gwy_app_get_surface_palette_key_for_id(newid);
            gwy_container_set_const_string(data, quark, gradient);
        }
    }
}

static GtkWidget*
basic_tab_new(FitShapeControls *controls,
              GwyDataField *dfield, GwyDataField *mfield)
{
    static const GwyEnum displays[] = {
        { N_("Data"),         FIT_SHAPE_DISPLAY_DATA,   },
        { N_("Fitted shape"), FIT_SHAPE_DISPLAY_RESULT, },
        { N_("Difference"),   FIT_SHAPE_DISPLAY_DIFF,   },
    };
    static const GwyEnum outputs[] = {
        { N_("Fitted shape"), FIT_SHAPE_OUTPUT_FIT,  },
        { N_("Difference"),   FIT_SHAPE_OUTPUT_DIFF, },
        { N_("Both"),         FIT_SHAPE_OUTPUT_BOTH, },
    };

    GtkWidget *table, *label, *hbox;
    FitShapeArgs *args = controls->args;
    gint row;

    hbox = gtk_hbox_new(FALSE, 0);

    table = gtk_table_new(8 + 4*(!!mfield), 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    controls->function = function_menu_new(args->function, dfield, controls);
    gwy_table_attach_hscale(table, row, _("_Function type:"), NULL,
                            GTK_OBJECT(controls->function), GWY_HSCALE_WIDGET);
    row++;

    controls->output
        = gwy_enum_combo_box_new(outputs, G_N_ELEMENTS(outputs),
                                 G_CALLBACK(output_changed), controls,
                                 args->output, TRUE);
    gwy_table_attach_hscale(table, row, _("Output _type:"), NULL,
                            GTK_OBJECT(controls->output), GWY_HSCALE_WIDGET);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new(_("Preview:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls->display
        = gwy_radio_buttons_create(displays, G_N_ELEMENTS(displays),
                                   G_CALLBACK(display_changed),
                                   controls, args->display);
    row = gwy_radio_buttons_attach_to_table(controls->display,
                                            GTK_TABLE(table), 3, row);
    row++;

    controls->diff_colourmap
        = gtk_check_button_new_with_mnemonic(_("Show differences with "
                                               "_adapted color map"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->diff_colourmap),
                                 args->diff_colourmap);
    gtk_table_attach(GTK_TABLE(table), controls->diff_colourmap,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls->diff_colourmap, "toggled",
                     G_CALLBACK(diff_colourmap_changed), controls);
    row++;

    if (mfield)
        row = basic_tab_add_masking(controls, table, row);

    return hbox;
}

static gint
basic_tab_add_masking(FitShapeControls *controls, GtkWidget *table, gint row)
{
    GtkWidget *label;
    FitShapeArgs *args = controls->args;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gwy_label_new_header(_("Masking Mode"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls->masking
        = gwy_radio_buttons_create(gwy_masking_type_get_enum(), -1,
                                   G_CALLBACK(masking_changed),
                                   controls, args->masking);
    row = gwy_radio_buttons_attach_to_table(controls->masking,
                                            GTK_TABLE(table), 3, row);
    row++;

    controls->diff_excluded
        = gtk_check_button_new_with_mnemonic(_("Calculate differences "
                                               "for e_xcluded pixels"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->diff_excluded),
                                 args->diff_excluded);
    gtk_table_attach(GTK_TABLE(table), controls->diff_excluded,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls->diff_excluded, "toggled",
                     G_CALLBACK(diff_excluded_changed), controls);
    row++;

    return row;
}

static GtkWidget*
parameters_tab_new(FitShapeControls *controls)
{
    GtkWidget *vbox, *hbox;
    GtkSizeGroup *sizegroup;
    GtkTable *table;

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

    controls->param_table = gtk_table_new(1, 8, FALSE);
    table = GTK_TABLE(controls->param_table);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 2);
    gtk_table_set_col_spacing(table, 0, 6);
    gtk_table_set_col_spacing(table, 4, 6);
    gtk_table_set_col_spacing(table, 5, 6);
    gtk_table_set_col_spacing(table, 7, 6);
    gtk_box_pack_start(GTK_BOX(vbox), controls->param_table, FALSE, FALSE, 0);

    gtk_table_attach(table, gwy_label_new_header(_("Fix")),
                     0, 1, 0, 1, GTK_FILL, 0, 0, 0);
    gtk_table_attach(table, gwy_label_new_header(_("Parameter")),
                     1, 5, 0, 1, GTK_FILL, 0, 0, 0);
    gtk_table_attach(table, gwy_label_new_header(_("Error")),
                     6, 8, 0, 1, GTK_FILL, 0, 0, 0);

    controls->param_controls = g_array_new(FALSE, FALSE,
                                           sizeof(FitParamControl));

    sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    controls->recalculate = gtk_button_new_with_mnemonic(_("_Recalculate "
                                                           "Image"));
    gtk_size_group_add_widget(sizegroup, controls->recalculate);
    gtk_box_pack_start(GTK_BOX(hbox), controls->recalculate, FALSE, FALSE, 0);
    g_signal_connect_swapped(controls->recalculate, "clicked",
                             G_CALLBACK(recalculate_image), controls);

    controls->revert = gtk_button_new_with_mnemonic(_("Revert to "
                                                      "_Previous Values"));
    gtk_size_group_add_widget(sizegroup, controls->revert);
    gtk_box_pack_start(GTK_BOX(hbox), controls->revert, FALSE, FALSE, 0);
    g_signal_connect_swapped(controls->revert, "clicked",
                             G_CALLBACK(revert_params), controls);

    g_object_unref(sizegroup);

    return vbox;
}

static void
fit_param_table_resize(FitShapeControls *controls)
{
    GtkTable *table;
    guint i, row, old_nparams, nparams;

    old_nparams = controls->param_controls->len;
    nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
    gwy_debug("%u -> %u", old_nparams, nparams);
    for (i = old_nparams; i > nparams; i--) {
        FitParamControl *cntrl = &g_array_index(controls->param_controls,
                                                FitParamControl, i-1);
        gtk_widget_destroy(cntrl->fix);
        gtk_widget_destroy(cntrl->name);
        gtk_widget_destroy(cntrl->equals);
        gtk_widget_destroy(cntrl->value);
        gtk_widget_destroy(cntrl->value_unit);
        gtk_widget_destroy(cntrl->pm);
        gtk_widget_destroy(cntrl->error);
        gtk_widget_destroy(cntrl->error_unit);
        g_array_set_size(controls->param_controls, i-1);
    }

    table = GTK_TABLE(controls->param_table);
    gtk_table_resize(table, 1+nparams, 8);
    row = old_nparams + 1;

    for (i = old_nparams; i < nparams; i++) {
        FitParamControl cntrl;

        cntrl.fix = gtk_check_button_new();
        gtk_table_attach(table, cntrl.fix, 0, 1, row, row+1, 0, 0, 0, 0);
        g_object_set_data(G_OBJECT(cntrl.fix), "id", GUINT_TO_POINTER(i));
        g_signal_connect(cntrl.fix, "toggled",
                         G_CALLBACK(fix_changed), controls);

        cntrl.name = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.name), 1.0, 0.5);
        gtk_table_attach(table, cntrl.name,
                         1, 2, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.equals = gtk_label_new("=");
        gtk_table_attach(table, cntrl.equals, 2, 3, row, row+1, 0, 0, 0, 0);

        cntrl.value = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(cntrl.value), 12);
        gtk_table_attach(table, cntrl.value,
                         3, 4, row, row+1, GTK_FILL, 0, 0, 0);
        g_object_set_data(G_OBJECT(cntrl.value), "id", GUINT_TO_POINTER(i));
        g_signal_connect(cntrl.value, "activate",
                         G_CALLBACK(param_value_activate), controls);
        gwy_widget_set_activate_on_unfocus(cntrl.value, TRUE);

        cntrl.value_unit = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.value_unit), 0.0, 0.5);
        gtk_table_attach(table, cntrl.value_unit,
                         4, 5, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.pm = gtk_label_new("±");
        gtk_table_attach(table, cntrl.pm, 5, 6, row, row+1, 0, 0, 0, 0);

        cntrl.error = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.error), 1.0, 0.5);
        gtk_table_attach(table, cntrl.error,
                         6, 7, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.error_unit = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.error_unit), 0.0, 0.5);
        gtk_table_attach(table, cntrl.error_unit,
                         7, 8, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.magnitude = 1.0;
        g_array_append_val(controls->param_controls, cntrl);
        row++;
    }

    for (i = 0; i < nparams; i++) {
        FitParamControl *cntrl = &g_array_index(controls->param_controls,
                                                FitParamControl, i);
        const gchar *name
            = gwy_shape_fit_preset_get_param_name(controls->preset, i);

        gtk_label_set_markup(GTK_LABEL(cntrl->name), name);
    }

    gtk_widget_show_all(controls->param_table);
}

static GtkWidget*
correl_tab_new(FitShapeControls *controls)
{
    GtkWidget *scwin;
    GtkTable *table;

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scwin), 4);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);

    controls->correl_table = gtk_table_new(1, 1, TRUE);
    table = GTK_TABLE(controls->correl_table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scwin),
                                          controls->correl_table);

    controls->correl_values = g_ptr_array_new();
    controls->correl_hlabels = g_ptr_array_new();
    controls->correl_vlabels = g_ptr_array_new();

    return scwin;
}

static void
fit_correl_table_resize(FitShapeControls *controls)
{
    GtkTable *table;
    GtkWidget *label;
    guint i, j, nparams;
    GPtrArray *vlabels = controls->correl_vlabels,
              *hlabels = controls->correl_hlabels,
              *values = controls->correl_values;

    nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
    gwy_debug("%u -> %u", hlabels->len, nparams);
    if (hlabels->len != nparams) {
        for (i = 0; i < hlabels->len; i++)
            gtk_widget_destroy((GtkWidget*)g_ptr_array_index(hlabels, i));
        g_ptr_array_set_size(hlabels, 0);

        for (i = 0; i < vlabels->len; i++)
            gtk_widget_destroy((GtkWidget*)g_ptr_array_index(vlabels, i));
        g_ptr_array_set_size(vlabels, 0);

        for (i = 0; i < values->len; i++)
            gtk_widget_destroy((GtkWidget*)g_ptr_array_index(values, i));
        g_ptr_array_set_size(values, 0);

        table = GTK_TABLE(controls->correl_table);
        gtk_table_resize(table, nparams+1, nparams+1);

        for (i = 0; i < nparams; i++) {
            label = gtk_label_new(NULL);
            gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
            gtk_table_attach(table, label, 0, 1, i, i+1, GTK_FILL, 0, 0, 0);
            g_ptr_array_add(vlabels, label);
        }

        for (i = 0; i < nparams; i++) {
            label = gtk_label_new(NULL);
            gtk_table_attach(table, label, i+1, i+2, nparams, nparams+1,
                             GTK_FILL, 0, 0, 0);
            g_ptr_array_add(hlabels, label);
        }

        for (i = 0; i < nparams; i++) {
            for (j = 0; j <= i; j++) {
                label = gtk_label_new(NULL);
                gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
                gtk_table_attach(table, label, j+1, j+2, i, i+1,
                                 GTK_FILL, 0, 0, 0);
                g_ptr_array_add(values, label);
            }
        }
    }

    for (i = 0; i < nparams; i++) {
        const gchar *name
            = gwy_shape_fit_preset_get_param_name(controls->preset, i);

        gtk_label_set_markup(g_ptr_array_index(vlabels, i), name);
        gtk_label_set_markup(g_ptr_array_index(hlabels, i), name);
    }

    gtk_widget_show_all(controls->correl_table);
}

static GtkWidget*
secondary_tab_new(FitShapeControls *controls)
{
    GtkTable *table;

    controls->secondary_table = gtk_table_new(1, 7, FALSE);
    table = GTK_TABLE(controls->secondary_table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 2);
    gtk_table_set_col_spacing(table, 3, 6);
    gtk_table_set_col_spacing(table, 4, 6);
    gtk_table_set_col_spacing(table, 6, 6);

    controls->secondary_controls = g_array_new(FALSE, FALSE,
                                               sizeof(FitParamControl));
    return controls->secondary_table;
}

static void
fit_secondary_table_resize(FitShapeControls *controls)
{
    GtkTable *table;
    guint i, row, old_nsecondary, nsecondary;

    old_nsecondary = controls->secondary_controls->len;
    nsecondary = gwy_shape_fit_preset_get_nsecondary(controls->preset);
    gwy_debug("%u -> %u", old_nsecondary, nsecondary);
    for (i = old_nsecondary; i > nsecondary; i--) {
        FitParamControl *cntrl = &g_array_index(controls->secondary_controls,
                                                FitParamControl, i-1);
        gtk_widget_destroy(cntrl->name);
        gtk_widget_destroy(cntrl->equals);
        gtk_widget_destroy(cntrl->value);
        gtk_widget_destroy(cntrl->value_unit);
        gtk_widget_destroy(cntrl->pm);
        gtk_widget_destroy(cntrl->error);
        gtk_widget_destroy(cntrl->error_unit);
        g_array_set_size(controls->secondary_controls, i-1);
    }

    table = GTK_TABLE(controls->secondary_table);
    gtk_table_resize(table, 1+nsecondary, 8);
    row = old_nsecondary + 1;

    for (i = old_nsecondary; i < nsecondary; i++) {
        FitParamControl cntrl;

        cntrl.name = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.name), 1.0, 0.5);
        gtk_table_attach(table, cntrl.name,
                         0, 1, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.equals = gtk_label_new("=");
        gtk_table_attach(table, cntrl.equals, 1, 2, row, row+1, 0, 0, 0, 0);

        cntrl.value = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.value), 1.0, 0.5);
        gtk_table_attach(table, cntrl.value,
                         2, 3, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.value_unit = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.value_unit), 0.0, 0.5);
        gtk_table_attach(table, cntrl.value_unit,
                         3, 4, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.pm = gtk_label_new("±");
        gtk_table_attach(table, cntrl.pm, 4, 5, row, row+1, 0, 0, 0, 0);

        cntrl.error = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.error), 1.0, 0.5);
        gtk_table_attach(table, cntrl.error,
                         5, 6, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.error_unit = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.error_unit), 0.0, 0.5);
        gtk_table_attach(table, cntrl.error_unit,
                         6, 7, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.magnitude = 1.0;
        g_array_append_val(controls->secondary_controls, cntrl);
        row++;
    }

    for (i = 0; i < nsecondary; i++) {
        FitParamControl *cntrl = &g_array_index(controls->secondary_controls,
                                                FitParamControl, i);
        const gchar *name
            = gwy_shape_fit_preset_get_secondary_name(controls->preset, i);

        gtk_label_set_markup(GTK_LABEL(cntrl->name), name);
    }

    gtk_widget_show_all(controls->secondary_table);
}

/* TODO TODO TODO TODO TODO disable items based on needs_same_units. */
static void
render_translated_name(G_GNUC_UNUSED GtkCellLayout *layout,
                       GtkCellRenderer *renderer,
                       GtkTreeModel *model,
                       GtkTreeIter *iter,
                       gpointer data)
{
    guint i = GPOINTER_TO_UINT(data);
    const gchar *text;

    gtk_tree_model_get(model, iter, i, &text, -1);
    g_object_set(renderer, "text", _(text), NULL);
}

static GtkWidget*
function_menu_new(const gchar *name, GwyDataField *dfield,
                  FitShapeControls *controls)
{
    GwyInventory *inventory;
    GwyInventoryStore *store;
    GtkCellRenderer *renderer;
    GwySIUnit *xyunit, *zunit;
    gboolean same_units;
    GtkWidget *combo;
    guint i;

    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    zunit = gwy_data_field_get_si_unit_z(dfield);
    same_units = gwy_si_unit_equal(xyunit, zunit);

    /* First try to find function @name.  If it was excluded fall back to the
     * default function. */
    inventory = gwy_shape_fit_presets();
    controls->preset = gwy_inventory_get_item_or_default(inventory, name);
    if (gwy_shape_fit_preset_needs_same_units(controls->preset)
        && !same_units) {
        controls->preset = gwy_inventory_get_item(inventory,
                                                  fit_shape_defaults.function);
    }
    name = gwy_resource_get_name(GWY_RESOURCE(controls->preset));

    store = gwy_inventory_store_new(inventory);
    combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    i = gwy_inventory_store_get_column_by_name(store, "name");
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo), renderer,
                                       render_translated_name,
                                       GUINT_TO_POINTER(i), NULL);
    i = gwy_inventory_get_item_position(inventory, name);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), i);
    g_signal_connect(combo, "changed", G_CALLBACK(function_changed), controls);

    return combo;
}

static void
function_changed(GtkComboBox *combo, FitShapeControls *controls)
{
    guint i = gwy_enum_combo_box_get_active(combo);
    FitShapeContext *ctx = controls->ctx;
    guint nparams, nsecondary;

    controls->preset = gwy_inventory_get_nth_item(gwy_shape_fit_presets(), i);
    nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
    nsecondary = gwy_shape_fit_preset_get_nsecondary(controls->preset);

    controls->param = g_renew(gdouble, controls->param, nparams);
    controls->alt_param = g_renew(gdouble, controls->alt_param, nparams);
    controls->param_err = g_renew(gdouble, controls->param_err, nparams);
    controls->secondary = g_renew(gdouble, controls->secondary, nsecondary);
    controls->secondary_err = g_renew(gdouble, controls->secondary_err,
                                      nsecondary);
    controls->correl = g_renew(gdouble, controls->correl,
                               (nparams + 1)*nparams/2);
    for (i = 0; i < nparams; i++)
        controls->param_err[i] = -1.0;
    fit_param_table_resize(controls);
    fit_correl_table_resize(controls);
    fit_secondary_table_resize(controls);
    fit_context_resize_params(ctx, nparams);
    gwy_shape_fit_preset_setup(controls->preset, ctx->xyz, ctx->n,
                               controls->param);
    controls->state = FIT_SHAPE_INITIALISED;
    fit_copy_correl_matrix(controls, NULL);
    gwy_assign(controls->alt_param, controls->param, nparams);
    calculate_secondary_params(controls);
    update_param_table(controls, controls->param, NULL);
    update_correl_table(controls, NULL);
    update_fit_results(controls, NULL);
    update_fields(controls);
    update_fit_state(controls);
}

static void
display_changed(GtkToggleButton *toggle, FitShapeControls *controls)
{
    GwyPixmapLayer *player;
    GQuark quark;

    if (toggle && !gtk_toggle_button_get_active(toggle))
        return;

    controls->args->display = gwy_radio_buttons_get_current(controls->display);
    player = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));
    quark = gwy_app_get_data_key_for_id(controls->args->display);
    gwy_pixmap_layer_set_data_key(player, g_quark_to_string(quark));
    update_colourmap_key(controls);
}

static void
diff_colourmap_changed(GtkToggleButton *toggle,
                       FitShapeControls *controls)
{
    controls->args->diff_colourmap = gtk_toggle_button_get_active(toggle);
    update_colourmap_key(controls);
}

static void
diff_excluded_changed(GtkToggleButton *toggle,
                      FitShapeControls *controls)
{
    controls->args->diff_excluded = gtk_toggle_button_get_active(toggle);
    if (controls->args->masking != GWY_MASK_IGNORE)
        update_fields(controls);
}

static void
output_changed(GtkComboBox *combo, FitShapeControls *controls)
{
    controls->args->output = gwy_enum_combo_box_get_active(combo);
}

static void
masking_changed(GtkToggleButton *toggle, FitShapeControls *controls)
{
    if (!gtk_toggle_button_get_active(toggle))
        return;

    controls->args->masking = gwy_radio_buttons_get_current(controls->masking);
    update_context_data(controls);
    controls->state = FIT_SHAPE_INITIALISED;
    update_fit_results(controls, NULL);
    if (!controls->args->diff_excluded)
        update_fields(controls);
    update_fit_state(controls);
}

static void
update_colourmap_key(FitShapeControls *controls)
{
    GwyPixmapLayer *player;

    player = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));
    if (controls->args->diff_colourmap
        && controls->args->display == FIT_SHAPE_DISPLAY_DIFF) {
        gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(player),
                                         "/2/base/palette");
    }
    else {
        gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(player),
                                         "/0/base/palette");
    }
}

static void
fix_changed(GtkToggleButton *button, FitShapeControls *controls)
{
    gboolean fixed = gtk_toggle_button_get_active(button);
    guint i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "id"));
    const FitShapeContext *ctx = controls->ctx;

    ctx->param_fixed[i] = fixed;
}

static void
update_param_value(FitShapeControls *controls, guint i)
{
    FitParamControl *cntrl;
    GwyNLFitParamFlags flags;
    GtkEntry *entry;

    cntrl = &g_array_index(controls->param_controls, FitParamControl, i);
    entry = GTK_ENTRY(cntrl->value);
    flags = gwy_shape_fit_preset_get_param_flags(controls->preset, i);
    controls->param[i] = g_strtod(gtk_entry_get_text(entry), NULL);
    controls->param[i] *= cntrl->magnitude;
    if (flags & GWY_NLFIT_PARAM_ANGLE)
        controls->param[i] *= G_PI/180.0;
    if (flags & GWY_NLFIT_PARAM_ABSVAL)
        controls->param[i] = fabs(controls->param[i]);
}

static void
param_value_activate(GtkEntry *entry, FitShapeControls *controls)
{
    guint i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(entry), "id"));

    update_param_value(controls, i);
    /* This (a) clears error labels in the table (b) reformats the parameter,
     * e.g. by moving the power-of-10 base appropriately. */
    controls->state = FIT_SHAPE_USER;
    calculate_secondary_params(controls);
    update_param_table(controls, controls->param, NULL);
    update_correl_table(controls, NULL);
    update_secondary_table(controls);
    update_fit_state(controls);
}

static void
update_all_param_values(FitShapeControls *controls)
{
    guint i;
    for (i = 0; i < controls->param_controls->len; i++)
        update_param_value(controls, i);
}

static void
revert_params(FitShapeControls *controls)
{
    guint i, nparams;

    nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
    update_all_param_values(controls);
    for (i = 0; i < nparams; i++)
        GWY_SWAP(gdouble, controls->param[i], controls->alt_param[i]);

    controls->state = FIT_SHAPE_USER;
    calculate_secondary_params(controls);
    update_param_table(controls, controls->param, NULL);
    update_correl_table(controls, NULL);
    update_secondary_table(controls);
    update_fit_state(controls);
}

static void
recalculate_image(FitShapeControls *controls)
{
    controls->state = FIT_SHAPE_USER;
    update_all_param_values(controls);
    update_fit_results(controls, NULL);
    update_fields(controls);
    update_fit_state(controls);
}

static void
update_param_table(FitShapeControls *controls,
                   const gdouble *param, const gdouble *param_err)
{
    guint i, nparams;
    GwyDataField *dfield;
    GwySIUnit *unit, *xyunit, *zunit;
    GwySIValueFormat *vf = NULL;

    nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    zunit = gwy_data_field_get_si_unit_z(dfield);
    unit = gwy_si_unit_new(NULL);

    for (i = 0; i < nparams; i++) {
        FitParamControl *cntrl = &g_array_index(controls->param_controls,
                                                FitParamControl, i);
        GwyNLFitParamFlags flags;
        guchar buf[32];
        gdouble v;

        flags = gwy_shape_fit_preset_get_param_flags(controls->preset, i);
        v = param[i];
        if (flags & GWY_NLFIT_PARAM_ANGLE) {
            v *= 180.0/G_PI;
            gwy_si_unit_set_from_string(unit, "deg");
        }
        else {
            g_object_unref(unit);
            unit = gwy_shape_fit_preset_get_param_units(controls->preset, i,
                                                        xyunit, zunit);
        }
        /* If the user enters exact zero, do not update the magnitude because
         * that means an unexpected reset to base units. */
        if (G_UNLIKELY(v == 0.0)) {
            gint power10 = GWY_ROUND(log10(cntrl->magnitude));
            vf = gwy_si_unit_get_format_for_power10(unit,
                                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                    power10, vf);
        }
        else {
            vf = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                        v, vf);
        }
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision+3, v/vf->magnitude);
        gtk_entry_set_text(GTK_ENTRY(cntrl->value), buf);
        gtk_label_set_markup(GTK_LABEL(cntrl->value_unit), vf->units);
        cntrl->magnitude = vf->magnitude;

        if (!param_err) {
            gtk_label_set_text(GTK_LABEL(cntrl->error), "");
            gtk_label_set_text(GTK_LABEL(cntrl->error_unit), "");
            continue;
        }

        v = param_err[i];
        if (flags & GWY_NLFIT_PARAM_ANGLE)
            v *= 180.0/G_PI;
        vf = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, v/vf->magnitude);
        gtk_label_set_text(GTK_LABEL(cntrl->error), buf);
        gtk_label_set_markup(GTK_LABEL(cntrl->error_unit), vf->units);
    }

    gwy_si_unit_value_format_free(vf);
    g_object_unref(unit);
}

static void
update_correl_table(FitShapeControls *controls, GwyNLFitter *fitter)
{
    GdkColor gdkcolor_bad = { 0, 51118, 0, 0 };
    GdkColor gdkcolor_warning = { 0, 45056, 20480, 0 };

    const gboolean *param_fixed = controls->ctx->param_fixed;
    GPtrArray *values = controls->correl_values;
    guint i, j, nparams;

    nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
    g_assert(values->len == (nparams + 1)*nparams/2);
    gwy_debug("fitter %p", fitter);

    for (i = 0; i < nparams; i++) {
        for (j = 0; j <= i; j++) {
            GtkWidget *label = g_ptr_array_index(values, i*(i + 1)/2 + j);

            if (fitter) {
                gchar buf[16];
                gdouble c = SLi(controls->correl, i, j);

                if (param_fixed[i] || param_fixed[j]) {
                    if (i == j) {
                        g_snprintf(buf, sizeof(buf), "%.3f", 1.0);
                        gtk_label_set_text(GTK_LABEL(label), buf);
                    }
                    else
                        gtk_label_set_text(GTK_LABEL(label), "—");
                    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, NULL);
                    continue;
                }

                g_snprintf(buf, sizeof(buf), "%.3f", c);
                gtk_label_set_text(GTK_LABEL(label), buf);
                if (i != j) {
                    if (fabs(c) >= 0.99)
                        gtk_widget_modify_fg(label, GTK_STATE_NORMAL,
                                             &gdkcolor_bad);
                    else if (fabs(c) >= 0.9)
                        gtk_widget_modify_fg(label, GTK_STATE_NORMAL,
                                             &gdkcolor_warning);
                    else
                        gtk_widget_modify_fg(label, GTK_STATE_NORMAL, NULL);
                }
            }
            else
                gtk_label_set_text(GTK_LABEL(label), "");

        }
    }

    /* For some reason, this does not happen automatically after the set-text
     * call so the labels that had initially zero width remain invisible even
     * though there is a number to display now. */
    if (fitter)
        gtk_widget_queue_resize(controls->correl_table);
}

static void
update_secondary_table(FitShapeControls *controls)
{
    guint i, nsecondary;
    GwyDataField *dfield;
    GwySIUnit *unit, *xyunit, *zunit;
    GwySIValueFormat *vf = NULL;
    gboolean is_fitted = (controls->state == FIT_SHAPE_FITTED
                          || controls->state == FIT_SHAPE_QUICK_FITTED);

    nsecondary = gwy_shape_fit_preset_get_nsecondary(controls->preset);
    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    zunit = gwy_data_field_get_si_unit_z(dfield);
    unit = gwy_si_unit_new(NULL);

    for (i = 0; i < nsecondary; i++) {
        FitParamControl *cntrl = &g_array_index(controls->secondary_controls,
                                                FitParamControl, i);
        GwyNLFitParamFlags flags;
        guchar buf[32];
        gdouble v;

        flags = gwy_shape_fit_preset_get_secondary_flags(controls->preset, i);
        v = controls->secondary[i];
        if (flags & GWY_NLFIT_PARAM_ANGLE) {
            v *= 180.0/G_PI;
            gwy_si_unit_set_from_string(unit, "deg");
        }
        else {
            g_object_unref(unit);
            unit = gwy_shape_fit_preset_get_secondary_units(controls->preset, i,
                                                            xyunit, zunit);
        }
        vf = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision+3, v/vf->magnitude);
        gtk_label_set_text(GTK_LABEL(cntrl->value), buf);
        gtk_label_set_markup(GTK_LABEL(cntrl->value_unit), vf->units);

        if (!is_fitted) {
            gtk_label_set_text(GTK_LABEL(cntrl->error), "");
            gtk_label_set_text(GTK_LABEL(cntrl->error_unit), "");
            continue;
        }

        v = controls->secondary_err[i];
        if (flags & GWY_NLFIT_PARAM_ANGLE)
            v *= 180.0/G_PI;
        vf = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, v/vf->magnitude);
        gtk_label_set_text(GTK_LABEL(cntrl->error), buf);
        gtk_label_set_markup(GTK_LABEL(cntrl->error_unit), vf->units);
    }

    GWY_SI_VALUE_FORMAT_FREE(vf);
    g_object_unref(unit);
}

static void
fit_shape_estimate(FitShapeControls *controls)
{
    const FitShapeContext *ctx = controls->ctx;
    guint i, nparams;

    gwy_app_wait_cursor_start(GTK_WINDOW(controls->dialogue));
    gwy_debug("start estimate");
    nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
    gwy_assign(controls->alt_param, controls->param, nparams);
    if (gwy_shape_fit_preset_guess(controls->preset, ctx->xyz, ctx->n,
                                   controls->param))
        controls->state = FIT_SHAPE_ESTIMATED;
    else
        controls->state = FIT_SHAPE_ESTIMATE_FAILED;

    /* XXX: We honour fixed parameters by reverting to previous values and
     * pretending nothing happened.  Is it OK? */
    for (i = 0; i < nparams; i++) {
        gwy_debug("[%u] %g", i, controls->param[i]);
        if (ctx->param_fixed[i])
            controls->param[i] = controls->alt_param[i];
    }
    update_fit_results(controls, NULL);
    update_fields(controls);
    update_fit_state(controls);
    gwy_app_wait_cursor_finish(GTK_WINDOW(controls->dialogue));
}

static void
fit_shape_reduced_fit(FitShapeControls *controls)
{
    const FitShapeContext *ctx = controls->ctx;
    GwyNLFitter *fitter;
    gdouble rss;
    guint nparams;

    gwy_app_wait_cursor_start(GTK_WINDOW(controls->dialogue));
    gwy_debug("start reduced fit");
    update_all_param_values(controls);
    nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
    gwy_assign(controls->alt_param, controls->param, nparams);
    fitter = fit_reduced(controls->preset, ctx, controls->param, &rss);
    if (rss >= 0.0)
        controls->state = FIT_SHAPE_QUICK_FITTED;
    else
        controls->state = FIT_SHAPE_QUICK_FIT_FAILED;

#ifdef DEBUG
    {
        guint i;
        for (i = 0; i < nparams; i++)
            gwy_debug("[%u] %g", i, controls->param[i]);
    }
#endif
    fit_copy_correl_matrix(controls, fitter);
    update_fit_results(controls, fitter);
    update_fields(controls);
    update_fit_state(controls);
    gwy_math_nlfit_free(fitter);
    gwy_app_wait_cursor_finish(GTK_WINDOW(controls->dialogue));
}

static void
fit_shape_full_fit(FitShapeControls *controls)
{
    const FitShapeContext *ctx = controls->ctx;
    GwyNLFitter *fitter;
    gdouble rss;
    guint nparams;

    gwy_app_wait_start(GTK_WINDOW(controls->dialogue), _("Fitting..."));
    gwy_debug("start fit");
    nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
    update_all_param_values(controls);
    gwy_assign(controls->alt_param, controls->param, nparams);
    fitter = fit(controls->preset, ctx, G_MAXUINT, controls->param, &rss,
                 gwy_app_wait_set_fraction, gwy_app_wait_set_message);

    if (rss >= 0.0)
        controls->state = FIT_SHAPE_FITTED;
    else if (rss == -2.0)
        controls->state = FIT_SHAPE_FIT_CANCELLED;
    else
        controls->state = FIT_SHAPE_FIT_FAILED;

#ifdef DEBUG
    {
        guint i;
        for (i = 0; i < nparams; i++)
            gwy_debug("[%u] %g", i, controls->param[i]);
    }
#endif
    fit_copy_correl_matrix(controls, fitter);
    update_fit_results(controls, fitter);
    update_fields(controls);
    update_fit_state(controls);
    gwy_math_nlfit_free(fitter);
    gwy_app_wait_finish();
}

static void
fit_copy_correl_matrix(FitShapeControls *controls, GwyNLFitter *fitter)
{
    guint i, j, nparams;
    gboolean is_fitted = (controls->state == FIT_SHAPE_FITTED
                          || controls->state == FIT_SHAPE_QUICK_FITTED);

    nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
    gwy_clear(controls->correl, (nparams + 1)*nparams/2);

    if (is_fitted) {
        g_return_if_fail(fitter && fitter->covar);

        for (i = 0; i < nparams; i++) {
            for (j = 0; j <= i; j++) {
                SLi(controls->correl, i, j)
                    = gwy_math_nlfit_get_correlations(fitter, i, j);
            }
        }
    }
}

static void
calculate_secondary_params(FitShapeControls *controls)
{
    guint i, nsecondary;
    gboolean is_fitted = (controls->state == FIT_SHAPE_FITTED
                          || controls->state == FIT_SHAPE_QUICK_FITTED);

    nsecondary = gwy_shape_fit_preset_get_nsecondary(controls->preset);
    for (i = 0; i < nsecondary; i++) {
        controls->secondary[i]
            = gwy_shape_fit_preset_get_secondary_value(controls->preset, i,
                                                       controls->param);

        if (is_fitted) {
            controls->secondary_err[i]
                = gwy_shape_fit_preset_get_secondary_error(controls->preset, i,
                                                           controls->param,
                                                           controls->param_err,
                                                           controls->correl);
        }
        else
            controls->secondary_err[i] = 0.0;

        gwy_debug("[%u] %g +- %g",
                  i, controls->secondary[i], controls->secondary_err[i]);
    }
}

static void
update_fields(FitShapeControls *controls)
{
    GwyDataField *dfield, *resfield, *difffield, *mask = NULL;
    GwySurface *surface;
    GwyMaskingType masking = controls->args->masking;
    FitShapeContext *ctx = controls->ctx;
    guint xres, yres, n, k;
    const gdouble *m;
    GwyXYZ *xyz;
    gdouble *d;

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    resfield = gwy_container_get_object_by_name(controls->mydata, "/1/data");
    difffield = gwy_container_get_object_by_name(controls->mydata, "/2/data");

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    n = xres*yres;
    gwy_container_gis_object_by_name(controls->mydata, "/0/mask",
                                     (GObject**)&mask);
    if (controls->pageno == GWY_PAGE_CHANNELS
        && (!mask || masking == GWY_MASK_IGNORE)) {
        /* We know ctx->f contains all the theoretical values. */
        g_assert(ctx->n == n);
        gwy_debug("directly copying f[] to result field");
        gwy_assign(gwy_data_field_get_data(resfield), ctx->f, n);
    }
    else if (controls->pageno == GWY_PAGE_XYZS) {
        surface = gwy_container_get_object_by_name(controls->mydata,
                                                   "/surface/0");
        surface = gwy_surface_duplicate(surface);
        n = gwy_surface_get_npoints(surface);
        g_assert(ctx->n == n);
        xyz = gwy_surface_get_data(surface);
        for (k = 0; k < n; k++)
            xyz[k].z = ctx->f[k];
        gwy_preview_surface_to_datafield(surface, resfield,
                                         PREVIEW_SIZE, PREVIEW_SIZE,
                                         GWY_PREVIEW_SURFACE_FILL);
        g_object_unref(surface);
    }
    else {
        /* Either the input is XYZ or we are using masking.  Just recalculate
         * everything, even values that are in ctx->f. */
        gwy_debug("recalculating result field the hard way");
        calculate_field(controls->preset, controls->param, resfield);
    }

    gwy_data_field_data_changed(resfield);
    gwy_data_field_subtract_fields(difffield, dfield, resfield);
    if (!controls->args->diff_excluded && mask && masking != GWY_MASK_IGNORE) {
        m = gwy_data_field_get_data_const(mask);
        d = gwy_data_field_get_data(difffield);
        if (masking == GWY_MASK_INCLUDE) {
            for (k = 0; k < n; k++) {
                if (m[k] <= 0.0)
                    d[k] = 0.0;
            }
        }
        else if (masking == GWY_MASK_EXCLUDE) {
            for (k = 0; k < n; k++) {
                if (m[k] > 0.0)
                    d[k] = 0.0;
            }
        }
    }
    gwy_data_field_data_changed(difffield);
    update_diff_gradient(controls);
}

static void
update_diff_gradient(FitShapeControls *controls)
{
    static const GwyRGBA rgba_negative = { 0.0, 0.0, 1.0, 1.0 };
    static const GwyRGBA rgba_positive = { 1.0, 0.0, 0.0, 1.0 };
    static const GwyRGBA rgba_neutral = { 1.0, 1.0, 1.0, 1.0 };

    GwyDataField *difffield;
    GwyGradient *gradient = controls->diff_gradient;
    gdouble min, max;

    difffield = gwy_container_get_object_by_name(controls->mydata, "/2/data");
    gwy_data_field_get_min_max(difffield, &min, &max);
    gwy_gradient_reset(gradient);

    /* Stretch the scale to the range when all the data are too high or too
     * low. */
    if (min >= 0.0) {
        gwy_gradient_set_point_color(gradient, 0, &rgba_neutral);
        gwy_gradient_set_point_color(gradient, 1, &rgba_positive);
    }
    else if (max <= 0.0) {
        gwy_gradient_set_point_color(gradient, 0, &rgba_negative);
        gwy_gradient_set_point_color(gradient, 1, &rgba_neutral);
    }
    else if (!(max > min)) {
        GwyGradientPoint zero_pt = { 0.5, rgba_neutral };
        gint pos;

        gwy_gradient_set_point_color(gradient, 0, &rgba_negative);
        gwy_gradient_set_point_color(gradient, 1, &rgba_positive);
        pos = gwy_gradient_insert_point_sorted(gradient, &zero_pt);
        g_assert(pos == 1);
    }
    else {
        /* Otherwise make zero neutral and map the two colours to both side,
         * with the same scale. */
        gdouble zero = -min/(max - min);
        GwyGradientPoint zero_pt = { zero, rgba_neutral };
        GwyRGBA rgba;
        gint pos;

        if (zero <= 0.5) {
            gwy_rgba_interpolate(&rgba_neutral, &rgba_negative,
                                 zero/(1.0 - zero), &rgba);
            gwy_gradient_set_point_color(gradient, 0, &rgba);
            gwy_gradient_set_point_color(gradient, 1, &rgba_positive);
        }
        else {
            gwy_gradient_set_point_color(gradient, 0, &rgba_negative);
            gwy_rgba_interpolate(&rgba_neutral, &rgba_positive,
                                 (1.0 - zero)/zero, &rgba);
            gwy_gradient_set_point_color(gradient, 1, &rgba);
        }
        pos = gwy_gradient_insert_point_sorted(gradient, &zero_pt);
        g_assert(pos == 1);
    }
}

static void
update_fit_state(FitShapeControls *controls)
{
    GdkColor gdkcolor = { 0, 51118, 0, 0 };
    const gchar *message = "";

    if (controls->state == FIT_SHAPE_ESTIMATE_FAILED)
        message = _("Parameter estimation failed");
    else if (controls->state == FIT_SHAPE_FIT_FAILED
             || controls->state == FIT_SHAPE_QUICK_FIT_FAILED)
        message = _("Fit failed");
    else if (controls->state == FIT_SHAPE_FIT_CANCELLED)
        message = _("Fit was interruped");

    gtk_widget_modify_fg(controls->fit_message, GTK_STATE_NORMAL, &gdkcolor);
    gtk_label_set_text(GTK_LABEL(controls->fit_message), message);

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialogue),
                                      RESPONSE_SAVE,
                                      controls->state == FIT_SHAPE_FITTED);
}

static void
update_fit_results(FitShapeControls *controls, GwyNLFitter *fitter)
{
    const FitShapeContext *ctx = controls->ctx;
    GwyDataField *dfield;
    gdouble z, rss = 0.0;
    guint k, n = ctx->n, i, nparams;
    GwySIUnit *zunit;
    GwySIValueFormat *vf;
    gboolean is_fitted = (controls->state == FIT_SHAPE_FITTED
                          || controls->state == FIT_SHAPE_QUICK_FITTED);
    const GwyXYZ *xyz = ctx->xyz;
    guchar buf[48];

    if (is_fitted)
        g_return_if_fail(fitter);

    gwy_shape_fit_preset_calculate_z(controls->preset, xyz, ctx->f, n,
                                     controls->param);
    for (k = 0; k < n; k++) {
        z = ctx->f[k] - xyz[k].z;
        rss += z*z;
    }
    controls->rss = sqrt(rss/n);

    if (is_fitted) {
        nparams = gwy_shape_fit_preset_get_nparams(controls->preset);
        for (i = 0; i < nparams; i++)
            controls->param_err[i] = gwy_math_nlfit_get_sigma(fitter, i);
    }

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    zunit = gwy_data_field_get_si_unit_z(dfield);
    vf = gwy_si_unit_get_format(zunit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                controls->rss, NULL);

    g_snprintf(buf, sizeof(buf), "%.*f%s%s",
               vf->precision+1, controls->rss/vf->magnitude,
               *vf->units ? " " : "", vf->units);
    gtk_label_set_markup(GTK_LABEL(controls->rss_label), buf);

    gwy_si_unit_value_format_free(vf);

    calculate_secondary_params(controls);
    update_param_table(controls, controls->param,
                       is_fitted ? controls->param_err : NULL);
    update_correl_table(controls, is_fitted ? fitter : NULL);
    update_secondary_table(controls);
}

static void
update_context_data(FitShapeControls *controls)
{
    GwyDataField *dfield = NULL, *mfield = NULL;
    GwySurface *surface = NULL;
    FitShapeContext *ctx = controls->ctx;

    if (controls->pageno == GWY_PAGE_XYZS) {
        surface = gwy_container_get_object_by_name(controls->mydata,
                                                   "/surface/0");
        if (!ctx->surface) {
            ctx->surface = surface;
            g_object_ref(ctx->surface);
        }
    }
    else {
        dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
        gwy_container_gis_object_by_name(controls->mydata, "/0/mask",
                                         (GObject**)&mfield);
        if (!ctx->surface)
            ctx->surface = gwy_surface_new();
        gwy_surface_set_from_data_field_mask(ctx->surface, dfield,
                                             mfield, controls->args->masking);
    }

    ctx->n = gwy_surface_get_npoints(ctx->surface);
    ctx->f = g_renew(gdouble, ctx->f, ctx->n);
    ctx->xyz = gwy_surface_get_data_const(ctx->surface);
}

static void
fit_context_resize_params(FitShapeContext *ctx,
                          guint n_param)
{
    guint i;

    ctx->nparam = n_param;
    ctx->param_fixed = g_renew(gboolean, ctx->param_fixed, n_param);
    for (i = 0; i < n_param; i++) {
        ctx->param_fixed[i] = FALSE;
    }
}

static void
fit_context_free(FitShapeContext *ctx)
{
    GWY_OBJECT_UNREF(ctx->surface);
    g_free(ctx->param_fixed);
    g_free(ctx->f);
    gwy_clear(ctx, 1);
}

static GwyNLFitter*
fit(GwyShapeFitPreset *preset, const FitShapeContext *ctx,
    guint maxiter, gdouble *param, gdouble *rss,
    GwySetFractionFunc set_fraction, GwySetMessageFunc set_message)
{
    GwyNLFitter *fitter;

    fitter = gwy_shape_fit_preset_create_fitter(preset);
    if (set_fraction || set_message)
        gwy_math_nlfit_set_callbacks(fitter, set_fraction, set_message);
    if (maxiter != G_MAXUINT)
        gwy_math_nlfit_set_max_iterations(fitter, maxiter);

    gwy_shape_fit_preset_fit(preset, fitter, ctx->xyz, ctx->n,
                             param, ctx->param_fixed, rss);
    gwy_debug("rss from nlfit %g", *rss);

    return fitter;
}

/* XXX XXX XXX XXX XXX This is duplicated in gwyshapefitpreset.c and it should
 * probably go to the library.  There we can do something better in the future
 * than random choice, for instance a mix of spatially uniform and random
 * choice. */
static GwyNLFitter*
fit_reduced(GwyShapeFitPreset *preset, const FitShapeContext *ctx,
            gdouble *param, gdouble *rss)
{
    GwyNLFitter *fitter;
    FitShapeContext ctxred;
    guint nred = (guint)sqrt(ctx->n*(gdouble)NREDLIM);

    if (nred >= ctx->n)
        return fit(preset, ctx, 30, param, rss, NULL, NULL);

    ctxred = *ctx;
    ctxred.n = nred;
    ctxred.surface = gwy_surface_new_sized(nred);
    ctxred.xyz = gwy_surface_get_data_const(ctxred.surface);
    reduce_data_size(gwy_surface_get_data_const(ctx->surface), ctx->n,
                     ctxred.surface);
    fitter = fit(preset, &ctxred, 30, param, rss, NULL, NULL);
    g_object_unref(ctxred.surface);

    return fitter;
}

static void
calculate_field(GwyShapeFitPreset *preset, const gdouble *params,
                GwyDataField *dfield)
{
    GwySurface *surface = gwy_surface_new();

    gwy_surface_set_from_data_field_mask(surface, dfield,
                                         NULL, GWY_MASK_IGNORE);
    gwy_shape_fit_preset_calculate_z(preset,
                                     gwy_surface_get_data_const(surface),
                                     gwy_data_field_get_data(dfield),
                                     gwy_surface_get_npoints(surface),
                                     params);
    g_object_unref(surface);
}

static void
reduce_data_size(const GwyXYZ *xyzsrc, guint nsrc, GwySurface *dest)
{
    GwyRandGenSet *rngset = gwy_rand_gen_set_new(1);
    guint ndest = gwy_surface_get_npoints(dest);
    guint *redindex = gwy_rand_gen_set_choose_shuffle(rngset, 0, nsrc, ndest);
    GwyXYZ *xyzdest = gwy_surface_get_data(dest);
    guint i;

    for (i = 0; i < ndest; i++)
        xyzdest[i] = xyzsrc[redindex[i]];

    g_free(redindex);
    gwy_rand_gen_set_free(rngset);
}

static GString*
create_fit_report(FitShapeControls *controls)
{
    GwyShapeFitPreset *preset = controls->preset;
    GwyDataField *dfield;
    GwySurface *surface;
    GwySIUnit *xyunit, *zunit, *unit;
    gchar *s, *unitstr;
    GString *report;
    guint n, i, j, nparams, nsecondary;
    const gboolean *param_fixed = controls->ctx->param_fixed;

    if (controls->pageno == GWY_PAGE_XYZS) {
        surface = gwy_container_get_object_by_name(controls->mydata,
                                                   "/surface/0");
        xyunit = gwy_surface_get_si_unit_xy(surface);
        zunit = gwy_surface_get_si_unit_z(surface);
        n = gwy_surface_get_npoints(surface);
    }
    else {
        dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
        xyunit = gwy_data_field_get_si_unit_xy(dfield);
        zunit = gwy_data_field_get_si_unit_z(dfield);
        n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
    }
    unit = gwy_si_unit_new(NULL);

    report = g_string_new(NULL);

    g_string_append(report, _("===== Fit Results ====="));
    g_string_append_c(report, '\n');
    g_string_append_printf(report, _("Data:             %s\n"),
                           controls->title);
    g_string_append_printf(report, _("Number of points: %d of %d\n"),
                           controls->ctx->n, n);
    g_string_append_printf(report, _("Fitted function:  %s\n"),
                           gwy_resource_get_name(GWY_RESOURCE(preset)));
    g_string_append_c(report, '\n');
    g_string_append_printf(report, _("Results\n"));

    nparams = gwy_shape_fit_preset_get_nparams(preset);
    for (i = 0; i < nparams; i++) {
        const gchar *name = gwy_shape_fit_preset_get_param_name(preset, i);
        gdouble param = controls->param[i], err = controls->param_err[i];
        GwyNLFitParamFlags flags;

        if (!pango_parse_markup(name, -1, 0, NULL, &s, NULL, NULL)) {
            g_warning("Parameter name is not valid Pango markup");
            s = g_strdup(name);
        }
        flags = gwy_shape_fit_preset_get_param_flags(preset, i);
        if (flags & GWY_NLFIT_PARAM_ANGLE) {
            param *= 180.0/G_PI;
            err *= 180.0/G_PI;
            unitstr = g_strdup("deg");
        }
        else {
            g_object_unref(unit);
            unit = gwy_shape_fit_preset_get_param_units(preset, i,
                                                        xyunit, zunit);
            unitstr = gwy_si_unit_get_string(unit, GWY_SI_UNIT_FORMAT_PLAIN);
        }

        if (param_fixed[i]) {
            g_string_append_printf(report, "%6s = %g %s%s %s\n",
                                   s, param, *unitstr ? " " : "", unitstr,
                                   _("(fixed)"));
        }
        else {
            g_string_append_printf(report, "%6s = %g ± %g%s%s\n",
                                   s, param, err, *unitstr ? " " : "", unitstr);
        }

        g_free(unitstr);
        g_free(s);
    }
    g_string_append_c(report, '\n');

    unitstr = gwy_si_unit_get_string(zunit, GWY_SI_UNIT_FORMAT_PLAIN);
    g_string_append_printf(report, "%s %g %s\n",
                           _("Mean square difference:"),
                           controls->rss, unitstr);
    g_free(unitstr);
    g_string_append_c(report, '\n');

    g_string_append(report, _("Correlation Matrix"));
    g_string_append_c(report, '\n');

    for (i = 0; i < nparams; i++) {
        g_string_append(report, "  ");
        for (j = 0; j <= i; j++) {
            if (param_fixed[i] || param_fixed[j]) {
                if (i == j)
                    g_string_append_printf(report, "% .03f", 1.0);
                else
                    g_string_append(report, "   ---");
            }
            else {
                g_string_append_printf(report, "% .03f",
                                       SLi(controls->correl, i, j));
            }
            if (j != i)
                g_string_append_c(report, ' ');
        }
        g_string_append_c(report, '\n');
    }
    g_string_append_c(report, '\n');

    nsecondary = gwy_shape_fit_preset_get_nsecondary(preset);
    if (nsecondary) {
        g_string_append(report, _("Derived Quantities"));
        g_string_append_c(report, '\n');
    }
    for (i = 0; i < nsecondary; i++) {
        const gchar *name = gwy_shape_fit_preset_get_secondary_name(preset, i);
        gdouble param = controls->secondary[i],
                err = controls->secondary_err[i];
        GwyNLFitParamFlags flags;

        if (!pango_parse_markup(name, -1, 0, NULL, &s, NULL, NULL)) {
            g_warning("Parameter name is not valid Pango markup");
            s = g_strdup(name);
        }
        flags = gwy_shape_fit_preset_get_secondary_flags(preset, i);
        if (flags & GWY_NLFIT_PARAM_ANGLE) {
            param *= 180.0/G_PI;
            err *= 180.0/G_PI;
            unitstr = g_strdup("deg");
        }
        else {
            g_object_unref(unit);
            unit = gwy_shape_fit_preset_get_secondary_units(preset, i,
                                                            xyunit, zunit);
            unitstr = gwy_si_unit_get_string(unit, GWY_SI_UNIT_FORMAT_PLAIN);
        }
        g_string_append_printf(report, "%6s = %g ± %g%s%s\n",
                               s, param, err, *unitstr ? " " : "", unitstr);
        g_free(unitstr);
        g_free(s);
    }
    if (nsecondary)
        g_string_append_c(report, '\n');

    g_object_unref(unit);

    return report;
}

static const gchar diff_colourmap_key[] = "/module/fit_shape/diff_colourmap";
static const gchar diff_excluded_key[]  = "/module/fit_shape/diff_excluded";
static const gchar display_key[]        = "/module/fit_shape/display";
static const gchar function_key[]       = "/module/fit_shape/function";
static const gchar masking_key[]        = "/module/fit_shape/masking";
static const gchar output_key[]         = "/module/fit_shape/output";

static void
fit_shape_sanitize_args(FitShapeArgs *args)
{
    args->masking = gwy_enum_sanitize_value(args->masking,
                                            GWY_TYPE_MASKING_TYPE);
    args->display = MIN(args->display, FIT_SHAPE_DISPLAY_DIFF);
    args->output = MIN(args->output, FIT_SHAPE_OUTPUT_BOTH);
    args->diff_colourmap = !!args->diff_colourmap;
    args->diff_excluded = !!args->diff_excluded;
    if (gwy_inventory_get_item_position(gwy_shape_fit_presets(),
                                        args->function) == (guint)-1)
        args->function = fit_shape_defaults.function;
}

static void
fit_shape_load_args(GwyContainer *container,
                    FitShapeArgs *args)
{
    *args = fit_shape_defaults;

    gwy_container_gis_string_by_name(container, function_key,
                                     (const guchar**)&args->function);
    gwy_container_gis_enum_by_name(container, display_key, &args->display);
    gwy_container_gis_enum_by_name(container, masking_key, &args->masking);
    gwy_container_gis_enum_by_name(container, output_key, &args->output);
    gwy_container_gis_boolean_by_name(container, diff_colourmap_key,
                                      &args->diff_colourmap);
    gwy_container_gis_boolean_by_name(container, diff_excluded_key,
                                      &args->diff_excluded);
    fit_shape_sanitize_args(args);
}

static void
fit_shape_save_args(GwyContainer *container,
                    FitShapeArgs *args)
{
    gwy_container_set_const_string_by_name(container, function_key,
                                           args->function);
    gwy_container_set_enum_by_name(container, display_key, args->display);
    gwy_container_set_enum_by_name(container, masking_key, args->masking);
    gwy_container_set_enum_by_name(container, output_key, args->output);
    gwy_container_set_boolean_by_name(container, diff_colourmap_key,
                                      args->diff_colourmap);
    gwy_container_set_boolean_by_name(container, diff_excluded_key,
                                      args->diff_excluded);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
