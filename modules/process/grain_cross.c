/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
#include <libgwydgets/gwygrainvaluemenu.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define DIST_RUN_MODES GWY_RUN_INTERACTIVE
#define STAT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    const gchar *abscissa;
    gint abscissa_expanded;
    const gchar *ordinate;
    gint ordinate_expanded;

    gboolean units_equal;
} GrainCrossArgs;

typedef struct {
    GrainCrossArgs *args;
    GtkDialog *dialog;
    GtkTreeView *abscissa;
    GtkTreeView *ordinate;
} GrainCrossControls;

static gboolean module_register                 (void);
static void grain_cross                         (GwyContainer *data,
                                                 GwyRunType run);
static void grain_cross_dialog                  (GrainCrossArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GwyDataField *mfield);
static void grain_cross_run                     (GrainCrossArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GwyDataField *mfield);
static void grain_cross_load_args               (GwyContainer *container,
                                                 GrainCrossArgs *args);
static void grain_cross_save_args               (GwyContainer *container,
                                                 GrainCrossArgs *args);

static const GrainCrossArgs grain_cross_defaults = {
    "Equivalent disc radius", 0,
    "Projected boundary length", 0,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Plots one grain quantity as a function of another."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas",
    "2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("grain_cross",
                              (GwyProcessFunc)&grain_cross,
                              N_("/_Grains/_Correlate..."),
                              GWY_STOCK_GRAINS_GRAPH,
                              STAT_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Correlate grain characteristics"));

    return TRUE;
}

static void
grain_cross(GwyContainer *data, GwyRunType run)
{
    GwySIUnit *siunitxy, *siunitz;
    GrainCrossArgs args;
    GwyDataField *dfield;
    GwyDataField *mfield;
    gint id;

    g_return_if_fail(run & DIST_RUN_MODES);
    grain_cross_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield && mfield);

    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    siunitz = gwy_data_field_get_si_unit_z(dfield);
    args.units_equal = gwy_si_unit_equal(siunitxy, siunitz);
    if (!args.units_equal) {
        GwyGrainValue *abscissa, *ordinate;

        abscissa = gwy_grain_values_get_grain_value(args.abscissa);
        ordinate = gwy_grain_values_get_grain_value(args.ordinate);
        if (gwy_grain_value_get_same_units(abscissa)
            || gwy_grain_value_get_same_units(ordinate)) {
            GtkWidget *dialog;

            dialog = gtk_message_dialog_new
                                    (gwy_app_find_window_for_channel(data, id),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_OK,
                                     _("Grain correlation: Lateral dimensions "
                                       "and value must be the same physical "
                                       "quantity for the selected grain "
                                       "properties."));
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return;
        }
    }
    if (run == GWY_RUN_IMMEDIATE)
        grain_cross_run(&args, data, dfield, mfield);
    else {
        grain_cross_dialog(&args, data, dfield, mfield);
        grain_cross_save_args(gwy_app_settings_get(), &args);
    }
}

static void
axis_quantity_changed(GrainCrossControls *controls)
{
    GtkTreeSelection *selection;
    GwyGrainValue *gvalue;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean ok;

    ok = TRUE;

    selection = gtk_tree_view_get_selection(controls->abscissa);
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        /* XXX XXX XXX: Model columns */
        gtk_tree_model_get(model, &iter, 0, &gvalue, -1);
        controls->args->abscissa = gwy_resource_get_name(GWY_RESOURCE(gvalue));
    }
    else
        ok = FALSE;

    selection = gtk_tree_view_get_selection(controls->ordinate);
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        /* XXX XXX XXX: Model columns */
        gtk_tree_model_get(model, &iter, 0, &gvalue, -1);
        controls->args->ordinate = gwy_resource_get_name(GWY_RESOURCE(gvalue));
    }
    else
        ok = FALSE;

    gtk_dialog_set_response_sensitive(controls->dialog, GTK_RESPONSE_OK, ok);
}

