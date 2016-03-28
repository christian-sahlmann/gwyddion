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
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/correct.h>
#include <libprocess/filters.h>
#include <libprocess/grains.h>
#include <libprocess/inttrans.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>
#include "preview.h"

#define LATMEAS_RUN_MODES (GWY_RUN_INTERACTIVE)

typedef enum {
    IMAGE_DATA,
    IMAGE_ACF,
    IMAGE_PSDF,
    IMAGE_NMODES
} ImageMode;

typedef enum {
    SELECTION_LATTICE,
    SELECTION_POINT,
    SELECTION_NMODES,
} SelectionMode;

/* Keep it simple and use a predefined set of zooms, these seem suitable. */
typedef enum {
    ZOOM_1 = 1,
    ZOOM_4 = 4,
    ZOOM_16 = 16
} ZoomType;

typedef struct {
    gdouble max;
    gdouble x;
    gdouble y;
    gdouble d;
    gdouble q;
    guint basecount;
} MaximumInfo;

typedef struct {
    ImageMode image_mode;
    SelectionMode selection_mode;
    ZoomType zoom_acf;
    ZoomType zoom_psdf;
    /* Cache */
    GType lattice_layer;
    GType point_layer;
    GType lattice_selection;
    GType point_selection;
} LatMeasArgs;

typedef struct {
    LatMeasArgs *args;
    GtkWidget *dialog;
    GtkWidget *view;
    GwyVectorLayer *vlayer;
    GwySelection *selection;
    gulong selection_id;
    GwyContainer *mydata;
    GtkWidget *zoom_label;
    GSList *zoom;
    GSList *image_mode;
    GSList *selection_mode;
    GwySIValueFormat *vf;
    GwySIValueFormat *vfphi;
    GtkWidget *a1_x;
    GtkWidget *a1_y;
    GtkWidget *a1_len;
    GtkWidget *a1_phi;
    GtkWidget *a2_x;
    GtkWidget *a2_y;
    GtkWidget *a2_len;
    GtkWidget *a2_phi;
    GtkWidget *phi;
    /* We always keep the direct-space selection here. */
    gdouble xy[4];
} LatMeasControls;

static gboolean   module_register       (void);
static void       measure_lattice       (GwyContainer *data,
                                         GwyRunType run);
static void       lat_meas_dialog       (LatMeasArgs *args,
                                         GwyContainer *data,
                                         GwyDataField *dfield,
                                         gint id);
static GtkWidget* make_lattice_table    (LatMeasControls *controls);
static GtkWidget* add_aux_button        (GtkWidget *hbox,
                                         const gchar *stock_id,
                                         const gchar *tooltip);
static void       lat_meas_copy         (LatMeasControls *controls);
static void       lat_meas_save         (LatMeasControls *controls);
static gchar*     format_report         (LatMeasControls *controls);
static void       do_estimate           (LatMeasControls *controls);
static void       init_selection        (LatMeasControls *controls);
static gboolean   smart_init_selection  (LatMeasControls *controls);
static void       set_selection         (LatMeasControls *controls,
                                         const gdouble *xy);
static gboolean   get_selection         (LatMeasControls *controls,
                                         gdouble *xy);
static void       image_mode_changed    (GtkToggleButton *button,
                                         LatMeasControls *controls);
static void       selection_mode_changed(GtkToggleButton *button,
                                         LatMeasControls *controls);
static void       zoom_changed          (GtkRadioButton *button,
                                         LatMeasControls *controls);
static void       calculate_acf_full    (LatMeasControls *controls,
                                         GwyDataField *dfield);
static void       calculate_psdf_full   (LatMeasControls *controls,
                                         GwyDataField *dfield);
static void       calculate_zoomed_field(LatMeasControls *controls);
static void       refine                (LatMeasControls *controls);
static void       selection_changed     (LatMeasControls *controls);
static void       find_maximum          (GwyDataField *dfield,
                                         gdouble *x,
                                         gdouble *y,
                                         gint xwinsize,
                                         gint ywinsize);
static gboolean   transform_selection   (gdouble *xy);
static void       invert_matrix         (gdouble *dest,
                                         const gdouble *src);
static gdouble    matrix_det            (const gdouble *m);
static void       load_args             (GwyContainer *container,
                                         LatMeasArgs *args);
static void       save_args             (GwyContainer *container,
                                         LatMeasArgs *args);
static void       sanitize_args         (LatMeasArgs *args);

static const LatMeasArgs lat_meas_defaults = {
    IMAGE_DATA, SELECTION_LATTICE, ZOOM_1, ZOOM_1,
    /* Cache */
    0, 0, 0, 0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Measures parameters of two-dimensional lattices."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("measure_lattice",
                              (GwyProcessFunc)&measure_lattice,
                              N_("/_Statistics/Measure _Lattice..."),
                              GWY_STOCK_MEASURE_LATTICE,
                              LATMEAS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Measure lattice"));

    return TRUE;
}

static void
measure_lattice(GwyContainer *data, GwyRunType run)
{
    LatMeasArgs args;
    GwyDataField *dfield;
    gint id;

    g_return_if_fail(run & LATMEAS_RUN_MODES);
    load_args(gwy_app_settings_get(), &args);
    args.lattice_layer = g_type_from_name("GwyLayerLattice");
    args.point_layer = g_type_from_name("GwyLayerPoint");
    args.lattice_selection = g_type_from_name("GwySelectionLattice");
    args.point_selection = g_type_from_name("GwySelectionPoint");
    g_assert(args.lattice_layer);
    g_assert(args.point_layer);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    lat_meas_dialog(&args, data, dfield, id);
    save_args(gwy_app_settings_get(), &args);
}

