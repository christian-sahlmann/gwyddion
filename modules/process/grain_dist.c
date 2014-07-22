/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2012 David Necas (Yeti), Petr Klapetek, Sven Neumann.
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

#define DIST_RUN_MODES GWY_RUN_INTERACTIVE

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
    gint *grains;
    guint ngrains;
} GrainDistArgs;

typedef struct {
    GrainDistArgs *args;
    GwyDataField *dfield;
    GtkWidget *graph;
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

static gboolean       module_register           (void);
static void           grain_dist                (GwyContainer *data,
                                                 GwyRunType run);
static void           grain_dist_dialog         (GrainDistArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield);
static void           mode_changed              (GtkToggleButton *button,
                                                 GrainDistControls *controls);
static void           selected_changed          (GrainDistControls *controls);
static void           resolution_changed        (GrainDistControls *controls,
                                                 GtkAdjustment *adj);
static void           fixres_changed            (GrainDistControls *controls,
                                                 GtkToggleButton *check);
static void           add_comment_changed       (GrainDistControls *controls,
                                                 GtkToggleButton *check);
static void           update_sensitivity        (GrainDistControls *controls,
                                                 GrainDistArgs *args);
static void           preview_dist              (GrainDistControls *controls);
static GwyGraphModel* add_one_distribution      (GwyDataField *dfield,
                                                 GrainDistExportData *expdata,
                                                 guint i);
static void           grain_dist_run            (GrainDistArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield);
static gchar*         grain_dist_export_create  (gpointer user_data,
                                                 gssize *data_len);
static void           grain_dist_load_args      (GwyContainer *container,
                                                 GrainDistArgs *args);
static void           grain_dist_save_args      (GwyContainer *container,
                                                 GrainDistArgs *args);

static const GrainDistArgs grain_dist_defaults = {
    MODE_GRAPH,
    "Equivalent disc radius",
    0,
    FALSE,
    FALSE,
    120,
    /* dynamic state */
    FALSE,
    NULL, 0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Evaluates distribution of grains (continuous parts of mask)."),
    "Petr Klapetek <petr@klapetek.cz>, Sven Neumann <neumann@jpk.com>, "
        "Yeti <yeti@gwyddion.net>",
    "4.0",
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

    args.grains = g_new0(gint, mfield->xres*mfield->yres);
    args.ngrains = gwy_data_field_number_grains(mfield, args.grains);

    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    siunitz = gwy_data_field_get_si_unit_z(dfield);
    args.units_equal = gwy_si_unit_equal(siunitxy, siunitz);

    if (run == GWY_RUN_IMMEDIATE)
        grain_dist_run(&args, data, dfield);
    else {
        grain_dist_dialog(&args, data, dfield);
        grain_dist_save_args(gwy_app_settings_get(), &args);
    }

    g_free(args.grains);
}

static void
grain_dist_dialog(GrainDistArgs *args,
                  GwyContainer *data,
                  GwyDataField *dfield)
{
    static const GwyEnum modes[] = {
        { N_("_Export raw data"), MODE_RAW,   },
        { N_("Plot _graphs"),     MODE_GRAPH, },
    };

    GrainDistControls controls;
    GtkWidget *scwin, *hbox, *vbox;
    GwyGraphModel *gmodel;
    GtkDialog *dialog;
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTable *table;
    GSList *l;
    gint row, response;

    controls.args = args;
    controls.dfield = dfield;

    dialog = GTK_DIALOG(gtk_dialog_new_with_buttons(_("Grain Distributions"),
                                                    NULL, 0,
                                                    GTK_STOCK_CLEAR,
                                                    RESPONSE_CLEAR,
                                                    GTK_STOCK_CANCEL,
                                                    GTK_RESPONSE_CANCEL,
                                                    NULL));
    controls.ok = gtk_dialog_add_button(dialog, GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 520);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 4);

    gmodel = gwy_graph_model_new();
    controls.graph = gwy_graph_new(gmodel);
    gtk_widget_set_size_request(controls.graph, 360, -1);
    g_object_unref(gmodel);
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, TRUE, TRUE, 4);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 4);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scwin, TRUE, TRUE, 0);

    controls.values = gwy_grain_value_tree_view_new(FALSE,
                                                    "name", "enabled", NULL);
    treeview = GTK_TREE_VIEW(controls.values);
    gtk_tree_view_set_headers_visible(treeview, FALSE);
    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(preview_dist), &controls);
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
                             G_CALLBACK(selected_changed), &controls);

    /* Options */
    table = GTK_TABLE(gtk_table_new(5, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    row = 0;

    controls.mode = gwy_radio_buttons_create(modes, G_N_ELEMENTS(modes),
                                             G_CALLBACK(mode_changed),
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
    g_signal_connect_swapped(controls.add_comment, "toggled",
                             G_CALLBACK(add_comment_changed), &controls);
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
    g_signal_connect_swapped(controls.resolution, "value-changed",
                             G_CALLBACK(resolution_changed), &controls);
    g_signal_connect_swapped(controls.fixres, "toggled",
                             G_CALLBACK(fixres_changed), &controls);
    row++;

    gtk_widget_show_all(GTK_WIDGET(dialog));
    update_sensitivity(&controls, args);

    do {
        response = gtk_dialog_run(dialog);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(GTK_WIDGET(dialog));
            case GTK_RESPONSE_NONE:
            return;
            break;

            case RESPONSE_CLEAR:
            g_signal_handlers_block_by_func(model,
                                            selected_changed, &controls);
            gwy_grain_value_tree_view_set_enabled(treeview, NULL);
            g_signal_handlers_unblock_by_func(model,
                                              selected_changed, &controls);
            selected_changed(&controls);
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(GTK_WIDGET(dialog));

    grain_dist_run(args, data, dfield);
}

static void
mode_changed(GtkToggleButton *button,
             GrainDistControls *controls)
{
    GrainDistArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args->mode = gwy_radio_buttons_get_current(controls->mode);
    update_sensitivity(controls, args);
}

static void
selected_changed(GrainDistControls *controls)
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
    controls->args->selected = gwy_container_get_string_by_name(settings,
                                                                selected_key);

    update_sensitivity(controls, controls->args);
}

static void
resolution_changed(GrainDistControls *controls, GtkAdjustment *adj)
{
    controls->args->resolution = gwy_adjustment_get_int(adj);
    preview_dist(controls);
}

static void
fixres_changed(GrainDistControls *controls, GtkToggleButton *check)
{
    controls->args->fixres = gtk_toggle_button_get_active(check);
    preview_dist(controls);
}

static void
add_comment_changed(GrainDistControls *controls, GtkToggleButton *check)
{
    controls->args->add_comment = gtk_toggle_button_get_active(check);
}

static void
update_sensitivity(GrainDistControls *controls, GrainDistArgs *args)
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
preview_dist(GrainDistControls *controls)
{
    GrainDistArgs *args = controls->args;
    GtkTreeSelection *selection;
    GrainDistExportData expdata;
    GwyGraphModel *gmodel;
    GwyGrainValue *gvalue;
    GtkTreeModel *model;
    GwyDataLine *dline;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->values));

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
        gwy_graph_model_remove_all_curves(gmodel);
        return;
    }

    gtk_tree_model_get(model, &iter, 0, &gvalue, -1);
    expdata.args = args;
    expdata.nvalues = 1;
    expdata.gvalues = &gvalue;
    dline = gwy_data_line_new(args->ngrains+1, 1.0, FALSE);
    expdata.rawvalues = &dline;
    expdata.add_comment = FALSE;
    gwy_grain_values_calculate(1, expdata.gvalues, &dline->data,
                               controls->dfield, args->ngrains, args->grains);
    gmodel = add_one_distribution(controls->dfield, &expdata, 0);
    gwy_graph_set_model(GWY_GRAPH(controls->graph), gmodel);
    g_object_unref(dline);
    g_object_unref(gmodel);
}

static GwyGraphModel*
add_one_distribution(GwyDataField *dfield,
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

    return gmodel;
}

static void
grain_dist_run(GrainDistArgs *args,
               GwyContainer *data,
               GwyDataField *dfield)
{
    GrainDistExportData expdata;
    GwyGrainValue *gvalue;
    GwyDataLine *dline;
    gchar **names;
    guint i, nvalues;
    gdouble **results;

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
        dline = gwy_data_line_new(args->ngrains+1, 1.0, FALSE);
        expdata.rawvalues[nvalues] = dline;
        results[nvalues] = gwy_data_line_get_data(dline);
        nvalues++;
    }
    expdata.nvalues = nvalues;
    g_strfreev(names);

    gwy_grain_values_calculate(nvalues, expdata.gvalues, results,
                               dfield, args->ngrains, args->grains);
    g_free(results);

    expdata.args = args;
    switch (args->mode) {
        case MODE_GRAPH:
        for (i = 0; i < expdata.nvalues; i++) {
            GwyGraphModel *gmodel;

            gmodel = add_one_distribution(dfield, &expdata, i);
            gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
            g_object_unref(gmodel);
        }
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
