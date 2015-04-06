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
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-volume.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define SLICE_RUN_MODES (GWY_RUN_INTERACTIVE)

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

enum {
    PREVIEW_SIZE = 360,
};

enum {
    RESPONSE_RESET = 1,
};

typedef struct {
    SliceBasePlane base_plane;
    SliceOutputType output_type;
    gint xpos;
    gint ypos;
    gint zpos;
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
    GtkObject *xpos;
    GtkObject *ypos;
    GtkObject *zpos;
    GwySIValueFormat *xvf;
    GwySIValueFormat *yvf;
    GwySIValueFormat *zvf;
    GtkWidget *xposreal;
    GtkWidget *yposreal;
    GtkWidget *zposreal;
    gboolean in_update;
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
static void     set_image_first_coord  (SliceControls *controls,
                                        gint i);
static void     set_image_second_coord (SliceControls *controls,
                                        gint i);
static void     set_graph_coord        (SliceControls *controls,
                                        gint i);
static void     update_selections      (SliceControls *controls);
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
    -1, -1, -1,
    /* Dynamic state. */
    NULL,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Extracts image planes and line graphsfrom volume data."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
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

    if (CLAMP(args.xpos, 0, brick->xres-1) != args.xpos)
        args.xpos = brick->xres/2;
    if (CLAMP(args.ypos, 0, brick->yres-1) != args.ypos)
        args.ypos = brick->yres/2;
    if (CLAMP(args.zpos, 0, brick->zres-1) != args.zpos)
        args.zpos = brick->zres/2;

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
                       TRUE, TRUE, 4);

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

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    table = gtk_table_new(5, 2, FALSE);
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

    controls.xpos = gtk_adjustment_new(args->xpos, 0.0, brick->xres-1.0,
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

    controls.ypos = gtk_adjustment_new(args->ypos, 0.0, brick->yres-1.0,
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

    controls.zpos = gtk_adjustment_new(args->zpos, 0.0, brick->zres-1.0,
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

    label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    set_graph_max(&controls);
    controls.in_update = FALSE;

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
slice_reset(SliceControls *controls)
{
    SliceArgs *args = controls->args;
    GwyBrick *brick = args->brick;

    args->xpos = brick->xres/2;
    args->ypos = brick->yres/2;
    args->zpos = brick->zres/2;
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->base_plane),
                                  slice_defaults.base_plane);
    gwy_radio_buttons_set_current(controls->output_type,
                                  slice_defaults.output_type);
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
                        G_GNUC_UNUSED gint id,
                        GwySelection *selection)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    gdouble xy[2];
    gint ixy[2];

    if (controls->in_update)
        return;

    if (!gwy_selection_get_object(selection, 0, xy))
        return;

    ixy[0] = CLAMP(gwy_data_field_rtoj(controls->image, xy[0]),
                   0, controls->image->xres-1);
    ixy[1] = CLAMP(gwy_data_field_rtoi(controls->image, xy[1]),
                   0, controls->image->yres-1);
    controls->in_update = TRUE;
    set_image_first_coord(controls, ixy[0]);
    set_image_second_coord(controls, ixy[1]);
    controls->in_update = FALSE;

    gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
    gcmodel = gwy_graph_model_get_curve(gmodel, 0);
    extract_graph_curve(controls->args, gmodel, gcmodel);

    update_labels(controls);
}

static void
plane_selection_changed(SliceControls *controls,
                        G_GNUC_UNUSED gint id,
                        GwySelection *selection)
{
    gdouble x;
    gint ix, max;

    if (controls->in_update)
        return;

    if (!gwy_selection_get_object(selection, 0, &x))
        return;

    max = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(selection), "max"));
    ix = CLAMP(GWY_ROUND(x), 0, max);
    controls->in_update = TRUE;
    set_graph_coord(controls, ix);
    controls->in_update = FALSE;

    extract_image_plane(controls->args, controls->image);
    gwy_data_field_data_changed(controls->image);

    update_labels(controls);
}

