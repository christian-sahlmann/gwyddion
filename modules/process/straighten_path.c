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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/correct.h>
#include <libprocess/interpolation.h>
#include <libprocess/spline.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>
#include "preview.h"

#define STRAIGHTEN_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    RESPONSE_PREVIEW = 1,
};

enum {
    COLUMN_I, COLUMN_X, COLUMN_Y, NCOLUMNS
};

typedef struct {
    gint thickness;
    GwyInterpolationType interp;
    gdouble slackness;
    gboolean closed;
} StraightenArgs;

typedef struct {
    StraightenArgs *args;
    GwyDataField *dfield;
    GtkWidget *dialogue;
    GtkWidget *view;
    GtkWidget *view_result;
    GwyVectorLayer *vlayer;
    GwySelection *selection;
    GwySelection *orig_selection;
    GwyContainer *mydata;
    GtkWidget *coordlist;
    GtkWidget *interp;
    GtkObject *thickness;
    GtkObject *slackness;
    GtkWidget *closed;
    gdouble zoom;
    gboolean realsquare;
} StraightenControls;

static gboolean      module_register         (void);
static void          straighten_path         (GwyContainer *data,
                                              GwyRunType run);
static gint          straighten_dialogue     (StraightenArgs *args,
                                              GwyContainer *data,
                                              GwyDataField *dfield,
                                              gint id,
                                              gint maxthickness);
static void          preview                 (StraightenControls *controls);
static void          reset_path              (StraightenControls *controls);
static void          restore_path            (StraightenControls *controls);
static void          reverse_path            (StraightenControls *controls);
static void          init_selection          (GwySelection *selection,
                                              GwyDataField *dfield,
                                              const StraightenArgs *args);
static void          path_selection_changed  (StraightenControls *controls,
                                              gint hint);
static void          interpolation_changed   (GtkComboBox *combo,
                                              StraightenControls *controls);
static void          thickness_changed       (StraightenControls *controls,
                                              GtkAdjustment *adj);
static void          slackness_changed       (StraightenControls *controls,
                                              GtkAdjustment *adj);
static void          closed_changed          (StraightenControls *controls,
                                              GtkToggleButton *toggle);
static GtkWidget*    create_coord_list       (StraightenControls *controls);
static void          render_coord_cell       (GtkCellLayout *layout,
                                              GtkCellRenderer *renderer,
                                              GtkTreeModel *model,
                                              GtkTreeIter *iter,
                                              gpointer user_data);
static gboolean      delete_selection_object (GtkTreeView *treeview,
                                              GdkEventKey *event,
                                              StraightenControls *controls);
static GwyDataField* straighten_do           (GwyDataField *dfield,
                                              GwyDataField *result,
                                              GwySelection *selection,
                                              const StraightenArgs *args,
                                              gboolean realsquare);
static void          straighten_load_args    (GwyContainer *container,
                                              StraightenArgs *args);
static void          straighten_save_args    (GwyContainer *container,
                                              StraightenArgs *args);
static void          straighten_sanitize_args(StraightenArgs *args);

static const StraightenArgs straighten_defaults = {
    1, GWY_INTERPOLATION_LINEAR,
    1.0/G_SQRT2, FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Extracts a straightened part of image along a curve."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("straighten_path",
                              (GwyProcessFunc)&straighten_path,
                              N_("/_Distortion/Straighten _Path..."),
                              NULL,
                              STRAIGHTEN_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Straighten along a path"));

    return TRUE;
}

