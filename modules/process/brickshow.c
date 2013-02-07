/*
 *  @(#) $Id: brickshow.c 13809 2012-09-19 18:19:21Z yeti-dn $
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <cairo.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/brick.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define BRICKSHOW_RUN_MODES (GWY_RUN_INTERACTIVE)
#define MAXPIX 600
#define MAXSIMPLIFY 5

typedef enum {
    CUT_DIRX = 0,
    CUT_DIRY = 1,
    CUT_DIRZ = 2,
    PROJ_DIRX = 3,
    PROJ_DIRY = 4,
    PROJ_DIRZ = 5,
} GwyDirType;

typedef enum {
    GRAPH_DIRX = 0,
    GRAPH_DIRY = 1,
    GRAPH_DIRZ = 2,
} GwyGDirType;

enum {
    PREVIEW_SIZE = 400,
    MAX_LENGTH = 1024
};

enum {
    RESPONSE_RESET   = 1,
    RESPONSE_PREVIEW = 2,
    RESPONSE_LOAD    = 3
};

typedef struct {
    GwyDirType type;
    GwyGDirType gtype;
    gdouble xpos;
    gdouble ypos;
    gdouble zpos;
    gboolean update;
    gint active_page;
    gboolean perspective;
    gdouble size;
    gdouble zscale;
    gdouble threshold;
} BrickshowArgs;

typedef struct {
    BrickshowArgs *args;
    GtkWidget *type;
    GtkWidget *gtype;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *gview;
    GtkWidget *info;
    GtkWidget *ginfo;
    GtkObject *xpos;
    GtkObject *ypos;
    GtkObject *zpos;
    GtkObject *size;
    GtkObject *zscale;
    GtkWidget *update;
    GtkWidget *gupdate;
    GtkWidget *drawarea;
    GtkWidget *graph;
    GtkWidget *perspective;
    GtkObject *threshold;
    GwyGraphModel *gmodel;
    GwyContainer *mydata;
    GwyContainer *data;
    GwyBrick *brick;
    gboolean computed;
    gboolean gcomputed;
    gboolean in_init;
    gint gcol;
    gint grow;
    gint glev;
    gdouble rpx;
    gdouble rpy;
    gdouble rm[3][3];
    gdouble *px;
    gdouble *py;
    gdouble *pz;
    gdouble *ps;
    gdouble *wpx;
    gdouble *wpy;
    gdouble *wpz;
    gdouble xangle;
    gdouble yangle;
    gdouble zangle;
    gdouble bwidth;
    gdouble bheight;
    gdouble bdepth;
    gint nps;
} BrickshowControls;

static gboolean module_register                    (void);
static void     brickshow                         (GwyContainer *data,
                                                    GwyRunType run);
static void     brickshow_dialog                  (BrickshowArgs *args,
                                                    GwyContainer *data,
                                                    GwyDataField *original_dfield,
                                                    gint id);

static void     brickshow_dialog_update_controls  (BrickshowControls *controls,
                                                    BrickshowArgs *args);
static void     brickshow_dialog_update_values    (BrickshowControls *controls,
                                                    BrickshowArgs *args);
static void     brickshow_load_data               (BrickshowControls *controls,
                                                    BrickshowArgs *args);
static void     brickshow_invalidate                   (BrickshowControls *controls);
static void     update_change_cb                   (BrickshowControls *controls);
static void     gupdate_change_cb                   (BrickshowControls *controls);

static void     preview                            (BrickshowControls *controls,
                                                    BrickshowArgs *args);
static void     brickshow_load_args               (GwyContainer *container,
                                                    BrickshowArgs *args);
static void     brickshow_save_args               (GwyContainer *container,
                                                    BrickshowArgs *args);
static void     type_changed_cb                   (GtkWidget *combo, 
                                                   BrickshowControls *controls);
static void     gtype_changed_cb                   (GtkWidget *combo, 
                                                   BrickshowControls *controls);
static void     page_switched                      (BrickshowControls *controls,
                                                    GtkNotebookPage *page,
                                                    gint pagenum);
static void     graph_selection_finished_cb        (GwySelection *selection,
                                                    BrickshowControls *controls);
static void     p3d_build                           (BrickshowControls *controls,
                                                     BrickshowArgs *args);
static void     p3d_prepare_wdata                   (BrickshowControls *controls,
                                                     BrickshowArgs *args);
static void     brickshow_zscale_cb                (BrickshowControls *controls);
static void     brickshow_threshold_cb                (BrickshowControls *controls);
//static gboolean p3d_on_draw_event                  (GtkWidget *widget, 
//                                                   cairo_t *cr, 
//                                                   BrickshowControls *controls);
static gboolean p3d_on_draw_event                  (GtkWidget *widget, 
                                                    GdkEventExpose *event, 
                                                    BrickshowControls *controls);
static gboolean p3d_clicked                        (GtkWidget *widget, 
                                                   GdkEventButton *event,
                                                   BrickshowControls *controls);
static gboolean p3d_moved                         (GtkWidget *widget, 
                                                   GdkEventMotion *event,
                                                   BrickshowControls *controls);
static void p3d_xview_cb                          (BrickshowControls *controls);
static void p3d_yview_cb                          (BrickshowControls *controls);
static void p3d_zview_cb                          (BrickshowControls *controls);
static void perspective_change_cb                 (BrickshowControls *controls);

static void p3d_set_axes                          (BrickshowControls *controls);
static void p3d_add_wireframe                     (BrickshowControls *controls);

static void create_brick_from_datafield           (BrickshowControls *controls,
                                                   GwyDataField *dfield);

static const BrickshowArgs brickshow_defaults = {
    CUT_DIRX,
    GRAPH_DIRX,
    50,
    50,
    50,
    TRUE,
    0,
    TRUE,
    50,
    100,
    0.5,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Loads and shows a brick (3D volume data)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("brickshow",
                              (GwyProcessFunc)&brickshow,
                              N_("/_Bricks/Load and show..."),
                              NULL,
                              BRICKSHOW_RUN_MODES,
                              0,
                              N_("Load and show 3D data"));

    return TRUE;
}

static void
brickshow(GwyContainer *data, GwyRunType run)
{
    BrickshowArgs args;
    GwyDataField *dfield = NULL;
    gint id;

    g_return_if_fail(run & BRICKSHOW_RUN_MODES);

    brickshow_load_args(gwy_app_settings_get(), &args);
   
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id, 
                                     0);

    brickshow_dialog(&args, data, dfield, id);
    brickshow_save_args(gwy_app_settings_get(), &args);
}


static void
brickshow_dialog(BrickshowArgs *args,
                  GwyContainer *data,
                  GwyDataField *original_dfield,
                  gint id)
{
    static const GwyEnum types[] = {
        { N_("X cross-section"), CUT_DIRX, },
        { N_("Y cross-section"), CUT_DIRY, },
        { N_("Z cross-section"), CUT_DIRZ, },
        { N_("X direction sum"), PROJ_DIRX, },
        { N_("Y direction sum"), PROJ_DIRY, },
        { N_("Z direction sum"), PROJ_DIRZ, },
    };
    static const GwyEnum gtypes[] = {
        { N_("X direction"), GRAPH_DIRX, },
        { N_("Y direction"), GRAPH_DIRY, },
        { N_("Z direction"), GRAPH_DIRZ, },
    };
  
    GtkWidget *dialog, *table, *hbox, *label, *notebook, *button;
    GwyDataField *dfield;
    GwyDataLine *dline;
    BrickshowControls controls;
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    gboolean temp;
    gint row, newid;
    gchar description[50];
    GwyContainer *newdata = NULL;
    GwyVectorLayer *vlayer = NULL;
    GwySelection *selection;

    controls.in_init = TRUE;
    controls.args = args;
    controls.data = data;
    controls.brick = NULL;
    controls.computed = controls.gcomputed = FALSE; 
    controls.args->active_page = 0;
    controls.rm[0][0] = controls.rm[1][1] = controls.rm[2][2] = 1;
    controls.rm[1][0] = controls.rm[2][0] = controls.rm[0][1] = controls.rm[0][2] = 0;
    controls.rm[1][2] = controls.rm[2][1] = 0;
    controls.px = NULL;
    controls.py = NULL;
    controls.pz = NULL;
    controls.ps = NULL;
    controls.wpx = NULL;
    controls.wpy = NULL;
    controls.wpz = NULL;
    controls.xangle = controls.yangle = controls.zangle = 0;
     controls.nps = 0;

    /*if there was a datafield, create a brick from it*/ 
    if (original_dfield) create_brick_from_datafield(&controls, original_dfield);


 
    /*dialogue controls*/

    dialog = gtk_dialog_new_with_buttons(_("Volume data"), NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Load"),
                                                           GTK_STOCK_OPEN),
                                 RESPONSE_LOAD);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                          gwy_stock_like_button_new(_("_Extract projection"), 
                                                    GTK_STOCK_OK), 
                          GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
    controls.dialog = dialog;


    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), notebook, 
                       TRUE, TRUE, 4);
    g_signal_connect_swapped(notebook, "switch-page",
                                 G_CALLBACK(page_switched), &controls);


    /////////////////////////////  projections page ///////////////////////////////////////

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                               hbox,
                               gtk_label_new(_("Projections")));

    controls.mydata = gwy_container_new();
    dfield = gwy_data_field_new(PREVIEW_SIZE, PREVIEW_SIZE, 
                                PREVIEW_SIZE, PREVIEW_SIZE, 
                                TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);

    if (data) gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            0);

    
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(11, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.xpos = gtk_adjustment_new(args->xpos,
                                            0.0, 100, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("X position"), "%",
                            controls.xpos, 0);
    g_signal_connect_swapped(controls.xpos, "value-changed",
                             G_CALLBACK(brickshow_invalidate), &controls);

    controls.ypos = gtk_adjustment_new(args->ypos,
                                            0.0, 100, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Y position"), "%",
                            controls.ypos, 0);
    g_signal_connect_swapped(controls.ypos, "value-changed",
                             G_CALLBACK(brickshow_invalidate), &controls);

    controls.zpos = gtk_adjustment_new(args->zpos,
                                            0.0, 100, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Z position"), "%",
                            controls.zpos, 0);
    g_signal_connect_swapped(controls.zpos, "value-changed",
                             G_CALLBACK(brickshow_invalidate), &controls);


    label = gtk_label_new(_("Shown cut direction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.type
        = gwy_enum_combo_box_new(types, G_N_ELEMENTS(types),
                                 G_CALLBACK(type_changed_cb), &controls,
                                 args->type, TRUE);
    gwy_table_attach_hscale(table, row, _("Show mode:"), NULL,
                            GTK_OBJECT(controls.type), GWY_HSCALE_WIDGET);
    type_changed_cb(controls.type, &controls);
    row++;


    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_change_cb), &controls);
    row++;

    controls.info = gtk_label_new(_("No data loaded"));
    gtk_misc_set_alignment(GTK_MISC(controls.info), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.info,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    /////////////////////////////  graphs page ///////////////////////////////////////

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                               hbox,
                               gtk_label_new(_("Graphs")));

    dfield = gwy_data_field_new(PREVIEW_SIZE, PREVIEW_SIZE, 
                                PREVIEW_SIZE, PREVIEW_SIZE, 
                                TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/1/data", dfield);

    if (data) gwy_app_sync_data_items(data, controls.mydata, id, 1, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            0);


    controls.gview = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/1/data",
                 "gradient-key", "/1/base/palette",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.gview), "/1/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.gview), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.gview), zoomval);

    vlayer = g_object_new(g_type_from_name("GwyLayerPoint"), NULL);
    gwy_vector_layer_set_selection_key(vlayer, "1/select/graph/point");
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.gview), vlayer);
    selection = gwy_vector_layer_ensure_selection(vlayer);
    g_signal_connect(selection, "finished", 
                     G_CALLBACK(graph_selection_finished_cb), &controls);


    gtk_box_pack_start(GTK_BOX(hbox), controls.gview, FALSE, FALSE, 4);

    table = gtk_table_new(11, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.gmodel = gwy_graph_model_new();
    controls.graph = gwy_graph_new(controls.gmodel);
    gtk_widget_set_size_request(controls.graph, 300, 200);
    gtk_table_attach(GTK_TABLE(table), controls.graph,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    row++; 

    
    label = gtk_label_new(_("Graph cut direction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.gtype
        = gwy_enum_combo_box_new(gtypes, G_N_ELEMENTS(gtypes),
                                 G_CALLBACK(gtype_changed_cb), &controls,
                                 args->gtype, TRUE);
    gwy_table_attach_hscale(table, row, _("Show mode:"), NULL,
                            GTK_OBJECT(controls.gtype), GWY_HSCALE_WIDGET);
    gtype_changed_cb(controls.gtype, &controls);
    row++;


    controls.gupdate = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.gupdate),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.gupdate,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.gupdate, "toggled",
                             G_CALLBACK(gupdate_change_cb), &controls);
    row++;

    controls.ginfo = gtk_label_new(_("No data loaded"));
    gtk_misc_set_alignment(GTK_MISC(controls.ginfo), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.ginfo,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;



    /////////////////////////////  3D page ///////////////////////////////////////

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                               hbox,
                               gtk_label_new(_("3D view")));

      controls.drawarea = gtk_drawing_area_new();
    gtk_widget_add_events(controls.drawarea, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);

    g_signal_connect(GTK_DRAWING_AREA(controls.drawarea), "expose-event", //should be "draw" for newer Gtk+
                     G_CALLBACK(p3d_on_draw_event), &controls);  
    g_signal_connect(controls.drawarea, "button-press-event", 
                     G_CALLBACK(p3d_clicked), &controls);
    g_signal_connect(controls.drawarea, "motion-notify-event", 
                     G_CALLBACK(p3d_moved), &controls);


    gtk_box_pack_start(GTK_BOX(hbox), controls.drawarea, FALSE, FALSE, 4);
    gtk_widget_set_size_request(controls.drawarea, PREVIEW_SIZE, PREVIEW_SIZE);


    table = gtk_table_new(11, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.size = gtk_adjustment_new(args->size,
                                            1, 100, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Zoom"), "%",
                            controls.size, 0);
    g_signal_connect_swapped(controls.size, "value-changed",
                             G_CALLBACK(brickshow_invalidate), &controls);
    row++;

    controls.threshold = gtk_adjustment_new(args->threshold,
                                            1, 100, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Wireframe threshold"), "%",
                            controls.threshold, 0);
    g_signal_connect_swapped(controls.threshold, "value-changed",
                             G_CALLBACK(brickshow_threshold_cb), &controls);
    row++;

    controls.zscale = gtk_adjustment_new(args->zscale,
                                            1, 100, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Z scale"), "%",
                            controls.zscale, 0);
    g_signal_connect_swapped(controls.zscale, "value-changed",
                             G_CALLBACK(brickshow_zscale_cb), &controls);
    row++;



    controls.perspective = gtk_check_button_new_with_mnemonic(_("apply perspective"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.perspective),
                                 args->perspective);
    gtk_table_attach(GTK_TABLE(table), controls.perspective,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.perspective, "toggled",
                             G_CALLBACK(perspective_change_cb), &controls);
    row++;


    button = gtk_button_new_with_mnemonic(_("X view"));
    gtk_table_attach(GTK_TABLE(table), button,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(p3d_xview_cb), &controls);

    button = gtk_button_new_with_mnemonic(_("Y view"));
    gtk_table_attach(GTK_TABLE(table), button,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(p3d_yview_cb), &controls);

    button = gtk_button_new_with_mnemonic(_("Z view"));
    gtk_table_attach(GTK_TABLE(table), button,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(p3d_zview_cb), &controls);

    p3d_build(&controls, args);
    p3d_prepare_wdata(&controls, args);

    row++;

    brickshow_invalidate(&controls);
    controls.in_init = FALSE;

    /* show initial preview if instant updates are on */
    if (args->update) {
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls.dialog),
                                          RESPONSE_PREVIEW, FALSE);
        preview(&controls, args);
    }

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            brickshow_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            return;
            break;

            case GTK_RESPONSE_OK:

            break;

            case RESPONSE_LOAD:
            brickshow_load_data(&controls, args);
            break;

            case RESPONSE_RESET:
            temp = args->update;
            *args = brickshow_defaults;
            args->update = temp;
            controls.in_init = TRUE;
            brickshow_dialog_update_controls(&controls, args);
            controls.in_init = FALSE;
            preview(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            brickshow_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    brickshow_dialog_update_values(&controls, args);

    if (response == GTK_RESPONSE_OK)
    {
        if (controls.computed) {
            dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata,
                                                                     "/0/data"));


            if (args->type == CUT_DIRX) 
                g_snprintf(description, sizeof(description), _("X cross-section at x: %d"), 
                           (gint)(args->xpos/100.0*(gwy_brick_get_xres(controls.brick)-1))); 
            else if (args->type == CUT_DIRY) 
                g_snprintf(description, sizeof(description), _("Y cross-section at y: %d"), 
                           (gint)(args->ypos/100.0*(gwy_brick_get_yres(controls.brick)-1))); 
            else if (args->type == CUT_DIRZ) 
                g_snprintf(description, sizeof(description), _("Z cross-section at z: %d"), 
                           (gint)(args->zpos/100.0*(gwy_brick_get_zres(controls.brick)-1))); 
            else if (args->type == PROJ_DIRX) 
                g_snprintf(description, sizeof(description), _("X direction sum")); 
            else if (args->type == PROJ_DIRY) 
                g_snprintf(description, sizeof(description), _("Y direction sum")); 
            else if (args->type == PROJ_DIRZ) 
                g_snprintf(description, sizeof(description), _("Z direction sum")); 

            if (data) {

                newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
                gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                        GWY_DATA_ITEM_GRADIENT,
                                        0);

                gwy_app_set_data_field_title(data, newid, description);

            }
            else { 
                newid = 0;
                newdata = gwy_container_new();
                gwy_container_set_object(newdata, gwy_app_get_data_key_for_id(newid),
                                         dfield);
                gwy_app_data_browser_add(newdata);
                gwy_app_data_browser_reset_visibility(newdata,
                                                      GWY_VISIBILITY_RESET_SHOW_ALL);
                g_object_unref(newdata);
                gwy_app_set_data_field_title(newdata, newid, description);

            }
        }
        if (controls.gcomputed) {
            GwyGraphModel *gmodel;
            GwyGraphCurveModel *gcmodel;


            dline = GWY_DATA_LINE(gwy_container_get_object_by_name(controls.mydata,
                                                                     "/1/graph"));


            gmodel = gwy_graph_model_new();
            gwy_graph_model_set_units_from_data_line(gmodel, dline);

            if (args->gtype == GRAPH_DIRX)
                g_snprintf(description, sizeof(description), _("X graph at y: %d z: %d"), 
                           controls.grow, controls.glev);
            else if (args->gtype == GRAPH_DIRY)
                g_snprintf(description, sizeof(description), _("Y graph at x: %d z: %d"), 
                           controls.gcol, controls.glev);
            else if (args->gtype == GRAPH_DIRZ)
                g_snprintf(description, sizeof(description), _("Z graph at x: %d y: %d"), 
                           controls.gcol, controls.grow);

            g_object_set(gmodel,
                         "title", description,
                         "axis-label-left", _("w"),
                         "axis-label-bottom", "x",
                         NULL);

            gcmodel = gwy_graph_curve_model_new();
            gwy_graph_curve_model_set_data_from_dataline(gcmodel, dline, -1, -1);
            g_object_set(gcmodel, "description", _("Brick graph"), 
                                  "mode", GWY_GRAPH_CURVE_LINE, 
                                  NULL);
            gwy_graph_model_add_curve(gmodel, gcmodel);
            gwy_object_unref(gcmodel);

            if (newdata)
               gwy_app_data_browser_add_graph_model(gmodel, newdata, TRUE);
            else if (data)
               gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
            else {
                newid = 0;
                newdata = gwy_container_new();
                gwy_app_data_browser_add(newdata);
                gwy_app_data_browser_reset_visibility(newdata,
                                                      GWY_VISIBILITY_RESET_SHOW_ALL);
                g_object_unref(newdata);
                gwy_app_data_browser_add_graph_model(gmodel, newdata, TRUE);
            }
            gwy_object_unref(gmodel);
        }
    }

    gtk_widget_destroy(dialog);

    g_object_unref(controls.mydata);
}

