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

/* XXX: Write all estimation and fitting functions for point clouds.  This
 * means we can easily update this module to handle XYZ data later. */
/* TODO:
 * - Weights; either remove them or implement the support properly.
 * - Handle fit failure, estimate failure.
 * - Parameter table export.
 * - Support parameter transforms between user/internal?  Rad vs. deg,
 *   curvature vs. radius...
 */

#define DEBUG 1
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
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>
#include "preview.h"

#define FIT_SHAPE_RUN_MODES GWY_RUN_INTERACTIVE

/* Caching seems totally worth it.  It adds some small time every function call
 * and it does not improve anything when we care calculating derivatives by
 * the cached parameter, but for evaluation and derivatives by any other
 * paramter it speeds up the functions considerably. */
#define FIT_SHAPE_CACHE 1

enum { NREDLIM = 4096 };

typedef enum {
    FIT_SHAPE_DISPLAY_DATA   = 0,
    FIT_SHAPE_DISPLAY_RESULT = 1,
    FIT_SHAPE_DISPLAY_DIFF   = 2
} FitShapeDisplayType;

typedef enum {
    FIT_SHAPE_OUTPUT_NONE = 0,
    FIT_SHAPE_OUTPUT_FIT  = 1,
    FIT_SHAPE_OUTPUT_DIFF = 2,
    FIT_SHAPE_OUTPUT_BOTH = 3,
} FitShapeOutputType;

typedef struct {
    const gchar *function;
    GwyMaskingType masking;
    FitShapeDisplayType display;
    FitShapeOutputType output;
} FitShapeArgs;

typedef gboolean (*FitShapeEstimate)(const GwyXY *xy,
                                     const gdouble *z,
                                     guint n,
                                     gdouble *param);

typedef struct {
    const char *name;
    gint power_xy;
    gint power_z;
} FitShapeParam;

/* XXX: We may need two sets of parameters: nice to fit (e.g. curvature
 * for spheres) and nice for user (e.g. radius for sphere).  A one-to-one
 * transformation function between the two sets is then required. */
typedef struct {
    const gchar *name;
    gboolean needs_same_units;
    GwyNLFitFunc function;
    FitShapeEstimate estimate;
    FitShapeEstimate init_param;
    guint nparams;
    const FitShapeParam *param;
} FitShapeFunc;

typedef struct {
    guint nparam;
    gboolean *param_fixed;

    guint n;
    gdouble *abscissa;
    GwyXY *xy;
    gdouble *z;
    gdouble *w;
} FitShapeContext;

typedef struct {
    GtkWidget *fix;
    GtkWidget *name;
    GtkWidget *equals;
    GtkWidget *value;
    GtkWidget *value_unit;
    GtkWidget *pm;
    GtkWidget *error;
    GtkWidget *error_unit;
    gdouble magnitude;
} FitParamControl;

typedef struct {
    FitShapeArgs *args;
    FitShapeContext *ctx;
    gint id;
    guint function_id;
    gdouble *param;
    gdouble *alt_param;
    gdouble *param_err;
    gdouble rss;
    GwyContainer *mydata;
    GtkWidget *dialogue;
    GtkWidget *view;
    GtkWidget *function;
    GtkWidget *display;
    GtkWidget *output;
    GtkWidget *rss_label;
    GtkWidget *revert;
    GtkWidget *recalculate;
    GSList *masking;
    GtkWidget *param_table;
    GArray *param_controls;
} FitShapeControls;

static gboolean     module_register          (void);
static void         fit_shape                (GwyContainer *data,
                                              GwyRunType run);
static void         fit_shape_dialogue       (FitShapeArgs *args,
                                              GwyContainer *data,
                                              gint id,
                                              GwyDataField *dfield,
                                              GwyDataField *mfield);
static void         create_output            (FitShapeControls *controls,
                                              GwyContainer *data);
static GtkWidget*   basic_tab_new            (FitShapeControls *controls,
                                              GwyDataField *dfield,
                                              GwyDataField *mfield);
static GtkWidget*   parameters_tab_new       (FitShapeControls *controls);
static void         fit_param_table_resize   (FitShapeControls *controls);
static GtkWidget*   function_menu_new        (const gchar *name,
                                              GwyDataField *dfield,
                                              FitShapeControls *controls);
static void         function_changed         (GtkComboBox *combo,
                                              FitShapeControls *controls);
static void         display_changed          (GtkComboBox *combo,
                                              FitShapeControls *controls);
static void         output_changed           (GtkComboBox *combo,
                                              FitShapeControls *controls);
static void         masking_changed          (GtkToggleButton *button,
                                              FitShapeControls *controls);
static void         fix_changed              (GtkToggleButton *button,
                                              FitShapeControls *controls);
static void         param_value_activate     (GtkEntry *entry,
                                              FitShapeControls *controls);
static void         update_all_param_values  (FitShapeControls *controls);
static void         revert_params            (FitShapeControls *controls);
static void         recalculate_image        (FitShapeControls *controls);
static void         update_param_table       (FitShapeControls *controls,
                                              const gdouble *param,
                                              const gdouble *param_err);
static void         fit_shape_estimate       (FitShapeControls *controls);
static void         fit_shape_reduced_fit    (FitShapeControls *controls);
static void         fit_shape_full_fit       (FitShapeControls *controls);
static void         update_fields            (FitShapeControls *controls);
static void         update_fit_results       (FitShapeControls *controls,
                                              GwyNLFitter *fitter);
static void         update_context_data      (FitShapeControls *controls);
static void         fit_context_resize_params(FitShapeContext *ctx,
                                              guint n_param);
static void         fit_context_fill_data    (FitShapeContext *ctx,
                                              GwyDataField *dfield,
                                              GwyDataField *mask,
                                              GwyDataField *weight,
                                              GwyMaskingType masking);
static void         fit_context_free         (FitShapeContext *ctx);
static GwyNLFitter* fit                      (const FitShapeFunc *func,
                                              const FitShapeContext *ctx,
                                              gdouble *param,
                                              GwySetFractionFunc set_fraction,
                                              GwySetMessageFunc set_message);
