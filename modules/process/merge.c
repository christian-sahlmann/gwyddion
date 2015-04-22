/*
 *  @(#) $Id$
 *  Copyright (C) 2004,2014-2015 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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
#include <libprocess/arithmetic.h>
#include <libprocess/correlation.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define MERGE_RUN_MODES GWY_RUN_INTERACTIVE

typedef enum {
    GWY_MERGE_DIRECTION_UP,
    GWY_MERGE_DIRECTION_DOWN,
    GWY_MERGE_DIRECTION_RIGHT,
    GWY_MERGE_DIRECTION_LEFT,
    GWY_MERGE_DIRECTION_LAST
} GwyMergeDirectionType;

typedef enum {
    GWY_MERGE_MODE_CORRELATE,
    GWY_MERGE_MODE_JOIN,
    GWY_MERGE_MODE_NONE,
    GWY_MERGE_MODE_LAST
} GwyMergeModeType;

typedef enum {
    GWY_MERGE_BOUNDARY_FIRST,
    GWY_MERGE_BOUNDARY_SECOND,
    GWY_MERGE_BOUNDARY_AVERAGE,
    GWY_MERGE_BOUNDARY_INTERPOLATE,
    GWY_MERGE_BOUNDARY_LAST
} GwyMergeBoundaryType;

typedef struct {
    gint x;
    gint y;
    gint width;
    gint height;
} GwyRectangle;

typedef struct {
    gint x;
    gint y;
} GwyCoord;

typedef struct {
    GwyMergeDirectionType direction;
    GwyMergeModeType mode;
    GwyMergeBoundaryType boundary;
    gboolean create_mask;
    gboolean crop_to_rectangle;
    GwyAppDataId op1;
    GwyAppDataId op2;
} MergeArgs;

typedef struct {
    MergeArgs *args;
    GtkWidget *dialog;
    GtkWidget *op2;
    GtkWidget *boundary;
    GtkWidget *crop_to_rectangle;
    GtkWidget *create_mask;
} MergeControls;

static gboolean module_register          (void);
static void     merge                    (GwyContainer *data,
                                          GwyRunType run);
static gboolean merge_dialog             (MergeArgs *args);
static void     merge_data_changed       (MergeControls *controls,
                                          GwyDataChooser *chooser);
static gboolean merge_data_filter        (GwyContainer *data,
                                          gint id,
                                          gpointer user_data);
static void     merge_do_correlate       (MergeArgs *args);
static void     merge_do_join            (MergeArgs *args);
static void     merge_do_none            (MergeArgs *args);
static void     merge_direction_changed  (GtkWidget *combo,
                                          MergeControls *control);
static void     merge_mode_changed       (GtkWidget *combo,
                                          MergeControls *controls);
static void     merge_boundary_changed   (GtkWidget *combo,
                                          MergeControls *controls);
static void     create_mask_changed      (MergeControls *controls,
                                          GtkToggleButton *toggle);
static void     crop_to_rectangle_changed(MergeControls *controls,
                                          GtkToggleButton *toggle);
static void     update_sensitivity       (MergeControls *controls);
static gdouble  get_row_difference       (GwyDataField *dfield1,
                                          gint col1,
                                          gint row1,
                                          GwyDataField *dfield2,
                                          gint col2,
                                          gint row2,
                                          gint width,
                                          gint height);
static gdouble  get_column_difference    (GwyDataField *dfield1,
                                          gint col1,
                                          gint row1,
                                          GwyDataField *dfield2,
                                          gint col2,
                                          gint row2,
                                          gint width,
                                          gint height);
static gboolean get_score_iteratively    (GwyDataField *data_field,
                                          GwyDataField *kernel_field,
                                          GwyDataField *score,
                                          MergeArgs *args);
static void     find_score_maximum       (GwyDataField *correlation_score,
                                          gint *max_col,
                                          gint *max_row);
static void     merge_boundary           (GwyDataField *dfield1,
                                          GwyDataField *dfield2,
                                          GwyDataField *result,
                                          GwyRectangle res_rect,
                                          GwyCoord f1_pos,
                                          GwyCoord f2_pos,
                                          GwyMergeBoundaryType boundary);
static void     create_merged_field      (GwyContainer *data,
                                          gint id1,
                                          GwyDataField *dfield1,
                                          GwyDataField *dfield2,
                                          gint px1,
                                          gint py1,
                                          gint px2,
                                          gint py2,
                                          GwyMergeBoundaryType boundary,
                                          GwyMergeDirectionType dir,
                                          gboolean create_mask,
                                          gboolean crop_to_rectangle);
static void     put_fields               (GwyDataField *dfield1,
                                          GwyDataField *dfield2,
                                          GwyDataField *result,
                                          GwyDataField *outsidemask,
                                          GwyMergeBoundaryType boundary,
                                          gint px1,
                                          gint py1,
                                          gint px2,
                                          gint py2);
static void     crop_result              (GwyDataField *result,
                                          GwyDataField *dfield1,
                                          GwyDataField *dfield2,
                                          GwyOrientation orientation,
                                          gint px1,
                                          gint py1,
                                          gint px2,
                                          gint py2);
static void     merge_load_args          (GwyContainer *settings,
                                          MergeArgs *args);
static void     merge_save_args          (GwyContainer *settings,
                                          MergeArgs *args);
static void     merge_sanitize_args      (MergeArgs *args);

static const GwyEnum directions[] = {
    { N_("Up"),           GWY_MERGE_DIRECTION_UP,    },
    { N_("Down"),         GWY_MERGE_DIRECTION_DOWN,  },
    { N_("adverb|Right"), GWY_MERGE_DIRECTION_RIGHT, },
    { N_("adverb|Left"),  GWY_MERGE_DIRECTION_LEFT,  },
};

static const GwyEnum modes[] = {
    { N_("Correlation"),     GWY_MERGE_MODE_CORRELATE, },
    { N_("merge-mode|Join"), GWY_MERGE_MODE_JOIN,      },
    { N_("merge-mode|None"), GWY_MERGE_MODE_NONE,      },
};

static const GwyEnum boundaries[] = {
    { N_("First operand"),  GWY_MERGE_BOUNDARY_FIRST,       },
    { N_("Second operand"), GWY_MERGE_BOUNDARY_SECOND,      },
    { N_("Average"),        GWY_MERGE_BOUNDARY_AVERAGE,     },
    { N_("Interpolation"),  GWY_MERGE_BOUNDARY_INTERPOLATE, },
};

static const MergeArgs merge_defaults = {
    GWY_MERGE_DIRECTION_RIGHT,
    GWY_MERGE_MODE_CORRELATE,
    GWY_MERGE_BOUNDARY_FIRST,
    FALSE, FALSE,
    { NULL, -1 },
    { NULL, -1 },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Merges two images."),
    "Petr Klapetek <klapetek@gwyddion.net>, Yeti <yeti@gwyddion.net>",
    "3.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("merge",
                              (GwyProcessFunc)&merge,
                              N_("/M_ultidata/_Merge..."),
                              GWY_STOCK_MERGE,
                              MERGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Merge two images"));

    return TRUE;
}

static void
merge(GwyContainer *data, GwyRunType run)
{
    MergeArgs args;
    GwyContainer *settings;

    g_return_if_fail(run & MERGE_RUN_MODES);

    settings = gwy_app_settings_get();
    merge_load_args(settings, &args);

    args.op1.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &args.op1.id, 0);
    args.op2 = args.op1;

    if (merge_dialog(&args)) {
        if (args.mode == GWY_MERGE_MODE_NONE)
            merge_do_none(&args);
        else if (args.mode == GWY_MERGE_MODE_JOIN)
            merge_do_join(&args);
        else
            merge_do_correlate(&args);
    }

    merge_save_args(settings, &args);
}

static gboolean
merge_dialog(MergeArgs *args)
{
    MergeControls controls;
    GtkWidget *dialog, *table, *chooser, *combo, *check;
    gint response, row;
    gboolean ok;

    gwy_clear(&controls, 1);
    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Merge Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = gtk_table_new(6, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /* Merge with */
    chooser = gwy_data_chooser_new_channels();
    controls.op2 = chooser;
    g_object_set_data(G_OBJECT(chooser), "dialog", dialog);
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
                                merge_data_filter, args, NULL);
    g_signal_connect_swapped(chooser, "changed",
                             G_CALLBACK(merge_data_changed), &controls);
    merge_data_changed(&controls, GWY_DATA_CHOOSER(chooser));
    gwy_table_attach_hscale(table, row, _("_Merge with:"), NULL,
                            GTK_OBJECT(chooser), GWY_HSCALE_WIDGET);
    row++;

    /* Parameters */
    combo = gwy_enum_combo_box_new(directions, G_N_ELEMENTS(directions),
                                   G_CALLBACK(merge_direction_changed),
                                   &controls,
                                   args->direction, TRUE);
    gwy_table_attach_hscale(table, row, _("_Put second operand:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    combo = gwy_enum_combo_box_new(modes, G_N_ELEMENTS(modes),
                                   G_CALLBACK(merge_mode_changed), &controls,
                                   args->mode, TRUE);
    gwy_table_attach_hscale(table, row, _("_Align second operand:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    combo = gwy_enum_combo_box_new(boundaries, G_N_ELEMENTS(boundaries),
                                   G_CALLBACK(merge_boundary_changed),
                                   &controls,
                                   args->boundary, TRUE);
    controls.boundary = combo;
    gwy_table_attach_hscale(table, row, _("_Boundary treatment:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Crop result to _avoid "
                                                 "outside pixels"));
    controls.crop_to_rectangle = check;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 args->crop_to_rectangle);
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(check, "toggled",
                             G_CALLBACK(crop_to_rectangle_changed), &controls);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Add _mask of outside pixels"));
    controls.create_mask = check;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), args->create_mask);
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(check, "toggled",
                             G_CALLBACK(create_mask_changed), &controls);
    row++;

    update_sensitivity(&controls);
    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(dialog);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            ok = TRUE;
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
merge_data_changed(MergeControls *controls,
                   GwyDataChooser *chooser)
{
    MergeArgs *args = controls->args;

    args->op2.data = gwy_data_chooser_get_active(chooser, &args->op2.id);
    gwy_debug("data: %p %d", args->op2.data, args->op2.id);

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      GTK_RESPONSE_OK,
                                      args->op2.data != NULL);
}

