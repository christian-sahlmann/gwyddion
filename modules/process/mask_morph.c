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
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/elliptic.h>
#include <libprocess/filters.h>
#include <libprocess/grains.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define MASKMORPH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)
#define MASKTHIN_RUN_MODES GWY_RUN_IMMEDIATE

typedef enum {
    MASKMORPH_EROSION     = 0,
    MASKMORPH_DILATION    = 1,
    MASKMORPH_OPENING     = 2,
    MASKMORPH_CLOSING     = 3,
    MASKMORPH_ASF_OPENING = 4,
    MASKMORPH_ASF_CLOSING = 5,
    MASKMORPH_NOPERATIONS
} MaskMorphOperation;

typedef enum {
    MASKMORPH_USER_KERNEL = 0,
    MASKMORPH_DISC        = 1,
    MASKMORPH_OCTAGON     = 2,
    MASKMORPH_SQUARE      = 3,
    MASKMORPH_DIAMOND     = 4,
    MASKMORPH_NSHAPES
} MaskMorphShapeType;

typedef struct {
    MaskMorphOperation mode;
    MaskMorphShapeType shape;
    gint radius;
    gboolean crop_kernel;
    GwyAppDataId kernel;
} MaskMorphArgs;

typedef struct {
    MaskMorphArgs *args;
    GtkWidget *dialog;
    GSList *mode;
    GSList *shape;
    GtkObject *radius;
    GtkWidget *kernel;
    GtkWidget *crop_kernel;
} MaskMorphControls;

static gboolean module_register        (void);
static void     mask_morph             (GwyContainer *data,
                                        GwyRunType run);
static gboolean maskmorph_dialog       (MaskMorphArgs *args,
                                        GwyDataField *mask);
static void     mode_changed           (GtkToggleButton *toggle,
                                        MaskMorphControls *controls);
static void     shape_changed          (GtkToggleButton *toggle,
                                        MaskMorphControls *controls);
static void     radius_changed         (GtkAdjustment *adj,
                                        MaskMorphControls *controls);
static void     kernel_changed         (GwyDataChooser *chooser,
                                        MaskMorphControls *controls);
static void     crop_kernel_changed    (GtkToggleButton *toggle,
                                        MaskMorphControls *controls);
static void     update_sensitivity     (MaskMorphControls *controls);
static gboolean kernel_filter          (GwyContainer *data,
                                        gint id,
                                        gpointer user_data);
static void     maskmorph_do           (GwyDataField *mfield,
                                        MaskMorphArgs *args);
static void     maskmorph_sanitize_args(MaskMorphArgs *args);
static void     maskmorph_load_args    (GwyContainer *settings,
                                        MaskMorphArgs *args);
static void     maskmorph_save_args    (GwyContainer *settings,
                                        MaskMorphArgs *args);

static const MaskMorphArgs maskmorph_defaults = {
    MASKMORPH_OPENING,
    MASKMORPH_DISC,
    5,
    TRUE,
    GWY_APP_DATA_ID_NONE,
};

static GwyAppDataId kernel_id = GWY_APP_DATA_ID_NONE;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Performs basic morphological operations with masks."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("mask_morph",
                              (GwyProcessFunc)&mask_morph,
                              N_("/_Mask/Morphological Operation..."),
                              GWY_STOCK_MASK_MORPH,
                              MASKMORPH_RUN_MODES,
                              GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA,
                              N_("Morphological operation with mask"));

    return TRUE;
}

static void
mask_morph(GwyContainer *data, GwyRunType run)
{
    GwyDataField *mfield;
    MaskMorphArgs args;
    GQuark quark;
    gint id;

    g_return_if_fail(run & MASKMORPH_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_MASK_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(mfield);

    maskmorph_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_IMMEDIATE || maskmorph_dialog(&args, mfield)) {
        gwy_app_undo_qcheckpointv(data, 1, &quark);
        maskmorph_do(mfield, &args);
        gwy_data_field_data_changed(mfield);
        gwy_app_channel_log_add_proc(data, id, id);
    }
    maskmorph_save_args(gwy_app_settings_get(), &args);
}