static void
page_switched(BrickshowControls *controls,
              G_GNUC_UNUSED GtkNotebookPage *page,
              gint pagenum)
{
    if (controls->in_init)
        return;

    controls->args->active_page = pagenum;
    preview(controls, controls->args);

}

static void
graph_selection_finished_cb(GwySelection *selection,
                            BrickshowControls *controls)
{
    gdouble sel[2];
    gint col, row, lev;
    GwyGraphCurveModel *cmodel;
    GwyDataLine *dline;
    BrickshowArgs *args = controls->args;

    if (!gwy_selection_get_object(selection, 0, sel)) return;

    dline = gwy_data_line_new(1, 1, FALSE);

    if (args->gtype == GRAPH_DIRX) {
        row = controls->grow = gwy_brick_rtoj(controls->brick, sel[0]);
        lev = controls->glev = gwy_brick_rtok(controls->brick, sel[1]);

        gwy_brick_extract_line(controls->brick, dline,
                                0,
                                row,
                                lev,
                                gwy_brick_get_xres(controls->brick),
                                row,
                                lev,       
                                0);
   } else if (args->gtype == GRAPH_DIRY) {
        col = controls->gcol = gwy_brick_rtoi(controls->brick, sel[0]);
        lev = controls->glev = gwy_brick_rtok(controls->brick, sel[1]);

        gwy_brick_extract_line(controls->brick, dline,
                                col,
                                0,
                                lev,
                                col,
                                gwy_brick_get_yres(controls->brick),
                                lev,       
                                0);
   } else if (args->gtype == GRAPH_DIRZ) {
        col = controls->gcol = gwy_brick_rtoi(controls->brick, sel[0]);
        row = controls->grow = gwy_brick_rtoj(controls->brick, sel[1]);

        gwy_brick_extract_line(controls->brick, dline,
                                col,
                                row,
                                0,
                                col,
                                row,
                                gwy_brick_get_zres(controls->brick),       
                                0);
    } 


    gwy_graph_model_remove_all_curves(controls->gmodel);

    cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_data_from_dataline(cmodel, dline, 0, 0);

    g_object_set(cmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "description", "Brick graph",
                 NULL);
    g_object_set(controls->gmodel,
                 /*"si-unit-x", gwy_data_line_get_si_unit_x(dline),
                 "si-unit-x", gwy_data_line_get_si_unit_x(dline),*/
                 "axis-label-bottom", "x",
                 "axis-label-left", "y",
                 NULL);


    gwy_graph_model_add_curve(controls->gmodel, cmodel);
    gwy_container_set_object_by_name(controls->mydata, "/1/graph", dline);
   
    controls->gcomputed = TRUE;

}

