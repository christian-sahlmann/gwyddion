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
#include <libprocess/inttrans.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define FFTF_1D_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define MAX_PARAMS 4

typedef enum {
    GWY_FFTF_1D_SUPPRESS_NULL         = 0,
    GWY_FFTF_1D_SUPPRESS_NEIGBOURHOOD = 1
} GwyFftf1dSuppressType;

typedef enum {
    GWY_FFTF_1D_VIEW_MARKED   = 0,
    GWY_FFTF_1D_VIEW_UNMARKED = 1
} GwyFftf1dViewType;

enum { 
    PREVIEW_SIZE = 200,
    MAX_PREV = 200 
};

typedef struct {
    /*
    GwyContainer *data;
    GwyContainer *original;
    GwyContainer *result;
    GwyContainer *original_vdata;
    GwyContainer *result_vdata;
    */
    GwyFftf1dSuppressType suppress;
    GwyFftf1dViewType view_type;
    GwyInterpolationType interpolation;
    GwyOrientation direction;
    gboolean update;
    //GwyDataLine *weights;
    //gint original_xres;
    //gint original_yres;
} Fftf1dArgs;

typedef struct {
    GtkWidget *view_original;
    GtkWidget *view_result;
    GtkWidget *type;
    GtkWidget *update;
    GtkWidget *menu_direction;
    GtkWidget *menu_interpolation;
    GtkWidget *menu_suppress;
    GtkWidget *menu_view_type;
    GtkWidget *graph;
    GwyDataLine *weights;
    GwyGraphModel *gmodel;
    GwyContainer *original_data;
    GwyContainer *result_data;
    GwyDataField *original_field;
    Fftf1dArgs *args;
} Fftf1dControls;

static gboolean   module_register         (void);
static void       fftf_1d                 (GwyContainer *data,
                                           GwyRunType run);
static void       fftf_1d_dialog          (Fftf1dArgs *args,
                                           GwyContainer *data,
                                           GwyDataField *dfield,
                                           gint id);
static void       restore_ps              (Fftf1dControls *controls,
                                           Fftf1dArgs *args);
static void       fftf_1d_load_args       (GwyContainer *container,
                                           Fftf1dArgs *args);
static void       fftf_1d_save_args       (GwyContainer *container,
                                           Fftf1dArgs *args);
static void       fftf_1d_sanitize_args   (Fftf1dArgs *args);
static void       fftf_1d_run             (Fftf1dControls *controls,
                                           Fftf1dArgs *args);
static void       fftf_1d_do              (Fftf1dControls *controls);
static void       fftf_1d_dialog_abandon  (Fftf1dControls *controls,
                                           Fftf1dArgs *args);
static void       suppress_changed_cb     (GtkWidget *combo,
                                           Fftf1dControls *controls);
static void       view_type_changed_cb    (GtkWidget *combo,
                                           Fftf1dControls *controls);
static void       direction_changed_cb    (GtkWidget *combo,
                                           Fftf1dControls *controls);
static void       interpolation_changed_cb(GtkWidget *combo,
                                           Fftf1dControls *controls);
static void       update_changed_cb       (GtkToggleButton *button,
                                           Fftf1dArgs *args);
static void       update_view             (Fftf1dControls *controls,
                                           Fftf1dArgs *args);
static void       graph_selected          (GwySelection* selection,
                                           gint i,
                                           Fftf1dControls *controls);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("FFT filtering"),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fft_filter_1d",
                              (GwyProcessFunc)&fftf_1d,
                              N_("/_Correct Data/1D _FFT filtering..."),
                              NULL,
                              FFTF_1D_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("1D FFT Filtering"));
    return TRUE;
}

static void
fftf_1d(GwyContainer *data, GwyRunType run)
{
    Fftf1dArgs args;
    GwyDataField *dfield;
    gint id;

    g_return_if_fail(run & FFTF_1D_RUN_MODES);

    fftf_1d_load_args(gwy_app_settings_get(), &args);
    
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);
    
    fftf_1d_dialog(&args, data, dfield, id);
    fftf_1d_save_args(gwy_app_settings_get(), &args);
}