static GwyNLFitter* fit_reduced              (const FitShapeFunc *func,
                                              const FitShapeContext *ctx,
                                              gdouble *param);
static void         calculate_field          (const FitShapeFunc *func,
                                              const gdouble *param,
                                              GwyDataField *dfield);
static void         calculate_function       (const FitShapeFunc *func,
                                              const FitShapeContext *ctx,
                                              const gdouble *param,
                                              gdouble *z);
static void         reduce_data_size         (const GwyXY *xy,
                                              const gdouble *z,
                                              guint n,
                                              GwyXY *xyred,
                                              gdouble *zred,
                                              guint nred);
static void         fit_shape_load_args      (GwyContainer *container,
                                              FitShapeArgs *args);
static void         fit_shape_save_args      (GwyContainer *container,
                                              FitShapeArgs *args);

#define DECLARE_SHAPE_FUNC(name) \
    static gdouble name##_func(gdouble abscissa, \
                               gint n_param, \
                               const gdouble *param, \
                               gpointer user_data, \
                               gboolean *fres); \
    static gboolean name##_estimate(const GwyXY *xy, \
                                    const gdouble *z, \
                                    guint n, \
                                    gdouble *param); \
    static gboolean name##_init(const GwyXY *xy, \
                                const gdouble *z, \
                                guint n, \
                                gdouble *param);

#define SHAPE_FUNC_ITEM(name) \
    &name##_func, &name##_estimate, &name##_init, \
    G_N_ELEMENTS(name##_params), name##_params

DECLARE_SHAPE_FUNC(sphere);
DECLARE_SHAPE_FUNC(grating);

static const FitShapeParam sphere_params[] = {
   { "x<sub>0</sub>", 1, 0, },
   { "y<sub>0</sub>", 1, 0, },
   { "z<sub>0</sub>", 0, 1, },
   { "C",             0, 1, },
};

static const FitShapeParam grating_params[] = {
   { "w",             1, 0, },
   { "p",             0, 0, },
   { "h",             0, 1, },
   { "z<sub>0</sub>", 0, 1, },
   { "x<sub>0</sub>", 1, 0, },
   { "α",             0, 0, },
   { "c",             0, 0, },
};

static const FitShapeFunc functions[] = {
    { N_("Sphere"),  TRUE,  SHAPE_FUNC_ITEM(sphere),  },
    { N_("Grating"), FALSE, SHAPE_FUNC_ITEM(grating), },
};

/* NB: The default must not require same units because then we could not fall
 * back to it. */
static const FitShapeArgs fit_shape_defaults = {
    "Grating", GWY_MASK_IGNORE,
    FIT_SHAPE_DISPLAY_RESULT, FIT_SHAPE_OUTPUT_FIT,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Fits predefined geometrical shapes to data."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
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

    fit_shape_dialogue(&args, data, id, dfield, mfield);

    fit_shape_save_args(gwy_app_settings_get(), &args);

}

static void
fit_shape_dialogue(FitShapeArgs *args,
                   GwyContainer *data, gint id,
                   GwyDataField *dfield, GwyDataField *mfield)
{
    GtkWidget *dialogue, *notebook, *widget, *vbox, *hbox, *alignment,
              *hbox2, *label;
    FitShapeControls controls;
    FitShapeContext ctx;
    gint response;

    gwy_clear(&ctx, 1);
    gwy_clear(&controls, 1);
    controls.args = args;
    controls.ctx = &ctx;
    controls.id = id;

    dialogue = gtk_dialog_new_with_buttons(_("Fit Shape"), NULL, 0,
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
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);
    controls.dialogue = dialogue;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialogue)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    if (mfield)
        gwy_container_set_object_by_name(controls.mydata, "/0/mask", mfield);
    dfield = gwy_data_field_duplicate(dfield);
    gwy_container_set_object_by_name(controls.mydata, "/1/data", dfield);
    g_object_unref(dfield);
    dfield = gwy_data_field_duplicate(dfield);
    gwy_container_set_object_by_name(controls.mydata, "/2/data", dfield);
    g_object_unref(dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
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

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    label = gtk_label_new(_("Mean square difference:"));
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 2);

    controls.rss_label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.rss_label, FALSE, FALSE, 2);

    update_context_data(&controls);
    function_changed(GTK_COMBO_BOX(controls.function), &controls);
    display_changed(GTK_COMBO_BOX(controls.display), &controls);

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

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    create_output(&controls, data);
    gtk_widget_destroy(dialogue);

finalise:
    g_object_unref(controls.mydata);
    g_free(controls.param);
    g_free(controls.alt_param);
    g_free(controls.param_err);
    g_array_free(controls.param_controls, TRUE);
    fit_context_free(controls.ctx);
}

/* NB: We reuse fields from mydata.  It is possible only because they are
 * newly created and we are going to destroy mydata anyway. */
static void
create_output(FitShapeControls *controls, GwyContainer *data)
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
        { N_("None"),         FIT_SHAPE_OUTPUT_NONE, },
        { N_("Fitted shape"), FIT_SHAPE_OUTPUT_FIT,  },
        { N_("Difference"),   FIT_SHAPE_OUTPUT_DIFF, },
        { N_("Both"),         FIT_SHAPE_OUTPUT_BOTH, },
    };

    GtkWidget *table, *label;
    FitShapeArgs *args = controls->args;
    gint row;

    table = gtk_table_new(7, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    row = 0;

    controls->function = function_menu_new(args->function, dfield, controls);
    gwy_table_attach_hscale(table, row, _("_Function type:"), NULL,
                            GTK_OBJECT(controls->function), GWY_HSCALE_WIDGET);
    row++;

    controls->display
        = gwy_enum_combo_box_new(displays, G_N_ELEMENTS(displays),
                                 G_CALLBACK(display_changed), controls,
                                 args->display, TRUE);
    gwy_table_attach_hscale(table, row, _("_Preview:"), NULL,
                            GTK_OBJECT(controls->display), GWY_HSCALE_WIDGET);
    row++;

    controls->output
        = gwy_enum_combo_box_new(outputs, G_N_ELEMENTS(outputs),
                                 G_CALLBACK(output_changed), controls,
                                 args->output, TRUE);
    gwy_table_attach_hscale(table, row, _("Output _type:"), NULL,
                            GTK_OBJECT(controls->output), GWY_HSCALE_WIDGET);
    row++;

    if (mfield) {
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
    }

    return table;
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
    gtk_table_set_row_spacing(table, 0, 2);
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
    gtk_box_pack_start(GTK_BOX(hbox), controls->recalculate, FALSE, FALSE, 8);
    g_signal_connect_swapped(controls->recalculate, "clicked",
                             G_CALLBACK(recalculate_image), controls);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    controls->revert = gtk_button_new_with_mnemonic(_("Revert to "
                                                      "_Previous Values"));
    gtk_size_group_add_widget(sizegroup, controls->revert);
    gtk_box_pack_start(GTK_BOX(hbox), controls->revert, FALSE, FALSE, 8);
    g_signal_connect_swapped(controls->revert, "clicked",
                             G_CALLBACK(revert_params), controls);

    g_object_unref(sizegroup);

    return vbox;
}

