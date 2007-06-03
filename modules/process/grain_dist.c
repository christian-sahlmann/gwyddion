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
    const gchar *gtitle;
    const gchar *cdesc;
} QuantityInfo;

typedef struct {
    GrainDistMode mode;
    guint selected;
    gboolean fixres;
    gint resolution;

    /* To mask impossible quantitities without really resetting the bits */
    gboolean units_equal;
    guint bitmask;
} GrainDistArgs;

typedef struct {
    GrainDistArgs *args;
    GSList *qlist;
    GSList *mode;
    GtkWidget *fixres;
    GtkObject *resolution;
    GtkWidget *ok;
} GrainDistControls;

typedef struct {
    GrainDistArgs *args;
    GwyDataField *dfield;
    gint ngrains;
    gint *grains;
} GrainDistExportData;

typedef struct {
    GwyGrainQuantity abscissa;
    GwyGrainQuantity ordinate;

    gboolean units_equal;
} GrainCrossArgs;

static gboolean module_register                 (void);
static void grain_dist                          (GwyContainer *data,
                                                 GwyRunType run);
static void grain_stat                          (GwyContainer *data,
                                                 GwyRunType run);
static void grain_cross                         (GwyContainer *data,
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
static void grain_cross_dialog                  (GrainCrossArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GwyDataField *mfield);
static GtkTreeStore* grain_cross_build_store    (void);
static void grain_cross_run                     (GrainCrossArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GwyDataField *mfield);
static void grain_cross_load_args               (GwyContainer *container,
                                                 GrainCrossArgs *args);
static void grain_cross_save_args               (GwyContainer *container,
                                                 GrainCrossArgs *args);

static const GrainDistArgs grain_dist_defaults = {
    MODE_GRAPH,
    1 << GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS,
    FALSE,
    120,
    FALSE,
    0xffffffffU,
};

static const GrainCrossArgs grain_cross_defaults = {
    GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS,
    GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Evaluates distribution of grains (continuous parts of mask)."),
    "Petr Klapetek <petr@klapetek.cz>, Sven Neumann <neumann@jpk.com>, "
        "Yeti <yeti@gwyddion.net>",
    "3.0",
    "David Nečas (Yeti) & Petr Klapetek & Sven Neumann",
    "2003-2007",
};

static const QuantityInfo quantities[] = {
    {
        -1,
        GRAIN_QUANTITY_SET_POSITION,
        N_("Position"),
        NULL,
        NULL,
        NULL,
    },
    {
        GWY_GRAIN_VALUE_CENTER_X,
        GRAIN_QUANTITY_SET_POSITION,
        N_("Center x _position"),
        "<i>x</i><sub>c</sub>",
        N_("Grain Center X Histogram"),
        N_("Grain center x positions"),
    },
    {
        GWY_GRAIN_VALUE_CENTER_Y,
        GRAIN_QUANTITY_SET_POSITION,
        N_("Center _y position"),
        "<i>x</i><sub>c</sub>",
        N_("Grain Center Y Histogram"),
        N_("Grain center y positions"),
    },
    {
        -1,
        GRAIN_QUANTITY_SET_VALUE,
        N_("Value"),
        NULL,
        NULL,
        NULL,
    },
    {
        GWY_GRAIN_VALUE_MINIMUM,
        GRAIN_QUANTITY_SET_VALUE,
        N_("M_inimum"),
        "<i>z</i><sub>min</sub>",
        N_("Grain Minimum Value Histogram"),
        N_("Grain min. values"),
    },
    {
        GWY_GRAIN_VALUE_MAXIMUM,
        GRAIN_QUANTITY_SET_VALUE,
        N_("Ma_ximum"),
        "<i>z</i><sub>max</sub>",
        N_("Grain Maximum Value Histogram"),
        N_("Grain max. values"),
    },
    {
        GWY_GRAIN_VALUE_MEAN,
        GRAIN_QUANTITY_SET_VALUE,
        N_("_Mean"),
        "<i>z̅</i>",
        N_("Grain Mean Value Histogram"),
        N_("Grain mean values"),
    },
    {
        GWY_GRAIN_VALUE_MEDIAN,
        GRAIN_QUANTITY_SET_VALUE,
        N_("Me_dian"),
        "<i>z</i><sub>med</sub>",
        N_("Grain Median Value Histogram"),
        N_("Grain median values"),
    },
    {
        -1,
        GRAIN_QUANTITY_SET_AREA,
        N_("Area"),
        NULL,
        NULL,
        NULL,
    },
    {
        GWY_GRAIN_VALUE_PROJECTED_AREA,
        GRAIN_QUANTITY_SET_AREA,
        N_("_Projected area"),
        "<i>A</i><sub>0</sub>",
        N_("Grain Projected Area Histogram"),
        N_("Grain proj. areas"),
    },
    {
        GWY_GRAIN_VALUE_SURFACE_AREA,
        GRAIN_QUANTITY_SET_AREA,
        N_("S_urface area"),
        "<i>A</i><sub>s</sub>",
        N_("Grain Surface Area Histogram"),
        N_("Grain surf. areas"),
    },
    {
        GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE,
        GRAIN_QUANTITY_SET_AREA,
        N_("Equivalent _square side"),
        "<i>a</i><sub>eq</sub>",
        N_("Grain Equivalent Square Side Histogram"),
        N_("Grain equiv. square sides"),
    },
    {
        GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS,
        GRAIN_QUANTITY_SET_AREA,
        N_("Equivalent disc _radius"),
        "<i>r</i><sub>eq</sub>",
        N_("Grain Equivalent Disc Radius Histogram"),
        N_("Grain equiv. disc radii"),
    },
    {
        -1,
        GRAIN_QUANTITY_SET_VOLUME,
        N_("Volume"),
        NULL,
        NULL,
        NULL,
    },
    {
        GWY_GRAIN_VALUE_VOLUME_0,
        GRAIN_QUANTITY_SET_VOLUME,
        N_("_Zero basis"),
        "<i>V</i><sub>0</sub>",
        N_("Grain Volume (Zero) Histogram"),
        N_("Grain volumes (zero)"),
    },
    {
        GWY_GRAIN_VALUE_VOLUME_MIN,
        GRAIN_QUANTITY_SET_VOLUME,
        N_("_Grain minimum basis"),
        "<i>V</i><sub>min</sub>",
        N_("Grain Volume (Minimum) Histogram"),
        N_("Grain volumes (minimum)"),
    },
    {
        GWY_GRAIN_VALUE_VOLUME_LAPLACE,
        GRAIN_QUANTITY_SET_VOLUME,
        N_("_Laplacian background basis"),
        "<i>V</i><sub>L</sub>",
        N_("Grain Volume (Laplacian) Histogram"),
        N_("Grain volumes (laplacian)"),
    },
    {
        -1,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Boundary"),
        NULL,
        NULL,
        NULL,
    },
    {
        GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Projected _boundary length"),
        "<i>L</i><sub>b0</sub>",
        N_("Grain Projected Boundary Length Histogram"),
        N_("Grain proj. boundary lengths"),
    },
    {
        GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Minimum bounding size"),
        "<i>D</i><sub>min</sub>",
        N_("Grain Minimum Bounding Size Histogram"),
        N_("Grain min. bound. sizes"),
    },
    {
        GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Minimum bounding direction"),
        "<i>φ</i><sub>min</sub>",
        N_("Grain Minimum Bounding Direction Histogram"),
        N_("Grain min. bound. directions"),
    },
    {
        GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Maximum bounding size"),
        "<i>D</i><sub>max</sub>",
        N_("Grain Maximum Bounding Size Histogram"),
        N_("Grain max. bound. sizes"),
    },
    {
        GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE,
        GRAIN_QUANTITY_SET_BOUNDARY,
        N_("Maximum bounding direction"),
        "<i>φ</i><sub>max</sub>",
        N_("Grain Maximum Bounding Direction Histogram"),
        N_("Grain max. bound. directions"),
    },
    {
        -1,
        GRAIN_QUANTITY_SET_SLOPE,
        N_("Slope"),
        NULL,
        NULL,
        NULL,
    },
    {
        GWY_GRAIN_VALUE_SLOPE_THETA,
        GRAIN_QUANTITY_SET_SLOPE,
        N_("Inclination θ"),
        "<i>θ</i>",
        N_("Grain Inclination θ Histogram"),
        N_("Grain incl. θ"),
    },
    {
        GWY_GRAIN_VALUE_SLOPE_PHI,
        GRAIN_QUANTITY_SET_SLOPE,
        N_("Inclination φ"),
        "<i>φ</i>",
        N_("Grain Inclination φ Histogram"),
        N_("Grain incl. φ"),
    },
};

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

static GSList*
append_checkbox_list(GtkTable *table,
                     gint row,
                     gint col,
                     GSList *list,
                     GrainQuantitySet set,
                     guint state,
                     guint bitmask)
{
    GtkWidget *label, *check;
    const gchar *title;
    GtkBox *vbox;
    guint i, bit;

    for (i = 0, title = NULL; i < G_N_ELEMENTS(quantities); i++) {
        if (quantities[i].set == set && quantities[i].quantity == -1) {
            title = quantities[i].label;
            break;
        }
    }
    g_return_val_if_fail(title, list);

    vbox = GTK_BOX(gtk_vbox_new(FALSE, 2));
    gtk_table_attach(table, GTK_WIDGET(vbox),
                     col, col + 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);

    label = gwy_label_new_header(title);
    gtk_box_pack_start(vbox, label, FALSE, FALSE, 0);

    for (i = 0; i < G_N_ELEMENTS(quantities); i++) {
        if (quantities[i].set != set || quantities[i].quantity == -1)
            continue;

        bit = 1 << quantities[i].quantity;
        check = gtk_check_button_new_with_mnemonic(_(quantities[i].label));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), state & bit);
        g_object_set_data(G_OBJECT(check), "bit", GUINT_TO_POINTER(bit));
        gtk_box_pack_start(vbox, check, FALSE, FALSE, 0);
        list = g_slist_prepend(list, check);
        if (!(bit & bitmask)) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), FALSE);
            gtk_widget_set_sensitive(check, FALSE);
        }
    }

    return list;
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
    GtkTable *table;
    gint row, response;
    GSList *l;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Grain Distributions"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    controls.ok = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                        GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    /* Output type */
    table = GTK_TABLE(gtk_table_new(2, 3, FALSE));
    gtk_table_set_row_spacings(table, 8);
    gtk_table_set_col_spacings(table, 12);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);

    controls.qlist = append_checkbox_list(table, 0, 0, NULL,
                                          GRAIN_QUANTITY_SET_VALUE,
                                          args->selected, args->bitmask);
    controls.qlist = append_checkbox_list(table, 1, 0, controls.qlist,
                                          GRAIN_QUANTITY_SET_AREA,
                                          args->selected, args->bitmask);
    controls.qlist = append_checkbox_list(table, 0, 1, controls.qlist,
                                          GRAIN_QUANTITY_SET_BOUNDARY,
                                          args->selected, args->bitmask);
    controls.qlist = append_checkbox_list(table, 1, 1, controls.qlist,
                                          GRAIN_QUANTITY_SET_VOLUME,
                                          args->selected, args->bitmask);
    controls.qlist = append_checkbox_list(table, 2, 0, controls.qlist,
                                          GRAIN_QUANTITY_SET_POSITION,
                                          args->selected, args->bitmask);
    controls.qlist = append_checkbox_list(table, 2, 1, controls.qlist,
                                          GRAIN_QUANTITY_SET_SLOPE,
                                          args->selected, args->bitmask);

    for (l = controls.qlist; l; l = g_slist_next(l))
        g_signal_connect_swapped(l->data, "toggled",
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
    GSList *l;
    guint bit;

    args->selected = 0;
    for (l = controls->qlist; l; l = g_slist_next(l)) {
        bit = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(l->data), "bit"));
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(l->data)))
            args->selected |= bit;
    }

    args->mode = gwy_radio_buttons_get_current(controls->mode);
    args->resolution = gwy_adjustment_get_int(controls->resolution);
    args->fixres
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->fixres));
}

