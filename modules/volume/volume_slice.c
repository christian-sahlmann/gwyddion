/*
 *  @(#) $Id$
 *  Copyright (C) 2015-2016 David Necas (Yeti).
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
#include <libprocess/brick.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-volume.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define SLICE_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 360,
};

enum {
    RESPONSE_RESET = 1,
};

enum {
    MAXOBJECTS = 64,
};

enum {
    COLUMN_I, COLUMN_X, COLUMN_Y, COLUMN_Z, NCOLUMNS
};

typedef enum {
    PLANE_XY = 0,
    PLANE_YZ = 1,
    PLANE_ZX = 2,
    PLANE_YX = 3,
    PLANE_ZY = 4,
    PLANE_XZ = 6,
    NPLANES
} SliceBasePlane;

typedef enum {
    OUTPUT_IMAGES = 0,
    OUTPUT_GRAPHS = 1,
    /* We might want to output curves as SPS... */
    NOUTPUTS
} SliceOutputType;

typedef struct {
    gint x;
    gint y;
    gint z;
} SlicePos;

typedef struct {
    SliceBasePlane base_plane;
    SliceOutputType output_type;
    SlicePos currpos;
    gboolean multiselect;
    GwyAppDataId target_graph;
    /* Dynamic state. */
    GwyBrick *brick;
    GArray *allpos;
} SliceArgs;

typedef struct {
    SliceArgs *args;
    GwyContainer *mydata;
    GwyDataField *image;
    GtkWidget *dialog;
    GtkWidget *view;
    GwyPixmapLayer *player;
    GwyVectorLayer *vlayer;
    GtkWidget *graph;
    GtkWidget *base_plane;
    GSList *output_type;
    GtkWidget *target_graph;
    GtkWidget *target_graph_label;
    GtkWidget *multiselect;
    GtkObject *xpos;
    GtkObject *ypos;
    GtkObject *zpos;
    GwySIValueFormat *xvf;
    GwySIValueFormat *yvf;
    GwySIValueFormat *zvf;
    GtkWidget *xposreal;
    GtkWidget *yposreal;
    GtkWidget *zposreal;
    GtkWidget *scwin;
    GwyNullStore *store;
    GtkWidget *coordlist;
    gboolean in_update;
    gint current_object;
} SliceControls;

static gboolean module_register        (void);
static void     slice                  (GwyContainer *data,
                                        GwyRunType run);
static gboolean slice_dialog           (SliceArgs *args,
                                        GwyContainer *data,
                                        gint id);
static void     slice_do               (SliceArgs *args,
                                        GwyContainer *data,
                                        gint id);
static void     extract_one_image      (SliceArgs *args,
                                        GwyContainer *data,
                                        gint id,
                                        gint idx);
static void     create_coordlist       (SliceControls *controls);
static void     slice_reset            (SliceControls *controls);
static void     point_selection_changed(SliceControls *controls,
                                        gint id,
                                        GwySelection *selection);
static void     plane_selection_changed(SliceControls *controls,
                                        gint id,
                                        GwySelection *selection);
static void     xpos_changed           (SliceControls *controls,
                                        GtkAdjustment *adj);
static void     ypos_changed           (SliceControls *controls,
                                        GtkAdjustment *adj);
static void     zpos_changed           (SliceControls *controls,
                                        GtkAdjustment *adj);
static void     base_plane_changed     (GtkComboBox *combo,
                                        SliceControls *controls);
static void     output_type_changed    (GtkWidget *button,
                                        SliceControls *controls);
static void update_sensitivity(SliceControls *controls);
static void     multiselect_changed    (SliceControls *controls,
                                        GtkToggleButton *button);
static void     reduce_selection       (SliceControls *controls);
static void     update_position        (SliceControls *controls,
                                        const SlicePos *pos);
static void     update_multiselection  (SliceControls *controls);
static void     update_labels          (SliceControls *controls);
static void     update_target_graphs   (SliceControls *controls);
static gboolean filter_target_graphs   (GwyContainer *data,
                                        gint id,
                                        gpointer user_data);
static void     target_graph_changed   (SliceControls *controls);
static void     extract_image_plane    (const SliceArgs *args,
                                        GwyDataField *dfield);
static void     extract_graph_curve    (const SliceArgs *args,
                                        GwyGraphCurveModel *gcmodel,
                                        gint idx,
                                        gboolean use_calibration);
static void     extract_gmodel         (const SliceArgs *args,
                                        GwyGraphModel *gmodel);
static void     flip_xy                (GwyDataField *dfield);
static void     slice_sanitize_args    (SliceArgs *args);
static void     slice_load_args        (GwyContainer *container,
                                        SliceArgs *args);
static void     slice_save_args        (GwyContainer *container,
                                        SliceArgs *args);

static const SlicePos nullpos = { -1, -1, -1 };

static const SliceArgs slice_defaults = {
    PLANE_XY, OUTPUT_IMAGES,
    { -1, -1, -1 },
    FALSE,
    GWY_APP_DATA_ID_NONE,
    /* Dynamic state. */
    NULL, NULL,
};

static GwyAppDataId target_graph_id = GWY_APP_DATA_ID_NONE;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Extracts image planes and line graphs from volume data."),
    "Yeti <yeti@gwyddion.net>",
    "2.2",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_volume_func_register("volume_slice",
                             (GwyVolumeFunc)&slice,
                             N_("/Cut and _Slice..."),
                             NULL,
                             SLICE_RUN_MODES,
                             GWY_MENU_FLAG_VOLUME,
                             N_("Extract image planes and line graphs"));

    return TRUE;
}

