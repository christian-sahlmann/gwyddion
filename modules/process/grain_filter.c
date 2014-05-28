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
#include <libgwyddion/gwymath.h>
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
    IS_UPPER = 1024,
    /* Taken from gwydgetutils. */
    GWY_HSCALE_WIDTH = 96
};

enum {
    RESPONSE_PREVIEW = 2
};

typedef enum {
    GRAIN_LOGICAL_A,
    GRAIN_LOGICAL_A_AND_B,
    GRAIN_LOGICAL_A_OR_B,
    GRAIN_LOGICAL_A_AND_B_AND_C,
    GRAIN_LOGICAL_A_OR_B_OR_C,
    GRAIN_LOGICAL_A_AND_B_OR_C,
    GRAIN_LOGICAL_A_OR_B_AND_C,
    GRAIN_LOGICAL_NTYPES,
} GrainLogical;

typedef struct {
    const gchar *quantity;
    gdouble lower;
    gdouble upper;
} RangeRecord;

typedef struct {
    gboolean update;
    gint expanded;
    GrainLogical logical;
    RangeRecord ranges[NQUANTITIES];

    GHashTable *ranges_history;

    /* To mask impossible quantitities without really resetting the bits */
    gboolean units_equal;

    GPtrArray *valuedata;
    GPtrArray *sortedvaluedata;
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
    GtkWidget *color_button;
    GtkWidget *set_as[NQUANTITIES];
    GtkWidget *logical_op;
    GwySIValueFormat *vf[NQUANTITIES];
    GtkWidget *header[NQUANTITIES];
    GtkObject *lower[NQUANTITIES];
    GtkWidget *lower_label[NQUANTITIES];
    GtkWidget *lower_scale[NQUANTITIES];
    GtkWidget *lower_entry[NQUANTITIES];
    GtkWidget *lower_units[NQUANTITIES];
    GtkObject *upper[NQUANTITIES];
    GtkWidget *upper_label[NQUANTITIES];
    GtkWidget *upper_scale[NQUANTITIES];
    GtkWidget *upper_entry[NQUANTITIES];
    GtkWidget *upper_units[NQUANTITIES];
    GtkWidget *update;

    gboolean computed;
    gboolean in_init;
} GFilterControls;

static gboolean   module_register              (void);
static void       grain_filter                 (GwyContainer *data,
                                                GwyRunType run);
static void       run_noninteractive           (GFilterArgs *args,
                                                GwyContainer *data,
                                                GwyDataField *mfield,
                                                GQuark mquark);
static void       gfilter_dialog               (GFilterArgs *args,
                                                GwyContainer *data,
                                                GwyDataField *dfield,
                                                GwyDataField *mfield,
                                                gint id,
                                                GQuark mquark);
static void       mask_color_changed           (GtkWidget *color_button,
                                                GFilterControls *controls);
static void       load_mask_color              (GtkWidget *color_button,
                                                GwyContainer *data);
static void       gfilter_invalidate           (GFilterControls *controls);
static void       preview                      (GFilterControls *controls);
static void       update_changed               (GFilterControls *controls,
                                                GtkToggleButton *toggle);
static void       set_as_clicked               (GFilterControls *controls,
                                                GtkButton *button);
static void       set_up_quantity              (GFilterControls *controls,
                                                GwyGrainValue *gvalue,
                                                guint id);
static void       set_adjustment_to_grain_value(GFilterControls *controls,
                                                GwyGrainValue *gvalue,
                                                GtkAdjustment *adj,
                                                gdouble value);
static guint      bisect_lower                 (const gdouble *a,
                                                guint n,
                                                gdouble x);
static void       logical_op_changed           (GtkComboBox *combo,
                                                GFilterControls *controls);
static void       threshold_changed            (GFilterControls *controls,
                                                GtkAdjustment *adj);
static void       threshold_activated          (GFilterControls *controls,
                                                GtkEntry *entry);