static void
type_changed_cb(GtkWidget *combo, BrickshowControls *controls)
{
    controls->args->type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    switch (controls->args->type) {
        case CUT_DIRX:
        gwy_table_hscale_set_sensitive(controls->xpos, TRUE);
        gwy_table_hscale_set_sensitive(controls->ypos, FALSE);
        gwy_table_hscale_set_sensitive(controls->zpos, FALSE);
        break;

        case CUT_DIRY:
        gwy_table_hscale_set_sensitive(controls->xpos, FALSE);
        gwy_table_hscale_set_sensitive(controls->ypos, TRUE);
        gwy_table_hscale_set_sensitive(controls->zpos, FALSE);
        break;

        case CUT_DIRZ:
        gwy_table_hscale_set_sensitive(controls->xpos, FALSE);
        gwy_table_hscale_set_sensitive(controls->ypos, FALSE);
        gwy_table_hscale_set_sensitive(controls->zpos, TRUE);
        break;

        default:
        gwy_table_hscale_set_sensitive(controls->xpos, FALSE);
        gwy_table_hscale_set_sensitive(controls->ypos, FALSE);
        gwy_table_hscale_set_sensitive(controls->zpos, FALSE);
        break;
    }
    brickshow_invalidate(controls);

}

static void
gtype_changed_cb(GtkWidget *combo, BrickshowControls *controls)
{
    controls->args->gtype = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    brickshow_invalidate(controls);

}

