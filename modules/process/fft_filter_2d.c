/*
 *  $Id$
 *  Copyright (C) 2005 Christopher Anderson, Molecular Imaging Corp.
 *  E-mail: Chris Anderson (sidewinder.asu@gmail.com)
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/*TODO: Only allow square images */

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include <libprocess/arithmetic.h>
#include <libprocess/elliptic.h>
#include <libprocess/inttrans.h>
#include <libprocess/stats.h>

#define FFTF_2D_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define PREVIEW_SIZE 400.0

/* Convenience macros */
#define get_toggled(obj) \
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(obj))
#define set_toggled(obj, tog) \
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(obj), tog)

enum {
    FFT_ELLIPSE_ADD    = 0,
    FFT_RECT_ADD       = 1,
    FFT_ELLIPSE_SUB    = 2,
    FFT_RECT_SUB       = 3,
};

enum {
    PREV_FFT,
    PREV_IMAGE,
    PREV_FILTERED,
    PREV_DIFF,
};

enum {
    OUTPUT_FFT = 1,
    OUTPUT_IMAGE = 2
};

enum {
    SENS_EDIT = 1 << 0,
    SENS_UNDO = 1 << 1,
};

enum {
    RESPONSE_RESET,
};

typedef struct {
    GwyContainer    *mydata;
    GwyContainer    *data;

    GwySensitivityGroup *sensgroup;

    gulong          rect_signal;
    gulong            ellipse_signal;

    GtkWidget       *view;

    guint           edit_mode;
    GSList          *mode;

    guint           prev_mode;
    GSList          *pmode;

    gboolean        snap;
    gboolean        zoom;
    guint           out_mode;

    gboolean        compute;
} ControlsType;

typedef void (*FieldFillFunc)(GwyDataField*, gint, gint, gint, gint, gdouble);

/* Gwyddion Module Routines */
static gboolean     module_register     (void);
static void         run_main            (GwyContainer *data,
                                         GwyRunType run);

/* Signal handlers */
static void        selection_finished_cb (GwySelection *selection,
                                          ControlsType *controls);
static void        edit_mode_changed_cb  (ControlsType *controls);
static void        prev_mode_changed_cb  (ControlsType *controls);
static void        remove_all_cb         (ControlsType *controls);
static void        undo_cb               (ControlsType *controls);
static void        snap_cb               (GtkWidget *check,
                                          ControlsType *controls);

/* Helper Functions */
static gboolean        run_dialog          (ControlsType *controls);
static GwyDataField*   create_mask_field   (GwyDataField *dfield);
static GwyVectorLayer* create_vlayer       (guint new_mode);
static void               switch_layer        (guint new_mode,
                                            ControlsType *controls);
static void            set_layer_channel   (GwyPixmapLayer *layer,
                                            gint channel);
static void            do_fft              (GwyDataField *dataInput,
                                            GwyDataField *dataOutput);
static void            set_dfield_modulus  (GwyDataField *re,
                                            GwyDataField *im,
                                            GwyDataField *target);
static void            fft_filter_2d       (GwyDataField *input,
                                            GwyDataField *output_image,
                                            GwyDataField *output_fft,
                                            GwyDataField *mask);
static void            save_settings       (ControlsType *controls);
static void            load_settings       (ControlsType *controls);
static void            build_tooltips      (GHashTable *hash_tips);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("2D FFT Filtering"),
    "Chris Anderson <sidewinder.asu@gmail.com>",
    "1.1",
    "Chris Anderson, Molecular Imaging Corp.",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fft_filter_2d",
                              (GwyProcessFunc)&run_main,
                              N_("/_Correct Data/_2D FFT filtering..."),
                              NULL,
                              FFTF_2D_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Two-dimensional FFT filtering"));

    return TRUE;
}

