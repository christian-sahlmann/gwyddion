/*
 *  @(#) $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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
    SlicePos pos;
    gboolean multiselect;
    /* Dynamic state. */
    GwyBrick *brick;
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
    GArray *pos;
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
static void     create_coordlist       (SliceControls *controls);
static void     slice_reset            (SliceControls *controls);
static void     set_graph_max          (SliceControls *controls);
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
static void     multiselect_changed    (SliceControls *controls,
                                        GtkToggleButton *button);
static void     reduce_selection       (SliceControls *controls);
static void     set_image_first_coord  (SliceControls *controls,
                                        gint i);
static void     set_image_second_coord (SliceControls *controls,
                                        gint i);
static void     set_graph_coord        (SliceControls *controls,
                                        gint i);
static void     update_selections      (SliceControls *controls);
static void     update_multiselection  (SliceControls *controls);
static void     update_labels          (SliceControls *controls);
static void     extract_image_plane    (const SliceArgs *args,
                                        GwyDataField *dfield);
static void     extract_graph_curve    (const SliceArgs *args,
                                        GwyGraphModel *gmodel,
                                        GwyGraphCurveModel *gcmodel);
static void     flip_xy                (GwyDataField *dfield);
static void     slice_sanitize_args    (SliceArgs *args);
static void     slice_load_args        (GwyContainer *container,
                                        SliceArgs *args);
static void     slice_save_args        (GwyContainer *container,
                                        SliceArgs *args);

static const SliceArgs slice_defaults = {
    PLANE_XY, OUTPUT_IMAGES,
    { -1, -1, -1 },
    FALSE,
    /* Dynamic state. */
    NULL,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Extracts image planes and line graphs from volume data."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
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

    if (CLAMP(args.pos.x, 0, brick->xres-1) != args.pos.x)
        args.pos.x = brick->xres/2;
    if (CLAMP(args.pos.y, 0, brick->yres-1) != args.pos.y)
        args.pos.y = brick->yres/2;
    if (CLAMP(args.pos.z, 0, brick->zres-1) != args.pos.z)
        args.pos.z = brick->zres/2;

    if (slice_dialog(&args, data, id)) {
        slice_do(&args, data, id);
    }

    slice_save_args(gwy_app_settings_get(), &args);
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
    const guchar *gradient;
    gchar key[40];

    controls.args = args;
    controls.in_update = TRUE;
    controls.pos = g_array_new(FALSE, FALSE, sizeof(SlicePos));

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
    gwy_graph_set_axis_visible(GWY_GRAPH(controls.graph), GTK_POS_LEFT,
                               FALSE);
    gwy_graph_set_axis_visible(GWY_GRAPH(controls.graph), GTK_POS_BOTTOM,
                               FALSE);
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

    table = gtk_table_new(6, 2, FALSE);
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
    label = gtk_label_new_with_mnemonic(_("Output type:"));
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

    controls.xpos = gtk_adjustment_new(args->pos.x, 0.0, brick->xres-1.0,
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
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     4, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.ypos = gtk_adjustment_new(args->pos.y, 0.0, brick->yres-1.0,
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
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     4, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.zpos = gtk_adjustment_new(args->pos.z, 0.0, brick->zres-1.0,
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
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     4, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    create_coordlist(&controls);
    gtk_box_pack_start(GTK_BOX(hbox), controls.scwin, FALSE, TRUE, 0);

    label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    set_graph_max(&controls);
    controls.in_update = FALSE;

    multiselect_changed(&controls, GTK_TOGGLE_BUTTON(controls.multiselect));
    update_selections(&controls);

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
            g_array_free(controls.pos, TRUE);
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
    g_array_free(controls.pos, TRUE);

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
        g_return_if_fail(idx < controls->pos->len);
        pos = &g_array_index(controls->pos, SlicePos, idx);
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

    controls->store = gwy_null_store_new(0);
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

    args->pos.x = brick->xres/2;
    args->pos.y = brick->yres/2;
    args->pos.z = brick->zres/2;
    reduce_selection(controls);
    /* Just reset the selection here?
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->base_plane),
                                  slice_defaults.base_plane);
    gwy_radio_buttons_set_current(controls->output_type,
                                  slice_defaults.output_type);
                                  */
}

static void
set_graph_max(SliceControls *controls)
{
    SliceBasePlane base_plane = controls->args->base_plane;
    GtkWidget *area;
    GwySelection *selection;
    gint max = 0;

    area = gwy_graph_get_area(GWY_GRAPH(controls->graph));
    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                             GWY_GRAPH_STATUS_XLINES);

    if (base_plane == PLANE_YZ || base_plane == PLANE_ZY)
        max = controls->args->brick->xres-1;
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX)
        max = controls->args->brick->yres-1;
    else if (base_plane == PLANE_YX || base_plane == PLANE_XY)
        max = controls->args->brick->zres-1;

    g_object_set_data(G_OBJECT(selection), "max", GINT_TO_POINTER(max));
}

static void
point_selection_changed(SliceControls *controls,
                        gint id,
                        GwySelection *selection)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    gdouble xy[2];
    gint ixy[2];

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

    ixy[0] = CLAMP(gwy_data_field_rtoi(controls->image, xy[0]),
                   0, controls->image->xres-1);
    ixy[1] = CLAMP(gwy_data_field_rtoj(controls->image, xy[1]),
                   0, controls->image->yres-1);
    controls->in_update = TRUE;
    set_image_first_coord(controls, ixy[0]);
    set_image_second_coord(controls, ixy[1]);
    controls->in_update = FALSE;

    gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
    gcmodel = gwy_graph_model_get_curve(gmodel, 0);
    extract_graph_curve(controls->args, gmodel, gcmodel);

    update_labels(controls);
    update_multiselection(controls);
}

