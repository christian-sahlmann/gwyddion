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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/filters.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define PRESENTATIONOPS_RUN_MODES GWY_RUN_IMMEDIATE

#define PRESENTATION_ATTACH_RUN_MODES GWY_RUN_INTERACTIVE

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

static gboolean module_register           (void);
static void     presentation_remove       (GwyContainer *data,
                                           GwyRunType run);
static void     presentation_extract      (GwyContainer *data,
                                           GwyRunType run);
static void     presentation_logscale     (GwyContainer *data,
                                           GwyRunType run);
static void     presentation_attach       (GwyContainer *data,
                                           GwyRunType run);
static void     presentation_attach_do    (const GwyDataObjectId *source,
                                           const GwyDataObjectId *target);
static gboolean presentation_attach_filter(GwyContainer *source,
                                           gint id,
                                           gpointer user_data);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Basic operations with presentation: extraction, removal."),
    "Yeti <yeti@gwyddion.net>",
    "1.7",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("presentation_remove",
                              (GwyProcessFunc)&presentation_remove,
                              N_("/_Presentation/_Remove Presentation"),
                              NULL,
                              PRESENTATIONOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA_SHOW | GWY_MENU_FLAG_DATA,
                              N_("Remove presentation from data"));
    gwy_process_func_register("presentation_extract",
                              (GwyProcessFunc)&presentation_extract,
                              N_("/_Presentation/E_xtract Presentation"),
                              NULL,
                              PRESENTATIONOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA_SHOW | GWY_MENU_FLAG_DATA,
                              N_("Extract presentation to a new channel"));
    /* XXX: not really ported */
    gwy_process_func_register("presentation_attach",
                              (GwyProcessFunc)&presentation_attach,
                              N_("/_Presentation/_Attach Presentation..."),
                              NULL,
                              PRESENTATION_ATTACH_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Attaches another data field as "
                                 "presentation"));
    gwy_process_func_register("presentation_logscale",
                              (GwyProcessFunc)&presentation_logscale,
                              N_("/_Presentation/_Logscale"),
                              NULL,
                              PRESENTATIONOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Creates a presentation with logarithmic "
                                 "color scale"));

    return TRUE;
}

static void
presentation_remove(GwyContainer *data, GwyRunType run)
{
    GQuark quark;

    g_return_if_fail(run & PRESENTATIONOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_SHOW_FIELD_KEY, &quark, 0);
    g_return_if_fail(quark);
    gwy_app_undo_qcheckpointv(data, 1, &quark);
    gwy_container_remove(data, quark);
}

static void
presentation_extract(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GQuark quark;
    gint oldid, newid;

    g_return_if_fail(run & PRESENTATIONOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &oldid,
                                     GWY_APP_SHOW_FIELD_KEY, &quark,
                                     GWY_APP_SHOW_FIELD, &dfield,
                                     0);
    g_return_if_fail(dfield && quark);

    dfield = gwy_data_field_duplicate(dfield);
    newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
    g_object_unref(dfield);
    gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            0);
    gwy_app_set_data_field_title(data, newid, NULL);
}

static void
presentation_logscale(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *sfield;
    GQuark squark;
    gdouble *d;
    gdouble min, max, m0;
    gint xres, yres, i, zeroes;

    g_return_if_fail(run & PRESENTATIONOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     GWY_APP_SHOW_FIELD, &sfield,
                                     0);
    g_return_if_fail(dfield && squark);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_app_undo_qcheckpointv(data, 1, &squark);
    if (!sfield) {
        sfield = gwy_data_field_duplicate(dfield);
        gwy_container_set_object(data, squark, sfield);
        g_object_unref(sfield);
    }
    else {
        gwy_data_field_resample(sfield, xres, yres, GWY_INTERPOLATION_NONE);
        gwy_data_field_copy(dfield, sfield, FALSE);
    }

    d = gwy_data_field_get_data(sfield);
    zeroes = 0;
    max = 0;
    min = G_MAXDOUBLE;
    for (i = 0; i < xres*yres; i++) {
        d[i] = ABS(d[i]);
        if (G_UNLIKELY(d[i] > max))
            max = d[i];
        if (d[i] == 0.0)
            zeroes++;
        else if (G_UNLIKELY(d[i] < min))
            min = d[i];
    }
    if (min == max || zeroes == xres*yres)
        return;

    if (!zeroes) {
        for (i = 0; i < xres*yres; i++)
            d[i] = log(d[i]);
    }
    else {
        m0 = log(min) - log(max/min)/512.0;
        for (i = 0; i < xres*yres; i++)
            d[i] = d[i] ? log(d[i]) : m0;
    }

    gwy_data_field_data_changed(sfield);
}

static void
presentation_attach(G_GNUC_UNUSED GwyContainer *data,
                    GwyRunType run)
{
    GtkWidget *dialog, *table, *label, *chooser;
    GwyDataObjectId source, target;
    gint row, response;

    g_return_if_fail(run & PRESENTATION_ATTACH_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &target.data,
                                     GWY_APP_DATA_FIELD_ID, &target.id,
                                     0);

    dialog = gtk_dialog_new_with_buttons(_("Attach Presentation"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(1, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_Data to attach:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    chooser = gwy_data_chooser_new_channels();
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
                                &presentation_attach_filter, &target, NULL);
    gwy_data_chooser_set_active(GWY_DATA_CHOOSER(chooser),
                                target.data, target.id);
    gtk_table_attach(GTK_TABLE(table), chooser, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), chooser);
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
            return;
            break;

            case GTK_RESPONSE_OK:
            source.data = gwy_data_chooser_get_active(GWY_DATA_CHOOSER(chooser),
                                                      &source.id);
            /* The data must be always compatible at least with itself */
            g_assert(source.data);
            presentation_attach_do(&source, &target);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
}

static gboolean
presentation_attach_filter(GwyContainer *source,
                           gint id,
                           gpointer user_data)
{
    const GwyDataObjectId *target = (const GwyDataObjectId*)user_data;
    GwyDataField *source_dfield, *target_dfield;
    gdouble xreal1, xreal2, yreal1, yreal2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    source_dfield = GWY_DATA_FIELD(gwy_container_get_object(source, quark));

    quark = gwy_app_get_data_key_for_id(target->id);
    target_dfield = GWY_DATA_FIELD(gwy_container_get_object(target->data,
                                                            quark));

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
presentation_attach_do(const GwyDataObjectId *source,
                       const GwyDataObjectId *target)
{
    GwyDataField *dfield;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(source->id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(source->data, quark));
    dfield = gwy_data_field_duplicate(dfield);
    quark = gwy_app_get_show_key_for_id(target->id);
    gwy_app_undo_qcheckpointv(target->data, 1, &quark);
    gwy_container_set_object(target->data, quark, dfield);
    g_object_unref(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