static void
fftf_1d_dialog(Fftf1dArgs *args, GwyContainer *data, GwyDataField *dfield, gint id)
{
    static const GwyEnum view_types[] = {
        { N_("Marked"),    GWY_FFTF_1D_VIEW_MARKED,    },
        { N_("Unmarked"),  GWY_FFTF_1D_VIEW_UNMARKED,  },
    };
    static const GwyEnum suppress_types[] = {
        { N_("Null"),      GWY_FFTF_1D_SUPPRESS_NULL,         },
        { N_("Suppress"),  GWY_FFTF_1D_SUPPRESS_NEIGBOURHOOD, },
    };
    
    GtkWidget *dialog, *table, *hbox, *vbox;
    Fftf1dControls controls;
    GwyDataField *result_field;
    GwyGraphArea *area;
    GwySelection *selection;

    enum {
        RESPONSE_RUN = 1,
        RESPONSE_RESTORE = 2
    };
    
    gint response;
    GwyPixmapLayer *layer;
    GtkWidget *label;
    gdouble zoomval;

    dialog = gtk_dialog_new_with_buttons(_("1D FFT filter"), NULL, 0,
                                         _("_Run"), RESPONSE_RUN,
                                         _("_Restore PS"), RESPONSE_RESTORE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);


    hbox = gtk_hbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    /* store pointer to data container */
    controls.args = args;
    controls.original_data = data;
    controls.original_field = dfield;

    /*setup original container*/
    /*controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", gwy_data_field_duplicate(dfield));

    dfield = gwy_data_field_duplicate(orig_dfield);
    args->original_xres = dfield->xres;
    args->original_yres = dfield->yres;
    newsize = gwy_fft_find_nice_size(MAX(dfield->xres, dfield->yres));
    gwy_data_field_resample(dfield, newsize, newsize, args->interpolation);
    args->original = gwy_container_new();
    gwy_container_set_object_by_name(args->original, "/0/data", dfield);
    gwy_app_copy_data_items(data, args->original, oldid, 0,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    g_object_unref(dfield);
*/
    /*setup result container*/
    result_field = gwy_data_field_new_alike(dfield, TRUE);
    controls.result_data = gwy_container_new();
    gwy_container_set_object_by_name(controls.result_data, "/0/data", result_field);
    gwy_app_sync_data_items(data, controls.result_data, id, 0, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE,
                            0);
    g_object_unref(result_field);

    controls.weights = NULL;

    vbox = gtk_vbox_new(FALSE, 3);
    /*set up rescaled image of the surface*/
    controls.view_original = gwy_data_view_new(controls.original_data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, g_quark_to_string(gwy_app_get_data_key_for_id(id)));
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view_original), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                            gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view_original), zoomval);


    /*set up fit controls*/
    gtk_box_pack_start(GTK_BOX(vbox), controls.view_original, FALSE, FALSE, 4);

    /*set up rescaled image of the result*/
    controls.view_result = gwy_data_view_new(controls.result_data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view_result), layer);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view_result), zoomval);


    gtk_box_pack_start(GTK_BOX(vbox), controls.view_result, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);


    /*settings*/
    vbox = gtk_vbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    label = gtk_label_new(NULL);
    /* `select areas with mouse' should be a tooltip or something...
    gtk_label_set_markup(GTK_LABEL(label),
                         _("<b>Power spectrum (select areas by mouse):</b>"));
                         */
    label = gtk_label_new(_("Power Spectrum"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);

    controls.gmodel = gwy_graph_model_new();
    controls.graph = gwy_graph_new(controls.gmodel);
    gwy_graph_set_status(GWY_GRAPH(controls.graph), GWY_GRAPH_STATUS_XSEL);

    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls.graph)));
    selection = gwy_graph_area_get_selection(area, GWY_GRAPH_STATUS_XSEL);

    gwy_selection_set_max_objects(selection, 1);
    g_signal_connect(selection, "changed",
                     G_CALLBACK(graph_selected), &controls);


    gtk_box_pack_start(GTK_BOX(vbox), controls.graph, FALSE, FALSE, 4);

    label = gtk_label_new(_("Options"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);

    table = gtk_table_new(2, 7, FALSE);

    label = gtk_label_new_with_mnemonic(_("_Suppress type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.menu_suppress
        = gwy_enum_combo_box_new(suppress_types, G_N_ELEMENTS(suppress_types),
                                 G_CALLBACK(suppress_changed_cb), &controls,
                                 args->suppress, TRUE);

    gtk_table_attach(GTK_TABLE(table), controls.menu_suppress, 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new_with_mnemonic(_("_Preview type:"));

    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.menu_view_type
        = gwy_enum_combo_box_new(view_types, G_N_ELEMENTS(view_types),
                                 G_CALLBACK(view_type_changed_cb), &controls,
                                 args->view_type, TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.menu_view_type, 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    gtk_container_add(GTK_CONTAINER(vbox), table);

    controls.menu_interpolation
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(interpolation_changed_cb), &controls,
                                 args->interpolation, TRUE);
    gwy_table_attach_row(table, 4, _("_Interpolation type:"), "",
                         controls.menu_interpolation);


    controls.menu_direction
        = gwy_enum_combo_box_new(gwy_orientation_get_enum(), -1,
                                 G_CALLBACK(direction_changed_cb), &controls,
                                 args->direction, TRUE);
    gwy_table_attach_row(table, 5, _("_Direction:"), "",
                         controls.menu_direction);

    controls.update
               = gtk_check_button_new_with_mnemonic(_("_Update dynamically"));

    gtk_table_attach(GTK_TABLE(table), controls.update, 0, 4, 6, 7,
                      GTK_EXPAND | GTK_FILL, 0, 2, 2);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);

    g_signal_connect(controls.update, "toggled",
                     G_CALLBACK(update_changed_cb), args);

    args->update = 0;
    restore_ps(&controls, args);
    update_view(&controls, args);

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
            fftf_1d_do(&controls);
            break;

            case RESPONSE_RUN:
            fftf_1d_run(&controls, args);
            break;

            case RESPONSE_RESTORE:
            restore_ps(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    fftf_1d_dialog_abandon(&controls, args);

}

static void
fftf_1d_dialog_abandon(Fftf1dControls *controls,
                       G_GNUC_UNUSED Fftf1dArgs *args)
{
    g_object_unref(controls->result_data);
}

/*update preview depending on user's wishes*/
static void
update_view(Fftf1dControls *controls,
            Fftf1dArgs *args)
{
    GwyDataField *rfield;
    rfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->result_data,
                                                             "/0/data"));

    gwy_data_field_fft_filter_1d(controls->original_field, rfield, controls->weights, args->direction,
                                 args->interpolation);

    gwy_data_field_data_changed(rfield);
}

static void
restore_ps(Fftf1dControls *controls, Fftf1dArgs *args)
{
    GwyDataField *dfield;
    GwyDataLine *dline;
    GwyGraphCurveModel *cmodel;
    //gdouble xdata[200];
    //gint i
    GwySelection *selection;
    gint nofselection;

    dline = gwy_data_line_new(MAX_PREV, MAX_PREV, FALSE);
    dfield = controls->original_field;

    gwy_data_field_psdf(dfield, dline, args->direction, args->interpolation,
                        GWY_WINDOWING_RECT, MAX_PREV);
    if (!controls->weights)
        controls->weights = gwy_data_line_new(dline->res, dline->real, FALSE);
    gwy_data_line_fill(controls->weights, 1);
 //   gwy_data_line_resample(dline, MAX_PREV, args->interpolation);

//    for (i = 0; i < MAX_PREV; i++)
//        xdata[i] = ((gdouble)i)/MAX_PREV;
//    gwy_data_line_multiply(dline, 1.0/gwy_data_line_get_max(dline));

    //for (i = 0; i < dline->res; i++) printf("%g\n", dline->data[i]);

    gwy_graph_model_remove_all_curves(controls->gmodel);

    cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_data_from_dataline(cmodel, dline, 0, 0);
    //gwy_graph_curve_model_set_curve_type(cmodel, GWY_GRAPH_CURVE_LINE);
    //gwy_graph_curve_model_set_description(cmodel, "PSDF");

    gwy_graph_model_add_curve(controls->gmodel, cmodel);

    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(
                                                   gwy_graph_get_area(GWY_GRAPH(controls->graph))), 
                                             GWY_GRAPH_STATUS_XSEL);
    
    nofselection = gwy_selection_get_data(selection, NULL);
    
    if (nofselection != 0)
        gwy_selection_clear(selection);

    if (args->update)
        update_view(controls, args);
}