static void
brickshow_dialog_update_controls(BrickshowControls *controls,
                                  BrickshowArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xpos),
                             args->xpos);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ypos),
                             args->ypos);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zpos),
                             args->zpos);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->type), args->type);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
}



static void
brickshow_dialog_update_values(BrickshowControls *controls,
                                BrickshowArgs *args)
{
    args->xpos
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->xpos));
    args->ypos
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->ypos));
    args->zpos
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zpos));

    args->update
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));

    args->size
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->size));
}


static void
brickshow_invalidate(BrickshowControls *controls)
{
    if (controls->args->active_page == 0) controls->computed = FALSE;
    else if (controls->args->active_page == 1) controls->gcomputed = FALSE;


    /* create preview if instant updates are on for the rest, 3d is instantly updated always*/
    if (controls->args->active_page == 2 || (controls->args->update && !controls->in_init)) {
        brickshow_dialog_update_values(controls, controls->args);
        preview(controls, controls->args);
    }
}

static void
update_change_cb(BrickshowControls *controls)
{
    controls->args->update
            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->gupdate), controls->args->update);

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);

    if (controls->args->update)
        brickshow_invalidate(controls);
}

static void
gupdate_change_cb(BrickshowControls *controls)
{
    controls->args->update
            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->gupdate));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update), controls->args->update);


    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);

    if (controls->args->update)
        brickshow_invalidate(controls);
}

static void
brickshow_load_data(BrickshowControls *controls,
        BrickshowArgs *args)
{
    GtkWidget *dialog, *msgdialog;
    gchar *filename;
    gchar *line, *text = NULL;
    gsize size;
    gint xres, yres, zres, col, row, lev;
    gdouble *data;
    GError *err = NULL;


    dialog = gtk_file_chooser_dialog_new (N_("Load volume data"),
                                          GTK_WINDOW(controls->dialog),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                          NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        gwy_app_get_current_directory());

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        if (!g_file_get_contents(filename, &text, &size, &err)) {
            msgdialog = gtk_message_dialog_new (GTK_WINDOW(dialog),
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_CLOSE,
                                                N_("Error loading file '%s'"),
                                                filename);
            gtk_dialog_run(GTK_DIALOG(msgdialog));
            gtk_widget_destroy(msgdialog);
            gtk_widget_destroy(dialog);
        } else {
            //printf("load file: %s\n", filename);

            line = gwy_str_next_line(&text);
            g_strstrip(line);
            xres = g_ascii_strtod(line, &line);
            yres = g_ascii_strtod(line, &line);
            zres = g_ascii_strtod(line, &line);

            //printf("brick %d x %d x %d appears to be there\n", xres, yres, zres);

            controls->brick = gwy_brick_new(xres, yres, zres, xres, yres, zres, FALSE);
            data = gwy_brick_get_data(controls->brick);

            line = gwy_str_next_line(&text);
            for (col=0; col<xres; col++)
            {
                for (row=0; row<yres; row++)
                {
                    for (lev=0; lev<zres; lev++) {
                        data[col + row*xres + lev*xres*yres] = g_ascii_strtod(line, &line);
                    }
                }
            }

            //g_free(text); FIXME: something is wrong - this leads to segfault

            gtk_widget_destroy(dialog);

            brickshow_invalidate(controls);

            controls->nps = 0;

            controls->rm[0][0] = controls->rm[1][1] = controls->rm[2][2] = 1;
            controls->rm[1][0] = controls->rm[2][0] = controls->rm[0][1] = controls->rm[0][2] = 0;
            controls->rm[1][2] = controls->rm[2][1] = 0;
            controls->xangle = controls->yangle = controls->zangle = 0;
   
            p3d_build(controls, args);
            p3d_prepare_wdata(controls, args);

        }
        g_free(filename);
    } else {
        gtk_widget_destroy(dialog);

    }

    
}

static void 
create_brick_from_datafield(BrickshowControls *controls, GwyDataField *dfield)
{
    gint xres, yres, zres;
    gint col, row, lev;
    gdouble ratio, *bdata, *ddata;
    gdouble zreal, offset;
    GwyDataField *lowres;
    
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    zres = MAX(xres, yres);

    if ((xres*yres)>(MAXPIX*MAXPIX))
    {
        ratio = (MAXPIX*MAXPIX)/(gdouble)(xres*yres);
        lowres = gwy_data_field_new_alike(dfield, TRUE);
        gwy_data_field_copy(dfield, lowres, TRUE);
        xres *= ratio;
        yres *= ratio;
        gwy_data_field_resample(lowres, xres, yres, GWY_INTERPOLATION_BILINEAR);
    }
    else lowres = dfield;

    zres = MAX(xres, yres);

    offset = gwy_data_field_get_min(lowres);
    zreal = gwy_data_field_get_max(lowres) - offset;
    

    //printf("Data field of res %d %d\n to brick z %d, real %g %g %g\n", xres, yres, zres, xreal, yreal, zreal);

    controls->brick = gwy_brick_new(xres, yres, zres, xres, yres, zres, TRUE);

    ddata = gwy_data_field_get_data(lowres);
    bdata = gwy_brick_get_data(controls->brick);

    for (col=0; col<xres; col++)
    {
        for (row=0; row<yres; row++)
        {
            for (lev=0; lev<zres; lev++) {
                if (ddata[col + xres*row]<(lev*zreal/zres + offset)) bdata[col + xres*row + xres*yres*lev] = 1;
                
            }
        }
    }


}



static void
preview(BrickshowControls *controls,
        BrickshowArgs *args)
{
    GwyDataField  *dfield;
    gchar message[50];
    gdouble zoomval;


    if (!controls->brick) return;

    if (args->active_page == 0) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                                 "/0/data"));

        if (args->type == CUT_DIRX) {
            gwy_brick_extract_plane(controls->brick, dfield,
                                    (gint)(args->xpos/100.0*(gwy_brick_get_xres(controls->brick)-1)), 
                                    0, 
                                    0,
                                    -1,
                                    gwy_brick_get_yres(controls->brick), 
                                    gwy_brick_get_zres(controls->brick), 
                                    0);
        } else if (args->type == CUT_DIRY) {
            gwy_brick_extract_plane(controls->brick, dfield,
                                    0, 
                                    (gint)(args->ypos/100.0*(gwy_brick_get_yres(controls->brick)-1)), 
                                    0,
                                    gwy_brick_get_xres(controls->brick),
                                    -1, 
                                    gwy_brick_get_zres(controls->brick), 
                                    0);
        } else if (args->type == CUT_DIRZ) {
            gwy_brick_extract_plane(controls->brick, dfield,
                                    0, 
                                    0, 
                                    (gint)(args->zpos/100.0*(gwy_brick_get_zres(controls->brick)-1)), 
                                    gwy_brick_get_xres(controls->brick), 
                                    gwy_brick_get_yres(controls->brick), 
                                    -1, 
                                    0);
        } else if (args->type == PROJ_DIRX) {
            gwy_brick_sum_plane(controls->brick, dfield,
                                0, 
                                0, 
                                0,
                                -1,
                                gwy_brick_get_yres(controls->brick), 
                                gwy_brick_get_zres(controls->brick), 
                                0);
        } else if (args->type == PROJ_DIRY) {
            gwy_brick_sum_plane(controls->brick, dfield,
                                0, 
                                0, 
                                0,
                                gwy_brick_get_xres(controls->brick),
                                -1, 
                                gwy_brick_get_zres(controls->brick), 
                                0);
        } else if (args->type == PROJ_DIRZ) {
            gwy_brick_sum_plane(controls->brick, dfield,
                                0, 
                                0, 
                                0, 
                                gwy_brick_get_xres(controls->brick), 
                                gwy_brick_get_yres(controls->brick), 
                                -1, 
                                0);
        }

        g_snprintf(message, sizeof(message), "Shown range %g to %g", 
                   gwy_data_field_get_min(dfield), 
                   gwy_data_field_get_max(dfield));
        gtk_label_set_text(GTK_LABEL(controls->info), message);

        gwy_data_field_data_changed(dfield);
        
        zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                            gwy_data_field_get_yres(dfield));
        gwy_data_view_set_zoom(GWY_DATA_VIEW(controls->view), zoomval);

        controls->computed = TRUE;
    } 
    else if (args->active_page == 1)
    {
       dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                                 "/1/data"));

       if (args->gtype == GRAPH_DIRX) {
           gwy_brick_sum_plane(controls->brick, dfield,
                               0, 
                               0, 
                               0,
                               -1,
                               gwy_brick_get_yres(controls->brick), 
                               gwy_brick_get_zres(controls->brick), 
                               0);
       } else if (args->gtype == GRAPH_DIRY) {
           gwy_brick_sum_plane(controls->brick, dfield,
                               0, 
                               0, 
                               0,
                               gwy_brick_get_xres(controls->brick),
                               -1, 
                               gwy_brick_get_zres(controls->brick), 
                               0);
       } else if (args->gtype == GRAPH_DIRZ) {
           gwy_brick_sum_plane(controls->brick, dfield,
                               0, 
                               0, 
                               0, 
                               gwy_brick_get_xres(controls->brick), 
                               gwy_brick_get_yres(controls->brick), 
                               -1, 
                               0);
       }

       gwy_data_field_data_changed(dfield);
       
       zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                           gwy_data_field_get_yres(dfield));
       gwy_data_view_set_zoom(GWY_DATA_VIEW(controls->gview), zoomval);


    }
    else if (args->active_page == 2)
    {
       gtk_widget_queue_draw(controls->drawarea);
    }
 }