static gboolean
maskmorph_dialog(MaskMorphArgs *args, GwyDataField *mask)
{
    static const GwyEnum modes[] = {
        { N_("Erosion"),        MASKMORPH_EROSION,     },
        { N_("Dilation"),       MASKMORPH_DILATION,    },
        { N_("filter|Opening"), MASKMORPH_OPENING,     },
        { N_("filter|Closing"), MASKMORPH_CLOSING,     },
        { N_("ASF Opening"),    MASKMORPH_ASF_OPENING, },
        { N_("ASF Closing"),    MASKMORPH_ASF_CLOSING, },
    };
    static const GwyEnum shapes[] = {
        { N_("Disc"),         MASKMORPH_DISC,        },
        { N_("Octagon"),      MASKMORPH_OCTAGON,     },
        { N_("Square"),       MASKMORPH_SQUARE,      },
        { N_("Diamond"),      MASKMORPH_DIAMOND,     },
        { N_("Another mask"), MASKMORPH_USER_KERNEL, },
    };

    MaskMorphControls controls;
    GtkWidget *dialog;
    GtkWidget *table, *label;
    gint response, row = 0;
    gboolean has_kernel;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Morphological Operation"),
                                         NULL, 0,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK,
                                         GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    table = gtk_table_new(MASKMORPH_NOPERATIONS + MASKMORPH_NSHAPES + 5,
                          4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    label = gtk_label_new(_("Operation:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.mode
        = gwy_radio_buttons_create(modes, G_N_ELEMENTS(modes),
                                   G_CALLBACK(mode_changed), &controls,
                                   args->mode);
    row = gwy_radio_buttons_attach_to_table(controls.mode, GTK_TABLE(table),
                                            3, row);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new(_("Structuring element:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.shape
        = gwy_radio_buttons_create(shapes, G_N_ELEMENTS(shapes),
                                   G_CALLBACK(shape_changed), &controls,
                                   args->shape);
    row = gwy_radio_buttons_attach_to_table(controls.shape, GTK_TABLE(table),
                                            3, row);

    controls.radius = gtk_adjustment_new(args->radius, 1, 1025, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Radius:"), "px", controls.radius,
                            GWY_HSCALE_SQRT);
    g_signal_connect(controls.radius, "value-changed",
                     G_CALLBACK(radius_changed), &controls);
    row++;

    controls.kernel = gwy_data_chooser_new_channels();
    gwy_data_chooser_set_active_id(GWY_DATA_CHOOSER(controls.kernel),
                                   &args->kernel);
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(controls.kernel),
                                &kernel_filter, mask, NULL);
    gwy_table_attach_hscale(table, row, _("_Mask:"), NULL,
                            GTK_OBJECT(controls.kernel), GWY_HSCALE_WIDGET);
    has_kernel = gwy_data_chooser_get_active_id(GWY_DATA_CHOOSER(controls.kernel),
                                                &args->kernel);
    g_signal_connect(controls.kernel, "changed",
                     G_CALLBACK(kernel_changed), &controls);
    row++;

    controls.crop_kernel
        = gtk_check_button_new_with_mnemonic(_("_Trim empty borders"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.crop_kernel),
                                 args->crop_kernel);
    gtk_table_attach(GTK_TABLE(table), controls.crop_kernel,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.crop_kernel, "toggled",
                     G_CALLBACK(crop_kernel_changed), &controls);

    if (!has_kernel && args->shape == MASKMORPH_USER_KERNEL)
        gwy_radio_buttons_set_current(controls.shape, MASKMORPH_DISC);
    update_sensitivity(&controls);

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
mode_changed(GtkToggleButton *toggle,
             MaskMorphControls *controls)
{
    MaskMorphArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(toggle))
        return;

    args->mode = gwy_radio_buttons_get_current(controls->mode);
    update_sensitivity(controls);
}

static void
shape_changed(GtkToggleButton *toggle,
              MaskMorphControls *controls)
{
    MaskMorphArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(toggle))
        return;

    args->shape = gwy_radio_buttons_get_current(controls->shape);
    update_sensitivity(controls);
}

static void
radius_changed(GtkAdjustment *adj,
               MaskMorphControls *controls)
{
    MaskMorphArgs *args = controls->args;

    args->radius = gwy_adjustment_get_int(adj);
    update_sensitivity(controls);
}

static void
kernel_changed(GwyDataChooser *chooser,
               MaskMorphControls *controls)
{
    MaskMorphArgs *args = controls->args;

    gwy_data_chooser_get_active_id(chooser, &args->kernel);
}

static void
crop_kernel_changed(GtkToggleButton *toggle,
                    MaskMorphControls *controls)
{
    MaskMorphArgs *args = controls->args;
    args->crop_kernel = gtk_toggle_button_get_active(toggle);
}

static void
update_sensitivity(MaskMorphControls *controls)
{
    MaskMorphArgs *args = controls->args;
    gboolean is_user_kernel = (args->shape == MASKMORPH_USER_KERNEL);
    gboolean needs_builtin = (args->mode == MASKMORPH_ASF_OPENING
                              || args->mode == MASKMORPH_ASF_CLOSING);
    gboolean has_kernel;

    if (needs_builtin && is_user_kernel) {
        gwy_radio_buttons_set_current(controls->shape, MASKMORPH_DISC);
        return;
    }
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->radius),
                                   !is_user_kernel);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->kernel),
                                   is_user_kernel);
    gtk_widget_set_sensitive(controls->crop_kernel, is_user_kernel);
    gtk_widget_set_sensitive(gwy_radio_buttons_find(controls->shape,
                                                    MASKMORPH_USER_KERNEL),
                             !needs_builtin);

    has_kernel
        = !!gwy_data_chooser_get_active(GWY_DATA_CHOOSER(controls->kernel),
                                        NULL);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      GTK_RESPONSE_OK,
                                      !is_user_kernel || has_kernel);
}

