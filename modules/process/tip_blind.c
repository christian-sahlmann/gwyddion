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

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/tip.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define TIP_BLIND_RUN_MODES \
    (GWY_RUN_MODAL)

enum {
    MIN_RES = 3,
    MAX_RES = 128
};

/* Data for this function. */
typedef struct {
    gint xres;
    gint yres;
    gdouble thresh;
    gboolean use_boundaries;
    gboolean same_resolution;
    GwyContainer *data;
} TipBlindArgs;

typedef struct {
    TipBlindArgs *args;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *data;
    GwyDataWindow *data_window;
    GtkWidget *type;
    GtkWidget *threshold;
    GtkWidget *boundaries;
    GwyContainer *tip;
    GwyContainer *vtip;
    GtkObject *xres;
    GtkObject *yres;
    gint vxres;
    gint vyres;
    gboolean tipdone;
    gboolean good_tip;
    GtkWidget *same_resolution;
    gboolean in_update;
} TipBlindControls;

static gboolean    module_register            (const gchar *name);
static gboolean    tip_blind                  (GwyContainer *data,
                                               GwyRunType run);
static gboolean    tip_blind_dialog           (TipBlindArgs *args,
                                               GwyContainer *data);
static void        reset                      (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void        tip_blind_run              (TipBlindControls *controls,
                                               TipBlindArgs *args,
                                               gboolean full);
static void        tip_blind_do               (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void        tip_blind_load_args        (GwyContainer *container,
                                               TipBlindArgs *args);
static void        tip_blind_save_args        (GwyContainer *container,
                                               TipBlindArgs *args);
static void        tip_blind_sanitize_args    (TipBlindArgs *args);
static void        width_changed_cb           (GtkAdjustment *adj,
                                               TipBlindControls *controls);
static void        height_changed_cb          (GtkAdjustment *adj,
                                               TipBlindControls *controls);
static void        thresh_changed_cb          (GwyValUnit *valunit,
                                               TipBlindArgs *args);
static void        bound_changed_cb           (GtkToggleButton *button,
                                               TipBlindArgs *args);
static void        same_resolution_changed_cb (GtkToggleButton *button,
                                               TipBlindControls *controls);
static void        data_window_cb             (GtkWidget *item,
                                               TipBlindControls *controls);
static void        tip_update                 (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void        tip_blind_dialog_abandon   (TipBlindControls *controls);
static void        prepare_fields             (GwyDataField *tipfield,
                                               GwyDataField *surface,
                                               gint xres,
                                               gint yres);

TipBlindArgs tip_blind_defaults = {
    100,
    100,
    0.0000000001,
    FALSE,
    TRUE,
    NULL,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Blind estimation of SPM tip using Villarubia's algorithm."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo tip_blind_func_info = {
        "tip_blind",
        N_("/_Tip/_Blind estimation..."),
        (GwyProcessFunc)&tip_blind,
        TIP_BLIND_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &tip_blind_func_info);

    return TRUE;
}

static gboolean
tip_blind(GwyContainer *data, GwyRunType run)
{
    TipBlindArgs args;

    g_assert(run & TIP_BLIND_RUN_MODES);

    tip_blind_load_args(gwy_app_settings_get(), &args);
    tip_blind_dialog(&args, data);
    tip_blind_save_args(gwy_app_settings_get(), &args);

    return FALSE;
}


static gboolean
tip_blind_dialog(TipBlindArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *hbox;
    TipBlindControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PARTIAL,
        RESPONSE_FULL
    };
    gint response, row;
    GtkObject *layer;
    GwyDataField *dfield;
    GtkWidget *label;

    dialog = gtk_dialog_new_with_buttons(_("Blind Tip Estimation"), NULL, 0,
                                         _("Run _Partial"), RESPONSE_PARTIAL,
                                         _("Run _Full"), RESPONSE_FULL,
                                         _("_Reset Tip"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    controls.args = args;
    controls.in_update = TRUE;
    controls.good_tip = FALSE;
    controls.dialog = dialog;
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
                                      controls.good_tip);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.vxres = 200;
    controls.vyres = 200;

    /*set initial tip properties*/
    controls.tip = gwy_container_duplicate_by_prefix(data,
                                                     "/0/data",
                                                     "/0/base/palette",
                                                     NULL);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.tip,
                                                             "/0/data"));
    gwy_data_field_resample(dfield, args->xres, args->yres,
                            GWY_INTERPOLATION_NONE);
    gwy_data_field_fill(dfield, 0);

    /*set up data of rescaled image of the tip*/
    controls.vtip = gwy_container_duplicate(controls.tip);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.vtip,
                                                             "/0/data"));
    gwy_data_field_resample(dfield, controls.vxres, controls.vyres,
                            GWY_INTERPOLATION_ROUND);

    /*set up rescaled image of the tip*/
    controls.view = gwy_data_view_new(controls.vtip);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));

    /*set up tip estimation controls*/
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(7, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 4);
    row = 0;

    controls.data_window = gwy_app_data_window_get_current();
    args->data = gwy_data_window_get_data(controls.data_window);
    controls.data
        = gwy_option_menu_data_window(G_CALLBACK(data_window_cb), &controls,
                                      NULL, GTK_WIDGET(controls.data_window));
    gwy_table_attach_hscale(table, row, _("Related _data:"), NULL,
                            GTK_OBJECT(controls.data), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new_with_mnemonic(_("Estimated Tip Size"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_FILL, 0, 2, 2);
    row++;

    controls.xres = gtk_adjustment_new(args->xres, MIN_RES, MAX_RES, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Width:"), "px", controls.xres, 0);
    g_object_set_data(G_OBJECT(controls.xres), "controls", &controls);
    g_signal_connect(controls.xres, "value_changed",
                     G_CALLBACK(width_changed_cb), &controls);
    row++;

    controls.yres = gtk_adjustment_new(args->yres, MIN_RES, MAX_RES, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Height:"), "px", controls.yres, 0);
    g_object_set_data(G_OBJECT(controls.yres), "controls", &controls);
    g_signal_connect(controls.yres, "value_changed",
                     G_CALLBACK(height_changed_cb), &controls);
    row++;

    controls.same_resolution
        = gtk_check_button_new_with_mnemonic(_("_Same resolution"));
    gtk_table_attach(GTK_TABLE(table), controls.same_resolution,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.same_resolution),
                                 args->same_resolution);
    g_signal_connect(controls.same_resolution, "toggled",
                     G_CALLBACK(same_resolution_changed_cb), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* FIXME: get rid of valunit */
    controls.threshold = gwy_val_unit_new(_("_Threshold:"),
                                          gwy_data_field_get_si_unit_z(dfield));
    gtk_table_attach(GTK_TABLE(table), controls.threshold,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gwy_val_unit_set_value(GWY_VAL_UNIT(controls.threshold), args->thresh);
    g_signal_connect(GWY_VAL_UNIT(controls.threshold), "value_changed",
                     G_CALLBACK(thresh_changed_cb), args);
    row++;

    controls.boundaries
                = gtk_check_button_new_with_mnemonic(_("Use _boundaries"));
    gtk_table_attach(GTK_TABLE(table), controls.boundaries,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.boundaries),
                                                 args->use_boundaries);
    g_signal_connect(controls.boundaries, "toggled",
                     G_CALLBACK(bound_changed_cb), args);

    controls.tipdone = FALSE;
    controls.in_update = FALSE;
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
            tip_blind_do(&controls, args);
            break;

            case RESPONSE_RESET:
            reset(&controls, args);
            break;

            case RESPONSE_PARTIAL:
            tip_blind_run(&controls, args, FALSE);
            break;

            case RESPONSE_FULL:
            tip_blind_run(&controls, args, TRUE);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    tip_blind_dialog_abandon(&controls);

    return TRUE;
}

static void
tip_blind_dialog_abandon(TipBlindControls *controls)
{
    /*free data of the rescaled tip image*/
    g_object_unref(controls->vtip);

    /*if dialog was cancelled, free also tip data*/
    if (!controls->tipdone)
        g_object_unref(controls->tip);
}

static void
width_changed_cb(GtkAdjustment *adj,
                 TipBlindControls *controls)
{
    TipBlindArgs *args;
    gdouble v;

    args = controls->args;
    v = gtk_adjustment_get_value(adj);
    args->xres = ROUND(v);
    if (controls->in_update)
        return;

    if (args->same_resolution) {
        controls->in_update = TRUE;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yres), v);
        controls->in_update = FALSE;
    }

    tip_update(controls, args);
}

static void
height_changed_cb(GtkAdjustment *adj,
                  TipBlindControls *controls)
{
    TipBlindArgs *args;
    gdouble v;

    args = controls->args;
    v = gtk_adjustment_get_value(adj);
    args->yres = ROUND(v);
    if (controls->in_update)
        return;

    if (args->same_resolution) {
        controls->in_update = TRUE;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xres), v);
        controls->in_update = FALSE;
    }

    tip_update(controls, args);
}