static void
xpos_changed(SliceControls *controls, GtkAdjustment *adj)
{
    if (controls->in_update)
        return;

    controls->args->xpos = gwy_adjustment_get_int(adj);
    update_selections(controls);
}

static void
ypos_changed(SliceControls *controls, GtkAdjustment *adj)
{
    if (controls->in_update)
        return;

    controls->args->ypos = gwy_adjustment_get_int(adj);
    update_selections(controls);
}

static void
zpos_changed(SliceControls *controls, GtkAdjustment *adj)
{
    if (controls->in_update)
        return;

    controls->args->zpos = gwy_adjustment_get_int(adj);
    update_selections(controls);
}

static void
base_plane_changed(GtkComboBox *combo, SliceControls *controls)
{
    SliceArgs *args = controls->args;
    gint xpos = args->xpos, ypos = args->ypos, zpos = args->zpos;

    controls->args->base_plane = gwy_enum_combo_box_get_active(combo);
    update_selections(controls);

    /* The selection got clipped during the switch.  Restore it. */
    args->xpos = xpos;
    args->ypos = ypos;
    args->zpos = zpos;
    update_selections(controls);
}

static void
output_type_changed(GtkWidget *button, SliceControls *controls)
{
    controls->args->output_type = gwy_radio_button_get_value(button);
}

static void
set_image_first_coord(SliceControls *controls, gint i)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;

    if (base_plane == PLANE_XY || base_plane == PLANE_XZ) {
        args->xpos = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xpos), i);
    }
    else if (base_plane == PLANE_YX || base_plane == PLANE_YZ) {
        args->ypos = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ypos), i);
    }
    else if (base_plane == PLANE_ZX || base_plane == PLANE_ZY) {
        args->zpos = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zpos), i);
    }
}

static void
set_image_second_coord(SliceControls *controls, gint i)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;

    if (base_plane == PLANE_YX || base_plane == PLANE_ZX) {
        args->xpos = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xpos), i);
    }
    else if (base_plane == PLANE_XY || base_plane == PLANE_ZY) {
        args->ypos = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ypos), i);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_YZ) {
        args->zpos = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zpos), i);
    }
}

static void
set_graph_coord(SliceControls *controls, gint i)
{
    SliceArgs *args = controls->args;
    SliceBasePlane base_plane = args->base_plane;

    if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        args->xpos = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xpos), i);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        args->ypos = i;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ypos), i);
    }
    else if (base_plane == PLANE_YX || base_plane == PLANE_XY) {
        args->zpos = i;
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
    gdouble xy[2], x;

    if (base_plane == PLANE_XY) {
        xy[0] = gwy_brick_jtor(brick, args->xpos);
        xy[1] = gwy_brick_itor(brick, args->ypos);
        x = gwy_brick_ktor(brick, args->zpos);
    }
    else if (base_plane == PLANE_YX) {
        xy[0] = gwy_brick_itor(brick, args->ypos);
        xy[1] = gwy_brick_jtor(brick, args->xpos);
        x = gwy_brick_ktor(brick, args->zpos);
    }
    else if (base_plane == PLANE_XZ) {
        xy[0] = gwy_brick_jtor(brick, args->xpos);
        xy[1] = gwy_brick_ktor(brick, args->zpos);
        x = gwy_brick_itor(brick, args->ypos);
    }
    else if (base_plane == PLANE_ZX) {
        xy[0] = gwy_brick_ktor(brick, args->zpos);
        xy[1] = gwy_brick_jtor(brick, args->xpos);
        x = gwy_brick_itor(brick, args->ypos);
    }
    else if (base_plane == PLANE_YZ) {
        xy[0] = gwy_brick_itor(brick, args->ypos);
        xy[1] = gwy_brick_ktor(brick, args->zpos);
        x = gwy_brick_jtor(brick, args->xpos);
    }
    else if (base_plane == PLANE_ZY) {
        xy[0] = gwy_brick_ktor(brick, args->zpos);
        xy[1] = gwy_brick_itor(brick, args->ypos);
        x = gwy_brick_jtor(brick, args->xpos);
    }
    else {
        g_return_if_reached();
    }

    selection = gwy_vector_layer_ensure_selection(controls->vlayer);
    gwy_selection_set_data(selection, 1, xy);

    area = gwy_graph_get_area(GWY_GRAPH(controls->graph));
    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                             GWY_GRAPH_STATUS_XLINES);
    gwy_selection_set_data(selection, 1, &x);

    update_labels(controls);
}