#define CX 200
#define CY 200

static void
convert_3d2d(gdouble x, gdouble y, gdouble z, gdouble *px, gdouble *py, gboolean perspective, gdouble size)
{
    if (perspective) {
        *px = 9*size*(x/(z+4)) + CX;
        *py = 9*size*(y/(z+4)) + CY;
    } else {
        *px = 3*size*x + CX;
        *py = 3*size*y + CY;
    }
}



static void
xrotmatrix(gdouble m[3][3], gdouble theta)
{
    m[0][0] = 1;
    m[1][0] = 0;
    m[2][0] = 0;

    m[0][1] = 0;
    m[1][1] = cos(theta);
    m[2][1] = -sin(theta);

    m[0][2] = 0;
    m[1][2] = sin(theta);
    m[2][2] = cos(theta);
}

static void
yrotmatrix(gdouble m[3][3], gdouble theta)
{
    m[0][0] = cos(theta);
    m[1][0] = 0;
    m[2][0] = sin(theta);

    m[0][1] = 0;
    m[1][1] = 1;
    m[2][1] = 0;

    m[0][2] = -sin(theta);
    m[1][2] = 0;
    m[2][2] = cos(theta);
}

static void
zrotmatrix(gdouble m[3][3], gdouble theta)
{
    m[0][0] = cos(theta);
    m[1][0] = sin(theta);
    m[2][0] = 0;

    m[0][1] = -sin(theta);
    m[1][1] = cos(theta);
    m[2][1] = 0;

    m[0][2] = 0;
    m[1][2] = 0;
    m[2][2] = 1;
}

static void
mmultm(gdouble a[3][3], gdouble b[3][3], gdouble result[3][3])
{
    gint i, j, k;

    for(i = 0; i < 3; i++)
    {
        for(j = 0; j < 3; j++)
        {
            result[i][j] = 0;
        }
    }

    for(i = 0; i < 3; i++)
    {
        for(j = 0; j < 3; j++)
        {
            for(k = 0; k < 3; k++)
            {
                result[i][j] += a[i][k]*b[k][j];
            }
        }
    }
}

static void
mmultv(gdouble m[3][3], gdouble x, gdouble y, gdouble z,
       gdouble *px, gdouble *py, gdouble *pz)
{
    *px = m[0][0]*x + m[0][1]*y + m[0][2]*z;
    *py = m[1][0]*x + m[1][1]*y + m[1][2]*z;
    *pz = m[2][0]*x + m[2][1]*y + m[2][2]*z;
}


/*static gdouble
mdet(gdouble m[3][3])
{
    return (m[0][0] * m[1][1] * m[2][2]) + (m[1][0] * m[2][1] * m[3][2]) + (m[2][0] * m[3][1] * m[4][2])
           - (m[0][2] * m[1][1] * m[2][0]) - (m[1][2] * m[2][1] * m[3][0]) - (m[2][2] * m[3][1] * m[4][0]);
}

static gboolean
minv(gdouble m[3][3], gdouble ret[3][3])
{
    gdouble ddet = 1.0/mdet(m);

    if (det == 0) return FALSE;

    ret[0][0] =  ((m[1][1]*m[2][2])-(m[1][2]*m[2][1]))*ddet;
    ret[0][1] = -((m[1][0]*m[2][2])-(m[1][2]*m[2][0]))*ddet;
    ret[0][2] =  ((m[1][0]*m[2][1])-(m[1][1]*m[2][0]))*ddet;

    ret[1][0] = -((m[0][1]*m[2][2])-(m[0][2]*m[2][1]))*ddet;
    ret[1][1] =  ((m[0][0]*m[2][2])-(m[0][2]*m[2][0]))*ddet;
    ret[1][2] = -((m[0][0]*m[2][1])-(m[0][1]*m[2][0]))*ddet;

    ret[2][0] =  ((m[0][1]*m[1][2])-(m[0][2]*m[1][1]))*ddet;
    ret[2][1] = -((m[0][0]*m[1][2])-(m[0][2]*m[1][0]))*ddet;
    ret[2][2] =  ((m[0][0]*m[1][1])-(m[0][1]*m[1][0]))*ddet;

    return TRUE;
}*/

static gboolean
//p3d_on_draw_event(GtkWidget *widget, cairo_t *cr, 
//              BrickshowControls *controls)
p3d_on_draw_event(GtkWidget *widget, G_GNUC_UNUSED GdkEventExpose *event, BrickshowControls *controls)
{
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(widget));
    gdouble sx, sy;
    gint i;
    
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width (cr, 0.5);

    convert_3d2d(controls->wpx[3], controls->wpy[3], controls->wpz[3], &sx, &sy, controls->args->perspective, controls->args->size);
    cairo_move_to(cr, sx, sy);

    for (i=4; i<controls->nps; i++) {
        convert_3d2d(controls->wpx[i], controls->wpy[i], controls->wpz[i], &sx, &sy, controls->args->perspective, controls->args->size);
        if (controls->ps[i]) cairo_line_to(cr, sx, sy);
        else cairo_move_to(cr, sx, sy);
    }

    /*axes description*/
    cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                                                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, 12.0);

    convert_3d2d(controls->wpx[3], controls->wpy[3], controls->wpz[3], &sx, &sy, controls->args->perspective, controls->args->size);
    if (sx<=200) sx -= 12; else sx += 12;
    cairo_move_to (cr, sx, sy);
    cairo_show_text (cr, "0");

    convert_3d2d(controls->wpx[4], controls->wpy[4], controls->wpz[4], &sx, &sy, controls->args->perspective, controls->args->size);
    if (sx<=200) sx -= 12; else sx += 12;
    cairo_move_to (cr, sx, sy);
    cairo_show_text (cr, "x");

    convert_3d2d(controls->wpx[14], controls->wpy[14], controls->wpz[14], &sx, &sy, controls->args->perspective, controls->args->size);
    if (sx<=200) sx -= 12; else sx += 12;
    cairo_move_to (cr, sx, sy);
    cairo_show_text (cr, "y");

    convert_3d2d(controls->wpx[8], controls->wpy[8], controls->wpz[8], &sx, &sy, controls->args->perspective, controls->args->size);
    if (sx<=200) sx -= 12; else sx += 12;
    cairo_move_to (cr, sx, sy);
    cairo_show_text (cr, "z");


    cairo_stroke(cr);
    cairo_destroy(cr);

    return FALSE;
}

