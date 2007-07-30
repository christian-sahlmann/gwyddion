/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2007 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/grains.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwygrainvaluemenu.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define DIST_RUN_MODES GWY_RUN_INTERACTIVE
#define STAT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    MIN_RESOLUTION = 4,
    MAX_RESOLUTION = 1024
};

typedef enum {
   GRAIN_QUANTITY_SET_ID,  /* Unused here */
   GRAIN_QUANTITY_SET_POSITION,
   GRAIN_QUANTITY_SET_VALUE,
   GRAIN_QUANTITY_SET_AREA,
   GRAIN_QUANTITY_SET_BOUNDARY,
   GRAIN_QUANTITY_SET_VOLUME,
   GRAIN_QUANTITY_SET_SLOPE,
   GRAIN_QUANTITY_NSETS
} GrainQuantitySet;

typedef enum {
    MODE_GRAPH,
    MODE_RAW
} GrainDistMode;

typedef struct {
    GwyGrainQuantity quantity;
    GrainQuantitySet set;
    const gchar *label;
    const gchar *symbol;
    const gchar *identifier;
    const gchar *gtitle;
    const gchar *cdesc;
} QuantityInfo;

typedef struct {
    GrainDistMode mode;
    const gchar *selected;
    gint expanded;
    gboolean fixres;
    gint resolution;

    /* To mask impossible quantitities without really resetting the bits */
    gboolean units_equal;
    guint bitmask;
} GrainDistArgs;

typedef struct {
    GrainDistArgs *args;
    GtkWidget *values;
    GSList *mode;
    GtkWidget *fixres;
    GtkObject *resolution;
    GtkWidget *ok;
} GrainDistControls;

typedef struct {
    GrainDistArgs *args;
    gchar **names;
    GwyDataField *dfield;
    gint ngrains;
    gint *grains;
} GrainDistExportData;

static gboolean module_register                 (void);
static void grain_dist                          (GwyContainer *data,
                                                 GwyRunType run);
static void grain_stat                          (GwyContainer *data,
                                                 GwyRunType run);
static void grain_dist_dialog                   (GrainDistArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GwyDataField *mfield);
static void mode_changed_cb                     (GObject *unused,
                                                 GrainDistControls *controls);
static void selected_changed_cb                 (GrainDistControls *controls);
static void grain_dist_dialog_update_values     (GrainDistControls *controls,
                                                 GrainDistArgs *args);
static void grain_dist_dialog_update_sensitivity(GrainDistControls *controls,
                                                 GrainDistArgs *args);
static void grain_dist_run                      (GrainDistArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GwyDataField *mfield);
static gchar* grain_dist_export_create          (gpointer user_data,
                                                 gssize *data_len);
static void grain_dist_load_args                (GwyContainer *container,
                                                 GrainDistArgs *args);
static void grain_dist_save_args                (GwyContainer *container,
                                                 GrainDistArgs *args);

static const GrainDistArgs grain_dist_defaults = {
    MODE_GRAPH,
    "Equivalent disc radius",
    0,
    FALSE,
    120,
    FALSE,
    0xffffffffU,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Evaluates distribution of grains (continuous parts of mask)."),
    "Petr Klapetek <petr@klapetek.cz>, Sven Neumann <neumann@jpk.com>, "
        "Yeti <yeti@gwyddion.net>",
    "3.2",
    "David Nečas (Yeti) & Petr Klapetek & Sven Neumann",
    "2003-2007",
};

static const gchar fixres_key[]     = "/module/grain_dist/fixres";
static const gchar mode_key[]       = "/module/grain_dist/mode";
static const gchar resolution_key[] = "/module/grain_dist/resolution";
static const gchar selected_key[]   = "/module/grain_dist/selected";
static const gchar expanded_key[]   = "/module/grain_dist/expanded";

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("grain_dist",
                              (GwyProcessFunc)&grain_dist,
                              N_("/_Grains/_Distributions..."),
                              GWY_STOCK_GRAINS_GRAPH,
                              DIST_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Distributions of various grain "
                                 "characteristics"));
    gwy_process_func_register("grain_stat",
                              (GwyProcessFunc)&grain_stat,
                              N_("/_Grains/S_tatistics..."),
                              NULL,
                              STAT_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Simple grain statistics"));

    return TRUE;
}