static void
update_labels(SliceControls *controls)
{
    SliceArgs *args = controls->args;
    GwyBrick *brick = args->brick;
    GwyDataLine *calibration;
    gdouble x, y, z;
    gchar buf[64];

    x = gwy_brick_itor(brick, args->xpos);
    g_snprintf(buf, sizeof(buf), "%.*f",
               controls->xvf->precision, x/controls->xvf->magnitude);
    gtk_label_set_markup(GTK_LABEL(controls->xposreal), buf);

    y = gwy_brick_jtor(brick, args->ypos);
    g_snprintf(buf, sizeof(buf), "%.*f",
               controls->xvf->precision, y/controls->yvf->magnitude);
    gtk_label_set_markup(GTK_LABEL(controls->yposreal), buf);

    z = gwy_brick_ktor(brick, args->zpos);
    if ((calibration = gwy_brick_get_zcalibration(brick)))
        z = gwy_data_line_get_val(calibration, args->zpos);
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
            i = args->zpos;
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
            i = args->ypos;
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
            i = args->xpos;
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
                                0, 0, args->zpos,
                                brick->xres, brick->yres, -1,
                                FALSE);
    }
    else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        do_flip = (base_plane == PLANE_ZY);
        gwy_brick_extract_plane(args->brick, dfield,
                                args->xpos, 0, 0,
                                -1, brick->yres, brick->zres,
                                FALSE);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        do_flip = (base_plane == PLANE_ZX);
        gwy_brick_extract_plane(args->brick, dfield,
                                0, args->ypos, 0,
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
                               args->xpos, args->ypos, 0,
                               args->xpos, args->ypos, brick->zres-1,
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
                   args->xpos, args->ypos);
    }
    else if (base_plane == PLANE_YZ || base_plane == PLANE_ZY) {
        gwy_brick_extract_line(brick, line,
                               0, args->ypos, args->zpos,
                               brick->xres-1, args->ypos, args->zpos,
                               FALSE);

        xlabel = "y";
        ylabel = "z";
        if (base_plane == PLANE_ZY)
            GWY_SWAP(const gchar*, xlabel, ylabel);

        g_snprintf(desc, sizeof(desc), _("X graph at y: %d z: %d"),
                   args->ypos, args->zpos);
    }
    else if (base_plane == PLANE_XZ || base_plane == PLANE_ZX) {
        gwy_brick_extract_line(brick, line,
                               args->xpos, 0, args->zpos,
                               args->xpos, brick->yres-1, args->zpos,
                               FALSE);

        xlabel = "x";
        ylabel = "z";
        if (base_plane == PLANE_ZX)
            GWY_SWAP(const gchar*, xlabel, ylabel);

        g_snprintf(desc, sizeof(desc), _("Y graph at x: %d z: %d"),
                   args->xpos, args->zpos);
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
    gwy_container_gis_int32_by_name(container, xpos_key, &args->xpos);
    gwy_container_gis_int32_by_name(container, ypos_key, &args->ypos);
    gwy_container_gis_int32_by_name(container, zpos_key, &args->zpos);
    slice_sanitize_args(args);
}

static void
slice_save_args(GwyContainer *container,
                SliceArgs *args)
{
    gwy_container_set_enum_by_name(container, base_plane_key, args->base_plane);
    gwy_container_set_enum_by_name(container, output_type_key, args->output_type);
    gwy_container_set_int32_by_name(container, xpos_key, args->xpos);
    gwy_container_set_int32_by_name(container, ypos_key, args->ypos);
    gwy_container_set_int32_by_name(container, zpos_key, args->zpos);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
