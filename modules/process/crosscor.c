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

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#define CROSSCOR_RUN_MODES \
    (GWY_RUN_INTERACTIVE)

typedef enum {
    GWY_CROSSCOR_ABS,
    GWY_CROSSCOR_X,
    GWY_CROSSCOR_Y,
    GWY_CROSSCOR_DIR,
    GWY_CROSSCOR_ANG,
    GWY_CROSSCOR_SCORE,
    GWY_CROSSCOR_LAST
} CrosscorResult;

typedef struct {
    CrosscorResult result;
    gint search_x;
    gint search_y;
    gint window_x;
    gint window_y;
    gdouble rot_pos;
    gdouble rot_neg;
    GwyDataWindow *win1;
    GwyDataWindow *win2;
    gboolean add_ls_mask;
    gdouble threshold;
} CrosscorArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *result;
    GtkObject *search_area_x;
    GtkObject *search_area_y;
    GtkObject *window_area_x;
    GtkObject *window_area_y;
    GtkObject *rotation_neg;
    GtkObject *rotation_pos;
    GtkWidget *add_ls_mask;
    GtkObject *threshold;
} CrosscorControls;

static gboolean   module_register             (const gchar *name);
static gboolean   crosscor                    (GwyContainer *data,
                                               GwyRunType run);
static GtkWidget* crosscor_window_construct   (CrosscorArgs *args,
                                               CrosscorControls *controls);
static GtkWidget* crosscor_data_option_menu   (GwyDataWindow **operand);
static void       crosscor_operation_cb       (GtkWidget *item,
                                               CrosscorArgs *args);
static void       crosscor_data_cb            (GtkWidget *item);
static void       crosscor_update_values      (CrosscorControls *controls,
                                               CrosscorArgs *args);
static gboolean   crosscor_check              (CrosscorArgs *args,
                                               GtkWidget *crosscor_window);
static gboolean   crosscor_do                 (CrosscorArgs *args);
static void       crosscor_load_args          (GwyContainer *settings,
                                               CrosscorArgs *args);
static void       crosscor_save_args          (GwyContainer *settings,
                                               CrosscorArgs *args);
static void       crosscor_sanitize_args      (CrosscorArgs *args);


static const GwyEnum results[] = {
    { "Absolute",    GWY_CROSSCOR_ABS },
    { "X distance",  GWY_CROSSCOR_X },
    { "Y distance",  GWY_CROSSCOR_Y },
    { "Angle",       GWY_CROSSCOR_DIR },
};

static const CrosscorArgs crosscor_defaults = {
    GWY_CROSSCOR_ABS, 10, 10, 25, 25, 0.0, 0.0, NULL, NULL, 1, 0.95
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "crosscor",
    "Cross-correlation of two data fields.",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo crosscor_func_info = {
        "crosscor",
        "/M_ultidata/_Cross-correlation",
        (GwyProcessFunc)&crosscor,
        CROSSCOR_RUN_MODES,
    };

    gwy_process_func_register(name, &crosscor_func_info);

    return TRUE;
}