static void
lat_meas_dialog(LatMeasArgs *args,
                GwyContainer *data,
                GwyDataField *dfield,
                gint id)
{
    GtkWidget *hbox, *hbox2, *label, *lattable, *alignment, *button;
    GtkDialog *dialog;
    GtkTable *table;
    GSList *l;
    LatMeasControls controls;
    ImageMode image_mode;
    SelectionMode selection_mode;
    gint response, row;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer;
    GwySelection *selection;
    GwySIUnit *unitphi;
    gchar selkey[40];

    gwy_clear(&controls, 1);
    controls.args = args;
    controls.vlayer = NULL;

    /* Start with ACF and LATTICE selection because that is nice to initialise.
     * We switch at the end. */
    image_mode = args->image_mode;
    selection_mode = args->selection_mode;
    args->image_mode = IMAGE_ACF;
    args->selection_mode = SELECTION_LATTICE;

    controls.dialog = gtk_dialog_new_with_buttons(_("Measure Lattice"),
                                                  NULL, 0, NULL);
    dialog = GTK_DIALOG(controls.dialog);
    gtk_dialog_add_button(dialog, _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(dialog, gwy_sgettext("verb|_Estimate"),
                          RESPONSE_ESTIMATE);
    gtk_dialog_add_button(dialog, _("_Refine"), RESPONSE_REFINE);
    gtk_dialog_add_button(dialog, GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    calculate_acf_full(&controls, dfield);
    calculate_psdf_full(&controls, dfield);
    calculate_zoomed_field(&controls);
    gwy_app_sync_data_items(data, controls.mydata, id, 1, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    alignment = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), alignment, FALSE, FALSE, 4);

    controls.view = gwy_data_view_new(controls.mydata);
    g_object_unref(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "gradient-key", "/0/base/palette",
                 "data-key", "/1/data",
                 "range-type-key", "/1/base/range-type",
                 "min-max-key", "/1/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);
    /* We know we are starting with Lattice. */
    vlayer = g_object_new(args->lattice_layer,
                          "selection-key", "/0/select/lattice",
                          NULL);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view), vlayer);
    controls.selection = gwy_vector_layer_ensure_selection(vlayer);
    gwy_selection_set_max_objects(controls.selection, 1);
    controls.selection_id
        = g_signal_connect_swapped(controls.selection, "changed",
                                   G_CALLBACK(selection_changed), &controls);

    gtk_container_add(GTK_CONTAINER(alignment), controls.view);

    table = GTK_TABLE(gtk_table_new(10, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(_("Display:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.image_mode
        = gwy_radio_buttons_createl(G_CALLBACK(image_mode_changed), &controls,
                                    args->image_mode,
                                    _("_Data"), IMAGE_DATA,
                                    _("_ACF"), IMAGE_ACF,
                                    _("_PSDF"), IMAGE_PSDF,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.image_mode,
                                            table, 4, row);

    hbox2 = gtk_hbox_new(FALSE, 8);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = controls.zoom_label = gtk_label_new(_("Zoom:"));
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    controls.zoom
        = gwy_radio_buttons_createl(G_CALLBACK(zoom_changed), &controls,
                                    args->zoom_acf,
                                    "1×", ZOOM_1,
                                    "4×", ZOOM_4,
                                    "16×", ZOOM_16,
                                    NULL);
    for (l = controls.zoom; l; l = g_slist_next(l)) {
        GtkWidget *widget = GTK_WIDGET(l->data);
        gtk_box_pack_start(GTK_BOX(hbox2), widget, FALSE, FALSE, 0);
    }
    row++;

    gtk_table_set_row_spacing(table, row-1, 8);
    label = gtk_label_new(_("Show lattice as:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.selection_mode
        = gwy_radio_buttons_createl(G_CALLBACK(selection_mode_changed),
                                    &controls,
                                    args->selection_mode,
                                    _("_Lattice"), SELECTION_LATTICE,
                                    _("_Vectors"), SELECTION_POINT,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.selection_mode,
                                            table, 4, row);

    gtk_table_set_row_spacing(table, row-1, 8);
    label = gwy_label_new_header(_("Lattice Vectors"));
    gtk_table_attach(table, label, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.vf
        = gwy_data_field_get_value_format_xy(dfield,
                                             GWY_SI_UNIT_FORMAT_MARKUP, NULL);
    controls.vf->precision += 2;

    unitphi = gwy_si_unit_new("deg");
    controls.vfphi
        = gwy_si_unit_get_format_with_resolution(unitphi,
                                                 GWY_SI_UNIT_FORMAT_MARKUP,
                                                 180.0, 0.01, NULL);
    g_object_unref(unitphi);

    lattable = make_lattice_table(&controls);
    gtk_table_attach(table, lattable,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_table_attach(table, hbox, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);

    button = add_aux_button(hbox, GTK_STOCK_SAVE, _("Save table to a file"));
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(lat_meas_save), &controls);

    button = add_aux_button(hbox, GTK_STOCK_COPY, _("Copy table to clipboard"));
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(lat_meas_copy), &controls);

    /* Restore lattice from data if any is present. */
    g_snprintf(selkey, sizeof(selkey), "/%d/select/lattice", id);
    if (gwy_container_gis_object_by_name(data, selkey, &selection)) {
        if (gwy_selection_get_object(selection, 0, controls.xy))
            set_selection(&controls, controls.xy);
    }
    else {
        do_estimate(&controls);
    }

    gwy_radio_buttons_set_current(controls.selection_mode, selection_mode);
    gwy_radio_buttons_set_current(controls.image_mode, image_mode);

    gtk_widget_show_all(controls.dialog);
    do {
        response = gtk_dialog_run(dialog);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(controls.dialog);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            init_selection(&controls);
            break;

            case RESPONSE_ESTIMATE:
            do_estimate(&controls);
            break;

            case RESPONSE_REFINE:
            refine(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    if (get_selection(&controls, controls.xy)) {
        selection = g_object_new(g_type_from_name("GwySelectionLattice"),
                                 "max-objects", 1,
                                 NULL);
        gwy_selection_set_data(selection, 1, controls.xy);
        gwy_container_set_object_by_name(data, selkey, selection);
        g_object_unref(selection);
    }

    g_signal_handler_disconnect(controls.selection, controls.selection_id);
    if (controls.vlayer)
        gwy_object_unref(controls.vlayer);
    gtk_widget_destroy(controls.dialog);
}

static GtkWidget*
make_lattice_table(LatMeasControls *controls)
{
    GtkWidget *table, *label;
    GString *str = g_string_new(NULL);

    table = gtk_table_new(4, 5, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);

    /* header row */
    g_string_assign(str, "x");
    if (strlen(controls->vf->units))
        g_string_append_printf(str, " [%s]", controls->vf->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

    g_string_assign(str, "y");
    if (strlen(controls->vf->units))
        g_string_append_printf(str, " [%s]", controls->vf->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);

    g_string_assign(str, _("length"));
    if (strlen(controls->vf->units))
        g_string_append_printf(str, " [%s]", controls->vf->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1, GTK_FILL, 0, 0, 0);

    g_string_assign(str, _("angle"));
    if (strlen(controls->vfphi->units))
        g_string_append_printf(str, " [%s]", controls->vfphi->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 0, 1, GTK_FILL, 0, 0, 0);

    /* a1 */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "a<sub>1</sub>:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

    controls->a1_x = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_width_chars(GTK_LABEL(label), 8);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);

    controls->a1_y = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_width_chars(GTK_LABEL(label), 8);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2, GTK_FILL, 0, 0, 0);

    controls->a1_len = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_width_chars(GTK_LABEL(label), 8);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 1, 2, GTK_FILL, 0, 0, 0);

    controls->a1_phi = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_width_chars(GTK_LABEL(label), 8);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 1, 2, GTK_FILL, 0, 0, 0);

    /* a2 */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "a<sub>2</sub>:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

    controls->a2_x = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_width_chars(GTK_LABEL(label), 8);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);

    controls->a2_y = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_width_chars(GTK_LABEL(label), 8);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 2, 3, GTK_FILL, 0, 0, 0);

    controls->a2_len = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_width_chars(GTK_LABEL(label), 8);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 2, 3, GTK_FILL, 0, 0, 0);

    controls->a2_phi = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_width_chars(GTK_LABEL(label), 8);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 2, 3, GTK_FILL, 0, 0, 0);

    /* phi */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "ϕ:");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_label_set_width_chars(GTK_LABEL(label), 8);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 3, 4, GTK_FILL, 0, 0, 0);

    controls->phi = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 3, 4, GTK_FILL, 0, 0, 0);

    g_string_free(str, TRUE);

    return table;
}