static void
run_main(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield, *fft, *filtered, *diff;
    GwyDataField *out_image = NULL, *out_fft = NULL;
    gint id, newid;
    GwyRGBA rgba;
    ControlsType controls;
    gboolean response;

    guint xres, yres, col, row;
    gdouble factor = 2.0;
    GwyDataField *temp;


    g_return_if_fail(run & FFTF_2D_RUN_MODES);

    /* Initialize */
    controls.rect_signal = controls.ellipse_signal = 0;
    controls.edit_mode = FFT_ELLIPSE_ADD;
    controls.prev_mode = PREV_FFT;
    controls.out_mode = OUTPUT_FFT | OUTPUT_IMAGE;
    controls.snap = FALSE;
    controls.zoom = FALSE;
    controls.data = data;
    controls.compute = TRUE;
    load_settings(&controls);

    /*XXX: should the mask and presentation get carried through? */
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));

    /* Create datafields */
    fft = gwy_data_field_new_alike(dfield, FALSE);
    do_fft(dfield, fft);

    /*XXX hackish zoom method
    xres = gwy_data_field_get_xres(fft);
    yres = gwy_data_field_get_yres(fft);
    col = ((gdouble)xres / 2.0) * (1 - (1.0 / factor));
    row = ((gdouble)yres / 2.0) * (1 - (1.0 / factor));
    temp = gwy_data_field_area_extract(fft, col, row,
                                       (gdouble)xres / factor,
                                       (gdouble)yres / factor);
    g_object_unref(fft);
    fft = gwy_data_field_new_resampled(temp, xres, yres,
                                       GWY_INTERPOLATION_BSPLINE);
    */

    mfield = create_mask_field(fft);
    filtered = gwy_data_field_new_alike(dfield, TRUE);
    diff = gwy_data_field_new_alike(dfield, TRUE);

    /* Setup the mydata container */
    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", fft);
    g_object_unref(fft);
    gwy_container_set_string_by_name(controls.mydata,
                                     "/0/base/palette",
                                     g_strdup("DFit"));
    gwy_container_set_enum_by_name(controls.mydata, "/0/base/range-type",
                                   GWY_LAYER_BASIC_RANGE_AUTO);

    gwy_container_set_object_by_name(controls.mydata, "/0/mask",
                                     mfield);
    g_object_unref(mfield);
    rgba.r = rgba.g = rgba.b = 1.0; rgba.a = 0.5;
    gwy_rgba_store_to_container(&rgba, controls.mydata, "/0/mask");

    gwy_container_set_object_by_name(controls.mydata, "/1/data",
                                     dfield);
    gwy_app_copy_data_items(data, controls.mydata, id, 1,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            0);

    gwy_container_set_object_by_name(controls.mydata, "/2/data",
                                     filtered);
    g_object_unref(filtered);
    gwy_app_copy_data_items(data, controls.mydata, id, 2,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            0);

    gwy_container_set_object_by_name(controls.mydata, "/3/data",
                                     diff);
    g_object_unref(diff);
    gwy_app_copy_data_items(data, controls.mydata, id, 3,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            0);

    /* Run the dialog */
    response = run_dialog(&controls);
    save_settings(&controls);

    /* Do the fft filtering */
    if (response) {
        if (controls.out_mode & OUTPUT_IMAGE)
            out_image = gwy_data_field_new_alike(dfield, FALSE);
        if (controls.out_mode & OUTPUT_FFT)
            out_fft = gwy_data_field_new_alike(dfield, FALSE);

        mfield =
            GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata,
                                                            "/0/mask"));
        fft_filter_2d(dfield, out_image, out_fft, mfield);

        if (out_image) {
            newid = gwy_app_data_browser_add_data_field(out_image, data, TRUE);
            gwy_app_copy_data_items(data, data, id, newid,
                                    GWY_DATA_ITEM_GRADIENT,
                                    GWY_DATA_ITEM_MASK_COLOR,
                                    GWY_DATA_ITEM_RANGE,
                                    GWY_DATA_ITEM_RANGE_TYPE,
                                    0);
            gwy_app_set_data_field_title(data, newid, _("Filtered Data"));
        }

        if (out_fft) {
            newid = gwy_app_data_browser_add_data_field(out_fft, data, TRUE);
            gwy_app_copy_data_items(controls.mydata, data, 0, newid,
                                    GWY_DATA_ITEM_GRADIENT,
                                    GWY_DATA_ITEM_MASK_COLOR,
                                    GWY_DATA_ITEM_RANGE,
                                    GWY_DATA_ITEM_RANGE_TYPE,
                                    0);
            gwy_app_set_data_field_title(data, newid, _("Filtered FFT"));
        }
    }
}