static void
grain_dist_dialog_update_sensitivity(GrainDistControls *controls,
                                     GrainDistArgs *args)
{
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

    gtk_widget_set_sensitive(controls->ok, args->selected & args->bitmask);
}

static const QuantityInfo*
find_quantity_info(GwyGrainQuantity quantity)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(quantities); i++) {
        if (quantities[i].quantity == quantity)
            return quantities + i;
    }
    g_return_val_if_reached(NULL);
}

static void
add_one_distribution(GwyContainer *container,
                     GwyDataField *dfield,
                     gint ngrains,
                     const gint *grains,
                     GwyGrainQuantity quantity,
                     gint resolution)
{
    GwyGraphCurveModel *cmodel;
    GwyGraphModel *gmodel;
    GwyDataLine *dataline;
    const QuantityInfo *qinfo;

    dataline = gwy_data_field_grains_get_distribution(dfield, NULL, NULL,
                                                      ngrains, grains, quantity,
                                                      resolution);
    gmodel = gwy_graph_model_new();
    cmodel = gwy_graph_curve_model_new();
    gwy_graph_model_add_curve(gmodel, cmodel);
    g_object_unref(cmodel);

    qinfo = find_quantity_info(quantity);
    g_object_set(gmodel,
                 "title", _(qinfo->gtitle),
                 "axis-label-left", _("count"),
                 "axis-label-bottom", qinfo->symbol,
                 NULL);
    gwy_graph_model_set_units_from_data_line(gmodel, dataline);
    g_object_set(cmodel, "description", _(qinfo->cdesc), NULL);
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
    gint *grains;
    guint i, bits;
    gint res, ngrains;

    res = gwy_data_field_get_xres(mfield)*gwy_data_field_get_yres(mfield);
    grains = g_new0(gint, res);
    ngrains = gwy_data_field_number_grains(mfield, grains);

    switch (args->mode) {
        case MODE_GRAPH:
        res = args->fixres ? args->resolution : 0;
        bits = args->selected & args->bitmask;
        for (i = 0; bits; i++, bits /= 2) {
            if (bits & 1)
                add_one_distribution(data, dfield, ngrains, grains, i, res);
        }
        break;

        case MODE_RAW:
        expdata.args = args;
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

    g_free(grains);
}

static gchar*
grain_dist_export_create(gpointer user_data,
                         gssize *data_len)
{
    const GrainDistExportData *expdata = (const GrainDistExportData*)user_data;
    const GrainDistArgs *args;
    GString *report;
    gdouble *values[32];
    gchar buffer[32];
    gint res, gno, ncols;
    guint i, bits;
    gchar *retval;

    memset(values, 0, sizeof(values));
    args = expdata->args;
    res = args->fixres ? args->resolution : 0;
    bits = args->selected;
    ncols = 0;
    for (i = 0; bits; i++, bits /= 2) {
        if (bits & 1) {
            values[i] = gwy_data_field_grains_get_values(expdata->dfield, NULL,
                                                         expdata->ngrains,
                                                         expdata->grains, i);
            ncols++;
        }
    }

    report = g_string_sized_new(12*expdata->ngrains*ncols);
    for (gno = 1; gno <= expdata->ngrains; gno++) {
        bits = args->selected;
        for (i = 0; bits; i++, bits /= 2) {
            if (bits & 1) {
                g_ascii_formatd(buffer, sizeof(buffer), "%g", values[i][gno]);
                g_string_append(report, buffer);
                g_string_append_c(report, bits == 1 ? '\n' : '\t');
            }
        }
    }

    bits = args->selected;
    for (i = 0; bits; i++, bits /= 2) {
        if (bits & 1)
            g_free(values[i]);
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

static const gchar fixres_key[]     = "/module/grain_dist/fixres";
static const gchar mode_key[]       = "/module/grain_dist/mode";
static const gchar resolution_key[] = "/module/grain_dist/resolution";
static const gchar selected_key[]   = "/module/grain_dist/selected";

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
    gwy_container_gis_int32_by_name(container, selected_key, &args->selected);
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
    gwy_container_set_int32_by_name(container, selected_key, args->selected);
    gwy_container_set_int32_by_name(container, resolution_key,
                                    args->resolution);
    gwy_container_set_enum_by_name(container, mode_key, args->mode);
}

static void
grain_cross(GwyContainer *data, GwyRunType run)
{
    GwySIUnit *siunitxy, *siunitz;
    GrainCrossArgs args;
    GwyDataField *dfield;
    GwyDataField *mfield;

    g_return_if_fail(run & DIST_RUN_MODES);
    grain_cross_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield && mfield);

    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    siunitz = gwy_data_field_get_si_unit_z(dfield);
    args.units_equal = gwy_si_unit_equal(siunitxy, siunitz);
    if (run == GWY_RUN_IMMEDIATE)
        grain_cross_run(&args, data, dfield, mfield);
    else {
        grain_cross_dialog(&args, data, dfield, mfield);
        grain_cross_save_args(gwy_app_settings_get(), &args);
    }
}

static void
render_name(G_GNUC_UNUSED GtkCellLayout *layout,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            gpointer data)
{
    const QuantityInfo *qinfo;
    const GrainCrossArgs *args = (const GrainCrossArgs*)data;
    gboolean sensitive = TRUE;
    gchar *label;

    gtk_tree_model_get(model, iter, 0, &qinfo, -1);
    if (qinfo->quantity == -1
        || (!args->units_equal
            && gwy_grain_quantity_needs_same_units(qinfo->quantity)))
        sensitive = FALSE;

    label = gwy_strkill(g_strdup(_(qinfo->label)), "_");
    g_object_set(renderer, "text", label, "sensitive", sensitive, NULL);
    g_free(label);
}

static gboolean
select_default_quantity(GtkTreeModel *model,
                        G_GNUC_UNUSED GtkTreePath *path,
                        GtkTreeIter *iter,
                        gpointer user_data)
{
    const QuantityInfo *qinfo;
    GwyGrainQuantity quantity;

    gtk_tree_model_get(model, iter, 0, &qinfo, -1);
    quantity = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(user_data),
                                                  "default"));
    if (qinfo->quantity == quantity) {
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(user_data), iter);
        return TRUE;
    }
    return FALSE;
}

