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
#include <errno.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/inttrans.h>
#include <libprocess/stats.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwydgets.h>
#include <libgwydgets/gwygrapher.h>
#include <app/gwyapp.h>

#define FFTF_1D_RUN_MODES \
    (GWY_RUN_MODAL)

#define MAX_PARAMS 4

typedef enum {
    GWY_FFTF_1D_SUPPRESS_NULL = 0,
    GWY_FFTF_1D_SUPPRESS_NEIGBOURHOOD = 1
} GwyFftf1dSuppressType;

typedef enum {
    GWY_FFTF_1D_VIEW_MARKED = 0,
    GWY_FFTF_1D_VIEW_UNMARKED = 1
} GwyFftf1dViewType;


/* Data for this function. */
typedef struct {
    gboolean is_inverted;
    GwyContainer *original;
    GwyContainer *result;
    GwyContainer *original_vdata;
    GwyContainer *result_vdata;
    GwyFftf1dSuppressType suppress;
    GwyFftf1dViewType view_type;
    GwyInterpolationType interpolation;
    GwyOrientation direction;
    gboolean update;
    GwyDataLine *weights;
    gint original_xres;
    gint original_yres;
} Fftf1dArgs;

typedef struct {
    gint vxres;
    gint vyres;
    GtkWidget *view_original;
    GtkWidget *view_result;
    GtkWidget *type;
    GtkWidget *update;
    GtkWidget *menu_direction;
    GtkWidget *menu_interpolation;
    GtkWidget *menu_suppress;
    GtkWidget *menu_view_type;
    GtkWidget *graph;
    GwyGraphModel *gmodel;
} Fftf1dControls;

static gboolean    module_register          (const gchar *name);
static gboolean    fftf_1d                   (GwyContainer *data,
                                             GwyRunType run);
static gboolean    fftf_1d_dialog            (Fftf1dArgs *args,
                                             GwyContainer *data);
static void        restore_ps               (Fftf1dControls *controls,
                                             Fftf1dArgs *args);
static void        fftf_1d_load_args         (GwyContainer *container,
                                             Fftf1dArgs *args);
static void        fftf_1d_save_args         (GwyContainer *container,
                                             Fftf1dArgs *args);
static void        fftf_1d_sanitize_args     (Fftf1dArgs *args);
static void        fftf_1d_run               (Fftf1dControls *controls,
                        	                 Fftf1dArgs *args);
static void        fftf_1d_do                (Fftf1dControls *controls,
                        	                 Fftf1dArgs *args);
static void        fftf_1d_dialog_abandon    (Fftf1dControls *controls,
                                             Fftf1dArgs *args);
static GtkWidget*  menu_suppress             (GCallback callback,
                                             gpointer cbdata,
                                             GwyFftf1dSuppressType current);
static GtkWidget*  menu_view_type            (GCallback callback,
                                             gpointer cbdata,
                                             GwyFftf1dViewType current);
static void        suppress_changed_cb       (GObject *item,
                                             Fftf1dArgs *args);
static void        view_type_changed_cb      (GObject *item,
                                             Fftf1dArgs *args);
static void        direction_changed_cb      (GObject *item,
                                             Fftf1dArgs *args);
static void        interpolation_changed_cb  (GObject *item,
                                             Fftf1dArgs *args);
static void        update_changed_cb         (GtkToggleButton *button,
					                         Fftf1dArgs *args);
static void        update_view               (Fftf1dControls *controls,
                                             Fftf1dArgs *args);
static void        graph_selected            (GwyGraphArea *area,
                                             Fftf1dArgs *args);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("FFT filtering"),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};


Fftf1dControls *pcontrols;
const gint MAX_PREV = 200;

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo fftf_1d_func_info = {
        "fftf_1d",
        N_("/_Correct Data/1D _FFT filtering..."),
        (GwyProcessFunc)&fftf_1d,
        FFTF_1D_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &fftf_1d_func_info);

    return TRUE;
}

static gboolean
fftf_1d(GwyContainer *data, GwyRunType run)
{
    Fftf1dArgs args;
    gboolean ok;

    g_assert(run & FFTF_1D_RUN_MODES);

    args.interpolation = GWY_INTERPOLATION_BILINEAR;
    args.direction = GTK_ORIENTATION_HORIZONTAL;
    args.suppress = GWY_FFTF_1D_SUPPRESS_NULL;
    fftf_1d_load_args(gwy_app_settings_get(), &args);

    args.is_inverted = FALSE;

    ok = fftf_1d_dialog(&args, data);
    fftf_1d_save_args(gwy_app_settings_get(), &args);

    return FALSE;
}


