/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include "gwyapp.h"

typedef enum {
    GWY_MASKCOR_OBJECTS,
    GWY_MASKCOR_MAXIMA,
    GWY_MASKCOR_SCORE,
    GWY_MASKCOR_LAST
} GwyMaskcorResult;

typedef struct {
    GwyMaskcorResult result;
    gdouble threshold;
    GwyDataWindow *win1;
    GwyDataWindow *win2;
} GwyMaskcorArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *result;
} GwyMaskcorControls;

static void       gwy_data_maskcor_load_args        (GwyContainer *settings,
                                                     GwyMaskcorArgs *args);
static void       gwy_data_maskcor_save_args        (GwyContainer *settings,
                                                     GwyMaskcorArgs *args);
static GtkWidget* gwy_data_maskcor_window_construct (GwyMaskcorArgs *args);
static GtkWidget* gwy_data_maskcor_data_option_menu (GwyDataWindow **operand);
static void       gwy_data_maskcor_operation_cb     (GtkWidget *item,
                                                     GwyMaskcorArgs *args);
static void       gwy_data_maskcor_threshold_cb     (GtkAdjustment *adj,
                                                     gdouble *value);
static void       gwy_data_maskcor_data_cb          (GtkWidget *item);
static gboolean   gwy_data_maskcor_do               (GwyMaskcorArgs *args,
                                                     GtkWidget *maskcor_window);


static const GwyEnum results[] = {
    { "Objects marked",       GWY_MASKCOR_OBJECTS },
    { "Correlation maxima",  GWY_MASKCOR_MAXIMA },
    { "Correlation score",  GWY_MASKCOR_SCORE },
};

static const GwyMaskcorArgs gwy_data_maskcor_defaults = {
    GWY_MASKCOR_OBJECTS, 0.95, NULL, NULL
};

void
gwy_app_data_maskcor(void)
{
    static GwyMaskcorArgs *args = NULL;
    static GtkWidget *maskcor_window = NULL;
    static gpointer win1 = NULL, win2 = NULL;
    GwyContainer *settings;
    gboolean ok = FALSE;

    if (!args) {
        args = g_new(GwyMaskcorArgs, 1);
        *args = gwy_data_maskcor_defaults;
    }
    settings = gwy_app_settings_get();
    if (!maskcor_window) {
        args->win1 = win1 ? win1 : gwy_app_data_window_get_current();
        args->win2 = win2 ? win2 : gwy_app_data_window_get_current();

        gwy_data_maskcor_load_args(settings, args);
        maskcor_window = gwy_data_maskcor_window_construct(args);
    }
    gtk_window_present(GTK_WINDOW(maskcor_window));
    do {
        switch (gtk_dialog_run(GTK_DIALOG(maskcor_window))) {
            case GTK_RESPONSE_CLOSE:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            ok = TRUE;
            break;

            case GTK_RESPONSE_APPLY:
            ok = gwy_data_maskcor_do(args, maskcor_window);
            if (ok) {
                gwy_data_maskcor_save_args(settings, args);
                if (win1)
                    g_object_remove_weak_pointer(G_OBJECT(win1), &win1);
                win1 = args->win1;
                if (win1)
                    g_object_add_weak_pointer(G_OBJECT(win1), &win1);
                if (win2)
                    g_object_remove_weak_pointer(G_OBJECT(win2), &win2);
                win2 = args->win2;
                if (win2)
                    g_object_add_weak_pointer(G_OBJECT(args->win2), &win2);
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    gtk_widget_destroy(maskcor_window);
    maskcor_window = NULL;
}

static GtkWidget*
gwy_data_maskcor_window_construct(GwyMaskcorArgs *args)
{
    GtkWidget *dialog, *table, *omenu, *label, *spin;
    GtkObject *adj;

    dialog = gtk_dialog_new_with_buttons(_("Create mask by correlation"),
                                         GTK_WINDOW(gwy_app_main_window_get()),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         NULL);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 8);

    table = gtk_table_new(2, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);

    /***** First operand *****/
    label = gtk_label_new_with_mnemonic(_("_Data field to modify:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

    omenu = gwy_data_maskcor_data_option_menu(&args->win1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 0, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);


    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("_Correlation kernel"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);


    omenu = gwy_data_maskcor_data_option_menu(&args->win2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 1, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);

    /**** Parameters ********/
    adj = gtk_adjustment_new(args->threshold, -1.0, 1.0, 0.01, 0.1, 0.1);
    spin = gwy_table_attach_spinbutton(table, 2, _("T_hreshold"), "", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    g_signal_connect(adj, "value_changed",
                     G_CALLBACK(gwy_data_maskcor_threshold_cb),
                     &args->threshold);

    /***** Result *****/
    label = gtk_label_new_with_mnemonic(_("_Result:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);

    omenu = gwy_option_menu_create(results, G_N_ELEMENTS(results),
                                   "operation",
                                   G_CALLBACK(gwy_data_maskcor_operation_cb),
                                   args,
                                   args->result);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 3, 4);

    gtk_widget_show_all(dialog);

    return dialog;
}

GtkWidget*
gwy_data_maskcor_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(gwy_data_maskcor_data_cb),
                                        NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}

static void
gwy_data_maskcor_operation_cb(GtkWidget *item, GwyMaskcorArgs *args)
{
    args->result
        = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "operation"));
}

static void
gwy_data_maskcor_threshold_cb(GtkAdjustment *adj, gdouble *value)
{
    *value = gtk_adjustment_get_value(adj);
}

static void
gwy_data_maskcor_data_cb(GtkWidget *item)
{
    GtkWidget *menu;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);

    p = g_object_get_data(G_OBJECT(item), "data-window");
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}

void
plot_correlated(GwyDataField * retfield, gint xsize, gint ysize,
                gdouble threshold)
{
    GwyDataField *field;
    gint i, j;

    field = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(retfield)));
    gwy_data_field_fill(retfield, 0);

    for (i = 0; i < retfield->xres; i++) {
        for (j = 0; j < retfield->yres; j++) {
            if ((field->data[i + retfield->xres * j]) > threshold)
                gwy_data_field_area_fill(retfield, i - xsize/2, j - ysize/2,
                                         i + xsize/2, j + ysize/2, 1.0);
        }
    }

}

