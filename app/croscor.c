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
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include "gwyapp.h"

typedef enum {
    GWY_CROSCOR_ABS,
    GWY_CROSCOR_X,
    GWY_CROSCOR_Y,
    GWY_CROSCOR_DIR,
    GWY_CROSCOR_ANG,
    GWY_CROSCOR_SCORE,
    GWY_CROSCOR_LAST
} GwyCroscorResult;

typedef struct {
    GwyCroscorResult result;
    gint search_x;
    gint search_y;
    gint window_x;
    gint window_y;
    gdouble rot_pos;
    gdouble rot_neg;
    GwyDataWindow *win1;
    GwyDataWindow *win2;
    gboolean mask;
    gdouble thresh;
} GwyCroscorArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *result;
    GtkObject *search_area_x;
    GtkObject *search_area_y;
    GtkObject *window_area_x;
    GtkObject *window_area_y;
    GtkObject *rotation_neg;
    GtkObject *rotation_pos;
    GtkWidget *mask;
    GtkObject *threshold;
} GwyCroscorControls;

static void       gwy_data_croscor_load_args       (GwyContainer *settings,
                                                    GwyCroscorArgs *args);
static void       gwy_data_croscor_save_args       (GwyContainer *settings,
                                                    GwyCroscorArgs *args);
static GtkWidget* gwy_data_croscor_window_construct(GwyCroscorArgs *args,
                                                    GwyCroscorControls *controls);
static GtkWidget* gwy_data_croscor_data_option_menu(GtkWidget *entry,
                                                    GwyDataWindow **operand);
static void       gwy_data_croscor_operation_cb    (GtkWidget *item,
                                                    GwyCroscorArgs *args);
static void       gwy_data_croscor_data_cb         (GtkWidget *item);
static gboolean   gwy_data_croscor_do              (GwyCroscorArgs *args,
                                                    GtkWidget *croscor_window,
                                                    GwyCroscorControls *controls);


static const GwyEnum results[] = {
    { "Absolute",       GWY_CROSCOR_ABS },
    { "X distance",  GWY_CROSCOR_X },
    { "Y distance",  GWY_CROSCOR_Y },
    { "Angle",    GWY_CROSCOR_DIR },
};

static const GwyCroscorArgs gwy_data_croscor_defaults = {
    GWY_CROSCOR_ABS, 10, 10, 25, 25, 0.0, 0.0, NULL, NULL, 1, 0.95
};