static void       set_threshold_value          (GFilterArgs *args,
                                                guint id,
                                                const gchar *name,
                                                gdouble value,
                                                gboolean is_upper);
static GPtrArray* calculate_all_grain_values   (GwyDataField *dfield,
                                                GwyDataField *mask,
                                                guint *ngrains,
                                                gint **grains);
static GPtrArray* sort_grain_values            (GPtrArray *valuedata,
                                                guint ngrains);
static void       gfilter_process              (GwyDataField *mfield,
                                                GFilterArgs *args);
static void       gfilter_load_args            (GwyContainer *container,
                                                GFilterArgs *args);
static void       gfilter_save_args            (GwyContainer *container,
                                                GFilterArgs *args);
static void       gfilter_sanitize_args        (GFilterArgs *args);
static void       gfilter_free_args            (GFilterArgs *args);

static const GFilterArgs gfilter_defaults = {
    TRUE, 0,
    GRAIN_LOGICAL_A,
    /* Only the symbols matter. */
    {
        { "Pixel area", 5.0, G_MAXDOUBLE },
        { "Pixel area", 5.0, G_MAXDOUBLE },
        { "Pixel area", 5.0, G_MAXDOUBLE },
    },
    /* Dynamic state */
    NULL,
    FALSE,
    NULL, NULL,
    NULL, 0,
};

static const GrainLogical logical_limits[NQUANTITIES+1] = {
    GRAIN_LOGICAL_A,
    GRAIN_LOGICAL_A_AND_B,
    GRAIN_LOGICAL_A_AND_B_AND_C,
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
    args.sortedvaluedata = sort_grain_values(args.valuedata, args.ngrains);
    if (!args.ngrains) {
        gfilter_free_args(&args);
        return;
    }

    if (run == GWY_RUN_IMMEDIATE) {
        run_noninteractive(&args, data, mfield, mquark);
        gwy_app_channel_log_add(data, id, id, "proc::grain_filter", NULL);
    }
    else
        gfilter_dialog(&args, data, dfield, mfield, id, mquark);

    gfilter_free_args(&args);
}