static GtkWidget*
add_aux_button(GtkWidget *hbox,
               const gchar *stock_id,
               const gchar *tooltip)
{
    GtkTooltips *tips;
    GtkWidget *button;

    tips = gwy_app_get_tooltips();
    button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_tooltips_set_tip(tips, button, tooltip, NULL);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(stock_id,
                                               GTK_ICON_SIZE_SMALL_TOOLBAR));
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    return button;
}

static void
lat_meas_save(LatMeasControls *controls)
{
    gchar *text = format_report(controls);

    gwy_save_auxiliary_data(_("Save Lattice Parameters"),
                            GTK_WINDOW(controls->dialog),
                            -1, text);
    g_free(text);
}

static void
lat_meas_copy(LatMeasControls *controls)
{
    GtkClipboard *clipboard;
    GdkDisplay *display;
    gchar *text = format_report(controls);

    display = gtk_widget_get_display(controls->dialog);
    clipboard = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, text, -1);
    g_free(text);
}

static gchar*
format_report(LatMeasControls *controls)
{
    GPtrArray *report = g_ptr_array_new();
    GString *str = g_string_new(NULL);
    GwyDataField *dfield;
    GwySIValueFormat *vf, *vfphi;
    gdouble xy[4];
    gdouble h, phi;
    guint i, maxlen;

    get_selection(controls, xy);
    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    vf = gwy_data_field_get_value_format_xy(dfield, GWY_SI_UNIT_FORMAT_PLAIN,
                                            NULL);
    vf->precision += 2;
    vfphi = controls->vfphi;

    for (i = 0; i < 2; i++) {
        g_string_printf(str, _("Vector %d:"), i+1);
        g_ptr_array_add(report, g_strdup(str->str));
        g_string_printf(str, "(%.*f, %.*f) %s",
                        vf->precision, xy[2*i + 0]/vf->magnitude,
                        vf->precision, xy[2*i + 1]/vf->magnitude,
                        vf->units);
        g_ptr_array_add(report, g_strdup(str->str));

        h = hypot(xy[2*i + 0], xy[2*i + 1]);
        g_string_printf(str, _("Length %d:"), i+1);
        g_ptr_array_add(report, g_strdup(str->str));
        g_string_printf(str, "%.*f %s",
                        vf->precision, h/vf->magnitude, vf->units);
        g_ptr_array_add(report, g_strdup(str->str));

        phi = 180.0/G_PI*atan2(-xy[2*i + 1], xy[2*i + 0]);
        g_string_printf(str, _("Angle %d:"), i+1);
        g_ptr_array_add(report, g_strdup(str->str));
        g_string_printf(str, "%.*f %s",
                        vfphi->precision, phi/vfphi->magnitude, vfphi->units);
        g_ptr_array_add(report, g_strdup(str->str));

        g_ptr_array_add(report, NULL);
        g_ptr_array_add(report, NULL);
    }

    phi = atan2(-xy[3], xy[2]) - atan2(-xy[1], xy[0]);
    if (phi < 0.0)
        phi += 2.0*G_PI;
    phi *= 180.0/G_PI;

    g_string_assign(str, _("Angle:"));
    g_ptr_array_add(report, g_strdup(str->str));
    g_string_printf(str, "%.*f %s",
                    vfphi->precision, phi/vfphi->magnitude, vfphi->units);
    g_ptr_array_add(report, g_strdup(str->str));

    gwy_si_unit_value_format_free(vf);

    maxlen = 0;
    for (i = 0; i < report->len/2; i++) {
        gchar *key = (gchar*)g_ptr_array_index(report, 2*i);
        if (key)
            maxlen = MAX(strlen(key), maxlen);
    }

    g_string_truncate(str, 0);
    g_string_append(str, _("Lattice Parameters"));
    g_string_append(str, "\n\n");

    for (i = 0; i < report->len/2; i++) {
        gchar *key = (gchar*)g_ptr_array_index(report, 2*i);
        if (key) {
            gchar *value = (gchar*)g_ptr_array_index(report, 2*i + 1);
            g_string_append_printf(str, "%-*s %s\n", maxlen+1, key, value);
            g_free(value);
        }
        else
            g_string_append_c(str, '\n');
        g_free(key);
    }
    g_ptr_array_free(report, TRUE);

    return g_string_free(str, FALSE);
}

