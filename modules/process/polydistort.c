/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-process.h>
#include <libprocess/linestats.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/interpolation.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <app/gwyapp.h>

#define DISTORT_RUN_MODES (GWY_RUN_INTERACTIVE | GWY_RUN_IMMEDIATE)

enum {
    PREVIEW_SIZE = 400,
    MAX_DEGREE   = 3,
    NCOEFF       = (MAX_DEGREE + 1)*(MAX_DEGREE + 1)
};

typedef enum {
    PREVIEW_CORRECTED = 0,
    PREVIEW_ORIGINAL  = 1,
    PREVIEW_LAST
} DistortPreviewType;

/* Data for this function. */
typedef struct {
    DistortPreviewType preview_type;
    GwyInterpolationType interp;
    gdouble *xcoeff;
    gdouble *ycoeff;
} DistortArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *interp;
    GtkWidget **xcoeff;
    GtkWidget **ycoeff;
    GSList *preview_type;
    GwyContainer *mydata;
    GwyDataField *result;
    gboolean computed;
    DistortArgs *args;
} DistortControls;

static gboolean   module_register               (void);
static void       polydistort                   (GwyContainer *data,
                                                 GwyRunType run);
static void       distort_dialog                (DistortArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 gint id);
static GtkWidget* coeff_table_new               (gdouble *coeff,
                                                 GtkWidget **entry,
                                                 gpointer id,
                                                 DistortControls *controls);
static void       run_noninteractive            (DistortArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GwyDataField *result,
                                                 gint id);
static void       distort_dialog_update_controls(DistortControls *controls,
                                                 DistortArgs *args);
static void       distort_dialog_update_values  (DistortControls *controls,
                                                 DistortArgs *args);
static gboolean   distort_coeff_unfocus         (GtkEntry *entry,
                                                 GdkEventFocus *event,
                                                 DistortControls *controls);
static void       distort_coeff_changed         (GtkEntry *entry,
                                                 DistortControls *controls);
static void       distort_invalidate            (GObject *obj,
                                                 DistortControls *controls);
static void       preview_type_changed          (GtkWidget *button,
                                                 DistortControls *controls);
static void       preview                       (DistortControls *controls,
                                                 DistortArgs *args);
static void       distort_load_args             (GwyContainer *container,
                                                 DistortArgs *args);
static void       distort_save_args             (GwyContainer *container,
                                                 DistortArgs *args);
static void       distort_sanitize_args         (DistortArgs *args);
static void       distort_do                    (DistortArgs *args,
                                                 GwyDataField *dfield,
                                                 GwyDataField *result);

static const DistortArgs distort_defaults = {
    PREVIEW_ORIGINAL,
    GWY_INTERPOLATION_BSPLINE,
    NULL,
    NULL
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Apllies polynomial distortion in the horizontal plane."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("polydistort",
                              (GwyProcessFunc)&polydistort,
                              N_("/_Correct Data/Pol_ynomial Distortion..."),
                              NULL,
                              DISTORT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Apllies polynomial distortion in the "
                                 "horizontal plane"));

    return TRUE;
}

static void
polydistort(GwyContainer *data, GwyRunType run)
{
    DistortArgs args;
    GwyDataField *dfield;
    gint id;

    g_return_if_fail(run & DISTORT_RUN_MODES);
    distort_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, data, dfield, NULL, id);
    else {
        distort_dialog(&args, data, dfield, id);
        distort_save_args(gwy_app_settings_get(), &args);
    }
}