/* FIXME: we ignore the Container argument and use current data window */
static gboolean
crosscor(GwyContainer *data, GwyRunType run)
{
    GtkWidget *crosscor_window;
    CrosscorArgs args;
    CrosscorControls controls;
    GwyContainer *settings;
    gboolean ok = FALSE;

    g_return_val_if_fail(run & CROSSCOR_RUN_MODES, FALSE);
    settings = gwy_app_settings_get();
    crosscor_load_args(settings, &args);
    args.win1 = args.win2 = gwy_app_data_window_get_current();
    g_assert(gwy_data_window_get_data(args.win1) == data);
    crosscor_window = crosscor_window_construct(&args, &controls);
    gtk_window_present(GTK_WINDOW(crosscor_window));

    do {
        switch (gtk_dialog_run(GTK_DIALOG(crosscor_window))) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(crosscor_window);
            ok = TRUE;
            break;

            case GTK_RESPONSE_APPLY:
            crosscor_update_values(&controls, &args);
            ok = crosscor_check(&args, crosscor_window);
            if (ok) {
                gtk_widget_destroy(crosscor_window);
                crosscor_do(&args);
                crosscor_save_args(settings, &args);
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    return FALSE;
}

static GtkWidget*
crosscor_window_construct(CrosscorArgs *args,
                          CrosscorControls *controls)
{
    GtkWidget *dialog, *table, *omenu, *label, *spin;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Data Croscorrelation"),
                                         GTK_WINDOW(gwy_app_main_window_get()),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         NULL);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 8);

    table = gtk_table_new(2, 10, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /***** First operand *****/
    label = gtk_label_new_with_mnemonic(_("_First data field:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = crosscor_data_option_menu(&args->win1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 4);
    row++;

    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("_Second data field:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = crosscor_data_option_menu(&args->win2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /**** Parameters ********/
    /*search size*/
    label = gtk_label_new_with_mnemonic(_("_Search size"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls->search_area_x = gtk_adjustment_new(args->search_x,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, row, _("Width"), _("pixels"),
                                controls->search_area_x);
    row++;

    controls->search_area_y = gtk_adjustment_new(args->search_y,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, row, _("Height"), _("pixels"),
                                controls->search_area_y);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /*window size*/
    label = gtk_label_new_with_mnemonic(_("_Window size"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls->window_area_x = gtk_adjustment_new(args->window_x,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, row, _("Width"), _("pixels"),
                                controls->window_area_x);
    row++;

    controls->window_area_y = gtk_adjustment_new(args->window_y,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, row, _("Height"), _("pixels"),
                                controls->window_area_y);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /*do mask of thresholds*/
    controls->add_ls_mask = gtk_check_button_new_with_mnemonic
                                (_("Add _low score results mask"));
    gtk_table_attach(GTK_TABLE(table), controls->add_ls_mask, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->add_ls_mask),
                                 args->add_ls_mask);
    row++;

    controls->threshold = gtk_adjustment_new(args->threshold,
                                             -1, 1, 0.005, 0.05, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Threshold value"), _(""),
                                       controls->threshold);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    row++;

    /***** Result *****/
    omenu = gwy_option_menu_create(results, G_N_ELEMENTS(results),
                                   "operation",
                                   G_CALLBACK(crosscor_operation_cb),
                                   args,
                                   args->result);
    gwy_table_attach_row(table, row, _("_Result:"), "", omenu);

    gtk_widget_show_all(dialog);

    return dialog;
}

static GtkWidget*
crosscor_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(crosscor_data_cb),
                                        NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}

static void
crosscor_operation_cb(GtkWidget *item, CrosscorArgs *args)
{
    args->result
        = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "operation"));
}

static void
crosscor_data_cb(GtkWidget *item)
{
    GtkWidget *menu;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);

    p = g_object_get_data(G_OBJECT(item), "data-window");
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}

static void
abs_field(GwyDataField *dfieldx, GwyDataField *dfieldy)
{
    gint i;

    for (i = 0; i < (dfieldx->xres * dfieldx->yres); i++) {
        dfieldx->data[i] =
            sqrt(dfieldx->data[i] * dfieldx->data[i] +
                 dfieldy->data[i] * dfieldy->data[i]);
    }
}

static void
dir_field(GwyDataField *dfieldx, GwyDataField *dfieldy)
{
    gint i;

    for (i = 0; i < (dfieldx->xres * dfieldx->yres); i++) {
        dfieldx->data[i] = atan2(dfieldy->data[i], dfieldx->data[i]);
    }
}

static void
crosscor_update_values(CrosscorControls *controls,
                       CrosscorArgs *args)
{
    args->search_x =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->search_area_x));
    args->search_y =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->search_area_y));
    args->window_x =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->window_area_x));
    args->window_y =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->window_area_y));
    args->threshold =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold));
    args->add_ls_mask =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->add_ls_mask));
}

static gboolean
crosscor_check(CrosscorArgs *args,
               GtkWidget *crosscor_window)
{
    GtkWidget *dialog;
    GwyContainer *data;
    GwyDataField *dfield1, *dfield2;
    GwyDataWindow *operand1, *operand2;

    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(operand1)
                         && GWY_IS_DATA_WINDOW(operand2),
                         FALSE);

    data = gwy_data_window_get_data(operand1);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    data = gwy_data_window_get_data(operand2);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    if (dfield1->xres == dfield2->xres && dfield1->yres == dfield2->yres)
        return TRUE;

    dialog = gtk_message_dialog_new(GTK_WINDOW(crosscor_window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE,
                                    _("Both data fields must have same size."));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return FALSE;
}