static void
do_estimate(LatMeasControls *controls)
{
    if (smart_init_selection(controls)) {
        GwyDataField *dfield;
        gdouble dh;

        refine(controls);
        /* Check if refine() did not produce two of the same vector which can
         * sometime happen with very skewed patterns. */
        dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
        dh = hypot(gwy_data_field_get_xmeasure(dfield),
                   gwy_data_field_get_ymeasure(dfield));
        if (hypot(controls->xy[0] - controls->xy[2],
                  controls->xy[1] - controls->xy[3]) > 1.8*dh)
            return;
    }
    init_selection(controls);
}

static void
init_selection(LatMeasControls *controls)
{
    GwyDataField *dfield;

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    controls->xy[0] = dfield->xreal/20;
    controls->xy[1] = 0.0;
    controls->xy[2] = 0.0;
    controls->xy[3] = -dfield->yreal/20;
    set_selection(controls, controls->xy);
}

static gint
compare_maxima(gconstpointer pa, gconstpointer pb)
{
    const MaximumInfo *a = (const MaximumInfo*)pa;
    const MaximumInfo *b = (const MaximumInfo*)pb;

    if (a->basecount*a->q > b->basecount*b->q)
        return -1;
    if (a->basecount*a->q < b->basecount*b->q)
        return 1;

    if (a->q > b->q)
        return -1;
    if (a->q < b->q)
        return 1;

    /* Ensure comparison stability.  This should play no role in significance
     * sorting. */
    if (a->y < b->y)
        return -1;
    if (a->y > b->y)
        return 1;
    if (a->x < b->x)
        return -1;
    if (a->x > b->x)
        return 1;
    return 0;
}

/* Intended for ACF (or PSDF, but that requires transformation), not the
 * original data. */
static gboolean
smart_init_selection(LatMeasControls *controls)
{
    enum { nquantities = 3 };
    GwyGrainQuantity quantities[nquantities] = {
        GWY_GRAIN_VALUE_MAXIMUM,
        GWY_GRAIN_VALUE_CENTER_X,
        GWY_GRAIN_VALUE_CENTER_Y,
    };
    GwyDataField *dfield, *smoothed, *mask;
    gdouble *values[nquantities];
    MaximumInfo *maxima;
    gint *grains;
    guint i, j, k, ngrains;
    gboolean ok = FALSE;
    gdouble dh, cphi, sphi;

    dfield = gwy_container_get_object_by_name(controls->mydata, "/2/data/full");
    smoothed = gwy_data_field_duplicate(dfield);
    mask = gwy_data_field_new_alike(dfield, FALSE);

    /* Mark local maxima. */
    gwy_data_field_filter_gaussian(smoothed, 0.5);
    gwy_data_field_mark_extrema(smoothed, mask, TRUE);
    grains = g_new0(gint, dfield->xres*dfield->yres);
    ngrains = gwy_data_field_number_grains(mask, grains);
    gwy_object_unref(mask);

    /* Find the position and value of each. */
    for (i = 0; i < nquantities; i++)
        values[i] = g_new(gdouble, ngrains+1);

    gwy_data_field_grains_get_quantities(smoothed, values,
                                         quantities, nquantities,
                                         ngrains, grains);
    gwy_object_unref(smoothed);

    maxima = g_new(MaximumInfo, ngrains);
    dh = hypot(gwy_data_field_get_xmeasure(dfield),
               gwy_data_field_get_ymeasure(dfield));
    for (i = 0; i < ngrains; i++) {
        maxima[i].max = values[0][i+1];
        maxima[i].x = values[1][i+1];
        maxima[i].y = values[2][i+1];
        maxima[i].d = hypot(maxima[i].x, maxima[i].y);
        maxima[i].q = maxima[i].max/(maxima[i].d + 5.0*dh);
        maxima[i].basecount = 0;
    }
    for (i = 0; i < nquantities; i++)
        g_free(values[i]);

    /* Remove the central peak, i.e. anything too close to the centre */
    i = j = 0;
    while (i < ngrains) {
        gdouble d = maxima[i].d;
        maxima[j] = maxima[i];
        if (d >= 1.8*dh)
            j++;
        i++;
    }
    ngrains = j;

    if (ngrains < 10) {
        gwy_debug("Too few maxima (after centre removal): %d.", ngrains);
        g_free(maxima);
        return FALSE;
    }

    qsort(maxima, ngrains, sizeof(MaximumInfo), compare_maxima);
#ifdef DEBUG
    for (i = 0; i < ngrains; i++) {
        gwy_debug("[%u] (%g, %g) %g :: %g",
                  i, maxima[i].x, maxima[i].y, maxima[i].max, maxima[i].q);
    }
#endif

    /* Remove anything with direction opposite to the first vector.  But we
     * must carefully accept ortohogonal vectors.  This is just a half-plane
     * selection though it influences the preferred vectors, of course. */
    gwy_debug("Base-plane selector [%u] (%g, %g) %g",
              0, maxima[0].x, maxima[0].y, maxima[0].max);
    cphi = maxima[0].x/maxima[0].d;
    sphi = maxima[0].y/maxima[0].d;
    i = j = 1;
    while (i < ngrains) {
        gdouble x = cphi*maxima[i].x + sphi*maxima[i].y,
                y = cphi*maxima[i].y - sphi*maxima[i].x;
        maxima[j] = maxima[i];
        if (x > 1e-9*dh || (x > -1e-9*dh && y > 1e-9*dh))
            j++;
        i++;
    }
    ngrains = j;

    if (ngrains < 10) {
        gwy_debug("Too few maxima (after half-plane removal): %d.", ngrains);
        g_free(maxima);
        return FALSE;
    }

    /* Locate the most important maxima. */
    ngrains = MIN(ngrains, 12);
    for (i = 0; i < ngrains; i++) {
        for (j = i+1; j < ngrains; j++) {
            gdouble x = maxima[i].x + maxima[j].x;
            gdouble y = maxima[i].y + maxima[j].y;
            for (k = 0; k < ngrains; k++) {
                if (fabs(maxima[k].x - x) < dh && fabs(maxima[k].y - y) < dh) {
                    maxima[i].basecount++;
                    maxima[j].basecount++;
                }
            }
        }
    }
    qsort(maxima, ngrains, sizeof(MaximumInfo), compare_maxima);
#ifdef DEBUG
    for (i = 0; i < ngrains; i++) {
        gwy_debug("[%u] (%g, %g) %g #%u",
                  i, maxima[i].x, maxima[i].y, maxima[i].max,
                  maxima[i].basecount);
    }
#endif

    if (maxima[1].basecount >= 3) {
        gdouble xy[4];

        xy[0] = maxima[0].x;
        xy[1] = maxima[0].y;
        dh = maxima[0].d;
        /* Exclude maxima that appear to be collinear with the first one,
         * otherwise take the next one with the highest basecount. */
        for (i = 1; i < ngrains; i++) {
            for (k = 2; k < 5; k++) {
                if (fabs(maxima[i].x/k - xy[0]) < 0.2*dh
                    && fabs(maxima[i].y/k - xy[1]) < 0.2*dh) {
                    gwy_debug("Excluding #%u for collinearity (%u).", i, k);
                    break;
                }
            }
            if (k == 5) {
                xy[2] = maxima[i].x;
                xy[3] = maxima[i].y;
                ok = TRUE;
                break;
            }
        }

        if (ok) {
            gdouble phi;

            /* Try to choose some sensible vectors among the equivalent
             * choices. */
            for (i = 0; i < 4; i++)
                xy[i] = -xy[i];

            phi = fmod(atan2(xy[1], xy[0]) + 4.0*G_PI - atan2(xy[3], xy[2]),
                       2.0*G_PI);
            if (phi > G_PI) {
                GWY_SWAP(gdouble, xy[0], xy[2]);
                GWY_SWAP(gdouble, xy[1], xy[3]);
            }
            set_selection(controls, xy);
        }
    }

    g_free(maxima);

    return ok;
}

