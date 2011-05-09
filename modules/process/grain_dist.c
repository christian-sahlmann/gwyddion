/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2008 David Necas (Yeti), Petr Klapetek.
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
#include <libprocess/linestats.h>
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
    MAX_RESOLUTION = 1024,
    RESPONSE_CLEAR = 2
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
    gboolean add_comment;
    gboolean fixres;
    gint resolution;

    /* To mask impossible quantitities without really resetting the bits */
    gboolean units_equal;
} GrainDistArgs;

typedef struct {
    GrainDistArgs *args;
    GtkWidget *values;
    GSList *mode;
    GtkWidget *add_comment;
    GtkWidget *fixres;
    GtkObject *resolution;
    GtkWidget *ok;
} GrainDistControls;

typedef struct {
    GrainDistArgs *args;
    guint nvalues;
    GwyGrainValue **gvalues;
    GwyDataLine **rawvalues;
    gboolean add_comment;
} GrainDistExportData;

static gboolean module_register                 (void);
static void grain_dist                          (GwyContainer *data,
                                                 GwyRunType run);
static void grain_stat                          (GwyContainer *data,
                                                 GwyRunType run);
static GtkWidget* grain_stats_add_aux_button    (GtkWidget *hbox,
                                                 const gchar *stock_id,
                                                 const gchar *tooltip);
static void grain_dist_dialog                   (GrainDistArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GwyDataField *mfield);
static void mode_changed_cb                     (GtkToggleButton *button,
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
    FALSE,
    120,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Evaluates distribution of grains (continuous parts of mask)."),
    "Petr Klapetek <petr@klapetek.cz>, Sven Neumann <neumann@jpk.com>, "
        "Yeti <yeti@gwyddion.net>",
    "3.8",
    "David Nečas (Yeti) & Petr Klapetek & Sven Neumann",
    "2003",
};