static GwyDataField*
create_mask_field(GwyDataField *dfield)
{
    GwyDataField *mfield;
    GwySIUnit *siunit;

    mfield = gwy_data_field_new_alike(dfield, TRUE);
    siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(mfield, siunit);
    g_object_unref(siunit);

    return mfield;
}

static gboolean
run_dialog(ControlsType *controls)
{
    static struct {
        guint edit_mode;
        const gchar *stock_id;
        const gchar *text;
    }
    const modes[] = {
        {
            FFT_ELLIPSE_ADD,
            GWY_STOCK_MASK_CIRCLE_INCLUSIVE,
            N_("Add an ellipse to the FFT mask"),
        },
        {
            FFT_RECT_ADD,
            GWY_STOCK_MASK_RECT_INCLUSIVE,
            N_("Add a rectangle to the FFT mask"),
        },
        {
            FFT_ELLIPSE_SUB,
            GWY_STOCK_MASK_CIRCLE_EXCLUSIVE,
            N_("Subtract an ellipse from the FFT mask"),
        },
        {
            FFT_RECT_SUB,
            GWY_STOCK_MASK_RECT_EXCLUSIVE,
            N_("Subtract a rectange from the FFT mask"),
        },
    };

    static struct {
        guint prev_mode;
        const gchar *text;
    }
    const prev_modes[] = {
        {
            PREV_FFT,
            N_("_FFT Editor"),
        },
        {
            PREV_IMAGE,
            N_("Original _Image"),
        },
        {
            PREV_FILTERED,
            N_("Filtered _Image"),
        },
        {
            PREV_DIFF,
            N_("Image _Difference"),
        },
    };

    static GwyEnum output_types[] = {
        { N_("Filtered Image"), OUTPUT_IMAGE              },
        { N_("Filtered FFT"),   OUTPUT_FFT                },
        { N_("Both"),           OUTPUT_IMAGE | OUTPUT_FFT },
    };

    GtkWidget *dialog;
    GtkWidget *table, *hbox, *hbox2;
    GtkWidget *label;
    GtkWidget *image;
    GtkWidget *button = NULL;
    GtkWidget *combo;
    GtkRadioButton *group;
    GtkTooltips *tips;
    GHashTable *hash_tips;
    GwyPixmapLayer *layer, *mlayer;
    GwyDataField *dfield;
    gdouble zoomval;
    gint i, row, response;

    /* Setup the dialog window */
    dialog = gtk_dialog_new_with_buttons(_("2D FFT Filtering"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    /* Setup tooltips */
    hash_tips = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
    build_tooltips(hash_tips);
    tips = gtk_tooltips_new();
    g_object_ref(tips);
    gtk_object_sink(GTK_OBJECT(tips));

    /* Setup sensitvity group */
    controls->sensgroup = gwy_sensitivity_group_new();

    /* Main Horizontal Box (contains the GwyDataView and the control panel) */
    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 5);

    /* Setup the GwyDataView and base layer */
    controls->view = gwy_data_view_new(controls->mydata);
    layer = gwy_layer_basic_new();
    set_layer_channel(layer, 0);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls->view), layer);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/1/data"));
    zoomval = PREVIEW_SIZE /
              (gdouble)MAX(gwy_data_field_get_xres(dfield),
                           gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls->view), zoomval);
    gtk_box_pack_start(GTK_BOX(hbox), controls->view, FALSE, FALSE, 5);

    /* setup vector layer */
    switch_layer(controls->edit_mode, controls);

    /* setup mask layer */
    mlayer = gwy_layer_mask_new();
    gwy_pixmap_layer_set_data_key(mlayer, "/0/mask");
    gwy_layer_mask_set_color_key(GWY_LAYER_MASK(mlayer), "/0/mask");
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), mlayer);

    /* Setup the control panel */
    table = gtk_table_new(20, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 10);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Filter Mask</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    /* MODE/SHAPE Buttons */
    group = NULL;
    for (i = 0; i < G_N_ELEMENTS(modes); i++) {
        button = gtk_radio_button_new_from_widget(group);
        gwy_sensitivity_group_add_widget(controls->sensgroup, button,
                                         SENS_EDIT);
        g_object_set(button, "draw-indicator", FALSE, NULL);
        image = gtk_image_new_from_stock(modes[i].stock_id,
                                         GTK_ICON_SIZE_BUTTON
                                         /*GTK_ICON_SIZE_LARGE_TOOLBAR*/);
        gtk_container_add(GTK_CONTAINER(button), image);
        gwy_radio_button_set_value(button, modes[i].edit_mode);
        gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
        gtk_tooltips_set_tip(tips, button, gettext(modes[i].text), NULL);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(edit_mode_changed_cb), controls);
        if (!group)
            group = GTK_RADIO_BUTTON(button);
    }
    controls->mode = gtk_radio_button_get_group(group);
    gwy_radio_buttons_set_current(controls->mode, controls->edit_mode);

    /* Remaining controls: */
    hbox2 = gtk_vbox_new(FALSE, 5);
    gtk_table_attach(GTK_TABLE(table), hbox2, 1, 2, row, row+2,
                     GTK_EXPAND | GTK_FILL, 0, 5, 0);

    button = gwy_stock_like_button_new(_("_Undo"), GTK_STOCK_UNDO);
    gwy_sensitivity_group_add_widget(controls->sensgroup, button,
                                     SENS_EDIT | SENS_UNDO);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "undo"), "");
    gtk_container_add(GTK_CONTAINER(hbox2), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(undo_cb), controls);

    button = gwy_stock_like_button_new(_("_Remove"), GWY_STOCK_MASK_REMOVE);
    gwy_sensitivity_group_add_widget(controls->sensgroup, button, SENS_EDIT);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "remove_all"), "");
    gtk_container_add(GTK_CONTAINER(hbox2), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(remove_all_cb), controls);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("_Snap to origin"));
    gwy_sensitivity_group_add_widget(controls->sensgroup, button, SENS_EDIT);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "origin"), "");
    gtk_table_attach(GTK_TABLE(table), button, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    set_toggled(button, controls->snap);
    g_signal_connect(button, "clicked", G_CALLBACK(snap_cb), controls);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 15);
    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Preview Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    /*XXX zoom hidden for now
    button = gtk_check_button_new_with_mnemonic(_("_Zoom"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(zoom_toggled), controls);
    gtk_table_attach(GTK_TABLE(table), button, 0, 1, row, row+1,
                     GTK_FILL, GTK_FILL, 0, 2);
    row++;
    */

    label = gtk_label_new(_("Display mode:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.05);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, GTK_FILL, 0, 2);
    row++;

    hbox2 = gtk_vbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    group = NULL;
    for (i = 0; i < G_N_ELEMENTS(prev_modes); i++) {
        button = gtk_radio_button_new_with_mnemonic_from_widget(group,
                                                            prev_modes[i].text);
        gwy_radio_button_set_value(button, prev_modes[i].prev_mode);
        gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(prev_mode_changed_cb), controls);
        if (!group)
            group = GTK_RADIO_BUTTON(button);
    }
    controls->pmode = gtk_radio_button_get_group(group);
    gwy_radio_buttons_set_current(controls->pmode, controls->prev_mode);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 15);
    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Output Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    combo = gwy_enum_combo_box_new(output_types, G_N_ELEMENTS(output_types),
                                   NULL, NULL, OUTPUT_IMAGE, TRUE);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(combo), controls->out_mode);
    gtk_table_attach(GTK_TABLE(table), combo, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);


    label = gtk_label_new_with_mnemonic(_("Output _type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 0, 0);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    gwy_sensitivity_group_set_state(controls->sensgroup, SENS_EDIT, SENS_EDIT);
    g_object_unref(controls->sensgroup);

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(dialog);
            return FALSE;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    controls->out_mode = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));

    /* Finalize */
    gtk_widget_destroy(dialog);
    g_object_unref(tips);
    g_hash_table_destroy(hash_tips);

    return TRUE;
}

