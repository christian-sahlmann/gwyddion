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

#define TIP_DILATION_RUN_MODES \
    (GWY_RUN_MODAL)


typedef struct {
    GwyDataWindow *win1;
    GwyDataWindow *win2;
} TipDilationArgs;

typedef struct {
    GtkWidget *dialog;
} TipDilationControls;

static gboolean   module_register             (const gchar *name);
static gboolean   tip_dilation                    (GwyContainer *data,
                                               GwyRunType run);
static GtkWidget* tip_dilation_window_construct   (TipDilationArgs *args,
                                               TipDilationControls *controls);
static void       tip_dilation_data_cb            (GtkWidget *item);
static gboolean   tip_dilation_check              (TipDilationArgs *args,
                                               GtkWidget *tip_dilation_window);
static gboolean   tip_dilation_do                 (TipDilationArgs *args);
static void       tip_dilation_load_args          (GwyContainer *settings,
                                               TipDilationArgs *args);
static void       tip_dilation_save_args          (GwyContainer *settings,
                                               TipDilationArgs *args);
static void       tip_dilation_sanitize_args      (TipDilationArgs *args);
static GtkWidget * tip_dilation_data_option_menu(GwyDataWindow **operand);


static const TipDilationArgs tip_dilation_defaults = {
    NULL, NULL,    
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "tip_dilation",
    "Tip Dilation.",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo tip_dilation_func_info = {
        "tip_dilation",
        "/_Tip operations/_Dilation",
        (GwyProcessFunc)&tip_dilation,
        TIP_DILATION_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &tip_dilation_func_info);

    return TRUE;
}

/* FIXME: we ignore the Container argument and use current data window */
static gboolean
tip_dilation(GwyContainer *data, GwyRunType run)
{
    GtkWidget *tip_dilation_window;
    TipDilationArgs args;
    TipDilationControls controls;
    GwyContainer *settings;
    gboolean ok = FALSE;

    g_return_val_if_fail(run & TIP_DILATION_RUN_MODES, FALSE);
    settings = gwy_app_settings_get();
    tip_dilation_load_args(settings, &args);
    args.win1 = args.win2 = gwy_app_data_window_get_current();
    g_assert(gwy_data_window_get_data(args.win1) == data);
    tip_dilation_window = tip_dilation_window_construct(&args, &controls);
    gtk_window_present(GTK_WINDOW(tip_dilation_window));

    do {
        switch (gtk_dialog_run(GTK_DIALOG(tip_dilation_window))) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            tip_dilation_save_args(settings, &args);
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(tip_dilation_window);
            ok = TRUE;
            break;

            case GTK_RESPONSE_OK:
            ok = tip_dilation_check(&args, tip_dilation_window);
            if (ok) {
                gtk_widget_destroy(tip_dilation_window);
                tip_dilation_do(&args);
                tip_dilation_save_args(settings, &args);
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
tip_dilation_window_construct(TipDilationArgs *args,
                          TipDilationControls *controls)
{
    GtkWidget *dialog, *table, *omenu, *label, *spin;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Tip dilation"),
                                         GTK_WINDOW(gwy_app_main_window_get()),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 8);

    table = gtk_table_new(2, 10, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /***** First operand *****/
    label = gtk_label_new_with_mnemonic(_("_Tip morphology:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = tip_dilation_data_option_menu(&args->win1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 4);
    row++;

    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("_Surface to be dilated:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = tip_dilation_data_option_menu(&args->win2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_widget_show_all(dialog);

    return dialog;
}

static GtkWidget*
tip_dilation_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(tip_dilation_data_cb),
                                        NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}


static void
tip_dilation_data_cb(GtkWidget *item)
{
    GtkWidget *menu;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);

    p = g_object_get_data(G_OBJECT(item), "data-window");
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}


static gboolean
tip_dilation_check(TipDilationArgs *args,
               GtkWidget *tip_dilation_window)
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

    if ((dfield1->xreal/dfield1->xres) != (dfield2->xreal/dfield2->xres) 
        || (dfield1->yreal/dfield1->yres) != (dfield2->yreal/dfield2->yres))
    {
        dialog = gtk_message_dialog_new(GTK_WINDOW(tip_dilation_window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE,
                                    _("Tip has different range/resolution ratio than image. Tip will be resampled."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
    return TRUE;
}

static gboolean
tip_dilation_do(TipDilationArgs *args)
{
    GtkWidget *data_window;
    GwyContainer *data;
    GwyDataField *dfield, *dfield1, *dfield2;
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

    /*result fields - after computation result should be at dfield */
    data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));


    iteration = 0;
    state = GWY_COMP_INIT;
    gwy_app_wait_start(GTK_WIDGET(args->win1),
                       "Initializing...");
    do {
        /*iteration*/
        state = GWY_COMP_FINISHED;
        gwy_app_wait_set_message("Dilating...");
        if (!gwy_app_wait_set_fraction
                (iteration/(gdouble)(dfield2->xres)))
        {
            g_object_unref(dfield);
            return FALSE;
        }

    } while (state != GWY_COMP_FINISHED);
    gwy_app_wait_finish();
    /*set right output */

    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    return TRUE;
}


static const gchar *result_key = "/module/tip_dilation/result";

static void
tip_dilation_sanitize_args(TipDilationArgs *args)
{
}

static void
tip_dilation_load_args(GwyContainer *settings,
                   TipDilationArgs *args)
{
    /* TODO: remove this someday (old keys we used as  */
    gwy_container_remove_by_prefix(settings, "/app/croscor");

    *args = tip_dilation_defaults;
    /*
    gwy_container_gis_enum_by_name(settings, result_key, &args->result);
    */
    tip_dilation_sanitize_args(args);
}

static void
tip_dilation_save_args(GwyContainer *settings,
                   TipDilationArgs *args)
{
    /*
    gwy_container_set_enum_by_name(settings, result_key, args->result);
    */
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