static gboolean 
p3d_clicked(G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event,
                 BrickshowControls *controls)
{

    controls->rpx = event->x;
    controls->rpy = event->y;    
    //gtk_widget_queue_draw(widget);

    return TRUE;
}

static void rotatem(BrickshowControls *controls)
{
    gdouble px, py, pz, im[3][3];
    gint i;

    im[0][0] = controls->rm[0][0]; im[0][1] = controls->rm[1][0]; im[0][2] = controls->rm[2][0];
    im[1][0] = controls->rm[0][1]; im[1][1] = controls->rm[1][1]; im[1][2] = controls->rm[2][1];
    im[2][0] = controls->rm[0][2]; im[2][1] = controls->rm[1][2]; im[2][2] = controls->rm[2][2];

    /*printf("rotation matrix:\n%g %g %g\n%g %g %g\n%g %g %g\n--------------\n",
           controls->rm[0][0], controls->rm[0][1], controls->rm[0][2],
           controls->rm[1][0], controls->rm[1][1], controls->rm[1][2],
           controls->rm[2][0], controls->rm[2][1], controls->rm[2][2]);
*/
    for (i=0; i<controls->nps; i++)
    {
        mmultv(im, controls->wpx[i], controls->wpy[i], controls->wpz[i], &px, &py, &pz);
        controls->wpx[i] = px;
        controls->wpy[i] = py;
        controls->wpz[i] = pz;
    }
}

static void rotate(BrickshowControls *controls, gdouble x, gdouble y, gdouble z)
{
    gdouble rotx[3][3], roty[3][3], rotz[3][3], rotbuf[3][3];
    gdouble px, py, pz;
    gint i;

    xrotmatrix(rotx, x);
    yrotmatrix(roty, y);
    zrotmatrix(rotz, z);

    mmultm(rotx, roty, rotbuf);
    mmultm(rotbuf, rotz, controls->rm);
    
    for (i=0; i<controls->nps; i++)
    {
        mmultv(controls->rm, controls->wpx[i], controls->wpy[i], controls->wpz[i], &px, &py, &pz);
        controls->wpx[i] = px;
        controls->wpy[i] = py;
        controls->wpz[i] = pz;
    }


    
    controls->rm[0][0] = controls->wpx[0];
    controls->rm[0][1] = controls->wpy[0];
    controls->rm[0][2] = controls->wpz[0];

    controls->rm[1][0] = controls->wpx[1];
    controls->rm[1][1] = controls->wpy[1];
    controls->rm[1][2] = controls->wpz[1];

    controls->rm[2][0] = controls->wpx[2];
    controls->rm[2][1] = controls->wpy[2];
    controls->rm[2][2] = controls->wpz[2];


   // printf("total rotation by angle %g %g %g (%g %g %g), rotating by %g %g %g\n", controls->xangle, 
   //        controls->yangle, 
   //        controls->zangle,
   //        controls->wpx[0], controls->wpy[1], controls->wpz[2], x, y, z);

}

static void
brickshow_zscale_cb(BrickshowControls *controls)
{
    controls->args->zscale = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zscale));
    p3d_prepare_wdata(controls, controls->args);
    rotatem(controls);

    preview(controls, controls->args);
}

static void
brickshow_threshold_cb(BrickshowControls *controls)
{
    controls->args->threshold = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold));

    p3d_build(controls, controls->args);
    p3d_prepare_wdata(controls, controls->args);
    rotatem(controls);

    preview(controls, controls->args);
}



static gboolean 
p3d_moved(GtkWidget *widget, GdkEventMotion *event,
                 BrickshowControls *controls)
{
    gdouble diffx, diffy;

    if (((event->state & GDK_BUTTON1_MASK) == GDK_BUTTON1_MASK))
    {
        controls->rm[0][0] = controls->rm[1][1] = controls->rm[2][2] = 1;
        controls->rm[1][0] = controls->rm[2][0] = controls->rm[0][1] = controls->rm[0][2] = 0;
        controls->rm[1][2] = controls->rm[2][1] = 0;

        diffx = event->x - controls->rpx;
        diffy = event->y - controls->rpy;
        controls->rpx = event->x;
        controls->rpy = event->y;

        rotate(controls, -0.02*diffy, 0.02*diffx, 0);

        gtk_widget_queue_draw(widget);
    }

    return TRUE;
}

static void
perspective_change_cb(BrickshowControls *controls)
{
    controls->args->perspective
            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->perspective));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->perspective), controls->args->perspective);

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->perspective);

    gtk_widget_queue_draw(controls->drawarea);
}

static void 
p3d_xview_cb(BrickshowControls *controls)
{

    p3d_prepare_wdata(controls, controls->args);
    rotate(controls, 0, G_PI/2, 0);
    gtk_widget_queue_draw(controls->drawarea);

}

static void 
p3d_yview_cb(BrickshowControls *controls)
{
    p3d_prepare_wdata(controls, controls->args);
    rotate(controls, G_PI/2, 0, 0);
    gtk_widget_queue_draw(controls->drawarea);

}

static void 
p3d_zview_cb(BrickshowControls *controls)
{
    p3d_prepare_wdata(controls, controls->args);
    rotate(controls, 0, 0, G_PI/2);
    gtk_widget_queue_draw(controls->drawarea);

}

static void 
p3d_set_axes(BrickshowControls *controls)
{
    gint i = 0;
    gint max;

    if (controls->px==NULL || controls->nps<21) 
       controls->px = (gdouble *)g_malloc(21*sizeof(gdouble));
    if (controls->py==NULL || controls->nps<21) 
       controls->py = (gdouble *)g_malloc(21*sizeof(gdouble));
    if (controls->pz==NULL || controls->nps<21) 
       controls->pz = (gdouble *)g_malloc(21*sizeof(gdouble));
    if (controls->ps==NULL || controls->nps<21) 
       controls->ps = (gdouble *)g_malloc(21*sizeof(gdouble));

    controls->bwidth = controls->bheight = controls->bdepth = 1;

    if (controls->brick) {
       max = MAX(MAX(gwy_brick_get_xres(controls->brick), gwy_brick_get_yres(controls->brick)), gwy_brick_get_zres(controls->brick));

       controls->bwidth = (gdouble)gwy_brick_get_xres(controls->brick)/(gdouble)max;
       controls->bheight = (gdouble)gwy_brick_get_yres(controls->brick)/(gdouble)max;
       controls->bdepth = (gdouble)gwy_brick_get_zres(controls->brick)/(gdouble)max;
    }

    controls->px[i] = 1; controls->py[i] = 0; controls->pz[i] = 0; controls->ps[i] = 0; i++;
    controls->px[i] = 0; controls->py[i] = 1; controls->pz[i] = 0; controls->ps[i] = 0; i++;
    controls->px[i] = 0; controls->py[i] = 0; controls->pz[i] = 1; controls->ps[i] = 0; i++;

    controls->px[i] = -1; controls->py[i] = -1; controls->pz[i] = -1; controls->ps[i] = 0; i++;
    controls->px[i] = 1; controls->py[i] = -1; controls->pz[i] = -1; controls->ps[i] = 1; i++;
    controls->px[i] = 1; controls->py[i] = 1; controls->pz[i] = -1; controls->ps[i] = 1; i++;
    controls->px[i] = 1; controls->py[i] = 1; controls->pz[i] = 1; controls->ps[i] = 1; i++;
    controls->px[i] = -1; controls->py[i] = 1; controls->pz[i] = 1; controls->ps[i] = 1; i++;
    controls->px[i] = -1; controls->py[i] = -1; controls->pz[i] = 1; controls->ps[i] = 1; i++;
    controls->px[i] = 1; controls->py[i] = -1; controls->pz[i] = 1; controls->ps[i] = 1; i++;
    controls->px[i] = 1; controls->py[i] = -1; controls->pz[i] = -1; controls->ps[i] = 1; i++;
    controls->px[i] = -1; controls->py[i] = -1; controls->pz[i] = -1; controls->ps[i] = 1; i++;
    controls->px[i] = -1; controls->py[i] = -1; controls->pz[i] = 1; controls->ps[i] = 1; i++;
    controls->px[i] = -1; controls->py[i] = -1; controls->pz[i] = -1; controls->ps[i] = 1; i++;
    controls->px[i] = -1; controls->py[i] = 1; controls->pz[i] = -1; controls->ps[i] = 1; i++;
    controls->px[i] = -1; controls->py[i] = 1; controls->pz[i] = 1; controls->ps[i] = 1; i++;
    controls->px[i] = -1; controls->py[i] = 1; controls->pz[i] = -1; controls->ps[i] = 0; i++;
    controls->px[i] = 1; controls->py[i] = 1; controls->pz[i] = -1; controls->ps[i] = 1; i++;
    controls->px[i] = 1; controls->py[i] = 1; controls->pz[i] = 1; controls->ps[i] = 0; i++;
    controls->px[i] = 1; controls->py[i] = -1; controls->pz[i] = 1; controls->ps[i] = 1; 

    for (i=3; i<20; i++) {
        controls->px[i] *= controls->bwidth;
        controls->py[i] *= controls->bheight;
        controls->pz[i] *= controls->bdepth;
    }

    controls->nps = 20;

}

