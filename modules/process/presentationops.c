/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/filters.h>
#include <app/gwyapp.h>

#define PRESENTATIONOPS_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

#define PRESENTATION_ATTACH_RUN_MODES \
    (GWY_RUN_MODAL)

static gboolean module_register           (const gchar *name);
static gboolean presentation_remove       (GwyContainer *data,
                                           GwyRunType run);
static gboolean presentation_extract      (GwyContainer *data,
                                           GwyRunType run);
static gboolean presentation_attach       (GwyContainer *data,
                                           GwyRunType run);
static void     presentation_attach_do    (GwyContainer *source,
                                           GwyContainer *target);
static gboolean presentation_attach_filter(GwyDataWindow *source,
                                           gpointer user_data);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Basic operations with presentation: extraction, removal."),
    "Yeti <yeti@gwyddion.net>",
    "1.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo presentation_remove_func_info = {
        "presentation_remove",
        N_("/_Display/_Remove Presentation"),
        (GwyProcessFunc)&presentation_remove,
        PRESENTATIONOPS_RUN_MODES,
        GWY_MENU_FLAG_DATA_SHOW,
    };
    static GwyProcessFuncInfo presentation_extract_func_info = {
        "presentation_extract",
        N_("/_Display/E_xtract Presentation"),
        (GwyProcessFunc)&presentation_extract,
        PRESENTATIONOPS_RUN_MODES,
        GWY_MENU_FLAG_DATA_SHOW,
    };
    static GwyProcessFuncInfo presentation_attach_func_info = {
        "presentation_attach",
        N_("/_Display/_Attach Presentation..."),
        (GwyProcessFunc)&presentation_attach,
        PRESENTATION_ATTACH_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &presentation_remove_func_info);
    gwy_process_func_register(name, &presentation_extract_func_info);
    gwy_process_func_register(name, &presentation_attach_func_info);

    return TRUE;
}

static gboolean
presentation_remove(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & PRESENTATIONOPS_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/show"));
    g_return_val_if_fail(dfield, FALSE);

    gwy_app_undo_checkpoint(data, "/0/show", NULL);
    gwy_container_remove_by_name(data, "/0/show");

    return TRUE;
}

static gboolean
presentation_extract(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    const guchar *pal = NULL;

    g_return_val_if_fail(run & PRESENTATIONOPS_RUN_MODES, FALSE);
    dfield = gwy_container_get_object_by_name(data, "/0/show");
    g_return_val_if_fail(dfield, FALSE);

    gwy_container_gis_string_by_name(data, "/0/base/palette", &pal);
    dfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_normalize(dfield);

    data = gwy_container_new();
    gwy_container_set_object_by_name(data, "/0/data", dfield);
    g_object_unref(dfield);
    if (pal)
        gwy_container_set_string_by_name(data, "/0/base/palette",
                                         g_strdup(pal));

    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    return FALSE;
}

static gboolean
presentation_attach(GwyContainer *data,
                    GwyRunType run)
{
    GtkWidget *dialog, *table, *label, *omenu;
    GtkWidget *source_menu;
    GwyDataWindow *source;
    gint row, response;

    g_return_val_if_fail(run & PRESENTATION_ATTACH_RUN_MODES, FALSE);
    source = gwy_app_data_window_get_current();

    dialog = gtk_dialog_new_with_buttons(_("Attach Presentation"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(1, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_Data to attach:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = gwy_option_menu_data_window_filtered(NULL, NULL, NULL,
                                                 GTK_WIDGET(source),
                                                 &presentation_attach_filter,
                                                 data);
    gtk_table_attach(GTK_TABLE(table), omenu, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    source_menu = omenu;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            source = GWY_DATA_WINDOW
                        (gwy_option_menu_data_window_get_history(source_menu));
            presentation_attach_do(gwy_data_window_get_data(source), data);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static gboolean
presentation_attach_filter(GwyDataWindow *source,
                           gpointer user_data)
{
    GwyContainer *data;
    GwyDataField *source_dfield, *target_dfield;
    gdouble xreal1, xreal2, yreal1, yreal2;

    data = gwy_data_window_get_data(source);
    source_dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                    "/0/data"));
    data = GWY_CONTAINER(user_data);
    target_dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                    "/0/data"));

    if ((gwy_data_field_get_xres(target_dfield)
         != gwy_data_field_get_xres(source_dfield))
        || (gwy_data_field_get_yres(target_dfield)
            != gwy_data_field_get_yres(source_dfield)))
        return FALSE;

    xreal1 = gwy_data_field_get_xreal(target_dfield);
    yreal1 = gwy_data_field_get_yreal(target_dfield);
    xreal2 = gwy_data_field_get_xreal(source_dfield);
    yreal2 = gwy_data_field_get_yreal(source_dfield);
    if (fabs(log(xreal1/xreal2)) > 0.0001
        || fabs(log(yreal1/yreal2)) > 0.0001)
        return FALSE;

    return TRUE;
}

static void
presentation_attach_do(GwyContainer *source,
                       GwyContainer *target)
{
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(source,
                                                             "/0/data"));
    dfield = gwy_data_field_duplicate(dfield);
    gwy_app_undo_checkpoint(target, "/0/show", NULL);
    gwy_container_set_object_by_name(target, "/0/show", dfield);
    g_object_unref(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