static void
slice(GwyContainer *data, GwyRunType run)
{
    SliceArgs args;
    GwyBrick *brick = NULL;
    gint id;

    g_return_if_fail(run & SLICE_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerPoint"));

    slice_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_BRICK, &brick,
                                     GWY_APP_BRICK_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_BRICK(brick));
    args.brick = brick;

    if (CLAMP(args.currpos.x, 0, brick->xres-1) != args.currpos.x)
        args.currpos.x = brick->xres/2;
    if (CLAMP(args.currpos.y, 0, brick->yres-1) != args.currpos.y)
        args.currpos.y = brick->yres/2;
    if (CLAMP(args.currpos.z, 0, brick->zres-1) != args.currpos.z)
        args.currpos.z = brick->zres/2;

    args.allpos = g_array_new(FALSE, FALSE, sizeof(SlicePos));
    g_array_append_val(args.allpos, args.currpos);

    if (slice_dialog(&args, data, id))
        slice_do(&args, data, id);

    slice_save_args(gwy_app_settings_get(), &args);
    g_array_free(args.allpos, TRUE);
}

static gboolean
slice_dialog(SliceArgs *args, GwyContainer *data, gint id)
{
    static const GwyEnum base_planes[] = {
        { "XY", PLANE_XY, },
        { "YZ", PLANE_YZ, },
        { "ZX", PLANE_ZX, },
        { "YX", PLANE_YX, },
        { "ZY", PLANE_ZY, },
        { "XZ", PLANE_XZ, },
    };

    static const GwyEnum output_types[] = {
        { N_("Image slice"), OUTPUT_IMAGES, },
        { N_("Line graph"),  OUTPUT_GRAPHS, },
    };

    GtkWidget *dialog, *table, *hbox, *label, *area;
    GwyDataChooser *chooser;
    SliceControls controls;
    GwyBrick *brick = args->brick;
    GwyDataField *dfield;
    GwyDataLine *calibration;
    GwyGraphCurveModel *gcmodel;
    GwyGraphModel *gmodel;
    gint response, row;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer = NULL;
    GwySelection *selection;
    SlicePos pos;
    const guchar *gradient;
    gchar key[40];

    controls.args = args;
    controls.in_update = TRUE;
    controls.current_object = 0;

    dialog = gtk_dialog_new_with_buttons(_("Slice Volume Data"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_volume_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    controls.image = dfield = gwy_data_field_new(1, 1, 1.0, 1.0, TRUE);
    extract_image_plane(args, dfield);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    g_object_unref(dfield);

    g_snprintf(key, sizeof(key), "/brick/%d/preview/palette", id);
    if (gwy_container_gis_string_by_name(data, key, &gradient)) {
        gwy_container_set_const_string_by_name(controls.mydata,
                                               "/0/base/palette", gradient);
    }

    controls.view = gwy_data_view_new(controls.mydata);
    controls.player = layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 0);

    controls.vlayer = vlayer = g_object_new(g_type_from_name("GwyLayerPoint"),
                                            NULL);
    gwy_vector_layer_set_selection_key(vlayer, "/0/select/pointer");
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view), vlayer);
    selection = gwy_vector_layer_ensure_selection(vlayer);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(point_selection_changed), &controls);

    gmodel = gwy_graph_model_new();
    g_object_set(gmodel, "label-visible", FALSE, NULL);
    gcmodel = gwy_graph_curve_model_new();
    gwy_graph_model_add_curve(gmodel, gcmodel);
    g_object_unref(gcmodel);

    controls.graph = gwy_graph_new(gmodel);
    gwy_graph_enable_user_input(GWY_GRAPH(controls.graph), FALSE);
    g_object_unref(gmodel);
    gtk_widget_set_size_request(controls.graph, PREVIEW_SIZE, PREVIEW_SIZE);
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, TRUE, TRUE, 0);

    area = gwy_graph_get_area(GWY_GRAPH(controls.graph));
    gwy_graph_area_set_status(GWY_GRAPH_AREA(area), GWY_GRAPH_STATUS_XLINES);
    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                             GWY_GRAPH_STATUS_XLINES);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(plane_selection_changed), &controls);

    hbox = gtk_hbox_new(FALSE, 24);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 4);

    table = gtk_table_new(7, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gwy_label_new_header(_("Output Options"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Base plane:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    controls.base_plane
        = gwy_enum_combo_box_new(base_planes, G_N_ELEMENTS(base_planes),
                                 G_CALLBACK(base_plane_changed), &controls,
                                 args->base_plane, TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.base_plane,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.base_plane);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new(_("Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.output_type
        = gwy_radio_buttons_create(output_types, G_N_ELEMENTS(output_types),
                                   G_CALLBACK(output_type_changed), &controls,
                                   args->output_type);
    row = gwy_radio_buttons_attach_to_table(controls.output_type,
                                            GTK_TABLE(table), 2, row);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new_with_mnemonic(_("Target _graph:"));
    controls.target_graph_label = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    controls.target_graph = gwy_data_chooser_new_graphs();
    chooser = GWY_DATA_CHOOSER(controls.target_graph);
    gwy_data_chooser_set_none(chooser, _("New graph"));
    gwy_data_chooser_set_active(chooser, NULL, -1);
    gwy_data_chooser_set_filter(chooser, filter_target_graphs, &controls, NULL);
    gwy_data_chooser_set_active_id(chooser, &args->target_graph);
    gwy_data_chooser_get_active_id(chooser, &args->target_graph);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.target_graph);
    gtk_table_attach(GTK_TABLE(table), controls.target_graph,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.target_graph, "changed",
                             G_CALLBACK(target_graph_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    controls.multiselect
        = gtk_check_button_new_with_mnemonic(_("Extract _multiple items"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.multiselect),
                                 args->multiselect);
    gtk_table_attach(GTK_TABLE(table), controls.multiselect,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.multiselect, "toggled",
                             G_CALLBACK(multiselect_changed), &controls);
    row++;

    table = gtk_table_new(4, 5, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_table_set_col_spacing(GTK_TABLE(table), 2, 12);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gwy_label_new_header(_("Positions"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.xpos = gtk_adjustment_new(args->currpos.x, 0.0, brick->xres-1.0,
                                       1.0, 10.0, 0);
    gwy_table_attach_spinbutton(table, row, _("_X:"), "px", controls.xpos);
    g_signal_connect_swapped(controls.xpos, "value-changed",
                             G_CALLBACK(xpos_changed), &controls);

    controls.xposreal = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.xposreal), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.xposreal,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    controls.xvf = gwy_brick_get_value_format_x(brick,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                NULL);
    label = gtk_label_new(controls.xvf->units);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     4, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.ypos = gtk_adjustment_new(args->currpos.y, 0.0, brick->yres-1.0,
                                       1.0, 10.0, 0);
    gwy_table_attach_spinbutton(table, row, _("_Y:"), "px", controls.ypos);
    g_signal_connect_swapped(controls.ypos, "value-changed",
                             G_CALLBACK(ypos_changed), &controls);

    controls.yposreal = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.yposreal), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.yposreal,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    controls.yvf = gwy_brick_get_value_format_y(brick,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                NULL);
    label = gtk_label_new(controls.yvf->units);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     4, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.zpos = gtk_adjustment_new(args->currpos.z, 0.0, brick->zres-1.0,
                                       1.0, 10.0, 0);
    gwy_table_attach_spinbutton(table, row, _("_Z:"), "px", controls.zpos);
    g_signal_connect_swapped(controls.zpos, "value-changed",
                             G_CALLBACK(zpos_changed), &controls);

    controls.zposreal = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.zposreal), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.zposreal,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    if ((calibration = gwy_brick_get_zcalibration(brick))) {
        controls.zvf = gwy_data_line_get_value_format_y(calibration,
                                                        GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                        NULL);
    }
    else {
        controls.zvf = gwy_brick_get_value_format_z(brick,
                                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                    NULL);
    }
    label = gtk_label_new(controls.zvf->units);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     4, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    create_coordlist(&controls);
    gtk_box_pack_start(GTK_BOX(hbox), controls.scwin, FALSE, TRUE, 0);

    label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    pos = args->currpos;
    args->currpos = nullpos;
    update_sensitivity(&controls);
    update_position(&controls, &pos);
    controls.in_update = FALSE;
    multiselect_changed(&controls, GTK_TOGGLE_BUTTON(controls.multiselect));

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            gwy_si_unit_value_format_free(controls.xvf);
            gwy_si_unit_value_format_free(controls.yvf);
            gwy_si_unit_value_format_free(controls.zvf);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            slice_reset(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    g_object_unref(controls.mydata);
    gwy_si_unit_value_format_free(controls.xvf);
    gwy_si_unit_value_format_free(controls.yvf);
    gwy_si_unit_value_format_free(controls.zvf);

    return TRUE;
}

static void
render_coord_cell(GtkCellLayout *layout,
                  GtkCellRenderer *renderer,
                  GtkTreeModel *model,
                  GtkTreeIter *iter,
                  gpointer user_data)
{
    SliceControls *controls = (SliceControls*)user_data;
    SlicePos *pos;
    gchar buf[32];
    guint idx, id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(layout), "id"));
    gtk_tree_model_get(model, iter, 0, &idx, -1);

    if (id == COLUMN_I)
        g_snprintf(buf, sizeof(buf), "%d", idx + 1);
    else {
        g_return_if_fail(idx < controls->args->allpos->len);
        pos = &g_array_index(controls->args->allpos, SlicePos, idx);
        if (id == COLUMN_X)
            g_snprintf(buf, sizeof(buf), "%d", pos->x);
        else if (id == COLUMN_Y)
            g_snprintf(buf, sizeof(buf), "%d", pos->y);
        else if (id == COLUMN_Z)
            g_snprintf(buf, sizeof(buf), "%d", pos->z);
    }

    g_object_set(renderer, "text", buf, NULL);
}

static void
create_coordlist(SliceControls *controls)
{
    static const gchar *titles[NCOLUMNS] = {
        "n", "x", "y", "z",
    };
    GtkTreeModel *model;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GtkWidget *label;
    GString *str;
    guint i;

    controls->store = gwy_null_store_new(1);
    model = GTK_TREE_MODEL(controls->store);
    controls->coordlist = gtk_tree_view_new_with_model(model);

    controls->scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(controls->scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(controls->scwin), controls->coordlist);

    str = g_string_new(NULL);
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
        label = gtk_label_new(NULL);
        gtk_tree_view_column_set_widget(column, label);
        gtk_widget_show(label);
        gtk_tree_view_append_column(GTK_TREE_VIEW(controls->coordlist), column);

        label = gtk_tree_view_column_get_widget(column);
        g_string_assign(str, "<b>");
        g_string_append(str, titles[i]);
        g_string_append(str, "</b>");
        gtk_label_set_markup(GTK_LABEL(label), str->str);
    }
    g_string_free(str, TRUE);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->coordlist));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
}

static void
slice_reset(SliceControls *controls)
{
    SliceArgs *args = controls->args;
    GwyBrick *brick = args->brick;

    args->currpos.x = brick->xres/2;
    args->currpos.y = brick->yres/2;
    args->currpos.z = brick->zres/2;
    reduce_selection(controls);
}

static void
point_selection_changed(SliceControls *controls,
                        gint id,
                        GwySelection *selection)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;
    SlicePos pos = args->currpos;
    gdouble xy[2];
    gint i, j;

    gwy_debug("%d (%d)", controls->in_update, id);
    if (controls->in_update)
        return;

    /* What should we do here?  Hope we always get another update with a
     * specific id afterwards. */
    if (id < 0)
        return;

    if (!gwy_selection_get_object(selection, id, xy))
        return;

    if (controls->args->output_type == OUTPUT_GRAPHS)
        controls->current_object = id;

    j = CLAMP(gwy_data_field_rtoj(controls->image, xy[0]),
              0, controls->image->xres-1);
    i = CLAMP(gwy_data_field_rtoi(controls->image, xy[1]),
              0, controls->image->yres-1);

    if (base_plane == PLANE_XY || base_plane == PLANE_XZ)
        pos.x = j;
    if (base_plane == PLANE_YZ || base_plane == PLANE_YX)
        pos.y = j;
    if (base_plane == PLANE_ZX || base_plane == PLANE_ZY)
        pos.z = j;

    if (base_plane == PLANE_YX || base_plane == PLANE_ZX)
        pos.x = i;
    if (base_plane == PLANE_ZY || base_plane == PLANE_XY)
        pos.y = i;
    if (base_plane == PLANE_XZ || base_plane == PLANE_YZ)
        pos.z = i;

    controls->in_update = TRUE;
    update_position(controls, &pos);
    controls->in_update = FALSE;
}

static void
plane_selection_changed(SliceControls *controls,
                        gint id,
                        GwySelection *selection)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;
    SlicePos pos = args->currpos;
    GwyBrick *brick = args->brick;
    gdouble r;

    gwy_debug("%d (%d)", controls->in_update, id);
    if (controls->in_update)
        return;

    /* What should we do here?  Hope we always get another update with a
     * specific id afterwards. */
    if (id < 0)
        return;

    if (!gwy_selection_get_object(selection, id, &r))
        return;

    if (controls->args->output_type == OUTPUT_IMAGES)
        controls->current_object = id;

    if (base_plane == PLANE_YZ || base_plane == PLANE_ZY)
        pos.x = CLAMP(gwy_brick_rtoi(brick, r - brick->xoff), 0, brick->xres-1);
    else if (base_plane == PLANE_YX || base_plane == PLANE_XY)
        pos.z = CLAMP(gwy_brick_rtok(brick, r - brick->zoff), 0, brick->zres-1);
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX)
        pos.y = CLAMP(gwy_brick_rtoj(brick, r - brick->yoff), 0, brick->yres-1);
    else {
        g_return_if_reached();
    }

    controls->in_update = TRUE;
    update_position(controls, &pos);
    controls->in_update = FALSE;
}

static void
xpos_changed(SliceControls *controls, GtkAdjustment *adj)
{
    SlicePos pos = controls->args->currpos;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    pos.x = gwy_adjustment_get_int(adj);
    update_position(controls, &pos);
    controls->in_update = FALSE;
}

static void
ypos_changed(SliceControls *controls, GtkAdjustment *adj)
{
    SlicePos pos = controls->args->currpos;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    pos.y = gwy_adjustment_get_int(adj);
    update_position(controls, &pos);
    controls->in_update = FALSE;
}

static void
zpos_changed(SliceControls *controls, GtkAdjustment *adj)
{
    SlicePos pos = controls->args->currpos;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    pos.z = gwy_adjustment_get_int(adj);
    update_position(controls, &pos);
    controls->in_update = FALSE;
}

static void
base_plane_changed(GtkComboBox *combo, SliceControls *controls)
{
    SliceArgs *args = controls->args;
    SlicePos pos;

    g_assert(!controls->in_update);

    reduce_selection(controls);
    pos = args->currpos;

    args->base_plane = gwy_enum_combo_box_get_active(combo);
    controls->in_update = TRUE;
    args->currpos = nullpos;
    update_position(controls, &pos);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls->view), PREVIEW_SIZE);
    update_target_graphs(controls);
    controls->in_update = FALSE;
}

static void
output_type_changed(GtkWidget *button, SliceControls *controls)
{
    controls->args->output_type = gwy_radio_button_get_value(button);
    /* In multiselection mode it ensures the non-multiple coordinates are
     * compacted to single one. */
    reduce_selection(controls);
    multiselect_changed(controls, GTK_TOGGLE_BUTTON(controls->multiselect));
    update_sensitivity(controls);
}

static void
update_sensitivity(SliceControls *controls)
{
    gboolean sens = (controls->args->output_type == OUTPUT_GRAPHS);
    gtk_widget_set_sensitive(controls->target_graph, sens);
    gtk_widget_set_sensitive(controls->target_graph_label, sens);
}

static void
multiselect_changed(SliceControls *controls, GtkToggleButton *button)
{
    SliceArgs *args = controls->args;
    GtkWidget *area;
    GwySelection *selection;

    args->multiselect = gtk_toggle_button_get_active(button);
    gtk_widget_set_no_show_all(controls->coordlist, !args->multiselect);
    if (args->multiselect)
        gtk_widget_show_all(controls->coordlist);
    else
        gtk_widget_hide(controls->coordlist);

    area = gwy_graph_get_area(GWY_GRAPH(controls->graph));

    if (args->multiselect) {
        selection = gwy_vector_layer_ensure_selection(controls->vlayer);
        gwy_selection_set_max_objects(selection,
                                      args->output_type == OUTPUT_GRAPHS
                                      ? MAXOBJECTS
                                      : 1);

        selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                                 GWY_GRAPH_STATUS_XLINES);
        gwy_selection_set_max_objects(selection,
                                      args->output_type == OUTPUT_IMAGES
                                      ? MAXOBJECTS
                                      : 1);

        return;
    }

    reduce_selection(controls);
    selection = gwy_vector_layer_ensure_selection(controls->vlayer);
    gwy_selection_set_max_objects(selection, 1);
    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                             GWY_GRAPH_STATUS_XLINES);
    gwy_selection_set_max_objects(selection, 1);
}

static void
reduce_selection(SliceControls *controls)
{
    SlicePos pos = controls->args->currpos;
    GwySelection *selection;
    GtkWidget *area;
    gdouble xyz[2] = { 0.0, 0.0 };

    g_assert(!controls->in_update);

    controls->current_object = 0;
    gwy_null_store_set_n_rows(controls->store, 1);
    g_array_set_size(controls->args->allpos, 1);

    controls->in_update = TRUE;
    selection = gwy_vector_layer_ensure_selection(controls->vlayer);
    gwy_selection_set_data(selection, 1, xyz);

    area = gwy_graph_get_area(GWY_GRAPH(controls->graph));
    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                             GWY_GRAPH_STATUS_XLINES);
    gwy_selection_set_data(selection, 1, xyz);

    controls->args->currpos = nullpos;
    update_position(controls, &pos);
    controls->in_update = FALSE;
}

/*
 * All signal handlers must
 * - do nothing in update
 * - calculate the integer coordinate
 * - enter in-update
 * - call this function
 * - leave in-update
 * This way there are no circular dependencies, we always completely update
 * anything that has changed here.
 */
static void
update_position(SliceControls *controls,
                const SlicePos *pos)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GtkWidget *area;
    GwySelection *selection;
    GwyBrick *brick = args->brick;
    gdouble xy[2], z;
    gboolean plane_changed = FALSE, point_changed = FALSE;
    gint id;

    g_assert(controls->in_update);

    if (base_plane == PLANE_XY || base_plane == PLANE_YX) {
        xy[0] = gwy_brick_itor(brick, pos->x);
        xy[1] = gwy_brick_jtor(brick, pos->y);
        if (base_plane != PLANE_XY)
            GWY_SWAP(gdouble, xy[0], xy[1]);
        z = gwy_brick_ktor(brick, pos->z) + brick->zoff;
        point_changed = (pos->x != args->currpos.x
                         || pos->y != args->currpos.y);
        plane_changed = (pos->z != args->currpos.z);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        xy[0] = gwy_brick_itor(brick, pos->x);
        xy[1] = gwy_brick_ktor(brick, pos->z);
        if (base_plane != PLANE_XZ)
            GWY_SWAP(gdouble, xy[0], xy[1]);
        z = gwy_brick_jtor(brick, pos->y) + brick->yoff;
        point_changed = (pos->x != args->currpos.x
                         || pos->z != args->currpos.z);
        plane_changed = (pos->y != args->currpos.y);
    }
    else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        xy[0] = gwy_brick_jtor(brick, pos->y);
        xy[1] = gwy_brick_ktor(brick, pos->z);
        if (base_plane != PLANE_YZ)
            GWY_SWAP(gdouble, xy[0], xy[1]);
        z = gwy_brick_itor(brick, pos->x) + brick->xoff;
        point_changed = (pos->y != args->currpos.y
                         || pos->z != args->currpos.z);
        plane_changed = (pos->x != args->currpos.x);
    }
    else {
        g_return_if_reached();
    }

    args->currpos = *pos;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xpos), pos->x);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ypos), pos->y);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zpos), pos->z);

    update_labels(controls);
    update_multiselection(controls);

    if (point_changed) {
        id = (args->output_type == OUTPUT_GRAPHS
              ? controls->current_object
              : 0);

        selection = gwy_vector_layer_ensure_selection(controls->vlayer);
        gwy_selection_set_object(selection, id, xy);

        gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
        extract_gmodel(args, gmodel);

        gcmodel = gwy_graph_model_get_curve(gmodel, 0);
        extract_graph_curve(args, gcmodel, controls->current_object, FALSE);
    }

    if (plane_changed) {
        id = (args->output_type == OUTPUT_IMAGES
              ? controls->current_object
              : 0);

        area = gwy_graph_get_area(GWY_GRAPH(controls->graph));
        selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                                 GWY_GRAPH_STATUS_XLINES);
        gwy_selection_set_object(selection, id, &z);

        extract_image_plane(args, controls->image);
        gwy_data_field_data_changed(controls->image);
    }
}

static void
update_multiselection(SliceControls *controls)
{
    GtkTreeSelection *selection;
    SliceArgs *args = controls->args;
    gint curr = controls->current_object;
    gint len = args->allpos->len;
    GtkTreeIter iter;
    GtkTreePath *path;

    gwy_debug("len: %d, curr: %d", len, curr);
    if (len == curr) {
        g_array_append_val(args->allpos, args->currpos);
        gwy_null_store_set_n_rows(controls->store, curr+1);
    }
    else if (len > controls->current_object) {
        g_array_index(args->allpos, SlicePos, curr) = args->currpos;
        gwy_null_store_row_changed(controls->store, curr);
    }
    else {
        g_assert_not_reached();
    }

    if (!args->multiselect)
        return;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->coordlist));
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(controls->store), &iter,
                                  NULL, controls->current_object);
    gtk_tree_selection_select_iter(selection, &iter);
    path = gtk_tree_model_get_path(GTK_TREE_MODEL(controls->store), &iter);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(controls->coordlist), path,
                                 NULL, FALSE, 0.0, 0.0);

    gtk_tree_path_free(path);
}

static void
update_labels(SliceControls *controls)
{
    SliceArgs *args = controls->args;
    GwyBrick *brick = args->brick;
    GwyDataLine *calibration;
    gdouble x, y, z;
    gchar buf[64];

    x = gwy_brick_itor(brick, args->currpos.x);
    g_snprintf(buf, sizeof(buf), "%.*f",
               controls->xvf->precision, x/controls->xvf->magnitude);
    gtk_label_set_markup(GTK_LABEL(controls->xposreal), buf);

    y = gwy_brick_jtor(brick, args->currpos.y);
    g_snprintf(buf, sizeof(buf), "%.*f",
               controls->xvf->precision, y/controls->yvf->magnitude);
    gtk_label_set_markup(GTK_LABEL(controls->yposreal), buf);

    z = gwy_brick_ktor(brick, args->currpos.z);
    if ((calibration = gwy_brick_get_zcalibration(brick)))
        z = gwy_data_line_get_val(calibration, args->currpos.z);
    g_snprintf(buf, sizeof(buf), "%.*f",
               controls->zvf->precision, z/controls->zvf->magnitude);
    gtk_label_set_markup(GTK_LABEL(controls->zposreal), buf);
}

static void
update_target_graphs(SliceControls *controls)
{
    GwyDataChooser *chooser = GWY_DATA_CHOOSER(controls->target_graph);
    gwy_data_chooser_refilter(chooser);
}

static gboolean
filter_target_graphs(GwyContainer *data, gint id, gpointer user_data)
{
    SliceControls *controls = (SliceControls*)user_data;
    GwyGraphModel *gmodel, *targetgmodel;
    GQuark quark = gwy_app_get_graph_key_for_id(id);

    gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), FALSE);
    return (gwy_container_gis_object(data, quark, (GObject**)&targetgmodel)
            && gwy_graph_model_units_are_compatible(gmodel, targetgmodel));
}

static void
target_graph_changed(SliceControls *controls)
{
    GwyDataChooser *chooser = GWY_DATA_CHOOSER(controls->target_graph);
    gwy_data_chooser_get_active_id(chooser, &controls->args->target_graph);
}

static void
slice_do(SliceArgs *args, GwyContainer *data, gint id)
{
    guint idx;

    if (args->output_type == OUTPUT_IMAGES) {
        for (idx = 0; idx < args->allpos->len; idx++)
            extract_one_image(args, data, id, idx);
    }
    else if (args->output_type == OUTPUT_GRAPHS) {
        GwyGraphModel *gmodel = gwy_graph_model_new();

        extract_gmodel(args, gmodel);
        for (idx = 0; idx < args->allpos->len; idx++) {
            GwyGraphCurveModel *gcmodel = gwy_graph_curve_model_new();

            extract_graph_curve(args, gcmodel, idx, TRUE);
            g_object_set(gcmodel,
                         "color", gwy_graph_get_preset_color(idx),
                         NULL);
            gwy_graph_model_add_curve(gmodel, gcmodel);
            g_object_unref(gcmodel);
        }

        gwy_app_add_graph_or_curves(gmodel, data, &args->target_graph, 1);
        g_object_unref(gmodel);
    }
}

static void
extract_one_image(SliceArgs *args, GwyContainer *data, gint id, gint idx)
{
    GwyDataField *dfield = gwy_data_field_new(1, 1, 1.0, 1.0, TRUE);
    SliceBasePlane base_plane = args->base_plane;
    GwyBrick *brick = args->brick;
    GwyDataLine *calibration;
    GwySIValueFormat *vf;
    const guchar *gradient;
    SlicePos *pos;
    gchar *title = NULL;
    gchar key[40];
    gdouble r;
    gint i, newid;

    pos = &g_array_index(args->allpos, SlicePos, idx);
    extract_image_plane(args, dfield);
    newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
    g_object_unref(dfield);

    gwy_app_channel_log_add(data, -1, newid, "volume::volume_slice", NULL);

    g_snprintf(key, sizeof(key), "/brick/%d/preview/palette", id);
    if (gwy_container_gis_string_by_name(data, key, &gradient)) {
        g_snprintf(key, sizeof(key), "/%d/base/palette", newid);
        gwy_container_set_const_string_by_name(data, key, gradient);
    }

    if (base_plane == PLANE_XY || base_plane == PLANE_YX) {
        i = pos->z;
        if ((calibration = gwy_brick_get_zcalibration(brick))) {
            r = gwy_data_line_get_val(calibration, i);
            vf = gwy_data_line_get_value_format_y(calibration,
                                                  GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                  NULL);
        }
        else {
            r = gwy_brick_ktor(brick, i);
            vf = gwy_brick_get_value_format_z(brick,
                                              GWY_SI_UNIT_FORMAT_VFMARKUP,
                                              NULL);
        }
        title = g_strdup_printf(_("Z slice at %.*f%s%s (#%d)"),
                                vf->precision, r/vf->magnitude,
                                strlen(vf->units) ? " " : "", vf->units,
                                i);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        i = pos->y;
        r = gwy_brick_jtor(brick, i);
        vf = gwy_brick_get_value_format_y(brick,
                                          GWY_SI_UNIT_FORMAT_VFMARKUP,
                                          NULL);
        title = g_strdup_printf(_("Y slice at %.*f%s%s (#%d)"),
                                vf->precision, r/vf->magnitude,
                                strlen(vf->units) ? " " : "", vf->units,
                                i);
    }
    else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        i = pos->x;
        r = gwy_brick_itor(brick, i);
        vf = gwy_brick_get_value_format_x(brick,
                                          GWY_SI_UNIT_FORMAT_VFMARKUP,
                                          NULL);
        title = g_strdup_printf(_("X slice at %.*f%s%s (#%d)"),
                                vf->precision, r/vf->magnitude,
                                strlen(vf->units) ? " " : "", vf->units,
                                i);
    }
    else {
        g_return_if_reached();
    }
    gwy_si_unit_value_format_free(vf);
    g_snprintf(key, sizeof(key), "/%d/data/title", newid);
    gwy_container_set_string_by_name(data, key, (const guchar*)title);
}

static void
extract_image_plane(const SliceArgs *args, GwyDataField *dfield)
{
    SliceBasePlane base_plane = args->base_plane;
    GwyBrick *brick = args->brick;
    gboolean do_flip = FALSE;

    if (base_plane == PLANE_XY || base_plane == PLANE_YX) {
        do_flip = (base_plane == PLANE_YX);
        gwy_brick_extract_plane(args->brick, dfield,
                                0, 0, args->currpos.z,
                                brick->xres, brick->yres, -1,
                                FALSE);
    }
    else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        do_flip = (base_plane == PLANE_ZY);
        gwy_brick_extract_plane(args->brick, dfield,
                                args->currpos.x, 0, 0,
                                -1, brick->yres, brick->zres,
                                FALSE);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        do_flip = (base_plane == PLANE_ZX);
        gwy_brick_extract_plane(args->brick, dfield,
                                0, args->currpos.y, 0,
                                brick->xres, -1, brick->zres,
                                FALSE);
    }

    if (do_flip)
        flip_xy(dfield);
}

static void
extract_graph_curve(const SliceArgs *args,
                    GwyGraphCurveModel *gcmodel,
                    gint idx,
                    gboolean use_calibration)
{
    SliceBasePlane base_plane = args->base_plane;
    GwyDataLine *line = gwy_data_line_new(1, 1.0, FALSE);
    GwyDataLine *calibration = NULL;
    GwyBrick *brick = args->brick;
    SlicePos *pos;
    gchar desc[80];

    pos = &g_array_index(args->allpos, SlicePos, idx);
    gwy_debug("%d (%u)", idx, (guint)args->allpos->len);
    gwy_debug("(%d, %d, %d)", pos->x, pos->y, pos->z);
    if (base_plane == PLANE_XY || base_plane == PLANE_YX) {
        gwy_brick_extract_line(brick, line,
                               pos->x, pos->y, 0,
                               pos->x, pos->y, brick->zres,
                               FALSE);
        gwy_data_line_set_offset(line, brick->zoff);
        /* Try to use the calibration.  Ignore if the dimension does not seem
         * right. */
        calibration = gwy_brick_get_zcalibration(brick);
        if (!use_calibration
            || !calibration
            || (gwy_data_line_get_res(line)
                != gwy_data_line_get_res(calibration)))
            calibration = NULL;

        g_snprintf(desc, sizeof(desc), _("Z graph at x: %d y: %d"),
                   pos->x, pos->y);
    }
    else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        gwy_brick_extract_line(brick, line,
                               0, pos->y, pos->z,
                               brick->xres-1, pos->y, pos->z,
                               FALSE);
        gwy_data_line_set_offset(line, brick->xoff);
        g_snprintf(desc, sizeof(desc), _("X graph at y: %d z: %d"),
                   pos->y, pos->z);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        gwy_brick_extract_line(brick, line,
                               pos->x, 0, pos->z,
                               pos->x, brick->yres-1, pos->z,
                               FALSE);
        gwy_data_line_set_offset(line, brick->yoff);
        g_snprintf(desc, sizeof(desc), _("Y graph at x: %d z: %d"),
                   pos->x, pos->z);
    }
    else {
        g_return_if_reached();
    }

    g_object_set(gcmodel,
                 "description", desc,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 NULL);

    if (calibration) {
        gwy_graph_curve_model_set_data(gcmodel,
                                       gwy_data_line_get_data(calibration),
                                       gwy_data_line_get_data(line),
                                       gwy_data_line_get_res(line));
    }
    else
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, line, 0, 0);

    g_object_unref(line);
}

