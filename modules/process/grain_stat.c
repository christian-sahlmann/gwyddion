/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti), Petr Klapetek, Sven Neumann.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, neumann@jpk.com.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/grains.h>
#include <libprocess/linestats.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwygrainvaluemenu.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define STAT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)
#define INSCRIBE_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean       module_register           (void);
static void           grain_stat                (GwyContainer *data,
                                                 GwyRunType run);
static void           grain_inscribe_discs      (GwyContainer *data,
                                                 GwyRunType run);
static void           grain_exscribe_circles    (GwyContainer *data,
                                                 GwyRunType run);
static GtkWidget*     grain_stats_add_aux_button(GtkWidget *hbox,
                                                 const gchar *stock_id,
                                                 const gchar *tooltip);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Displays overall grain statistics."),
    "Petr Klapetek <petr@klapetek.cz>, Sven Neumann <neumann@jpk.com>, "
        "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek & Sven Neumann",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("grain_stat",
                              (GwyProcessFunc)&grain_stat,
                              N_("/_Grains/S_tatistics..."),
                              NULL,
                              STAT_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Simple grain statistics"));
    gwy_process_func_register("grain_inscribe_discs",
                              (GwyProcessFunc)&grain_inscribe_discs,
                              N_("/_Grains/Select _Inscribed Discs"),
                              NULL,
                              INSCRIBE_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Create a selection visualising discs "
                                 "incribed into grains"));
    gwy_process_func_register("grain_exscribe_circles",
                              (GwyProcessFunc)&grain_exscribe_circles,
                              N_("/_Grains/Select _Excscribed Circles"),
                              NULL,
                              INSCRIBE_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Create a selection visualising grain "
                                 "circumcircles"));

    return TRUE;
}

static gdouble
grains_get_total_value(GwyDataField *dfield,
                       gint ngrains,
                       const gint *grains,
                       gdouble **values,
                       GwyGrainQuantity quantity)
{
    gint i;
    gdouble sum;

    *values = gwy_data_field_grains_get_values(dfield, *values, ngrains, grains,
                                               quantity);
    sum = 0.0;
    for (i = 1; i <= ngrains; i++)
        sum += (*values)[i];

    return sum;
}

static void
add_report_row(GtkTable *table,
               gint *row,
               const gchar *name,
               const gchar *value,
               const gchar *plainvalue,
               GPtrArray *report)
{
    GtkWidget *label;

    label = gtk_label_new(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_table_attach(table, label, 0, 1, *row, *row+1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), value);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 1, 2, *row, *row+1, GTK_FILL, 0, 2, 2);
    (*row)++;

    g_ptr_array_add(report, (gpointer)name);
    g_ptr_array_add(report, g_strdup(plainvalue));
}

static void
grain_stat_save(GtkWidget *dialog)
{
    gchar *text = (gchar*)g_object_get_data(G_OBJECT(dialog), "report");

    gwy_save_auxiliary_data(_("Save Grain Statistics"), GTK_WINDOW(dialog),
                            -1, text);
}

static void
grain_stat_copy(GtkWidget *dialog)
{
    GtkClipboard *clipboard;
    GdkDisplay *display;
    gchar *text = (gchar*)g_object_get_data(G_OBJECT(dialog), "report");

    display = gtk_widget_get_display(dialog);
    clipboard = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, text, -1);
}