static void
fit_param_table_resize(FitShapeControls *controls)
{
    GtkTable *table;
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, row, old_nparams, nparams;

    old_nparams = controls->param_controls->len;
    nparams = func->nparams;
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
        gtk_entry_set_width_chars(GTK_ENTRY(cntrl.value), 10);
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
        const FitShapeParam *param = func->param + i;

        gtk_label_set_markup(GTK_LABEL(cntrl->name), param->name);
    }

    gtk_widget_show_all(controls->param_table);
}

static GtkWidget*
function_menu_new(const gchar *name, GwyDataField *dfield,
                  FitShapeControls *controls)
{
    GwySIUnit *xyunit, *zunit;
    gboolean same_units;
    GArray *entries = g_array_new(FALSE, FALSE, sizeof(GwyEnum));
    GtkWidget *combo;
    GwyEnum *model;
    guint i, n, active = G_MAXUINT;

    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    zunit = gwy_data_field_get_si_unit_z(dfield);
    same_units = gwy_si_unit_equal(xyunit, zunit);

    for (i = 0; i < G_N_ELEMENTS(functions); i++) {
        GwyEnum entry;

        if (functions[i].needs_same_units && !same_units)
            continue;

        entry.name = functions[i].name;
        entry.value = i;
        controls->function_id = i;
        if (gwy_strequal(entry.name, name))
            active = entries->len;
        g_array_append_val(entries, entry);
    }

    if (active == G_MAXUINT) {
        name = fit_shape_defaults.function;
        for (i = 0; i < G_N_ELEMENTS(functions); i++) {
            GwyEnum entry;

            if (functions[i].needs_same_units && !same_units)
                continue;

            entry.name = functions[i].name;
            entry.value = i;
            controls->function_id = i;
            if (gwy_strequal(entry.name, name))
                active = entries->len;
            g_array_append_val(entries, entry);
        }

        g_assert(active != G_MAXUINT);
    }

    n = entries->len;
    model = (GwyEnum*)g_array_free(entries, FALSE);
    combo = gwy_enum_combo_box_new(model, n,
                                   G_CALLBACK(function_changed), controls,
                                   active, TRUE);
    g_object_set_data(G_OBJECT(combo), "model", model);
    g_object_weak_ref(G_OBJECT(combo), (GWeakNotify)g_free, model);

    return combo;
}

static void
function_changed(GtkComboBox *combo, FitShapeControls *controls)
{
    guint i = gwy_enum_combo_box_get_active(combo);
    const GwyEnum *model = (const GwyEnum*)g_object_get_data(G_OBJECT(combo),
                                                             "model");
    FitShapeContext *ctx = controls->ctx;
    const FitShapeFunc *func;
    guint nparams;

    controls->function_id = model[i].value;
    controls->args->function = model[i].name;
    func = functions + controls->function_id;
    nparams = func->nparams;

    controls->param = g_renew(gdouble, controls->param, nparams);
    controls->alt_param = g_renew(gdouble, controls->alt_param, nparams);
    controls->param_err = g_renew(gdouble, controls->param_err, nparams);
    for (i = 0; i < nparams; i++)
        controls->param_err[i] = -1.0;
    fit_param_table_resize(controls);
    fit_context_resize_params(ctx, nparams);
    func->init_param(ctx->xy, ctx->z, ctx->n, controls->param);
    memcpy(controls->alt_param, controls->param, nparams*sizeof(gdouble));
    update_param_table(controls, controls->param, NULL);
    update_fields(controls);
}

static void
display_changed(GtkComboBox *combo, FitShapeControls *controls)
{
    GwyPixmapLayer *player;
    GQuark quark;

    controls->args->display = gwy_enum_combo_box_get_active(combo);
    player = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));
    quark = gwy_app_get_data_key_for_id(controls->args->display);
    gwy_pixmap_layer_set_data_key(player, g_quark_to_string(quark));
}

static void
output_changed(GtkComboBox *combo, FitShapeControls *controls)
{
    controls->args->output = gwy_enum_combo_box_get_active(combo);
}

static void
masking_changed(GtkToggleButton *button, FitShapeControls *controls)
{
    FitShapeArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args->masking = gwy_radio_buttons_get_current(controls->masking);
    update_context_data(controls);
    // TODO: Do anything else here?
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
param_value_activate(GtkEntry *entry, FitShapeControls *controls)
{
    guint i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(entry), "id"));
    FitParamControl *cntrl = &g_array_index(controls->param_controls,
                                            FitParamControl, i);

    controls->param[i] = g_strtod(gtk_entry_get_text(entry), NULL);
    controls->param[i] *= cntrl->magnitude;
    /* This (a) clears error labels in the table (b) reformats the parameter,
     * e.g. by moving the power-of-10 base appropriately. */
    update_param_table(controls, controls->param, NULL);
}

static void
update_all_param_values(FitShapeControls *controls)
{
    guint i;

    for (i = 0; i < controls->param_controls->len; i++) {
        FitParamControl *cntrl = &g_array_index(controls->param_controls,
                                                FitParamControl, i);
        GtkEntry *entry = GTK_ENTRY(cntrl->value);

        controls->param[i] = g_strtod(gtk_entry_get_text(entry), NULL);
        controls->param[i] *= cntrl->magnitude;
    }
}