static void
set_selection(LatMeasControls *controls, const gdouble *xy)
{
    LatMeasArgs *args = controls->args;
    GwySelection *selection = controls->selection;
    GType stype = G_OBJECT_TYPE(selection);
    GwyDataField *dfield;
    gdouble xoff, yoff;
    gdouble tmpxy[4];

    memcpy(tmpxy, xy, sizeof(tmpxy));
    if (args->image_mode == IMAGE_PSDF)
        transform_selection(tmpxy);

    if (g_type_is_a(stype, args->lattice_selection)) {
        gwy_selection_set_data(selection, 1, tmpxy);
        return;
    }

    /* This is much more convoluted because point selections have origin of
     * real coordinates in the top left corner. */
    g_return_if_fail(g_type_is_a(stype, args->point_selection));
    if (args->image_mode == IMAGE_DATA)
        dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    else
        dfield = gwy_container_get_object_by_name(controls->mydata, "/1/data");

    xoff = 0.5*dfield->xreal;
    yoff = 0.5*dfield->yreal;
    tmpxy[0] += xoff;
    tmpxy[1] += yoff;
    tmpxy[2] += xoff;
    tmpxy[3] += yoff;
    gwy_selection_set_data(selection, 2, tmpxy);
}

static gboolean
get_selection(LatMeasControls *controls, gdouble *xy)
{
    GwySelection *selection = controls->selection;
    LatMeasArgs *args = controls->args;
    GType stype = G_OBJECT_TYPE(selection);
    GwyDataField *dfield;
    gdouble xoff, yoff;

    if (!gwy_selection_is_full(selection))
        return FALSE;

    gwy_selection_get_data(selection, xy);

    if (g_type_is_a(stype, args->lattice_selection)) {
        if (args->image_mode == IMAGE_PSDF)
            transform_selection(xy);
        return TRUE;
    }

    /* This is much more convoluted because point selections have origin of
     * real coordinates in the top left corner. */
    g_return_val_if_fail(g_type_is_a(stype, args->point_selection), FALSE);
    if (args->image_mode == IMAGE_DATA)
        dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    else
        dfield = gwy_container_get_object_by_name(controls->mydata, "/1/data");

    xoff = 0.5*dfield->xreal;
    yoff = 0.5*dfield->yreal;
    xy[0] -= xoff;
    xy[1] -= yoff;
    xy[2] -= xoff;
    xy[3] -= yoff;
    if (args->image_mode == IMAGE_PSDF)
        transform_selection(xy);

    return TRUE;
}