static gboolean
fftf_1d_dialog(Fftf1dArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *hbox, *vbox;
    Fftf1dControls controls;
    enum {
        RESPONSE_RUN = 1,
        RESPONSE_RESTORE = 2
    };
    gint response, newsize;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    GtkWidget *label;

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

    controls.vxres = 200;
    controls.vyres = 200;
    pcontrols = &controls;

    /*copy data*/
    args->original = gwy_container_duplicate_by_prefix(data,
                                                     "/0/data",
                                                     "/0/base/palette",
                                                     NULL);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->original,
                                                             "/0/data"));
    args->original_xres = dfield->xres;
    args->original_yres = dfield->yres;
    newsize = gwy_data_field_get_fft_res(MAX(dfield->xres, dfield->yres));

    gwy_data_field_resample(dfield, newsize, newsize,
                            args->interpolation);

    args->result = gwy_container_duplicate_by_prefix(args->original,
                                                    "/0/data",
                                                    "/0/base/palette",
                                                    NULL);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->result,
                                                             "/0/data"));
    gwy_data_field_fill(dfield, 0);

    args->original_vdata = gwy_container_duplicate_by_prefix(args->original,
                                                             "/0/data",
                                                             "/0/base/palette",
                                                             NULL);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->original_vdata,
                                                             "/0/data"));
    gwy_data_field_resample(dfield, controls.vxres, controls.vyres,
                            GWY_INTERPOLATION_ROUND);

    args->result_vdata = gwy_container_duplicate_by_prefix(args->result,
                                                             "/0/data",
                                                             "/0/base/palette",
                                                             NULL);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->result_vdata,
                                                             "/0/data"));
    gwy_data_field_resample(dfield, controls.vxres, controls.vyres,
                            GWY_INTERPOLATION_ROUND);

    args->weights = NULL;

    vbox = gtk_vbox_new(FALSE, 3);
    /*set up rescaled image of the surface*/
    controls.view_original = gwy_data_view_new(args->original_vdata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view_original), layer);

    /*set up fit controls*/
    gtk_box_pack_start(GTK_BOX(vbox), controls.view_original, FALSE, FALSE, 4);

    /*set up rescaled image of the result*/
    controls.view_result = gwy_data_view_new(args->result_vdata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view_result), layer);

    gtk_box_pack_start(GTK_BOX(vbox), controls.view_result, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);


    /*settings*/
    vbox = gtk_vbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Power spectrum (select areas by mouse):</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);

    controls.gmodel = GWY_GRAPH_MODEL(gwy_graph_model_new(NULL));
    controls.graph = gwy_grapher_new(GWY_GRAPH_MODEL(controls.gmodel));
    gwy_axiser_set_visible(GWY_GRAPHER(controls.graph)->axis_top, FALSE);
    gwy_axiser_set_visible(GWY_GRAPHER(controls.graph)->axis_left, FALSE);
    gwy_axiser_set_visible(GWY_GRAPHER(controls.graph)->axis_bottom, FALSE);
    gwy_axiser_set_visible(GWY_GRAPHER(controls.graph)->axis_right, FALSE);
    gwy_grapher_set_status(GWY_GRAPHER(controls.graph), GWY_GRAPH_STATUS_XSEL);

    g_signal_connect(GWY_GRAPHER(controls.graph)->area, "selected",
                               G_CALLBACK(graph_selected), args);


    gtk_box_pack_start(GTK_BOX(vbox), controls.graph, FALSE, FALSE, 4);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Settings:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);

    table = gtk_table_new(2, 7, FALSE);

    label = gtk_label_new_with_mnemonic(_("Suppress type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.menu_suppress = menu_suppress(G_CALLBACK(suppress_changed_cb),
                                        args,
                                        args->suppress);

    gtk_table_attach(GTK_TABLE(table), controls.menu_suppress, 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new_with_mnemonic(_("Preview type:"));

    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.menu_view_type = menu_view_type(G_CALLBACK(view_type_changed_cb),
                                        args,
                                        args->view_type);

    gtk_table_attach(GTK_TABLE(table), controls.menu_view_type, 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    gtk_container_add(GTK_CONTAINER(vbox), table);

    controls.menu_interpolation
                = gwy_option_menu_interpolation(G_CALLBACK(interpolation_changed_cb),
                                                          args, args->interpolation);
    gwy_table_attach_row(table, 4, _("_Interpolation type:"), "",
                                                      controls.menu_interpolation);


    controls.menu_direction
        = gwy_option_menu_orientation(G_CALLBACK(direction_changed_cb),
                                      args, args->direction);
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
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            fftf_1d_do(&controls, args);
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

    return TRUE;
}

static void
fftf_1d_dialog_abandon(Fftf1dControls *controls, Fftf1dArgs *args)
{
    g_object_unref(args->original_vdata);
    g_object_unref(args->result_vdata);
}

/*update preview depending on user's wishes*/
static void
update_view(Fftf1dControls *controls, Fftf1dArgs *args)
{
    GwyDataField *dfield, *rfield, *rvfield;
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->original,
                                                             "/0/data"));
    rfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->result,
                                                             "/0/data"));
    rvfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->result_vdata,
                                                             "/0/data"));

    gwy_data_field_resample(rfield, dfield->xres, dfield->yres,
                            args->interpolation);

    gwy_data_field_fft_filter_1d(dfield, rfield, args->weights, args->direction,
                                 args->interpolation);

    gwy_data_field_resample(rfield, rvfield->xres, rvfield->yres,
                            args->interpolation);

    gwy_data_field_copy(rfield, rvfield, FALSE);
    gwy_data_field_data_changed(rvfield);
}