static void
graph_selected(GwySelection* selection,
               G_GNUC_UNUSED gint i,
               Fftf1dControls *controls)
{
    gint k, nofselection;
    gdouble beg, end;
    gdouble *selection_data;
    gint fill_from, fill_to;

    nofselection = gwy_selection_get_data(selection, NULL);
     if (nofselection == 0) {
        restore_ps(controls, controls->args);
    }
    else {
        selection_data = (gdouble *)g_malloc(2*nofselection*sizeof(gdouble));
        gwy_selection_get_data(selection, selection_data);

        /*setup weights for inverse FFT computation*/
        if (controls->weights == NULL)
            controls->weights = gwy_data_line_new(MAX_PREV, MAX_PREV, FALSE);

        if (controls->args->view_type == GWY_FFTF_1D_VIEW_UNMARKED) {
            gwy_data_line_fill(controls->weights, 1);

            for (k = 0; k < nofselection; k++) {
                beg = selection_data[2*k];
                end = selection_data[2*k+1];
                fill_from = MAX(0, gwy_data_line_rtoi(controls->weights, beg));
                fill_from = MIN(controls->weights->res, fill_from);
                fill_to = MIN(controls->weights->res,
                              gwy_data_line_rtoi(controls->weights, end));

                if (controls->args->suppress == GWY_FFTF_1D_SUPPRESS_NULL)
                    gwy_data_line_part_fill(controls->weights,
                                            fill_from, fill_to, 0);
                else /*TODO put here at least some linear interpolation*/
                    gwy_data_line_part_fill(controls->weights,
                                            fill_from, fill_to, 0.3);
            }
            if (controls->args->update) update_view(controls, controls->args);
        }
        if (controls->args->view_type == GWY_FFTF_1D_VIEW_MARKED) {
            gwy_data_line_fill(controls->weights, 0);

            for (k = 0; k < nofselection; k++) {
                beg = selection_data[2*k];
                end = selection_data[2*k+1];

                fill_from = MAX(0, gwy_data_line_rtoi(controls->weights, beg));
                fill_from = MIN(controls->weights->res, fill_from);
                fill_to = MIN(controls->weights->res,
                              gwy_data_line_rtoi(controls->weights, end));

                gwy_data_line_part_fill(controls->weights, fill_from, fill_to, 1);
            }

            if (controls->args->update)
                update_view(controls, controls->args);
        }
        g_free(selection_data);
    }
}