static void
grain_stat(GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog, *table, *hbox, *button;
    GwyDataField *dfield, *mfield;
    GwySIUnit *siunit, *siunit2;
    GwySIValueFormat *vf, *vf2;
    gint xres, yres, ngrains;
    gdouble total_area, area, size, vol_0, vol_min, vol_laplace, bound_len, v;
    gdouble *values = NULL;
    gint *grains;
    GString *str, *str2;
    GPtrArray *report;
    const guchar *title;
    gchar *key, *value;
    gint row, id;
    guint i, maxlen;

    g_return_if_fail(run & STAT_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);
    g_return_if_fail(mfield);

    report = g_ptr_array_sized_new(20);

    if (gwy_container_gis_string_by_name(data, "/filename", &title)) {
        g_ptr_array_add(report, _("File:"));
        g_ptr_array_add(report, g_strdup(title));
    }

    key = g_strdup_printf("/%d/data/title", id);
    if (gwy_container_gis_string_by_name(data, key, &title)) {
        g_ptr_array_add(report, _("Data channel:"));
        g_ptr_array_add(report, g_strdup(title));
    }
    g_free(key);

    /* Make empty line in the report */
    g_ptr_array_add(report, NULL);
    g_ptr_array_add(report, NULL);

    xres = gwy_data_field_get_xres(mfield);
    yres = gwy_data_field_get_yres(mfield);
    total_area = gwy_data_field_get_xreal(dfield)
                 *gwy_data_field_get_yreal(dfield);

    grains = g_new0(gint, xres*yres);
    ngrains = gwy_data_field_number_grains(mfield, grains);
    area = grains_get_total_value(dfield, ngrains, grains, &values,
                                  GWY_GRAIN_VALUE_PROJECTED_AREA);
    size = grains_get_total_value(dfield, ngrains, grains, &values,
                                  GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE);
    vol_0 = grains_get_total_value(dfield, ngrains, grains, &values,
                                   GWY_GRAIN_VALUE_VOLUME_0);
    vol_min = grains_get_total_value(dfield, ngrains, grains, &values,
                                     GWY_GRAIN_VALUE_VOLUME_MIN);
    vol_laplace = grains_get_total_value(dfield, ngrains, grains, &values,
                                         GWY_GRAIN_VALUE_VOLUME_LAPLACE);
    bound_len = grains_get_total_value(dfield, ngrains, grains, &values,
                                       GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH);
    g_free(values);
    g_free(grains);

    dialog = gtk_dialog_new_with_buttons(_("Grain Statistics"), NULL, 0,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);

    table = gtk_table_new(10, 2, FALSE);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    row = 0;
    str = g_string_new(NULL);
    str2 = g_string_new(NULL);

    g_string_printf(str, "%d", ngrains);
    add_report_row(GTK_TABLE(table), &row, _("Number of grains:"),
                   str->str, str->str, report);

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    siunit2 = gwy_si_unit_power(siunit, 2, NULL);

    v = area;
    vf = gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, NULL);
    vf2 = gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_PLAIN, v, NULL);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    g_string_printf(str2, "%.*f %s",
                    vf2->precision, v/vf2->magnitude, vf2->units);
    add_report_row(GTK_TABLE(table), &row, _("Total projected area (abs.):"),
                   str->str, str2->str, report);

    g_string_printf(str, "%.2f %%", 100.0*area/total_area);
    add_report_row(GTK_TABLE(table), &row, _("Total projected area (rel.):"),
                   str->str, str->str, report);

    v = area/ngrains;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_PLAIN, v, vf2);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    g_string_printf(str2, "%.*f %s",
                    vf2->precision, v/vf2->magnitude, vf2->units);
    add_report_row(GTK_TABLE(table), &row, _("Mean grain area:"),
                   str->str, str2->str, report);

    v = size/ngrains;
    gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_PLAIN, v, vf2);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    g_string_printf(str2, "%.*f %s",
                    vf2->precision, v/vf2->magnitude, vf2->units);
    add_report_row(GTK_TABLE(table), &row, _("Mean grain size:"),
                   str->str, str2->str, report);

    siunit = gwy_data_field_get_si_unit_z(dfield);
    gwy_si_unit_multiply(siunit2, siunit, siunit2);

    v = vol_0;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_PLAIN, v, vf2);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    g_string_printf(str2, "%.*f %s",
                    vf2->precision, v/vf2->magnitude, vf2->units);
    add_report_row(GTK_TABLE(table), &row, _("Total grain volume (zero):"),
                   str->str, str2->str, report);

    v = vol_min;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_PLAIN, v, vf2);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    g_string_printf(str2, "%.*f %s",
                    vf2->precision, v/vf2->magnitude, vf2->units);
    add_report_row(GTK_TABLE(table), &row, _("Total grain volume (minimum):"),
                   str->str, str2->str, report);

    v = vol_laplace;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_PLAIN, v, vf2);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    g_string_printf(str2, "%.*f %s",
                    vf2->precision, v/vf2->magnitude, vf2->units);
    add_report_row(GTK_TABLE(table), &row, _("Total grain volume (laplacian):"),
                   str->str, str2->str, report);

    v = bound_len;
    gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_PLAIN, v, vf2);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    g_string_printf(str2, "%.*f %s",
                    vf2->precision, v/vf2->magnitude, vf2->units);
    add_report_row(GTK_TABLE(table), &row, _("Total projected boundary length:"),
                   str->str, str2->str, report);

    gwy_si_unit_value_format_free(vf2);
    gwy_si_unit_value_format_free(vf);
    g_object_unref(siunit2);

    maxlen = 0;
    for (i = 0; i < report->len/2; i++) {
        key = (gchar*)g_ptr_array_index(report, 2*i);
        if (key)
            maxlen = MAX(strlen(key), maxlen);
    }

    g_string_truncate(str, 0);
    g_string_append(str, _("Grain Statistics"));
    g_string_append(str, "\n\n");

    for (i = 0; i < report->len/2; i++) {
        key = (gchar*)g_ptr_array_index(report, 2*i);
        if (key) {
            value = (gchar*)g_ptr_array_index(report, 2*i + 1);
            g_string_append_printf(str, "%-*s %s\n", maxlen+1, key, value);
            g_free(value);
        }
        else
            g_string_append_c(str, '\n');
    }
    g_ptr_array_free(report, TRUE);
    g_object_set_data(G_OBJECT(dialog), "report", str->str);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    button = grain_stats_add_aux_button(hbox, GTK_STOCK_SAVE,
                                        _("Save table to a file"));
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(grain_stat_save), dialog);

    button = grain_stats_add_aux_button(hbox, GTK_STOCK_COPY,
                                        _("Copy table to clipboard"));
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(grain_stat_copy), dialog);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    g_string_free(str2, TRUE);
    g_string_free(str, TRUE);
}