static gboolean
merge_data_filter(GwyContainer *data,
                  gint id,
                  gpointer user_data)
{
    MergeArgs *args = (MergeArgs*)user_data;
    GwyDataCompatibilityFlags compatflags;
    GwyDataField *op1, *op2;
    gboolean ok;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(args->op1.id);
    op1 = GWY_DATA_FIELD(gwy_container_get_object(args->op1.data, quark));

    quark = gwy_app_get_data_key_for_id(id);
    op2 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    if (op1 == op2)
        return FALSE;

    compatflags = (GWY_DATA_COMPATIBILITY_MEASURE
                   | GWY_DATA_COMPATIBILITY_LATERAL
                   | GWY_DATA_COMPATIBILITY_VALUE);
    ok = !gwy_data_field_check_compatibility(op1, op2, compatflags);
    if (args->mode == GWY_MERGE_MODE_JOIN) {
        if (args->direction == GWY_MERGE_DIRECTION_UP
            || args->direction == GWY_MERGE_DIRECTION_DOWN) {
            if (op1->xres != op2->xres)
                ok = FALSE;
        }
        if (args->direction == GWY_MERGE_DIRECTION_LEFT
            || args->direction == GWY_MERGE_DIRECTION_RIGHT) {
            if (op1->yres != op2->yres)
                ok = FALSE;
        }
    }
    return ok;
}