static void
run_noninteractive(GFilterArgs *args,
                   GwyContainer *data,
                   GwyDataField *mfield,
                   GQuark mquark)
{
    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    gfilter_process(mfield, args);
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
    GtkWidget *dialog, *table, *vbox, *hbox, *scwin, *hbox2, *label;
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GFilterControls controls;
    gint response, row, i;
    GwySIUnit *siunit;
    GwyPixmapLayer *layer;

    controls.args = args;
    controls.mask = mfield;
    controls.in_init = TRUE;
    controls.computed = FALSE;

    siunit = gwy_si_unit_new(NULL);
    for (i = 0; i < NQUANTITIES; i++) {
        controls.vf[i]
            = gwy_si_unit_get_format_with_digits(siunit,
                                                 GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                 1.0, 4, NULL);
    }
    g_object_unref(siunit);

    dialog = gtk_dialog_new_with_buttons(_("GFilter Grains by Threshold"),
                                         NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), RESPONSE_PREVIEW,
                                      !args->update);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 4);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

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

    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);

    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_box_pack_start(GTK_BOX(vbox), controls.update, FALSE, FALSE, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_changed), &controls);

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("_Mask color:"));
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    controls.color_button = gwy_color_button_new();
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(controls.color_button),
                                   TRUE);
    load_mask_color(controls.color_button,
                    gwy_data_view_get_data(GWY_DATA_VIEW(controls.view)));
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.color_button);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.color_button, FALSE, FALSE, 0);
    g_signal_connect(controls.color_button, "clicked",
                     G_CALLBACK(mask_color_changed), &controls);

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
    gtk_widget_set_size_request(scwin, -1, 120);
    gtk_tree_view_set_headers_visible(treeview, FALSE);
    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    gwy_grain_value_tree_view_set_same_units(treeview, args->units_equal);
    gwy_grain_value_tree_view_set_expanded_groups(treeview, args->expanded);
    gtk_container_add(GTK_CONTAINER(scwin), controls.values);
    row++;

    hbox2 = gtk_hbox_new(FALSE, 0);
    for (i = 0; i < NQUANTITIES; i++) {
        gchar buf[2];
        buf[0] = 'A' + i;
        buf[1] = '\0';
        controls.set_as[i] = gtk_button_new_with_label(buf);
        gtk_box_pack_start(GTK_BOX(hbox2), controls.set_as[i], FALSE, FALSE, 0);
        g_object_set_data(G_OBJECT(controls.set_as[i]),
                          "id", GUINT_TO_POINTER(i));
        g_signal_connect_swapped(controls.set_as[i], "clicked",
                                 G_CALLBACK(set_as_clicked), &controls);
    }
    gwy_table_attach_hscale(table, row++,
                            _("Set selected as:"), NULL,
                            GTK_OBJECT(hbox2), GWY_HSCALE_WIDGET_NO_EXPAND);

    controls.logical_op
        = gwy_enum_combo_box_newl(G_CALLBACK(logical_op_changed), &controls,
                                  args->logical,
                                  "A", GRAIN_LOGICAL_A,
                                  "A ∧ B", GRAIN_LOGICAL_A_AND_B,
                                  "A ∨ B", GRAIN_LOGICAL_A_OR_B,
                                  "A ∧ B ∧ C", GRAIN_LOGICAL_A_AND_B_AND_C,
                                  "A ∨ B ∨ C", GRAIN_LOGICAL_A_OR_B_OR_C,
                                  "(A ∧ B) ∨ C", GRAIN_LOGICAL_A_AND_B_OR_C,
                                  "(A ∨ B) ∧ C", GRAIN_LOGICAL_A_OR_B_AND_C,
                                  NULL);
    gwy_table_attach_hscale(table, row++,
                            _("Keep grains satisfying:"), NULL,
                            GTK_OBJECT(controls.logical_op),
                            GWY_HSCALE_WIDGET);

    for (i = 0; i < NQUANTITIES; i++) {
        gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

        controls.header[i] = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(controls.header[i]), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), controls.header[i],
                         0, 3, row, row+1, GTK_FILL, 0, 0, 0);
        row++;

        /* The values are set properly later. */
        controls.lower_label[i] = gtk_label_new(_("Lower threshold:"));
        gtk_misc_set_alignment(GTK_MISC(controls.lower_label[i]), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), controls.lower_label[i],
                         0, 1, row, row+1, GTK_FILL, 0, 0, 0);

        controls.lower[i] = gtk_adjustment_new(1.0, 1.0, args->ngrains-1,
                                               1.0, 10.0, 0.0);
        g_object_set_data(G_OBJECT(controls.lower[i]), "id",
                          GUINT_TO_POINTER(i));
        g_signal_connect_swapped(controls.lower[i], "value-changed",
                                 G_CALLBACK(threshold_changed), &controls);
        controls.lower_scale[i]
            = gtk_hscale_new(GTK_ADJUSTMENT(controls.lower[i]));
        gtk_scale_set_draw_value(GTK_SCALE(controls.lower_scale[i]), FALSE);
        gtk_widget_set_size_request(controls.lower_scale[i],
                                    GWY_HSCALE_WIDTH, -1);
        gtk_table_attach(GTK_TABLE(table), controls.lower_scale[i],
                         1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

        controls.lower_entry[i] = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(controls.lower_entry[i]), 8);
        gtk_table_attach(GTK_TABLE(table), controls.lower_entry[i],
                         2, 3, row, row+1, GTK_FILL, 0, 0, 0);
        gwy_widget_set_activate_on_unfocus(controls.lower_entry[i], TRUE);
        g_object_set_data(G_OBJECT(controls.lower_entry[i]), "id",
                          GUINT_TO_POINTER(i));
        g_signal_connect_swapped(controls.lower_entry[i], "activate",
                                 G_CALLBACK(threshold_activated), &controls);

        controls.lower_units[i] = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(controls.lower_units[i]), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), controls.lower_units[i],
                         3, 4, row, row+1, GTK_FILL, 0, 0, 0);
        row++;

        controls.upper_label[i] = gtk_label_new(_("Upper threshold:"));
        gtk_misc_set_alignment(GTK_MISC(controls.upper_label[i]), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), controls.upper_label[i],
                         0, 1, row, row+1, GTK_FILL, 0, 0, 0);

        controls.upper[i] = gtk_adjustment_new(1.0, 1.0, args->ngrains-1,
                                               1.0, 10.0, 0.0);
        g_object_set_data(G_OBJECT(controls.upper[i]), "id",
                          GUINT_TO_POINTER(i | IS_UPPER));
        g_signal_connect_swapped(controls.upper[i], "value-changed",
                                 G_CALLBACK(threshold_changed), &controls);
        controls.upper_scale[i]
            = gtk_hscale_new(GTK_ADJUSTMENT(controls.upper[i]));
        gtk_scale_set_draw_value(GTK_SCALE(controls.upper_scale[i]), FALSE);
        gtk_widget_set_size_request(controls.upper_scale[i],
                                    GWY_HSCALE_WIDTH, -1);
        gtk_table_attach(GTK_TABLE(table), controls.upper_scale[i],
                         1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

        controls.upper_entry[i] = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(controls.upper_entry[i]), 8);
        gtk_table_attach(GTK_TABLE(table), controls.upper_entry[i],
                         2, 3, row, row+1, GTK_FILL, 0, 0, 0);
        gwy_widget_set_activate_on_unfocus(controls.upper_entry[i], TRUE);
        g_object_set_data(G_OBJECT(controls.upper_entry[i]), "id",
                          GUINT_TO_POINTER(i | IS_UPPER));
        g_signal_connect_swapped(controls.upper_entry[i], "activate",
                                 G_CALLBACK(threshold_activated), &controls);

        controls.upper_units[i] = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(controls.upper_units[i]), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), controls.upper_units[i],
                         3, 4, row, row+1, GTK_FILL, 0, 0, 0);
        row++;
    }

    for (i = 0; i < NQUANTITIES; i++) {
        GwyInventory *inventory;
        GwyGrainValue *gvalue;

        inventory = gwy_grain_values();
        gvalue = gwy_inventory_get_item(inventory, args->ranges[i].quantity);
        set_up_quantity(&controls, gvalue, i);
    }
    logical_op_changed(GTK_COMBO_BOX(controls.logical_op), &controls);

    /* finished initializing, allow instant updates */
    controls.in_init = FALSE;

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->expanded = gwy_grain_value_tree_view_get_expanded_groups
                                             (GTK_TREE_VIEW(controls.values));
            for (i = 0; i < NQUANTITIES; i++)
                gwy_si_unit_value_format_free(controls.vf[i]);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            gfilter_save_args(gwy_app_settings_get(), args);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_PREVIEW:
            preview(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    for (i = 0; i < NQUANTITIES; i++)
        gwy_si_unit_value_format_free(controls.vf[i]);
    args->expanded = gwy_grain_value_tree_view_get_expanded_groups
                                             (GTK_TREE_VIEW(controls.values));
    gwy_app_sync_data_items(controls.mydata, data, 0, id, FALSE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
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
        run_noninteractive(args, data, controls.mask, mquark);
    }

    gwy_app_channel_log_add(data, id, id, "proc::grain_filter", NULL);
}