gint simplify(gdouble *px, gdouble *py, gdouble *pz, gdouble *ps, gint nps)
{
    gint i;
    gdouble *nx, *ny, *nz, *ns;
    gint newn;
   

    nx = g_malloc(nps*sizeof(gdouble));
    ny = g_malloc(nps*sizeof(gdouble));
    nz = g_malloc(nps*sizeof(gdouble));
    ns = g_malloc(nps*sizeof(gdouble));
     
   
    for (i=0; i<6; i++) {
        nx[i] = px[i]; ny[i] = py[i]; nz[i] = pz[i]; ns[i] = ps[i];
    }
    newn = 6;

    for (i=6; i<nps; i++)
    {
        if (ps[i] == 0 || !((px[i]-px[i-1]) == (px[i-1]-px[i-2]) 
                             && (py[i]-py[i-1]) == (py[i-1]-py[i-2]) 
                             && (pz[i]-pz[i-1]) == (pz[i-1]-pz[i-2]))) 
        {
            nx[newn] = px[i];
            ny[newn] = py[i];
            nz[newn] = pz[i];
            ns[newn] = ps[i];
            newn++;
        } 
    }

    for (i=0; i<newn; i++) {
        px[i] = nx[i];
        py[i] = ny[i];
        pz[i] = nz[i];
        ps[i] = ns[i];
    }

    return newn;
}

static void     
p3d_build(BrickshowControls *controls, G_GNUC_UNUSED BrickshowArgs *args)
{
    if (controls->brick == NULL) {
        //printf("No brick\n");
        return;
    }


    gwy_app_wait_start(GTK_WINDOW(controls->dialog), _("Building wireframe model..."));
    p3d_set_axes(controls);
    p3d_add_wireframe(controls);
    gwy_app_wait_finish();

}


static void     
p3d_prepare_wdata(BrickshowControls *controls, BrickshowArgs *args)
{
    gint i;

    if (controls->brick == NULL) {
        //printf("No brick\n");
        return;
    }


    for (i=0; i<3; i++) {
        controls->wpx[i] = controls->px[i];
        controls->wpy[i] = controls->py[i];
        controls->wpz[i] = controls->pz[i];
    }

    for (i=3; i<controls->nps; i++) 
    {
        controls->wpx[i] = controls->px[i];
        controls->wpy[i] = controls->py[i];
        controls->wpz[i] = controls->pz[i]*args->zscale/100.0;
    }

}

static gboolean
gothere(gdouble *data, gdouble *vdata, gint xres, gint yres, gint col, gint row, gint dir, gdouble threshold)
{
    if (vdata[col+xres*row] == 1) return FALSE;

    if (row < 1 || row>=(yres-1)) return FALSE;
    if (col < 1 || col>=(xres-1)) return FALSE;


    if (dir == 0) /*y const*/ {
        if (data[col + yres*row]>threshold &&
                                     (data[col-1 + yres*row]<threshold || data[col + yres*(row-1)]<threshold 
                                      || data[col+1 + yres*row]<threshold || data[col + yres*(row+1)]<threshold
                                      || data[col+1 + yres*(row+1)]<threshold || data[col-1 + yres*(row-1)]<threshold
                                      || data[col+1 + yres*(row-1)]<threshold || data[col-1 + yres*(row+1)]<threshold)) return TRUE; 

    } else if (dir == 1) /*y const*/ {
        if (data[col + xres*row]>threshold &&
                                     (data[col-1 + xres*row]<threshold || data[col + xres*(row-1)]<threshold 
                                      || data[col+1 + xres*row]<threshold || data[col + xres*(row+1)]<threshold
                                      || data[col+1 + xres*(row+1)]<threshold || data[col-1 + xres*(row-1)]<threshold
                                      || data[col+1 + xres*(row-1)]<threshold || data[col-1 + xres*(row+1)]<threshold)) return TRUE; 
    } else {
        if (data[col + xres*row]>threshold &&
                                     (data[col-1 + xres*row]<threshold || data[col + xres*(row-1)]<threshold 
                                      || data[col+1 + xres*row]<threshold || data[col + xres*(row+1)]<threshold
                                      || data[col+1 + xres*(row+1)]<threshold || data[col-1 + xres*(row-1)]<threshold
                                      || data[col+1 + xres*(row-1)]<threshold || data[col-1 + xres*(row+1)]<threshold)) return TRUE; 
     }

    vdata[col+xres*row] = 1;
    return FALSE;
 
}

static void
visitme(BrickshowControls *controls, gint *actual_nps, gdouble *data, gdouble *vdata, gint xres, gint yres, gint zres, gint col, gint row, gint dir, gint tval, gboolean *move, gdouble threshold)
{
    /*detect ad add a segment of necessary*/
    //printf("pos %d %d ", col, row);

    /*increase allocation if necessary*/
    if (((*actual_nps)-controls->nps)<1000) {
        (*actual_nps) += 1000;
        controls->px = g_realloc(controls->px, ((*actual_nps)*sizeof(gdouble)));
        controls->py = g_realloc(controls->py, ((*actual_nps)*sizeof(gdouble)));
        controls->pz = g_realloc(controls->pz, ((*actual_nps)*sizeof(gdouble)));
        controls->ps = g_realloc(controls->ps, ((*actual_nps)*sizeof(gdouble)));
    }

    if (dir == 0) /*y const*/ {
        controls->px[controls->nps] = 2*controls->bwidth*(gdouble)tval/(gdouble)xres - controls->bwidth;
        controls->py[controls->nps] = 2*controls->bheight*(gdouble)col/(gdouble)yres - controls->bheight;
        controls->pz[controls->nps] = 2*controls->bdepth*(gdouble)row/(gdouble)zres - controls->bdepth;
    } else if (dir==1) /*y const*/ {
        controls->px[controls->nps] = 2*controls->bwidth*(gdouble)col/(gdouble)xres - controls->bwidth;
        controls->py[controls->nps] = 2*controls->bheight*(gdouble)tval/(gdouble)yres - controls->bheight;
        controls->pz[controls->nps] = 2*controls->bdepth*(gdouble)row/(gdouble)zres - controls->bdepth;
    } else {
        controls->px[controls->nps] = 2*controls->bwidth*(gdouble)col/(gdouble)xres - controls->bwidth;
        controls->py[controls->nps] = 2*controls->bheight*(gdouble)row/(gdouble)yres - controls->bheight;
        controls->pz[controls->nps] = 2*controls->bdepth*(gdouble)tval/(gdouble)zres - controls->bdepth;
    }
    if (*move) {
        controls->ps[controls->nps] = 0; 
        *move = 0;
    }
    else controls->ps[controls->nps] = 1;

    controls->nps += 1;
    vdata[col+xres*row] = 1;

    /*go to neighbor positions*/
    if (gothere(data, vdata, xres, yres, col+1, row, dir, threshold))  
        visitme(controls, actual_nps,data, vdata, xres, yres, zres, col+1, row, dir, tval, move, threshold);
    else if (gothere(data, vdata, xres, yres, col-1, row, dir, threshold))
             visitme(controls, actual_nps, data, vdata, xres, yres, zres, col-1, row, dir, tval, move, threshold);
    else if (gothere(data, vdata, xres, yres, col, row+1, dir, threshold))
             visitme(controls, actual_nps, data, vdata, xres, yres, zres, col, row+1, dir, tval, move, threshold);
    else if (gothere(data, vdata, xres, yres, col, row-1, dir, threshold))
             visitme(controls, actual_nps, data, vdata, xres, yres, zres, col, row-1, dir, tval, move, threshold);
    else if (gothere(data, vdata, xres, yres, col+1, row+1, dir, threshold))
             visitme(controls, actual_nps, data, vdata, xres, yres, zres, col+1, row+1, dir, tval, move, threshold);
    else if (gothere(data, vdata, xres, yres, col-1, row-1, dir, threshold))
             visitme(controls, actual_nps, data, vdata, xres, yres, zres, col-1, row-1, dir, tval, move, threshold);
    else if (gothere(data, vdata, xres, yres, col+1, row-1, dir, threshold))
             visitme(controls, actual_nps, data, vdata, xres, yres, zres, col+1, row-1, dir, tval, move, threshold);
    else if (gothere(data, vdata, xres, yres, col-1, row+1, dir, threshold))
             visitme(controls, actual_nps, data, vdata, xres, yres, zres, col-1, row+1, dir, tval, move, threshold);

}

