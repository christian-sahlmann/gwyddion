/*
 *  $Id$
 *  Copyright (C) 2005 Christopher Anderson, Molecular Imaging Corp.
 *  E-mail: Chris Anderson (sidewinder.asu@gmail.com)
 *  Copyright (C) 2011 David Necas (Yeti).
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/elliptic.h>
#include <libprocess/inttrans.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define FFTF_2D_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400
};

/* Convenience macros */
#define get_toggled(obj) \
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(obj))
#define set_toggled(obj, tog) \
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(obj), tog)

typedef enum {
    FFT_ELLIPSE_ADD    = 0,
    FFT_RECT_ADD       = 1,
    FFT_ELLIPSE_SUB    = 2,
    FFT_RECT_SUB       = 3,
} EditType;

typedef enum {
    PREV_FFT,
    PREV_IMAGE,
    PREV_FILTERED,
    PREV_DIFF,
} PreviewType;

typedef enum {
    OUTPUT_FFT   = 1 << 0,
    OUTPUT_IMAGE = 1 << 1,
    OUTPUT_DIFF  = 1 << 2
} OutputType;

enum {
    SENS_EDIT = 1 << 0,
    SENS_UNDO = 1 << 1
};

/* Keep it simple and use a predefined set of zooms, these seem suitable. */
typedef enum {
    ZOOM_1 = 1,
    ZOOM_4 = 4,
    ZOOM_16 = 16
} ZoomType;

enum {
    RESPONSE_RESET,
};