static void
merge_direction_changed(GtkWidget *combo, MergeControls *controls)
{
    MergeArgs *args = controls->args;
    args->direction = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    gwy_data_chooser_refilter(GWY_DATA_CHOOSER(controls->op2));
}

static void
merge_mode_changed(GtkWidget *combo, MergeControls *controls)
{
    MergeArgs *args = controls->args;
    args->mode = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    gwy_data_chooser_refilter(GWY_DATA_CHOOSER(controls->op2));
    update_sensitivity(controls);
}

static void
merge_boundary_changed(GtkWidget *combo, MergeControls *controls)
{
    MergeArgs *args = controls->args;
    args->boundary = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void
create_mask_changed(MergeControls *controls, GtkToggleButton *toggle)
{
    MergeArgs *args = controls->args;
    args->create_mask = gtk_toggle_button_get_active(toggle);
}

static void
crop_to_rectangle_changed(MergeControls *controls, GtkToggleButton *toggle)
{
    MergeArgs *args = controls->args;
    args->crop_to_rectangle = gtk_toggle_button_get_active(toggle);
    update_sensitivity(controls);
}

static void
update_sensitivity(MergeControls *controls)
{
    MergeArgs *args = controls->args;
    gtk_widget_set_sensitive(controls->create_mask,
                             args->mode != GWY_MERGE_MODE_JOIN
                             && !args->crop_to_rectangle);
    gtk_widget_set_sensitive(controls->crop_to_rectangle,
                             args->mode != GWY_MERGE_MODE_JOIN);
    gtk_widget_set_sensitive(controls->boundary,
                             args->mode != GWY_MERGE_MODE_NONE);
}

static void
merge_do_correlate(MergeArgs *args)
{
    GwyDataField *dfield1, *dfield2;
    GwyDataField *correlation_data, *correlation_kernel, *correlation_score;
    GwyRectangle cdata, kdata;
    gint max_col, max_row;
    gint xres1, xres2, yres1, yres2;
    GwyMergeDirectionType real_dir = args->direction;
    GwyMergeBoundaryType real_boundary = args->boundary;
    gint px1, py1, px2, py2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(args->op1.id);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object(args->op1.data, quark));

    quark = gwy_app_get_data_key_for_id(args->op2.id);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object(args->op2.data, quark));

    if ((dfield1->xres*dfield1->yres) < (dfield2->xres*dfield2->yres)) {
        GWY_SWAP(GwyDataField*, dfield1, dfield2);
        if (args->direction == GWY_MERGE_DIRECTION_UP)
            real_dir = GWY_MERGE_DIRECTION_DOWN;
        else if (args->direction == GWY_MERGE_DIRECTION_DOWN)
            real_dir = GWY_MERGE_DIRECTION_UP;
        else if (args->direction == GWY_MERGE_DIRECTION_LEFT)
            real_dir = GWY_MERGE_DIRECTION_RIGHT;
        else if (args->direction == GWY_MERGE_DIRECTION_RIGHT)
            real_dir = GWY_MERGE_DIRECTION_LEFT;
        else
            g_return_if_reached();

        if (args->boundary == GWY_MERGE_BOUNDARY_FIRST)
            real_boundary = GWY_MERGE_BOUNDARY_SECOND;
        else if (args->boundary == GWY_MERGE_BOUNDARY_SECOND)
            real_boundary = GWY_MERGE_BOUNDARY_FIRST;
    }

    xres1 = gwy_data_field_get_xres(dfield1);
    xres2 = gwy_data_field_get_xres(dfield2);
    yres1 = gwy_data_field_get_yres(dfield1);
    yres2 = gwy_data_field_get_yres(dfield2);

    /*cut data for correlation*/
    switch (real_dir) {
        case GWY_MERGE_DIRECTION_UP:
        cdata.x = 0;
        cdata.y = 0;
        cdata.width = xres1;
        cdata.height = yres1/2;
        kdata.width = MIN(xres2, cdata.width/2);
        kdata.height = MIN(yres2, cdata.height/3);
        kdata.x = MAX(0, xres2/2 - kdata.width/2);
        kdata.y = MAX(0, yres2 - cdata.height/3);
        break;

        case GWY_MERGE_DIRECTION_DOWN:
        cdata.x = 0;
        cdata.y = yres1 - (yres1/2);
        cdata.width = xres1;
        cdata.height = yres1/2;
        kdata.width = MIN(xres2, cdata.width/2);
        kdata.height = MIN(yres2, cdata.height/3);
        kdata.x = MAX(0, xres2/2 - kdata.width/2);
        kdata.y = 0;
        break;

        case GWY_MERGE_DIRECTION_RIGHT:
        cdata.x = xres1 - (xres1/2);
        cdata.y = 0;
        cdata.width = xres1/2;
        cdata.height = yres1;
        kdata.width = MIN(xres2, cdata.width/3);
        kdata.height = MIN(yres2, cdata.height/2);
        kdata.x = 0;
        kdata.y = MAX(0, yres2/2 - kdata.height/2);
        break;

        case GWY_MERGE_DIRECTION_LEFT:
        cdata.x = 0;
        cdata.y = 0;
        cdata.width = xres1/2;
        cdata.height = yres1;
        kdata.width = MIN(xres2, cdata.width/3);
        kdata.height = MIN(yres2, cdata.height/2);
        kdata.x = MAX(0, xres2 - cdata.width/3);
        kdata.y = MAX(0, yres2/2 - kdata.height/2);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    correlation_data = gwy_data_field_area_extract(dfield1,
                                                   cdata.x,
                                                   cdata.y,
                                                   cdata.width,
                                                   cdata.height);
    correlation_kernel = gwy_data_field_area_extract(dfield2,
                                                     kdata.x,
                                                     kdata.y,
                                                     kdata.width,
                                                     kdata.height);
    correlation_score = gwy_data_field_new_alike(correlation_data, FALSE);

    /* get appropriate correlation score */
    if (!get_score_iteratively(correlation_data, correlation_kernel,
                               correlation_score, args))
        goto end;

    find_score_maximum(correlation_score, &max_col, &max_row);
    gwy_debug("c: %d %d %dx%d  k: %d %d %dx%d res: %d %d",
              cdata.x,
              cdata.y,
              cdata.width,
              cdata.height,
              kdata.x,
              kdata.y,
              kdata.width,
              kdata.height,
              max_col, max_row);

    px1 = 0;
    px2 = (max_col - (kdata.width-1)/2) + cdata.x - kdata.x;
    py1 = 0;
    py2 = (max_row - (kdata.height-1)/2) + cdata.y - kdata.y;
    if (px2 < 0) {
        px1 = -px2;
        px2 = 0;
    }
    if (py2 < 0) {
        py1 = -py2;
        py2 = 0;
    }

    create_merged_field(args->op1.data, args->op1.id,
                        dfield1, dfield2,
                        px1, py1, px2, py2,
                        real_boundary, real_dir,
                        args->create_mask, args->crop_to_rectangle);

end:
    g_object_unref(correlation_data);
    g_object_unref(correlation_kernel);
    g_object_unref(correlation_score);
}