static const gchar fixres_key[]      = "/module/grain_dist/fixres";
static const gchar mode_key[]        = "/module/grain_dist/mode";
static const gchar resolution_key[]  = "/module/grain_dist/resolution";
static const gchar add_comment_key[] = "/module/grain_dist/add_comment";
static const gchar selected_key[]    = "/module/grain_dist/selected";
static const gchar expanded_key[]    = "/module/grain_dist/expanded";

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
    GtkWidget *scwin;
    GtkDialog *dialog;
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTable *table;
    GSList *l;
    gint row, response;

    controls.args = args;

    dialog = GTK_DIALOG(gtk_dialog_new_with_buttons(_("Grain Distributions"),
                                                    NULL, 0,
                                                    GTK_STOCK_CLEAR,
                                                    RESPONSE_CLEAR,
                                                    GTK_STOCK_CANCEL,
                                                    GTK_RESPONSE_CANCEL,
                                                    NULL));
    controls.ok = gtk_dialog_add_button(dialog, GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(dialog, FALSE);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 520);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), scwin, TRUE, TRUE, 0);

    controls.values = gwy_grain_value_tree_view_new(FALSE,
                                                    "name", "enabled", NULL);
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
    gtk_container_add(GTK_CONTAINER(scwin), controls.values);

    model = gtk_tree_view_get_model(treeview);
    g_signal_connect_swapped(model, "row-changed",
                             G_CALLBACK(selected_changed_cb), &controls);

    /* Options */
    table = GTK_TABLE(gtk_table_new(5, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);
    row = 0;

    controls.mode = gwy_radio_buttons_create(modes, G_N_ELEMENTS(modes),
                                             G_CALLBACK(mode_changed_cb),
                                             &controls,
                                             args->mode);

    gtk_table_attach(table, gwy_label_new_header(_("Options")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    l = controls.mode;
    gtk_table_attach(table, GTK_WIDGET(l->data),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    l = g_slist_next(l);
    row++;

    controls.add_comment
        = gtk_check_button_new_with_mnemonic(_("Add _informational "
                                               "comment header"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.add_comment),
                                 args->add_comment);
    gtk_table_attach(table, controls.add_comment,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    gtk_table_attach(table, GTK_WIDGET(l->data),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    l = g_slist_next(l);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    controls.resolution = gtk_adjustment_new(args->resolution,
                                             MIN_RESOLUTION, MAX_RESOLUTION,
                                             1, 10, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Fix res.:"), NULL,
                            controls.resolution, GWY_HSCALE_CHECK);
    controls.fixres = gwy_table_hscale_get_check(controls.resolution);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.fixres),
                                 args->fixres);
    row++;

    gtk_widget_show_all(GTK_WIDGET(dialog));
    grain_dist_dialog_update_sensitivity(&controls, args);

    do {
        response = gtk_dialog_run(dialog);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            grain_dist_dialog_update_values(&controls, args);
            gtk_widget_destroy(GTK_WIDGET(dialog));
            case GTK_RESPONSE_NONE:
            return;
            break;

            case RESPONSE_CLEAR:
            g_signal_handlers_block_by_func(model,
                                            selected_changed_cb, &controls);
            gwy_grain_value_tree_view_set_enabled(treeview, NULL);
            g_signal_handlers_unblock_by_func(model,
                                              selected_changed_cb, &controls);
            selected_changed_cb(&controls);
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    grain_dist_dialog_update_values(&controls, args);
    gtk_widget_destroy(GTK_WIDGET(dialog));

    grain_dist_run(args, data, dfield, mfield);
}

static void
mode_changed_cb(GtkToggleButton *button,
                GrainDistControls *controls)
{
    if (!gtk_toggle_button_get_active(button))
        return;

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
    args->add_comment
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->add_comment));
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
        gtk_widget_set_sensitive(controls->add_comment, FALSE);
        gtk_widget_set_sensitive(check, TRUE);
        gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(check));
        gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(check));
        break;

        case MODE_RAW:
        gtk_widget_set_sensitive(controls->add_comment, TRUE);
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
                     GrainDistExportData *expdata,
                     guint i)
{
    GwyGraphCurveModel *cmodel;
    GwyGraphModel *gmodel;
    GwyDataLine *dline, *distribution;
    GwyGrainValue *gvalue;
    GwySIUnit *xyunit, *zunit, *lineunit;
    gdouble *data;
    gint res, ngrains;
    const gchar *name;

    dline = expdata->rawvalues[i];
    gvalue = expdata->gvalues[i];
    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    zunit = gwy_data_field_get_si_unit_z(dfield);
    lineunit = gwy_data_line_get_si_unit_y(dline);
    gwy_si_unit_power_multiply(xyunit, gwy_grain_value_get_power_xy(gvalue),
                               zunit, gwy_grain_value_get_power_z(gvalue),
                               lineunit);

    res = expdata->args->fixres ? expdata->args->resolution : 0;
    distribution = gwy_data_line_new(res ? res : 1, 1.0, FALSE);
    data = gwy_data_line_get_data(dline);
    ngrains = gwy_data_line_get_res(dline) - 1;
    /* Get rid of the zeroth bogus item corresponding to no grain. */
    data[0] = data[ngrains];
    /* FIXME: Direct access. */
    dline->res = ngrains;
    gwy_data_line_distribution(dline, distribution, 0.0, 0.0, FALSE, res);

    gmodel = gwy_graph_model_new();
    cmodel = gwy_graph_curve_model_new();
    gwy_graph_model_add_curve(gmodel, cmodel);
    g_object_unref(cmodel);

    name = gettext(gwy_resource_get_name(GWY_RESOURCE(gvalue)));
    g_object_set(gmodel,
                 "title", name,
                 "axis-label-left", _("count"),
                 "axis-label-bottom", gwy_grain_value_get_symbol_markup(gvalue),
                 NULL);
    gwy_graph_model_set_units_from_data_line(gmodel, distribution);
    g_object_set(cmodel, "description", name, NULL);
    gwy_graph_curve_model_set_data_from_dataline(cmodel, distribution, 0, 0);
    g_object_unref(distribution);

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
    GwyDataLine *dline;
    gchar **names;
    gint *grains;
    guint i, nvalues;
    gint ngrains;
    gdouble **results;

    grains = g_new0(gint, gwy_data_field_get_xres(mfield)
                          *gwy_data_field_get_yres(mfield));
    ngrains = gwy_data_field_number_grains(mfield, grains);

    names = g_strsplit(args->selected, "\n", 0);
    nvalues = g_strv_length(names);
    expdata.gvalues = g_new(GwyGrainValue*, nvalues);
    expdata.rawvalues = g_new(GwyDataLine*, nvalues);
    expdata.add_comment = args->add_comment;
    results = g_new(gdouble*, nvalues);
    for (nvalues = i = 0; names[i]; i++) {
        gvalue = gwy_grain_values_get_grain_value(names[nvalues]);
        if (!gvalue)
            continue;

        if (!args->units_equal
            && (gwy_grain_value_get_flags(gvalue) & GWY_GRAIN_VALUE_SAME_UNITS))
            continue;

        expdata.gvalues[nvalues] = gvalue;
        dline = gwy_data_line_new(ngrains+1, 1.0, FALSE);
        expdata.rawvalues[nvalues] = dline;
        results[nvalues] = gwy_data_line_get_data(dline);
        nvalues++;
    }
    expdata.nvalues = nvalues;
    g_strfreev(names);

    gwy_grain_values_calculate(nvalues, expdata.gvalues, results,
                               dfield, ngrains, grains);
    g_free(grains);
    g_free(results);

    expdata.args = args;
    switch (args->mode) {
        case MODE_GRAPH:
        for (i = 0; i < expdata.nvalues; i++)
            add_one_distribution(data, dfield, &expdata, i);
        break;

        case MODE_RAW:
        gwy_save_auxiliary_with_callback(_("Export Raw Grain Values"), NULL,
                                         grain_dist_export_create,
                                         (GwySaveAuxiliaryDestroy)g_free,
                                         &expdata);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    for (i = 0; i < expdata.nvalues; i++)
        g_object_unref(expdata.rawvalues[i]);
    g_free(expdata.rawvalues);
    g_free(expdata.gvalues);
}

static gchar*
grain_dist_export_create(gpointer user_data,
                         gssize *data_len)
{
    const GrainDistExportData *expdata = (const GrainDistExportData*)user_data;
    GString *report;
    gchar buffer[32];
    gint gno;
    gchar *retval;
    guint i, ngrains = 0;
    gdouble val;

    if (expdata->nvalues)
        ngrains = gwy_data_line_get_res(expdata->rawvalues[0]) - 1;

    report = g_string_sized_new(12*ngrains*expdata->nvalues);

    if (expdata->add_comment) {
        g_string_append_c(report, '#');
        for (i = 0; i < expdata->nvalues; i++) {
            g_string_append_c(report, ' ');
            g_string_append(report,
                            gwy_grain_value_get_symbol(expdata->gvalues[i]));
        }
        g_string_append_c(report, '\n');
    }

    for (gno = 1; gno <= ngrains; gno++) {
        for (i = 0; i < expdata->nvalues; i++) {
            val = gwy_data_line_get_val(expdata->rawvalues[i], gno);
            g_ascii_formatd(buffer, sizeof(buffer), "%g", val);
            g_string_append(report, buffer);
            g_string_append_c(report, i == expdata->nvalues-1 ? '\n' : '\t');
        }
    }

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
               const gchar *value,
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
    /* FIXME: We should use PLAIN format for text output, not Pango markup */
    g_ptr_array_add(report, g_strdup(value));
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
    GwySIValueFormat *vf;
    gint xres, yres, ngrains;
    gdouble total_area, area, size, vol_0, vol_min, vol_laplace, v;
    gdouble *values = NULL;
    gint *grains;
    GString *str;
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
    g_free(values);
    g_free(grains);

    dialog = gtk_dialog_new_with_buttons(_("Grain Statistics"), NULL, 0,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    table = gtk_table_new(9, 2, FALSE);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    row = 0;
    str = g_string_new(NULL);

    g_string_printf(str, "%d", ngrains);
    add_report_row(GTK_TABLE(table), &row, _("Number of grains:"),
                   str->str, report);

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    siunit2 = gwy_si_unit_power(siunit, 2, NULL);

    v = area;
    vf = gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, NULL);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Total projected area (abs.):"),
                   str->str, report);

    g_string_printf(str, "%.2f %%", 100.0*area/total_area);
    add_report_row(GTK_TABLE(table), &row, _("Total projected area (rel.):"),
                   str->str, report);

    v = area/ngrains;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Mean grain area:"),
                   str->str, report);

    v = size/ngrains;
    gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Mean grain size:"),
                   str->str, report);

    siunit = gwy_data_field_get_si_unit_z(dfield);
    gwy_si_unit_multiply(siunit2, siunit, siunit2);

    v = vol_0;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Total grain volume (zero):"),
                   str->str, report);

    v = vol_min;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Total grain volume (minimum):"),
                   str->str, report);

    v = vol_laplace;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s", vf->precision, v/vf->magnitude, vf->units);
    add_report_row(GTK_TABLE(table), &row, _("Total grain volume (laplacian):"),
                   str->str, report);

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
    gwy_container_gis_boolean_by_name(container, add_comment_key,
                                      &args->add_comment);
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
    gwy_container_set_boolean_by_name(container, add_comment_key,
                                      args->add_comment);
    /* args->selected is updated immediately in settings */
    gwy_container_set_int32_by_name(container, expanded_key, args->expanded);
    gwy_container_set_int32_by_name(container, resolution_key,
                                    args->resolution);
    gwy_container_set_enum_by_name(container, mode_key, args->mode);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