static void
gfilter_invalidate(GFilterControls *controls)
{
    controls->computed = FALSE;

    if (controls->in_init || !controls->args->update)
        return;

    preview(controls);
}

static void
preview(GFilterControls *controls)
{
    GwyDataField *mfield;

    mfield = gwy_container_get_object_by_name(controls->mydata, "/0/mask");
    gfilter_process(mfield, controls->args);
    gwy_data_field_data_changed(mfield);
    controls->computed = TRUE;
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
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW, !args->update);
    gfilter_invalidate(controls);
}

static void
set_as_clicked(GFilterControls *controls, GtkButton *button)
{
    GtkTreeSelection *selection;
    GwyGrainValue *gvalue;
    GtkTreeIter iter;
    GtkTreeModel *model;
    guint id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "id"));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->values));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, 0, &gvalue, -1);
    set_up_quantity(controls, gvalue, id);
}

static void
set_up_quantity(GFilterControls *controls, GwyGrainValue *gvalue, guint id)
{
    GFilterArgs *args = controls->args;
    GwyInventory *inventory;
    GwyDataField *dfield;
    RangeRecord *rr;
    const gchar *name;
    const gdouble *v;
    gchar *s, *t;
    gdouble vmin, vmax, lower = -G_MAXDOUBLE, upper = G_MAXDOUBLE;
    GwySIUnit *siunit, *siunitxy, *siunitz;
    gboolean was_in_init = controls->in_init;
    guint i;

    controls->in_init = TRUE;
    name = gwy_resource_get_name(GWY_RESOURCE(gvalue));
    args->ranges[id].quantity = name;
    /* TRANSLATORS: %c is replaced with quantity label A, B or C. */
    s = g_strdup_printf(_("Condition %c: %s"), 'A' + id, name);
    t = g_strconcat("<b>", s, "</b>", NULL);
    gtk_label_set_markup(GTK_LABEL(controls->header[id]), t);
    g_free(t);
    g_free(s);

    rr = g_hash_table_lookup(args->ranges_history, (gpointer)name);
    if (rr) {
        lower = rr->lower;
        upper = rr->upper;
    }

    inventory = gwy_grain_values();
    i = gwy_inventory_get_item_position(inventory, name);
    v = g_ptr_array_index(args->sortedvaluedata, i);
    vmin = v[0];
    vmax = v[args->ngrains-1];
    lower = CLAMP(lower, vmin, vmax);
    upper = CLAMP(upper, vmin, vmax);

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    siunitz = gwy_data_field_get_si_unit_z(dfield);
    siunit = gwy_si_unit_power_multiply(siunitxy,
                                        gwy_grain_value_get_power_xy(gvalue),
                                        siunitz,
                                        gwy_grain_value_get_power_z(gvalue),
                                        NULL);
    gwy_si_unit_get_format_with_digits(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                       MAX(fabs(vmin), fabs(vmax)), 4,
                                       controls->vf[id]);
    g_object_unref(siunit);
    /* Special-case the pixel area format. */
    if (gwy_strequal(name, "Pixel area")) {
        controls->vf[id]->magnitude = 1.0;
        controls->vf[id]->precision = 0;
        gwy_si_unit_value_format_set_units(controls->vf[id], "");
    }
    gtk_label_set_markup(GTK_LABEL(controls->lower_units[id]),
                         controls->vf[id]->units);
    gtk_label_set_markup(GTK_LABEL(controls->upper_units[id]),
                         controls->vf[id]->units);

    args->ranges[id].quantity = name;
    args->ranges[id].lower = lower;
    args->ranges[id].upper = upper;

    set_adjustment_to_grain_value(controls, gvalue,
                                  GTK_ADJUSTMENT(controls->lower[id]), lower);
    set_adjustment_to_grain_value(controls, gvalue,
                                  GTK_ADJUSTMENT(controls->upper[id]), upper);

    s = g_strdup_printf("%.*f",
                        controls->vf[id]->precision,
                        lower/controls->vf[id]->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->lower_entry[id]), s);
    g_free(s);

    s = g_strdup_printf("%.*f",
                        controls->vf[id]->precision,
                        upper/controls->vf[id]->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->upper_entry[id]), s);
    g_free(s);

    /* XXX: We might have modified the range by CLAMP().  Store the new one
     * right here?  Pro: consistency.  Con: the user did not do anything,
     * he just may be browsing. */
    controls->in_init = was_in_init;
    gfilter_invalidate(controls);
}

