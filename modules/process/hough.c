/*
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/hough.h>
#include <libprocess/filters.h>
#include <libprocess/cwt.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define HOUGH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    GWY_HOUGH_OUTPUT_LINE   = 0,
    GWY_HOUGH_OUTPUT_CIRCLE = 1
} GwyHoughOutputType;


typedef struct {
    GwyHoughOutputType output;
    gint circle_size;
} HoughArgs;

typedef struct {
    GtkWidget *output;
    GtkObject *circle_size;
    GtkWidget *circle_spin;
} HoughControls;

static gboolean    module_register            (void);
static void        hough                      (GwyContainer *data,
                                               GwyRunType run);
static gboolean    hough_dialog               (HoughArgs *args);
static void        hough_load_args            (GwyContainer *container,
                                               HoughArgs *args);
static void        hough_save_args            (GwyContainer *container,
                                               HoughArgs *args);
static void        hough_dialog_update        (HoughControls *controls,
                                               HoughArgs *args);
static void        type_changed_cb            (GtkComboBox *combo,
                                               HoughControls *controls);

static const HoughArgs hough_defaults = {
    GWY_HOUGH_OUTPUT_LINE,
    10,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Hough transform."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("hough",
                              (GwyProcessFunc)&hough,
                              N_("/_Integral Transforms/_Hough..."),
                              NULL,
                              HOUGH_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Compute Hough transform"));

    return TRUE;
}

static void
hough(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *edgefield, *result, *f1, *f2;
    GwySIUnit *siunit;
    gboolean ok;
    HoughArgs args;
    gint newid;
    gchar title[30];

    g_return_if_fail(run & HOUGH_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     0);
    g_return_if_fail(dfield);

    hough_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = hough_dialog(&args);
        hough_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    result = gwy_data_field_new_alike(dfield, FALSE);
    siunit = gwy_si_unit_new(NULL);
    gwy_data_field_set_si_unit_z(result, siunit);
    g_object_unref(siunit);
    newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
    g_object_unref(result);

    edgefield = gwy_data_field_duplicate(dfield);

    f1 = gwy_data_field_duplicate(dfield);
    f2 = gwy_data_field_duplicate(dfield);

    gwy_data_field_filter_canny(edgefield, 0.1);
    gwy_data_field_filter_sobel(f1, GWY_ORIENTATION_HORIZONTAL);
    gwy_data_field_filter_sobel(f2, GWY_ORIENTATION_VERTICAL);
    if (args.output == GWY_HOUGH_OUTPUT_LINE) {
        g_snprintf(title, sizeof(title), "Hough line");
        gwy_data_field_hough_line(edgefield,
                              f1,
                              f2,
                              result,
                              1,
                              FALSE);
    }
    else  {
        g_snprintf(title, sizeof(title), "Hough circle r=%d px",
                   args.circle_size);
        gwy_data_field_hough_circle(edgefield,
                                    f1,
                                    f2,
                                    result,
                                    args.circle_size);
    }

    gwy_app_set_data_field_title(data, newid, title);
    gwy_data_field_data_changed(result);
    g_object_unref(edgefield);
    g_object_unref(f1);
    g_object_unref(f2);
}


static gboolean
hough_dialog(HoughArgs *args)
{
    enum { RESPONSE_RESET = 1 };
    static const GwyEnum hough_outputs[] = {
        { N_("Line"),   GWY_HOUGH_OUTPUT_LINE,   },
        { N_("Circle"), GWY_HOUGH_OUTPUT_CIRCLE, },
    };

    GtkWidget *dialog, *table;
    HoughControls controls;
    gint response, row;

    dialog = gtk_dialog_new_with_buttons(_("Hough transform"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    row = 0;

    controls.output
        = gwy_enum_combo_box_new(hough_outputs, G_N_ELEMENTS(hough_outputs),
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->output, args->output, TRUE);
    g_signal_connect(GTK_COMBO_BOX(controls.output), "changed",
                     G_CALLBACK(type_changed_cb), &controls);
    gwy_table_attach_row(table, row, _("_Transform type:"), NULL,
                         controls.output);



    row++;


    controls.circle_size
        = gtk_adjustment_new(args->circle_size, 1.0, 1000.0, 1, 10, 0);
    controls.circle_spin
        = gwy_table_attach_spinbutton(table, 1, _("_Circle size:"), _("pixels"),
                                      controls.circle_size);

    hough_dialog_update(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->circle_size
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.circle_size));
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = hough_defaults;
            hough_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->circle_size
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.circle_size));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
type_changed_cb(GtkComboBox *combo, HoughControls *controls)
{
    if (gwy_enum_combo_box_get_active(combo) == GWY_HOUGH_OUTPUT_CIRCLE)
        gtk_widget_set_sensitive(controls->circle_spin, TRUE);
    else
        gtk_widget_set_sensitive(controls->circle_spin, FALSE);
}

static void
hough_dialog_update(HoughControls *controls,
                  HoughArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->circle_size),
                             args->circle_size);
    if (args->output == GWY_HOUGH_OUTPUT_CIRCLE)
        gtk_widget_set_sensitive(controls->circle_spin, TRUE);
    else
        gtk_widget_set_sensitive(controls->circle_spin, FALSE);
}

static const gchar output_key[]      = "/module/hough/output";
static const gchar circle_size_key[] = "/module/hough/circle_size";

static void
hough_sanitize_args(HoughArgs *args)
{
    args->output = MIN(args->output, GWY_HOUGH_OUTPUT_CIRCLE);
    args->circle_size = CLAMP(args->circle_size, 1.0, 1000.0);
}

static void
hough_load_args(GwyContainer *container,
              HoughArgs *args)
{
    *args = hough_defaults;

    gwy_container_gis_enum_by_name(container, output_key, &args->output);
    gwy_container_gis_int32_by_name(container, circle_size_key,
                                    &args->circle_size);
    hough_sanitize_args(args);
}

static void
hough_save_args(GwyContainer *container,
              HoughArgs *args)
{
    gwy_container_set_enum_by_name(container, output_key, args->output);
    gwy_container_set_int32_by_name(container, circle_size_key,
                                    args->circle_size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