static void
straighten_path(GwyContainer *data, GwyRunType run)
{
    StraightenArgs args;
    GwyDataField *dfield;
    gint id, newid, maxthickness;

    g_return_if_fail(run & STRAIGHTEN_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerPath"));
    straighten_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    maxthickness = MAX(gwy_data_field_get_xres(dfield),
                       gwy_data_field_get_yres(dfield))/2;
    maxthickness = MAX(maxthickness, 3);

    newid = straighten_dialogue(&args, data, dfield, id, maxthickness);
    straighten_save_args(gwy_app_settings_get(), &args);
    if (newid != -1)
        gwy_app_channel_log_add_proc(data, id, newid);
}

static gint
straighten_dialogue(StraightenArgs *args,
                    GwyContainer *data,
                    GwyDataField *dfield,
                    gint id,
                    gint maxthickness)
{
    GtkWidget *hbox, *alignment, *scwin, *hbox2, *button;
    GtkDialog *dialogue;
    GtkTable *table;
    StraightenControls controls;
    GwyDataField *result, *mask;
    gint response, row, newid = -1;
    GObject *selection;
    gchar selkey[40];

    gwy_clear(&controls, 1);
    controls.args = args;
    controls.dfield = dfield;

    controls.dialogue = gtk_dialog_new_with_buttons(_("Straighten Path"),
                                                    NULL, 0, NULL);
    dialogue = GTK_DIALOG(controls.dialogue);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialogue),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(dialogue, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(dialogue, GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(dialogue, GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialogue)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    gwy_container_gis_boolean_by_name(controls.mydata, "/0/data/realsquare",
                                      &controls.realsquare);

    result = gwy_data_field_new(5, gwy_data_field_get_yres(dfield),
                                5, gwy_data_field_get_yres(dfield),
                                TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/1/data", result);
    gwy_app_sync_data_items(data, controls.mydata, id, 1, FALSE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);

    table = GTK_TABLE(gtk_table_new(6, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    row = 0;

    scwin = create_coord_list(&controls);
    gtk_table_attach(table, scwin, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    row++;

    hbox2 = gtk_hbox_new(TRUE, 0);
    gtk_table_attach(table, hbox2, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    button = gtk_button_new_with_mnemonic(_("_Reset"));
    gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(reset_path), &controls);

    button = gtk_button_new_with_mnemonic(_("Res_tore"));
    gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(restore_path), &controls);

    button = gtk_button_new_with_mnemonic(_("Re_verse"));
    gtk_box_pack_start(GTK_BOX(hbox2), button, TRUE, TRUE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(reverse_path), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(interpolation_changed), &controls,
                                 args->interp, TRUE);
    gwy_table_attach_hscale(GTK_WIDGET(table), row,
                            _("_Interpolation type:"), NULL,
                            GTK_OBJECT(controls.interp), GWY_HSCALE_WIDGET);
    row++;

    args->thickness = MIN(args->thickness, maxthickness);
    controls.thickness = gtk_adjustment_new(args->thickness, 3.0, maxthickness,
                                            1.0, 10.0, 0.0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Thickness:"), "px",
                            controls.thickness, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.thickness, "value-changed",
                             G_CALLBACK(thickness_changed), &controls);
    row++;

    controls.slackness = gtk_adjustment_new(args->slackness, 0.0, G_SQRT2,
                                            0.001, 0.1, 0.0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Slackness:"), NULL,
                            controls.slackness, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.slackness, "value-changed",
                             G_CALLBACK(slackness_changed), &controls);
    row++;

    controls.closed = gtk_check_button_new_with_mnemonic(_("C_losed curve"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.closed),
                                 args->closed);
    gtk_table_attach(table, controls.closed,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.closed, "toggled",
                             G_CALLBACK(closed_changed), &controls);
    row++;

    alignment = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), alignment, FALSE, FALSE, 4);

    controls.view = create_preview(controls.mydata, 0, PREVIEW_SIZE, FALSE);
    controls.selection = create_vector_layer(GWY_DATA_VIEW(controls.view),
                                             0, "Path", TRUE);
    g_object_ref(controls.selection);
    gwy_selection_set_max_objects(controls.selection, 1024);
    controls.vlayer = gwy_data_view_get_top_layer(GWY_DATA_VIEW(controls.view));
    gtk_container_add(GTK_CONTAINER(alignment), controls.view);

    alignment = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), alignment, FALSE, FALSE, 4);

    controls.view_result = create_preview(controls.mydata, 1, PREVIEW_SIZE,
                                          TRUE);
    ensure_mask_color(controls.mydata, 1);
    gtk_container_add(GTK_CONTAINER(alignment), controls.view_result);

    gtk_widget_show_all(controls.dialogue);

    g_signal_connect_swapped(controls.selection, "changed",
                             G_CALLBACK(path_selection_changed), &controls);

    g_snprintf(selkey, sizeof(selkey), "/%d/select/path", id);
    if (gwy_container_gis_object_by_name(data, selkey, &selection)
        && gwy_selection_get_data(GWY_SELECTION(selection), NULL) > 1) {
        gwy_debug("init selection from container");
        gwy_serializable_clone(selection, G_OBJECT(controls.selection));
        g_object_get(selection,
                     "slackness", &args->slackness,
                     "closed", &args->closed,
                     NULL);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls.slackness),
                                 args->slackness);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.closed),
                                     args->closed);
    }
    else {
        gwy_debug("init selection afresh");
        init_selection(controls.selection, dfield, args);
    }

    controls.orig_selection = gwy_selection_duplicate(controls.selection);

    /* We do not get the right value before the data view is shown. */
    controls.zoom = gwy_data_view_get_real_zoom(GWY_DATA_VIEW(controls.view));
    g_object_set(controls.vlayer, "thickness",
                 GWY_ROUND(controls.zoom*args->thickness), NULL);

    do {
        response = gtk_dialog_run(dialogue);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(controls.dialogue);
            case GTK_RESPONSE_NONE:
            goto finalize;
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

    result = gwy_container_get_object_by_name(controls.mydata, "/1/data");
    mask = straighten_do(dfield, result, controls.selection,
                         args, controls.realsquare);

    newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
    gwy_app_set_data_field_title(data, newid, _("Straightened"));
    gwy_app_sync_data_items(data, data, id, newid, FALSE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    if (mask) {
        gwy_container_set_object(data,
                                 gwy_app_get_mask_key_for_id(newid), mask);
        g_object_unref(mask);
    }

    gtk_widget_destroy(controls.dialogue);

finalize:
    g_snprintf(selkey, sizeof(selkey), "/%d/select/path", id);
    selection = gwy_serializable_duplicate(G_OBJECT(controls.selection));
    gwy_container_set_object_by_name(data, selkey, selection);
    g_object_unref(selection);
    g_object_unref(controls.selection);
    g_object_unref(controls.orig_selection);
    g_object_unref(controls.mydata);

    return newid;
}

static void
reset_path(StraightenControls *controls)
{
    init_selection(controls->selection, controls->dfield, controls->args);
}

static void
restore_path(StraightenControls *controls)
{
    gwy_serializable_clone(G_OBJECT(controls->orig_selection),
                           G_OBJECT(controls->selection));
}

static void
reverse_path(StraightenControls *controls)
{
    guint i, n = gwy_selection_get_data(controls->selection, NULL);
    gdouble *xy = g_new(gdouble, 2*n);

    gwy_selection_get_data(controls->selection, xy);
    for (i = 0; i < n/2; i++) {
        GWY_SWAP(gdouble, xy[2*i], xy[2*(n-1 - i)]);
        GWY_SWAP(gdouble, xy[2*i + 1], xy[2*(n-1 - i) + 1]);
    }
    gwy_selection_set_data(controls->selection, n, xy);
    g_free(xy);
}

static void
preview(StraightenControls *controls)
{
    GwyDataField *result, *mask;

    result = gwy_container_get_object_by_name(controls->mydata, "/1/data");
    mask = straighten_do(controls->dfield, result, controls->selection,
                         controls->args, controls->realsquare);
    gwy_data_field_data_changed(result);

    if (mask) {
        gwy_container_set_object_by_name(controls->mydata, "/1/mask", mask);
        g_object_unref(mask);
    }
    else
        gwy_container_remove_by_name(controls->mydata, "/1/mask");

    gwy_set_data_preview_size(GWY_DATA_VIEW(controls->view_result),
                              PREVIEW_SIZE);
}

static GtkWidget*
create_coord_list(StraightenControls *controls)
{
    static const gchar *column_labels[] = { "n", "x", "y" };

    GwyNullStore *store;
    GtkTreeView *treeview;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkWidget *label, *scwin;
    guint i;

    store = gwy_null_store_new(0);
    controls->coordlist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    treeview = GTK_TREE_VIEW(controls->coordlist);
    g_signal_connect(treeview, "key-press-event",
                     G_CALLBACK(delete_selection_object), controls);

    for (i = 0; i < NCOLUMNS; i++) {
        column = gtk_tree_view_column_new();
        gtk_tree_view_column_set_expand(column, TRUE);
        gtk_tree_view_column_set_alignment(column, 0.5);
        g_object_set_data(G_OBJECT(column), "id", GUINT_TO_POINTER(i));
        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "xalign", 1.0, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                           render_coord_cell, controls,
                                           NULL);
        label = gtk_label_new(column_labels[i]);
        gtk_tree_view_column_set_widget(column, label);
        gtk_widget_show(label);
        gtk_tree_view_append_column(treeview, column);
    }

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scwin), controls->coordlist);

    return scwin;
}