static void
set_adjustment_to_grain_value(GFilterControls *controls,
                              GwyGrainValue *gvalue,
                              GtkAdjustment *adj,
                              gdouble value)
{
    GFilterArgs *args = controls->args;
    GwyInventory *inventory;
    const gchar *name;
    guint k, i;
    const gdouble *v;

    inventory = gwy_grain_values();
    name = gwy_resource_get_name(GWY_RESOURCE(gvalue));
    i = gwy_inventory_get_item_position(inventory, name);
    v = g_ptr_array_index(args->sortedvaluedata, i);
    k = bisect_lower(v, args->ngrains, value);
    /* XXX: This is approximate, but maybe sufficient. */
    gtk_adjustment_set_value(adj, k);
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

static void
logical_op_changed(GtkComboBox *combo, GFilterControls *controls)
{
    guint i;
    GrainLogical logical;

    logical = gwy_enum_combo_box_get_active(combo);
    controls->args->logical = logical;

    for (i = 0; i < NQUANTITIES; i++) {
        gboolean sens = (logical >= logical_limits[i]);

        gtk_widget_set_sensitive(controls->set_as[i], sens);
        gtk_widget_set_sensitive(controls->header[i], sens);
        gtk_widget_set_sensitive(controls->lower_label[i], sens);
        gtk_widget_set_sensitive(controls->lower_scale[i], sens);
        gtk_widget_set_sensitive(controls->lower_entry[i], sens);
        gtk_widget_set_sensitive(controls->lower_units[i], sens);
        gtk_widget_set_sensitive(controls->upper_label[i], sens);
        gtk_widget_set_sensitive(controls->upper_scale[i], sens);
        gtk_widget_set_sensitive(controls->upper_entry[i], sens);
        gtk_widget_set_sensitive(controls->upper_units[i], sens);
    }
    gfilter_invalidate(controls);
}

static void
threshold_changed(GFilterControls *controls, GtkAdjustment *adj)
{
    GFilterArgs *args = controls->args;
    GwyInventory *inventory;
    GwyGrainValue *gvalue;
    GtkEntry *entry;
    const gchar *name;
    gboolean is_upper;
    guint id, i, k;
    gdouble *v;
    gdouble value;
    gchar *s;

    if (controls->in_init)
        return;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(adj), "id"));
    is_upper = id & IS_UPPER;
    id &= ~IS_UPPER;

    k = gwy_adjustment_get_int(adj);
    inventory = gwy_grain_values();
    gvalue = gwy_inventory_get_item(inventory, args->ranges[id].quantity);
    g_return_if_fail(gvalue);
    name = gwy_resource_get_name(GWY_RESOURCE(gvalue));
    i = gwy_inventory_get_item_position(inventory, args->ranges[id].quantity);
    v = g_ptr_array_index(args->sortedvaluedata, i);
    value = v[k];

    s = g_strdup_printf("%.*f",
                        controls->vf[id]->precision,
                        value/controls->vf[id]->magnitude);
    if (is_upper)
        entry = GTK_ENTRY(controls->upper_entry[id]);
    else
        entry = GTK_ENTRY(controls->lower_entry[id]);

    gtk_entry_set_text(entry, s);
    g_free(s);

    set_threshold_value(args, id, name, value, is_upper);
    gfilter_invalidate(controls);
}