/*fit data*/
static void
fftf_1d_run(Fftf1dControls *controls,
            Fftf1dArgs *args)
{
    update_view(controls, args);
}

/*dialog finished, export result data*/
static void
fftf_1d_do(Fftf1dControls *controls)
{
    GwyDataField *rfield;
    gint newid;

    rfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->result_data,
                            "/0/data"));
    newid = gwy_app_data_browser_add_data_field(rfield, controls->original_data, TRUE);
    gwy_app_sync_data_items(controls->result_data, controls->original_data, 0, newid,
                            GWY_DATA_ITEM_GRADIENT,
                            0);
    gwy_app_set_data_field_title(controls->original_data, newid, _("1D FFT Filtered Data"));
}

static void
update_changed_cb(GtkToggleButton *button, Fftf1dArgs *args)
{
    args->update = gtk_toggle_button_get_active(button);
}

static void
suppress_changed_cb(GtkWidget *combo,
                    Fftf1dControls *controls)
{
    GwyGraphArea *area;
    GwySelection *selection;
    
    controls->args->suppress = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (controls->args->suppress == GWY_FFTF_1D_SUPPRESS_NEIGBOURHOOD) {
        controls->args->view_type = GWY_FFTF_1D_VIEW_UNMARKED;
        gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->menu_view_type),
                                      controls->args->view_type);
        gtk_widget_set_sensitive(controls->menu_view_type, FALSE);
    }
    else
        gtk_widget_set_sensitive(controls->menu_view_type, TRUE);

    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls->graph)));
    selection = gwy_graph_area_get_selection(area, GWY_GRAPH_STATUS_XSEL);

    graph_selected(selection, 0, controls);
    update_view(controls, controls->args);
}