static void
render_coord_cell(GtkCellLayout *layout,
                  GtkCellRenderer *renderer,
                  GtkTreeModel *model,
                  GtkTreeIter *iter,
                  gpointer user_data)
{
    StraightenControls *controls = (StraightenControls*)user_data;
    gchar buf[32];
    guint idx, id;
    gint ival;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(layout), "id"));
    gtk_tree_model_get(model, iter, 0, &idx, -1);
    if (id == COLUMN_I)
        ival = idx+1;
    else {
        gdouble xy[2];

        gwy_selection_get_object(controls->selection, idx, xy);
        if (id == COLUMN_X)
            ival = gwy_data_field_rtoj(controls->dfield, xy[0]);
        else
            ival = gwy_data_field_rtoi(controls->dfield, xy[1]);
    }

    g_snprintf(buf, sizeof(buf), "%d", ival);
    g_object_set(renderer, "text", buf, NULL);
}

static gboolean
delete_selection_object(GtkTreeView *treeview,
                        GdkEventKey *event,
                        StraightenControls *controls)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    const gint *indices;

    if (event->keyval != GDK_Delete)
        return FALSE;

    selection = gtk_tree_view_get_selection(treeview);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return FALSE;

    /* Do not permit reduction to a single point. */
    if (gwy_selection_get_data(controls->selection, NULL) < 3)
        return FALSE;

    path = gtk_tree_model_get_path(model, &iter);
    indices = gtk_tree_path_get_indices(path);
    gwy_selection_delete_object(controls->selection, indices[0]);
    gtk_tree_path_free(path);

    return TRUE;
}