static void
merge_do_join(MergeArgs *args)
{
    GwyDataField *dfield1, *dfield2;
    gint xres1, xres2, yres1, yres2;
    gint maxover, i, off = 0;
    gint px1, py1, px2, py2;
    gdouble s, smin = G_MAXDOUBLE;
    GwyMergeBoundaryType real_boundary = args->boundary;
    GwyMergeDirectionType real_dir = args->direction;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(args->op1.id);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object(args->op1.data, quark));

    quark = gwy_app_get_data_key_for_id(args->op2.id);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object(args->op2.data, quark));

    /* Reduce joining to two cases. */
    if (args->direction == GWY_MERGE_DIRECTION_UP
        || args->direction == GWY_MERGE_DIRECTION_LEFT) {
        GWY_SWAP(GwyDataField*, dfield1, dfield2);

        if (args->direction == GWY_MERGE_DIRECTION_UP)
            real_dir = GWY_MERGE_DIRECTION_DOWN;
        else if (args->direction == GWY_MERGE_DIRECTION_LEFT)
            real_dir = GWY_MERGE_DIRECTION_RIGHT;
        else {
            g_assert_not_reached();
        }

        if (args->boundary == GWY_MERGE_BOUNDARY_FIRST)
            real_boundary = GWY_MERGE_BOUNDARY_SECOND;
        else if (args->boundary == GWY_MERGE_BOUNDARY_SECOND)
            real_boundary = GWY_MERGE_BOUNDARY_FIRST;
    }

    xres1 = gwy_data_field_get_xres(dfield1);
    yres1 = gwy_data_field_get_yres(dfield1);
    xres2 = gwy_data_field_get_xres(dfield2);
    yres2 = gwy_data_field_get_yres(dfield2);

    if (real_dir == GWY_MERGE_DIRECTION_DOWN) {
        g_return_if_fail(xres1 == xres2);
        maxover = 2*MIN(yres1, yres2)/5;
        for (i = 1; i <= maxover; i++) {
            s = get_row_difference(dfield1, 0, yres1 - i,
                                   dfield2, 0, 0,
                                   xres1, i);
            if (s < smin) {
                off = i;
                smin = s;
            }
        }
        /* Turn one-pixel overlap to no overlap. */
        if (off == 1)
            off = 0;
        px1 = px2 = 0;
        py1 = 0;
        py2 = yres1 - off;
    }
    else if (real_dir == GWY_MERGE_DIRECTION_RIGHT) {
        g_return_if_fail(yres1 == yres2);
        maxover = 2*MIN(xres1, xres2)/5;
        for (i = 1; i <= maxover; i++) {
            s = get_column_difference(dfield1, xres1 - i, 0,
                                      dfield2, 0, 0,
                                      i, yres1);
            if (s < smin) {
                off = i;
                smin = s;
            }
        }
        /* Turn one-pixel overlap to no overlap. */
        if (off == 1)
            off = 0;
        py1 = py2 = 0;
        px1 = 0;
        px2 = xres1 - off;
    }
    else {
        g_assert_not_reached();
    }

    create_merged_field(args->op1.data, args->op1.id,
                        dfield1, dfield2,
                        px1, py1, px2, py2,
                        real_boundary, real_dir,
                        FALSE, FALSE);
}