static GwyDataField*
create_kernel(MaskMorphShapeType shape, gint radius)
{
    GwyDataField *kernel;
    gint i, j, res;
    gdouble *d;

    res = 2*radius + 1;
    kernel = gwy_data_field_new(res, res, res, res, TRUE);
    if (shape == MASKMORPH_DISC)
        gwy_data_field_elliptic_area_fill(kernel, 0, 0, res, res, 1.0);
    else if (shape == MASKMORPH_OCTAGON || shape == MASKMORPH_DIAMOND) {
        gint rlim = (shape == MASKMORPH_OCTAGON
                     ? GWY_ROUND(res/G_SQRT2)
                     : radius);
        d = gwy_data_field_get_data(kernel);
        for (i = 0; i < res; i++) {
            gint ii = ABS(i - radius);
            for (j = 0; j < res; j++) {
                gint jj = ABS(j - radius);
                if (ii + jj <= rlim)
                    d[i*res + j] = 1.0;
            }
        }
    }
    else if (shape == MASKMORPH_SQUARE)
        gwy_data_field_fill(kernel, 1.0);
    else {
        g_assert_not_reached();
    }

    return kernel;
}

static void
maskmorph_do(GwyDataField *mask, MaskMorphArgs *args)
{
    static struct {
        GwyMinMaxFilterType filtertype;
        MaskMorphOperation mode;
    }
    operation_map[] = {
        { GWY_MIN_MAX_FILTER_EROSION,  MASKMORPH_EROSION,  },
        { GWY_MIN_MAX_FILTER_DILATION, MASKMORPH_DILATION, },
        { GWY_MIN_MAX_FILTER_OPENING,  MASKMORPH_OPENING,  },
        { GWY_MIN_MAX_FILTER_CLOSING,  MASKMORPH_CLOSING,  },
    };

    gint xres = gwy_data_field_get_xres(mask);
    gint yres = gwy_data_field_get_yres(mask);
    MaskMorphOperation mode = args->mode;
    GwyMinMaxFilterType filtertype1, filtertype2;
    GwyDataField *kernel;
    guint i, radius = args->radius;
    GwyContainer *kdata;
    GQuark quark;

    for (i = 0; i < G_N_ELEMENTS(operation_map); i++) {
        if (operation_map[i].mode != mode)
            continue;

        if (args->shape == MASKMORPH_USER_KERNEL) {
            if (!args->kernel.datano)
                return;

            kdata = gwy_app_data_browser_get(args->kernel.datano);
            g_return_if_fail(kdata);
            quark = gwy_app_get_mask_key_for_id(args->kernel.id);
            if (!gwy_container_gis_object(kdata, quark, (GObject**)&kernel))
                return;
            kernel = gwy_data_field_duplicate(kernel);
            if (args->crop_kernel)
                gwy_data_field_grains_autocrop(kernel, FALSE,
                                               NULL, NULL, NULL, NULL);
        }
        else
            kernel = create_kernel(args->shape, radius);

        gwy_data_field_area_filter_min_max(mask, kernel,
                                           operation_map[i].filtertype,
                                           0, 0, xres, yres);
        g_object_unref(kernel);
        return;
    }

    g_return_if_fail(mode == MASKMORPH_ASF_OPENING
                     || mode == MASKMORPH_ASF_CLOSING);

    /* We can get here by repeating the operation or module call. */
    if (args->shape == MASKMORPH_USER_KERNEL)
        return;

    if (args->shape == MASKMORPH_DISC) {
        gwy_data_field_area_filter_disc_asf(mask, radius,
                                            mode == MASKMORPH_ASF_CLOSING,
                                            0, 0, xres, yres);
        return;
    }

    if (mode == MASKMORPH_ASF_CLOSING) {
        filtertype1 = GWY_MIN_MAX_FILTER_OPENING;
        filtertype2 = GWY_MIN_MAX_FILTER_CLOSING;
    }
    else {
        filtertype1 = GWY_MIN_MAX_FILTER_CLOSING;
        filtertype2 = GWY_MIN_MAX_FILTER_OPENING;
    }

    for (i = 1; i <= radius; i++) {
        kernel = create_kernel(args->shape, i);
        gwy_data_field_area_filter_min_max(mask, kernel, filtertype1,
                                           0, 0, xres, yres);
        gwy_data_field_area_filter_min_max(mask, kernel, filtertype2,
                                           0, 0, xres, yres);
        g_object_unref(kernel);
    }
}