static void
restore_ps(Fftf1dControls *controls, Fftf1dArgs *args)
{
    GwyDataField *dfield;
    GwyDataLine *dline;
    GwyGraphCurveModel *cmodel;
    gdouble xdata[200];
    gint i;

    dline = gwy_data_line_new(MAX_PREV, MAX_PREV, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->original,
                                                             "/0/data"));

    gwy_data_field_get_line_stat_function(dfield, dline,
                                          0, 0, dfield->xres, dfield->yres,
                                          GWY_SF_OUTPUT_PSDF,
                                          args->direction,
                                          args->interpolation,
                                          GWY_WINDOWING_RECT,
                                          MAX_PREV);

    if (!args->weights)
        args->weights = gwy_data_line_new(dline->res, dline->res, FALSE);
    gwy_data_line_fill(args->weights, 1);
    gwy_data_line_resample(dline, MAX_PREV, args->interpolation);

    for (i = 0; i < MAX_PREV; i++)
        xdata[i] = ((gdouble)i)/MAX_PREV;
    gwy_data_line_multiply(dline, 1.0/gwy_data_line_get_max(dline));

    cmodel = GWY_GRAPH_CURVE_MODEL(gwy_graph_curve_model_new());
    cmodel->xdata = xdata;
    cmodel->type = GWY_GRAPH_CURVE_LINE;
    cmodel->ydata = dline->data;
    cmodel->n = MAX_PREV;
    cmodel->description = g_string_new("PSDF");

    gwy_graph_model_remove_all_curves(controls->gmodel);
    gwy_graph_model_add_curve(controls->gmodel, cmodel);
    gwy_grapher_clear_selection(GWY_GRAPHER(controls->graph));

    if (args->update)
        update_view(controls, args);
}

static void
graph_selected(GwyGraphArea *area, Fftf1dArgs *args)
{
    gint i, j, nofselection;
    gdouble beg, end;
    gdouble *selection;

    /*get graph selection*/
    nofselection = gwy_grapher_get_selection_number(GWY_GRAPHER(pcontrols->graph));
    if (nofselection == 0)
    {
        restore_ps(pcontrols, args);
    }
    else
    {
        selection = (gdouble *)g_malloc(2*nofselection*sizeof(gdouble));
        gwy_grapher_get_selection(GWY_GRAPHER(pcontrols->graph), selection);

        /*setup weights for inverse FFT computation*/
        if (args->weights == NULL) args->weights = gwy_data_line_new(MAX_PREV, MAX_PREV, FALSE);

        if (args->view_type == GWY_FFTF_1D_VIEW_UNMARKED)
        {
            gwy_data_line_fill(args->weights, 1);

            for (i = 0; i < nofselection; i++)
            {
                beg = selection[2*i];
                end = selection[2*i+1];
                if (args->suppress == GWY_FFTF_1D_SUPPRESS_NULL)
                    gwy_data_line_part_fill(args->weights,
                                    MAX(0, args->weights->res*beg),
                                    MIN(args->weights->res, args->weights->res*end),
                                    0);
                else /*TODO put there at least some linear interpolation*/
                    gwy_data_line_part_fill(args->weights,
                                            MAX(0, args->weights->res*beg),
                                            MIN(args->weights->res, args->weights->res*end),
                                            0.3);

            }
            if (args->update) update_view(pcontrols, args);
        }
        if (args->view_type == GWY_FFTF_1D_VIEW_MARKED)
        {
            gwy_data_line_fill(args->weights, 0);

            for (i = 0; i < nofselection; i++)
            {
                beg = selection[2*i];
                end = selection[2*i+1];

                gwy_data_line_part_fill(args->weights,
                                        MAX(0, args->weights->res*beg),
                                        MIN(args->weights->res, args->weights->res*end),
                                        1);
            }
            if (args->update) update_view(pcontrols, args);
        }

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
fftf_1d_do(Fftf1dControls *controls,
             Fftf1dArgs *args)
{
    GtkWidget *data_window;
    GwyDataField *rfield;

    rfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->result,
                                                                "/0/data"));
    gwy_data_field_resample(rfield, args->original_xres, args->original_yres,
                            args->interpolation);

    data_window = gwy_app_data_window_create(args->result);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                      _("Filtered data"));

}