static void
init_selection(GwySelection *selection,
               GwyDataField *dfield,
               const StraightenArgs *args)
{
    gdouble xreal = gwy_data_field_get_xreal(dfield);
    gdouble yreal = gwy_data_field_get_yreal(dfield);
    gdouble xy[8];

    if (args->closed) {
        xy[0] = 0.75*xreal;
        xy[1] = xy[5] = 0.5*yreal;
        xy[2] = xy[6] = 0.5*xreal;
        xy[3] = 0.25*yreal;
        xy[4] = 0.25*xreal;
        xy[7] = 0.75*yreal;
        gwy_selection_set_data(selection, 4, xy);
    }
    else {
        xy[0] = xy[2] = xy[4] = 0.5*xreal;
        xy[1] = 0.2*yreal;
        xy[3] = 0.5*yreal;
        xy[5] = 0.8*yreal;
        gwy_selection_set_data(selection, 3, xy);
    }

    g_object_set(selection,
                 "slackness", args->slackness,
                 "closed", args->closed,
                 NULL);
}

static void
path_selection_changed(StraightenControls *controls, gint hint)
{
    GtkTreeView *treeview;
    GtkTreeModel *model;
    GwyNullStore *store;
    gint n;

    treeview = GTK_TREE_VIEW(controls->coordlist);
    model = gtk_tree_view_get_model(treeview);
    store = GWY_NULL_STORE(model);
    n = gwy_null_store_get_n_rows(store);
    g_return_if_fail(hint <= n);

    if (hint < 0) {
        g_object_ref(model);
        gtk_tree_view_set_model(treeview, NULL);
        n = gwy_selection_get_data(controls->selection, NULL);
        gwy_null_store_set_n_rows(store, n);
        gtk_tree_view_set_model(treeview, model);
        g_object_unref(model);
    }
    else {
        GtkTreeSelection *selection;
        GtkTreePath *path;
        GtkTreeIter iter;

        if (hint < n)
            gwy_null_store_row_changed(store, hint);
        else
            gwy_null_store_set_n_rows(store, n+1);

        gtk_tree_model_iter_nth_child(model, &iter, NULL, hint);
        path = gtk_tree_model_get_path(model, &iter);
        selection = gtk_tree_view_get_selection(treeview);
        gtk_tree_selection_select_iter(selection, &iter);
        gtk_tree_view_scroll_to_cell(treeview, path, NULL, FALSE, 0.0, 0.0);
        gtk_tree_path_free(path);
    }
}