static void
merge_do_none(MergeArgs *args)
{
    GwyDataField *dfield1, *dfield2;
    gint xres1, xres2, yres1, yres2;
    gint px1, py1, px2, py2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(args->op1.id);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object(args->op1.data, quark));

    quark = gwy_app_get_data_key_for_id(args->op2.id);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object(args->op2.data, quark));

    xres1 = gwy_data_field_get_xres(dfield1);
    xres2 = gwy_data_field_get_xres(dfield2);
    yres1 = gwy_data_field_get_yres(dfield1);
    yres2 = gwy_data_field_get_yres(dfield2);

    if (args->direction == GWY_MERGE_DIRECTION_UP) {
        px1 = px2 = 0;
        py1 = yres2;
        py2 = 0;
    }
    else if (args->direction == GWY_MERGE_DIRECTION_DOWN) {
        px1 = px2 = 0;
        py1 = 0;
        py2 = yres1;
    }
    else if (args->direction == GWY_MERGE_DIRECTION_LEFT) {
        py1 = py2 = 0;
        px1 = xres2;
        px2 = 0;
    }
    else if (args->direction == GWY_MERGE_DIRECTION_RIGHT) {
        py1 = py2 = 0;
        px1 = 0;
        px2 = xres1;
    }
    else {
        g_assert_not_reached();
    }

    create_merged_field(args->op1.data, args->op1.id,
                        dfield1, dfield2,
                        px1, py1, px2, py2,
                        args->boundary, args->direction,
                        args->create_mask, args->crop_to_rectangle);
}

static void
create_merged_field(GwyContainer *data, gint id1,
                    GwyDataField *dfield1, GwyDataField *dfield2,
                    gint px1, gint py1, gint px2, gint py2,
                    GwyMergeBoundaryType boundary, GwyMergeDirectionType dir,
                    gboolean create_mask, gboolean crop_to_rectangle)
{
    GwyDataField *result, *outsidemask = NULL;
    gint newxres, newyres, newid;

    gwy_debug("field1 %dx%d", dfield1->xres, dfield1->yres);
    gwy_debug("field2 %dx%d", dfield2->xres, dfield2->yres);
    gwy_debug("px1: %d, py1: %d, px2: %d, py2: %d", px1, py1, px2, py2);

    result = gwy_data_field_new_alike(dfield1, FALSE);

    newxres = MAX(dfield1->xres + px1, dfield2->xres + px2);
    newyres = MAX(dfield1->yres + py1, dfield2->yres + py2);

    gwy_data_field_resample(result, newxres, newyres, GWY_INTERPOLATION_NONE);
    if (create_mask && !crop_to_rectangle) {
        outsidemask = gwy_data_field_new_alike(result, FALSE);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(outsidemask),
                                    NULL);
    }
    put_fields(dfield1, dfield2, result, outsidemask,
               boundary, px1, py1, px2, py2);

    if (crop_to_rectangle) {
        GwyOrientation orientation = GWY_ORIENTATION_HORIZONTAL;
        if (dir == GWY_MERGE_DIRECTION_UP || dir == GWY_MERGE_DIRECTION_DOWN)
            orientation = GWY_ORIENTATION_VERTICAL;

        crop_result(result, dfield1, dfield2, orientation, px1, py1, px2, py2);
    }

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
    gwy_app_set_data_field_title(data, newid, _("Merged images"));
    gwy_app_sync_data_items(data, data, id1, newid,
                            FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);

    if (outsidemask) {
        if (gwy_data_field_get_max(outsidemask) > 0.0) {
            GQuark quark = gwy_app_get_mask_key_for_id(newid);
            gwy_container_set_object(data, quark, outsidemask);
        }
        g_object_unref(outsidemask);
    }

    gwy_app_channel_log_add_proc(data, -1, newid);
    g_object_unref(result);
}