static void
set_layer_channel(GwyPixmapLayer *layer, gint channel)
{
    gchar data_key[30];
    gchar grad_key[30];
    gchar mm_key[30];
    gchar range_key[30];

    g_snprintf(data_key, sizeof(data_key), "/%i/data", channel);
    g_snprintf(grad_key, sizeof(grad_key), "/%i/base/palette", channel);
    g_snprintf(mm_key, sizeof(mm_key), "/%i/base", channel);
    g_snprintf(range_key, sizeof(range_key), "/%i/base/range-type", channel);

    gwy_pixmap_layer_set_data_key(layer, data_key);
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), grad_key);
    gwy_layer_basic_set_min_max_key(GWY_LAYER_BASIC(layer), mm_key);
    gwy_layer_basic_set_range_type_key(GWY_LAYER_BASIC(layer), range_key);
}

static void
prev_mode_changed_cb(ControlsType *controls)
{
    GwyPixmapLayer *layer, *mlayer;
    GwyDataField *dfield, *mfield, *filtered, *diff;
    guint new_mode;
    guint state = 0;

    new_mode = gwy_radio_buttons_get_current(controls->pmode);

    if (new_mode != controls->prev_mode) {
        layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));

        mfield =
            GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                            "/0/mask"));
        dfield =
            GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                            "/1/data"));
        filtered =
            GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                            "/2/data"));
        diff =
            GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                            "/3/data"));

        if (!GWY_IS_DATA_FIELD(mfield))
            g_debug("No mfield.");
        if (!GWY_IS_DATA_FIELD(dfield))
            g_debug("No dfield.");
        if (!GWY_IS_DATA_FIELD(filtered))
            g_debug("No filtered.");
        if (!GWY_IS_DATA_FIELD(diff))
            g_debug("No diff.");


        switch(new_mode) {
            case PREV_FFT:
            set_layer_channel(layer, 0);
            mlayer = gwy_layer_mask_new();
            gwy_pixmap_layer_set_data_key(mlayer, "/0/mask");
            gwy_layer_mask_set_color_key(GWY_LAYER_MASK(mlayer), "/0/mask");
            gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view),
                                          mlayer);
            controls->prev_mode = new_mode;
            state = SENS_EDIT;
            break;

            case PREV_IMAGE:
            set_layer_channel(layer, 1);
            break;

            case PREV_FILTERED:
            if (controls->compute) {
                fft_filter_2d(dfield, filtered, NULL, mfield);
                gwy_data_field_subtract_fields(diff, dfield, filtered);
            }
            controls->compute = FALSE;
            set_layer_channel(layer, 2);
            break;

            case PREV_DIFF:
            if (controls->compute) {
                fft_filter_2d(dfield, filtered, NULL, mfield);
                gwy_data_field_subtract_fields(diff, dfield, filtered);
            }
            controls->compute = FALSE;
            set_layer_channel(layer, 3);
            break;

            default:
            g_assert_not_reached();
            break;
        }

        gwy_sensitivity_group_set_state(controls->sensgroup, SENS_EDIT, state);

        if (new_mode == PREV_FFT) {
            controls->edit_mode = -1;
            edit_mode_changed_cb(controls);
        }
        else {
            gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), NULL);
            gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), NULL);
        }
        controls->prev_mode = new_mode;
    }
}

