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
#include <libprocess/tip.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#define NANOINDENT_ADJUST_RUN_MODES \
    (GWY_RUN_MODAL)


typedef struct {
    GwyDataWindow *win1;
    GwyDataWindow *win2;
    GwyInterpolationType interp;
    gboolean expand;
    gboolean resample;
    gboolean rotate;
    gboolean move;
} NanoindentAdjustArgs;

typedef struct {
    GtkWidget *interp;
    GtkWidget *expand;
    GtkWidget *resample;
    GtkWidget *rotate;
    GtkWidget *move;
} NanoindentAdjustControls;

static gboolean   module_register               (const gchar *name);
static gboolean   nanoindent_adjust                  (GwyContainer *data,
                                                 GwyRunType run);
static GtkWidget* nanoindent_adjust_window_construct (NanoindentAdjustArgs *args);
static void       nanoindent_adjust_data_cb          (GtkWidget *item);
static gboolean   nanoindent_adjust_check            (NanoindentAdjustArgs *args,
                                                 GtkWidget *nanoindent_adjust_window);
static gboolean   nanoindent_adjust_do               (NanoindentAdjustArgs *args);
static GtkWidget* nanoindent_adjust_data_option_menu (GwyDataWindow **operand);

static void        expand_changed_cb          (GtkWidget *toggle,
                                               NanoindentAdjustControls *controls);
static void        move_changed_cb            (GtkWidget *toggle,
                                               NanoindentAdjustControls *controls);
static void        rotate_changed_cb          (GtkWidget *toggle,
                                               NanoindentAdjustControls *controls);

static void        interp_changed_cb          (GObject *item,
                                               NanoindentAdjustControls *controls);


static GwyDataField *gwy_nanoindent_adjust           (GwyDataField *model, 
                                                      GwyDataField *sample, 
                                                      GwyDataField *result,
                                                      GwySetFractionFunc set_fraction,
                                                      GwySetMessageFunc set_message);

static const NanoindentAdjustArgs nanoindent_adjust_defaults = {
    NULL, NULL, 0, TRUE, TRUE, FALSE, TRUE
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "nanoindent_adjust",
    N_("Adjust images of two indentor prints."),
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
    static GwyProcessFuncInfo nanoindent_adjust_func_info = {
        "nanoindent_adjust",
        N_("/_Nanoindentation/_Adjust..."),
        (GwyProcessFunc)&nanoindent_adjust,
        NANOINDENT_ADJUST_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &nanoindent_adjust_func_info);

    return TRUE;
}