static void
put_fields(GwyDataField *dfield1, GwyDataField *dfield2,
           GwyDataField *result, GwyDataField *outsidemask,
           GwyMergeBoundaryType boundary,
           gint px1, gint py1,
           gint px2, gint py2)
{
    GwyRectangle res_rect;
    GwyCoord f1_pos;
    GwyCoord f2_pos;
    gint w1, w2, h1, h2;
    gdouble xreal, yreal;

    gwy_debug("field1 %dx%d", dfield1->xres, dfield1->yres);
    gwy_debug("field2 %dx%d", dfield2->xres, dfield2->yres);
    gwy_debug("result %dx%d", result->xres, result->yres);
    gwy_debug("px1: %d, py1: %d, px2: %d, py2: %d", px1, py1, px2, py2);

    gwy_data_field_fill(result,
                        MIN(gwy_data_field_get_min(dfield1),
                            gwy_data_field_get_min(dfield2)));

    w1 = gwy_data_field_get_xres(dfield1);
    h1 = gwy_data_field_get_yres(dfield1);
    w2 = gwy_data_field_get_xres(dfield2);
    h2 = gwy_data_field_get_yres(dfield2);

    if (boundary == GWY_MERGE_BOUNDARY_SECOND) {
        gwy_data_field_area_copy(dfield1, result, 0, 0, w1, h1, px1, py1);
        gwy_data_field_area_copy(dfield2, result, 0, 0, w2, h2, px2, py2);
    }
    else {
        gwy_data_field_area_copy(dfield2, result, 0, 0, w2, h2, px2, py2);
        gwy_data_field_area_copy(dfield1, result, 0, 0, w1, h1, px1, py1);
    }

    if (outsidemask) {
        gwy_data_field_fill(outsidemask, 1.0);
        gwy_data_field_area_clear(outsidemask, px1, py1, w1, h1);
        gwy_data_field_area_clear(outsidemask, px2, py2, w2, h2);
    }

    /* adjust boundary to be as smooth as possible */
    if (boundary == GWY_MERGE_BOUNDARY_AVERAGE
        || boundary == GWY_MERGE_BOUNDARY_INTERPOLATE) {
        if (px1 < px2) {
            res_rect.x = px2;
            res_rect.width = px1 + w1 - px2;
        }
        else {
            res_rect.x = px1;
            res_rect.width = px2 + w2 - px1;
        }

        if (py1 < py2) {
            res_rect.y = py2;
            res_rect.height = py1 + h1 - py2;
        }
        else {
            res_rect.y = py1;
            res_rect.height = py2 + h2 - py1;
        }

        res_rect.height = MIN(res_rect.height, MIN(h1, h2));
        res_rect.width = MIN(res_rect.width, MIN(w1, w2));

        /* This is where the result rectangle is positioned in the fields,
         * not where the fields themselves are placed! */
        f1_pos.x = res_rect.x - px1;
        f1_pos.y = res_rect.y - py1;
        f2_pos.x = res_rect.x - px2;
        f2_pos.y = res_rect.y - py2;

        merge_boundary(dfield1, dfield2, result, res_rect, f1_pos, f2_pos,
                       boundary);
    }

    /* Use the pixels sizes of field 1 -- they must be identical. */
    xreal = result->xres * gwy_data_field_get_xmeasure(dfield1);
    yreal = result->yres * gwy_data_field_get_ymeasure(dfield1);
    gwy_data_field_set_xreal(result, xreal);
    gwy_data_field_set_yreal(result, yreal);
    if (outsidemask) {
        gwy_data_field_set_xreal(outsidemask, xreal);
        gwy_data_field_set_yreal(outsidemask, yreal);
    }
}

static void
crop_result(GwyDataField *result,
            GwyDataField *dfield1, GwyDataField *dfield2,
            GwyOrientation orientation,
            gint px1, gint py1,
            gint px2, gint py2)
{
    if (orientation == GWY_ORIENTATION_HORIZONTAL) {
        gint top = MAX(MAX(py1, py2), 0);
        gint bot = MIN(MIN(dfield1->yres + py1, dfield2->yres + py2),
                       result->yres);
        gdouble yreal = (bot - top)*gwy_data_field_get_ymeasure(result);
        g_return_if_fail(bot > top);
        gwy_data_field_resize(result, 0, top, result->xres, bot);
        gwy_data_field_set_yreal(result, yreal);
    }
    else {
        gint left = MAX(MAX(px1, px2), 0);
        gint right = MIN(MIN(dfield1->xres + px1, dfield2->xres + px2),
                         result->xres);
        gdouble xreal = (right - left)*gwy_data_field_get_xmeasure(result);
        g_return_if_fail(right > left);
        gwy_data_field_resize(result, left, 0, right, result->yres);
        gwy_data_field_set_xreal(result, xreal);
    }
}

/* Note this is not a correlation score since we care also about absolute
 * differences and try to suppress the influence of outliers. */
static gdouble
get_row_difference(GwyDataField *dfield1,
                   gint col1,
                   gint row1,
                   GwyDataField *dfield2,
                   gint col2,
                   gint row2,
                   gint width,
                   gint height)
{
    gint xres1, yres1, xres2, yres2, i, j;
    const gdouble *data1, *data2;
    gdouble *row;
    gdouble s = 0.0;

    g_return_val_if_fail(width > 0, G_MAXDOUBLE);
    g_return_val_if_fail(height > 0, G_MAXDOUBLE);

    xres1 = gwy_data_field_get_xres(dfield1);
    yres1 = gwy_data_field_get_yres(dfield1);
    xres2 = gwy_data_field_get_xres(dfield2);
    yres2 = gwy_data_field_get_yres(dfield2);
    data1 = gwy_data_field_get_data_const(dfield1);
    data2 = gwy_data_field_get_data_const(dfield2);

    g_return_val_if_fail(col1 + width <= xres1, G_MAXDOUBLE);
    g_return_val_if_fail(col2 + width <= xres2, G_MAXDOUBLE);
    g_return_val_if_fail(row1 + height <= yres1, G_MAXDOUBLE);
    g_return_val_if_fail(row2 + height <= yres2, G_MAXDOUBLE);

    row = g_new(gdouble, width);

    for (i = 0; i < height; i++) {
        const gdouble *d1 = data1 + (row1 + i)*xres1 + col1;
        const gdouble *d2 = data2 + (row2 + i)*xres2 + col2;
        gdouble d;

        for (j = 0; j < width; j++) {
            d = d1[j] - d2[j];
            row[j] = d;
        }
        d = gwy_math_median(width, row);
        s += d*d;
    }

    g_free(row);

    return sqrt(s/height);
}