typedef struct {
    GwyContainer    *mydata;
    GwyContainer    *data;

    GwySensitivityGroup *sensgroup;

    gulong          rect_signal;
    gulong          ellipse_signal;

    GtkWidget       *view;
    GwyPixmapLayer  *view_layer;
    GwyPixmapLayer  *mask_layer;

    EditType        edit_mode;
    GSList          *mode;

    PreviewType     prev_mode;
    GSList          *pmode;

    ZoomType        zoom_mode;
    GSList          *zoom;

    gboolean        snap;
    OutputType      out_mode;

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
static void        fill_all_cb           (ControlsType *controls);
static void        undo_cb               (ControlsType *controls);
static void        snap_cb               (GtkWidget *check,
                                          ControlsType *controls);
static void        zoom_changed          (GtkRadioButton *button,
                                          ControlsType *controls);
static void        out_mode_changed      (GtkToggleButton *check,
                                          ControlsType *controls);

/* Helper Functions */
static gboolean        run_dialog        (ControlsType *controls);
static GwyDataField*   create_mask_field (GwyDataField *dfield);
static GwyVectorLayer* create_vlayer     (guint new_mode);
static void            switch_layer      (guint new_mode,
                                          ControlsType *controls);
static void            set_layer_channel (GwyPixmapLayer *layer,
                                          gint channel);
static void            do_fft            (GwyDataField *dataInput,
                                          GwyDataField *dataOutput);
static void            calculate_zooms   (GwyContainer *container,
                                          GwyDataField *field,
                                          GwyDataField *mask);
static void            set_dfield_modulus(GwyDataField *re,
                                          GwyDataField *im,
                                          GwyDataField *target);
static void            fft_filter_2d     (GwyDataField *input,
                                          GwyDataField *output_image,
                                          GwyDataField *output_fft,
                                          GwyDataField *mask);
static void            save_settings     (ControlsType *controls);
static void            load_settings     (ControlsType *controls);
static void            build_tooltips    (GHashTable *hash_tips);

static const GwyRGBA mask_color = { 0.56, 0.39, 0.07, 0.5 };

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("2D FFT Filtering"),
    "Chris Anderson <sidewinder.asu@gmail.com>",
    "1.8",
    "Chris Anderson, Molecular Imaging Corp.",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fft_filter_2d",
                              (GwyProcessFunc)&run_main,
                              N_("/_Correct Data/_2D FFT Filtering..."),
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
    GwyDataField *out_image = NULL, *out_fft = NULL, *out_diff = NULL;
    gint id, newid;
    ControlsType controls;
    gboolean response;

    g_return_if_fail(run & FFTF_2D_RUN_MODES);

    /* Initialise */
    controls.rect_signal = controls.ellipse_signal = 0;
    controls.edit_mode = FFT_ELLIPSE_ADD;
    controls.prev_mode = PREV_FFT;
    controls.zoom_mode = ZOOM_1;
    controls.out_mode = OUTPUT_IMAGE | OUTPUT_DIFF;
    controls.snap = FALSE;
    controls.data = data;
    controls.compute = TRUE;
    load_settings(&controls);

    /*XXX: should the mask and presentation get carried through? */
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    fft = gwy_data_field_new_alike(dfield, FALSE);
    do_fft(dfield, fft);

    mfield = create_mask_field(fft);
    filtered = gwy_data_field_new_alike(dfield, TRUE);
    diff = gwy_data_field_new_alike(dfield, TRUE);

    /* Setup the mydata container */
    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", fft);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    calculate_zooms(controls.mydata, fft, mfield);
    g_object_unref(fft);

    gwy_container_set_string_by_name(controls.mydata,
                                     "/0/base/palette",
                                     g_strdup("DFit"));
    gwy_container_set_enum_by_name(controls.mydata, "/0/base/range-type",
                                   GWY_LAYER_BASIC_RANGE_AUTO);

    gwy_container_set_object_by_name(controls.mydata, "/0/mask",
                                     mfield);
    g_object_unref(mfield);
    gwy_rgba_store_to_container(&mask_color, controls.mydata, "/0/mask");

    gwy_container_set_object_by_name(controls.mydata, "/1/data",
                                     dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 1, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    gwy_container_set_object_by_name(controls.mydata, "/2/data",
                                     filtered);
    g_object_unref(filtered);
    gwy_app_sync_data_items(data, controls.mydata, id, 2, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_MASK_COLOR,
                            /* GWY_DATA_ITEM_RANGE, */
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    gwy_container_set_object_by_name(controls.mydata, "/3/data",
                                     diff);
    g_object_unref(diff);
    gwy_app_sync_data_items(data, controls.mydata, id, 3, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    /* Run the dialog */
    response = run_dialog(&controls);
    save_settings(&controls);

    /* Do the fft filtering */
    if (response) {
        if (controls.out_mode & (OUTPUT_IMAGE | OUTPUT_DIFF))
            out_image = gwy_data_field_new_alike(dfield, FALSE);
        if (controls.out_mode & OUTPUT_FFT)
            out_fft = gwy_data_field_new_alike(dfield, FALSE);

        mfield =
            GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata,
                                                            "/0/mask"));
        fft_filter_2d(dfield, out_image, out_fft, mfield);

        if (controls.out_mode & OUTPUT_IMAGE) {
            newid = gwy_app_data_browser_add_data_field(out_image, data, TRUE);
            gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                    GWY_DATA_ITEM_GRADIENT,
                                    GWY_DATA_ITEM_MASK_COLOR,
                                    GWY_DATA_ITEM_RANGE,
                                    GWY_DATA_ITEM_RANGE_TYPE,
                                    GWY_DATA_ITEM_REAL_SQUARE,
                                    0);
            gwy_app_set_data_field_title(data, newid, _("Filtered Data"));
        }

        if (controls.out_mode & OUTPUT_DIFF) {
            out_diff = gwy_data_field_duplicate(dfield);
            gwy_data_field_subtract_fields(out_diff, dfield, out_image);
            newid = gwy_app_data_browser_add_data_field(out_diff, data, TRUE);
            gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                    GWY_DATA_ITEM_GRADIENT,
                                    GWY_DATA_ITEM_MASK_COLOR,
                                    GWY_DATA_ITEM_RANGE,
                                    GWY_DATA_ITEM_RANGE_TYPE,
                                    GWY_DATA_ITEM_REAL_SQUARE,
                                    0);
            gwy_app_set_data_field_title(data, newid, _("Difference"));
        }

        if (controls.out_mode & OUTPUT_FFT) {
            newid = gwy_app_data_browser_add_data_field(out_fft, data, TRUE);
            gwy_app_sync_data_items(controls.mydata, data, 0, newid, FALSE,
                                    GWY_DATA_ITEM_GRADIENT,
                                    GWY_DATA_ITEM_MASK_COLOR,
                                    GWY_DATA_ITEM_RANGE,
                                    GWY_DATA_ITEM_RANGE_TYPE,
                                    GWY_DATA_ITEM_REAL_SQUARE,
                                    0);
            gwy_app_set_data_field_title(data, newid, _("Filtered FFT"));
        }

        gwy_object_unref(out_diff);
        gwy_object_unref(out_image);
        gwy_object_unref(out_fft);
    }

    g_object_unref(controls.mydata);
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
            N_("_FFT mask editor"),
        },
        {
            PREV_IMAGE,
            N_("Original _image"),
        },
        {
            PREV_FILTERED,
            N_("Fi_ltered image"),
        },
        {
            PREV_DIFF,
            N_("Image _difference"),
        },
    };

    static struct {
        guint out_mode;
        const gchar *text;
    }
    const out_modes[] = {
        {
            OUTPUT_IMAGE,
            N_("Filtered i_mage"),
        },
        {
            OUTPUT_DIFF,
            N_("Ima_ge difference"),
        },
        {
            OUTPUT_FFT,
            N_("Filtered FFT mo_dulus"),
        },
    };

    GtkWidget *dialog;
    GtkWidget *table, *hbox, *hbox2;
    GtkWidget *label;
    GtkWidget *image;
    GtkWidget *button = NULL;
    GtkWidget *check;
    GtkRadioButton *group;
    GtkTooltips *tips;
    GHashTable *hash_tips;
    GwyPixmapLayer *layer, *mlayer;
    GSList *l;
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
    tips = gwy_app_get_tooltips();

    /* Setup sensitvity group */
    controls->sensgroup = gwy_sensitivity_group_new();

    /* Main Horizontal Box (contains the GwyDataView and the control panel) */
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 4);

    /* Setup the GwyDataView and base layer */
    controls->view = gwy_data_view_new(controls->mydata);
    layer = controls->view_layer = gwy_layer_basic_new();
    set_layer_channel(layer, 0);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls->view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls->view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls->view), PREVIEW_SIZE);
    gtk_box_pack_start(GTK_BOX(hbox), controls->view, FALSE, FALSE, 4);

    /* setup vector layer */
    switch_layer(controls->edit_mode, controls);

    /* setup mask layer */
    mlayer = controls->mask_layer = gwy_layer_mask_new();
    gwy_pixmap_layer_set_data_key(mlayer, "/0/mask");
    gwy_layer_mask_set_color_key(GWY_LAYER_MASK(mlayer), "/0/mask");
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), mlayer);

    /* Setup the control panel */
    table = gtk_table_new(16, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gwy_label_new_header(_("Filter Mask"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
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
                                         GTK_ICON_SIZE_BUTTON);
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
    hbox2 = gtk_vbox_new(FALSE, 4);
    gtk_table_attach(GTK_TABLE(table), hbox2, 1, 2, row, row+3,
                     GTK_EXPAND | GTK_FILL, 0, 4, 0);

    button = gwy_stock_like_button_new(_("_Undo"), GTK_STOCK_UNDO);
    gwy_sensitivity_group_add_widget(controls->sensgroup, button,
                                     SENS_EDIT | SENS_UNDO);
    gtk_tooltips_set_tip(tips, button,
                         g_hash_table_lookup(hash_tips, "undo"), NULL);
    gtk_container_add(GTK_CONTAINER(hbox2), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(undo_cb), controls);

    button = gwy_stock_like_button_new(_("_Remove"), GWY_STOCK_MASK_REMOVE);
    gwy_sensitivity_group_add_widget(controls->sensgroup, button, SENS_EDIT);
    gtk_tooltips_set_tip(tips, button,
                         g_hash_table_lookup(hash_tips, "remove_all"), NULL);
    gtk_container_add(GTK_CONTAINER(hbox2), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(remove_all_cb), controls);

    button = gwy_stock_like_button_new(_("_Fill"), GWY_STOCK_MASK);
    gwy_sensitivity_group_add_widget(controls->sensgroup, button, SENS_EDIT);
    gtk_tooltips_set_tip(tips, button,
                         g_hash_table_lookup(hash_tips, "fill_all"), NULL);
    gtk_container_add(GTK_CONTAINER(hbox2), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(fill_all_cb), controls);

    row++;

    button = gtk_check_button_new_with_mnemonic(_("_Snap to origin"));
    gwy_sensitivity_group_add_widget(controls->sensgroup, button, SENS_EDIT);
    gtk_tooltips_set_tip(tips, button,
                         g_hash_table_lookup(hash_tips, "origin"), NULL);
    gtk_table_attach(GTK_TABLE(table), button, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    set_toggled(button, controls->snap);
    g_signal_connect(button, "clicked", G_CALLBACK(snap_cb), controls);

    row++;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gwy_label_new_header(_("Preview Options"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Display:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, GTK_FILL, 0, 2);
    row++;

    hbox2 = gtk_vbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    group = NULL;
    for (i = 0; i < G_N_ELEMENTS(prev_modes); i++) {
        button = gtk_radio_button_new_with_mnemonic_from_widget(group,
                                                   gettext(prev_modes[i].text));
        gwy_radio_button_set_value(button, prev_modes[i].prev_mode);
        gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(prev_mode_changed_cb), controls);
        if (!group)
            group = GTK_RADIO_BUTTON(button);
    }
    controls->pmode = gtk_radio_button_get_group(group);
    gwy_radio_buttons_set_current(controls->pmode, controls->prev_mode);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* Zoom */
    hbox2 = gtk_hbox_new(FALSE, 8);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new(_("Zoom:"));
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
    gwy_sensitivity_group_add_widget(controls->sensgroup, label, SENS_EDIT);

    controls->zoom
        = gwy_radio_buttons_createl(G_CALLBACK(zoom_changed), controls,
                                    controls->zoom_mode,
                                    "1×", ZOOM_1,
                                    "4×", ZOOM_4,
                                    "16×", ZOOM_16,
                                    NULL);
    for (l = controls->zoom; l; l = g_slist_next(l)) {
        GtkWidget *widget = GTK_WIDGET(l->data);
        gtk_box_pack_start(GTK_BOX(hbox2), widget, FALSE, FALSE, 0);
        gwy_sensitivity_group_add_widget(controls->sensgroup, widget,
                                         SENS_EDIT);
    }
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* Output */
    label = gwy_label_new_header(_("Output Options"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 2);
    row++;

    for (i = 0; i < G_N_ELEMENTS(out_modes); i++) {
        check = gtk_check_button_new_with_mnemonic(_(out_modes[i].text));
        g_object_set_data(G_OBJECT(check), "value",
                          GUINT_TO_POINTER(out_modes[i].out_mode));
        if (controls->out_mode & out_modes[i].out_mode)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
        gtk_table_attach(GTK_TABLE(table), check, 0, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        g_signal_connect(check, "toggled",
                         G_CALLBACK(out_mode_changed), controls);
        row++;
    }
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    gwy_sensitivity_group_set_state(controls->sensgroup, SENS_EDIT, SENS_EDIT);
    g_object_unref(controls->sensgroup);

    if (controls->prev_mode == PREV_FFT)
        zoom_changed(NULL, controls);

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

    /* Finalize */
    gtk_widget_destroy(dialog);
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


        switch (new_mode) {
            case PREV_FFT:
            set_layer_channel(layer, 0);
            mlayer = controls->mask_layer = gwy_layer_mask_new();
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
        gwy_set_data_preview_size(GWY_DATA_VIEW(controls->view), PREVIEW_SIZE);

        gwy_sensitivity_group_set_state(controls->sensgroup, SENS_EDIT, state);

        if (new_mode == PREV_FFT) {
            controls->edit_mode = -1;
            edit_mode_changed_cb(controls);
        }
        else {
            gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), NULL);
            gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), NULL);
            controls->mask_layer = NULL;
        }
        controls->prev_mode = new_mode;
        if (new_mode == PREV_FFT)
            zoom_changed(NULL, controls);
    }
}

