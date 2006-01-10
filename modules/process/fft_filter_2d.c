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

/*TODO: Only allow 2^n sized images */
#include <math.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/inttrans.h>
#include <libdraw/gwygradient.h>
#include <libdraw/gwypixfield.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwycombobox.h>
#include <app/settings.h>
#include <app/gwyapp.h>

#define FFTF_2D_RUN_MODES (GWY_RUN_MODAL)

#define PREVIEW_SIZE 400.0
#define CR_DEFAULT 5
#define CR_MAX 200

/* Convenience macros */
#define is_marker_zero_sized(marker) \
    (marker->left == marker->right || marker->top == marker->bottom)

#define get_toggled(obj) \
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(obj))
#define set_toggled(obj, tog) \
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(obj), tog)

#define get_combo_index(obj, key) \
    gwy_option_menu_get_history(obj, key)
#define set_combo_index(obj, key, i) \
    gwy_option_menu_set_history(obj, key, i)

#define get_container_data(obj) \
    GWY_DATA_FIELD(gwy_container_get_object_by_name(obj, "/0/data"))

#define radio_new gtk_radio_button_new_with_mnemonic_from_widget
#define check_new gtk_check_button_new_with_mnemonic

#define get_distance(x1, x2, y1, y2) \
    sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2))

enum {
    MARKER_CIRCLE,
    MARKER_RECT,
};

enum {
    OUTPUT_FFT = 1,
    OUTPUT_IMAGE = 2
};

enum {
    RESPONSE_RESET,
};

typedef struct {
    gdouble left;
    gdouble right;
    gdouble top;
    gdouble bottom;

    gdouble x_center;
    gdouble y_center;
    gdouble radius;

    gdouble proximity;

    gint shape;
    gboolean inclusive;
} MarkerType;

typedef struct {
    GwyContainer *cont_data;
    GwyContainer *cont_fft;

    GwyDataField *data_output_fft;
    GwyDataField *data_output_image;

    GtkWidget *draw_fft;
    GtkWidget *scale_fft;

    GdkPixbuf *buf_fft;
    GdkPixbuf *bbuf_fft;
    GdkPixbuf *buf_preview_fft;
    GdkPixbuf *buf_preview_image;
    GdkPixbuf *buf_original_image;
    GdkPixbuf *buf_preview_diff;

    GtkWidget *dialog;

    GtkWidget *button_circle_inc;
    GtkWidget *button_circle_exc;
    GtkWidget *button_rect_inc;
    GtkWidget *button_rect_exc;
    GtkWidget *button_drag;
    GtkWidget *button_remove;

    GtkWidget *check_zoom;
    GtkWidget *check_origin;

    GtkWidget *button_show_fft;
    GtkWidget *button_show_original_image;
    GtkWidget *button_show_fft_preview;
    GtkWidget *button_show_image_preview;
    GtkWidget *button_show_diff_preview;

    GtkWidget *combo_output;

    GSList *markers;
    MarkerType *marker_selected;
    gboolean can_change_marker;

    gint xres;
    gdouble color_range;
    gboolean preview;
    gboolean preview_invalid;
    gboolean output_image;
    gboolean output_fft;
    gdouble zoom_factor;

} ControlsType;

/* Gwyddion Module Routines */
static gboolean     module_register     (const gchar *name);
static gboolean     run_main            (GwyContainer *data,
                                         GwyRunType run);

/* Signal handlers */
static gboolean     paint_fft           (GtkWidget *widget,
                                         GdkEventExpose *event,
                                         ControlsType *controls);
static gboolean     mouse_down_fft      (ControlsType *controls,
                                         GdkEventButton *event);
static gboolean     mouse_up_fft        (ControlsType *controls,
                                         GdkEventButton *event);
static gboolean     mouse_move_fft      (GtkWidget *widget,
                                         GdkEventMotion *event,
                                         ControlsType *controls);
static void         scale_changed_fft   (GtkRange *range,
                                         ControlsType *controls);
static void         remove_all_clicked  (ControlsType *controls);
static void         display_mode_changed(ControlsType *controls);
static void         zoom_toggled        (ControlsType *controls);

/* Helper Functions */
static gboolean     run_dialog          (ControlsType *controls);
static void         build_tooltips      (GHashTable *hash_tips);
static void         save_settings       (ControlsType *controls);
static void         load_settings       (ControlsType *controls,
                                         gboolean load_defaults);
static void         fft_filter_2d       (GwyDataField *input,
                                         GwyDataField *output_image,
                                         GwyDataField *output_fft,
                                         GSList *markers);
static void         do_fft              (GwyDataField *dataInput,
                                         GwyDataField *dataOutput);
static void         set_dfield_modulus  (GwyDataField *re,
                                         GwyDataField *im,
                                         GwyDataField *target);
static void         draw_marker         (GdkDrawable *pix_target,
                                         MarkerType *marker,
                                         GdkFunction function,
                                         ControlsType *controls);
static void         draw_markers        (GdkPixmap *pix_target,
                                         ControlsType *controls);
static MarkerType*  get_selected_marker (gdouble x,
                                         gdouble y,
                                         ControlsType *controls);
static void         calc_circle_box     (MarkerType *marker);
static GdkColor     get_color_from_rgb  (gint red,
                                         gint green,
                                         gint blue);
static void         screen_to_dfield    (gdouble *x,
                                         gdouble *y,
                                         ControlsType *controls);
static void         dfield_to_screen    (gdouble *x,
                                         gdouble *y,
                                         ControlsType *controls);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("2D FFT Filtering"),
    "Chris Anderson <sidewinder.asu@gmail.com>",
    "1.0",
    "Chris Anderson, Molecular Imaging Corp.",
    "2005",
};