static gboolean
kernel_filter(GwyContainer *data,
              gint id,
              gpointer user_data)
{
    GwyDataField *kernel, *mask = (GwyDataField*)user_data;
    GQuark quark;

    quark = gwy_app_get_mask_key_for_id(id);
    if (!gwy_container_gis_object(data, quark, &kernel))
        return FALSE;

    if (kernel->xres > mask->xres/2
        || kernel->yres > mask->yres/2)
        return FALSE;

    return TRUE;
}

static const gchar crop_kernel_key[] = "/module/mask_morph/crop_kernel";
static const gchar mode_key[]        = "/module/mask_morph/mode";
static const gchar radius_key[]      = "/module/mask_morph/radius";
static const gchar shape_key[]       = "/module/mask_morph/shape";

static void
maskmorph_sanitize_args(MaskMorphArgs *args)
{
    args->mode = MIN(args->mode, MASKMORPH_NOPERATIONS-1);
    args->shape = MIN(args->shape, MASKMORPH_NSHAPES-1);
    args->radius = CLAMP(args->radius, 1, 1025);
    args->crop_kernel = !!args->crop_kernel;
    gwy_app_data_id_verify_channel(&args->kernel);
}

static void
maskmorph_load_args(GwyContainer *settings,
                    MaskMorphArgs *args)
{
    *args = maskmorph_defaults;
    gwy_container_gis_enum_by_name(settings, mode_key, &args->mode);
    gwy_container_gis_enum_by_name(settings, shape_key, &args->shape);
    gwy_container_gis_int32_by_name(settings, radius_key, &args->radius);
    gwy_container_gis_boolean_by_name(settings, crop_kernel_key,
                                      &args->crop_kernel);
    args->kernel = kernel_id;
    maskmorph_sanitize_args(args);
}

static void
maskmorph_save_args(GwyContainer *settings,
                    MaskMorphArgs *args)
{
    kernel_id = args->kernel;
    gwy_container_set_enum_by_name(settings, mode_key, args->mode);
    gwy_container_set_enum_by_name(settings, shape_key, args->shape);
    gwy_container_set_int32_by_name(settings, radius_key, args->radius);
    gwy_container_set_boolean_by_name(settings, crop_kernel_key,
                                      args->crop_kernel);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