static void
distort_dialog(DistortArgs *args,
               GwyContainer *data,
               GwyDataField *dfield,
               gint id)
{
    enum {
        RESPONSE_RESET   = 1,
        RESPONSE_PREVIEW = 2
    };

    GtkWidget *dialog, *table, *hbox, *label;
    DistortControls controls;
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    gint row;

    memset(&controls, 0, sizeof(DistortControls));
    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Distort by Polynom"),
                                         NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    g_object_unref(controls.mydata);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(7, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->interp, args->interp, TRUE);
    gwy_table_attach_hscale(table, row, _("_Interpolation type:"), NULL,
                            GTK_OBJECT(controls.interp),
                            GWY_HSCALE_WIDGET_NO_EXPAND);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new(_("Preview type"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.preview_type
        = gwy_radio_buttons_createl(G_CALLBACK(preview_type_changed), &controls,
                                    args->preview_type,
                                    _("Or_iginal"), PREVIEW_ORIGINAL,
                                    _("Correc_ted data"), PREVIEW_CORRECTED,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.preview_type,
                                            GTK_TABLE(table), 3, row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("X Coefficients")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.xcoeff = g_new0(GtkWidget*, NCOEFF);
    gtk_table_attach(GTK_TABLE(table),
                     coeff_table_new(args->xcoeff, controls.xcoeff,
                                     (gpointer)"x", &controls),
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Y Coefficients")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.ycoeff = g_new0(GtkWidget*, NCOEFF);
    gtk_table_attach(GTK_TABLE(table),
                     coeff_table_new(args->ycoeff, controls.ycoeff,
                                     (gpointer)"y", &controls),
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.computed = FALSE;
    /* Set up initial layer keys properly */
    preview_type_changed(NULL, &controls);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            distort_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            gwy_object_unref(controls.result);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            args->interp = distort_defaults.interp;
            memset(args->xcoeff, 0, NCOEFF*sizeof(gdouble));
            memset(args->ycoeff, 0, NCOEFF*sizeof(gdouble));
            distort_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            distort_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    distort_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);

    if (controls.computed)
        run_noninteractive(args, data, dfield, controls.result, id);
    else {
        gwy_object_unref(controls.result);
        run_noninteractive(args, data, dfield, NULL, id);
    }
}

static GtkWidget*
coeff_table_new(gdouble *coeff,
                GtkWidget **entry,
                gpointer id,
                DistortControls *controls)
{
    GtkWidget *widget, *label;
    GtkTable *table;
    gchar buf[24];
    gint i, j, k;

    widget = gtk_table_new(MAX_DEGREE + 2, MAX_DEGREE + 2, FALSE);
    table = GTK_TABLE(widget);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);

    for (i = 0; i < MAX_DEGREE + 1; i++) {
        for (j = 0; j < MAX_DEGREE + 1; j++) {
            if (i == 0 && j == 0)
                continue;
            if (i + j > MAX_DEGREE)
                continue;

            k = i*(MAX_DEGREE + 1) + j;
            entry[k] = gtk_entry_new();
            gtk_entry_set_width_chars(GTK_ENTRY(entry[k]), 6);
            gtk_table_attach(table, entry[k], j+1, j+2, i+1, i+2, 0, 0, 0, 0);
            g_object_set_data(G_OBJECT(entry[k]), "y", GINT_TO_POINTER(i));
            g_object_set_data(G_OBJECT(entry[k]), "x", GINT_TO_POINTER(j));
            g_object_set_data(G_OBJECT(entry[k]), "id", id);
            g_signal_connect(entry[k], "activate",
                             G_CALLBACK(distort_coeff_changed), controls);
            g_signal_connect(entry[k], "focus-out-event",
                             G_CALLBACK(distort_coeff_unfocus), controls);
        }
    }

    for (i = 0; i < MAX_DEGREE + 1; i++) {
        label = gtk_label_new(NULL);
        if (i == 0)
            strcpy(buf, "1");
        else if (i == 1)
            strcpy(buf, "y");
        else
            g_snprintf(buf, sizeof(buf), "y<sup>%d</sup>", i);
        gtk_label_set_markup(GTK_LABEL(label), buf);
        gtk_table_attach(table, label, 0, 1, i+1, i+2, 0, 0, 0, 0);
    }

    for (j = 0; j < MAX_DEGREE + 1; j++) {
        label = gtk_label_new(NULL);
        if (j == 0)
            strcpy(buf, "1");
        else if (j == 1)
            strcpy(buf, "x");
        else
            g_snprintf(buf, sizeof(buf), "x<sup>%d</sup>", j);
        gtk_label_set_markup(GTK_LABEL(label), buf);
        gtk_table_attach(table, label, j+1, j+2, 0, 1, 0, 0, 0, 0);
    }

    return widget;
}

/* XXX: Eats result */
static void
run_noninteractive(DistortArgs *args,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   GwyDataField *result,
                   gint id)
{
    gint newid;

    newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
    gwy_app_set_data_field_title(data, newid, _("Distort-corrected"));
    gwy_app_sync_data_items(data, data, id, newid, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_RANGE,
                            0);

    g_object_unref(result);
}

static void
distort_dialog_update_controls(DistortControls *controls,
                               DistortArgs *args)
{
    guint i, j;

    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);

    /* TODO: Update coeff */
}

static void
distort_dialog_update_values(DistortControls *controls,
                             DistortArgs *args)
{
}

static gboolean
distort_coeff_unfocus(GtkEntry *entry,
                      G_GNUC_UNUSED GdkEventFocus *event,
                      DistortControls *controls)
{
    distort_coeff_changed(entry, controls);
    return FALSE;
}

static void
distort_coeff_changed(GtkEntry *entry,
                      DistortControls *controls)
{
    gint i, j;
    const gchar *id;
    gchar *end;
    gdouble val;
    gdouble *coeff;

    /*
       if (controls->in_update)
       return;
     */

    i = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(entry), "y"));
    j = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(entry), "x"));
    id = g_object_get_data(G_OBJECT(entry), "id");
    if (gwy_strequal(id, "x"))
        coeff = controls->args->xcoeff;
    else if (gwy_strequal(id, "y"))
        coeff = controls->args->ycoeff;
    else
        g_return_if_reached();

    val = g_strtod(gtk_entry_get_text(entry), &end);
    if (val == coeff[i*(MAX_DEGREE + 1) + j])
        return;

    coeff[i*(MAX_DEGREE + 1) + j] = val;
    distort_invalidate(NULL, controls);
}