static GwyVectorLayer*
create_vlayer(guint new_mode)
{
    GwyVectorLayer *vlayer = NULL;

    switch(new_mode)
    {
        case FFT_RECT_ADD:
        case FFT_RECT_SUB:
        vlayer = g_object_new(g_type_from_name("GwyLayerRectangle"), NULL);
        gwy_vector_layer_set_selection_key(vlayer, "/0/select/fft/rect");
        break;

        case FFT_ELLIPSE_ADD:
        case FFT_ELLIPSE_SUB:
        vlayer = g_object_new(g_type_from_name("GwyLayerEllipse"), NULL);
        gwy_vector_layer_set_selection_key(vlayer, "/0/select/fft/ellipse");
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return vlayer;
}

static void
switch_layer(guint new_mode, ControlsType *controls)
{
    GwyVectorLayer *vlayer = NULL;
    GwySelection *selection;

    vlayer = create_vlayer(new_mode);
    g_object_set(G_OBJECT(vlayer),
                 "snap-to-center", controls->snap,
                 "draw-reflection", !controls->snap,
                 NULL);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), vlayer);
    selection = gwy_vector_layer_ensure_selection(vlayer);

    switch(new_mode)
    {
        case FFT_RECT_ADD:
        case FFT_RECT_SUB:
        if (!controls->rect_signal)
            controls->rect_signal = g_signal_connect(selection, "finished",
                                    G_CALLBACK(selection_finished_cb), controls);
        break;

        case FFT_ELLIPSE_ADD:
        case FFT_ELLIPSE_SUB:
        if (!controls->ellipse_signal)
            controls->ellipse_signal = g_signal_connect(selection, "finished",
                                    G_CALLBACK(selection_finished_cb), controls);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
edit_mode_changed_cb(ControlsType *controls)
{
    guint new_mode;

    if (controls->prev_mode != PREV_FFT)
        return;

    new_mode = gwy_radio_buttons_get_current(controls->mode);

    if (controls->edit_mode != new_mode) {
        switch_layer(new_mode, controls);
        controls->edit_mode = new_mode;
    }
}

static void
selection_finished_cb(GwySelection *selection,
                          ControlsType *controls)
{
    GwyDataField *mfield, *fft;
    FieldFillFunc fill_func;
    gdouble sel[4];
    gint isel[4];
    gint mirror[4];
    gdouble value;
    gint xwidth;

    mfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/mask"));
    fft = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                          "/0/data"));
    if (!GWY_IS_DATA_FIELD(mfield)) {
        g_debug("Mask doesn't exist in container!");
        return;
    }

    xwidth = gwy_data_field_get_xres(fft);

    /* get the selection coordinates */
    if (!gwy_selection_get_object(selection, 0, sel))
        return;
    isel[0] = gwy_data_field_rtoj(mfield, sel[0]);
    isel[1] = gwy_data_field_rtoj(mfield, sel[1]);
    isel[2] = gwy_data_field_rtoj(mfield, sel[2]);
    isel[3] = gwy_data_field_rtoj(mfield, sel[3]);
    if (!controls->snap) {
        isel[2]++;
        isel[3]++;
    }

    /* because of the offset (see below), must make sure selection is not along
    left or top edge */
    if (isel[0] == 0)
        isel[0] = 1;
    if (isel[1] == 0)
        isel[1] = 1;

    /*XXX: for the mirrored selection to look "correct" as far as the FFT
    goes, it must be shifted one pixel to the right, and one pixel down. */
    mirror[2] = (-isel[0] + xwidth) + 1;
    mirror[3] = (-isel[1] + xwidth) + 1;
    mirror[0] = (-isel[2] + xwidth) + 1;
    mirror[1] = (-isel[3] + xwidth) + 1;

    /* change coordinates to widths */
    isel[2] -= isel[0];
    isel[3] -= isel[1];
    mirror[2] -= mirror[0];
    mirror[3] -= mirror[1];

    /* decide between rectangle and ellipse */
    switch (controls->edit_mode) {
        case FFT_RECT_ADD:
        fill_func = &gwy_data_field_area_fill;
        value = 1.0;
        break;

        case FFT_RECT_SUB:
        fill_func = &gwy_data_field_area_fill;
        value = 0.0;
        break;

        case FFT_ELLIPSE_ADD:
        fill_func = (FieldFillFunc)&gwy_data_field_elliptic_area_fill;
        value = 1.0;
        break;

        case FFT_ELLIPSE_SUB:
        fill_func = (FieldFillFunc)&gwy_data_field_elliptic_area_fill;
        value = 0.0;
        break;

        default:
        g_assert_not_reached();
        break;
    }

    /* apply change to mask */
    gwy_app_undo_checkpoint(controls->mydata, "/0/mask", NULL);
    fill_func(mfield, isel[0], isel[1], isel[2], isel[3], value);
    fill_func(mfield, mirror[0], mirror[1], mirror[2], mirror[3], value);
    gwy_data_field_data_changed(mfield);
    /*XXX - uncomment this line when done testing */
    /* gwy_selection_clear(selection); */

    gwy_sensitivity_group_set_state(controls->sensgroup, SENS_UNDO, SENS_UNDO);
    controls->compute = TRUE;
}