static void
plane_selection_changed(SliceControls *controls,
                        gint id,
                        GwySelection *selection)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;
    GwyBrick *brick = args->brick;
    gdouble z;
    gint ix, max;

    gwy_debug("%d (%d)", controls->in_update, id);
    if (controls->in_update)
        return;

    /* What should we do here?  Hope we always get another update with a
     * specific id afterwards. */
    if (id < 0)
        return;

    if (!gwy_selection_get_object(selection, id, &z))
        return;

    if (controls->args->output_type == OUTPUT_IMAGES)
        controls->current_object = id;

    max = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(selection), "max"));
    if (base_plane == PLANE_YZ || base_plane == PLANE_ZY)
        ix = CLAMP(gwy_brick_rtoi(brick, z), 0, max);
    else if (base_plane == PLANE_YX || base_plane == PLANE_XY)
        ix = CLAMP(gwy_brick_rtok(brick, z), 0, max);
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX)
        ix = CLAMP(gwy_brick_rtoj(brick, z), 0, max);
    else {
        g_return_if_reached();
    }

    controls->in_update = TRUE;
    set_graph_coord(controls, ix);
    controls->in_update = FALSE;

    extract_image_plane(controls->args, controls->image);
    gwy_data_field_data_changed(controls->image);

    update_labels(controls);
    update_multiselection(controls);
}

static void
xpos_changed(SliceControls *controls, GtkAdjustment *adj)
{
    if (controls->in_update)
        return;

    controls->args->pos.x = gwy_adjustment_get_int(adj);
    update_selections(controls);
}

static void
ypos_changed(SliceControls *controls, GtkAdjustment *adj)
{
    if (controls->in_update)
        return;

    controls->args->pos.y = gwy_adjustment_get_int(adj);
    update_selections(controls);
}

static void
zpos_changed(SliceControls *controls, GtkAdjustment *adj)
{
    if (controls->in_update)
        return;

    controls->args->pos.z = gwy_adjustment_get_int(adj);
    update_selections(controls);
}

static void
base_plane_changed(GtkComboBox *combo, SliceControls *controls)
{
    SliceArgs *args = controls->args;
    gint xpos = args->pos.x, ypos = args->pos.y, zpos = args->pos.z;

    controls->args->base_plane = gwy_enum_combo_box_get_active(combo);
    set_graph_max(controls);
    update_selections(controls);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls->view), PREVIEW_SIZE);

    /* The selection got clipped during the switch.  Restore it. */
    args->pos.x = xpos;
    args->pos.y = ypos;
    args->pos.z = zpos;
    update_selections(controls);
}