static void
thresh_changed_cb(GwyValUnit *valunit,
                  TipBlindArgs *args)
{
    args->thresh = gwy_val_unit_get_value(valunit);
}

static void
bound_changed_cb(GtkToggleButton *button,
                 TipBlindArgs *args)
{
    args->use_boundaries
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
}

static void
same_resolution_changed_cb(GtkToggleButton *button,
                           TipBlindControls *controls)
{
    TipBlindArgs *args;

    args = controls->args;
    args->same_resolution
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

    if (!args->same_resolution)
        return;

    controls->in_update = TRUE;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yres), args->xres);
    controls->in_update = FALSE;

    tip_update(controls, args);
}

static void
data_window_cb(GtkWidget *item, TipBlindControls *controls)
{
    controls->data_window = (GwyDataWindow*)g_object_get_data(G_OBJECT(item),
                                                              "data-window");
    controls->args->data = gwy_data_window_get_data(controls->data_window);
}

static void
reset(TipBlindControls *controls, TipBlindArgs *args)
{
    GwyDataField *tipfield;

    tipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->tip,
                                                               "/0/data"));
    gwy_data_field_fill(tipfield, 0);
    controls->good_tip = FALSE;
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      GTK_RESPONSE_OK, controls->good_tip);

    tip_update(controls, args);
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}