static void
view_type_changed_cb(GtkWidget *combo,
                     Fftf1dControls *controls)
{
    GwyGraphArea *area;
    GwySelection *selection;
    
    controls->args->view_type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls->graph)));
    selection = gwy_graph_area_get_selection(area, GWY_GRAPH_STATUS_XSEL);

    graph_selected(selection, 0, controls);
    update_view(controls, controls->args);
}

static void
direction_changed_cb(GtkWidget *combo,
                     Fftf1dControls *controls)
{
    controls->args->direction = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    restore_ps(controls, controls->args);
}

static void
interpolation_changed_cb(GtkWidget *combo,
                         Fftf1dControls *controls)
{
    controls->args->interpolation = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    update_view(controls, controls->args);
}

static const gchar suppress_key[]      = "/module/fft_filter_1d/suppress";
static const gchar update_key[]        = "/module/fft_filter_1d/update";
static const gchar view_key[]          = "/module/fft_filter_1d/view";
static const gchar direction_key[]     = "/module/fft_filter_1d/direction";
static const gchar interpolation_key[] = "/module/fft_filter_1d/interpolation";

static void
fftf_1d_sanitize_args(Fftf1dArgs *args)
{
    args->suppress = MIN(args->suppress, GWY_FFTF_1D_SUPPRESS_NEIGBOURHOOD);
    args->view_type = MIN(args->view_type, GWY_FFTF_1D_VIEW_UNMARKED);
    args->direction = MIN(args->direction, GWY_ORIENTATION_VERTICAL);
    args->interpolation = MIN(args->interpolation, GWY_INTERPOLATION_NNA);
    args->update = !!args->update;

    if (args->suppress == GWY_FFTF_1D_SUPPRESS_NEIGBOURHOOD)
        args->view_type = GWY_FFTF_1D_VIEW_UNMARKED;

}

static void
fftf_1d_load_args(GwyContainer *container,
                  Fftf1dArgs *args)
{
    args->interpolation = GWY_INTERPOLATION_BILINEAR;
    args->direction = GWY_ORIENTATION_HORIZONTAL;
    args->suppress = GWY_FFTF_1D_SUPPRESS_NULL;

    gwy_container_gis_enum_by_name(container, suppress_key, &args->suppress);
    gwy_container_gis_enum_by_name(container, view_key, &args->view_type);
    gwy_container_gis_enum_by_name(container, direction_key, &args->direction);
    gwy_container_gis_enum_by_name(container, interpolation_key, &args->interpolation);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);

    fftf_1d_sanitize_args(args);
}

static void
fftf_1d_save_args(GwyContainer *container,
                    Fftf1dArgs *args)
{
    gwy_container_set_enum_by_name(container, suppress_key, args->suppress);
    gwy_container_set_enum_by_name(container, view_key, args->view_type);
    gwy_container_set_enum_by_name(container, direction_key, args->direction);
    gwy_container_set_enum_by_name(container, interpolation_key, args->interpolation);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */




