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

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/grains.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define ESETUP_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 320
};

typedef struct {
    gint Hi_I_am_a_dummy_struct_field_preventing_MSVC_from_going_mad_when_it_encounters_an_empty_struct_declaration_Remove_me_when_you_add_some_real_stuff_here;
} EsetupArgs;

typedef struct {
    GtkWidget *view;
    GwyContainer *mydata;
} EsetupControls;

static gboolean    module_register            (void);
static void        esetup                    (GwyContainer *data,
                                               GwyRunType run);
static void        esetup_dialog                (EsetupArgs *args,
                                               GwyContainer *data);
static void        esetup_dialog_update_controls(EsetupControls *controls,
                                               EsetupArgs *args);
static void        esetup_dialog_update_values  (EsetupControls *controls,
                                               EsetupArgs *args);
static void        esetup_load_args             (GwyContainer *container,
                                               EsetupArgs *args);
static void        esetup_save_args             (GwyContainer *container,
                                               EsetupArgs *args);
static void        esetup_sanitize_args         (EsetupArgs *args);


static const EsetupArgs esetup_defaults = {
    0
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Automatic evaluator setup."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.8",  /* FIXME */
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",  /* FIXME */
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("esetup",
                              (GwyProcessFunc)&esetup,
                              N_("/_Evaluator/_Setup..."),
                              GWY_STOCK_GRAINS,
                              ESETUP_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Setup automatic evaluator"));

    return TRUE;
}

static void
esetup(GwyContainer *data, GwyRunType run)
{
    EsetupArgs args;

    g_return_if_fail(run & ESETUP_RUN_MODES);
    esetup_load_args(gwy_app_settings_get(), &args);

    esetup_dialog(&args, data);
    esetup_save_args(gwy_app_settings_get(), &args);
}


static void
esetup_dialog(EsetupArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *label, *hbox;
    EsetupControls controls;
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    GQuark mquark;
    gint row, id;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);
    g_return_if_fail(dfield);

    dialog = gtk_dialog_new_with_buttons(_("Evaluator setup"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_copy_data_items(data, controls.mydata, id, 0,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(10, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Display</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Selected features</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Evaluator tasks</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;


    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            esetup_dialog_update_values(&controls, args);
            g_object_unref(controls.mydata);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    esetup_dialog_update_values(&controls, args);
    gwy_app_copy_data_items(controls.mydata, data, 0, id,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    gtk_widget_destroy(dialog);

}

static void
esetup_dialog_update_controls(EsetupControls *controls,
                            EsetupArgs *args)
{
}

static void
esetup_dialog_update_values(EsetupControls *controls,
                          EsetupArgs *args)
{
}


static void
esetup_sanitize_args(EsetupArgs *args)
{
}

static void
esetup_load_args(GwyContainer *container,
               EsetupArgs *args)
{
    *args = esetup_defaults;
}

static void
esetup_save_args(GwyContainer *container,
               EsetupArgs *args)
{
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