/* This is the ONLY exported symbol. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo do_main_func_info = {
        "run_main",
        N_("/_Correct Data/_2D FFT filtering..."),
        (GwyProcessFunc)&run_main,
        FFTF_2D_RUN_MODES,
        0
    };

    gwy_process_func_register(name, &do_main_func_info);

    return TRUE;
}

static gboolean
run_main(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GwyContainer *cont_output_image;
    GtkWidget *win_output_image;
    GwyContainer *cont_output_fft;
    GtkWidget *win_output_fft;
    GwySIUnit *siunit;
    ControlsType controls;
    gboolean response;
    gdouble min, max;
    GSList *list;

    g_assert(run & FFTF_2D_RUN_MODES);

    /* Setup containers and data fields */
    controls.cont_data = data;
    controls.cont_fft = gwy_container_duplicate_by_prefix(data, "/0/data",
                                                          NULL);
    gwy_app_clean_up_data(controls.cont_fft);

    cont_output_image = gwy_container_duplicate_by_prefix(data, "/0/data",
                                                          "/0/base/palette",
                                                          NULL);
    cont_output_fft = gwy_container_duplicate_by_prefix(data, "/0/data", NULL);
    gwy_app_clean_up_data(cont_output_image);
    gwy_app_clean_up_data(cont_output_fft);
    controls.data_output_image = get_container_data(cont_output_image);
    controls.data_output_fft = get_container_data(cont_output_fft);

    response = run_dialog(&controls);

    if (response && controls.markers) {
        dfield = get_container_data(data);

        if (controls.preview_invalid)
            fft_filter_2d(dfield, controls.data_output_image,
                          controls.data_output_fft,
                          controls.markers);

        if (controls.output_fft) {
            gwy_container_set_string_by_name(cont_output_fft, "/0/base/palette",
                                             g_strdup("DFit"));
            siunit = gwy_data_field_get_si_unit_xy(controls.data_output_fft);
            gwy_si_unit_power(siunit, -1, siunit);
            min = gwy_data_field_get_min(controls.data_output_fft);
            max = controls.color_range;
            gwy_container_set_double_by_name(cont_output_fft,
                                             "/0/base/min", min);
            gwy_container_set_double_by_name(cont_output_fft,
                                             "/0/base/max", max);
            win_output_fft = gwy_app_data_window_create(cont_output_fft);
            gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(win_output_fft),
                                             _("Filtered FFT"));
        }

        if (controls.output_image) {
            win_output_image = gwy_app_data_window_create(cont_output_image);
            gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(win_output_image),
                                             _("Filtered Image"));
        }

        /* Unref unused container(s) */
        if (controls.output_fft && !controls.output_image)
            g_object_unref(cont_output_image);
        if (controls.output_image && !controls.output_fft)
            g_object_unref(cont_output_fft);
    } else {
        g_object_unref(cont_output_image);
        g_object_unref(cont_output_fft);
    }

    /* Finalize variables */
    g_object_unref(controls.buf_fft);
    g_object_unref(controls.bbuf_fft);
    g_object_unref(controls.buf_preview_fft);
    g_object_unref(controls.buf_preview_image);
    g_object_unref(controls.buf_original_image);
    g_object_unref(controls.buf_preview_diff);
    g_object_unref(controls.cont_fft);

    /* Free markers */
    list = controls.markers;
    while (list) {
        g_free(list->data);
        list = g_slist_next(list);
    }
    g_slist_free(controls.markers);
    controls.markers = NULL;

    return FALSE;
}

