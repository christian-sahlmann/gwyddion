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
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/grains.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwygrainvaluemenu.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define GFILTER_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400,
    NQUANTITIES = 3,
};

enum {
    RESPONSE_RESET   = 1,
    RESPONSE_PREVIEW = 2
};

typedef enum {
    GRAIN_LOGICAL1_A,
    GRAIN_LOGICAL1_NTYPES,
} GrainLogical1;

typedef enum {
    GRAIN_LOGICAL2_A_AND_B,
    GRAIN_LOGICAL2_A_OR_B,
    GRAIN_LOGICAL2_NTYPES,
} GrainLogical2;

typedef enum {
    GRAIN_LOGICAL3_A_AND_B_AND_C,
    GRAIN_LOGICAL3_A_OR_B_OR_C,
    GRAIN_LOGICAL3_A_AND_B_OR_C,
    GRAIN_LOGICAL3_A_OR_B_AND_C,
    GRAIN_LOGICAL3_NTYPES,
} GrainLogical3;

typedef struct {
    gchar *quantity;
    gdouble lower;
    gdouble upper;
    gboolean is_expr;    /* just a cache */
} RangeRecord;

typedef struct {
    gboolean update;
    gint expanded;
    gint nquantities;
    GrainLogical1 logical1;
    GrainLogical2 logical2;
    GrainLogical3 logical3;
    RangeRecord ranges[NQUANTITIES];

    GHashTable *ranges_history;

    /* To mask impossible quantitities without really resetting the bits */
    gboolean units_equal;

    GPtrArray *valuedata;
    gint *grains;
    guint ngrains;
} GFilterArgs;

typedef struct {
    GFilterArgs *args;
    GwyContainer *mydata;
    GwyDataField *mask;

    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *table;
    GtkWidget *values;
    GtkWidget *expression;
    GtkWidget *color_button;
    GtkObject *nquantities;
    GtkWidget *set_as;
    GtkWidget *logical_op[NQUANTITIES];
    gint logop_col;
    gint logop_row;
    GtkObject *name[NQUANTITIES];
    GtkObject *lower[NQUANTITIES];
    GtkObject *upper[NQUANTITIES];
    GtkWidget *update;

    gboolean computed;
    gboolean in_init;
    gint set_as_id;
} GFilterControls;

static gboolean   module_register               (void);
static void       grain_filter                  (GwyContainer *data,
                                                 GwyRunType run);
static void       run_noninteractive            (GFilterArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GwyDataField *mfield,
                                                 GQuark mquark);
static void       gfilter_dialog                (GFilterArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GwyDataField *mfield,
                                                 gint id,
                                                 GQuark mquark);
static void       mask_color_changed            (GtkWidget *color_button,
                                                 GFilterControls *controls);
static void       load_mask_color               (GtkWidget *color_button,
                                                 GwyContainer *data);
static void       gfilter_dialog_update_controls(GFilterControls *controls,
                                                 GFilterArgs *args);
static void       gfilter_invalidate            (GFilterControls *controls);
static void       update_changed                (GFilterControls *controls,
                                                 GtkToggleButton *toggle);
static void       value_selected                (GFilterControls *controls,
                                                 GtkTreeSelection *selection);
static void       set_as_changed                (GtkComboBox *combo,
                                                 GFilterControls *controls);
static void       nquantities_changed           (GFilterControls *controls,
                                                 GtkAdjustment *adj);
static void       logical_op1_changed           (GtkComboBox *combo,
                                                 GFilterControls *controls);
static void       logical_op2_changed           (GtkComboBox *combo,
                                                 GFilterControls *controls);
static void       logical_op3_changed           (GtkComboBox *combo,
                                                 GFilterControls *controls);
static GPtrArray* calculate_all_grain_values    (GwyDataField *dfield,
                                                 GwyDataField *mask,
                                                 guint *ngrains,
                                                 gint **grains);
static void       gfilter_load_args             (GwyContainer *container,
                                                 GFilterArgs *args);