static gdouble
get_column_difference(GwyDataField *dfield1,
                      gint col1,
                      gint row1,
                      GwyDataField *dfield2,
                      gint col2,
                      gint row2,
                      gint width,
                      gint height)
{
    gint xres1, yres1, xres2, yres2, i, j;
    const gdouble *data1, *data2;
    gdouble *column;
    gdouble s = 0.0;

    g_return_val_if_fail(width > 0, G_MAXDOUBLE);
    g_return_val_if_fail(height > 0, G_MAXDOUBLE);

    xres1 = gwy_data_field_get_xres(dfield1);
    yres1 = gwy_data_field_get_yres(dfield1);
    xres2 = gwy_data_field_get_xres(dfield2);
    yres2 = gwy_data_field_get_yres(dfield2);
    data1 = gwy_data_field_get_data_const(dfield1);
    data2 = gwy_data_field_get_data_const(dfield2);

    g_return_val_if_fail(col1 + width <= xres1, G_MAXDOUBLE);
    g_return_val_if_fail(col2 + width <= xres2, G_MAXDOUBLE);
    g_return_val_if_fail(row1 + height <= yres1, G_MAXDOUBLE);
    g_return_val_if_fail(row2 + height <= yres2, G_MAXDOUBLE);

    column = g_new(gdouble, height);

    for (j = 0; j < width; j++) {
        const gdouble *d1 = data1 + row1*xres1 + col1 + j;
        const gdouble *d2 = data2 + row2*xres2 + col2 + j;
        gdouble d;

        for (i = 0; i < height; i++) {
            d = d1[i*xres1] - d2[i*xres2];
            column[i] = d;
        }
        d = gwy_math_median(height, column);
        s += d*d;
    }

    g_free(column);

    return sqrt(s/height);
}

/* compute corelation */
static gboolean
get_score_iteratively(GwyDataField *data_field, GwyDataField *kernel_field,
                      GwyDataField *score, MergeArgs *args)
{
    enum { WORK_PER_UPDATE = 50000000 };
    GwyComputationState *state;
    gboolean ok = FALSE;
    int work, wpi;

    work = 0;
    wpi = gwy_data_field_get_xres(kernel_field)
          *gwy_data_field_get_yres(kernel_field);
    wpi = MIN(wpi, WORK_PER_UPDATE);
    state = gwy_data_field_correlate_init(data_field, kernel_field, score);

    /* FIXME */
    gwy_app_wait_start(gwy_app_find_window_for_channel(args->op1.data,
                                                       args->op1.id),
                       _("Initializing"));
    gwy_data_field_correlate_iteration(state);
    if (!gwy_app_wait_set_message(_("Correlating")))
        goto get_score_fail;
    do {
        gwy_data_field_correlate_iteration(state);
        work += wpi;
        if (work > WORK_PER_UPDATE) {
            work -= WORK_PER_UPDATE;
            if (!gwy_app_wait_set_fraction(state->fraction))
                goto get_score_fail;
        }
    } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
    ok = TRUE;

get_score_fail:
    gwy_data_field_correlate_finalize(state);
    gwy_app_wait_finish();

    return ok;
}

static void
find_score_maximum(GwyDataField *correlation_score,
                   gint *max_col,
                   gint *max_row)
{
    gint i, n, maxi = 0;
    gdouble max = -G_MAXDOUBLE;
    const gdouble *data;

    n = gwy_data_field_get_xres(correlation_score)
        *gwy_data_field_get_yres(correlation_score);
    data = gwy_data_field_get_data_const(correlation_score);

    for (i = 0; i < n; i++) {
        if (max < data[i]) {
            max = data[i];
            maxi = i;
        }
    }

    *max_row = (gint)floor(maxi/gwy_data_field_get_xres(correlation_score));
    *max_col = maxi - (*max_row)*gwy_data_field_get_xres(correlation_score);
}

static inline gboolean
gwy_data_field_inside(GwyDataField *data_field, gint i, gint j)
{
    if (i >= 0
        && j >= 0
        && i < gwy_data_field_get_xres(data_field)
        && j < gwy_data_field_get_yres(data_field))
        return TRUE;
    else
        return FALSE;
}

static void
assign_edge(gint edgepos, gint pos1, gint pos2, gint *w1, gint *w2)
{
    gboolean onedge1 = (pos1 == edgepos);
    gboolean onedge2 = (pos2 == edgepos);

    g_assert(onedge1 || onedge2);
    *w1 = onedge2;
    *w2 = onedge1;
}

