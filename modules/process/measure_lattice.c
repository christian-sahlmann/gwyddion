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

#define LATMEAS_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400,
};

enum {
    RESPONSE_RESET = 1,
};

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
    ImageMode image_mode;
    SelectionMode selection_mode;
    ZoomType zoom_acf;
    ZoomType zoom_psdf;
} LatMeasArgs;

typedef struct {
    LatMeasArgs *args;
    GtkWidget *dialog;
    GtkWidget *view;
    GwyVectorLayer *vlayer;
    GwySelection *selection;
    GwyContainer *mydata;
    GtkWidget *zoom_label;
    GSList *zoom;
    GSList *image_mode;
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
    GtkWidget *refine;
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
static void       init_selection        (GwySelection *selection,
                                         GwyDataField *dfield,
                                         ImageMode mode);
static void       image_mode_changed    (GtkToggleButton *button,
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
                              NULL,
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
    g_return_if_fail(g_type_from_name("GwyLayerPoint"));
    g_return_if_fail(g_type_from_name("GwyLayerLattice"));
    load_args(gwy_app_settings_get(), &args);
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
    GtkWidget *hbox, *hbox2, *label, *lattable, *alignment;
    GtkDialog *dialog;
    GtkTable *table;
    GSList *l;
    LatMeasControls controls;
    gint response, row;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer;
    GwySIUnit *unitphi;

    gwy_clear(&controls, 1);
    controls.args = args;

    controls.dialog = gtk_dialog_new_with_buttons(_("Measure Lattice"),
                                                  NULL, 0, NULL);
    dialog = GTK_DIALOG(controls.dialog);
    gtk_dialog_add_button(dialog, _("_Reset"), RESPONSE_RESET);
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
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    vlayer = g_object_new(g_type_from_name("GwyLayerLattice"),
                          "selection-key", "/0/select/lattice",
                          NULL);
    controls.vlayer = g_object_ref(vlayer);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view), vlayer);
    controls.selection = gwy_vector_layer_ensure_selection(vlayer);
    g_object_ref(controls.selection);
    gwy_selection_set_max_objects(controls.selection, 1);
    g_signal_connect_swapped(controls.selection, "changed",
                             G_CALLBACK(selection_changed), &controls);

    gtk_container_add(GTK_CONTAINER(alignment), controls.view);

    table = GTK_TABLE(gtk_table_new(6, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(_("Display:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 5, row, row+1, GTK_FILL, 0, 0, 0);
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
    gtk_widget_set_sensitive(label, FALSE);
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
        gtk_widget_set_sensitive(widget, FALSE);
    }
    row++;

    gtk_table_set_row_spacing(table, row-1, 8);
    label = gwy_label_new_header(_("Lattice Vectors"));
    gtk_table_attach(table, label, 0, 5, row, row+1, GTK_FILL, 0, 0, 0);
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
                     0, 5, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.refine, "clicked",
                             G_CALLBACK(refine), &controls);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    label = gwy_label_new_header(_("Options"));
    gtk_table_attach(table, label, 0, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

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
            /* TODO: Replace with intelligent re-init. */
            init_selection(controls.selection, dfield, args->image_mode);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(controls.dialog);
}

static GtkWidget*
make_lattice_table(LatMeasControls *controls)
{
    GtkWidget *table, *label, *button;
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

    controls->refine = button = gtk_button_new_with_mnemonic(_("Re_fine"));
    gtk_table_attach(GTK_TABLE(table), button,
                     0, 3, 3, 4, GTK_FILL, 0, 0, 0);

    g_string_free(str, TRUE);

    return table;
}

static void
init_selection(GwySelection *selection,
               GwyDataField *dfield,
               ImageMode mode)
{
    gdouble xy[4] = { 0.0, 0.0, 0.0, 0.0 };

    xy[0] = dfield->xreal/20;
    xy[3] = -dfield->yreal/20;
    if (mode == IMAGE_PSDF)
        transform_selection(xy);

    gwy_selection_set_data(selection, 1, xy);
}

static void
image_mode_changed(G_GNUC_UNUSED GtkToggleButton *button,
                   LatMeasControls *controls)
{
    LatMeasArgs *args = controls->args;
    GwyDataView *dataview;
    GwyPixmapLayer *layer;
    ImageMode mode;
    gboolean transform_sel, zoom_sens;
    GSList *l;

    mode = gwy_radio_buttons_get_current(controls->image_mode);
    if (mode == args->image_mode)
        return;

    transform_sel = (mode == IMAGE_PSDF) || (args->image_mode == IMAGE_PSDF);
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
    }
    else {
        /* There are no range-type and min-max keys, which is the point,
         * because we want full-colour-scale, whatever the image has set. */
        g_object_set(layer,
                     "data-key", "/1/data",
                     "range-type-key", "/1/base/range-type",
                     "min-max-key", "/1/base",
                     NULL);
        zoom_sens = TRUE;
    }

    if (transform_sel) {
        if (gwy_selection_is_full(controls->selection)) {
            guint n;

            gwy_selection_get_data(controls->selection, controls->xy);
            if (!transform_selection(controls->xy)) {
                // XXX: Init selection, transform to frequencies if necessary.
                g_warning("Selection transformation failed.  FIXME.");
            }
            n = 4/gwy_selection_get_object_size(controls->selection);
            gwy_selection_set_data(controls->selection, n, controls->xy);
        }
        else {
            // XXX: Init selection, transform to frequencies if necessary.
        }
    }

    gwy_set_data_preview_size(dataview, PREVIEW_SIZE);

    gtk_widget_set_sensitive(controls->zoom_label, zoom_sens);
    for (l = controls->zoom; l; l = g_slist_next(l))
        gtk_widget_set_sensitive(GTK_WIDGET(l->data), zoom_sens);
}

static void
zoom_changed(GtkRadioButton *button,
             LatMeasControls *controls)
{
    LatMeasArgs *args = controls->args;
    ZoomType zoom = gwy_radio_buttons_get_current(controls->zoom);

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
    acfwidth = MIN(MAX(dfield->xres/4, 64), dfield->xres/2);
    acfheight = MIN(MAX(dfield->yres/4, 64), dfield->yres/2);
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
    gdouble xy[4];

    if (!gwy_selection_is_full(controls->selection))
        return;

    gwy_selection_get_data(controls->selection, xy);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/1/data"));
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
    gwy_selection_set_object(controls->selection, 0, xy);
}

static void
selection_changed(LatMeasControls *controls)
{
    GwySIValueFormat *vf;
    gdouble a1, a2, phi1, phi2, phi;
    gdouble xy[4];
    GString *str = g_string_new(NULL);

    if (!gwy_selection_is_full(controls->selection))
        return;

    gwy_selection_get_data(controls->selection, controls->xy);
    memcpy(xy, controls->xy, 4*sizeof(gdouble));
    if (controls->args->image_mode == IMAGE_PSDF)
        transform_selection(xy);

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
    gdouble max = -G_MAXDOUBLE;
    gint mi = yi, mj = xj, i, j;
    gint xres = dfield->xres, yres = dfield->yres;
    const gdouble *d = dfield->data;
    gdouble sz, szx, szy, szxx, szxy, szyy;
    gdouble v, bx, by, cxx, cxy, cyy, D, sx, sy;
    gdouble m[6], rhs[3];

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

    *x = mj;
    *y = mi;
    gwy_debug("pixel maximum at: %g, %g", *x, *y);

    /* Don't try any sub-pixel refinement if it's on the edge. */
    if (mi < 1 || mi+1 > yres-1 || mj < 1 || mj+1 > xres-1)
        return;

    sz = (d[(mi - 1)*xres + (mj - 1)]
          + d[(mi - 1)*xres + mj]
          + d[(mi - 1)*xres + (mj + 1)]
          + d[mi*xres + (mj - 1)]
          + d[mi*xres + mj]
          + d[mi*xres + (mj + 1)]
          + d[(mi + 1)*xres + (mj - 1)]
          + d[(mi + 1)*xres + mj]
          + d[(mi + 1)*xres + (mj + 1)]);
    szx = (-d[(mi - 1)*xres + (mj - 1)]
           + d[(mi - 1)*xres + (mj + 1)]
           - d[mi*xres + (mj - 1)]
           + d[mi*xres + (mj + 1)]
           - d[(mi + 1)*xres + (mj - 1)]
           + d[(mi + 1)*xres + (mj + 1)]);
    szy = (-d[(mi - 1)*xres + (mj - 1)]
           - d[(mi - 1)*xres + mj]
           - d[(mi - 1)*xres + (mj + 1)]
           + d[(mi + 1)*xres + (mj - 1)]
           + d[(mi + 1)*xres + mj]
           + d[(mi + 1)*xres + (mj + 1)]);
    szxx = (d[(mi - 1)*xres + (mj - 1)]
            + d[(mi - 1)*xres + (mj + 1)]
            + d[mi*xres + (mj - 1)]
            + d[mi*xres + (mj + 1)]
            + d[(mi + 1)*xres + (mj - 1)]
            + d[(mi + 1)*xres + (mj + 1)]);
    szxy = (d[(mi - 1)*xres + (mj - 1)]
            - d[(mi - 1)*xres + (mj + 1)]
            - d[(mi + 1)*xres + (mj - 1)]
            + d[(mi + 1)*xres + (mj + 1)]);
    szyy = (d[(mi - 1)*xres + (mj - 1)]
            + d[(mi - 1)*xres + mj]
            + d[(mi - 1)*xres + (mj + 1)]
            + d[(mi + 1)*xres + (mj - 1)]
            + d[(mi + 1)*xres + mj]
            + d[(mi + 1)*xres + (mj + 1)]);

    m[0] = 9.0;
    m[1] = m[2] = m[3] = m[5] = 6.0;
    m[4] = 4.0;
    gwy_math_choleski_decompose(3, m);

    rhs[0] = sz;
    rhs[1] = szxx;
    rhs[2] = szyy;
    gwy_math_choleski_solve(3, m, rhs);

    bx = szx/6.0;
    by = szy/6.0;
    cxx = rhs[1];
    cxy = szxy/4.0;
    cyy = rhs[2];

    D = 4.0*cxx*cyy - cxy*cxy;
    /* Don't try the sub-pixel refinement if bad cancellation occurs. */
    if (fabs(D) < 1e-8*MAX(fabs(4.0*cxx*cyy), fabs(cxy*cxy)))
        return;

    sx = (by*cxy - 2.0*bx*cyy)/D;
    sy = (bx*cxy - 2.0*by*cxx)/D;

    /* Don't trust the sub-pixel refinement if it moves the maximum outside
     * the 3×3 neighbourhood. */
    gwy_debug("refinements: %g, %g", sx, sy);
    if (fabs(sx) > 1.5 || fabs(sy) > 1.5)
        return;

    *x += sx;
    *y += sy;
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

static const gchar selection_mode_key[] = "/module/measure_lattice/selection_mode";
static const gchar zoom_acf_key[]       = "/module/measure_lattice/zoom_acf";
static const gchar zoom_psdf_key[]      = "/module/measure_lattice/zoom_psdf";

static void
sanitize_args(LatMeasArgs *args)
{
    args->selection_mode = MIN(args->selection_mode, SELECTION_NMODES-1);
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

    gwy_container_gis_enum_by_name(container, selection_mode_key,
                                   &args->selection_mode);
    gwy_container_gis_enum_by_name(container, zoom_acf_key, &args->zoom_acf);
    gwy_container_gis_enum_by_name(container, zoom_psdf_key, &args->zoom_psdf);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container, LatMeasArgs *args)
{
    gwy_container_set_enum_by_name(container, selection_mode_key,
                                   args->selection_mode);
    gwy_container_set_enum_by_name(container, zoom_acf_key, args->zoom_acf);
    gwy_container_set_enum_by_name(container, zoom_psdf_key, args->zoom_psdf);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