void
gwy_app_data_croscor(void)
{
    static GwyCroscorArgs *args = NULL;
    static GwyCroscorControls *controls = NULL;
    static GtkWidget *croscor_window = NULL;
    static gpointer win1 = NULL, win2 = NULL;
    GwyContainer *settings;
    gboolean ok = FALSE;

    if (!args) {
        args = g_new(GwyCroscorArgs, 1);
        *args = gwy_data_croscor_defaults;
    }
    if (!controls) {
        controls = g_new(GwyCroscorControls, 1);
    }

    settings = gwy_app_settings_get();
    if (!croscor_window) {
        args->win1 = win1 ? win1 : gwy_app_data_window_get_current();
        args->win2 = win2 ? win2 : gwy_app_data_window_get_current();

        gwy_data_croscor_load_args(settings, args);
        croscor_window = gwy_data_croscor_window_construct(args, controls);
    }
    gtk_window_present(GTK_WINDOW(croscor_window));
    do {
        switch (gtk_dialog_run(GTK_DIALOG(croscor_window))) {
            case GTK_RESPONSE_CLOSE:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            ok = TRUE;
            break;

            case GTK_RESPONSE_APPLY:
            ok = gwy_data_croscor_do(args, croscor_window, controls);
            if (ok) {
                gwy_data_croscor_save_args(settings, args);
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

    gtk_widget_destroy(croscor_window);
    croscor_window = NULL;
}

static GtkWidget*
gwy_data_croscor_window_construct(GwyCroscorArgs *args,
                                  GwyCroscorControls *controls)
{
    GtkWidget *dialog, *table, *omenu, *entry, *label, *spin;

    dialog = gtk_dialog_new_with_buttons(_("Data Croscorrelation"),
                                         GTK_WINDOW(gwy_app_main_window_get()),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         NULL);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 8);

    table = gtk_table_new(2, 10, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);

    /***** First operand *****/
    label = gtk_label_new_with_mnemonic(_("_First data field:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

    entry = gtk_entry_new();
    omenu = gwy_data_croscor_data_option_menu(entry, &args->win1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 0, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);


    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("_Second data field:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);


    omenu = gwy_data_croscor_data_option_menu(entry, &args->win2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 1, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);

    /**** Parameters ********/
    /*search size*/
    label = gtk_label_new_with_mnemonic(_("_Search size [pixels]"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);

    controls->search_area_x = gtk_adjustment_new(args->search_x,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, 3, _("width"), _(""),
                                controls->search_area_x);
    controls->search_area_y = gtk_adjustment_new(args->search_y,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, 4, _("height"), _(""),
                                controls->search_area_y);

    /*window size*/
    label = gtk_label_new_with_mnemonic(_("_Window size [pixels]"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 5, 6);

    controls->window_area_x = gtk_adjustment_new(args->window_x,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, 6, _("width"), _(""),
                                controls->window_area_x);
    controls->window_area_y = gtk_adjustment_new(args->window_y,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, 7, _("height"), _(""),
                                controls->window_area_y);

    /*do mask of thresholds*/
/*    label = gtk_label_new_with_mnemonic(_("_Low score results mask"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 8, 9);*/
    controls->mask = gtk_check_button_new_with_label("add");
    gwy_table_attach_row(table, 8, _("_Low score results mask:"), "",
                         controls->mask);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->mask), args->mask);

    controls->threshold = gtk_adjustment_new(args->thresh,
                                             -1, 1, 0.005, 0.05, 0);
    spin = gwy_table_attach_spinbutton(table, 9, _("threshold value"), _(""),
                                       controls->threshold);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);

    /***** Result *****/
    label = gtk_label_new_with_mnemonic(_("_Result:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 10, 11);

    omenu = gwy_option_menu_create(results, G_N_ELEMENTS(results),
                                   "operation",
                                   G_CALLBACK(gwy_data_croscor_operation_cb),
                                   args,
                                   args->result);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 10, 11);

    gtk_widget_show_all(dialog);

    return dialog;
}

GtkWidget*
gwy_data_croscor_data_option_menu(GtkWidget *entry,
                                  GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(gwy_data_croscor_data_cb),
                                        NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "entry", entry);
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}

static void
gwy_data_croscor_operation_cb(GtkWidget *item, GwyCroscorArgs *args)
{
    args->result
        = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "operation"));
}

static void
gwy_data_croscor_data_cb(GtkWidget *item)
{
    GtkWidget *menu, *entry;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);
    entry = GTK_WIDGET(g_object_get_data(G_OBJECT(menu), "entry"));

    p = g_object_get_data(G_OBJECT(item), "data-window");
    gtk_widget_set_sensitive(entry, p == NULL);
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}

void
abs_field(GwyDataField * dfieldx, GwyDataField * dfieldy)
{
    gint i;

    for (i = 0; i < (dfieldx->xres * dfieldx->yres); i++) {
        dfieldx->data[i] =
            sqrt(dfieldx->data[i] * dfieldx->data[i] +
                 dfieldy->data[i] * dfieldy->data[i]);
    }
}

void
dir_field(GwyDataField * dfieldx, GwyDataField * dfieldy)
{
    gint i;

    for (i = 0; i < (dfieldx->xres * dfieldx->yres); i++) {
        dfieldx->data[i] = atan2(dfieldy->data[i], dfieldx->data[i]);
    }
}

static gboolean
gwy_data_croscor_do(GwyCroscorArgs * args,
                    GtkWidget *croscor_window, GwyCroscorControls * controls)
{
    GtkWidget *dialog, *data_window;
    GwyContainer *data;
    GwyDataField *dfieldx, *dfieldy, *dfield1, *dfield2, *score;
    GwyDataWindow *operand1, *operand2;
    gint iteration = 0;
    GwyComputationStateType state;

    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(operand1 != NULL && operand2 != NULL, FALSE);

    /*get all parameters back */
    args->search_x =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->search_area_x));
    args->search_y =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->search_area_y));
    args->window_x =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->window_area_x));
    args->window_y =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->window_area_y));
    args->thresh =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold));
    args->mask =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->mask));


    data = gwy_data_window_get_data(operand1);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    data = gwy_data_window_get_data(operand2);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));


    if (dfield1->xres != dfield2->xres || dfield1->yres != dfield2->yres) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(croscor_window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_INFO,
                                        GTK_BUTTONS_CLOSE,
                                        _("Both data fields must have same size."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;
    }

    /*result fields - after computation result should be at dfieldx */
    data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    dfieldx = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    dfieldy = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfieldx)));
    score = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfieldx)));

    /*compute crosscorelation */


    iteration = 0;
    state = GWY_COMP_INIT;
    gwy_app_wait_start(GTK_WIDGET(gwy_app_data_window_get_current()),
                       "Initializing...");
    do {
        gwy_data_field_croscorrelate_iteration(dfield1, dfield2, dfieldx,
                                               dfieldy, score, args->search_x,
                                               args->search_y, args->window_x,
                                               args->window_y, &state,
                                               &iteration);
        gwy_app_wait_set_message("Correlating...");
        if (!gwy_app_wait_set_fraction
            (iteration / (gdouble)(dfield1->xres - (args->search_x) / 2)))
            return FALSE;

    } while (state != GWY_COMP_FINISHED);
    gwy_app_wait_finish();
    /*set right output */

    if (args->result == GWY_CROSCOR_ABS) {
        abs_field(dfieldx, dfieldy);
    }
    else if (args->result == GWY_CROSCOR_Y) {
        gwy_data_field_copy(dfieldy, dfieldx);
    }
    else if (args->result == GWY_CROSCOR_DIR) {
        dir_field(dfieldx, dfieldy);
    }

    /*create score mask if requested */
    if (args->mask) {
        gwy_data_field_threshold(score, args->thresh, 1, 0);
        gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(score));
    }

    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    if (!args->mask)
        g_object_unref(score);
    g_object_unref(dfieldy);
    return TRUE;
}