static void
merge_boundary(GwyDataField *dfield1,
               GwyDataField *dfield2,
               GwyDataField *result,
               GwyRectangle res_rect,
               GwyCoord f1_pos,
               GwyCoord f2_pos,
               GwyMergeBoundaryType boundary)
{
    gint xres1, xres2, xres, yres1, yres2, col, row;
    gdouble weight, val1, val2;
    gint w1top = 0, w1bot = 0, w1left = 0, w1right = 0;
    gint w2top = 0, w2bot = 0, w2left = 0, w2right = 0;
    const gdouble *d1, *d2;
    gdouble *d;

    xres1 = dfield1->xres;
    yres1 = dfield1->yres;
    xres2 = dfield2->xres;
    yres2 = dfield2->yres;
    xres = result->xres;

    gwy_debug("dfield1: %d x %d at (%d, %d)",
              xres1, yres1, f1_pos.x, f1_pos.y);
    gwy_debug("dfield2: %d x %d at (%d, %d)",
              xres2, yres2, f2_pos.x, f2_pos.y);
    gwy_debug("result: %d x %d", xres, result->yres);
    gwy_debug("rect in result : %d x %d at (%d,%d)",
              res_rect.width, res_rect.height, res_rect.x, res_rect.y);

    assign_edge(res_rect.x, f1_pos.x, f2_pos.x, &w1left, &w2left);
    gwy_debug("left: %d %d", w1left, w2left);
    assign_edge(res_rect.y, f1_pos.y, f2_pos.y, &w1top, &w2top);
    gwy_debug("top: %d %d", w1top, w2top);
    assign_edge(res_rect.width,
                xres1 - f1_pos.x, xres2 - f2_pos.x,
                &w1right, &w2right);
    gwy_debug("right: %d %d", w1right, w2right);
    assign_edge(res_rect.height,
                yres1 - f1_pos.y, yres2 - f2_pos.y,
                &w1bot, &w2bot);
    gwy_debug("bot: %d %d", w1bot, w2bot);

    d1 = gwy_data_field_get_data_const(dfield1);
    d2 = gwy_data_field_get_data_const(dfield2);
    d = gwy_data_field_get_data(result);

    for (row = 0; row < res_rect.height; row++) {
        gint dtop = row + 1, dbot = res_rect.height - row;
        for (col = 0; col < res_rect.width; col++) {
            weight = 0.5;
            if (boundary == GWY_MERGE_BOUNDARY_INTERPOLATE) {
                gint dleft = col + 1, dright = res_rect.width - col;
                gint d1min = G_MAXINT, d2min = G_MAXINT;
                /* FIXME: This can be probably simplified... */
                if (w1top && dtop < d1min)
                    d1min = dtop;
                if (w1bot && dbot < d1min)
                    d1min = dbot;
                if (w1left && dleft < d1min)
                    d1min = dleft;
                if (w1right && dright < d1min)
                    d1min = dright;
                if (w2top && dtop < d2min)
                    d2min = dtop;
                if (w2bot && dbot < d2min)
                    d2min = dbot;
                if (w2left && dleft < d2min)
                    d2min = dleft;
                if (w2right && dright < d2min)
                    d2min = dright;

                weight = (gdouble)d2min/(d1min + d2min);
            }
            val1 = d1[xres1*(row + f1_pos.y) + (col + f1_pos.x)];
            val2 = d2[xres2*(row + f2_pos.y) + (col + f2_pos.x)];
            d[xres*(row + res_rect.y) + col + res_rect.x]
                = (1.0 - weight)*val1 + weight*val2;
        }
    }
}

static const gchar boundary_key[]          = "/module/merge/boundary";
static const gchar create_mask_key[]       = "/module/merge/create_mask";
static const gchar crop_to_rectangle_key[] = "/module/merge/crop_to_rectangle";
static const gchar direction_key[]         = "/module/merge/direction";
static const gchar mode_key[]              = "/module/merge/mode";

static void
merge_sanitize_args(MergeArgs *args)
{
    args->direction = MIN(args->direction, GWY_MERGE_DIRECTION_LAST - 1);
    args->mode = MIN(args->mode, GWY_MERGE_MODE_LAST - 1);
    args->boundary = MIN(args->boundary, GWY_MERGE_BOUNDARY_LAST - 1);
    args->create_mask = !!args->create_mask;
    args->crop_to_rectangle = !!args->crop_to_rectangle;
}

static void
merge_load_args(GwyContainer *settings,
                MergeArgs *args)
{
    *args = merge_defaults;
    gwy_container_gis_enum_by_name(settings, direction_key, &args->direction);
    gwy_container_gis_enum_by_name(settings, mode_key, &args->mode);
    gwy_container_gis_enum_by_name(settings, boundary_key, &args->boundary);
    gwy_container_gis_boolean_by_name(settings, create_mask_key,
                                      &args->create_mask);
    gwy_container_gis_boolean_by_name(settings, crop_to_rectangle_key,
                                      &args->crop_to_rectangle);
    merge_sanitize_args(args);
}

static void
merge_save_args(GwyContainer *settings,
                MergeArgs *args)
{
    gwy_container_set_enum_by_name(settings, direction_key, args->direction);
    gwy_container_set_enum_by_name(settings, mode_key, args->mode);
    gwy_container_set_enum_by_name(settings, boundary_key, args->boundary);
    gwy_container_set_boolean_by_name(settings, create_mask_key,
                                      args->create_mask);
    gwy_container_set_boolean_by_name(settings, crop_to_rectangle_key,
                                      args->crop_to_rectangle);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