static void
prepare_fields(GwyDataField *tipfield,
               GwyDataField *surface,
               gint xres, gint yres)
{
    gint xoldres, yoldres;

    /*set real sizes corresponding to actual data*/
    gwy_data_field_set_xreal(tipfield,
                             gwy_data_field_get_xreal(surface)
                             /gwy_data_field_get_xres(surface)
                             *gwy_data_field_get_xres(tipfield));
    gwy_data_field_set_yreal(tipfield,
                             gwy_data_field_get_yreal(surface)
                             /gwy_data_field_get_yres(surface)
                             *gwy_data_field_get_yres(tipfield));

    /*if user has changed tip size, change it*/
    if ((xres != tipfield->xres) || (yres != tipfield->yres)) {
        xoldres = gwy_data_field_get_xres(tipfield);
        yoldres = gwy_data_field_get_yres(tipfield);
        gwy_data_field_resample(tipfield, xres, yres,
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_set_xreal(tipfield,
                                 gwy_data_field_get_xreal(tipfield)
                                 /xoldres*xres);
        gwy_data_field_set_yreal(tipfield,
                                 gwy_data_field_get_yreal(tipfield)
                                 /yoldres*yres);

    }
}

static void
tip_blind_run(TipBlindControls *controls,
              TipBlindArgs *args,
              gboolean full)
{
    GwyDataField *tipfield, *surface;
    GwyContainer *data;
    GwyPixmapLayer *layer;
    const guchar *name;
    gint count = -1;

    data = args->data;
    surface = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                              "/0/data"));
    tipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->tip,
                                                               "/0/data"));

    gwy_app_wait_start(controls->dialog, _("Initializing"));

    /* control tip resolution and real/res ratio*/
    prepare_fields(tipfield, surface, args->xres, args->yres);
    if (full) {
        tipfield = gwy_tip_estimate_full(tipfield, surface, args->thresh,
                                         args->use_boundaries,
                                         &count,
                                         gwy_app_wait_set_fraction,
                                         gwy_app_wait_set_message);
        controls->good_tip = (tipfield != NULL && count > 0);
    }
    else {
        gwy_data_field_fill(tipfield, 0);
        tipfield = gwy_tip_estimate_partial(tipfield, surface, args->thresh,
                                            args->use_boundaries,
                                            &count,
                                            gwy_app_wait_set_fraction,
                                            gwy_app_wait_set_message);
        controls->good_tip = (tipfield != NULL && count > 0);
    }
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      GTK_RESPONSE_OK, controls->good_tip);
    gwy_debug("count = %d", count);

    gwy_app_wait_finish();
    tip_update(controls, args);

    if (gwy_container_gis_string_by_name(args->data, "/0/base/palette",
                                         &name)) {
        layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));
        g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
        gwy_layer_basic_set_gradient(GWY_LAYER_BASIC(layer), name);
    }
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}