static void
undo_cb(ControlsType *controls)
{
    if (gwy_undo_container_has_undo(controls->mydata)) {
        gwy_undo_undo_container(controls->mydata);
        controls->compute = TRUE;
        if (!gwy_undo_container_has_undo(controls->mydata))
            gwy_sensitivity_group_set_state(controls->sensgroup, SENS_UNDO, 0);
    }
}

static void
remove_all_cb(ControlsType *controls)
{
    GwyDataField *mfield;

    gwy_app_undo_checkpoint(controls->mydata, "/0/mask", NULL);
    mfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                            "/0/mask"));

    gwy_data_field_clear(mfield);
    gwy_data_field_data_changed(mfield);
    gwy_sensitivity_group_set_state(controls->sensgroup, SENS_UNDO, SENS_UNDO);
    controls->compute = TRUE;
}

static void
snap_cb(GtkWidget *check, ControlsType *controls)
{
    GwyVectorLayer *layer =
        gwy_data_view_get_top_layer(GWY_DATA_VIEW(controls->view));

    controls->snap = get_toggled(check);
    g_object_set(G_OBJECT(layer),
                 "snap-to-center", controls->snap,
                 "draw-reflection", !controls->snap,
                 NULL);
}

/* set_dfield_modulus is copied from fft.c */
static void
set_dfield_modulus(GwyDataField *re, GwyDataField *im, GwyDataField *target)
{
    const gdouble *datare, *dataim;
    gdouble *data;
    gint xres, yres, i;

    xres = gwy_data_field_get_xres(re);
    yres = gwy_data_field_get_yres(re);
    datare = gwy_data_field_get_data_const(re);
    dataim = gwy_data_field_get_data_const(im);
    data = gwy_data_field_get_data(target);
    for (i = xres*yres; i; i--, datare++, dataim++, data++)
        *data = hypot(*datare, *dataim);
}