static void
image_mode_changed(G_GNUC_UNUSED GtkToggleButton *button,
                   LatMeasControls *controls)
{
    LatMeasArgs *args = controls->args;
    GwyDataView *dataview;
    GwyPixmapLayer *layer;
    ImageMode mode;
    gboolean zoom_sens;
    gdouble xy[4];   /* To survive the nested zoom and whatever changes. */
    GSList *l;

    mode = gwy_radio_buttons_get_current(controls->image_mode);
    gwy_debug("current: %u, new: %u", args->image_mode, mode);
    if (mode == args->image_mode)
        return;

    get_selection(controls, xy);
    args->image_mode = mode;
    dataview = GWY_DATA_VIEW(controls->view);
    layer = gwy_data_view_get_base_layer(dataview);

    if (mode == IMAGE_ACF)
        gwy_radio_buttons_set_current(controls->zoom, args->zoom_acf);
    else if (mode == IMAGE_PSDF)
        gwy_radio_buttons_set_current(controls->zoom, args->zoom_psdf);

    calculate_zoomed_field(controls);

    if (args->image_mode == IMAGE_DATA) {
        g_object_set(layer,
                     "data-key", "/0/data",
                     "range-type-key", "/0/base/range-type",
                     "min-max-key", "/0/base",
                     NULL);
        zoom_sens = FALSE;
        if (args->selection_mode == SELECTION_POINT) {
            controls->vlayer = gwy_data_view_get_top_layer(dataview);
            g_object_ref(controls->vlayer);
            gwy_data_view_set_top_layer(dataview, NULL);
        }
    }
    else {
        /* There are no range-type and min-max keys, which is the point,
         * because we want full-colour-scale, whatever the image has set. */
        g_object_set(layer,
                     "data-key", "/1/data",
                     "range-type-key", "/1/base/range-type",
                     "min-max-key", "/1/base",
                     NULL);
        if (mode == IMAGE_ACF)
            gwy_container_set_enum_by_name(controls->mydata,
                                           "/1/base/range-type",
                                           GWY_LAYER_BASIC_RANGE_FULL);
        else
            gwy_container_set_enum_by_name(controls->mydata,
                                           "/1/base/range-type",
                                           GWY_LAYER_BASIC_RANGE_ADAPT);
        zoom_sens = TRUE;
        if (controls->vlayer) {
            gwy_data_view_set_top_layer(dataview, controls->vlayer);
            gwy_object_unref(controls->vlayer);
        }
    }

    gwy_set_data_preview_size(dataview, PREVIEW_SIZE);

    gtk_widget_set_sensitive(controls->zoom_label, zoom_sens);
    for (l = controls->zoom; l; l = g_slist_next(l))
        gtk_widget_set_sensitive(GTK_WIDGET(l->data), zoom_sens);

    set_selection(controls, xy);
}

static void
selection_mode_changed(G_GNUC_UNUSED GtkToggleButton *button,
                       LatMeasControls *controls)
{
    LatMeasArgs *args = controls->args;
    GwyDataView *dataview;
    GwyVectorLayer *vlayer;
    SelectionMode mode;
    guint maxobjects;

    mode = gwy_radio_buttons_get_current(controls->selection_mode);
    gwy_debug("current: %u, new: %u", args->selection_mode, mode);
    if (mode == args->selection_mode)
        return;

    get_selection(controls, controls->xy);
    g_signal_handler_disconnect(controls->selection, controls->selection_id);
    controls->selection_id = 0;
    args->selection_mode = mode;

    dataview = GWY_DATA_VIEW(controls->view);
    if (controls->vlayer) {
        gwy_data_view_set_top_layer(dataview, controls->vlayer);
        gwy_object_unref(controls->vlayer);
    }

    if (mode == SELECTION_LATTICE) {
        vlayer = g_object_new(args->lattice_layer,
                              "selection-key", "/0/select/lattice",
                              NULL);
        maxobjects = 1;
    }
    else {
        vlayer = g_object_new(args->point_layer,
                              "selection-key", "/0/select/point",
                              "draw-as-vector", TRUE,
                              NULL);
        maxobjects = 2;
    }

    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), vlayer);
    controls->selection = gwy_vector_layer_ensure_selection(vlayer);
    gwy_selection_set_max_objects(controls->selection, maxobjects);
    set_selection(controls, controls->xy);
    if (mode == SELECTION_POINT && args->image_mode == IMAGE_DATA) {
        controls->vlayer = g_object_ref(vlayer);
        gwy_data_view_set_top_layer(dataview, NULL);
    }
    controls->selection_id
        = g_signal_connect_swapped(controls->selection, "changed",
                                   G_CALLBACK(selection_changed), controls);
}

static void
zoom_changed(GtkRadioButton *button,
             LatMeasControls *controls)
{
    LatMeasArgs *args = controls->args;
    ZoomType zoom = gwy_radio_buttons_get_current(controls->zoom);

    get_selection(controls, controls->xy);
    if (args->image_mode == IMAGE_ACF) {
        if (button && zoom == args->zoom_acf)
            return;
        args->zoom_acf = zoom;
    }
    else if (args->image_mode == IMAGE_PSDF) {
        if (button && zoom == args->zoom_psdf)
            return;
        args->zoom_psdf = zoom;
    }
    else {
        g_assert(args->image_mode == IMAGE_DATA);
        return;
    }

    calculate_zoomed_field(controls);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls->view), PREVIEW_SIZE);
    set_selection(controls, controls->xy);
}

static void
calculate_acf_full(LatMeasControls *controls,
                   GwyDataField *dfield)
{
    GwyDataField *acf;
    guint acfwidth, acfheight;

    dfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));
    acf = gwy_data_field_new_alike(dfield, FALSE);
    acfwidth = MIN(MAX(3*dfield->xres/8, 64), dfield->xres/2);
    acfheight = MIN(MAX(3*dfield->yres/8, 64), dfield->yres/2);
    gwy_data_field_area_2dacf(dfield, acf, 0, 0, dfield->xres, dfield->yres,
                              acfwidth, acfheight);
    g_object_unref(dfield);
    gwy_container_set_object_by_name(controls->mydata, "/2/data/full", acf);
    g_object_unref(acf);
}