void
plot_maxima(GwyDataField * retfield, gdouble threshold)
{
    gint i, j;

    for (i = 0; i < retfield->xres; i++) {
        for (j = 0; j < retfield->yres; j++) {
            if (retfield->data[i + retfield->xres * j] > threshold)
                retfield->data[i + retfield->xres * j] = 1;
            else
                retfield->data[i + retfield->xres * j] = 0;
        }
    }

}

static gboolean
gwy_data_maskcor_do(GwyMaskcorArgs *args,
                    G_GNUC_UNUSED GtkWidget *maskcor_window)
{
    GtkWidget *data_window;
    GwyContainer *data, *ret, *kernel;
    GwyDataField *dfield, *kernelfield, *retfield;
    GwyDataWindow *operand1, *operand2;
    gint iteration = 0;
    GwyComputationStateType state;

    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(operand1 != NULL && operand2 != NULL, FALSE);

    data = gwy_data_window_get_data(operand1);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                             "/0/data"));

    ret = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    retfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(ret,
                                                             "/0/data"));

    kernel = gwy_data_window_get_data(operand2);
    kernelfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(kernel,
                                                                  "/0/data"));

    state = GWY_COMP_INIT;
    gwy_app_wait_start(GTK_WIDGET(gwy_app_data_window_get_current()),
                       "Initializing...");
    do {
        gwy_data_field_correlate_iteration(dfield, kernelfield, retfield,
                                           &state, &iteration);
        gwy_app_wait_set_message("Correlating...");
        if (!gwy_app_wait_set_fraction(iteration/(gdouble)(dfield->xres - (kernelfield->xres)/2)))
            return FALSE;

    } while (state != GWY_COMP_FINISHED);
    gwy_app_wait_finish();

    /*score - do new data with score*/
    if (args->result == GWY_MASKCOR_SCORE) {
        if (gwy_container_contains_by_name(ret, "/0/mask"))
            gwy_container_remove_by_name(ret, "/0/mask");

        data_window = gwy_app_data_window_create(ret);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    }
    else { /*add mask*/
        if (args->result == GWY_MASKCOR_OBJECTS) {
            plot_correlated(retfield, kernelfield->xres, kernelfield->yres,
                            args->threshold);
        }
        else if (args->result == GWY_MASKCOR_MAXIMA) {
            plot_maxima(retfield, args->threshold);
        }
        gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(retfield));
    }
    gwy_app_data_view_update(gwy_data_window_get_data_view(operand1));

    return TRUE;
}


static const gchar *result_key = "/app/maskcor/result";
static const gchar *threshold_key = "/app/maskcor/threshold";

static void
gwy_data_maskcor_load_args(GwyContainer *settings,
                         GwyMaskcorArgs *args)
{
    gwy_container_gis_enum_by_name(settings, result_key, &args->result);
    gwy_container_gis_double_by_name(settings, threshold_key, &args->threshold);
}

static void
gwy_data_maskcor_save_args(GwyContainer *settings,
                         GwyMaskcorArgs *args)
{
    gwy_container_set_enum_by_name(settings, result_key, args->result);
    gwy_container_set_double_by_name(settings, threshold_key, args->threshold);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