static void
distort_invalidate(G_GNUC_UNUSED GObject *obj,
                   DistortControls *controls)
{
    controls->computed = FALSE;
}

static void
preview_type_changed(G_GNUC_UNUSED GtkWidget *button,
                     DistortControls *controls)
{
    GwyPixmapLayer *blayer;

    controls->args->preview_type
        = gwy_radio_buttons_get_current(controls->preview_type);

    blayer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));
    switch (controls->args->preview_type) {
        case PREVIEW_CORRECTED:
        gwy_layer_basic_set_presentation_key(GWY_LAYER_BASIC(blayer),
                                             "/1/data");
        break;

        case PREVIEW_ORIGINAL:
        gwy_layer_basic_set_presentation_key(GWY_LAYER_BASIC(blayer), NULL);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
preview(DistortControls *controls,
        DistortArgs *args)
{
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    gwy_app_wait_cursor_start(GTK_WINDOW(controls->dialog));
    if (!controls->result) {
        controls->result = gwy_data_field_duplicate(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/1/data",
                                         controls->result);
    }
    else
        gwy_data_field_copy(dfield, controls->result, FALSE);
    distort_do(args, dfield, controls->result);
    gwy_data_field_data_changed(controls->result);
    gwy_app_wait_cursor_finish(GTK_WINDOW(controls->dialog));

    controls->computed = TRUE;
}

static void
distort_do(DistortArgs *args,
           GwyDataField *dfield,
           GwyDataField *result)
{
}

static const gchar interp_key[] = "/module/polydistort/interp";
static const gchar coeff_key[]  = "/module/polydistort/%ccoeff-%d-%d";

static void
distort_sanitize_args(DistortArgs *args)
{
    args->interp = gwy_enum_sanitize_value(args->interp,
                                           GWY_TYPE_INTERPOLATION_TYPE);
}

static gdouble*
load_coeffs(gdouble *coeff,
            gchar type,
            GwyContainer *settings)
{
    gchar buf[40];
    gint i, j;

    if (!coeff)
        coeff = g_new0(gdouble, NCOEFF);

    for (i = 0; i < MAX_DEGREE + 1; i++) {
        for (j = 0; j < MAX_DEGREE + 1; j++) {
            if (i + j > MAX_DEGREE)
                continue;

            g_snprintf(buf, sizeof(buf), coeff_key, type, i, j);
            gwy_container_gis_double_by_name(settings, coeff_key,
                                             coeff + i*(MAX_DEGREE + 1) + j);
        }
    }

    return coeff;
}

static void
distort_load_args(GwyContainer *container,
                  DistortArgs *args)
{

    *args = distort_defaults;

    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    args->xcoeff = load_coeffs(args->xcoeff, 'x', container);
    args->ycoeff = load_coeffs(args->ycoeff, 'y', container);

    distort_sanitize_args(args);
}

static void
save_coeffs(const gdouble *coeff,
            gchar type,
            GwyContainer *settings)
{
    gchar buf[40];
    gdouble v;
    gint i, j;

    for (i = 0; i < MAX_DEGREE + 1; i++) {
        for (j = 0; j < MAX_DEGREE + 1; j++) {
            if (i + j > MAX_DEGREE)
                continue;

            g_snprintf(buf, sizeof(buf), coeff_key, type, i, j);
            v = coeff[i*(MAX_DEGREE + 1) + j];
            if (!v)
                gwy_container_remove_by_name(settings, coeff_key);
            else
                gwy_container_set_double_by_name(settings, coeff_key, v);
        }
    }
}

static void
distort_save_args(GwyContainer *container,
                  DistortArgs *args)
{
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    save_coeffs(args->xcoeff, 'x', container);
    save_coeffs(args->ycoeff, 'y', container);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