static GwyVectorLayer*
create_vlayer(guint new_mode)
{
    GwyVectorLayer *vlayer = NULL;

    switch (new_mode) {
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
    g_object_set(vlayer,
                 "snap-to-center", controls->snap,
                 "draw-reflection", !controls->snap,
                 NULL);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), vlayer);
    selection = gwy_vector_layer_ensure_selection(vlayer);

    switch (new_mode) {
        case FFT_RECT_ADD:
        case FFT_RECT_SUB:
        if (!controls->rect_signal)
            controls->rect_signal
                = g_signal_connect(selection, "finished",
                                   G_CALLBACK(selection_finished_cb), controls);
        break;

        case FFT_ELLIPSE_ADD:
        case FFT_ELLIPSE_SUB:
        if (!controls->ellipse_signal)
            controls->ellipse_signal
                = g_signal_connect(selection, "finished",
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
    GwyDataField *mfield, *fft, *zoomed;
    FieldFillFunc fill_func;
    const gchar *key;
    gdouble sel[4];
    gint isel[4];
    gint mirror[4];
    gdouble value;
    gint width, height, zwidth, zheight;

    /* Get the selection coordinates. */
    if (!gwy_selection_get_object(selection, 0, sel))
        return;

    mfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/mask"));
    fft = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                          "/0/data"));
    if (!GWY_IS_DATA_FIELD(mfield)) {
        g_warning("Mask doesn't exist in container!");
        gwy_selection_clear(selection);
        return;
    }

    key = gwy_pixmap_layer_get_data_key(controls->mask_layer);
    zoomed = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             key));
    if (!GWY_IS_DATA_FIELD(zoomed)) {
        g_warning("Cannot get the zoomed field!");
        gwy_selection_clear(selection);
        return;
    }

    width = gwy_data_field_get_xres(fft);
    height = gwy_data_field_get_yres(fft);
    zwidth = gwy_data_field_get_xres(zoomed);
    zheight = gwy_data_field_get_yres(zoomed);

    isel[0] = gwy_data_field_rtoj(zoomed, sel[0]) + (width - zwidth)/2;
    isel[1] = gwy_data_field_rtoi(zoomed, sel[1]) + (height - zheight)/2;
    isel[2] = gwy_data_field_rtoj(zoomed, sel[2]) + (width - zwidth)/2;
    isel[3] = gwy_data_field_rtoi(zoomed, sel[3]) + (height - zheight)/2;
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

    /* For the mirrored selection to look "correct" as far as the FFT goes, it
     * must be shifted one pixel to the right, and one pixel down for
     * even-sized images.  XXX: It looks a bit weird when the selection and
     * mask are displayed together. */
    mirror[0] = (-isel[2] + width) + (1 - width % 2);
    mirror[1] = (-isel[3] + height) + (1 - height % 2);;
    mirror[2] = (-isel[0] + width) + (1 - width % 2);
    mirror[3] = (-isel[1] + height) + (1 - height % 2);

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
    calculate_zooms(controls->mydata, NULL, mfield);
    gwy_data_field_data_changed(mfield);
    gwy_selection_clear(selection);

    gwy_sensitivity_group_set_state(controls->sensgroup, SENS_UNDO, SENS_UNDO);
    controls->compute = TRUE;
}