static void
output_type_changed(GtkWidget *button, SliceControls *controls)
{
    controls->args->output_type = gwy_radio_button_get_value(button);
    /* In multiselection mode it ensures the non-multiple coordinates are
     * compacted to single one. */
    reduce_selection(controls);
    multiselect_changed(controls, GTK_TOGGLE_BUTTON(controls->multiselect));
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
    GwySelection *selection;
    GtkWidget *area;
    SlicePos pos = controls->args->pos;
    gdouble coord[2] = { 0.0, 0.0 };

    controls->current_object = 0;
    gwy_null_store_set_n_rows(controls->store, 1);
    g_array_set_size(controls->pos, 1);

    controls->in_update = TRUE;
    selection = gwy_vector_layer_ensure_selection(controls->vlayer);
    gwy_selection_set_data(selection, 1, coord);

    area = gwy_graph_get_area(GWY_GRAPH(controls->graph));
    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                             GWY_GRAPH_STATUS_XLINES);
    gwy_selection_set_data(selection, 1, coord);
    controls->in_update = FALSE;

    gwy_debug("(%d %d %d)", pos.x, pos.y, pos.z);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xpos), pos.x);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ypos), pos.y);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zpos), pos.z);
    /* Since we might not emit "value-changed" on the adjustments update
     * selections explicitly afterwards.*/
    update_selections(controls);
}

static void
set_image_first_coord(SliceControls *controls, gint i)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;

    if (base_plane == PLANE_XY || base_plane == PLANE_XZ) {
        args->pos.x = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xpos), i);
    }
    else if (base_plane == PLANE_YX || base_plane == PLANE_YZ) {
        args->pos.y = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ypos), i);
    }
    else if (base_plane == PLANE_ZX || base_plane == PLANE_ZY) {
        args->pos.z = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zpos), i);
    }
}

static void
set_image_second_coord(SliceControls *controls, gint i)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;

    if (base_plane == PLANE_YX || base_plane == PLANE_ZX) {
        args->pos.x = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xpos), i);
    }
    else if (base_plane == PLANE_XY || base_plane == PLANE_ZY) {
        args->pos.y = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ypos), i);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_YZ) {
        args->pos.z = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zpos), i);
    }
}

static void
set_graph_coord(SliceControls *controls, gint i)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;

    if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        args->pos.x = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xpos), i);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        args->pos.y = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ypos), i);
    }
    else if (base_plane == PLANE_YX || base_plane == PLANE_XY) {
        args->pos.z = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zpos), i);
    }
}

static void
update_selections(SliceControls *controls)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;
    GtkWidget *area;
    GwySelection *selection;
    GwyBrick *brick = args->brick;
    gdouble xy[2], z;

    if (base_plane == PLANE_XY) {
        xy[0] = gwy_brick_itor(brick, args->pos.x);
        xy[1] = gwy_brick_jtor(brick, args->pos.y);
        z = gwy_brick_ktor(brick, args->pos.z);
    }
    else if (base_plane == PLANE_YX) {
        xy[0] = gwy_brick_jtor(brick, args->pos.y);
        xy[1] = gwy_brick_itor(brick, args->pos.x);
        z = gwy_brick_ktor(brick, args->pos.z);
    }
    else if (base_plane == PLANE_XZ) {
        xy[0] = gwy_brick_itor(brick, args->pos.x);
        xy[1] = gwy_brick_ktor(brick, args->pos.z);
        z = gwy_brick_jtor(brick, args->pos.y);
    }
    else if (base_plane == PLANE_ZX) {
        xy[0] = gwy_brick_ktor(brick, args->pos.z);
        xy[1] = gwy_brick_itor(brick, args->pos.x);
        z = gwy_brick_jtor(brick, args->pos.y);
    }
    else if (base_plane == PLANE_YZ) {
        xy[0] = gwy_brick_jtor(brick, args->pos.y);
        xy[1] = gwy_brick_ktor(brick, args->pos.z);
        z = gwy_brick_itor(brick, args->pos.x);
    }
    else if (base_plane == PLANE_ZY) {
        xy[0] = gwy_brick_ktor(brick, args->pos.z);
        xy[1] = gwy_brick_jtor(brick, args->pos.y);
        z = gwy_brick_itor(brick, args->pos.x);
    }
    else {
        g_return_if_reached();
    }

    selection = gwy_vector_layer_ensure_selection(controls->vlayer);
    gwy_selection_set_object(selection, controls->current_object, xy);

    area = gwy_graph_get_area(GWY_GRAPH(controls->graph));
    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                             GWY_GRAPH_STATUS_XLINES);
    gwy_selection_set_object(selection, controls->current_object, &z);

    update_labels(controls);
    update_multiselection(controls);
}