static gint
reduce_size(gint n)
{
    gint n0 = (n & 1) ? n : n-1;
    gint nmin = MAX(n0, 65);
    gint nred = 3*n/4;
    gint nredodd = (nred & 1) ? nred : nred-1;
    return MIN(nmin, nredodd);
}

static void
calculate_psdf_full(LatMeasControls *controls,
                    GwyDataField *dfield)
{
    GwyDataField *reout, *imout, *psdf;
    gint i, j, psdfwidth, psdfheight;

    reout = gwy_data_field_new_alike(dfield, FALSE);
    imout = gwy_data_field_new_alike(dfield, FALSE);
    gwy_data_field_2dfft(dfield, NULL, reout, imout,
                         GWY_WINDOWING_HANN, GWY_TRANSFORM_DIRECTION_FORWARD,
                         GWY_INTERPOLATION_LINEAR,  /* ignored */
                         FALSE, 1);
    gwy_data_field_fft_postprocess(reout, TRUE);
    gwy_data_field_fft_postprocess(imout, TRUE);
    gwy_data_field_hypot_of_fields(reout, reout, imout);
    /* XXX: We do not fix the value units because they should not leak from the
     * in-module preview. */
    g_object_unref(imout);

    psdfwidth = reduce_size(dfield->xres);
    psdfheight = reduce_size(dfield->yres);
    i = dfield->yres - psdfheight - (dfield->yres - psdfheight)/2;
    j = dfield->xres - psdfwidth - (dfield->xres - psdfwidth)/2;
    psdf = gwy_data_field_area_extract(reout, j, i, psdfwidth, psdfheight);
    g_object_unref(reout);
    gwy_data_field_set_xoffset(psdf, -0.5*psdf->xreal);
    gwy_data_field_set_yoffset(psdf, -0.5*psdf->yreal);
    gwy_container_set_object_by_name(controls->mydata, "/3/data/full", psdf);
    g_object_unref(psdf);
}

static void
calculate_zoomed_field(LatMeasControls *controls)
{
    LatMeasArgs *args = controls->args;
    ZoomType zoom;
    GwyDataField *zoomed;
    const gchar *key = NULL;

    if (args->image_mode == IMAGE_ACF) {
        zoom = args->zoom_acf;
        key = "/2/data/full";
    }
    else if (args->image_mode == IMAGE_PSDF) {
        zoom = args->zoom_psdf;
        key = "/3/data/full";
    }
    else
        return;

    zoomed = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             key));
    if (zoom == ZOOM_1) {
        g_object_ref(zoomed);
    }
    else {
        guint xres = zoomed->xres;
        guint yres = zoomed->yres;
        guint width = (xres/zoom) | 1;
        guint height = (yres/zoom) | 1;

        if (width < 17)
            width = MAX(width, MIN(17, xres));

        if (height < 17)
            height = MAX(height, MIN(17, yres));

        zoomed = gwy_data_field_area_extract(zoomed,
                                             (xres - width)/2,
                                             (yres - height)/2,
                                             width, height);
        gwy_data_field_set_xoffset(zoomed, -0.5*zoomed->xreal);
        gwy_data_field_set_yoffset(zoomed, -0.5*zoomed->yreal);
    }
    gwy_container_set_object_by_name(controls->mydata, "/1/data", zoomed);
    g_object_unref(zoomed);
}

static void
refine(LatMeasControls *controls)
{
    GwyDataField *dfield;
    gint xwinsize, ywinsize;
    const gchar *key = NULL;
    gdouble xy[4];

    if (!get_selection(controls, xy))
        return;

    if (controls->args->image_mode == IMAGE_PSDF) {
        /* For refine we need selection coordinates for the visible image
         * which is PSDF, not the real-space lattice. */
        transform_selection(xy);
        key = "/3/data/full";
    }
    else
        key = "/2/data/full";

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             key));
    xwinsize = (gint)(0.32*MAX(fabs(xy[0]), fabs(xy[2]))
                      /gwy_data_field_get_xmeasure(dfield) + 0.5);
    ywinsize = (gint)(0.32*MAX(fabs(xy[1]), fabs(xy[3]))
                      /gwy_data_field_get_ymeasure(dfield) + 0.5);
    gwy_debug("window size: %dx%d", xwinsize, ywinsize);

    xy[0] = (xy[0] - dfield->xoff)/gwy_data_field_get_xmeasure(dfield);
    xy[1] = (xy[1] - dfield->yoff)/gwy_data_field_get_ymeasure(dfield);
    xy[2] = (xy[2] - dfield->xoff)/gwy_data_field_get_xmeasure(dfield);
    xy[3] = (xy[3] - dfield->yoff)/gwy_data_field_get_ymeasure(dfield);
    find_maximum(dfield, xy + 0, xy + 1, xwinsize, ywinsize);
    find_maximum(dfield, xy + 2, xy + 3, xwinsize, ywinsize);
    xy[0] = (xy[0] + 0.5)*gwy_data_field_get_xmeasure(dfield) + dfield->xoff;
    xy[1] = (xy[1] + 0.5)*gwy_data_field_get_ymeasure(dfield) + dfield->yoff;
    xy[2] = (xy[2] + 0.5)*gwy_data_field_get_xmeasure(dfield) + dfield->xoff;
    xy[3] = (xy[3] + 0.5)*gwy_data_field_get_ymeasure(dfield) + dfield->yoff;

    if (controls->args->image_mode == IMAGE_PSDF)
        transform_selection(xy);

    set_selection(controls, xy);
}