static void
undo_cb(ControlsType *controls)
{
    GwyDataField *mfield;

    if (!gwy_undo_container_has_undo(controls->mydata))
        return;

    gwy_undo_undo_container(controls->mydata);
    mfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/mask"));
    calculate_zooms(controls->mydata, NULL, mfield);
    controls->compute = TRUE;
    if (!gwy_undo_container_has_undo(controls->mydata))
        gwy_sensitivity_group_set_state(controls->sensgroup, SENS_UNDO, 0);
}

static void
remove_all_cb(ControlsType *controls)
{
    GwyDataField *mfield;

    gwy_app_undo_checkpoint(controls->mydata, "/0/mask", NULL);
    mfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/mask"));

    gwy_data_field_clear(mfield);
    calculate_zooms(controls->mydata, NULL, mfield);
    gwy_data_field_data_changed(mfield);
    gwy_sensitivity_group_set_state(controls->sensgroup, SENS_UNDO, SENS_UNDO);
    controls->compute = TRUE;
}

static void
fill_all_cb(ControlsType *controls)
{
    GwyDataField *mfield;

    gwy_app_undo_checkpoint(controls->mydata, "/0/mask", NULL);
    mfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/mask"));

    gwy_data_field_fill(mfield, 1.0);
    calculate_zooms(controls->mydata, NULL, mfield);
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
    g_object_set(layer,
                 "snap-to-center", controls->snap,
                 "draw-reflection", !controls->snap,
                 NULL);
}