static void
update_multiselection(SliceControls *controls)
{
    GtkTreeSelection *selection;
    SliceArgs *args = controls->args;
    gint curr = controls->current_object;
    gint len = controls->pos->len;
    GtkTreeIter iter;
    GtkTreePath *path;

    /* TODO: */
    gwy_debug("len: %d, curr: %d", len, curr);
    if (len == curr) {
        g_array_append_val(controls->pos, args->pos);
        gwy_null_store_set_n_rows(controls->store, curr+1);
    }
    else if (len > controls->current_object) {
        g_array_index(controls->pos, SlicePos, curr) = args->pos;
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

    x = gwy_brick_itor(brick, args->pos.x);
    g_snprintf(buf, sizeof(buf), "%.*f",
               controls->xvf->precision, x/controls->xvf->magnitude);
    gtk_label_set_markup(GTK_LABEL(controls->xposreal), buf);

    y = gwy_brick_jtor(brick, args->pos.y);
    g_snprintf(buf, sizeof(buf), "%.*f",
               controls->xvf->precision, y/controls->yvf->magnitude);
    gtk_label_set_markup(GTK_LABEL(controls->yposreal), buf);

    z = gwy_brick_ktor(brick, args->pos.z);
    if ((calibration = gwy_brick_get_zcalibration(brick)))
        z = gwy_data_line_get_val(calibration, args->pos.z);
    g_snprintf(buf, sizeof(buf), "%.*f",
               controls->zvf->precision, z/controls->zvf->magnitude);
    gtk_label_set_markup(GTK_LABEL(controls->zposreal), buf);
}

static void
slice_do(SliceArgs *args, GwyContainer *data, gint id)
{
    GwyBrick *brick = args->brick;
    SliceBasePlane base_plane = args->base_plane;
    SliceOutputType output_type = args->output_type;
    gint newid;

    if (output_type == OUTPUT_IMAGES) {
        GwyDataField *dfield = gwy_data_field_new(1, 1, 1.0, 1.0, TRUE);
        GwyDataLine *calibration;
        GwySIValueFormat *vf;
        const guchar *gradient;
        gchar *title = NULL;
        gchar key[40];
        gdouble r;
        gint i;

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
            i = args->pos.z;
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
            title = g_strdup_printf(_("Z cross-section at Z = %.*f%s%s (#%d)"),
                                    vf->precision, r/vf->magnitude,
                                    strlen(vf->units) ? " " : "", vf->units,
                                    i);
        }
        else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
            i = args->pos.y;
            r = gwy_brick_jtor(brick, i);
            vf = gwy_brick_get_value_format_y(brick,
                                              GWY_SI_UNIT_FORMAT_VFMARKUP,
                                              NULL);
            title = g_strdup_printf(_("Y cross-section at Y = %.*f%s%s (#%d)"),
                                    vf->precision, r/vf->magnitude,
                                    strlen(vf->units) ? " " : "", vf->units,
                                    i);
        }
        else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
            i = args->pos.x;
            r = gwy_brick_itor(brick, i);
            vf = gwy_brick_get_value_format_x(brick,
                                              GWY_SI_UNIT_FORMAT_VFMARKUP,
                                              NULL);
            title = g_strdup_printf(_("X cross-section at X = %.*f%s%s (#%d)"),
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
    else if (output_type == OUTPUT_GRAPHS) {
        GwyGraphModel *gmodel = gwy_graph_model_new();
        GwyGraphCurveModel *gcmodel = gwy_graph_curve_model_new();

        extract_graph_curve(args, gmodel, gcmodel);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);

        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        g_object_unref(gmodel);
    }
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
                                0, 0, args->pos.z,
                                brick->xres, brick->yres, -1,
                                FALSE);
    }
    else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        do_flip = (base_plane == PLANE_ZY);
        gwy_brick_extract_plane(args->brick, dfield,
                                args->pos.x, 0, 0,
                                -1, brick->yres, brick->zres,
                                FALSE);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        do_flip = (base_plane == PLANE_ZX);
        gwy_brick_extract_plane(args->brick, dfield,
                                0, args->pos.y, 0,
                                brick->xres, -1, brick->zres,
                                FALSE);
    }

    if (do_flip)
        flip_xy(dfield);
}