static void
axis_quantity_changed(GtkComboBox *combo,
                      gpointer user_data)
{
    GwyGrainQuantity *quantity = (GwyGrainQuantity*)user_data;
    const QuantityInfo *qinfo;
    GtkTreeIter iter;

    gtk_combo_box_get_active_iter(combo, &iter);
    gtk_tree_model_get(gtk_combo_box_get_model(combo), &iter, 0, &qinfo, -1);
    g_return_if_fail(qinfo->quantity != -1);
    *quantity = qinfo->quantity;
}

static GtkWidget*
attach_axis_combo(GtkTable *table,
                  GtkTreeModel *model,
                  GrainCrossArgs *args,
                  const gchar *name,
                  GwyGrainQuantity *quantity,
                  gint *row)
{
    GtkCellRenderer *renderer;
    GtkWidget *label, *combo;

    label = gtk_label_new_with_mnemonic(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, *row, *row+1, GTK_FILL, 0, 0, 0);

    combo = gtk_combo_box_new_with_model(model);
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo), renderer,
                                       render_name, args, NULL);
    gtk_table_attach(table, combo, 1, 2, *row, *row+1, GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
    (*row)++;

    g_object_set_data(G_OBJECT(combo), "default", GUINT_TO_POINTER(*quantity));
    gtk_tree_model_foreach(model, select_default_quantity, combo);
    g_signal_connect(combo, "changed",
                     G_CALLBACK(axis_quantity_changed), quantity);

    return combo;
}