static void
selection_changed(LatMeasControls *controls)
{
    GwySIValueFormat *vf;
    gdouble a1, a2, phi1, phi2, phi;
    gdouble xy[4];
    GString *str = g_string_new(NULL);

    if (!get_selection(controls, xy))
        return;

    vf = controls->vf;
    g_string_printf(str, "%.*f", vf->precision, xy[0]/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a1_x), str->str);

    g_string_printf(str, "%.*f", vf->precision, -xy[1]/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a1_y), str->str);

    a1 = hypot(xy[0], xy[1]);
    g_string_printf(str, "%.*f", vf->precision, a1/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a1_len), str->str);

    vf = controls->vfphi;
    phi1 = atan2(-xy[1], xy[0]);
    g_string_printf(str, "%.*f", vf->precision, 180.0/G_PI*phi1/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a1_phi), str->str);

    vf = controls->vf;
    g_string_printf(str, "%.*f", vf->precision, xy[2]/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a2_x), str->str);

    g_string_printf(str, "%.*f", vf->precision, -xy[3]/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a2_y), str->str);

    a2 = hypot(xy[2], xy[3]);
    g_string_printf(str, "%.*f", vf->precision, a2/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a2_len), str->str);

    vf = controls->vfphi;
    phi2 = atan2(-xy[3], xy[2]);
    g_string_printf(str, "%.*f", vf->precision, 180.0/G_PI*phi2/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a2_phi), str->str);

    phi = phi2 - phi1;
    if (phi < 0.0)
        phi += 2.0*G_PI;
    g_string_printf(str, "%.*f", vf->precision, 180.0/G_PI*phi/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->phi), str->str);

    g_string_free(str, TRUE);
}

static void
find_maximum(GwyDataField *dfield,
             gdouble *x, gdouble *y,
             gint xwinsize, gint ywinsize)
{
    gint xj = (gint)*x, yi = (gint)*y;
    gdouble v, max = -G_MAXDOUBLE;
    gint mi = yi, mj = xj, i, j;
    gint xres = dfield->xres, yres = dfield->yres;
    const gdouble *d = dfield->data;
    gdouble z[9];

    gwy_debug("searching from: %g, %g", *x, *y);
    for (i = -ywinsize; i <= ywinsize; i++) {
        if (i + yi < 0 || i + yi > yres-1)
            continue;
        for (j = -xwinsize; j <= xwinsize; j++) {
            if (j + xj < 0 || j + xj > xres-1)
                continue;

            v = d[(i + yi)*xres + (j + xj)];
            if (v > max) {
                max = v;
                mi = i + yi;
                mj = j + xj;
            }
        }
    }
    gwy_debug("pixel maximum at: %d, %d", *mj, *mi);

    /* Don't try any sub-pixel refinement if it's on the edge. */
    if (mi >= 1 && mi+1 <= yres-1 && mj >= 1 && mj+1 <= xres-1) {
        for (i = -1; i <= 1; i++) {
            for (j = -1; j <= 1; j++)
                z[3*(i + 1) + (j + 1)] = d[(mi + i)*xres + (mj + j)];
        }
        gwy_math_refine_maximum(z, x, y);
        gwy_debug("refinement by (%g, %g)", *x, *y);
    }
    else {
        *x = *y = 0.0;
    }

    *x += mj;
    *y += mi;
}

static gboolean
transform_selection(gdouble *xy)
{
    gdouble D = matrix_det(xy);
    gdouble a = fabs(xy[0]*xy[3]) + fabs(xy[1]*xy[2]);

    if (fabs(D)/a < 1e-9)
        return FALSE;

    invert_matrix(xy, xy);
    /* Transpose. */
    GWY_SWAP(gdouble, xy[1], xy[2]);
    return TRUE;
}

/* Permit dest = src */
static void
invert_matrix(gdouble *dest,
              const gdouble *src)
{
    gdouble D = matrix_det(src);
    gdouble xy[4];

    gwy_debug("D %g", D);
    xy[0] = src[3]/D;
    xy[1] = -src[1]/D;
    xy[2] = -src[2]/D;
    xy[3] = src[0]/D;
    dest[0] = xy[0];
    dest[1] = xy[1];
    dest[2] = xy[2];
    dest[3] = xy[3];
}

static gdouble
matrix_det(const gdouble *m)
{
    return m[0]*m[3] - m[1]*m[2];
}

static const gchar image_mode_key[]     = "/module/measure_lattice/image_mode";
static const gchar selection_mode_key[] = "/module/measure_lattice/selection_mode";
static const gchar zoom_acf_key[]       = "/module/measure_lattice/zoom_acf";
static const gchar zoom_psdf_key[]      = "/module/measure_lattice/zoom_psdf";

static void
sanitize_args(LatMeasArgs *args)
{
    args->selection_mode = MIN(args->selection_mode, SELECTION_NMODES-1);
    args->image_mode = MIN(args->image_mode, IMAGE_NMODES-1);
    if (args->zoom_acf != ZOOM_1
        && args->zoom_acf != ZOOM_4
        && args->zoom_acf != ZOOM_16)
        args->zoom_acf = lat_meas_defaults.zoom_acf;
    if (args->zoom_psdf != ZOOM_1
        && args->zoom_psdf != ZOOM_4
        && args->zoom_psdf != ZOOM_16)
        args->zoom_psdf = lat_meas_defaults.zoom_psdf;
}

static void
load_args(GwyContainer *container, LatMeasArgs *args)
{
    *args = lat_meas_defaults;

    gwy_container_gis_enum_by_name(container, image_mode_key,
                                   &args->image_mode);
    gwy_container_gis_enum_by_name(container, selection_mode_key,
                                   &args->selection_mode);
    gwy_container_gis_enum_by_name(container, zoom_acf_key, &args->zoom_acf);
    gwy_container_gis_enum_by_name(container, zoom_psdf_key, &args->zoom_psdf);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container, LatMeasArgs *args)
{
    gwy_container_set_enum_by_name(container, image_mode_key,
                                   args->image_mode);
    gwy_container_set_enum_by_name(container, selection_mode_key,
                                   args->selection_mode);
    gwy_container_set_enum_by_name(container, zoom_acf_key, args->zoom_acf);
    gwy_container_set_enum_by_name(container, zoom_psdf_key, args->zoom_psdf);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