static void
revert_params(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, nparams = func->nparams;

    update_all_param_values(controls);
    for (i = 0; i < nparams; i++)
        GWY_SWAP(gdouble, controls->param[i], controls->alt_param[i]);

    update_param_table(controls, controls->param, NULL);
}

static void
recalculate_image(FitShapeControls *controls)
{
    update_all_param_values(controls);
    update_fields(controls);
}

static void
update_param_table(FitShapeControls *controls,
                   const gdouble *param, const gdouble *param_err)
{
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, nparams = func->nparams;
    GwyDataField *dfield;
    GwySIUnit *unit, *xyunit, *zunit;
    GwySIValueFormat *vf = NULL;

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    zunit = gwy_data_field_get_si_unit_z(dfield);
    unit = gwy_si_unit_new(NULL);

    for (i = 0; i < nparams; i++) {
        FitParamControl *cntrl = &g_array_index(controls->param_controls,
                                                FitParamControl, i);
        const FitShapeParam *fitparam = func->param + i;
        guchar buf[32];
        gdouble v;

        v = param[i];
        gwy_si_unit_power_multiply(xyunit, fitparam->power_xy,
                                   zunit, fitparam->power_z,
                                   unit);
        vf = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
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
        vf = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, v/vf->magnitude);
        gtk_label_set_text(GTK_LABEL(cntrl->error), buf);
        gtk_label_set_markup(GTK_LABEL(cntrl->error_unit), vf->units);
    }

    gwy_si_unit_value_format_free(vf);
    g_object_unref(unit);
}

static void
fit_shape_estimate(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    const FitShapeContext *ctx = controls->ctx;
    guint i, nparams = func->nparams;

    gwy_app_wait_cursor_start(GTK_WINDOW(controls->dialogue));
    gwy_debug("start estimate");
    memcpy(controls->alt_param, controls->param, nparams*sizeof(gdouble));
    func->estimate(ctx->xy, ctx->z, ctx->n, controls->param);
    /* XXX: We honour fixed parameters by reverting to previous values and
     * pretending nothing happened.  Is it OK? */
    for (i = 0; i < nparams; i++) {
        gwy_debug("[%u] %g", i, controls->param[i]);
        if (ctx->param_fixed[i])
            controls->param[i] = controls->alt_param[i];
    }
    update_fields(controls);
    update_fit_results(controls, NULL);
    gwy_app_wait_cursor_finish(GTK_WINDOW(controls->dialogue));
}

static void
fit_shape_reduced_fit(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    const FitShapeContext *ctx = controls->ctx;
    GwyNLFitter *fitter;

    gwy_app_wait_cursor_start(GTK_WINDOW(controls->dialogue));
    gwy_debug("start reduced fit");
    update_all_param_values(controls);
    memcpy(controls->alt_param, controls->param, func->nparams*sizeof(gdouble));
    fitter = fit_reduced(func, ctx, controls->param);
#ifdef DEBUG
    {
        guint i;
        for (i = 0; i < func->nparams; i++)
            gwy_debug("[%u] %g", i, controls->param[i]);
    }
#endif
    update_fields(controls);
    update_fit_results(controls, fitter);
    gwy_math_nlfit_free(fitter);
    gwy_app_wait_cursor_finish(GTK_WINDOW(controls->dialogue));
}

static void
fit_shape_full_fit(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    const FitShapeContext *ctx = controls->ctx;
    GwyNLFitter *fitter;

    gwy_app_wait_start(GTK_WINDOW(controls->dialogue), _("Fitting..."));
    gwy_debug("start fit");
    update_all_param_values(controls);
    memcpy(controls->alt_param, controls->param, func->nparams*sizeof(gdouble));
    fitter = fit(func, ctx, controls->param,
                 gwy_app_wait_set_fraction, gwy_app_wait_set_message);
#ifdef DEBUG
    {
        guint i;
        for (i = 0; i < func->nparams; i++)
            gwy_debug("[%u] %g", i, controls->param[i]);
    }
#endif
    update_fields(controls);
    update_fit_results(controls, fitter);
    gwy_math_nlfit_free(fitter);
    gwy_app_wait_finish();
}

static void
update_fields(FitShapeControls *controls)
{
    GwyDataField *dfield, *resfield, *difffield;

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    resfield = gwy_container_get_object_by_name(controls->mydata, "/1/data");
    difffield = gwy_container_get_object_by_name(controls->mydata, "/2/data");
    calculate_field(functions + controls->function_id,
                    controls->param, resfield);
    gwy_data_field_data_changed(resfield);
    gwy_data_field_subtract_fields(difffield, dfield, resfield);
    gwy_data_field_data_changed(difffield);
}