/* FIXME: we ignore the Container argument and use current data window */
static gboolean
nanoindent_adjust(GwyContainer *data, GwyRunType run)
{
    GtkWidget *nanoindent_adjust_window;
    NanoindentAdjustArgs args;
    gboolean ok = FALSE;

    g_return_val_if_fail(run & NANOINDENT_ADJUST_RUN_MODES, FALSE);
    args.win1 = args.win2 = gwy_app_data_window_get_current();
    g_assert(gwy_data_window_get_data(args.win1) == data);
    nanoindent_adjust_window = nanoindent_adjust_window_construct(&args);
    gtk_window_present(GTK_WINDOW(nanoindent_adjust_window));

    do {
        switch (gtk_dialog_run(GTK_DIALOG(nanoindent_adjust_window))) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(nanoindent_adjust_window);
            ok = TRUE;
            break;

            case GTK_RESPONSE_OK:
            ok = nanoindent_adjust_check(&args, nanoindent_adjust_window);
            if (ok) {
                gtk_widget_destroy(nanoindent_adjust_window);
                nanoindent_adjust_do(&args);
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
nanoindent_adjust_window_construct(NanoindentAdjustArgs *args)
{
    GtkWidget *dialog, *table, *omenu, *label;
    gint row;

    NanoindentAdjustControls controls;
    
    dialog = gtk_dialog_new_with_buttons(_("Imprints adjustment"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(2, 10, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /***** First operand *****/
    label = gtk_label_new_with_mnemonic(_("_Indentor model imprint:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = nanoindent_adjust_data_option_menu(&args->win1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    row++;

    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("Imprint to be _adjusted:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = nanoindent_adjust_data_option_menu(&args->win2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    row++;

    controls.move
             = gtk_check_button_new_with_mnemonic(_("_Move data"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.move),
                                 args->move);
    g_signal_connect(controls.move, "toggled",
                     G_CALLBACK(move_changed_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), controls.move, 0, 4, 2, 3,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    
    controls.rotate
             = gtk_check_button_new_with_mnemonic(_("_Rotate data"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.rotate),
                                 args->rotate);
    g_signal_connect(controls.rotate, "toggled",
                     G_CALLBACK(rotate_changed_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), controls.rotate, 0, 4, 3, 4,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
     
    controls.expand
             = gtk_check_button_new_with_mnemonic(_("E_xtrapolate result data out of measured range"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.expand),
                                 args->expand);
    g_signal_connect(controls.expand, "toggled",
                     G_CALLBACK(expand_changed_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), controls.expand, 0, 4, 4, 5,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

   
    controls.interp
             = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                                   &controls, args->interp);
    gwy_table_attach_hscale(table, 5, _("_Interpolation type:"), NULL,
                            GTK_OBJECT(controls.interp), GWY_HSCALE_WIDGET);
         
     
    gtk_widget_show_all(dialog);

    return dialog;
}

static GtkWidget*
nanoindent_adjust_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(nanoindent_adjust_data_cb),
                                        NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}


static void
nanoindent_adjust_data_cb(GtkWidget *item)
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
nanoindent_adjust_check(NanoindentAdjustArgs *args,
               GtkWidget *nanoindent_adjust_window)
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

    if (fabs((dfield1->xreal/dfield1->xres)
             /(dfield2->xreal/dfield2->xres) - 1) > 0.01
        || fabs((dfield1->yreal/dfield1->yres)
                /(dfield2->yreal/dfield2->yres) - 1) > 0.01) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(nanoindent_adjust_window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK,
                                    _("Tip has different range/resolution "
                                      "ratio than image. Tip will be "
                                      "resampled."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
    return TRUE;
}

static gboolean
nanoindent_adjust_do(NanoindentAdjustArgs *args)
{
    GtkWidget *data_window;
    GwyContainer *data;
    GwyDataField *dfield, *dfield1, *dfield2;
    GwyDataWindow *operand1, *operand2;

    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(operand1 != NULL && operand2 != NULL, FALSE);

    data = gwy_data_window_get_data(operand1);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    data = gwy_data_window_get_data(operand2);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    /*result fields - after computation result should be at dfield */
    data = gwy_container_duplicate_by_prefix(data,
                                             "/0/data",
                                             "/0/base/palette",
                                             "/0/select",
                                             NULL);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    gwy_app_wait_start(GTK_WIDGET(args->win2), "Initializing...");
    dfield = gwy_nanoindent_adjust(dfield1, dfield2, dfield,
                              gwy_app_wait_set_fraction,
                              gwy_app_wait_set_message);
    gwy_app_wait_finish();
    /*set right output */

    if (dfield) {
        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    }
    else
        g_object_unref(data);

    return TRUE;
}

static void        
expand_changed_cb(GtkWidget *toggle, NanoindentAdjustControls *controls)
{
}

static void        
move_changed_cb(GtkWidget *toggle, NanoindentAdjustControls *controls)
{
}

static void        
rotate_changed_cb(GtkWidget *toggle, NanoindentAdjustControls *controls)
{
}

static void        
interp_changed_cb(GObject *item, NanoindentAdjustControls *controls)
{
}


static GwyDataField*
gwy_nanoindent_adjust(GwyDataField *model, GwyDataField *sample, GwyDataField *result,
                                GwySetFractionFunc set_fraction,
                                GwySetMessageFunc set_message)
{
    return result;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