static void
threshold_activated(GFilterControls *controls, GtkEntry *entry)
{
    GFilterArgs *args = controls->args;
    GwyInventory *inventory;
    GwyGrainValue *gvalue;
    GtkAdjustment *adj;
    guint id;
    gboolean is_upper;
    const gchar *s, *name;
    gdouble value;

    if (controls->in_init)
        return;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(entry), "id"));
    is_upper = id & IS_UPPER;
    id &= ~IS_UPPER;

    inventory = gwy_grain_values();
    gvalue = gwy_inventory_get_item(inventory, args->ranges[id].quantity);
    g_return_if_fail(gvalue);
    name = gwy_resource_get_name(GWY_RESOURCE(gvalue));

    s = gtk_entry_get_text(entry);
    value = g_strtod(s, NULL) * controls->vf[id]->magnitude;

    if (is_upper)
        adj = GTK_ADJUSTMENT(controls->upper[id]);
    else
        adj = GTK_ADJUSTMENT(controls->lower[id]);

    controls->in_init = TRUE;
    set_adjustment_to_grain_value(controls, gvalue, adj, value);
    controls->in_init = FALSE;

    set_threshold_value(args, id, name, value, is_upper);
    gfilter_invalidate(controls);
}

static void
set_threshold_value(GFilterArgs *args,
                    guint id, const gchar *name,
                    gdouble value, gboolean is_upper)
{
    RangeRecord *rr;

    if (is_upper)
        args->ranges[id].upper = value;
    else
        args->ranges[id].lower = value;

    rr = g_hash_table_lookup(args->ranges_history, (gpointer)name);
    if (rr) {
        if (is_upper)
            rr->upper = value;
        else
            rr->lower = value;
    }
    else {
        rr = g_slice_new(RangeRecord);
        rr->lower = args->ranges[id].lower;
        rr->upper = args->ranges[id].upper;
        rr->quantity = name;
        g_hash_table_insert(args->ranges_history, (gpointer)name, rr);
    }
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

static GPtrArray*
sort_grain_values(GPtrArray *valuedata,
                  guint ngrains)
{
    GwyInventory *inventory;
    GPtrArray *sortedvaluedata;
    guint n, i, datasize;

    inventory = gwy_grain_values();
    n = gwy_inventory_get_n_items(inventory);

    sortedvaluedata = g_ptr_array_new();
    g_ptr_array_set_size(sortedvaluedata, n);

    datasize = ngrains*sizeof(gdouble);
    for (i = 0; i < n; i++) {
        const gdouble *vdi = g_ptr_array_index(valuedata, i);
        /* +1 to avoid the zeroth non-grain */
        gdouble *svdi = g_memdup(vdi + 1, datasize);

        gwy_math_sort(ngrains, svdi);
        g_ptr_array_index(sortedvaluedata, i) = svdi;
    }

    return sortedvaluedata;
}

static inline gboolean
check_threshold(gdouble v, gdouble lower, gdouble upper)
{
    return (lower <= upper
            ? v >= lower && v <= upper
            : v >= lower || v <= upper);
}

static void
gfilter_process(GwyDataField *mfield, GFilterArgs *args)
{
    GwyInventory *inventory;
    GrainLogical logical;
    const gdouble *v[NQUANTITIES];
    const guint *grains;
    const RangeRecord *ranges;
    gboolean *keep_grain;
    guint i, k, n, ngrains;

    inventory = gwy_grain_values();
    for (i = 0; i < NQUANTITIES; i++) {
        k = gwy_inventory_get_item_position(inventory,
                                            args->ranges[i].quantity);
        v[i] = g_ptr_array_index(args->valuedata, k);
    }

    ngrains = args->ngrains;
    grains = args->grains;
    logical = args->logical;
    ranges = &args->ranges[0];

    keep_grain = g_new(gboolean, ngrains+1);
    keep_grain[0] = FALSE;
    for (k = 1; k <= ngrains; k++) {
        gboolean is_ok[NQUANTITIES];

        for (i = 0; i < NQUANTITIES; i++) {
             is_ok[i] = check_threshold(v[i][k],
                                        ranges[i].lower,
                                        ranges[i].upper);
        }
        if (logical == GRAIN_LOGICAL_A)
            keep_grain[k] = is_ok[0];
        else if (logical == GRAIN_LOGICAL_A_AND_B)
            keep_grain[k] = is_ok[0] && is_ok[1];
        else if (logical == GRAIN_LOGICAL_A_OR_B)
            keep_grain[k] = is_ok[0] || is_ok[1];
        else if (logical == GRAIN_LOGICAL_A_AND_B_AND_C)
            keep_grain[k] = is_ok[0] && is_ok[1] && is_ok[2];
        else if (logical == GRAIN_LOGICAL_A_OR_B_OR_C)
            keep_grain[k] = is_ok[0] || is_ok[1] || is_ok[2];
        else if (logical == GRAIN_LOGICAL_A_AND_B_OR_C)
            keep_grain[k] = (is_ok[0] && is_ok[1]) || is_ok[2];
        else if (logical == GRAIN_LOGICAL_A_OR_B_AND_C)
            keep_grain[k] = (is_ok[0] || is_ok[1]) && is_ok[2];
        else {
            g_assert_not_reached();
        }
    }

    n = mfield->xres * mfield->yres;
    for (k = 0; k < n; k++)
        mfield->data[k] = keep_grain[grains[k]];
    gwy_data_field_invalidate(mfield);

    g_free(keep_grain);
}

static const gchar logical_key[]     = "/module/grain_filter/logical";
static const gchar quantity_key[]    = "/module/grain_filter/quantity";
static const gchar update_key[]      = "/module/grain_filter/update";
static const gchar expanded_key[]    = "/module/grain_filter/expanded";

static void
gfilter_sanitize_args(GFilterArgs *args)
{
    GwyInventory *inventory;
    guint i;

    inventory = gwy_grain_values();

    args->logical = MIN(args->logical, GRAIN_LOGICAL_NTYPES);
    for (i = 0; i < NQUANTITIES; i++) {
        RangeRecord *rr = args->ranges + i;
        GwyGrainValue *gvalue;

        if ((gvalue = gwy_inventory_get_item(inventory, rr->quantity)))
            rr->quantity = gwy_resource_get_name(GWY_RESOURCE(gvalue));
        else
            rr->quantity = gfilter_defaults.ranges[i].quantity;
        /* The actual range is restored later from range_history. */
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
    GwyInventory *inventory;
    gchar *filename, *buffer;
    gsize size;
    guint i;

    inventory = gwy_grain_values();
    *args = gfilter_defaults;

    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, expanded_key, &args->expanded);
    gwy_container_gis_enum_by_name(container, logical_key, &args->logical);

    for (i = 0; i < NQUANTITIES; i++) {
        RangeRecord *rr = args->ranges + i;
        gchar buf[sizeof(quantity_key) + 10];

        g_snprintf(buf, sizeof(buf), "%s%u", quantity_key, i+1);
        gwy_container_gis_string_by_name(container, buf,
                                         (const guchar**)&rr->quantity);
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
                GwyGrainValue *gvalue;
                RangeRecord *rr;
                gchar *s = line, *end;
                gdouble lower, upper;

                lower = g_ascii_strtod(s, &end);
                s = end;
                upper = g_ascii_strtod(s, &end);
                if (end == s) {
                    g_warning("Invalid grain_filter range record: %s.", line);
                    continue;
                }
                s = end;
                g_strstrip(s);
                if (!(gvalue = gwy_inventory_get_item(inventory, s))) {
                    g_warning("Invalid grain_filter range record: %s.", line);
                    continue;
                }

                rr = g_slice_new(RangeRecord);
                rr->lower = lower;
                rr->upper = upper;
                rr->quantity = gwy_resource_get_name(GWY_RESOURCE(gvalue));
                g_hash_table_insert(args->ranges_history,
                                    (gpointer)rr->quantity, rr);
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
    gwy_container_set_enum_by_name(container, logical_key, args->logical);

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

    g_hash_table_destroy(args->ranges_history);

    inventory = gwy_grain_values();
    n = gwy_inventory_get_n_items(inventory);

    for (i = 0; i < n; i++) {
        g_free(g_ptr_array_index(args->valuedata, i));
        g_free(g_ptr_array_index(args->sortedvaluedata, i));
    }

    g_ptr_array_free(args->valuedata, TRUE);
    g_ptr_array_free(args->sortedvaluedata, TRUE);
    g_free(args->grains);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