static void
extract_graph_curve(const SliceArgs *args,
                    GwyGraphModel *gmodel,
                    GwyGraphCurveModel *gcmodel)
{
    SliceBasePlane base_plane = args->base_plane;
    GwyDataLine *line = gwy_data_line_new(1, 1.0, FALSE);
    GwyDataLine *calibration = NULL;
    GwyBrick *brick = args->brick;
    const gchar *xlabel, *ylabel;
    gchar desc[80];

    if (base_plane == PLANE_XY || base_plane == PLANE_YX) {
        gwy_brick_extract_line(brick, line,
                               args->pos.x, args->pos.y, 0,
                               args->pos.x, args->pos.y, brick->zres,
                               FALSE);
        /* Try to use the calibration.  Ignore if the dimension does not seem
         * right. */
        calibration = gwy_brick_get_zcalibration(brick);
        if (calibration
            && (gwy_data_line_get_res(line)
                != gwy_data_line_get_res(calibration)))
            calibration = NULL;

        xlabel = "x";
        ylabel = "y";
        if (base_plane == PLANE_YX)
            GWY_SWAP(const gchar*, xlabel, ylabel);

        g_snprintf(desc, sizeof(desc), _("Z graph at x: %d y: %d"),
                   args->pos.x, args->pos.y);
    }
    else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        gwy_brick_extract_line(brick, line,
                               0, args->pos.y, args->pos.z,
                               brick->xres-1, args->pos.y, args->pos.z,
                               FALSE);

        xlabel = "y";
        ylabel = "z";
        if (base_plane == PLANE_ZY)
            GWY_SWAP(const gchar*, xlabel, ylabel);

        g_snprintf(desc, sizeof(desc), _("X graph at y: %d z: %d"),
                   args->pos.y, args->pos.z);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        gwy_brick_extract_line(brick, line,
                               args->pos.x, 0, args->pos.z,
                               args->pos.x, brick->yres-1, args->pos.z,
                               FALSE);

        xlabel = "x";
        ylabel = "z";
        if (base_plane == PLANE_ZX)
            GWY_SWAP(const gchar*, xlabel, ylabel);

        g_snprintf(desc, sizeof(desc), _("Y graph at x: %d z: %d"),
                   args->pos.x, args->pos.z);
    }
    else {
        g_return_if_reached();
    }

    g_object_set(gcmodel,
                 "description", _("Brick graph"),
                 "mode", GWY_GRAPH_CURVE_LINE,
                 NULL);

    if (calibration) {
        gwy_graph_curve_model_set_data(gcmodel,
                                       gwy_data_line_get_data(calibration),
                                       gwy_data_line_get_data(line),
                                       gwy_data_line_get_res(line));
        /* XXX: This unit object sharing is safe only because we are going to
         * destroy the data line immediately. */
        gwy_data_line_set_si_unit_x(line,
                                    gwy_data_line_get_si_unit_y(calibration));
    }
    else
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, line, 0, 0);

    gwy_graph_model_set_units_from_data_line(gmodel, line);
    g_object_unref(line);

    g_object_set(gmodel,
                 "title", desc,
                 "axis-label-bottom", xlabel,
                 "axis-label-left", ylabel,
                 NULL);
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
    gwy_container_gis_int32_by_name(container, xpos_key, &args->pos.x);
    gwy_container_gis_int32_by_name(container, ypos_key, &args->pos.y);
    gwy_container_gis_int32_by_name(container, zpos_key, &args->pos.z);
    gwy_container_gis_boolean_by_name(container, multiselect_key,
                                      &args->multiselect);
    slice_sanitize_args(args);
}

static void
slice_save_args(GwyContainer *container,
                SliceArgs *args)
{
    gwy_container_set_enum_by_name(container, base_plane_key, args->base_plane);
    gwy_container_set_enum_by_name(container, output_type_key,
                                   args->output_type);
    gwy_container_set_int32_by_name(container, xpos_key, args->pos.x);
    gwy_container_set_int32_by_name(container, ypos_key, args->pos.y);
    gwy_container_set_int32_by_name(container, zpos_key, args->pos.z);
    gwy_container_set_boolean_by_name(container, multiselect_key,
                                      args->multiselect);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