static void
grain_cross_dialog(GrainCrossArgs *args,
                  GwyContainer *data,
                  GwyDataField *dfield,
                  GwyDataField *mfield)
{
    GtkWidget *dialog, *abscissa, *ordinate;
    GtkTreeStore *store;
    GtkTable *table;
    gint row, response;

    dialog = gtk_dialog_new_with_buttons(_("Grain Correlations"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = GTK_TABLE(gtk_table_new(2, 2, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);
    row = 0;

    store = grain_cross_build_store();
    abscissa = attach_axis_combo(table, GTK_TREE_MODEL(store), args,
                                 _("_Abscissa:"), &args->abscissa, &row);
    ordinate = attach_axis_combo(table, GTK_TREE_MODEL(store), args,
                                 _("O_rdinate:"), &args->ordinate, &row);

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            /* grain_cross_dialog_update_values(&controls, args); */
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

    /* grain_cross_dialog_update_values(&controls, args); */
    gtk_widget_destroy(dialog);

    grain_cross_run(args, data, dfield, mfield);
}

static GtkTreeStore*
grain_cross_build_store(void)
{
    const QuantityInfo *qinfo;
    GtkTreeStore *store;
    GtkTreeIter siter, iter;
    guint i, j;

    store = gtk_tree_store_new(1, G_TYPE_POINTER);

    for (i = j = 0; i < G_N_ELEMENTS(quantities); i++) {
        qinfo = quantities + i;
        if (qinfo->quantity == -1) {
            if (!i)
                gtk_tree_store_insert_after(store, &siter, NULL, NULL);
            else
                gtk_tree_store_insert_after(store, &siter, NULL, &siter);
            gtk_tree_store_set(store, &siter, 0, qinfo, -1);
            j = 0;
        }
        else {
            if (!j)
                gtk_tree_store_insert_after(store, &iter, &siter, NULL);
            else
                gtk_tree_store_insert_after(store, &iter, &siter, &iter);
            gtk_tree_store_set(store, &iter, 0, qinfo, -1);
            j++;
        }
    }

    return store;
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
    const QuantityInfo *qinfox, *qinfoy;
    gdouble *xdata, *ydata, *bothdata;
    GwySIUnit *siunitxy, *siunitz, *siunitx, *siunity;
    gint *grains;
    gint res, ngrains, i;

    res = gwy_data_field_get_xres(mfield)*gwy_data_field_get_yres(mfield);
    grains = g_new0(gint, res);
    ngrains = gwy_data_field_number_grains(mfield, grains);

    bothdata = g_new(gdouble, 4*ngrains + 2);
    xdata = bothdata + 2*ngrains;
    ydata = bothdata + 3*ngrains + 1;
    gwy_data_field_grains_get_values(dfield, xdata, ngrains, grains,
                                     args->abscissa);
    gwy_data_field_grains_get_values(dfield, ydata, ngrains, grains,
                                     args->ordinate);
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

    qinfox = find_quantity_info(args->abscissa);
    qinfoy = find_quantity_info(args->ordinate);
    siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    siunitz = gwy_data_field_get_si_unit_z(dfield);
    siunitx = gwy_grain_quantity_get_units(args->abscissa,
                                           siunitxy, siunitz, NULL);
    siunity = gwy_grain_quantity_get_units(args->ordinate,
                                           siunitxy, siunitz, NULL);
    g_object_set(gmodel,
                 "title", _(qinfoy->cdesc),  /* FIXME */
                 "axis-label-left", qinfoy->symbol,
                 "axis-label-bottom", qinfox->symbol,
                 "si-unit-x", siunitx,
                 "si-unit-y", siunity,
                 NULL);
    g_object_unref(siunitx);
    g_object_unref(siunity);

    g_object_set(cmodel,
                 "description", _(qinfoy->cdesc),
                 "mode", GWY_GRAPH_CURVE_POINTS,
                 NULL);
    gwy_graph_curve_model_set_data(cmodel, xdata, ydata, ngrains);
    g_free(bothdata);

    gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
    g_object_unref(gmodel);
}

static const gchar abscissa_key[] = "/module/grain_cross/abscissa";
static const gchar ordinate_key[] = "/module/grain_cross/ordinate";

/* FIXME: GLib can do this somehow, but I'm not sure if it has a suitable
 * API, we have a suitable API, but it requires GwyEnum. */
static GwyGrainQuantity
grain_quantity_validate(GwyGrainQuantity quantity)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(quantities); i++) {
        if (quantities[i].quantity == quantity)
            return quantity;
    }
    return GWY_GRAIN_VALUE_MEAN;
}

static void
grain_cross_sanitize_args(GrainCrossArgs *args)
{
    args->abscissa = grain_quantity_validate(args->abscissa);
    args->ordinate = grain_quantity_validate(args->ordinate);
}

static void
grain_cross_load_args(GwyContainer *container,
                      GrainCrossArgs *args)
{
    *args = grain_cross_defaults;

    gwy_container_gis_enum_by_name(container, abscissa_key, &args->abscissa);
    gwy_container_gis_enum_by_name(container, ordinate_key, &args->ordinate);
    grain_cross_sanitize_args(args);
}

static void
grain_cross_save_args(GwyContainer *container,
                      GrainCrossArgs *args)
{
    gwy_container_set_enum_by_name(container, abscissa_key, args->abscissa);
    gwy_container_set_enum_by_name(container, ordinate_key, args->ordinate);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