static GtkWidget*
grain_stats_add_aux_button(GtkWidget *hbox,
                           const gchar *stock_id,
                           const gchar *tooltip)
{
    GtkTooltips *tips;
    GtkWidget *button;

    tips = gwy_app_get_tooltips();
    button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_tooltips_set_tip(tips, button, tooltip, NULL);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(stock_id,
                                               GTK_ICON_SIZE_SMALL_TOOLBAR));
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    return button;
}

static GwySelection*
create_selection(const gchar *typename,
                 guint *ngrains)
{
    GParamSpecInt *pspec;
    GObjectClass *klass;
    GType type;

    type = g_type_from_name(typename);
    g_return_val_if_fail(type, NULL);

    klass = g_type_class_ref(type);
    pspec = (GParamSpecInt*)g_object_class_find_property(klass, "max-objects");
    g_return_val_if_fail(G_IS_PARAM_SPEC_UINT(pspec), NULL);

    if ((gint)*ngrains > pspec->maximum) {
        g_warning("Too many grains for %s, only first %d will be shown.",
                  typename, pspec->maximum);
        *ngrains = pspec->maximum;
    }
    return g_object_new(type, "max-objects", *ngrains, NULL);
}

/* FIXME: It would be nice to have something like that also for minimum and
 * maximum bounding dimensions. */
static void
grain_inscribe_discs(GwyContainer *data, GwyRunType run)
{
    static const GwyGrainQuantity quantities[] = {
        GWY_GRAIN_VALUE_INSCRIBED_DISC_R,
        GWY_GRAIN_VALUE_INSCRIBED_DISC_X,
        GWY_GRAIN_VALUE_INSCRIBED_DISC_Y,
    };

    GwyDataField *dfield, *mfield;
    GwySelection *selection;
    guint ngrains, i;
    gint *grains;
    gdouble *inscd;
    gdouble *values[3];
    gchar *key;
    gint id;

    g_return_if_fail(run & INSCRIBE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    grains = g_new0(gint, mfield->xres * mfield->yres);
    ngrains = gwy_data_field_number_grains(mfield, grains);
    inscd = g_new(gdouble, 3*(ngrains + 1));
    for (i = 0; i < 3; i++)
        values[i] = inscd + i*(ngrains + 1);

    gwy_data_field_grains_get_quantities(dfield, values, quantities, 3,
                                         ngrains, grains);

    selection = create_selection("GwySelectionEllipse", &ngrains);
    for (i = 1; i <= ngrains; i++) {
        gdouble r = values[0][i], x = values[1][i], y = values[2][i];
        gdouble xy[4] = { x - r, y - r, x + r, y + r };
        gwy_selection_set_object(selection, i-1, xy);
    }

    key = g_strdup_printf("/%d/select/ellipse", id);
    gwy_container_set_object_by_name(data, key, selection);
    g_object_unref(selection);

    g_free(grains);
    g_free(inscd);
}

static void
grain_exscribe_circles(GwyContainer *data, GwyRunType run)
{
    static const GwyGrainQuantity quantities[] = {
        GWY_GRAIN_VALUE_CIRCUMCIRCLE_R,
        GWY_GRAIN_VALUE_CIRCUMCIRCLE_X,
        GWY_GRAIN_VALUE_CIRCUMCIRCLE_Y,
    };

    GwyDataField *dfield, *mfield;
    GwySelection *selection;
    guint ngrains, i;
    gint *grains;
    gdouble *circc;
    gdouble *values[3];
    gchar *key;
    gint id;

    g_return_if_fail(run & INSCRIBE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    grains = g_new0(gint, mfield->xres * mfield->yres);
    ngrains = gwy_data_field_number_grains(mfield, grains);
    circc = g_new(gdouble, 3*(ngrains + 1));
    for (i = 0; i < 3; i++)
        values[i] = circc + i*(ngrains + 1);

    gwy_data_field_grains_get_quantities(dfield, values, quantities, 3,
                                         ngrains, grains);

    selection = create_selection("GwySelectionEllipse", &ngrains);
    for (i = 1; i <= ngrains; i++) {
        gdouble r = values[0][i], x = values[1][i], y = values[2][i];
        gdouble xy[4] = { x - r, y - r, x + r, y + r };
        gwy_selection_set_object(selection, i-1, xy);
    }

    key = g_strdup_printf("/%d/select/ellipse", id);
    gwy_container_set_object_by_name(data, key, selection);
    g_object_unref(selection);

    g_free(grains);
    g_free(circc);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