static void
zoom_changed(GtkRadioButton *button,
             ControlsType *controls)
{
    ZoomType zoom_mode = gwy_radio_buttons_get_current(controls->zoom);
    gchar key[32];

    if (button && zoom_mode == controls->zoom_mode)
        return;

    controls->zoom_mode = zoom_mode;
    if (controls->prev_mode != PREV_FFT)
        return;

    g_snprintf(key, sizeof(key), "/zoomed/%u", zoom_mode);
    gwy_pixmap_layer_set_data_key(controls->view_layer, key);

    g_snprintf(key, sizeof(key), "/zoomed-mask/%u", zoom_mode);
    gwy_pixmap_layer_set_data_key(controls->mask_layer, key);

    gwy_set_data_preview_size(GWY_DATA_VIEW(controls->view), PREVIEW_SIZE);
}

static void
out_mode_changed(GtkToggleButton *check,
                 ControlsType *controls)
{
    OutputType value = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(check),
                                                          "value"));
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check));

    if (active)
        controls->out_mode |= value;
    else
        controls->out_mode &= ~value;
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
    GwyInterpolationType interp = GWY_INTERPOLATION_LINEAR;
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

static GwyDataField*
zoom_in(GwyDataField *field)
{
    gint xres = gwy_data_field_get_xres(field),
         yres = gwy_data_field_get_yres(field);
    gint newxres = MIN(MAX(xres/4, 4), xres),
         newyres = MIN(MAX(yres/4, 4), yres);

    /* Keep parity in the zoomed field. */
    if (newxres % 2 != xres % 2)
        newxres++;
    if (newyres % 2 != yres % 2)
        newyres++;
    return gwy_data_field_area_extract(field,
                                       (xres - newxres)/2, (yres - newyres)/2,
                                       newxres, newyres);
}