/*display mode menu*/
static GtkWidget*
menu_suppress(GCallback callback, gpointer cbdata, GwyFftf1dSuppressType current)
{
    static const GwyEnum entries[] = {
        { N_("Null"),  GWY_FFTF_1D_SUPPRESS_NULL,  },
        { N_("Suppress"),  GWY_FFTF_1D_SUPPRESS_NEIGBOURHOOD,  },
    };
    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "suppress-type", callback, cbdata,
                                  current);
}

/*function type menu*/
static GtkWidget*
menu_view_type(GCallback callback, gpointer cbdata, GwyFftf1dViewType current)
{
    static const GwyEnum entries[] = {
        { N_("Marked"),  GWY_FFTF_1D_VIEW_MARKED,  },
        { N_("Unmarked"),  GWY_FFTF_1D_VIEW_UNMARKED,  },
    };
    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "view-type", callback, cbdata,
                                  current);

}


static void
update_changed_cb(GtkToggleButton *button, Fftf1dArgs *args)
{
    args->update = gtk_toggle_button_get_active(button);
}

static void
suppress_changed_cb(GObject *item, Fftf1dArgs *args)
{
    args->suppress = GPOINTER_TO_INT(g_object_get_data(item, "suppress-type"));
    if (args->suppress == GWY_FFTF_1D_SUPPRESS_NEIGBOURHOOD)
    {
        args->view_type = GWY_FFTF_1D_VIEW_UNMARKED;
        gwy_option_menu_set_history(pcontrols->menu_view_type,
                                    "view-type",
                                    args->view_type);
        gtk_widget_set_sensitive(pcontrols->menu_view_type, FALSE);
    }
    else
        gtk_widget_set_sensitive(pcontrols->menu_view_type, TRUE);
    graph_selected(GWY_GRAPHER_AREA(GWY_GRAPHER(pcontrols->graph)->area), args);
    update_view(pcontrols, args);
}

static void
view_type_changed_cb(GObject *item, Fftf1dArgs *args)
{
    args->view_type = GPOINTER_TO_INT(g_object_get_data(item, "view-type"));
    graph_selected(GWY_GRAPHER_AREA(GWY_GRAPHER(pcontrols->graph)->area), args);
    update_view(pcontrols, args);
}

static void
direction_changed_cb(GObject *item, Fftf1dArgs *args)
{
    args->direction = GPOINTER_TO_INT(g_object_get_data(item,
                                                        "orientation-type"));
    restore_ps(pcontrols, args);
}

static void
interpolation_changed_cb(GObject *item, Fftf1dArgs *args)
{
    args->interpolation = GPOINTER_TO_INT(g_object_get_data(item, "interpolation-type"));
    update_view(pcontrols, args);
}



static const gchar *suppress_key = "/module/fftf_1d/suppress";
static const gchar *update_key = "/module/fftf_1d/update";
static const gchar *view_key = "/module/fftf_1d/view";
static const gchar *direction_key = "/module/fftf_1d/direction";
static const gchar *interpolation_key = "/module/fftf_1d/interpolation";

static void
fftf_1d_sanitize_args(Fftf1dArgs *args)
{
    args->suppress = MIN(args->suppress, GWY_FFTF_1D_SUPPRESS_NEIGBOURHOOD);
    args->view_type = MIN(args->view_type, GWY_FFTF_1D_VIEW_UNMARKED);
    args->direction = MIN(args->direction, GTK_ORIENTATION_VERTICAL);
    args->interpolation = MIN(args->interpolation, GWY_INTERPOLATION_NNA);
    args->update = !!args->update;

    if (args->suppress == GWY_FFTF_1D_SUPPRESS_NEIGBOURHOOD)
       args->view_type = GWY_FFTF_1D_VIEW_UNMARKED;

}

static void
fftf_1d_load_args(GwyContainer *container,
                    Fftf1dArgs *args)
{
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