static gboolean
run_dialog(ControlsType *controls)
{
    static GwyEnum output_types[] = {
        { N_("Filtered Image"), OUTPUT_IMAGE              },
        { N_("Filtered FFT"),   OUTPUT_FFT                },
        { N_("Both"),           OUTPUT_IMAGE | OUTPUT_FFT },
    };
    GtkWidget *dialog;
    GtkWidget *table, *hbox, *hbox2, *hbox3;
    GtkWidget *table2;
    GtkWidget *label;
    GtkWidget *scale_fft;
    GtkWidget *image;
    GtkWidget *button = NULL;
    GtkWidget *combo;
    GtkTooltips *tips;
    GHashTable *hash_tips;
    GdkCursor *cursor;
    GwyDataField *data_fft;
    GwyDataField *data_original;
    GwyContainer *cont_fft;
    GwyGradient *gradient_fft;
    GwyGradient *gradient;
    const guchar *gradient_name = NULL;
    gint response;
    gint i;
    gint xres;
    gdouble min, max;
    gint row;

    /* Starting out, we are not in preview mode */
    controls->preview = FALSE;

    /* Initialize markers */
    controls->markers = NULL;
    controls->marker_selected = NULL;
    controls->preview_invalid = FALSE;
    controls->can_change_marker = FALSE;

    /* Setup containers and data fields */
    data_original = get_container_data(controls->cont_data);
    cont_fft = controls->cont_fft;
    data_fft = get_container_data(cont_fft);
    xres = gwy_data_field_get_xres(data_fft);
    controls->xres = xres;
    /* TODO: possibly add zoom options:
               100%, 200%, 300%, etc, Auto*/
    controls->zoom_factor = ((gdouble)xres / 128.0) * 0.8;

    /* Do the fft (for preview window) */
    do_fft(data_fft, data_fft);

    /* Initialize the pixbufs */
    gradient_fft = gwy_gradients_get_gradient("DFit");
    controls->buf_fft = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                       xres, xres);
    controls->buf_original_image = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                                  FALSE, 8,
                                                  xres, xres);
    controls->buf_preview_fft = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                               xres, xres);
    controls->buf_preview_image = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                                 xres, xres);
    controls->buf_preview_diff = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                                xres, xres);
    controls->bbuf_fft = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                        PREVIEW_SIZE, PREVIEW_SIZE);
    min = gwy_data_field_get_min(data_fft);
    max = gwy_data_field_get_rms(data_fft);
    controls->color_range = max;
    gwy_pixbuf_draw_data_field_with_range(controls->buf_fft, data_fft,
                                          gradient_fft, min, max);
    gwy_container_gis_string_by_name(controls->cont_data, "/0/base/palette",
                                     &gradient_name);
    if (!gradient_name)
        gradient_name = GWY_GRADIENT_DEFAULT;
    gradient = gwy_gradients_get_gradient(gradient_name);
    gwy_pixbuf_draw_data_field(controls->buf_original_image, data_original,
                               gradient);

    /* Setup the dialog window */
    dialog = gtk_dialog_new_with_buttons(_("2D FFT Filtering"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls->dialog = dialog;

    /* Setup tooltips */
    hash_tips = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
    build_tooltips(hash_tips);
    tips = gtk_tooltips_new();
    g_object_ref(tips);
    gtk_object_sink(GTK_OBJECT(tips));

    /* Setup all the widgets */
    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 5);

    controls->draw_fft = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(controls->draw_fft),
                          PREVIEW_SIZE, PREVIEW_SIZE);
    gtk_box_pack_start(GTK_BOX(hbox), controls->draw_fft, FALSE, FALSE, 5);

    table = gtk_table_new(20, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 10);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Filter Drawing</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    button = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(button));
    image = gtk_image_new_from_stock(GWY_STOCK_MASK_CIRCLE_INCLUSIVE,
                                     GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(button), image);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "circle_inclusive"),
                         "");
    gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
    controls->button_circle_inc = button;

    button = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(button));
    image = gtk_image_new_from_stock(GWY_STOCK_MASK_RECT_INCLUSIVE,
                                     GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(button), image);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "rectangle_inclusive"),
                         "");
    gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
    controls->button_rect_inc = button;

    button = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(button));
    image = gtk_image_new_from_stock(GWY_STOCK_MASK_CIRCLE_EXCLUSIVE,
                                     GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(button), image);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "circle_exclusive"),
                         "");
    gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
    controls->button_circle_exc = button;

    button = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(button));
    image = gtk_image_new_from_stock(GWY_STOCK_MASK_RECT_EXCLUSIVE,
                                     GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(button), image);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "rectangle_exclusive"),
                         "");
    gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
    controls->button_rect_exc = button;

    hbox3 = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), hbox3, FALSE, FALSE, 5);

    button = radio_new(GTK_RADIO_BUTTON(button), _("_Edit"));
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "drag"), "");
    gtk_container_add(GTK_CONTAINER(hbox3), button);
    controls->button_drag = button;

    button = radio_new(GTK_RADIO_BUTTON(button), _("Re_move"));
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "remove"), "");
    gtk_container_add(GTK_CONTAINER(hbox3), button);
    controls->button_remove = button;

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    controls->check_origin = check_new(_("_Snap to origin"));
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), controls->check_origin,
                         g_hash_table_lookup(hash_tips, "origin"), "");
    gtk_table_attach(GTK_TABLE(table), controls->check_origin, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    hbox2 = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox2), GTK_BUTTONBOX_START);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    button = gtk_button_new_with_mnemonic(_("Remove _All"));
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "remove_all"), "");
    gtk_container_add(GTK_CONTAINER(hbox2), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(remove_all_clicked), controls);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 15);
    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Preview Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    scale_fft = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(CR_DEFAULT,
                                                                 0.1,
                                                                 CR_MAX,
                                                                 0.1, 1, 1)));
    gtk_scale_set_digits(GTK_SCALE(scale_fft), 3);
    gtk_scale_set_draw_value(GTK_SCALE(scale_fft), FALSE);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), scale_fft,
                         g_hash_table_lookup(hash_tips, "color_range"), "");
    gtk_box_pack_end(GTK_BOX(hbox2), scale_fft, TRUE, TRUE, 0);
    controls->scale_fft = scale_fft;

    label = gtk_label_new_with_mnemonic(_("Color Ran_ge:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), scale_fft);
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("_Zoom"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(zoom_toggled), controls);
    gtk_table_attach(GTK_TABLE(table), button, 0, 1, row, row+1,
                     GTK_FILL, GTK_FILL, 0, 2);
    controls->check_zoom = button;
    row++;

    label = gtk_label_new(_("Display mode:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.05);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, GTK_FILL, 0, 2);
    row++;

    table2 = gtk_table_new(3, 2, FALSE);
    gtk_table_attach(GTK_TABLE(table), table2, 0, 2, row, row+1,
                     GTK_FILL, GTK_FILL, 0, 2);

    button = gtk_radio_button_new_with_mnemonic(NULL, _("Original _FFT"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(display_mode_changed), controls);
    gtk_table_attach(GTK_TABLE(table2), button, 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 1);
    controls->button_show_fft = button;

    button = radio_new(GTK_RADIO_BUTTON(button), _("Original _Image"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(display_mode_changed), controls);
    gtk_table_attach(GTK_TABLE(table2), button, 1, 2, 0, 1,
                     GTK_FILL, GTK_FILL, 5, 1);
    controls->button_show_original_image = button;

    button = radio_new(GTK_RADIO_BUTTON(button), _("Filtered _Image"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(display_mode_changed), controls);
    gtk_table_attach(GTK_TABLE(table2), button, 1, 2, 1, 2,
                     GTK_FILL, GTK_FILL, 5, 1);
    controls->button_show_image_preview = button;

    button = radio_new(GTK_RADIO_BUTTON(button), _("Image _Difference"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(display_mode_changed), controls);
    gtk_table_attach(GTK_TABLE(table2), button, 1, 2, 2, 3,
                     GTK_FILL, GTK_FILL, 5, 1);
    controls->button_show_diff_preview = button;

    button = radio_new(GTK_RADIO_BUTTON(button), _("Filtered _FFT"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(display_mode_changed), controls);
    gtk_table_attach(GTK_TABLE(table2), button, 0, 1, 1, 2,
                     GTK_FILL, GTK_FILL, 0, 1);
    controls->button_show_fft_preview = button;
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

    //combo = gwy_option_menu_create(output_types, G_N_ELEMENTS(output_types),
    //                               "output-type", NULL, NULL, OUTPUT_IMAGE);
    gtk_table_attach(GTK_TABLE(table), combo, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls->combo_output = combo;

    label = gtk_label_new_with_mnemonic(_("Output _type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 0, 0);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    /* Set up signal/event handlers */
    g_signal_connect(scale_fft, "value_changed",
                     G_CALLBACK(scale_changed_fft), controls);
    g_signal_connect(controls->draw_fft, "expose_event",
                     G_CALLBACK(paint_fft), controls);
    g_signal_connect_swapped(controls->draw_fft, "button_press_event",
                             G_CALLBACK(mouse_down_fft), controls);
    g_signal_connect_swapped(controls->draw_fft, "button_release_event",
                             G_CALLBACK(mouse_up_fft), controls);
    g_signal_connect(controls->draw_fft, "motion_notify_event",
                     G_CALLBACK(mouse_move_fft), controls);
    gtk_widget_set_events(controls->draw_fft,
                          GDK_EXPOSURE_MASK
                          | GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK
                          | GDK_POINTER_MOTION_MASK
                          | GDK_POINTER_MOTION_HINT_MASK);

    gtk_widget_show_all(dialog);
    load_settings(controls, FALSE);
    cursor = gdk_cursor_new(GDK_CROSSHAIR);
    gdk_window_set_cursor(controls->draw_fft->window, cursor);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            save_settings(controls);
            gtk_widget_destroy(dialog);
            return FALSE;

            case GTK_RESPONSE_OK:
            save_settings(controls);
            break;

            case RESPONSE_RESET:
            load_settings(controls, TRUE);
            set_toggled(controls->button_show_fft, TRUE);
            set_toggled(controls->button_circle_inc, TRUE);
            gtk_widget_queue_draw_area(controls->draw_fft, 0, 0,
                                       PREVIEW_SIZE, PREVIEW_SIZE);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    i = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->combo_output));
    //i = get_combo_index(controls->combo_output, "output-type");
    controls->output_image = i & OUTPUT_IMAGE;
    controls->output_fft = i & OUTPUT_FFT;

    /* Finalize */
    gtk_widget_destroy(dialog);
    g_object_unref(tips);
    g_hash_table_destroy(hash_tips);

    return TRUE;
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
    g_hash_table_insert(hash_tips, "drag",
        _("Click and drag a marker to resize. Hold Shift to move marker."));
    g_hash_table_insert(hash_tips, "remove",
        _("Click on a marker to remove it."));
    g_hash_table_insert(hash_tips, "remove_all",
        _("Removes all markers."));
    g_hash_table_insert(hash_tips, "origin",
        _("Forces new markers to center around the origin."));
    g_hash_table_insert(hash_tips, "color_range",
        _("Changes the range of values displayed in the FFT."));
}

static void
save_settings(ControlsType *controls)
{
    GwyContainer *settings;
    gint i;

    settings = gwy_app_settings_get();

    i = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->combo_output));
    gwy_container_set_int32_by_name(settings,
                                    "/module/fft_filter_2d/combo_output",
                                    i);
    //gwy_container_set_int32_by_name(settings,
    //                                "/module/fft_filter_2d/combo_output",
    //                                get_combo_index(controls->combo_output,
    //                                                "output-type"));

    gwy_container_set_boolean_by_name(settings,
                                      "/module/fft_filter_2d/check_origin",
                                      get_toggled(controls->check_origin));
    gwy_container_set_double_by_name(settings,
                    "/module/fft_filter_2d/color_range",
                    gtk_range_get_value(GTK_RANGE(controls->scale_fft)));
    gwy_container_set_boolean_by_name(settings,
                                      "/module/fft_filter_2d/zoom",
                                      get_toggled(controls->check_zoom));
}

static void
load_settings(ControlsType *controls, gboolean load_defaults)
{
    GwyContainer *settings;
    gint output;
    gboolean origin;
    gdouble color;
    gboolean zoom;

    /* Set defaults */
    output = 2;
    origin = FALSE;
    color = CR_DEFAULT;
    zoom = FALSE;

    /* Load settings */
    if (!load_defaults) {
        settings = gwy_app_settings_get();
        gwy_container_gis_int32_by_name(settings,
                                        "/module/fft_filter_2d/combo_output",
                                        &output);
        gwy_container_gis_boolean_by_name(settings,
                                          "/module/fft_filter_2d/check_origin",
                                          &origin);
        gwy_container_gis_double_by_name(settings,
                                         "/module/fft_filter_2d/color_range",
                                         &color);
        gwy_container_gis_boolean_by_name(settings,
                                          "/module/fft_filter_2d/zoom",
                                          &zoom);
    }

    /* Change controls to match settings */
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->combo_output),
                                  output);
    //set_combo_index(controls->combo_output, "output-type", output);
    set_toggled(controls->check_origin, origin);
    gtk_range_set_value(GTK_RANGE(controls->scale_fft), color);
    set_toggled(controls->check_zoom, zoom);
}

static gboolean
paint_fft(GtkWidget *widget,
          G_GNUC_UNUSED GdkEventExpose *event,
          ControlsType *controls)
{
    GdkPixmap *pix_work;
    GdkPixbuf *buf_temp = NULL;
    GdkPixbuf *buf_source;
    gboolean zoom = FALSE;
    gdouble offset, scale;
    zoom = get_toggled(controls->check_zoom);

    /* Create temp drawable so we can draw shapes onto the FFT image */
    pix_work = gdk_pixmap_new(widget->window, PREVIEW_SIZE, PREVIEW_SIZE, -1);

    /* Create a temp pixbuf for scaling operations (only if zooming) */
    if (zoom)
        buf_temp = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                  PREVIEW_SIZE, PREVIEW_SIZE);

    /* Pick what our source buffer is (depending on preview mode) */
    if (controls->preview) {
        if (get_toggled(controls->button_show_fft_preview))
            buf_source = controls->buf_preview_fft;
        else if (get_toggled(controls->button_show_image_preview))
            buf_source = controls->buf_preview_image;
        else if (get_toggled(controls->button_show_original_image))
            buf_source = controls->buf_original_image;
        else if (get_toggled(controls->button_show_diff_preview))
            buf_source = controls->buf_preview_diff;
        else
            buf_source = controls->buf_fft;
    } else {
        buf_source = controls->buf_fft;
    }

    /* Resize the stored buffer to the preview size */
    if (zoom && buf_temp) {
        offset = -(gdouble)PREVIEW_SIZE*(controls->zoom_factor - 1.0)/2.0;
        scale = controls->zoom_factor *
                ((gdouble)PREVIEW_SIZE/(gdouble)controls->xres);

        gdk_pixbuf_scale(buf_source, buf_temp, 0, 0,
                         PREVIEW_SIZE, PREVIEW_SIZE,
                         offset, offset, scale, scale,
                         GDK_INTERP_BILINEAR);
    }
    else {
        buf_temp = gdk_pixbuf_scale_simple(buf_source,
                                           PREVIEW_SIZE, PREVIEW_SIZE,
                                           GDK_INTERP_BILINEAR);
    }

    /* Draw the resized pixbuf onto pix_work */
    gdk_draw_pixbuf(pix_work, NULL, buf_temp, 0, 0, 0, 0,
                    PREVIEW_SIZE, PREVIEW_SIZE,
                    GDK_RGB_DITHER_NONE, 0, 0);

    /* Draw the markers onto pix_work */
    if (!controls->preview)
        draw_markers(pix_work, controls);

    /* Draw the pix_work onto the backbuffer */
    gdk_pixbuf_get_from_drawable(controls->bbuf_fft, pix_work,
                                 gdk_drawable_get_colormap(pix_work),
                                 0, 0, 0, 0, PREVIEW_SIZE, PREVIEW_SIZE);

    /* Flip backbuffer to window */
    gdk_draw_pixbuf(widget->window, NULL, controls->bbuf_fft, 0, 0, 0, 0,
                    PREVIEW_SIZE, PREVIEW_SIZE, GDK_RGB_DITHER_NONE, 0, 0);

    /* Finalize */
    g_object_unref(pix_work);
    g_object_unref(buf_temp);

    return TRUE;
}

static MarkerType*
get_selected_marker(gdouble x, gdouble y, ControlsType *controls)
{
    MarkerType *marker;
    gdouble distance;
    gdouble distance1, distance2;
    gdouble d1, d2, d3, d4;
    gdouble min_proximity = 99999.9;
    MarkerType *min_marker = NULL;
    gdouble proximity = 0;
    GSList *list;

    /* Go through all markers, and calculate their proximity to the cursor.
       Return the marker with the closest proximity. */
    list = controls->markers;
    while (list) {
        marker = list->data;

        if (marker->shape == MARKER_CIRCLE) {
            distance = get_distance(marker->x_center, x, marker->y_center, y);
            proximity = ABS(marker->radius - distance);
        } else {
            d1 = get_distance(marker->left, x, marker->top, y);
            d2 = get_distance(marker->right, x, marker->top, y);
            d3 = get_distance(marker->left, x, marker->bottom, y);
            d4 = get_distance(marker->right, x, marker->bottom, y);
            distance1 = d1 < d2 ? d1 : d2;
            distance2 = d3 < d4 ? d3 : d4;
            proximity = distance1 < distance2 ? distance1 : distance2;
        }

        if (proximity < min_proximity) {
            min_proximity = proximity;
            min_marker = marker;
        }

        list = g_slist_next(list);
    }

    /* If the minimum proximity is within tolerance, return the marker */
    if (min_proximity <= 5)
        return min_marker;

    return NULL;
}

static void
draw_marker(GdkDrawable *pix_target, MarkerType *marker,
            GdkFunction function, ControlsType *controls)
{
    MarkerType marker_a, marker_b;
    GdkColor color_a, color_b;
    GdkGC *gc_a, *gc_b;

    /* Setup graphics context */
    gc_a = gdk_gc_new(pix_target);
    gc_b = gdk_gc_new(pix_target);
    gdk_gc_set_function(gc_a, function);
    gdk_gc_set_function(gc_b, function);

    /* Set up coordinates */
    if (marker->shape == MARKER_CIRCLE) {
        /* CIRCLE */
        marker_a.radius = marker->radius;
        marker_a.x_center = marker->x_center;
        marker_a.y_center = marker->y_center;

        marker_b.radius = marker_a.radius;
        marker_b.x_center = -marker_a.x_center + controls->xres;
        marker_b.y_center = -marker_a.y_center + controls->xres;

        marker_a.left = marker_a.x_center - marker_a.radius;
        marker_a.right = marker_a.x_center + marker_a.radius;
        marker_a.top = marker_a.y_center - marker_a.radius;
        marker_a.bottom = marker_a.y_center + marker_a.radius;

        marker_b.left = marker_b.x_center - marker_b.radius;
        marker_b.right = marker_b.x_center + marker_b.radius;
        marker_b.top = marker_b.y_center - marker_b.radius;
        marker_b.bottom = marker_b.y_center + marker_b.radius;
    } else {
        /* RECTANGLE */
        marker_a.left = marker->left;
        marker_a.right = marker->right;
        marker_a.top = marker->top;
        marker_a.bottom = marker->bottom;

        /* Fix orientation */
        if (marker_a.left > marker_a.right)
            GWY_SWAP(gdouble, marker_a.left, marker_a.right);
        if (marker_a.top > marker_a.bottom)
            GWY_SWAP(gdouble, marker_a.top, marker_a.bottom);

        /* Calculate secondary coordinates */
        marker_b.left = -marker_a.left + controls->xres;
        marker_b.right = -marker_a.right + controls->xres;
        marker_b.top = -marker_a.top + controls->xres;
        marker_b.bottom = -marker_a.bottom + controls->xres;

        /* Fix orientation */
        if (marker_b.left > marker_b.right)
            GWY_SWAP(gdouble, marker_b.left, marker_b.right);
        if (marker_b.top > marker_b.bottom)
            GWY_SWAP(gdouble, marker_b.top, marker_b.bottom);
    }

    /* Convert to screen coordinate system */
    dfield_to_screen(&marker_a.left, &marker_a.top, controls);
    dfield_to_screen(&marker_a.right, &marker_a.bottom, controls);
    dfield_to_screen(&marker_b.left, &marker_b.top, controls);
    dfield_to_screen(&marker_b.right, &marker_b.bottom, controls);

    /* Nudge (only for circles) */
    if (marker->shape == MARKER_CIRCLE) {
        marker_b.left+=1;
        marker_b.right+=1;
        marker_b.top+=1;
        marker_b.bottom+=1;
    }

    /* Set draw color  */
    if (function == GDK_COPY) {
        if (marker->inclusive) {
            color_a = get_color_from_rgb(0, 255, 0);
            color_b = get_color_from_rgb(0, 255, 125);
        } else {
            color_a = get_color_from_rgb(255, 0, 0);
            color_b = get_color_from_rgb(255, 0, 125);
        }
    } else {
        color_a = get_color_from_rgb(255, 255, 255);
        color_b = get_color_from_rgb(255, 200, 255);
    }
    gdk_gc_set_rgb_fg_color(gc_a, &color_a);
    gdk_gc_set_rgb_bg_color(gc_a, &color_a);
    gdk_gc_set_rgb_fg_color(gc_b, &color_b);
    gdk_gc_set_rgb_bg_color(gc_b, &color_b);

    if (marker->shape == MARKER_CIRCLE) {
        gdk_draw_arc(pix_target, gc_a, function==GDK_COPY ? FALSE : TRUE,
                     marker_a.left, marker_a.top,
                     marker_a.right - marker_a.left,
                     marker_a.bottom - marker_a.top,
                     0, 23040);

        if (!(marker_a.x_center == marker_b.x_center &&
              marker_a.y_center == marker_b.y_center))
        gdk_draw_arc(pix_target, gc_b,  function==GDK_COPY ? FALSE : TRUE,
                     marker_b.left, marker_b.top,
                     marker_b.right - marker_b.left,
                     marker_b.bottom - marker_b.top,
                     0, 23040);
    } else {
        gdk_draw_rectangle(pix_target, gc_a, function==GDK_COPY ? FALSE : TRUE,
                           marker_a.left, marker_a.top,
                           marker_a.right - marker_a.left,
                           marker_a.bottom - marker_a.top);

        gdk_draw_rectangle(pix_target, gc_b, function==GDK_COPY ? FALSE : TRUE,
                           marker_b.left, marker_b.top,
                           marker_b.right - marker_b.left,
                           marker_b.bottom - marker_b.top);
    }

    /* Finalize */
    g_object_unref(gc_a);
    g_object_unref(gc_b);
}

static void
draw_markers(GdkPixmap *pix_target, ControlsType *controls)
{
    GSList *list;
    MarkerType *marker;
    MarkerType *marker_selected;

    marker_selected = NULL;
    list = controls->markers;
    while (list) {
        marker = list->data;
        if (marker == controls->marker_selected)
            marker_selected = marker;
        else
            draw_marker(pix_target, marker, GDK_COPY, controls);
        list = g_slist_next(list);
    }

    if (marker_selected != NULL)
        draw_marker(pix_target, marker_selected, GDK_XOR, controls);
}

static void
calc_circle_box(MarkerType *marker)
{
    marker->left = marker->x_center - marker->radius;
    marker->right = marker->x_center + marker->radius;
    marker->top = marker->y_center - marker->radius;
    marker->bottom = marker->y_center + marker->radius;
}

static GdkColor
get_color_from_rgb(gint red, gint green, gint blue)
{
    GdkColor col;

    col.red = 256 * red;
    col.green = 256 * green;
    col.blue = 256 * blue;
    col.pixel = 0;

    return col;
}

static void
screen_to_dfield(gdouble *x, gdouble *y, ControlsType *controls)
{
    gboolean zoom = FALSE;
    gdouble scale_factor;
    gdouble zoom_factor = 1.0;
    gdouble offset;

    /* Setup zooming */
    zoom = get_toggled(controls->check_zoom);
    if (zoom)
        zoom_factor = controls->zoom_factor;

    /* Subtract off zoom offset */
    if (zoom) {
        offset = (gdouble)PREVIEW_SIZE *
                 (controls->zoom_factor - 1.0)/2.0;
        *x += offset;
        *y += offset;
    }

    /* Convert from screen coordinates to dfield coordinates */
    scale_factor = ((gdouble)controls->xres/(gdouble)PREVIEW_SIZE);
    *x *= scale_factor / zoom_factor;
    *y *= scale_factor / zoom_factor;
}

static void
dfield_to_screen(gdouble *x, gdouble *y, ControlsType *controls)
{
    gboolean zoom = FALSE;
    gdouble scale_factor;
    gdouble zoom_factor = 1.0;
    gdouble offset;

    /* Setup zooming */
    zoom = get_toggled(controls->check_zoom);
    if (zoom)
        zoom_factor = controls->zoom_factor;

    /* Convert from dfield coordinates to screen coordinates */
    scale_factor = ((gdouble)PREVIEW_SIZE/((gdouble)controls->xres));
    *x *= scale_factor * zoom_factor;
    *y *= scale_factor * zoom_factor;

    /* Add in zoom offset */
    if (zoom) {
        offset = -(gdouble)PREVIEW_SIZE *
                  (controls->zoom_factor - 1.0)/2.0;
        *x += offset;
        *y += offset;
    }
}

static gboolean
mouse_down_fft(ControlsType *controls, GdkEventButton *event)
{
    gdouble x, y;
    MarkerType *marker;

    if (controls->preview)
        return TRUE;

    x = event->x; y = event->y;
    screen_to_dfield(&x, &y, controls);

    if (get_toggled(controls->button_drag)
        || get_toggled(controls->button_remove)) {
        controls->marker_selected = get_selected_marker(x, y, controls);
        if (controls->marker_selected != NULL)
            controls->can_change_marker = TRUE;
        gtk_widget_queue_draw_area(controls->draw_fft, 0, 0,
                                   PREVIEW_SIZE, PREVIEW_SIZE);
    }
    else {
        /* Create new marker */
        marker = g_new(MarkerType, 1);
        if (get_toggled(controls->check_origin)) {
            marker->x_center = controls->xres / 2.0;
            marker->y_center = controls->xres / 2.0;
        } else {
            marker->x_center = x;
            marker->y_center = y;
        }
        marker->left = marker->x_center;
        marker->right = marker->x_center;
        marker->top = marker->y_center;
        marker->bottom = marker->y_center;
        marker->radius = 0;

        marker->inclusive = get_toggled(controls->button_circle_inc) ||
                            get_toggled(controls->button_rect_inc);
        marker->shape = get_toggled(controls->button_circle_inc) ||
                        get_toggled(controls->button_circle_exc) ?
                        MARKER_CIRCLE : MARKER_RECT;

        /* Add marker to linked list */
        controls->markers = g_slist_append(controls->markers, marker);
        controls->preview_invalid = TRUE;
        controls->can_change_marker = TRUE;
    }

    return TRUE;
}

static gboolean
mouse_up_fft(ControlsType *controls, G_GNUC_UNUSED GdkEventButton *event)
{
    MarkerType *marker;
    GSList *list;
    gboolean remove_marker;
    gboolean editing;

    if (controls->preview)
        return TRUE;

    editing = get_toggled(controls->button_drag)
              || get_toggled(controls->button_remove);
    if (editing && controls->marker_selected) {
        marker = controls->marker_selected;
    }
    else if (!editing && controls->markers) {
        list = g_slist_last(controls->markers);
        marker = list->data;
    }
    else {
        return TRUE;
    }

    /* XOR Marker (clear it away) */
    draw_marker(controls->draw_fft->window, marker, GDK_XOR, controls);

    /* Remove Marker if remove button selected, or if the user resized the
     * marker to be zero sized */
    remove_marker = (get_toggled(controls->button_remove)
                     && controls->marker_selected)
                    || is_marker_zero_sized(marker);
    if (remove_marker) {
        list = g_slist_find(controls->markers, marker);
        controls->markers = g_slist_delete_link(controls->markers, list);
        g_free(marker);
        controls->preview_invalid = TRUE;
    } else {
        /* Redraw marker with proper color */
        draw_marker(controls->draw_fft->window, marker, GDK_COPY, controls);
    }

    controls->marker_selected = NULL;
    controls->can_change_marker = FALSE;

    return TRUE;
}

static gboolean
mouse_move_fft(GtkWidget *widget, GdkEventMotion *event, ControlsType *controls)
{
    gint screen_x, screen_y;
    gdouble x, y;
    MarkerType *marker;
    MarkerType backup_marker;
    GdkModifierType state;
    gdouble radius_x, radius_y;
    gdouble width, height;
    gboolean constrain;
    GSList *list;

    if (controls->preview)
        return TRUE;

    if (event->is_hint)
        gdk_window_get_pointer(widget->window, &screen_x, &screen_y, &state);
    else {
        screen_x = event->x;
        screen_y = event->y;
        state = event->state;
    }

    x = (gdouble)screen_x;
    y = (gdouble)screen_y;

    if (controls->preview) {

    }
    else if ((state & GDK_BUTTON1_MASK)
             && controls->markers
             && controls->can_change_marker) {
        if (get_toggled(controls->button_remove))
            return TRUE;
        screen_to_dfield(&x, &y, controls);

        if (controls->marker_selected) {
            marker = controls->marker_selected;
        }
        else if (!get_toggled(controls->button_drag)) {
            list = g_slist_last(controls->markers);
            marker = list->data;
        }
        else {
            return TRUE;
        }

        /* XOR the last cursor marker */
        draw_marker(widget->window, marker, GDK_XOR, controls);

        /* Update the marker dimensions */
        if (marker->shape == MARKER_CIRCLE) {
            backup_marker.radius = marker->radius;
            backup_marker.x_center = marker->x_center;
            backup_marker.y_center = marker->y_center;
            if (state & GDK_SHIFT_MASK) {
                marker->x_center = x;
                marker->y_center = y;
                calc_circle_box(marker);
            } else {
                radius_x = ABS(x - marker->x_center);
                radius_y = ABS(y - marker->y_center);
                marker->radius = (radius_x > radius_y) ? radius_x : radius_y;
                calc_circle_box(marker);
            }

            /* Constrain marker to stay within window */
            constrain = marker->left < 0 || marker->right >= controls->xres ||
                        marker->top < 0 || marker->bottom >= controls->xres;
            if (constrain) {
                marker->radius = backup_marker.radius;
                marker->x_center = backup_marker.x_center;
                marker->y_center = backup_marker.y_center;
                calc_circle_box(marker);
            }

        }
        else {

            if (state & GDK_SHIFT_MASK) {
                width = marker->right - marker->left;
                height = marker->bottom - marker->top;
                marker->right = x;
                marker->bottom = y;
                marker->left = x - width;
                marker->top = y - height;
            } else {
                marker->right = x;
                marker->bottom = y;
            }

            /* Constrain marker to stay within window */
            if (marker->right < 0)
                marker->right = 0;
            if (marker->right >= controls->xres)
                marker->right = controls->xres - 1;
            if (marker->bottom < 0)
                marker->bottom = 0;
            if (marker->bottom >= controls->xres)
                marker->bottom = controls->xres - 1;
        }

        /* XOR the new cursor marker */
        draw_marker(widget->window, marker, GDK_XOR, controls);
        controls->preview_invalid = TRUE;
    }

    return TRUE;
}

static void
scale_changed_fft(GtkRange *range, ControlsType *controls)
{
    GwyGradient *gradient_fft;
    GwyDataField *dfield;
    gdouble value;
    gdouble min, max;

    value = gtk_range_get_value(range);
    dfield = get_container_data(controls->cont_fft);
    min = gwy_data_field_get_min(dfield);
    max = gwy_data_field_get_rms(dfield) * value;
    controls->color_range = max;

    gradient_fft = gwy_gradients_get_gradient("DFit");
    gwy_pixbuf_draw_data_field_with_range(controls->buf_fft, dfield,
                                          gradient_fft, min, max);

    set_toggled(controls->button_show_fft, TRUE);

    controls->preview_invalid = TRUE;
    gtk_widget_queue_draw_area(controls->draw_fft, 0, 0,
                               PREVIEW_SIZE, PREVIEW_SIZE);
}

static void
remove_all_clicked(ControlsType *controls)
{
    GSList *list;

    list = controls->markers;
    while (list) {
        g_free(list->data);
        list = g_slist_next(list);
    }
    g_slist_free(controls->markers);
    controls->markers = NULL;
    controls->preview_invalid = TRUE;
    set_toggled(controls->button_show_fft, TRUE);
    gtk_widget_queue_draw_area(controls->draw_fft, 0, 0,
                               PREVIEW_SIZE, PREVIEW_SIZE);
}

static void
display_mode_changed(ControlsType *controls)
{
    GwyDataField *dfield;
    GwyContainer *cont_diff;
    GwyDataField *data_diff;
    GwyGradient *gradient;
    GwyGradient *gradient_fft;
    const guchar *gradient_name = NULL;
    gdouble min, max;

    if (get_toggled(controls->button_show_fft)) {
        controls->preview = FALSE;
        gtk_widget_queue_draw_area(controls->draw_fft, 0, 0,
                                   PREVIEW_SIZE, PREVIEW_SIZE);
    } else {
        if (controls->preview_invalid) {
            /* Run the 2d fft filter on the original data */
            dfield = get_container_data(controls->cont_data);
            fft_filter_2d(dfield, controls->data_output_image,
                          controls->data_output_fft,
                          controls->markers);

            /* Paint the filtered image output into preview pixbuf */
            gwy_container_gis_string_by_name(controls->cont_data,
                                             "/0/base/palette",
                                             &gradient_name);
            if (gradient_name == NULL)
                gradient_name = GWY_GRADIENT_DEFAULT;
            gradient = gwy_gradients_get_gradient(gradient_name);
            gwy_pixbuf_draw_data_field(controls->buf_preview_image,
                                       controls->data_output_image,
                                       gradient);

            /* Paint the filtered fft output into preview pixbuf */
            gradient_fft = gwy_gradients_get_gradient("DFit");
            min = gwy_data_field_get_min(controls->data_output_fft);
            max = controls->color_range;
            gwy_pixbuf_draw_data_field_with_range(controls->buf_preview_fft,
                                                  controls->data_output_fft,
                                                  gradient_fft,
                                                  min, max);

            /* Calculate the difference between original image, and filtered
               image, and paint it into preview pixbuf */
            cont_diff = gwy_container_duplicate_by_prefix(controls->cont_data,
                                                          "/0/data", NULL);
            data_diff = get_container_data(cont_diff);
            gwy_data_field_subtract_fields(data_diff,
                                           controls->data_output_image,
                                           data_diff);
            gwy_pixbuf_draw_data_field(controls->buf_preview_diff,
                                       data_diff,
                                       gradient);
            g_object_unref(cont_diff);
        }

        controls->preview = TRUE;
        controls->preview_invalid = FALSE;
        gtk_widget_queue_draw_area(controls->draw_fft, 0, 0,
                                   PREVIEW_SIZE, PREVIEW_SIZE);
    }
}

static void
zoom_toggled(ControlsType *controls)
{
    gtk_widget_queue_draw_area(controls->draw_fft, 0, 0,
                               PREVIEW_SIZE, PREVIEW_SIZE);
}

static void
set_dfield_modulus(GwyDataField *re, GwyDataField *im, GwyDataField *target)
{
    gint i, j;
    gint xres;
    gdouble rval, ival;
    gdouble *re_data, *im_data, *target_data;

    xres = gwy_data_field_get_xres(re);

    re_data = gwy_data_field_get_data(re);
    im_data = gwy_data_field_get_data(im);
    target_data = gwy_data_field_get_data(target);

    for (i=0; i<xres; i++) {
        for (j=0; j<xres; j++) {
            rval = re_data[j*xres + i];
            ival = im_data[j*xres + i];
            target_data[j*xres + i] = sqrt(sqrt(rval*rval + ival*ival));
        }
    }
}

static void
do_fft(GwyDataField *data_input, GwyDataField *data_output)
{
    GwyDataField *i_in, *r_out, *i_out;
    GwyWindowingType window = GWY_WINDOWING_NONE;
    GwyTransformDirection direction = GWY_TRANSFORM_DIRECTION_FORWARD;
    GwyInterpolationType interp = GWY_INTERPOLATION_BILINEAR;

    i_in = GWY_DATA_FIELD(gwy_data_field_new_alike(data_input, TRUE));
    r_out = GWY_DATA_FIELD(gwy_data_field_new_alike(data_input, TRUE));
    i_out = GWY_DATA_FIELD(gwy_data_field_new_alike(data_input, TRUE));

    gwy_data_field_2dfft(data_input, i_in, r_out, i_out,
                         window, direction, interp, FALSE, FALSE);
    /*//
    gwy_data_field_2dfft(data_input, i_in, r_out, i_out, gwy_data_line_fft_hum,
                         window, 1, interp, 0, 0);
    */
    gwy_data_field_2dfft_humanize(r_out);
    gwy_data_field_2dfft_humanize(i_out);

    set_dfield_modulus(r_out, i_out, data_output);

    g_object_unref(i_out);
    g_object_unref(r_out);
    g_object_unref(i_in);
}

static void
fft_filter_2d(GwyDataField *input, GwyDataField *output_image,
              GwyDataField *output_fft, GSList *markers)
{
    GwyDataField *r_in, *i_in, *r_out, *i_out;
    GwyDataField *mask;
    MarkerType marker_a, marker_b, *marker;
    gint fill_bit;
    gint xres;
    GSList *list;

    /* Prepare the mask dfield */
    xres = gwy_data_field_get_xres(input);
    mask = GWY_DATA_FIELD(gwy_data_field_new_alike(input, TRUE));
    /* Check to see if there are any inclusive markers.
       If there are, the mask should be 0's by default.
       If there are not, it should be 1's by default    */
    list = markers;
    if (g_slist_length(list) > 0) {
        marker = list->data;
        if (!marker->inclusive)
            gwy_data_field_fill(mask, 1);
    }
    else
        gwy_data_field_fill(mask, 1);
    /* Draw the markers onto the mask */
    list = markers;
    while (list) {
        marker = list->data;

        if (marker->inclusive)
            fill_bit = 1;
        else
            fill_bit = 0;

        if (marker->shape == MARKER_CIRCLE) {
            marker_a.radius = marker->radius;
            marker_a.x_center = marker->x_center;
            marker_a.y_center = marker->y_center;

            marker_b.radius = marker_a.radius;
            marker_b.x_center = -marker_a.x_center + xres;
            marker_b.y_center = -marker_a.y_center + xres;

            marker_a.left = marker_a.x_center - marker_a.radius;
            marker_a.right = marker_a.x_center + marker_a.radius;
            marker_a.top = marker_a.y_center - marker_a.radius;
            marker_a.bottom = marker_a.y_center + marker_a.radius;

            marker_b.left = marker_b.x_center - marker_b.radius;
            marker_b.right = marker_b.x_center + marker_b.radius;
            marker_b.top = marker_b.y_center - marker_b.radius;
            marker_b.bottom = marker_b.y_center + marker_b.radius;

            gwy_data_field_elliptic_area_fill(mask, marker_a.left, marker_a.top,
                                              marker_a.right, marker_a.bottom,
                                              fill_bit);
            gwy_data_field_elliptic_area_fill(mask, marker_b.left, marker_b.top,
                                              marker_b.right, marker_b.bottom,
                                              fill_bit);
        } else {
            marker_a.left = marker->left;
            marker_a.right = marker->right;
            marker_a.top = marker->top;
            marker_a.bottom = marker->bottom;

            marker_b.left = -marker_a.left + xres;
            marker_b.right = -marker_a.right + xres;
            marker_b.top = -marker_a.top + xres;
            marker_b.bottom = -marker_a.bottom + xres;

            gwy_data_field_area_fill(mask, marker_a.left, marker_a.top,
                                     marker_a.right, marker_a.bottom, fill_bit);
            gwy_data_field_area_fill(mask, marker_b.left, marker_b.top,
                                     marker_b.right, marker_b.bottom, fill_bit);
        }

        list = g_slist_next(list);
    }

    /* Run the forward FFT */
    r_in = GWY_DATA_FIELD(gwy_data_field_new_alike(input, TRUE));
    i_in = GWY_DATA_FIELD(gwy_data_field_new_alike(input, TRUE));
    r_out = GWY_DATA_FIELD(gwy_data_field_new_alike(input, TRUE));
    i_out = GWY_DATA_FIELD(gwy_data_field_new_alike(input, TRUE));
    gwy_data_field_copy(input, r_in, TRUE);
    gwy_data_field_2dfft(r_in, i_in, r_out, i_out,
                         GWY_WINDOWING_NONE,
                         GWY_TRANSFORM_DIRECTION_FORWARD,
                         GWY_INTERPOLATION_BILINEAR,
                         FALSE, FALSE);
    /*//
    gwy_data_field_2dfft(r_in, i_in, r_out, i_out, gwy_data_line_fft_hum,
                         window, 1, interp, 0, 0);
    */
    gwy_data_field_2dfft_humanize(r_out);
    gwy_data_field_2dfft_humanize(i_out);
    if (output_fft != NULL)
       set_dfield_modulus(r_out, i_out, output_fft);

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
    /*//
    gwy_data_field_2dfft(r_out, i_out, r_in, i_in, gwy_data_line_fft_hum,
                         window, -1, interp, 0, 0);
    */
    if (output_image != NULL)
        gwy_data_field_copy(r_in, output_image, TRUE);

    /* Finalize */
    g_object_unref(i_out);
    g_object_unref(r_out);
    g_object_unref(i_in);
    g_object_unref(r_in);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