static void
do_fft(GwyDataField *data_input, GwyDataField *data_output)
{
    GwyDataField *r_in, *r_out, *i_out;
    GwyWindowingType window = GWY_WINDOWING_LANCZOS;
    GwyTransformDirection direction = GWY_TRANSFORM_DIRECTION_FORWARD;
    GwyInterpolationType interp = GWY_INTERPOLATION_BILINEAR;
    gint level = 1; /* subtract mean value */

    r_in = gwy_data_field_duplicate(data_input);
    r_out = gwy_data_field_new_alike(data_input, TRUE);
    i_out = gwy_data_field_new_alike(data_input, TRUE);

    /* normalize */
    gwy_data_field_multiply(r_in, 1.0
                            /(gwy_data_field_get_max(r_in)
                              - gwy_data_field_get_min(r_in)));

    /* perform fft */
    gwy_data_field_2dfft(r_in, NULL, r_out, i_out,
                         window, direction, interp,
                         FALSE, level);

    gwy_data_field_2dfft_humanize(r_out);
    gwy_data_field_2dfft_humanize(i_out);

    set_dfield_modulus(r_out, i_out, data_output);

    g_object_unref(r_in);
    g_object_unref(r_out);
    g_object_unref(i_out);
}

static void
fft_filter_2d(GwyDataField *input,
              GwyDataField *output_image, GwyDataField *output_fft,
              GwyDataField *mask)
{
    GwyDataField *r_in, *i_in, *r_out, *i_out;
    GwySIUnit *siunit;

    /* Run the forward FFT */
    r_in = gwy_data_field_duplicate(input);
    siunit = gwy_data_field_get_si_unit_xy(r_in);
    gwy_si_unit_power(siunit, -1, siunit);

    i_in = GWY_DATA_FIELD(gwy_data_field_new_alike(r_in, TRUE));
    r_out = GWY_DATA_FIELD(gwy_data_field_new_alike(r_in, FALSE));
    i_out = GWY_DATA_FIELD(gwy_data_field_new_alike(r_in, FALSE));

    /*XXX Should I normalize? */
    //gwy_data_field_multiply(r_in, 1.0
    //                        /(gwy_data_field_get_max(r_in)
    //                                - gwy_data_field_get_min(r_in)));

    gwy_data_field_2dfft(r_in, NULL, r_out, i_out,
                         GWY_WINDOWING_NONE,
                         GWY_TRANSFORM_DIRECTION_FORWARD,
                         GWY_INTERPOLATION_BILINEAR,
                         FALSE, FALSE);

    gwy_data_field_2dfft_humanize(r_out);
    gwy_data_field_2dfft_humanize(i_out);

    if (output_fft != NULL)
    {
        set_dfield_modulus(r_out, i_out, output_fft);
    }

    /* Apply mask to the fft */
    gwy_data_field_multiply_fields(r_out, r_out, mask);
    gwy_data_field_multiply_fields(i_out, i_out, mask);
    if (output_fft != NULL)
        gwy_data_field_multiply_fields(output_fft, output_fft, mask);

    /* Run the inverse FFT */
    gwy_data_field_2dfft_humanize(r_out);
    gwy_data_field_2dfft_humanize(i_out);
    gwy_data_field_2dfft(r_out, i_out, r_in, i_in,
                         GWY_WINDOWING_NONE,
                         GWY_TRANSFORM_DIRECTION_BACKWARD,
                         GWY_INTERPOLATION_BILINEAR,
                         FALSE, FALSE);

    if (output_image != NULL)
        gwy_data_field_copy(r_in, output_image, TRUE);


    /* Finalize */
    g_object_unref(i_out);
    g_object_unref(r_out);
    g_object_unref(i_in);
    g_object_unref(r_in);
}