static void       gfilter_save_args             (GwyContainer *container,
                                                 GFilterArgs *args);
static void       gfilter_sanitize_args         (GFilterArgs *args);
static void       gfilter_free_args             (GFilterArgs *args);

static const GFilterArgs gfilter_defaults = {
    TRUE, 0,
    1,
    GRAIN_LOGICAL1_A,
    GRAIN_LOGICAL2_A_AND_B,
    GRAIN_LOGICAL3_A_AND_B_AND_C,
    /* Only the symbols matter. */
    {
        { "A_px", 5.0, G_MAXDOUBLE, FALSE },
        { "A_px", 5.0, G_MAXDOUBLE, FALSE },
        { "A_px", 5.0, G_MAXDOUBLE, FALSE },
    },
    /* Dynamic state */
    NULL,
    FALSE,
    NULL, NULL, 0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Filters grains by their properties, using logical expressions "
       "and thresholds."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("grain_filter",
                              (GwyProcessFunc)&grain_filter,
                              N_("/_Grains/_Filter..."),
                              GWY_STOCK_GRAINS_REMOVE,
                              GFILTER_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Filter grains by their properties"));

    return TRUE;
}

static void
grain_filter(GwyContainer *data, GwyRunType run)
{
    GwySIUnit *siunitxy, *siunitz;
    GFilterArgs args;
    GwyDataField *dfield, *mfield;
    GQuark mquark;
    gint id;

    g_return_if_fail(run & GFILTER_RUN_MODES);
    gfilter_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && mfield);

    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    siunitz = gwy_data_field_get_si_unit_z(dfield);
    args.units_equal = gwy_si_unit_equal(siunitxy, siunitz);
    args.valuedata = calculate_all_grain_values(dfield, mfield,
                                                &args.ngrains, &args.grains);

    /* Must precalculate grain quantities to limit the ranges correctly. */

    if (run == GWY_RUN_IMMEDIATE) {
        run_noninteractive(&args, data, dfield, mfield, mquark);
        gwy_app_channel_log_add(data, id, id, "proc::grain_filter",
                                NULL);
    }
    else
        gfilter_dialog(&args, data, dfield, mfield, id, mquark);

    gfilter_free_args(&args);
}

static void
run_noninteractive(GFilterArgs *args,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   GwyDataField *mfield,
                   GQuark mquark)
{
    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    //mask_process(dfield, mfield, args);
    gwy_data_field_data_changed(mfield);
}

static void
gfilter_dialog(GFilterArgs *args,
              GwyContainer *data,
              GwyDataField *dfield,
              GwyDataField *mfield,
              gint id,
              GQuark mquark)
{
    GtkWidget *dialog, *table, *hbox, *label, *scwin;
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GFilterControls controls;
    gint response, row, i;
    GwyPixmapLayer *layer;

    controls.args = args;
    controls.mask = mfield;
    controls.in_init = TRUE;
    controls.computed = FALSE;
    controls.set_as_id = 0;

    dialog = gtk_dialog_new_with_buttons(_("GFilter Grains by Threshold"),
                                         NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    mfield = gwy_data_field_duplicate(mfield);
    gwy_container_set_object_by_name(controls.mydata, "/0/mask", mfield);
    g_object_unref(mfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    layer = gwy_layer_mask_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/mask");
    gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(10, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    controls.table = table;
    row = 0;

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_table_attach(GTK_TABLE(table), scwin, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    controls.values = gwy_grain_value_tree_view_new(FALSE,
                                                    "name", "symbol_markup",
                                                    NULL);
    treeview = GTK_TREE_VIEW(controls.values);
    gtk_tree_view_set_headers_visible(treeview, FALSE);
    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    gwy_grain_value_tree_view_set_same_units(treeview, args->units_equal);
    gwy_grain_value_tree_view_set_expanded_groups(treeview, args->expanded);
    gtk_container_add(GTK_CONTAINER(scwin), controls.values);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(value_selected), &controls);
    row++;

    controls.expression = gtk_entry_new();
    gwy_table_attach_hscale(table, row++, _("E_xpression:"), NULL,
                            GTK_OBJECT(controls.expression), GWY_HSCALE_WIDGET);

    controls.set_as
        = gwy_enum_combo_box_newl(G_CALLBACK(set_as_changed), &controls, 0,
                                  "A", 0,
                                  "B", 1,
                                  "C", 2,
                                  NULL);
    gtk_table_attach(GTK_TABLE(table), controls.set_as, 3, 4, row-1, row,
                     0, 0, 0, 0);

    controls.nquantities = gtk_adjustment_new(args->nquantities, 1, 3, 1, 1, 0);
    gwy_table_attach_hscale(table, row++, _("_Number of quantities:"), NULL,
                            controls.nquantities, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.nquantities, "value-changed",
                             G_CALLBACK(nquantities_changed), &controls);

    controls.logical_op[0]
        = gwy_enum_combo_box_newl(G_CALLBACK(logical_op1_changed), &controls,
                                  args->logical1,
                                  "A", GRAIN_LOGICAL1_A,
                                  NULL);
    controls.logical_op[1]
        = gwy_enum_combo_box_newl(G_CALLBACK(logical_op2_changed), &controls,
                                  args->logical2,
                                  "A ∧ B", GRAIN_LOGICAL2_A_AND_B,
                                  "A ∨ B", GRAIN_LOGICAL2_A_OR_B,
                                  NULL);
    controls.logical_op[2]
        = gwy_enum_combo_box_newl(G_CALLBACK(logical_op3_changed), &controls,
                                  args->logical3,
                                  "A ∧ B ∧ C", GRAIN_LOGICAL3_A_AND_B_AND_C,
                                  "A ∨ B ∨ C", GRAIN_LOGICAL3_A_OR_B_OR_C,
                                  "(A ∧ B) ∨ C", GRAIN_LOGICAL3_A_AND_B_OR_C,
                                  "(A ∨ B) ∧ C", GRAIN_LOGICAL3_A_OR_B_AND_C,
                                  NULL);

    for (i = 0; i < NQUANTITIES; i++) {
        g_object_ref(controls.logical_op[i]);
        if (i+1 == args->nquantities) {
            gwy_table_attach_hscale(table, row++,
                                    _("Keep grains satisfying:"), NULL,
                                    GTK_OBJECT(controls.logical_op[i]),
                                    GWY_HSCALE_WIDGET);
            gtk_container_child_get(GTK_CONTAINER(table),
                                    controls.logical_op[i],
                                    "top-attach", &controls.logop_row,
                                    "left-attach", &controls.logop_col,
                                    NULL);
        }
    }

    controls.color_button = gwy_color_button_new();
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(controls.color_button),
                                   TRUE);
    load_mask_color(controls.color_button,
                    gwy_data_view_get_data(GWY_DATA_VIEW(controls.view)));
    gwy_table_attach_hscale(table, row++, _("_Mask color:"), NULL,
                            GTK_OBJECT(controls.color_button),
                            GWY_HSCALE_WIDGET_NO_EXPAND);
    g_signal_connect(controls.color_button, "clicked",
                     G_CALLBACK(mask_color_changed), &controls);
    row++;

    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_changed), &controls);

    /* finished initializing, allow instant updates */
    controls.in_init = FALSE;
    /* TODO: update entry to show expression for quantity A. */
    gfilter_invalidate(&controls);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->expanded = gwy_grain_value_tree_view_get_expanded_groups
                                             (GTK_TREE_VIEW(controls.values));
            for (i = 0; i < NQUANTITIES; i++)
                g_object_unref(controls.logical_op[i]);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            gfilter_save_args(gwy_app_settings_get(), args);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            controls.in_init = TRUE;
            /* TODO */
            gfilter_dialog_update_controls(&controls, args);
            controls.in_init = FALSE;
            gfilter_invalidate(&controls);
            break;

            case RESPONSE_PREVIEW:
            //preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->expanded = gwy_grain_value_tree_view_get_expanded_groups
                                             (GTK_TREE_VIEW(controls.values));
    gwy_app_sync_data_items(controls.mydata, data, 0, id, FALSE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    for (i = 0; i < NQUANTITIES; i++)
        g_object_unref(controls.logical_op[i]);
    gtk_widget_destroy(dialog);

    gfilter_save_args(gwy_app_settings_get(), args);

    if (controls.computed) {
        mfield = gwy_container_get_object_by_name(controls.mydata, "/0/mask");
        gwy_app_undo_qcheckpointv(data, 1, &mquark);
        gwy_container_set_object(data, mquark, mfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        run_noninteractive(args, data, dfield, controls.mask, mquark);
    }

    gwy_app_channel_log_add(data, id, id, "proc::grain_filter", NULL);
}

static void
gfilter_dialog_update_controls(GFilterControls *controls,
                               GFilterArgs *args)
{
    controls->in_init = TRUE;

    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->logical_op[0]),
                                  args->logical1);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->logical_op[1]),
                                  args->logical2);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->logical_op[2]),
                                  args->logical3);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->nquantities),
                             args->nquantities);

    controls->in_init = FALSE;
    gfilter_invalidate(controls);
}

static void
gfilter_invalidate(GFilterControls *controls)
{
    controls->computed = FALSE;

    if (controls->in_init || !controls->args->update)
        return;
}

static void
mask_color_changed(GtkWidget *color_button,
                   GFilterControls *controls)
{
    GwyContainer *data;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    gwy_mask_color_selector_run(NULL, GTK_WINDOW(controls->dialog),
                                GWY_COLOR_BUTTON(color_button), data,
                                "/0/mask");
    load_mask_color(color_button, data);
}

static void
load_mask_color(GtkWidget *color_button,
                GwyContainer *data)
{
    GwyRGBA rgba;

    if (!gwy_rgba_get_from_container(&rgba, data, "/0/mask")) {
        gwy_rgba_get_from_container(&rgba, gwy_app_settings_get(), "/mask");
        gwy_rgba_store_to_container(&rgba, data, "/0/mask");
    }
    gwy_color_button_set_color(GWY_COLOR_BUTTON(color_button), &rgba);
}

static void
update_changed(GFilterControls *controls, GtkToggleButton *toggle)
{
    GFilterArgs *args = controls->args;

    args->update = gtk_toggle_button_get_active(toggle);
    gfilter_invalidate(controls);
}

static void
value_selected(GFilterControls *controls, GtkTreeSelection *selection)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GwyGrainValue *gvalue;
    const gchar *symbol;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, 0, &gvalue, -1);
    symbol = gwy_grain_value_get_symbol(gvalue);
    gtk_entry_set_text(GTK_ENTRY(controls->expression), symbol);
}

static void
set_as_changed(GtkComboBox *combo, GFilterControls *controls)
{
    controls->set_as_id = gwy_enum_combo_box_get_active(combo);
    gtk_entry_set_text(GTK_ENTRY(controls->expression),
                       controls->args->ranges[controls->set_as_id].quantity);
}

static void
nquantities_changed(GFilterControls *controls, GtkAdjustment *adj)
{
    GFilterArgs *args = controls->args;

    gtk_container_remove(GTK_CONTAINER(controls->table),
                         controls->logical_op[args->nquantities-1]);
    args->nquantities = gwy_adjustment_get_int(adj);
    gtk_table_attach(GTK_TABLE(controls->table),
                     controls->logical_op[args->nquantities-1],
                     controls->logop_col, controls->logop_col+2,
                     controls->logop_row, controls->logop_row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_widget_show(controls->logical_op[args->nquantities-1]);
    gfilter_invalidate(controls);
}

static void
logical_op1_changed(GtkComboBox *combo, GFilterControls *controls)
{
    controls->args->logical1 = gwy_enum_combo_box_get_active(combo);
    gfilter_invalidate(controls);
}

static void
logical_op2_changed(GtkComboBox *combo, GFilterControls *controls)
{
    controls->args->logical2 = gwy_enum_combo_box_get_active(combo);
    gfilter_invalidate(controls);
}

static void
logical_op3_changed(GtkComboBox *combo, GFilterControls *controls)
{
    controls->args->logical3 = gwy_enum_combo_box_get_active(combo);
    gfilter_invalidate(controls);
}

static GPtrArray*
calculate_all_grain_values(GwyDataField *dfield,
                           GwyDataField *mask,
                           guint *ngrains,
                           gint **grains)
{
    GwyGrainValue **gvalues;
    guint xres = dfield->xres, yres = dfield->yres, n, i;
    GwyInventory *inventory;
    GPtrArray *valuedata;

    *grains = g_new0(gint, xres*yres);
    *ngrains = gwy_data_field_number_grains(mask, *grains);

    inventory = gwy_grain_values();
    n = gwy_inventory_get_n_items(inventory);

    valuedata = g_ptr_array_new();
    g_ptr_array_set_size(valuedata, n);

    gvalues = g_new(GwyGrainValue*, n);
    for (i = 0; i < n; i++) {
        gvalues[i] = gwy_inventory_get_nth_item(inventory, i);
        g_ptr_array_index(valuedata, i) = g_new(gdouble, *ngrains + 1);
    }

    gwy_grain_values_calculate(n, gvalues, (gdouble**)valuedata->pdata,
                               dfield, *ngrains, *grains);
    g_free(gvalues);

    return valuedata;
}

static const gchar nquantities_key[] = "/module/grain_filter/nquantities";
static const gchar logical1_key[]    = "/module/grain_filter/logical1";
static const gchar logical2_key[]    = "/module/grain_filter/logical2";
static const gchar logical3_key[]    = "/module/grain_filter/logical3";
static const gchar quantity_key[]    = "/module/grain_filter/quantity";
static const gchar update_key[]      = "/module/grain_filter/update";
static const gchar expanded_key[]    = "/module/grain_filter/expanded";

static void
gfilter_sanitize_args(GFilterArgs *args)
{
    guint i;

    args->nquantities = CLAMP(args->nquantities, 1, 3);
    args->logical1 = MIN(args->logical1, GRAIN_LOGICAL1_NTYPES);
    args->logical2 = MIN(args->logical2, GRAIN_LOGICAL2_NTYPES);
    args->logical3 = MIN(args->logical3, GRAIN_LOGICAL3_NTYPES);
    for (i = 0; i < NQUANTITIES; i++) {
        args->ranges[i].quantity = g_strdup(args->ranges[i].quantity);
        /* The rest of validation is the same as initialisation when the user
         * selects a new quantity.  Do it there. */
        /* Set is_expr:
         * - parses as expr and is identifier: TRUE
         * - parses as expr and is not identifier: FALSE
         * - does not parse: reset to some default quantity
         */
    }
    args->update = !!args->update;
}

static void
range_record_free(gpointer p)
{
    RangeRecord *rr = (RangeRecord*)p;
    g_slice_free(RangeRecord, rr);
}

static void
gfilter_load_args(GwyContainer *container,
                  GFilterArgs *args)
{
    gchar *filename, *buffer;
    gsize size;
    guint i;

    *args = gfilter_defaults;

    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, expanded_key, &args->expanded);
    gwy_container_gis_int32_by_name(container, nquantities_key,
                                    &args->nquantities);
    gwy_container_gis_enum_by_name(container, logical1_key, &args->logical1);
    gwy_container_gis_enum_by_name(container, logical2_key, &args->logical2);
    gwy_container_gis_enum_by_name(container, logical3_key, &args->logical3);

    for (i = 0; i < NQUANTITIES; i++) {
        gchar buf[sizeof(quantity_key) + 10];

        g_snprintf(buf, sizeof(buf), "%s%u", quantity_key, i+1);
        gwy_container_gis_string_by_name(container, buf,
                                         (const guchar**)&args->ranges[i].quantity);
    }

    args->ranges_history = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 NULL, range_record_free);
    filename = g_build_filename(gwy_get_user_dir(), "grain_filter", "ranges",
                                NULL);
    if (g_file_get_contents(filename, &buffer, &size, NULL)) {
        gchar *p = buffer, *line;
        for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
            g_strstrip(line);
            if (*line) {
                gchar **fields = g_strsplit(line, " ", 3);
                if (g_strv_length(fields) == 3) {
                    RangeRecord *rr = g_slice_new(RangeRecord);

                    /* TODO: we probably want to do sequential number parsing,
                     * not splitting, this handles multiple delimiters and
                     * stuff like that much better. */
                    rr->lower = g_ascii_strtod(fields[0], NULL);
                    rr->upper = g_ascii_strtod(fields[1], NULL);
                    g_strstrip(fields[2]);
                    rr->quantity = g_strdup(fields[2]);
                    /* FIXME */
                    rr->is_expr = TRUE;
                    g_hash_table_insert(args->ranges_history, rr->quantity, rr);
                }
                g_strfreev(fields);
            }
        }
        g_free(buffer);
    }
    g_free(filename);

    gfilter_sanitize_args(args);
}