static void
grain_dist(GwyContainer *data, GwyRunType run)
{
    GwySIUnit *siunitxy, *siunitz;
    GrainDistArgs args;
    GwyDataField *dfield;
    GwyDataField *mfield;

    g_return_if_fail(run & DIST_RUN_MODES);
    grain_dist_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield && mfield);

    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    siunitz = gwy_data_field_get_si_unit_z(dfield);
    args.units_equal = gwy_si_unit_equal(siunitxy, siunitz);
    args.bitmask = 0xffffffffU;
    /* FIXME: Do this generically with gwy_grain_quantity_needs_same_units() */
    if (!args.units_equal)
        args.bitmask ^= ((1 << GWY_GRAIN_VALUE_SURFACE_AREA)
                         | (1 << GWY_GRAIN_VALUE_SLOPE_THETA));

    if (run == GWY_RUN_IMMEDIATE)
        grain_dist_run(&args, data, dfield, mfield);
    else {
        grain_dist_dialog(&args, data, dfield, mfield);
        grain_dist_save_args(gwy_app_settings_get(), &args);
    }
}

static void
grain_dist_dialog(GrainDistArgs *args,
                  GwyContainer *data,
                  GwyDataField *dfield,
                  GwyDataField *mfield)
{
    static const GwyEnum modes[] = {
        { N_("_Export raw data"), MODE_RAW,   },
        { N_("Plot _graphs"),     MODE_GRAPH, },
    };

    GrainDistControls controls;
    GtkWidget *dialog;
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GtkTable *table;
    gint row, response;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Grain Distributions"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    controls.ok = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                        GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 480);

    controls.values = gwy_grain_value_tree_view_new(FALSE,
                                                    "name",
                                                    "symbol_markup",
                                                    "enabled",
                                                    NULL);
    treeview = GTK_TREE_VIEW(controls.values);
    gtk_tree_view_set_headers_visible(treeview, FALSE);
    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);
    gwy_grain_value_tree_view_set_same_units(treeview, args->units_equal);
    gwy_grain_value_tree_view_set_expanded_groups(treeview, args->expanded);
    if (args->selected) {
        gchar **names;

        names = g_strsplit(args->selected, "\n", 0);
        gwy_grain_value_tree_view_set_enabled(treeview, names);
        g_strfreev(names);
    }
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), controls.values,
                       TRUE, TRUE, 0);
    g_signal_connect_swapped(treeview, "row-changed",
                             G_CALLBACK(selected_changed_cb), &controls);

    /* Options */
    table = GTK_TABLE(gtk_table_new(4, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);
    row = 0;

    controls.mode = gwy_radio_buttons_create(modes, G_N_ELEMENTS(modes),
                                             G_CALLBACK(mode_changed_cb),
                                             &controls,
                                             args->mode);

    gtk_table_attach(table, gwy_label_new_header(_("Options")),
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    row = gwy_radio_buttons_attach_to_table(controls.mode, table, 4, row);

    controls.resolution = gtk_adjustment_new(args->resolution,
                                             MIN_RESOLUTION, MAX_RESOLUTION,
                                             1, 10, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Fix res.:"), NULL,
                            controls.resolution, GWY_HSCALE_CHECK);
    controls.fixres = gwy_table_hscale_get_check(controls.resolution);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.fixres),
                                 args->fixres);

    gtk_widget_show_all(dialog);
    grain_dist_dialog_update_sensitivity(&controls, args);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            grain_dist_dialog_update_values(&controls, args);
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

    grain_dist_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);

    grain_dist_run(args, data, dfield, mfield);
}

static void
mode_changed_cb(G_GNUC_UNUSED GObject *unused,
                GrainDistControls *controls)
{
    grain_dist_dialog_update_values(controls, controls->args);
    grain_dist_dialog_update_sensitivity(controls, controls->args);
}

static void
selected_changed_cb(GrainDistControls *controls)
{
    grain_dist_dialog_update_values(controls, controls->args);
    grain_dist_dialog_update_sensitivity(controls, controls->args);
}

static void
grain_dist_dialog_update_values(GrainDistControls *controls,
                                GrainDistArgs *args)
{
    GwyContainer *settings;
    GtkTreeView *treeview;
    const gchar **names;
    gchar *s;

    settings = gwy_app_settings_get();
    treeview = GTK_TREE_VIEW(controls->values);
    names = gwy_grain_value_tree_view_get_enabled(treeview);
    s = g_strjoinv("\n", (gchar**)names);
    g_free(names);
    gwy_container_set_string_by_name(settings, selected_key, s);
    /* Ensures args->selected is never owned by us. */
    args->selected = gwy_container_get_string_by_name(settings, selected_key);

    args->mode = gwy_radio_buttons_get_current(controls->mode);
    args->resolution = gwy_adjustment_get_int(controls->resolution);
    args->fixres
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->fixres));
}