static void
extract_gmodel(const SliceArgs *args, GwyGraphModel *gmodel)
{
    SliceBasePlane base_plane = args->base_plane;
    GwyBrick *brick = args->brick;
    GwyDataLine *calibration = NULL;
    const gchar *xlabel, *ylabel, *gtitle;
    GwySIUnit *xunit = NULL, *yunit;

    if (base_plane == PLANE_XY || base_plane == PLANE_YX) {
        gtitle = _("Volume Z graphs");
        xlabel = "x";
        ylabel = "y";
        if (base_plane == PLANE_YX)
            GWY_SWAP(const gchar*, xlabel, ylabel);
    }
    else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        gtitle = _("Volume X graphs");
        xlabel = "y";
        ylabel = "z";
        if (base_plane == PLANE_ZY)
            GWY_SWAP(const gchar*, xlabel, ylabel);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        gtitle = _("Volume Y graphs");
        xlabel = "x";
        ylabel = "z";
        if (base_plane == PLANE_ZX)
            GWY_SWAP(const gchar*, xlabel, ylabel);
    }
    else {
        g_return_if_reached();
    }

    calibration = gwy_brick_get_zcalibration(brick);
    if (base_plane == PLANE_XY || base_plane == PLANE_YX) {
        if (calibration)
            xunit = gwy_data_line_get_si_unit_x(calibration);
        else
            xunit = gwy_brick_get_si_unit_z(brick);
    }
    else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY)
        xunit = gwy_brick_get_si_unit_x(brick);
    else if (base_plane == PLANE_ZX || base_plane == PLANE_XZ)
        xunit = gwy_brick_get_si_unit_y(brick);
    xunit = gwy_si_unit_duplicate(xunit);
    yunit = gwy_si_unit_duplicate(gwy_brick_get_si_unit_w(brick));

    g_object_set(gmodel,
                 "title", gtitle,
                 "si-unit-x", xunit,
                 "si-unit-y", yunit,
                 "axis-label-bottom", xlabel,
                 "axis-label-left", ylabel,
                 NULL);
    g_object_unref(xunit);
    g_object_unref(yunit);
}

static void
flip_xy(GwyDataField *dfield)
{
    GwyDataField *tmp;
    gint xres, yres, i, j;
    gdouble *dd;
    const gdouble *sd;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    tmp = gwy_data_field_duplicate(dfield);
    gwy_data_field_resample(dfield, yres, xres, GWY_INTERPOLATION_NONE);
    sd = gwy_data_field_get_data_const(tmp);
    dd = gwy_data_field_get_data(dfield);
    for (i = 0; i < xres; i++) {
        for (j = 0; j < yres; j++) {
            dd[i*yres + j] = sd[j*xres + i];
        }
    }
    gwy_data_field_set_xreal(dfield, gwy_data_field_get_yreal(tmp));
    gwy_data_field_set_yreal(dfield, gwy_data_field_get_xreal(tmp));
    g_object_unref(tmp);
}

static const gchar base_plane_key[]  = "/module/volume_slice/base_plane";
static const gchar multiselect_key[] = "/module/volume_slice/multiselect";
static const gchar output_type_key[] = "/module/volume_slice/output_type";
static const gchar xpos_key[]        = "/module/volume_slice/xpos";
static const gchar ypos_key[]        = "/module/volume_slice/ypos";
static const gchar zpos_key[]        = "/module/volume_slice/zpos";

static void
slice_sanitize_args(SliceArgs *args)
{
    /* Positions are validated against the brick. */
    args->base_plane = MIN(args->base_plane, NPLANES-1);
    args->output_type = MIN(args->output_type, NOUTPUTS-1);
    args->multiselect = !!args->multiselect;
    gwy_app_data_id_verify_graph(&args->target_graph);
}

static void
slice_load_args(GwyContainer *container,
                SliceArgs *args)
{
    *args = slice_defaults;

    gwy_container_gis_enum_by_name(container, base_plane_key,
                                   &args->base_plane);
    gwy_container_gis_enum_by_name(container, output_type_key,
                                   &args->output_type);
    gwy_container_gis_int32_by_name(container, xpos_key, &args->currpos.x);
    gwy_container_gis_int32_by_name(container, ypos_key, &args->currpos.y);
    gwy_container_gis_int32_by_name(container, zpos_key, &args->currpos.z);
    gwy_container_gis_boolean_by_name(container, multiselect_key,
                                      &args->multiselect);
    args->target_graph = target_graph_id;
    slice_sanitize_args(args);
}

static void
slice_save_args(GwyContainer *container,
                SliceArgs *args)
{
    target_graph_id = args->target_graph;
    gwy_container_set_enum_by_name(container, base_plane_key, args->base_plane);
    gwy_container_set_enum_by_name(container, output_type_key,
                                   args->output_type);
    gwy_container_set_int32_by_name(container, xpos_key, args->currpos.x);
    gwy_container_set_int32_by_name(container, ypos_key, args->currpos.y);
    gwy_container_set_int32_by_name(container, zpos_key, args->currpos.z);
    gwy_container_set_boolean_by_name(container, multiselect_key,
                                      args->multiselect);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