static gboolean
crosscor_do(CrosscorArgs *args)
{
    GtkWidget *data_window;
    GwyContainer *data;
    GwyDataField *dfieldx, *dfieldy, *dfield1, *dfield2, *score;
    GwyDataWindow *operand1, *operand2;
    gint iteration = 0;
    GwyComputationStateType state;

    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(operand1 != NULL && operand2 != NULL, FALSE);

    data = gwy_data_window_get_data(operand1);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    data = gwy_data_window_get_data(operand2);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

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
                (iteration/(gdouble)(dfield1->xres - (args->search_x)/2)))
            return FALSE;

    } while (state != GWY_COMP_FINISHED);
    gwy_app_wait_finish();
    /*set right output */

    if (args->result == GWY_CROSSCOR_ABS) {
        abs_field(dfieldx, dfieldy);
    }
    else if (args->result == GWY_CROSSCOR_Y) {
        gwy_data_field_copy(dfieldy, dfieldx);
    }
    else if (args->result == GWY_CROSSCOR_DIR) {
        dir_field(dfieldx, dfieldy);
    }

    /*create score mask if requested */
    if (args->add_ls_mask) {
        gwy_data_field_threshold(score, args->threshold, 1, 0);
        gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(score));
    }

    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    if (!args->add_ls_mask)
        g_object_unref(score);
    g_object_unref(dfieldy);
    return TRUE;
}


static const gchar *result_key = "/module/crosscor/result";
static const gchar *search_x_key = "/module/crosscor/search_x";
static const gchar *search_y_key = "/module/crosscor/search_y";
static const gchar *window_x_key = "/module/crosscor/window_x";
static const gchar *window_y_key = "/module/crosscor/window_y";
static const gchar *add_ls_mask_key = "/module/crosscor/add_ls_mask";
static const gchar *threshold_key = "/module/crosscor/threshold";
static const gchar *rot_pos_key = "/module/crosscor/rot_pos";
static const gchar *rot_neg_key = "/module/crosscor/rot_neg";

static void
crosscor_sanitize_args(CrosscorArgs *args)
{
    args->result = MIN(args->result, GWY_CROSSCOR_LAST-1);
    args->search_x = CLAMP(args->search_x, 0, 100);
    args->search_y = CLAMP(args->search_y, 0, 100);
    args->window_x = CLAMP(args->window_x, 0, 100);
    args->window_y = CLAMP(args->window_y, 0, 100);
    args->threshold = CLAMP(args->threshold, -1.0, 1.0);
    args->add_ls_mask = !!args->add_ls_mask;
}

static void
crosscor_load_args(GwyContainer *settings,
                   CrosscorArgs *args)
{
    /* TODO: remove this someday (old keys we used as  */
    gwy_container_remove_by_prefix(settings, "/app/croscor");

    *args = crosscor_defaults;
    gwy_container_gis_enum_by_name(settings, result_key, &args->result);
    gwy_container_gis_int32_by_name(settings, search_x_key, &args->search_x);
    gwy_container_gis_int32_by_name(settings, search_y_key, &args->search_y);
    gwy_container_gis_int32_by_name(settings, window_x_key, &args->window_x);
    gwy_container_gis_int32_by_name(settings, window_y_key, &args->window_y);
    gwy_container_gis_double_by_name(settings, threshold_key, &args->threshold);
    gwy_container_gis_boolean_by_name(settings, add_ls_mask_key,
                                      &args->add_ls_mask);
    gwy_container_gis_double_by_name(settings, rot_pos_key, &args->rot_pos);
    gwy_container_gis_double_by_name(settings, rot_neg_key, &args->rot_neg);
    crosscor_sanitize_args(args);
}

static void
crosscor_save_args(GwyContainer *settings,
                   CrosscorArgs *args)
{
    gwy_container_set_enum_by_name(settings, result_key, args->result);
    gwy_container_set_int32_by_name(settings, search_x_key, args->search_x);
    gwy_container_set_int32_by_name(settings, search_y_key, args->search_y);
    gwy_container_set_int32_by_name(settings, window_x_key, args->window_x);
    gwy_container_set_int32_by_name(settings, window_y_key, args->window_y);
    gwy_container_set_double_by_name(settings, threshold_key, args->threshold);
    gwy_container_set_boolean_by_name(settings, add_ls_mask_key,
                                      args->add_ls_mask);
    gwy_container_set_double_by_name(settings, rot_pos_key, args->rot_pos);
    gwy_container_set_double_by_name(settings, rot_neg_key, args->rot_neg);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