static void
grain_dist_dialog_update_sensitivity(GrainDistControls *controls,
                                     GrainDistArgs *args)
{
    GtkTreeView *treeview;
    GtkWidget *check, *w;

    check = gwy_table_hscale_get_check(controls->resolution);
    switch (args->mode) {
        case MODE_GRAPH:
        gtk_widget_set_sensitive(check, TRUE);
        gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(check));
        gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(check));
        break;

        case MODE_RAW:
        gtk_widget_set_sensitive(check, FALSE);
        w = gwy_table_hscale_get_scale(controls->resolution);
        gtk_widget_set_sensitive(w, FALSE);
        w = gwy_table_hscale_get_middle_widget(controls->resolution);
        gtk_widget_set_sensitive(w, FALSE);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    treeview = GTK_TREE_VIEW(controls->values);
    gtk_widget_set_sensitive(controls->ok,
                             gwy_grain_value_tree_view_n_enabled(treeview));
}

static void
add_one_distribution(GwyContainer *container,
                     GwyDataField *dfield,
                     gint ngrains,
                     const gint *grains,
                     GwyGrainValue *gvalue,
                     const gchar *name,
                     gint resolution)
{
    GwyGraphCurveModel *cmodel;
    GwyGraphModel *gmodel;
    GwyDataLine *dataline;

    /* TODO
    dataline = gwy_data_field_grains_get_distribution(dfield, NULL, NULL,
                                                      ngrains, grains, quantity,
                                                      resolution);
                                                      */
    gmodel = gwy_graph_model_new();
    cmodel = gwy_graph_curve_model_new();
    gwy_graph_model_add_curve(gmodel, cmodel);
    g_object_unref(cmodel);

    name = gettext(name);
    g_object_set(gmodel,
                 "title", name,
                 "axis-label-left", gwy_sgettext("noun|count"),
                 "axis-label-bottom", gwy_grain_value_get_symbol_markup(gvalue),
                 NULL);
    gwy_graph_model_set_units_from_data_line(gmodel, dataline);
    g_object_set(cmodel, "description", name, NULL);
    gwy_graph_curve_model_set_data_from_dataline(cmodel, dataline, 0, 0);
    g_object_unref(dataline);

    gwy_app_data_browser_add_graph_model(gmodel, container, TRUE);
    g_object_unref(gmodel);
}

static void
grain_dist_run(GrainDistArgs *args,
               GwyContainer *data,
               GwyDataField *dfield,
               GwyDataField *mfield)
{
    GrainDistExportData expdata;
    GwyGrainValue *gvalue;
    gchar **names;
    gint *grains;
    guint i;
    gint res, ngrains;

    res = gwy_data_field_get_xres(mfield)*gwy_data_field_get_yres(mfield);
    grains = g_new0(gint, res);
    ngrains = gwy_data_field_number_grains(mfield, grains);

    names = g_strsplit(args->selected, "\n", 0);

    switch (args->mode) {
        case MODE_GRAPH:
        res = args->fixres ? args->resolution : 0;
        for (i = 0; names[i]; i++) {
            gvalue = gwy_grain_values_get_grain_value(names[i]);
            if (!gvalue)
                continue;

            add_one_distribution(data, dfield, ngrains, grains,
                                 gvalue, names[i], res);
        }
        break;

        case MODE_RAW:
        expdata.args = args;
        expdata.names = names;
        expdata.dfield = dfield;
        expdata.ngrains = ngrains;
        expdata.grains = grains;
        gwy_save_auxiliary_with_callback(_("Export Raw Grain Values"), NULL,
                                         grain_dist_export_create,
                                         (GwySaveAuxiliaryDestroy)g_free,
                                         &expdata);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    g_strfreev(names);
    g_free(grains);
}

static gchar*
grain_dist_export_create(gpointer user_data,
                         gssize *data_len)
{
    const GrainDistExportData *expdata = (const GrainDistExportData*)user_data;
    GString *report;
    gchar buffer[32];
    gint gno;
    guint i, nvalues;
    GwyGrainValue **gvalues;
    GwyGrainValue *gvalue;
    gdouble **results;
    gdouble *all_results;
    gchar *retval;

    nvalues = g_strv_length(expdata->names);
    gvalues = g_new(GwyGrainValue*, nvalues);
    for (nvalues = i = 0; expdata->names[i]; i++) {
        gvalue = gwy_grain_values_get_grain_value(expdata->names[nvalues]);
        if (gvalue) {
            gvalues[nvalues] = gvalue;
            nvalues++;
        }
    }

    all_results = g_new(gdouble, nvalues*(expdata->ngrains + 1));
    results = g_new(gdouble*, nvalues);
    for (i = 0; i < nvalues; i++)
        results[i] = all_results + i*(expdata->ngrains + 1);

    gwy_grain_values_calculate(nvalues, gvalues, results,
                               expdata->dfield,
                               expdata->ngrains, expdata->grains);
    g_free(gvalues);

    report = g_string_sized_new(12*expdata->ngrains*nvalues);
    for (gno = 1; gno <= expdata->ngrains; gno++) {
        for (i = 0; i < nvalues; i++) {
            g_ascii_formatd(buffer, sizeof(buffer), "%g", results[i][gno]);
            g_string_append(report, buffer);
            g_string_append_c(report, i == nvalues-1 ? '\n' : '\t');
        }
    }

    g_free(all_results);
    g_free(results);

    retval = report->str;
    g_string_free(report, FALSE);
    *data_len = -1;

    return retval;
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
               const gchar *value)
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
}