static GtkTreeView*
attach_axis_list(GtkTable *table,
                 const gchar *name,
                 gint column,
                 const gchar *selected,
                 gint expanded,
                 GrainCrossControls *controls)
{
    GwyGrainValue *gvalue;
    GtkTreeSelection *selection;
    GtkTreeView *list;
    GtkWidget *label, *widget, *scwin;

    label = gtk_label_new_with_mnemonic(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label,
                     column, column+1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_table_attach(table, scwin,
                     column, column+1, 1, 2,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    widget = gwy_grain_value_tree_view_new(FALSE, "name", NULL);
    list = GTK_TREE_VIEW(widget);
    gtk_tree_view_set_headers_visible(list, FALSE);
    gwy_grain_value_tree_view_set_same_units(list, controls->args->units_equal);
    gwy_grain_value_tree_view_set_expanded_groups(list, expanded);
    if ((gvalue = gwy_grain_values_get_grain_value(selected)))
        gwy_grain_value_tree_view_select(list, gvalue);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), widget);
    gtk_container_add(GTK_CONTAINER(scwin), widget);

    selection = gtk_tree_view_get_selection(list);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(axis_quantity_changed), controls);

    return list;
}

static void
grain_cross_dialog(GrainCrossArgs *args,
                  GwyContainer *data,
                  GwyDataField *dfield,
                  GwyDataField *mfield)
{
    GrainCrossControls controls;
    GtkWidget *dialog;
    GtkTable *table;
    gint response;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Grain Correlations"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = GTK_DIALOG(dialog);
    gtk_dialog_set_has_separator(controls.dialog, FALSE);
    gtk_dialog_set_default_response(controls.dialog, GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 440, 520);

    table = GTK_TABLE(gtk_table_new(2, 2, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(controls.dialog->vbox), GTK_WIDGET(table),
                       TRUE, TRUE, 0);

    controls.abscissa = attach_axis_list(table, _("_Abscissa"), 0,
                                         args->abscissa,
                                         args->abscissa_expanded,
                                         &controls);
    controls.ordinate = attach_axis_list(table, _("O_rdinate"), 1,
                                         args->ordinate,
                                         args->ordinate_expanded,
                                         &controls);
    axis_quantity_changed(&controls);

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(controls.dialog);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
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

    gtk_widget_destroy(dialog);

    grain_cross_run(args, data, dfield, mfield);
}

static int
compare_doubles(const void *a, const void *b)
{
    gdouble da = *(gdouble*)a;
    gdouble db = *(gdouble*)b;

    if (da < db)
        return -1;
    if (db < da)
        return 1;
    return 0.0;
}

static void
grain_cross_run(GrainCrossArgs *args,
                GwyContainer *data,
                GwyDataField *dfield,
                GwyDataField *mfield)
{
    GwyGraphCurveModel *cmodel;
    GwyGraphModel *gmodel;
    GwyGrainValue *gvalues[2];
    gdouble *xdata, *ydata, *bothdata, *rdata[2];
    GwySIUnit *siunitxy, *siunitz, *siunitx, *siunity;
    const gchar *title;
    gint *grains;
    gint res, ngrains, i;

    gvalues[0] = gwy_grain_values_get_grain_value(args->abscissa);
    gvalues[1] = gwy_grain_values_get_grain_value(args->ordinate);

    res = gwy_data_field_get_xres(mfield)*gwy_data_field_get_yres(mfield);
    grains = g_new0(gint, res);
    ngrains = gwy_data_field_number_grains(mfield, grains);

    bothdata = g_new(gdouble, 4*ngrains + 2);
    rdata[0] = xdata = bothdata + 2*ngrains;
    rdata[1] = ydata = bothdata + 3*ngrains + 1;
    gwy_grain_values_calculate(2, gvalues, rdata, dfield, ngrains, grains);
    g_free(grains);

    for (i = 0; i < ngrains; i++) {
        bothdata[2*i + 0] = xdata[i+1];
        bothdata[2*i + 1] = ydata[i+1];
    }
    qsort(bothdata, ngrains, 2*sizeof(gdouble), compare_doubles);
    for (i = 0; i < ngrains; i++) {
        xdata[i] = bothdata[2*i + 0];
        ydata[i] = bothdata[2*i + 1];
    }

    gmodel = gwy_graph_model_new();
    cmodel = gwy_graph_curve_model_new();
    gwy_graph_model_add_curve(gmodel, cmodel);
    g_object_unref(cmodel);

    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    siunitz = gwy_data_field_get_si_unit_z(dfield);
    siunitx = gwy_si_unit_power_multiply
                            (siunitxy, gwy_grain_value_get_power_xy(gvalues[0]),
                             siunitz, gwy_grain_value_get_power_z(gvalues[0]),
                             NULL);
    siunity = gwy_si_unit_power_multiply
                            (siunitxy, gwy_grain_value_get_power_xy(gvalues[1]),
                             siunitz, gwy_grain_value_get_power_z(gvalues[1]),
                             NULL);
    /* FIXME */
    title = gettext(gwy_resource_get_name(GWY_RESOURCE(gvalues[1])));
    g_object_set
        (gmodel,
         "title", title,
         "axis-label-left", gwy_grain_value_get_symbol_markup(gvalues[1]),
         "axis-label-bottom", gwy_grain_value_get_symbol_markup(gvalues[0]),
         "si-unit-x", siunitx,
         "si-unit-y", siunity,
         NULL);
    g_object_unref(siunitx);
    g_object_unref(siunity);

    g_object_set(cmodel,
                 "description", title,
                 "mode", GWY_GRAPH_CURVE_POINTS,
                 NULL);
    gwy_graph_curve_model_set_data(cmodel, xdata, ydata, ngrains);
    g_free(bothdata);

    gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
    g_object_unref(gmodel);
}

static const gchar abscissa_key[]          = "/module/grain_cross/abscissa";
static const gchar abscissa_expanded_key[] = "/module/grain_cross/abscissa_expanded";
static const gchar ordinate_key[]          = "/module/grain_cross/ordinate";
static const gchar ordinate_expanded_key[] = "/module/grain_cross/ordinate_expanded";

static void
grain_cross_sanitize_args(GrainCrossArgs *args)
{
    if (!gwy_grain_values_get_grain_value(args->abscissa))
        args->abscissa = grain_cross_defaults.abscissa;

    if (!gwy_grain_values_get_grain_value(args->ordinate))
        args->ordinate = grain_cross_defaults.ordinate;
}

static void
grain_cross_load_args(GwyContainer *container,
                      GrainCrossArgs *args)
{
    *args = grain_cross_defaults;

    if (gwy_container_value_type_by_name(container, abscissa_key) != G_TYPE_INT)
        gwy_container_gis_string_by_name(container, abscissa_key,
                                         (const guchar**)&args->abscissa);
    if (gwy_container_value_type_by_name(container, ordinate_key) != G_TYPE_INT)
        gwy_container_gis_string_by_name(container, ordinate_key,
                                         (const guchar**)&args->ordinate);

    gwy_container_gis_boolean_by_name(container, abscissa_expanded_key,
                                      &args->abscissa_expanded);
    gwy_container_gis_boolean_by_name(container, ordinate_expanded_key,
                                      &args->ordinate_expanded);
    grain_cross_sanitize_args(args);
}

static void
grain_cross_save_args(GwyContainer *container,
                      GrainCrossArgs *args)
{
    gwy_container_set_string_by_name(container, abscissa_key,
                                     g_strdup(args->abscissa));
    gwy_container_set_boolean_by_name(container, abscissa_expanded_key,
                                      args->abscissa_expanded);
    gwy_container_set_string_by_name(container, ordinate_key,
                                     g_strdup(args->ordinate));
    gwy_container_set_boolean_by_name(container, ordinate_expanded_key,
                                      args->ordinate_expanded);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