static void
interpolation_changed(GtkComboBox *combo, StraightenControls *controls)
{
    StraightenArgs *args = controls->args;
    args->interp = gwy_enum_combo_box_get_active(combo);
}

static void
thickness_changed(StraightenControls *controls, GtkAdjustment *adj)
{
    StraightenArgs *args = controls->args;

    args->thickness = gwy_adjustment_get_int(adj);
    g_object_set(controls->vlayer, "thickness",
                 GWY_ROUND(controls->zoom*args->thickness), NULL);
}

static void
slackness_changed(StraightenControls *controls, GtkAdjustment *adj)
{
    StraightenArgs *args = controls->args;
    gdouble slackness;

    args->slackness = gtk_adjustment_get_value(adj);
    g_object_get(controls->selection, "slackness", &slackness, NULL);
    if (args->slackness != slackness)
        g_object_set(controls->selection, "slackness", args->slackness, NULL);
}

static void
closed_changed(StraightenControls *controls, GtkToggleButton *toggle)
{
    StraightenArgs *args = controls->args;
    gboolean closed;

    args->closed = gtk_toggle_button_get_active(toggle);
    g_object_get(controls->selection, "closed", &closed, NULL);
    if (!args->closed != !closed)
        g_object_set(controls->selection, "closed", args->closed, NULL);
}

/* XXX: This replicates straighten_path.c */
static GwyXY*
rescale_points(GwySelection *selection, GwyDataField *dfield,
               gboolean realsquare,
               gdouble *pdx, gdouble *pdy, gdouble *pqx, gdouble *pqy)
{
    gdouble dx, dy, qx, qy, h;
    GwyXY *points;
    guint n, i;

    dx = gwy_data_field_get_xmeasure(dfield);
    dy = gwy_data_field_get_ymeasure(dfield);
    h = MIN(dx, dy);
    if (realsquare) {
        qx = h/dx;
        qy = h/dy;
        dx = dy = h;
    }
    else
        qx = qy = 1.0;

    n = gwy_selection_get_data(selection, NULL);
    points = g_new(GwyXY, n);
    for (i = 0; i < n; i++) {
        gdouble xy[2];

        gwy_selection_get_object(selection, i, xy);
        points[i].x = xy[0]/dx;
        points[i].y = xy[1]/dy;
    }

    *pdx = dx;
    *pdy = dy;
    *pqx = qx;
    *pqy = qy;
    return points;
}