static void
save_settings(ControlsType *controls)
{
    GwyContainer *settings = gwy_app_settings_get();

    gwy_container_set_int32_by_name(settings, "/module/fft_filter_2d/edit_mode",
                                    controls->edit_mode);
    gwy_container_set_boolean_by_name(settings, "/module/fft_filter_2d/snap",
                                      controls->snap);
    gwy_container_set_boolean_by_name(settings, "/module/fft_filter_2d/zoom",
                                      controls->zoom);
    gwy_container_set_int32_by_name(settings, "/module/fft_filter_2d/out_mode",
                                    controls->out_mode);
}

static void
load_settings(ControlsType *controls)
{
    GwyContainer *settings = gwy_app_settings_get();

    gwy_container_gis_int32_by_name(settings, "/module/fft_filter_2d/edit_mode",
                                    &controls->edit_mode);
    gwy_container_gis_boolean_by_name(settings, "/module/fft_filter_2d/snap",
                                      &controls->snap);
    gwy_container_gis_boolean_by_name(settings, "/module/fft_filter_2d/zoom",
                                      &controls->zoom);
    gwy_container_gis_int32_by_name(settings, "/module/fft_filter_2d/out_mode",
                                    &controls->out_mode);
}

/*XXX: fix */
static void
build_tooltips(GHashTable *hash_tips)
{
    g_hash_table_insert(hash_tips, "circle_inclusive",
                        _("Draw a new inclusive circle marker."));
    g_hash_table_insert(hash_tips, "rectangle_inclusive",
                        _("Draw a new inclusive rectangle marker."));
    g_hash_table_insert(hash_tips, "circle_exclusive",
                        _("Draw a new exclusive circle marker."));
    g_hash_table_insert(hash_tips, "rectangle_exclusive",
                        _("Draw a new exclusive rectangle marker."));
    g_hash_table_insert(hash_tips, "undo",
                        _("Undo the last change to the filter mask."));
    g_hash_table_insert(hash_tips, "remove_all",
                        _("Removes the entire filter mask."));
    g_hash_table_insert(hash_tips, "origin",
                        _("Forces new markers to center around the origin."));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