static void
tip_update(TipBlindControls *controls,
           G_GNUC_UNUSED TipBlindArgs *args)
{
    GwyDataField *tipfield, *vtipfield, *buffer;

    tipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->tip,
                                                               "/0/data"));
    buffer = gwy_data_field_duplicate(tipfield);
    gwy_data_field_resample(buffer, controls->vxres, controls->vyres,
                            GWY_INTERPOLATION_ROUND);

    vtipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->vtip,
                                                                "/0/data"));

    gwy_data_field_copy(buffer, vtipfield, FALSE);
    g_object_unref(buffer);
}

static void
tip_blind_do(TipBlindControls *controls,
             TipBlindArgs *args)
{
    GtkWidget *data_window;
    const guchar *pal;

    if (gwy_container_gis_string_by_name(args->data, "/0/base/palette", &pal))
        gwy_container_set_string_by_name(controls->tip, "/0/base/palette",
                                         g_strdup(pal));

    data_window = gwy_app_data_window_create(controls->tip);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    controls->tipdone = TRUE;
}

static const gchar *xres_key = "/module/tip_blind/xres";
static const gchar *yres_key = "/module/tip_blind/yres";
static const gchar *thresh_key = "/module/tip_blind/threshold";
static const gchar *use_boundaries_key = "/module/tip_blind/use_boundaries";
static const gchar *same_resolution_key = "/module/tip_blind/same_resolution";

static void
tip_blind_sanitize_args(TipBlindArgs *args)
{
    args->xres = CLAMP(args->xres, MIN_RES, MAX_RES);
    args->yres = CLAMP(args->yres, MIN_RES, MAX_RES);
    args->use_boundaries = !!args->use_boundaries;
    args->same_resolution = !!args->same_resolution;
    if (args->same_resolution)
        args->yres = args->xres;
}

static void
tip_blind_load_args(GwyContainer *container,
                    TipBlindArgs *args)
{
    *args = tip_blind_defaults;

    gwy_container_gis_int32_by_name(container, xres_key, &args->xres);
    gwy_container_gis_int32_by_name(container, yres_key, &args->yres);
    gwy_container_gis_double_by_name(container, thresh_key, &args->thresh);
    gwy_container_gis_boolean_by_name(container, use_boundaries_key,
                                      &args->use_boundaries);
    gwy_container_gis_boolean_by_name(container, same_resolution_key,
                                      &args->same_resolution);
    tip_blind_sanitize_args(args);
}

static void
tip_blind_save_args(GwyContainer *container,
                    TipBlindArgs *args)
{
    gwy_container_set_int32_by_name(container, xres_key, args->xres);
    gwy_container_set_int32_by_name(container, yres_key, args->yres);
    gwy_container_set_double_by_name(container, thresh_key, args->thresh);
    gwy_container_set_boolean_by_name(container, use_boundaries_key,
                                      args->use_boundaries);
    gwy_container_set_boolean_by_name(container, same_resolution_key,
                                      args->same_resolution);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