static GwyDataField*
straighten_do(GwyDataField *dfield, GwyDataField *result,
              GwySelection *selection,
              const StraightenArgs *args, gboolean realsquare)
{
    GwyDataField *mask;
    GwySpline *spline;
    GwyXY *points, *tangents, *coords;
    gdouble dx, dy, qx, qy, h, length;
    guint n, i, j, k, thickness;
    gboolean have_exterior = FALSE;
    gint xres, yres;
    gdouble *m;

    n = gwy_selection_get_data(selection, NULL);
    if (n < 2)
        return NULL;

    points = rescale_points(selection, dfield, realsquare, &dx, &dy, &qx, &qy);
    h = MIN(dx, dy);
    spline = gwy_spline_new_from_points(points, n);
    /* Assume args and selection agree on the parameters... */
    gwy_spline_set_closed(spline, args->closed);
    gwy_spline_set_slackness(spline, args->slackness);
    g_free(points);

    length = gwy_spline_length(spline);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    thickness = args->thickness;

    /* This would give natural sampling for a straight line along some axis. */
    n = GWY_ROUND(length + 1.0);

    gwy_data_field_resample(result, thickness, n, GWY_INTERPOLATION_NONE);
    gwy_data_field_set_xreal(result, h*thickness);
    gwy_data_field_set_yreal(result, h*n);
    gwy_data_field_set_xoffset(result, 0.0);
    gwy_data_field_set_yoffset(result, 0.0);
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_xy(dfield)),
                           G_OBJECT(gwy_data_field_get_si_unit_xy(result)));
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_z(dfield)),
                           G_OBJECT(gwy_data_field_get_si_unit_z(result)));

    mask = gwy_data_field_new_alike(result, TRUE);
    m = gwy_data_field_get_data(mask);

    points = g_new(GwyXY, n);
    tangents = g_new(GwyXY, n);
    coords = g_new(GwyXY, n*thickness);
    gwy_spline_sample_uniformly(spline, points, tangents, n);
    for (i = k = 0; i < n; i++) {
        gdouble xc = qx*points[i].x, yc = qy*points[i].y;
        gdouble vx = qx*tangents[i].y, vy = -qy*tangents[i].x;

        /* If the derivative is zero we just fill the entire row with the
         * same value.  I declare it acceptable. */
        for (j = 0; j < thickness; j++, k++) {
            gdouble x = xc + (j + 0.5 - 0.5*thickness)*vx;
            gdouble y = yc + (j + 0.5 - 0.5*thickness)*vy;

            coords[k].x = x;
            coords[k].y = y;
            if (y > yres || x > xres || y < 0.0 || x < 0.0) {
                m[i*thickness + j] = 1.0;
                have_exterior = TRUE;
            }
        }
    }
    /* Pass mirror because we handle exterior ourselves here and mirror is
     * the least code which simultaneously does not produce undefined pixels
     * where we disagree with the function on which pixels are numerically
     * outside. */
    gwy_data_field_sample_distorted(dfield, result, coords,
                                    args->interp,
                                    GWY_EXTERIOR_MIRROR_EXTEND, 0.0);

    g_free(coords);
    g_free(points);
    g_free(tangents);

    if (have_exterior)
        gwy_data_field_correct_average_unmasked(result, mask);
    else
        gwy_object_unref(mask);

    return mask;
}

static const gchar closed_key[]    = "/module/straighten_path/closed";
static const gchar interp_key[]    = "/module/straighten_path/interp";
static const gchar slackness_key[] = "/module/straighten_path/slackness";
static const gchar thickness_key[] = "/module/straighten_path/thickness";

static void
straighten_sanitize_args(StraightenArgs *args)
{
    /* Upper limit is set based on image dimensions. */
    args->thickness = MAX(args->thickness, 3);
    args->interp = gwy_enum_sanitize_value(args->interp,
                                           GWY_TYPE_INTERPOLATION_TYPE);
    args->slackness = CLAMP(args->slackness, 0.0, G_SQRT2);
    args->closed = !!args->closed;
}

static void
straighten_load_args(GwyContainer *container,
                     StraightenArgs *args)
{
    *args = straighten_defaults;

    gwy_container_gis_int32_by_name(container, thickness_key, &args->thickness);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_double_by_name(container, slackness_key,
                                     &args->slackness);
    gwy_container_gis_boolean_by_name(container, closed_key, &args->closed);

    straighten_sanitize_args(args);
}

static void
straighten_save_args(GwyContainer *container,
                     StraightenArgs *args)
{
    gwy_container_set_int32_by_name(container, thickness_key, args->thickness);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_double_by_name(container, slackness_key, args->slackness);
    gwy_container_set_boolean_by_name(container, closed_key, args->closed);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