static const gchar *result_key = "/app/croscor/result";
static const gchar *search_x_key = "/app/croscor/search_x";
static const gchar *search_y_key = "/app/croscor/search_y";
static const gchar *window_x_key = "/app/croscor/window_x";
static const gchar *window_y_key = "/app/croscor/window_y";
static const gchar *rot_pos_key = "/app/croscor/rot_pos";
static const gchar *rot_neg_key = "/app/croscor/rot_neg";
static const gchar *scalar_is1_key = "/app/croscor/is1";
static const gchar *scalar_is2_key = "/app/croscor/is2";

static void
gwy_data_croscor_load_args(GwyContainer *settings,
                         GwyCroscorArgs *args)
{
    gboolean b;

    gwy_container_gis_enum_by_name(settings, result_key, &args->result);
    gwy_container_gis_int32_by_name(settings, search_x_key, &args->search_x);
    gwy_container_gis_int32_by_name(settings, search_y_key, &args->search_y);
    gwy_container_gis_int32_by_name(settings, window_x_key, &args->window_x);
    gwy_container_gis_int32_by_name(settings, window_y_key, &args->window_y);
    gwy_container_gis_double_by_name(settings, rot_pos_key, &args->rot_pos);
    gwy_container_gis_double_by_name(settings, rot_neg_key, &args->rot_neg);
    gwy_container_gis_boolean_by_name(settings, scalar_is1_key, &b);
    if (b)
        args->win1 = NULL;
    gwy_container_gis_boolean_by_name(settings, scalar_is2_key, &b);
    if (b)
        args->win2 = NULL;
}

static void
gwy_data_croscor_save_args(GwyContainer *settings,
                         GwyCroscorArgs *args)
{
    gwy_container_set_enum_by_name(settings, result_key, args->result);
    gwy_container_set_int32_by_name(settings, search_x_key, args->search_x);
    gwy_container_set_int32_by_name(settings, search_y_key, args->search_y);
    gwy_container_set_int32_by_name(settings, window_x_key, args->window_x);
    gwy_container_set_int32_by_name(settings, window_y_key, args->window_y);
    gwy_container_set_double_by_name(settings, rot_pos_key, args->rot_pos);
    gwy_container_set_double_by_name(settings, rot_neg_key, args->rot_neg);
    gwy_container_set_boolean_by_name(settings, scalar_is1_key,
                                      args->win1 == NULL);
    gwy_container_set_boolean_by_name(settings, scalar_is2_key,
                                      args->win2 == NULL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