static void
calculate_zooms(GwyContainer *container,
                GwyDataField *field, GwyDataField *mask)
{
    if (field) {
        gwy_container_set_object_by_name(container, "/zoomed/1", field);

        field = zoom_in(field);
        gwy_container_set_object_by_name(container, "/zoomed/4", field);
        g_object_unref(field);

        field = zoom_in(field);
        gwy_container_set_object_by_name(container, "/zoomed/16", field);
        g_object_unref(field);
    }

    if (mask) {
        gwy_container_set_object_by_name(container, "/zoomed-mask/1", mask);

        mask = zoom_in(mask);
        gwy_container_set_object_by_name(container, "/zoomed-mask/4", mask);
        g_object_unref(mask);

        mask = zoom_in(mask);
        gwy_container_set_object_by_name(container, "/zoomed-mask/16", mask);
        g_object_unref(mask);
    }
}

static void
fft_filter_2d(GwyDataField *input,
              GwyDataField *output_image, GwyDataField *output_fft,
              GwyDataField *mask)
{
    GwyDataField *r_in, *i_in, *r_out, *i_out;

    /* Run the forward FFT */
    r_in = gwy_data_field_duplicate(input);

    i_in = GWY_DATA_FIELD(gwy_data_field_new_alike(r_in, TRUE));
    r_out = GWY_DATA_FIELD(gwy_data_field_new_alike(r_in, FALSE));
    i_out = GWY_DATA_FIELD(gwy_data_field_new_alike(r_in, FALSE));

    /*XXX Should I normalize? */
    /*
      gwy_data_field_multiply(r_in, 1.0
                              /(gwy_data_field_get_max(r_in)
                                      - gwy_data_field_get_min(r_in)));
    */

    gwy_data_field_2dfft_raw(r_in, NULL, r_out, i_out,
                             GWY_TRANSFORM_DIRECTION_FORWARD);

    gwy_data_field_2dfft_humanize(r_out);
    gwy_data_field_2dfft_humanize(i_out);

    if (output_fft != NULL) {
        GwySIUnit *siunit;

        set_dfield_modulus(r_out, i_out, output_fft);

        siunit = gwy_data_field_get_si_unit_xy(input);
        siunit = gwy_si_unit_power(siunit, -1, NULL);
        gwy_data_field_set_si_unit_xy(output_fft, siunit);
        g_object_unref(siunit);

        gwy_data_field_set_xreal(output_fft,
                                 gwy_data_field_get_xres(input)
                                 /gwy_data_field_get_xreal(input));
        gwy_data_field_set_yreal(output_fft,
                                 gwy_data_field_get_yres(input)
                                 /gwy_data_field_get_yreal(input));
    }

    /* Apply mask to the fft */
    gwy_data_field_multiply_fields(r_out, r_out, mask);
    gwy_data_field_multiply_fields(i_out, i_out, mask);
    if (output_fft != NULL)
        gwy_data_field_multiply_fields(output_fft, output_fft, mask);

    /* Run the inverse FFT */
    gwy_data_field_2dfft_dehumanize(r_out);
    gwy_data_field_2dfft_dehumanize(i_out);
    gwy_data_field_2dfft_raw(r_out, i_out, r_in, i_in,
                             GWY_TRANSFORM_DIRECTION_BACKWARD);

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

    gwy_container_set_enum_by_name(settings, "/module/fft_filter_2d/edit_mode",
                                   controls->edit_mode);
    gwy_container_set_boolean_by_name(settings, "/module/fft_filter_2d/snap",
                                      controls->snap);
    gwy_container_set_enum_by_name(settings, "/module/fft_filter_2d/zoom",
                                   controls->zoom_mode);
    gwy_container_set_enum_by_name(settings, "/module/fft_filter_2d/out_mode",
                                   controls->out_mode);
}

static void
load_settings(ControlsType *controls)
{
    GwyContainer *settings = gwy_app_settings_get();

    gwy_container_gis_enum_by_name(settings, "/module/fft_filter_2d/edit_mode",
                                   &controls->edit_mode);
    gwy_container_gis_boolean_by_name(settings, "/module/fft_filter_2d/snap",
                                      &controls->snap);
    gwy_container_gis_enum_by_name(settings, "/module/fft_filter_2d/zoom",
                                   &controls->zoom_mode);
    gwy_container_gis_enum_by_name(settings, "/module/fft_filter_2d/out_mode",
                                   &controls->out_mode);

    if (controls->zoom_mode != ZOOM_1
        && controls->zoom_mode != ZOOM_4
        && controls->zoom_mode != ZOOM_16)
        controls->zoom_mode = ZOOM_1;
}

/*XXX: fix */
static void
build_tooltips(GHashTable *hash_tips)
{
    g_hash_table_insert(hash_tips, "circle_inclusive",
                        _("Draw ellipses on the mask"));
    g_hash_table_insert(hash_tips, "rectangle_inclusive",
                        _("Draw rectangles on the mask"));
    g_hash_table_insert(hash_tips, "circle_exclusive",
                        _("Undraw ellipses on the mask"));
    g_hash_table_insert(hash_tips, "rectangle_exclusive",
                        _("Undraw rectangles on the mask"));
    g_hash_table_insert(hash_tips, "undo",
                        _("Undo the last change to the filter mask"));
    g_hash_table_insert(hash_tips, "remove_all",
                        _("Clear the entire filter mask"));
    g_hash_table_insert(hash_tips, "fill_all",
                        _("Fill the entire filter mask"));
    g_hash_table_insert(hash_tips, "origin",
                        _("Force shapes to center around the origin"));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