static void
save_range(G_GNUC_UNUSED gpointer key, gpointer data, gpointer user_data)
{
    RangeRecord *rr = (RangeRecord*)data;
    FILE *fh = user_data;
    gchar buf_lower[G_ASCII_DTOSTR_BUF_SIZE];
    gchar buf_upper[G_ASCII_DTOSTR_BUF_SIZE];

    g_ascii_dtostr(buf_lower, G_ASCII_DTOSTR_BUF_SIZE, rr->lower);
    g_ascii_dtostr(buf_upper, G_ASCII_DTOSTR_BUF_SIZE, rr->upper);
    fprintf(fh, "%s %s %s\n", buf_lower, buf_upper, rr->quantity);
}

static void
gfilter_save_args(GwyContainer *container,
                  GFilterArgs *args)
{
    gchar *filename;
    FILE *fh;
    guint i;

    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, expanded_key, args->expanded);
    gwy_container_set_int32_by_name(container, nquantities_key,
                                    args->nquantities);
    gwy_container_set_enum_by_name(container, logical1_key, args->logical1);
    gwy_container_set_enum_by_name(container, logical2_key, args->logical2);
    gwy_container_set_enum_by_name(container, logical3_key, args->logical3);

    for (i = 0; i < NQUANTITIES; i++) {
        gchar buf[sizeof(quantity_key) + 10];

        g_snprintf(buf, sizeof(buf), "%s%u", quantity_key, i+1);
        gwy_container_set_string_by_name(container, buf,
                                         g_strdup(args->ranges[i].quantity));
    }

    filename = g_build_filename(gwy_get_user_dir(), "grain_filter", NULL);
    if (!g_file_test(filename, G_FILE_TEST_IS_DIR))
        g_mkdir(filename, 0700);
    g_free(filename);

    filename = g_build_filename(gwy_get_user_dir(), "grain_filter", "ranges",
                                NULL);
    if ((fh = g_fopen(filename, "w"))) {
        g_hash_table_foreach(args->ranges_history, save_range, fh);
        fclose(fh);
    }
    g_free(filename);
}

static void
gfilter_free_args(GFilterArgs *args)
{
    GwyInventory *inventory;
    guint i, n;

    for (i = 0; i < NQUANTITIES; i++)
        g_free(args->ranges[i].quantity);

    g_hash_table_destroy(args->ranges_history);

    inventory = gwy_grain_values();
    n = gwy_inventory_get_n_items(inventory);

    for (i = 0; i < n; i++)
        g_free(g_ptr_array_index(args->valuedata, i));

    g_ptr_array_free(args->valuedata, TRUE);
    g_free(args->grains);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