static void 
p3d_add_wireframe(BrickshowControls *controls)
{
    gint actual_nps = controls->nps;
    GwyDataField *cut, *visited;
    gdouble threshold, *data, *vdata;
    gint col, row, i, spacing = 40;
    gint xres, yres, zres;
    gboolean move;
   
  
    if (controls->brick == NULL) {
        //printf("No brick\n");
        return;
    } 
  
    xres = gwy_brick_get_xres(controls->brick);
    yres = gwy_brick_get_yres(controls->brick);
    zres = gwy_brick_get_zres(controls->brick);
    cut = gwy_data_field_new(1, 1, 1, 1, FALSE);

    visited = gwy_data_field_new(yres, zres, yres, zres, FALSE);

    //printf("brick min %g, max %g\n", gwy_brick_get_min(controls->brick), gwy_brick_get_max(controls->brick));
    threshold = gwy_brick_get_min(controls->brick) 
        + (gwy_brick_get_max(controls->brick) - gwy_brick_get_min(controls->brick))/100.0*controls->args->threshold;

    for (i=0; i<xres; i+=spacing)
    {
        gwy_brick_extract_plane(controls->brick, cut, i, 0, 0, -1, yres, zres, FALSE);
        data = gwy_data_field_get_data(cut);

        gwy_data_field_clear(visited);
        vdata = gwy_data_field_get_data(visited);

        gwy_data_field_threshold(cut, threshold, 0, 1);

        move = 1;

        /*here comes the algorithm*/
        for (col=1; col<yres; col++)
        {
            for (row=1; row<zres; row++)
            {
                move = 1;
                if (gothere(data, vdata, xres, yres, col, row, 0, threshold))
                    visitme(controls, &actual_nps, data, vdata, xres, yres, zres, col, row, 0, i, &move, threshold);
            }
        }
    }

    gwy_data_field_resample(visited, xres, zres, GWY_INTERPOLATION_NONE);

    for (i=0; i<yres; i+=spacing)
    {
        gwy_brick_extract_plane(controls->brick, cut, 0, i, 0, xres, -1, zres, FALSE);
        data = gwy_data_field_get_data(cut);

        gwy_data_field_clear(visited);
        vdata = gwy_data_field_get_data(visited);

        gwy_data_field_threshold(cut, threshold, 0, 1);

        move = 1;

        /*here comes the algorithm*/
        for (col=1; col<xres; col++)
        {
            for (row=1; row<zres; row++)
            {
                    move = 1;
                    if (gothere(data, vdata, xres, yres, col, row, 1, threshold))
                        visitme(controls, &actual_nps, data, vdata, xres, yres, zres, col, row, 1, i, &move, threshold);                
            }
        }
    }

    gwy_data_field_resample(visited, xres, yres, GWY_INTERPOLATION_NONE);

    for (i=0; i<yres; i+=spacing)
    {
        gwy_brick_extract_plane(controls->brick, cut, 0, 0, i, xres, yres, -1, FALSE);
        data = gwy_data_field_get_data(cut);

        gwy_data_field_clear(visited);
        vdata = gwy_data_field_get_data(visited);

        gwy_data_field_threshold(cut, threshold, 0, 1);

        move = 1;

        /*here comes the algorithm*/
        for (col=1; col<xres; col++)
        {
            for (row=1; row<yres; row++)
            {
                    move = 1;
                    if (gothere(data, vdata, xres, yres, col, row, 2, threshold))
                        visitme(controls, &actual_nps, data, vdata, xres, yres, zres, col, row, 2, i, &move, threshold);                
            }
        }
    }
//    printf("we have %d segments at the end\nRunning simplification:\n", controls->nps);

    controls->nps = simplify(controls->px, controls->py, controls->pz, controls->ps, controls->nps);
   

    if (controls->wpx) g_free(controls->wpx);
    if (controls->wpy) g_free(controls->wpy);
    if (controls->wpz) g_free(controls->wpz);

    controls->wpx = (gdouble *) g_malloc(controls->nps*sizeof(gdouble));
    controls->wpy = (gdouble *) g_malloc(controls->nps*sizeof(gdouble));
    controls->wpz = (gdouble *) g_malloc(controls->nps*sizeof(gdouble));


//    printf("we have %d segments after simplification\n", controls->nps);


}



static const gchar xpos_key[]       = "/module/brickshow/xpos";
static const gchar ypos_key[]       = "/module/brickshow/ypos";
static const gchar zpos_key[]       = "/module/brickshow/zpos";
static const gchar type_key[]    = "/module/brickshow/dirtype";
static const gchar gtype_key[]    = "/module/brickshow/dirgtype";
static const gchar update_key[] = "/module/brickshow/update";
static const gchar perspective_key[] = "/module/brickshow/perspective";
static const gchar size_key[] = "/module/brickshow/size";
static const gchar zscale_key[] = "/module/brickshow/zscale";

static void
brickshow_sanitize_args(BrickshowArgs *args)
{
    args->xpos = CLAMP(args->xpos, 0, 100);
    args->ypos = CLAMP(args->ypos, 0, 100);
    args->zpos = CLAMP(args->zpos, 0, 100);
    args->size = CLAMP(args->size, 1, 100);
    args->zscale = CLAMP(args->zscale, 1, 100);
    args->type = MIN(args->type, PROJ_DIRZ);
    args->gtype = MIN(args->gtype, GRAPH_DIRZ);
    args->update = !!args->update;
    args->perspective = !!args->perspective;
}

static void
brickshow_load_args(GwyContainer *container,
                     BrickshowArgs *args)
{
    *args = brickshow_defaults;

    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    gwy_container_gis_enum_by_name(container, gtype_key, &args->gtype);
    gwy_container_gis_double_by_name(container, xpos_key, &args->xpos);
    gwy_container_gis_double_by_name(container, ypos_key, &args->ypos);
    gwy_container_gis_double_by_name(container, zpos_key, &args->zpos);
    gwy_container_gis_double_by_name(container, size_key, &args->size);
    gwy_container_gis_double_by_name(container, zscale_key, &args->zscale);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_boolean_by_name(container, perspective_key, &args->perspective);
    brickshow_sanitize_args(args);
}

static void
brickshow_save_args(GwyContainer *container,
                     BrickshowArgs *args)
{
    gwy_container_set_enum_by_name(container, type_key, args->type);
    gwy_container_set_enum_by_name(container, gtype_key, args->gtype);
    gwy_container_set_double_by_name(container, xpos_key, args->xpos);
    gwy_container_set_double_by_name(container, ypos_key, args->ypos);
    gwy_container_set_double_by_name(container, zpos_key, args->zpos);
    gwy_container_set_double_by_name(container, size_key, args->size);
    gwy_container_set_double_by_name(container, zscale_key, args->zscale);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_boolean_by_name(container, perspective_key, args->perspective);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