static void
update_fit_results(FitShapeControls *controls, GwyNLFitter *fitter)
{
    const FitShapeFunc *func = functions + controls->function_id;
    const FitShapeContext *ctx = controls->ctx;
    GwyDataField *dfield;
    gdouble rss = 0.0;
    guint k, n = ctx->n, i, nparams = func->nparams;
    GwySIUnit *zunit;
    GwySIValueFormat *vf;
    guchar buf[48];

    for (k = 0; k < n; k++) {
        gboolean fres;
        gdouble z;

        z = func->function((gdouble)k, nparams, controls->param,
                           (gpointer)ctx, &fres);
        if (!fres) {
            g_warning("Cannot evaluate function for pixel.");
        }
        else {
            z -= ctx->z[k];
            rss += z*z;
        }
    }
    controls->rss = sqrt(rss/n);

    if (fitter) {
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

    update_param_table(controls, controls->param,
                       fitter ? controls->param_err : NULL);
}

static void
update_context_data(FitShapeControls *controls)
{
    GwyDataField *dfield, *mfield = NULL;

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    gwy_container_gis_object_by_name(controls->mydata, "/0/mask",
                                     (GObject**)&mfield);
    fit_context_fill_data(controls->ctx, dfield, mfield, NULL,
                          controls->args->masking);
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

/* Construct separate xy[], z[] and w[] arrays from data field pixels under
 * the mask. */
static void
fit_context_fill_data(FitShapeContext *ctx,
                      GwyDataField *dfield,
                      GwyDataField *mask,
                      GwyDataField *weight,
                      GwyMaskingType masking)
{
    guint n, k, i, j, nn, xres, yres;
    const gdouble *d, *m, *w;
    gdouble dx, dy, xoff, yoff;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    dx = gwy_data_field_get_xmeasure(dfield);
    dy = gwy_data_field_get_ymeasure(dfield);
    xoff = gwy_data_field_get_xoffset(dfield);
    yoff = gwy_data_field_get_yoffset(dfield);

    if (masking == GWY_MASK_IGNORE)
        mask = NULL;
    else if (!mask)
        masking = GWY_MASK_IGNORE;

    nn = xres*yres;
    if (mask) {
        m = gwy_data_field_get_data_const(mask);
        n = 0;
        if (masking == GWY_MASK_INCLUDE) {
            for (k = 0; k < nn; k++) {
                if (m[k] > 0.0)
                    n++;
            }
        }
        else {
            for (k = 0; k < nn; k++) {
                if (m[k] <= 0.0)
                    n++;
            }
        }
    }
    else {
        m = NULL;
        n = nn;
    }

    ctx->n = n;
    ctx->abscissa = g_renew(gdouble, ctx->abscissa, n);
    ctx->xy = g_renew(GwyXY, ctx->xy, n);
    ctx->z = g_renew(gdouble, ctx->z, n);
    ctx->w = g_renew(gdouble, ctx->w, n);
    d = gwy_data_field_get_data_const(dfield);
    w = weight ? gwy_data_field_get_data_const(weight) : NULL;

    n = k = 0;
    for (i = 0; i < yres; i++) {
        gdouble y = (i + 0.5)*dy + yoff;

        for (j = 0; j < xres; j++, k++) {
            if (!m
                || (masking == GWY_MASK_INCLUDE && m[k] > 0.0)
                || (masking == GWY_MASK_EXCLUDE && m[k] <= 0.0)) {
                gdouble x = (j + 0.5)*dx + xoff;

                ctx->abscissa[n] = n;
                ctx->xy[n].x = x;
                ctx->xy[n].y = y;
                ctx->z[n] = d[k];
                ctx->w[n] = w ? w[k] : 1.0;
                n++;
            }
        }
    }
}

static void
fit_context_free(FitShapeContext *ctx)
{
    g_free(ctx->param_fixed);
    g_free(ctx->abscissa);
    g_free(ctx->xy);
    g_free(ctx->z);
    g_free(ctx->w);
    gwy_clear(ctx, 1);
}

static GwyNLFitter*
fit(const FitShapeFunc *func, const FitShapeContext *ctx, gdouble *param,
    GwySetFractionFunc set_fraction, GwySetMessageFunc set_message)
{
    GwyNLFitter *fitter;
    gdouble rss;

    fitter = gwy_math_nlfit_new(func->function, gwy_math_nlfit_derive);
    if (set_fraction || set_message)
        gwy_math_nlfit_set_callbacks(fitter, set_fraction, set_message);

    rss = gwy_math_nlfit_fit_full(fitter,
                                  ctx->n, ctx->abscissa, ctx->z, ctx->w,
                                  func->nparams, param, ctx->param_fixed, NULL,
                                  (gpointer)ctx);
    if (rss < 0.0)
        g_warning("Fit failed.");

    return fitter;
}

static GwyNLFitter*
fit_reduced(const FitShapeFunc *func, const FitShapeContext *ctx,
            gdouble *param)
{
    GwyNLFitter *fitter;
    FitShapeContext ctxred;
    guint nred;

    if (ctx->n <= NREDLIM)
        return NULL;

    ctxred = *ctx;
    nred = ctxred.n = sqrt(ctx->n*(gdouble)NREDLIM);
    ctxred.xy = g_new(GwyXY, nred);
    ctxred.z = g_new(gdouble, nred);
    /* TODO TODO TODO we must also reduce weights. */
    reduce_data_size(ctx->xy, ctx->z, ctx->n, ctxred.xy, ctxred.z, nred);
    fitter = fit(func, &ctxred, param, NULL, NULL);
    g_free(ctxred.xy);
    g_free(ctxred.z);

    return fitter;
}

static void
calculate_field(const FitShapeFunc *func,
                const gdouble *param,
                GwyDataField *dfield)
{
    FitShapeContext ctx;

    gwy_clear(&ctx, 1);
    fit_context_resize_params(&ctx, func->nparams);
    fit_context_fill_data(&ctx, dfield, NULL, NULL, GWY_MASK_IGNORE);
    calculate_function(func, &ctx, param, gwy_data_field_get_data(dfield));
    fit_context_free(&ctx);
}

static void
calculate_function(const FitShapeFunc *func,
                   const FitShapeContext *ctx,
                   const gdouble *param,
                   gdouble *z)
{
    guint k, n = ctx->n, nparams = func->nparams;

    for (k = 0; k < n; k++) {
        gboolean fres;

        z[k] = func->function((gdouble)k, nparams, param, (gpointer)ctx, &fres);
        if (!fres) {
            g_warning("Cannot evaluate function for pixel.");
            z[k] = 0.0;
        }
    }
}

/* FIXME: Weights? */
static void
reduce_data_size(const GwyXY *xy, const gdouble *z, guint n,
                 GwyXY *xyred, gdouble *zred, guint nred)
{
    GRand *rng = g_rand_new();
    guint *redindex = g_new(guint, n);
    guint i, j;

    for (i = 0; i < n; i++)
        redindex[i] = i;

    for (i = 0; i < nred; i++) {
        j = g_rand_int_range(rng, 0, n-nred);
        xyred[i] = xy[redindex[j]];
        zred[i] = z[redindex[j]];
        redindex[j] = redindex[n-1-nred];
    }

    g_rand_free(rng);
}

/**************************************************************************
 *
 * General estimator helpers and math support functions.
 *
 **************************************************************************/

#ifdef HAVE_SINCOS
#define _gwy_sincos sincos
#else
static inline void
_gwy_sincos(gdouble x, gdouble *s, gdouble *c)
{
    *s = sin(x);
    *c = cos(x);
}
#endif

/* cosh(x) - 1 safe for small arguments */
static inline gdouble
gwy_coshm1(gdouble x)
{
    gdouble x2 = x*x;
    if (x2 > 3e-5)
        return cosh(x) - 1.0;
    return x2*(0.5 + x2/24.0);
}

/* Mean value of xy point cloud (not necessarily centre, that depends on
 * the density). */
static void
mean_x_y(const GwyXY *xy, guint n, gdouble *pxc, gdouble *pyc)
{
    gdouble xc = 0.0, yc = 0.0;
    guint i;

    if (!n) {
        *pxc = *pyc = 0.0;
        return;
    }

    for (i = 0; i < n; i++) {
        xc += xy[i].x;
        yc += xy[i].y;
    }

    *pxc = xc/n;
    *pyc = yc/n;
}

/* Minimum and maximum of an array of values. */
static void
range_z(const gdouble *z, guint n, gdouble *pmin, gdouble *pmax)
{
    gdouble min = G_MAXDOUBLE, max = -G_MAXDOUBLE;
    guint i;

    if (!n) {
        *pmin = *pmax = 0.0;
        return;
    }

    for (i = 0; i < n; i++) {
        if (z[i] < min)
            min = z[i];
        if (z[i] > max)
            max = z[i];
    }

    *pmin = min;
    *pmax = max;
}

/* Approximately cicrumscribe a set of points by finding a containing
 * octagon. */
static void
circumscribe_x_y(const GwyXY *xy, guint n,
                 gdouble *pxc, gdouble *pyc, gdouble *pr)
{
    gdouble min[4], max[4], r[4];
    guint i, j;

    if (!n) {
        *pxc = *pyc = 0.0;
        *pr = 1.0;
        return;
    }

    for (j = 0; j < 4; j++) {
        min[j] = G_MAXDOUBLE;
        max[j] = -G_MAXDOUBLE;
    }

    for (i = 0; i < n; i++) {
        gdouble x = xy[i].x, y = xy[i].y;
        gdouble t[4] = { x, x+y, y, y-x };

        for (j = 0; j < 4; j++) {
            if (t[j] < min[j])
                min[j] = t[j];
            if (t[j] > max[j])
                max[j] = t[j];
        }
    }

    for (j = 0; j < 4; j++) {
        r[j] = sqrt(10.0)/3.0*(max[j] - min[j]);
        if (j % 2)
            r[j] /= G_SQRT2;
    }

    i = 0;
    for (j = 1; j < 4; j++) {
        if (r[j] > r[i])
            i = j;
    }

    *pr = 0.5*r[i];
    if (i % 2) {
        *pxc = (min[1] - min[3] + max[1] - max[3])/4.0;
        *pyc = (min[1] + min[3] + max[1] + max[3])/4.0;
    }
    else {
        *pxc = (min[0] + max[0])/2.0;
        *pyc = (min[2] + max[2])/2.0;
    }
}

/* Project xyz point cloud to a line rotated by angle alpha anti-clockwise
 * from the horizontal line (x axis). */
static gdouble
projection_to_line(const GwyXY *xy,
                   const gdouble *z,
                   guint n,
                   gdouble alpha,
                   gdouble xc, gdouble yc,
                   GwyDataLine *mean_line,
                   GwyDataLine *rms_line,
                   guint *counts)
{
    guint res = gwy_data_line_get_res(mean_line);
    gdouble *mean = gwy_data_line_get_data(mean_line);
    gdouble *rms = rms_line ? gwy_data_line_get_data(rms_line) : NULL;
    gdouble dx = gwy_data_line_get_real(mean_line)/res;
    gdouble off = gwy_data_line_get_offset(mean_line);
    gdouble c = cos(alpha), s = sin(alpha), total_ms = 0.0;
    guint i, total_n = 0;
    gint j;

    gwy_data_line_clear(mean_line);
    gwy_clear(counts, res);

    for (i = 0; i < n; i++) {
        gdouble x = xy[i].x - xc, y = xy[i].y - yc;
        x = x*c - y*s;
        j = (gint)floor((x - off)/dx);
        if (j >= 0 && j < res) {
            mean[j] += z[i];
            counts[j]++;
        }
    }

    for (j = 0; j < res; j++) {
        if (counts[j]) {
            mean[j] /= counts[j];
        }
    }

    if (!rms_line)
        return 0.0;

    gwy_data_line_clear(rms_line);

    for (i = 0; i < n; i++) {
        gdouble x = xy[i].x - xc, y = xy[i].y - yc;
        x = x*c - y*s;
        j = (gint)floor((x - off)/dx);
        if (j >= 0 && j < res)
            rms[j] += (z[i] - mean[j])*(z[i] - mean[j]);
    }

    for (j = 0; j < res; j++) {
        if (counts[j]) {
            total_ms += rms[j];
            rms[j] = sqrt(rms[j]/counts[j]);
            total_n += counts[j];
        }
    }

    return sqrt(total_ms/total_n);
}

/* Find direction along which projections capture best the shape, i.e. most
 * variance remains in the line-averaged data.  The returned angle is rotation
 * of the axis anti-clockwise with respect to the x-axis. */
static gdouble
estimate_projection_direction(const GwyXY *xy,
                              const gdouble *z,
                              guint n)
{
    enum { NROUGH = 48, NFINE = 8 };

    GwyDataLine *mean_line, *rms_line;
    guint *counts;
    gdouble xc, yc, r, alpha, alpha0, alpha_step, rms;
    gdouble best_rms = G_MAXDOUBLE, best_alpha = 0.0;
    guint iter, i, ni, res;

    circumscribe_x_y(xy, n, &xc, &yc, &r);
    res = (guint)floor(2.0*sqrt(n) + 1.0);

    mean_line = gwy_data_line_new(res, 2.0*r, FALSE);
    gwy_data_line_set_offset(mean_line, -r);
    rms_line = gwy_data_line_new_alike(mean_line, FALSE);
    counts = g_new(guint, res);

    for (iter = 0; iter < 6; iter++) {
        if (iter == 0) {
            ni = NROUGH;
            alpha_step = G_PI/ni;
            alpha0 = 0.5*alpha_step;
        }
        else {
            /* Choose the fine points so that we do not repeat calculation in
             * any of the rough points. */
            ni = NFINE;
            alpha0 = best_alpha - alpha_step*(NFINE - 1.0)/(NFINE + 1.0);
            alpha_step = 2.0*alpha_step/(NFINE + 1.0);
        }

        for (i = 0; i < ni; i++) {
            alpha = alpha0 + i*alpha_step;
            rms = projection_to_line(xy, z, n, alpha, xc, yc,
                                     mean_line, rms_line, counts);
            gwy_debug("[%u] %g %g", iter, alpha, rms);
            if (rms < best_rms) {
                best_rms = rms;
                best_alpha = alpha;
            }
        }
    }

    g_object_unref(mean_line);
    g_object_unref(rms_line);
    g_free(counts);

    return best_alpha;
}

/* Estimate the period of a periodic structure, knowing already the rotation.
 * The returned phase is such that if you subtract it from the rotated abscissa
 * value then the projection will have a positive peak (some kind of maximum)
 * centered around zero, whatever that means for specific grating-like
 * structures.  */
static gboolean
estimate_period_and_phase(const GwyXY *xy, const gdouble *z, guint n,
                          gdouble alpha, gdouble *pT, gdouble *poff)
{
    GwyDataLine *mean_line, *tmp_line;
    gdouble xc, yc, r, T, t, real, off, a_s, a_c, phi0;
    const gdouble *mean, *tmp;
    guint *counts;
    guint res, i, ibest;
    gboolean found;

    circumscribe_x_y(xy, n, &xc, &yc, &r);
    res = (guint)floor(3.0*sqrt(n) + 1.0);

    *pT = r/4.0;
    *poff = 0.0;

    mean_line = gwy_data_line_new(res, 2.0*r, FALSE);
    gwy_data_line_set_offset(mean_line, -r);
    tmp_line = gwy_data_line_new_alike(mean_line, FALSE);
    counts = g_new(guint, res);

    projection_to_line(xy, z, n, alpha, xc, yc, mean_line, NULL, counts);
    gwy_data_line_add(mean_line, -gwy_data_line_get_avg(mean_line));
    gwy_data_line_psdf(mean_line, tmp_line,
                       GWY_WINDOWING_HANN, GWY_INTERPOLATION_LINEAR);
    tmp = gwy_data_line_get_data_const(tmp_line);

    found = FALSE;
    ibest = G_MAXUINT;
    for (i = 4; i < MIN(res/3, res-3); i++) {
        if (tmp[i] > tmp[i-2] && tmp[i] > tmp[i-1]
            && tmp[i] > tmp[i+1] && tmp[i] > tmp[i+2]) {
            if (ibest == G_MAXUINT || tmp[i] > tmp[ibest]) {
                found = TRUE;
                ibest = i;
            }
        }
    }
    if (!found)
        goto fail;

    T = *pT = 2.0*G_PI/gwy_data_line_itor(tmp_line, ibest);
    gwy_debug("found period %g", T);

    mean = gwy_data_line_get_data_const(mean_line);
    real = gwy_data_line_get_real(mean_line);
    off = gwy_data_line_get_offset(mean_line);
    a_s = a_c = 0.0;
    for (i = 0; i < res; i++) {
        t = off + real/res*(i + 0.5);
        a_s += sin(2*G_PI*t/T)*mean[i];
        a_c += cos(2*G_PI*t/T)*mean[i];
    }
    gwy_debug("a_s %g, a_c %g", a_s, a_c);

    phi0 = atan2(a_s, a_c);
    *poff = phi0*T/(2.0*G_PI) + xc*cos(alpha) - yc*sin(alpha);

fail:
    g_object_unref(mean_line);
    g_object_unref(tmp_line);
    g_free(counts);

    return found;
}

/**************************************************************************
 *
 * Sphere
 *
 **************************************************************************/

static gdouble
sphere_func(gdouble abscissa,
            G_GNUC_UNUSED gint n_param,
            const gdouble *param,
            gpointer user_data,
            gboolean *fres)
{
    const FitShapeContext *ctx = (const FitShapeContext*)user_data;
    gdouble xc = param[0];
    gdouble yc = param[1];
    gdouble z0 = param[2];
    gdouble kappa = param[3];
    gdouble x, y, r2k, t, val;
    guint i;

    g_assert(n_param == 4);

    i = (guint)abscissa;
    x = ctx->xy[i].x - xc;
    y = ctx->xy[i].y - yc;
    /* Rewrite R - sqrt(R² - r²) as κ*r²/(1 + sqrt(1 - κ²r²)) where
     * r² = x² + y² and κR = 1 to get nice behaviour in the close-to-denegerate
     * cases, including completely flat surface.  The expression 1.0/kappa
     * is safe because we cannot get to this branch for κ → 0 unless
     * simultaneously r → ∞. */
    r2k = kappa*(x*x + y*y);
    t = 1.0 - kappa*r2k;
    if (t > 0.0)
        val = z0 + r2k/(1.0 + sqrt(t));
    else
        val = z0 + 1.0/kappa;

    *fres = TRUE;
    return val;
}

static gboolean
sphere_init(const GwyXY *xy,
            const gdouble *z,
            guint n,
            gdouble *param)
{
    gdouble xc, yc, r, zmin, zmax;

    circumscribe_x_y(xy, n, &xc, &yc, &r);
    range_z(z, n, &zmin, &zmax);

    param[0] = xc;
    param[1] = yc;
    param[2] = zmin;
    param[3] = 2.0*(zmax - zmin)/(r*r);

    return TRUE;
}

/* Fit the data with a rotationally symmetric parabola and use its parameters
 * for the spherical surface estimate. */
static gboolean
sphere_estimate(const GwyXY *xy,
                const gdouble *z,
                guint n,
                gdouble *param)
{
    gdouble xc, yc;
    /* Linear fit with functions 1, x, y and x²+y². */
    gdouble a[10], b[4];
    guint i;

    /* XXX: Handle the surrounding flat area, which can be a part of the
     * function, better? */

    /* Using centered coodinates improves the condition number. */
    mean_x_y(xy, n, &xc, &yc);
    gwy_clear(a, 10);
    gwy_clear(b, 4);
    for (i = 0; i < n; i++) {
        gdouble x = xy[i].x - xc, y = xy[i].y - yc;
        gdouble r2 = x*x + y*y;

        b[0] += z[i];
        b[1] += x*z[i];
        b[2] += y*z[i];
        b[3] += r2*z[i];

        a[2] += x*x;
        a[4] += x*y;
        a[5] += y*y;
        a[6] += r2;
        a[7] += x*r2;
        a[8] += y*r2;
        a[9] += r2*r2;
    }
    a[0] = n;
    a[1] = a[3] = 0.0;

    param[0] = xc;
    param[1] = yc;
    param[2] = b[0]/n;
    param[3] = 0.0;

    if (!gwy_math_choleski_decompose(4, a))
        return FALSE;

    gwy_math_choleski_solve(4, a, b);

    param[3] = 2.0*b[3];
    if (param[3]) {
        param[0] = xc - b[1]/param[3];
        param[1] = yc - b[2]/param[3];
        param[2] = b[0] - 0.5*(b[1]*b[1] + b[2]*b[2])/param[3];
    }

    return TRUE;
}

/**************************************************************************
 *
 * Grating
 *
 **************************************************************************/

static gdouble
grating_func(gdouble abscissa,
             G_GNUC_UNUSED gint n_param,
             const gdouble *param,
             gpointer user_data,
             gboolean *fres)
{
#ifdef FIT_SHAPE_CACHE
    static gdouble c_last = 0.0, coshm1_c_last = 1.0;
    static gdouble alpha_last = 0.0, ca_last = 1.0, sa_last = 0.0;
#endif

    const FitShapeContext *ctx = (const FitShapeContext*)user_data;
    gdouble w = param[0];
    gdouble p = param[1];
    gdouble h = param[2];
    gdouble z0 = param[3];
    gdouble x0 = param[4];
    gdouble alpha = param[5];
    gdouble c = param[6];
    gdouble x, y, t, wp2, val, coshm1_c, ca, sa;
    guint i;

    g_assert(n_param == 7);

    i = (guint)abscissa;
    x = ctx->xy[i].x;
    y = ctx->xy[i].y;

    *fres = TRUE;
    wp2 = 0.5*w*p;
    if (G_UNLIKELY(!wp2))
        return z0;

#ifdef FIT_SHAPE_CACHE
    if (alpha == alpha_last) {
        ca = ca_last;
        sa = sa_last;
    }
    else {
        sincos(alpha, &sa, &ca);
        ca_last = ca;
        sa_last = sa;
        alpha_last = alpha;
    }
#else
    ca = cos(alpha);
    sa = sin(alpha);
#endif
    t = x*ca - y*sa - x0 + wp2;
    t = (t - w*floor(t/w))/wp2 - 1.0;
    if (fabs(t) < 1.0) {
#ifdef FIT_SHAPE_CACHE
        if (c == c_last)
            coshm1_c = coshm1_c_last;
        else {
            coshm1_c = coshm1_c_last = gwy_coshm1(c);
            c_last = c;
        }
#else
        coshm1_c = gwy_coshm1(c);
#endif

        val = z0 + h*(1.0 - gwy_coshm1(c*t)/coshm1_c);
    }
    else
        val = z0;

    return val;
}

static gboolean
grating_init(const GwyXY *xy,
             const gdouble *z,
             guint n,
             gdouble *param)
{
    gdouble xc, yc, r, zmin, zmax;

    circumscribe_x_y(xy, n, &xc, &yc, &r);
    range_z(z, n, &zmin, &zmax);

    param[0] = r/4.0;
    param[1] = 0.5;
    param[2] = zmax - zmin;
    param[3] = zmin;
    param[4] = 0.0;
    param[5] = 0.0;
    param[6] = 5.0;

    return TRUE;
}

static gboolean
grating_estimate(const GwyXY *xy,
                 const gdouble *z,
                 guint n,
                 gdouble *param)
{
    GwyXY *xyred = NULL;
    gdouble *zred = NULL;
    guint nred = 0;
    gdouble t;

    /* Just initialise the percentage and shape with some sane defaults. */
    param[1] = 0.5;
    param[6] = 5.0;

    /* Simple height parameter estimate. */
    range_z(z, n, param+3, param+2);
    t = param[2] - param[3];
    param[2] = 0.9*t;
    param[3] += 0.05*t;

    /* First we estimate the orientation (alpha). */
    if (n > NREDLIM) {
        nred = sqrt(n*(gdouble)NREDLIM);
        xyred = g_new(GwyXY, nred);
        zred = g_new(gdouble, nred);
        reduce_data_size(xy, z, n, xyred, zred, nred);
    }

    if (nred)
        param[5] = estimate_projection_direction(xyred, zred, nred);
    else
        param[5] = estimate_projection_direction(xy, z, n);

    if (nred) {
        g_free(xyred);
        g_free(zred);
    }

    /* Then we extract a representative profile with this orientation. */
    return estimate_period_and_phase(xy, z, n, param[5], param + 0, param + 4);
}

static const gchar display_key[]  = "/module/fit_shape/display";
static const gchar function_key[] = "/module/fit_shape/function";
static const gchar masking_key[]  = "/module/fit_shape/masking";
static const gchar output_key[]   = "/module/fit_shape/output";

static void
fit_shape_sanitize_args(FitShapeArgs *args)
{
    guint i;
    gboolean ok;

    args->masking = gwy_enum_sanitize_value(args->masking,
                                            GWY_TYPE_MASKING_TYPE);
    args->display = MIN(args->display, FIT_SHAPE_DISPLAY_DIFF);
    args->output = MIN(args->output, FIT_SHAPE_OUTPUT_BOTH);

    ok = FALSE;
    for (i = 0; i < G_N_ELEMENTS(functions); i++) {
        if (gwy_strequal(args->function, functions[i].name)) {
            ok = TRUE;
            break;
        }
    }
    if (!ok)
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
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