static void
grain_stat(G_GNUC_UNUSED GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog, *table;
    GwyDataField *dfield, *mfield;
    GwySIUnit *siunit, *siunit2;
    GwySIValueFormat *vf;
    gint xres, yres, ngrains;
    gdouble total_area, area, size, vol_0, vol_min, vol_laplace, v;
    gdouble *values = NULL;
    gint *grains;
    GString *str;
    gint row;

    g_return_if_fail(run & STAT_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield);
    g_return_if_fail(mfield);

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
    g_free(values);
    g_free(grains);

    dialog = gtk_dialog_new_with_buttons(_("Grain Statistics"), NULL, 0,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    table = gtk_table_new(7, 2, FALSE);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    row = 0;
    str = g_string_new(NULL);

    g_string_printf(str, "%d", ngrains);
    add_report_row(GTK_TABLE(table), &row, _("Number of grains:"), str->str);

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    siunit2 = gwy_si_unit_power(siunit, 2, NULL);

    v = area;
    vf = gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, NULL);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Total projected area (abs.):"),
                   str->str);

    g_string_printf(str, "%.2f %%", 100.0*area/total_area);
    add_report_row(GTK_TABLE(table), &row, _("Total projected area (rel.):"),
                   str->str);

    v = area/ngrains;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Mean grain area:"), str->str);

    v = size/ngrains;
    gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Mean grain size:"), str->str);

    siunit = gwy_data_field_get_si_unit_z(dfield);
    gwy_si_unit_multiply(siunit2, siunit, siunit2);

    v = vol_0;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Total grain volume (zero):"),
                   str->str);

    v = vol_min;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Total grain volume (minimum):"),
                   str->str);

    v = vol_laplace;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Total grain volume (laplacian):"),
                   str->str);

    gwy_si_unit_value_format_free(vf);
    g_string_free(str, TRUE);
    g_object_unref(siunit2);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void
grain_dist_sanitize_args(GrainDistArgs *args)
{
    args->fixres = !!args->fixres;
    args->mode = MIN(args->mode, MODE_RAW);
    args->resolution = CLAMP(args->resolution, MIN_RESOLUTION, MAX_RESOLUTION);
}

static void
grain_dist_load_args(GwyContainer *container,
                     GrainDistArgs *args)
{
    *args = grain_dist_defaults;

    gwy_container_gis_boolean_by_name(container, fixres_key, &args->fixres);
    if (gwy_container_value_type_by_name(container, selected_key) != G_TYPE_INT)
        gwy_container_gis_string_by_name(container, selected_key,
                                         (const guchar**)&args->selected);
    gwy_container_gis_int32_by_name(container, expanded_key, &args->expanded);
    gwy_container_gis_int32_by_name(container, resolution_key,
                                    &args->resolution);
    gwy_container_gis_enum_by_name(container, mode_key, &args->mode);
    grain_dist_sanitize_args(args);
}

static void
grain_dist_save_args(GwyContainer *container,
                     GrainDistArgs *args)
{
    gwy_container_set_boolean_by_name(container, fixres_key, args->fixres);
    /* args->selected is updated immediately in settings */
    gwy_container_set_int32_by_name(container, expanded_key, args->expanded);
    gwy_container_set_int32_by_name(container, resolution_key,
                                    args->resolution);
    gwy_container_set_enum_by_name(container, mode_key, args->mode);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
