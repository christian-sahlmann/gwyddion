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
#include <libgwyddion/gwyddion.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/filters.h>
#include <libprocess/hough.h>
#include <libprocess/grains.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <string.h>
#include <glib/gstdio.h>
#include "evaluator.h"
#include "evaluator_task_dialog.h"
#include "evaluator_feature_dialog.h"

#define ESETUP_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    NO_COLUMN = 0,
    DESCRIPTION_COLUMN = 1,
    TYPE_COLUMN = 2
};

enum {
    TEXT_COLUMN = 1,
    PREVIEW_SIZE = 320,
    DEFAULT_POINT_SIZE = 50,
    DEFAULT_RHO_SIZE = 10,
    DEFAULT_THETA_SIZE = 10
};

typedef enum {
    SETUP_VIEW_DETECTED_POINTS  = 0,
    SETUP_VIEW_DETECTED_LINES  = 1,
    SETUP_VIEW_SELECTED_POINTS  = 2,
    SETUP_VIEW_SELECTED_LINES  = 3,
    SETUP_VIEW_FIXED_POINTS  = 4,
    SETUP_VIEW_FIXED_LINES  = 5,
    SETUP_VIEW_CORRELATION_POINTS  = 6
} GwySetupViewType;


typedef struct {
    GwySetupViewType what;
    GwyEvaluator *evaluator;
} EsetupArgs;

typedef struct {
    EsetupArgs *args;
    GtkWidget *view;
    GtkWidget *features;
    GtkListStore *detected_list;
    GtkListStore *relative_list;
    GtkListStore *correlation_list;
    GtkListStore *evaluator_list;
    GtkTreeSelection *detected_selection;
    GtkTreeSelection *relative_selection;
    GtkTreeSelection *correlation_selection;
    GtkTreeSelection *evaluator_selection;
    GtkWidget *what;
    GtkWidget *detected_edit_button;
    GtkWidget *detected_remove_button;
    GtkWidget *correlation_edit_button;
    GtkWidget *correlation_remove_button;
    GtkWidget *relative_remove_button;
    GwyContainer *mydata;
    GwyVectorLayer *vlayer_dpoint;
    GwyVectorLayer *vlayer_dline;
    GwyVectorLayer *vlayer_spoint;
    GwyVectorLayer *vlayer_sline;
    GwyVectorLayer *vlayer_fpoint;
    GwyVectorLayer *vlayer_fline;
    GwyVectorLayer *vlayer_cpoint;
    GArray *detected_point_chosen;
    GArray *detected_line_chosen;
    gint task_edited;
    gint fixed_point_max;
    gint fixed_line_max;
    gint correlation_point_max;
    GtkDialog *pdialog;
} EsetupControls;

static gboolean    module_register            (void);
static void        esetup                    (GwyContainer *data,
                                               GwyRunType run);
static void        esetup_dialog                (EsetupArgs *args,
                                               GwyContainer *data);
static void        esetup_load_args             (GwyContainer *container,
                                               EsetupArgs *args);
static void        esetup_save_args             (GwyContainer *container,
                                               EsetupArgs *args);
static void        esetup_sanitize_args         (EsetupArgs *args);

static void        detected_remove_cb          (EsetupControls *controls);
static void        detected_edit_cb            (EsetupControls *controls);
static void        relative_remove_cb          (EsetupControls *controls);
static void        correlation_remove_cb       (EsetupControls *controls);
static void        correlation_edit_cb         (EsetupControls *controls);
static void        task_add_cb                 (EsetupControls *controls);
static void        task_remove_cb              (EsetupControls *controls);
static void        task_edit_cb                (EsetupControls *controls);
static void        what_changed_cb             (GtkWidget *combo, 
                                                EsetupControls *controls);
static void        evaluator_load_cb           (EsetupControls *controls);
static void        evaluator_save_cb           (EsetupControls *controls);

static void        dpoints_object_chosen_cb(GwyVectorLayer *layer, 
                                                 gint i,
                                                 EsetupControls *controls);
static void        dlines_object_chosen_cb(GwyVectorLayer *layer, 
                                                gint i,
                                                 EsetupControls *controls);
static void        fpoints_selection_changed_cb(GwySelection *selection, 
                                                 gint i,
                                                 EsetupControls *controls);
static void        flines_selection_changed_cb(GwySelection *selection, 
                                                gint i,
                                                 EsetupControls *controls);
static void        cpoints_selection_changed_cb(GwySelection *selection,
                                                 gint i,
                                                 EsetupControls *controls);

static void        selections_unref            (EsetupControls *controls);      
static void        detect_points              (EsetupControls *controls);
static void        detect_lines               (EsetupControls *controls);
static void        preset_relative            (EsetupControls *controls);

static void        update_selected_points     (EsetupControls *controls);
static void        update_selected_lines     (EsetupControls *controls);

static GwyVectorLayer *create_layer_with_selection(const gchar* ltype, 
                                                   const gchar* stype,
                                                   const gchar* key, 
                                                   GwyContainer *container);
static void        expression_add(EsetupControls *controls, 
                                  gchar *expression,
                                  gchar *threshold);
static void        test_stupid_class_init();
static void        update_after_load         (EsetupControls *controls);

static const EsetupArgs esetup_defaults = {
    0, NULL
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Automatic evaluator setup."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.8",  /* FIXME */
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",  /* FIXME */
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("esetup",
                              (GwyProcessFunc)&esetup,
                              N_("/_Evaluator/_Setup..."),
                              GWY_STOCK_GRAINS,
                              ESETUP_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Setup automatic evaluator"));

    return TRUE;
}

static void
esetup(GwyContainer *data, GwyRunType run)
{
    EsetupArgs args;

    g_return_if_fail(run & ESETUP_RUN_MODES);
    esetup_load_args(gwy_app_settings_get(), &args);

    esetup_dialog(&args, data);
    esetup_save_args(gwy_app_settings_get(), &args);
}


static void
esetup_dialog(EsetupArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *label, *hbox, *tree, *notebook, *page, *button, *scroll;
    GtkCellRenderer *renderer;
    EsetupControls controls;
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    GQuark mquark;
    gint row, prow, id, column_id;

    
    static const GwyEnum whats[] = {
        { N_("Detected points"),          SETUP_VIEW_DETECTED_POINTS,        },
        { N_("Detected lines"),           SETUP_VIEW_DETECTED_LINES,         },
        { N_("Selected points"),          SETUP_VIEW_SELECTED_POINTS,        },
        { N_("Selected lines"),           SETUP_VIEW_SELECTED_LINES,         },
        { N_("Relative points"),          SETUP_VIEW_FIXED_POINTS,           },
        { N_("Relative lines"),           SETUP_VIEW_FIXED_LINES,            },
        { N_("Correlation points"),       SETUP_VIEW_CORRELATION_POINTS,     },
    };


    enum {
          RESPONSE_SAVE = 1,
          RESPONSE_OPEN = 2
    };
    
    controls.args = args;
    args->evaluator = gwy_evaluator_new();

    controls.detected_point_chosen = g_array_new (FALSE, FALSE, sizeof(gboolean));
    controls.detected_line_chosen = g_array_new (FALSE, FALSE, sizeof(gboolean));
    controls.task_edited = -1;
    controls.correlation_point_max = 0;
    controls.fixed_point_max = 0;
    controls.fixed_line_max = 0;
    
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);
    g_return_if_fail(dfield);

    dialog = gtk_dialog_new_with_buttons(_("Evaluator setup"), NULL, 0,
                                         GTK_STOCK_OPEN, RESPONSE_OPEN,
                                         GTK_STOCK_SAVE, RESPONSE_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    controls.pdialog = GTK_DIALOG(dialog);
    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_copy_data_items(data, controls.mydata, id, 0,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    

    controls.vlayer_dpoint = create_layer_with_selection("GwyLayerPoint",
                                "GwySelectionPoint", "/0/select/sel_dpoint",
                                controls.mydata);
    g_object_set(controls.vlayer_dpoint, "editable", FALSE, NULL);
    controls.vlayer_dline = create_layer_with_selection("GwyLayerLine",
                                "GwySelectionLine", "/0/select/sel_dline",
                                controls.mydata);
    g_object_set(controls.vlayer_dline, "editable", FALSE, NULL);
    controls.vlayer_spoint = create_layer_with_selection("GwyLayerPoint",
                                "GwySelectionPoint", "/0/select/sel_spoint",
                                controls.mydata);
    g_object_set(controls.vlayer_spoint, "editable", FALSE, NULL);
    controls.vlayer_sline = create_layer_with_selection("GwyLayerLine",
                                "GwySelectionLine", "/0/select/sel_sline",
                                controls.mydata);
    g_object_set(controls.vlayer_sline, "editable", FALSE, NULL);
    controls.vlayer_fpoint = create_layer_with_selection("GwyLayerPoint",
                                "GwySelectionPoint", "/0/select/sel_fpoint",
                                controls.mydata);
    controls.vlayer_fline = create_layer_with_selection("GwyLayerLine",
                                "GwySelectionLine", "/0/select/sel_fline",
                                controls.mydata);
    controls.vlayer_cpoint = create_layer_with_selection("GwyLayerPoint",
                                "GwySelectionPoint", "/0/select/sel_cpoint",
                                controls.mydata);
         
    
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(10, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Display</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.what
        = gwy_enum_combo_box_new(whats, G_N_ELEMENTS(whats),
                                 G_CALLBACK(what_changed_cb),
                                 &controls, args->what, TRUE);
    gwy_table_attach_row(table, row, _("_Display:"), "",
                         controls.what);
    row++;



    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Selected features</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    notebook = gtk_notebook_new();
    
    prow = 0;
    page = gtk_table_new(2, 4, FALSE);
    label = gtk_label_new("Selected");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, label);

    controls.detected_list = gtk_list_store_new(3, G_TYPE_STRING, 
                                                 G_TYPE_STRING, G_TYPE_STRING);
    
    tree = gtk_tree_view_new();
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                "No.", renderer, 
                                                "text", NO_COLUMN,
                                                NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                "Description.", renderer, 
                                                "text", DESCRIPTION_COLUMN,
                                                NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                "Type", renderer, 
                                                "text", TYPE_COLUMN,
                                                NULL);

    gtk_widget_set_size_request(tree, 100, 100);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_table_attach(GTK_TABLE(page), scroll, 0, 4, prow, prow+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
 
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(controls.detected_list));
    controls.detected_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    
    prow++;
    controls.detected_remove_button = gtk_button_new_with_label("Remove");
    g_signal_connect_swapped(controls.detected_remove_button, "clicked", 
                             G_CALLBACK(detected_remove_cb), &controls);
    gtk_table_attach(GTK_TABLE(page), controls.detected_remove_button, 1, 2, prow, prow+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
    controls.detected_edit_button = gtk_button_new_with_label("Edit");
    g_signal_connect_swapped(controls.detected_edit_button, "clicked", 
                             G_CALLBACK(detected_edit_cb), &controls);
    gtk_table_attach(GTK_TABLE(page), controls.detected_edit_button, 2, 3, prow, prow+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
     
    prow = 0;
    page = gtk_table_new(2, 4, FALSE);
    label = gtk_label_new("Relative");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, label);

    controls.relative_list = gtk_list_store_new(3, G_TYPE_STRING, 
                                                 G_TYPE_STRING, G_TYPE_STRING);
    
    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(controls.relative_list));
    controls.relative_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    renderer = gtk_cell_renderer_text_new();
    column_id = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree),  -1,
                                                         "No.",
                                                      renderer, 
                                                      "text", NO_COLUMN, NULL);
    column_id = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                         "Type",
                                                      renderer, 
                                                      "text", TYPE_COLUMN, NULL);
    column_id = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                         "Description",
                                                      renderer, 
                                                      "text", DESCRIPTION_COLUMN, NULL);
    gtk_widget_set_size_request(tree, 100, 100);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_table_attach(GTK_TABLE(page), scroll, 0, 4, prow, prow+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
    prow++;
    controls.relative_remove_button = gtk_button_new_with_label("Remove");
    g_signal_connect_swapped(controls.relative_remove_button, "clicked", 
                             G_CALLBACK(relative_remove_cb), &controls);
    gtk_table_attach(GTK_TABLE(page), controls.relative_remove_button, 1, 2, prow, prow+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
  

    prow = 0;
    page = gtk_table_new(2, 4, FALSE);
    label = gtk_label_new("Correlation");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, label);

    controls.correlation_list = gtk_list_store_new(3, G_TYPE_STRING, 
                                                 G_TYPE_STRING, G_TYPE_STRING);
    
    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(controls.correlation_list));
    controls.correlation_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    renderer = gtk_cell_renderer_text_new();
    column_id = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                         "No.",
                                                      renderer, 
                                                      "text", NO_COLUMN,
                                                      NULL);
    column_id = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                         "Type",
                                                      renderer, 
                                                      "text", TYPE_COLUMN,
                                                      NULL);
    column_id = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                         "Description",
                                                      renderer, 
                                                      "text", DESCRIPTION_COLUMN, 
                                                      NULL);
    gtk_widget_set_size_request(tree, 100, 100);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_table_attach(GTK_TABLE(page), scroll, 0, 4, prow, prow+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
    prow++;
    controls.correlation_remove_button = gtk_button_new_with_label("Remove");
    g_signal_connect_swapped(controls.correlation_remove_button, "clicked", 
                             G_CALLBACK(correlation_remove_cb), &controls);
    gtk_table_attach(GTK_TABLE(page), controls.correlation_remove_button, 1, 2, prow, prow+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
    controls.correlation_edit_button = gtk_button_new_with_label("Edit");
    g_signal_connect_swapped(controls.correlation_edit_button, "clicked", 
                             G_CALLBACK(correlation_edit_cb), &controls);
    gtk_table_attach(GTK_TABLE(page), controls.correlation_edit_button, 2, 3, prow, prow+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
   
    gtk_table_attach(GTK_TABLE(table), notebook, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;
        
    
    
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Evaluator tasks</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    
    controls.evaluator_list = gtk_list_store_new(3, G_TYPE_STRING, 
                                                 G_TYPE_STRING, G_TYPE_STRING);
    
    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(controls.evaluator_list));
    controls.evaluator_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    renderer = gtk_cell_renderer_text_new();
    column_id = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                         "No.",
                                                      renderer, 
                                                      "text", NO_COLUMN,
                                                      NULL);
    column_id = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                         "Expression",
                                                      renderer, 
                                                      "text", TYPE_COLUMN,
                                                      NULL);
    column_id = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1,
                                                         "Threshold",
                                                      renderer, 
                                                      "text", DESCRIPTION_COLUMN,
                                                      NULL);
    gtk_widget_set_size_request(tree, 100, 100);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_table_attach(GTK_TABLE(table), scroll, 0, 4, row, row+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
    
    row++;
    button = gtk_button_new_with_label("Add");
    g_signal_connect_swapped(button, "clicked", 
                             G_CALLBACK(task_add_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), button, 0, 1, row, row+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
    button = gtk_button_new_with_label("Remove");
    g_signal_connect_swapped(button, "clicked", 
                             G_CALLBACK(task_remove_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), button, 1, 2, row, row+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
    button = gtk_button_new_with_label("Edit");
    g_signal_connect_swapped(button, "clicked", 
                             G_CALLBACK(task_edit_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), button, 2, 3, row, row+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);
 
    row++;
    
    gtk_widget_set_sensitive(controls.detected_remove_button, FALSE);
    gtk_widget_set_sensitive(controls.detected_edit_button, FALSE);
    gtk_widget_set_sensitive(controls.relative_remove_button, FALSE);
    gtk_widget_set_sensitive(controls.correlation_remove_button, FALSE);
    gtk_widget_set_sensitive(controls.correlation_edit_button, FALSE);
    
    test_stupid_class_init();
    detect_points(&controls);
    detect_lines(&controls);
    preset_relative(&controls);
    what_changed_cb(controls.what, &controls);
    
    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            selections_unref(&controls);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_OPEN:
            evaluator_load_cb(&controls);
            break;

            case RESPONSE_SAVE:
            evaluator_save_cb(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gwy_app_copy_data_items(controls.mydata, data, 0, id,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    gtk_widget_destroy(dialog);

    selections_unref(&controls);
}

static GwyVectorLayer *
create_layer_with_selection(const gchar* ltype, const gchar* stype,
                            const gchar* key, GwyContainer *container)
{
    GwyVectorLayer *layer;
    GwySelection *selection;
    
    layer = g_object_new(g_type_from_name(ltype), NULL);
    selection = GWY_SELECTION(g_object_new(g_type_from_name(stype), NULL));
    gwy_container_set_object_by_name(container, key, selection);
    gwy_vector_layer_set_selection_key(layer, key);
    gwy_object_unref(selection);

    g_object_ref(layer);
    return layer;
}

/*FIXME why this stupix piece of code must precede any deserialization
 in order to prevent all types being unknown?*/
static void
test_stupid_class_init()
{
    GwySearchPoint *spset;
    GwySearchLine *slset;
    GwyFixedPoint *fpset;
    GwyFixedLine *flset;
    GwyCorrelationPoint *cpset;
    GwyEvaluatorTask *etset;

    spset = gwy_search_point_new();
    slset = gwy_search_line_new();
    fpset = gwy_fixed_point_new();
    flset = gwy_fixed_line_new();
    cpset = gwy_correlation_point_new();
    etset = gwy_evaluator_task_new();
    
    g_object_unref(spset);
    g_object_unref(slset);
    g_object_unref(fpset);
    g_object_unref(flset);
    g_object_unref(cpset);
    g_object_unref(etset);
}


static gint
get_dpoint_pos_by_id(GPtrArray *array, gchar *id)
{
    guint i;
    for (i=0; i<array->len; i++)
    {
        if (strstr(id, GWY_SEARCH_POINT(g_ptr_array_index(array, i))->id) != NULL)
            return i;
    }
    
    return -1; 
}
static gint
get_dline_pos_by_id(GPtrArray *array, gchar *id)
{
    guint i;
    for (i=0; i<array->len; i++)
    {
        if (strstr(id, GWY_SEARCH_LINE(g_ptr_array_index(array, i))->id) != NULL)
            return i;
    }
    
    return -1; 
}

static gint
get_dpoint_selection_index_by_id(EsetupControls *controls, gchar *id)
{
    gint i;
    GwySelection *selection;
    
    selection = gwy_container_get_object_by_name(controls->mydata,
                                    gwy_vector_layer_get_selection_key(controls->vlayer_dpoint));
   
    for (i=0; i<gwy_selection_get_data(selection, NULL); i++)
    {
        if (strstr(id, g_strdup_printf("sp%d", i+1)) != NULL) return i;
    }
    return -1;
}
static gint
get_dline_selection_index_by_id(EsetupControls *controls, gchar *id)
{
    gint i;
    GwySelection *selection;
    
    selection = gwy_container_get_object_by_name(controls->mydata,
                                    gwy_vector_layer_get_selection_key(controls->vlayer_dline));
   
    for (i=0; i<gwy_selection_get_data(selection, NULL); i++)
    {
        if (strstr(id, g_strdup_printf("sl%d", i+1)) != NULL) return i;
    }
    return -1;
}

static void        
detected_remove_cb(EsetupControls *controls)
{
    gint ipos, selpos;
    gchar *id;
    GtkTreeIter iter;
   
    if (gtk_tree_selection_get_selected(controls->detected_selection,
                                 &(controls->detected_list), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(controls->detected_list), &iter, NO_COLUMN, &id, -1);
        ipos = get_dpoint_pos_by_id(controls->args->evaluator->detected_point_array, id);
        if (ipos >= 0) {
            selpos = get_dpoint_selection_index_by_id(controls, id);
            g_ptr_array_remove_index(controls->args->evaluator->detected_point_array, ipos);
            g_array_index(controls->detected_point_chosen, gboolean, selpos) = FALSE;
        } 
        ipos = get_dline_pos_by_id(controls->args->evaluator->detected_line_array, id);
        if (ipos >= 0) {
            selpos = get_dline_selection_index_by_id(controls, id); 
            g_ptr_array_remove_index(controls->args->evaluator->detected_line_array, ipos);
            g_array_index(controls->detected_line_chosen, gboolean, selpos) = FALSE;
             
        }
        gtk_list_store_remove(controls->detected_list, &iter);
        update_selected_points(controls);
        update_selected_lines(controls);
    }

    if (!controls->args->evaluator->detected_point_array->len && !controls->args->evaluator->detected_line_array->len) {
        gtk_widget_set_sensitive(controls->detected_remove_button, FALSE);
        gtk_widget_set_sensitive(controls->detected_edit_button, FALSE);
    }
}
static void        
detected_edit_cb(EsetupControls *controls)
{
    GwySearchPoint *ppoint;
    GwySearchLine *pline;
    GtkTreeIter iter;
    gchar *id;
    GtkDialog *dialog;
    gint response, edited;

 
    if (gtk_tree_selection_get_selected(controls->detected_selection,
                                        &(controls->detected_list), &iter))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(controls->detected_list), &iter, NO_COLUMN, &id, -1);
        if (strstr(id, "sp") != NULL)
        {
            edited = get_dpoint_pos_by_id(controls->args->evaluator->detected_point_array, id);
            ppoint = g_ptr_array_index(controls->args->evaluator->detected_point_array, 
                             edited);
       
            dialog = GTK_DIALOG(gwy_evaluator_point_dialog_new(ppoint->width, ppoint->height));
            response = gtk_dialog_run(dialog);
    
            switch (response)
            {
                case GTK_RESPONSE_APPLY:
                ppoint->width = gtk_adjustment_get_value(GTK_ADJUSTMENT(
                                                  GWY_EVALUATOR_POINT_DIALOG(dialog)->width_adj));
                ppoint->height = gtk_adjustment_get_value(GTK_ADJUSTMENT(
                                                  GWY_EVALUATOR_POINT_DIALOG(dialog)->height_adj));
                break;
      
                default:
                break;
            }
            gtk_widget_destroy(GTK_WIDGET(dialog));  
        }
        else
        {
            edited = get_dline_pos_by_id(controls->args->evaluator->detected_line_array, id);
            pline = g_ptr_array_index(controls->args->evaluator->detected_line_array, 
                             edited);
       
            dialog = GTK_DIALOG(gwy_evaluator_line_dialog_new(pline->rho, pline->theta));
            response = gtk_dialog_run(dialog);
    
            switch (response)
            {
                case GTK_RESPONSE_APPLY:
                pline->rho = gtk_adjustment_get_value(GTK_ADJUSTMENT(
                                                  GWY_EVALUATOR_LINE_DIALOG(dialog)->rho_adj));
                pline->theta = gtk_adjustment_get_value(GTK_ADJUSTMENT(
                                                  GWY_EVALUATOR_LINE_DIALOG(dialog)->theta_adj));
                break;
      
                default:
                break;
            }
            gtk_widget_destroy(GTK_WIDGET(dialog));  
         }
    }
}
static void        
detected_correlation_cb(EsetupControls *controls)
{
}

static gint
get_fpoint_pos_by_id(GPtrArray *array, gchar *id)
{
    guint i;
    for (i=0; i<array->len; i++)
    {
        if (strstr(id, GWY_FIXED_POINT(g_ptr_array_index(array, i))->id) != NULL)
            return i;
    }
    
    return -1; 
}

static gint
get_fline_pos_by_id(GPtrArray *array, gchar *id)
{
    guint i;
    for (i=0; i<array->len; i++)
    {
        if (strstr(id, GWY_FIXED_LINE(g_ptr_array_index(array, i))->id) != NULL)
            return i;
    }
    
    return -1; 
}

static gint
get_fpoint_selection_index_by_position(EsetupControls *controls, gdouble x, gdouble y)
{
    gint i;
    GwySelection *selection;
    gdouble xdiff, ydiff;
    gdouble pointdata[2];
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                     controls->mydata, "/0/data"));
    selection = gwy_container_get_object_by_name(controls->mydata,
                                    gwy_vector_layer_get_selection_key(controls->vlayer_fpoint));
    xdiff = gwy_data_field_get_xreal(dfield)/
                             gwy_data_field_get_xres(dfield);
    ydiff = gwy_data_field_get_yreal(dfield)/
                             gwy_data_field_get_yres(dfield);

    for (i=0; i<gwy_selection_get_data(selection, NULL); i++)
    {
        gwy_selection_get_object(selection, i, pointdata);
        if (fabs(x - pointdata[0]) < xdiff &&
            fabs(y - pointdata[1]) < ydiff) return i;
    }
    return -1;
}

static gint
get_fline_selection_index_by_position(EsetupControls *controls, gdouble xstart, gdouble ystart,
                                      gdouble xend, gdouble yend)
{
    gint i;
    GwySelection *selection;
    gdouble xdiff, ydiff;
    gdouble linedata[4];
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                     controls->mydata, "/0/data"));
    selection = gwy_container_get_object_by_name(controls->mydata,
                                    gwy_vector_layer_get_selection_key(controls->vlayer_fline));
    xdiff = gwy_data_field_get_xreal(dfield)/
                             gwy_data_field_get_xres(dfield);
    ydiff = gwy_data_field_get_yreal(dfield)/
                             gwy_data_field_get_yres(dfield);

    for (i=0; i<gwy_selection_get_data(selection, NULL); i++)
    {
        gwy_selection_get_object(selection, i, linedata);
        if (fabs(xstart - linedata[0]) < xdiff &&
            fabs(ystart - linedata[1]) < ydiff &&
            fabs(xend - linedata[2]) < xdiff &&
            fabs(yend - linedata[3]) < ydiff) return i;
    }
    return -1;
}




static void        
relative_remove_cb(EsetupControls *controls)
{
    gint ipos, selpos;
    gchar *id;
    GtkTreeIter iter;
    GwySelection *selection;

    if (gtk_tree_selection_get_selected(controls->relative_selection,
                                 &(controls->relative_list), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(controls->relative_list), &iter, NO_COLUMN, &id, -1);
        ipos = get_fpoint_pos_by_id(controls->args->evaluator->fixed_point_array, id);
        if (ipos >= 0) {
            
            selection = gwy_container_get_object_by_name(controls->mydata,
                               gwy_vector_layer_get_selection_key(controls->vlayer_fpoint));
            selpos = get_fpoint_selection_index_by_position(controls,
                          GWY_FIXED_POINT(g_ptr_array_index(controls->args->evaluator->fixed_point_array, 
                                        ipos))->xc,
                          GWY_FIXED_POINT(g_ptr_array_index(controls->args->evaluator->fixed_point_array, 
                                        ipos))->yc);
            if (ipos >= 0) g_ptr_array_remove_index(controls->args->evaluator->fixed_point_array, ipos);
            if (selpos >= 0) gwy_selection_delete_object(selection, selpos);
        } 
        ipos = get_fline_pos_by_id(controls->args->evaluator->fixed_line_array, id);
        if (ipos >= 0) {
             
            selection = gwy_container_get_object_by_name(controls->mydata,
                               gwy_vector_layer_get_selection_key(controls->vlayer_fline));
            selpos = get_fline_selection_index_by_position(controls,
                          GWY_FIXED_LINE(g_ptr_array_index(controls->args->evaluator->fixed_line_array, 
                                        ipos))->xstart,
                          GWY_FIXED_LINE(g_ptr_array_index(controls->args->evaluator->fixed_line_array, 
                                        ipos))->ystart,
                          GWY_FIXED_LINE(g_ptr_array_index(controls->args->evaluator->fixed_line_array, 
                                        ipos))->xend,
                          GWY_FIXED_LINE(g_ptr_array_index(controls->args->evaluator->fixed_line_array, 
                                        ipos))->yend);
        
            if (ipos >= 0) g_ptr_array_remove_index(controls->args->evaluator->fixed_line_array, ipos);
            if (selpos >= 0) gwy_selection_delete_object(selection, selpos);
        }
        gtk_list_store_remove(controls->relative_list, &iter);
     }

    if (!controls->args->evaluator->fixed_point_array->len && !controls->args->evaluator->fixed_line_array->len) {
        gtk_widget_set_sensitive(controls->relative_remove_button, FALSE);
    }
}

static gint
get_cpoint_pos_by_id(GPtrArray *array, gchar *id)
{
    guint i;
    for (i=0; i<array->len; i++)
    {
        if (strstr(id, GWY_CORRELATION_POINT(g_ptr_array_index(array, i))->id) != NULL)
            return i;
    }
    
    return -1; 
}
static gint
get_cpoint_selection_index_by_position(EsetupControls *controls, gdouble x, gdouble y)
{
    gint i;
    GwySelection *selection;
    gdouble xdiff, ydiff;
    gdouble pointdata[2];
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                     controls->mydata, "/0/data"));
    selection = gwy_container_get_object_by_name(controls->mydata,
                                    gwy_vector_layer_get_selection_key(controls->vlayer_cpoint));
    xdiff = gwy_data_field_get_xreal(dfield)/
                             gwy_data_field_get_xres(dfield);
    ydiff = gwy_data_field_get_yreal(dfield)/
                             gwy_data_field_get_yres(dfield);

    for (i=0; i<gwy_selection_get_data(selection, NULL); i++)
    {
        gwy_selection_get_object(selection, i, pointdata);
        if (fabs(x - pointdata[0]) < xdiff &&
            fabs(y - pointdata[1]) < ydiff) return i;
    }
    return -1;
}
static void        
correlation_remove_cb(EsetupControls *controls)
{
    gint ipos, selpos;
    gchar *id;
    GtkTreeIter iter;
    GwySelection *selection;

    selection = gwy_container_get_object_by_name(controls->mydata,
                               gwy_vector_layer_get_selection_key(controls->vlayer_cpoint));
   
    if (gtk_tree_selection_get_selected(controls->correlation_selection,
                                 &(controls->correlation_list), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(controls->correlation_list), &iter, NO_COLUMN, &id, -1);
        ipos = get_cpoint_pos_by_id(controls->args->evaluator->correlation_point_array, id);
        selpos = get_cpoint_selection_index_by_position(controls,
                          GWY_CORRELATION_POINT(g_ptr_array_index(controls->args->evaluator->correlation_point_array, 
                                        ipos))->xc,
                          GWY_CORRELATION_POINT(g_ptr_array_index(controls->args->evaluator->correlation_point_array, 
                                        ipos))->yc);
        if (ipos >= 0) g_ptr_array_remove_index(controls->args->evaluator->correlation_point_array, ipos);
        if (selpos >= 0) gwy_selection_delete_object(selection, selpos);
        gtk_list_store_remove(controls->correlation_list, &iter);
     }

    if (!controls->args->evaluator->correlation_point_array->len) {
        gtk_widget_set_sensitive(controls->correlation_remove_button, FALSE);
        gtk_widget_set_sensitive(controls->correlation_edit_button, FALSE);
    }

}
static void        
correlation_edit_cb(EsetupControls *controls)
{
    GwyCorrelationPoint *ppoint;
    GtkTreeIter iter;
    gchar *id;
    GwyEvaluatorCorrelationPointDialog *dialog;
    gint response, edited;

 
    if (gtk_tree_selection_get_selected(controls->correlation_selection,
                                        &(controls->correlation_list), &iter))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(controls->correlation_list), &iter, NO_COLUMN, &id, -1);
        edited = get_cpoint_pos_by_id(controls->args->evaluator->correlation_point_array, id);
        ppoint = g_ptr_array_index(controls->args->evaluator->correlation_point_array,
                         edited);
       
        dialog = GWY_EVALUATOR_CORRELATION_POINT_DIALOG(gwy_evaluator_correlation_point_dialog_new(
                                           ppoint->width, ppoint->height, ppoint->swidth, ppoint->sheight));
        response = gtk_dialog_run (GTK_DIALOG (dialog));
    
        switch (response)
        {
            case GTK_RESPONSE_APPLY:
            ppoint->width = gtk_adjustment_get_value(GTK_ADJUSTMENT(dialog->width_adj));
            ppoint->height = gtk_adjustment_get_value(GTK_ADJUSTMENT(dialog->height_adj));
            ppoint->swidth = gtk_adjustment_get_value(GTK_ADJUSTMENT(dialog->swidth_adj));
            ppoint->sheight = gtk_adjustment_get_value(GTK_ADJUSTMENT(dialog->sheight_adj));
             break;
      
            default:
            break;
        }
        gtk_widget_destroy (GTK_WIDGET(dialog));  
    }
}
static void        
task_add_cb(EsetupControls *controls)
{
    GwyEvaluatorTaskDialog *dialog;
    gint response;

    dialog = GWY_EVALUATOR_TASK_DIALOG(gwy_evaluator_task_dialog_new());
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    switch (response)
    {
      case GTK_RESPONSE_APPLY:
         expression_add(controls, 
                        gtk_entry_get_text(GTK_ENTRY(dialog->expression)),
                        gtk_entry_get_text(GTK_ENTRY(dialog->threshold_expression)));   
      break;
      
      default:
         break;
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));  
    
}

static guint
get_task_pos_by_id(GPtrArray *array, gchar *id)
{
    guint i;
    for (i=0; i<array->len; i++)
    {
        if (strstr(id, GWY_EVALUATOR_TASK(g_ptr_array_index(array, i))->id) != NULL)
            return i;
    }
    
    return 0; 
}

static void        
task_remove_cb(EsetupControls *controls)
{
    guint ipos;
    gchar *id;
    GtkTreeIter iter;
   
    if (gtk_tree_selection_get_selected(controls->evaluator_selection,
                                 &(controls->evaluator_list), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(controls->evaluator_list), &iter, NO_COLUMN, &id, -1);
        ipos = get_task_pos_by_id(controls->args->evaluator->expression_task_array, id);
        gtk_list_store_remove(controls->evaluator_list, &iter);
        g_ptr_array_remove_index(controls->args->evaluator->expression_task_array, ipos);
    }

 }
static void        
task_edit_cb(EsetupControls *controls)
{
    GwyEvaluatorTask *ptask;
    GtkTreeIter iter;
    gchar *id;
    GwyEvaluatorTaskDialog *dialog;
    gint response;

 
    if (gtk_tree_selection_get_selected(controls->evaluator_selection,
                                        &(controls->evaluator_list), &iter))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(controls->evaluator_list), &iter, NO_COLUMN, &id, -1);
        controls->task_edited = get_task_pos_by_id(controls->args->evaluator->expression_task_array, id);
        ptask = g_ptr_array_index(controls->args->evaluator->expression_task_array,  
                             controls->task_edited);

        dialog = GWY_EVALUATOR_TASK_DIALOG(gwy_evaluator_task_dialog_new());
        gtk_entry_set_text(GTK_ENTRY(dialog->expression), ptask->expression);
        gtk_entry_set_text(GTK_ENTRY(dialog->threshold_expression), ptask->threshold);
    
        response = gtk_dialog_run (GTK_DIALOG (dialog));
    
        switch (response)
        {
            case GTK_RESPONSE_APPLY:
            expression_add(controls, 
                        gtk_entry_get_text(GTK_ENTRY(dialog->expression)),
                        gtk_entry_get_text(GTK_ENTRY(dialog->threshold_expression)));   
            break;
      
            default:
            break;
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));  
    }
}

static void
esetup_sanitize_args(EsetupArgs *args)
{
}

static void
expression_add(EsetupControls *controls, gchar *expression, gchar *threshold)
{
    GwyEvaluatorTask *ptask;
    GtkTreeIter iter;
    gint k;
    
    if (controls->task_edited == -1) {
        ptask = gwy_evaluator_task_new();
        ptask->id = g_strdup_printf("task%d", controls->args->evaluator->expression_task_array->len);    
        ptask->expression = g_strdup(expression);
        ptask->threshold = g_strdup(threshold);
        g_ptr_array_add(controls->args->evaluator->expression_task_array, ptask); 
        gtk_list_store_insert_with_values(controls->evaluator_list, &iter, controls->task_edited,
                       NO_COLUMN, ptask->id,
                       TYPE_COLUMN, ptask->expression,
                       DESCRIPTION_COLUMN, ptask->threshold,
                       -1);
        
     } else {
        ptask = g_ptr_array_index(controls->args->evaluator->expression_task_array,
                                         controls->task_edited);
        ptask->expression = g_strdup(expression);
        ptask->threshold = g_strdup(threshold);
    
        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(controls->evaluator_list), &iter);
        for (k = 0; k<controls->task_edited; k++) 
            gtk_tree_model_iter_next(GTK_TREE_MODEL(controls->evaluator_list), &iter);
        
        gtk_list_store_set(controls->evaluator_list, 
                       &iter,
                       NO_COLUMN, ptask->id,
                       TYPE_COLUMN, ptask->expression,
                       DESCRIPTION_COLUMN, ptask->threshold,
                       -1);
        controls->task_edited = -1;
     }
    


}


static void        
what_changed_cb(GtkWidget *combo, EsetupControls *controls)
{
    GwySelection *selection;
    
    controls->args->what = (GwySetupViewType)gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

    /* setup vector layer */
    if (controls->args->what == SETUP_VIEW_DETECTED_POINTS)
    {
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), controls->vlayer_dpoint);
        g_signal_connect(GWY_VECTOR_LAYER(controls->vlayer_dpoint), "object-chosen",
                         G_CALLBACK(dpoints_object_chosen_cb), controls);
    }
    else if (controls->args->what == SETUP_VIEW_DETECTED_LINES)
    {
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), controls->vlayer_dline);
        g_signal_connect(GWY_VECTOR_LAYER(controls->vlayer_dline), "object-chosen",
                         G_CALLBACK(dlines_object_chosen_cb), controls);
    }
    else if (controls->args->what == SETUP_VIEW_SELECTED_POINTS)
    {
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), controls->vlayer_spoint);
    }
    else if (controls->args->what == SETUP_VIEW_SELECTED_LINES)
    {
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), controls->vlayer_sline);
    }
    else if (controls->args->what == SETUP_VIEW_FIXED_POINTS)
    {
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), controls->vlayer_fpoint);
        selection = gwy_container_get_object_by_name(controls->mydata,
                             gwy_vector_layer_get_selection_key(controls->vlayer_fpoint));
        g_signal_connect(selection, "changed",
                         G_CALLBACK(fpoints_selection_changed_cb), controls);
    }
    else if (controls->args->what == SETUP_VIEW_FIXED_LINES)
    {
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), controls->vlayer_fline);
        selection = gwy_container_get_object_by_name(controls->mydata,
                             gwy_vector_layer_get_selection_key(controls->vlayer_fline));
        g_signal_connect(selection, "changed",
                         G_CALLBACK(flines_selection_changed_cb), controls);
    }
    else if (controls->args->what == SETUP_VIEW_CORRELATION_POINTS)
    {
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), controls->vlayer_cpoint);
        selection = gwy_container_get_object_by_name(controls->mydata,
                             gwy_vector_layer_get_selection_key(controls->vlayer_cpoint));
        g_signal_connect(selection, "changed",
                         G_CALLBACK(cpoints_selection_changed_cb), controls);
    }
}

static void        
detect_points(EsetupControls *controls)
{
    GwyDataField *dfield;
    GwyDataField *filtered, *x_gradient, *y_gradient;    
    GwySelection *selection;
    gint ndata = 50, skip = 10;
    gint i;
    gdouble xdata[50], ydata[50];
    gdouble zdata[50];
    gdouble seldata[2], threshval, hmin, hmax;
    gboolean notchosen = FALSE;
    
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                  controls->mydata, "/0/data"));
    filtered = gwy_data_field_new_alike(dfield, FALSE);

    x_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(x_gradient, GWY_ORIENTATION_HORIZONTAL);
    y_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(y_gradient, GWY_ORIENTATION_VERTICAL);
    
    gwy_data_field_filter_harris(x_gradient, y_gradient, filtered, 4, 0.07);
    gwy_data_field_invert(filtered, FALSE, FALSE, TRUE);
    
    hmin = gwy_data_field_get_min(filtered);
    hmax = gwy_data_field_get_max(filtered);
    threshval = hmin + (hmax - hmin)*0.8;
    ndata = gwy_data_field_get_local_maxima_list(filtered,
                                         xdata,
                                         ydata,
                                         zdata,
                                         ndata,
                                         skip,
                                         threshval,
                                         TRUE);


    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_dpoint));
    gwy_selection_set_max_objects(selection, MAX(ndata, 1));
     for (i=0; i<ndata; i++)
    {
        seldata[0] = gwy_data_field_itor(dfield, xdata[i]);
        seldata[1] = gwy_data_field_jtor(dfield, ydata[i]);
        gwy_selection_set_object(selection, i, seldata);
        g_array_append_val(controls->detected_point_chosen, notchosen);       
    }
}

static void        
detect_lines(EsetupControls *controls)
{
    GwyDataField *dfield, *f1, *f2, *edgefield, *filtered, *water;
    gdouble xdata[10], ydata[10];
    gdouble zdata[10];
    gdouble rho, theta;
    gint ndata = 10, skip = 10;
    gint i, px1, px2, py1, py2;
    GwySelection *selection;
    gdouble seldata[4], threshval, hmin, hmax;
    gboolean notchosen = FALSE;
    
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                       controls->mydata, "/0/data"));
    edgefield = gwy_data_field_duplicate(dfield);
    f1 = gwy_data_field_duplicate(dfield);
    f2 = gwy_data_field_duplicate(dfield);
    filtered = gwy_data_field_new(3*(sqrt(gwy_data_field_get_xres(dfield)*gwy_data_field_get_xres(dfield)
                             +gwy_data_field_get_yres(dfield)*gwy_data_field_get_yres(dfield))),
                             360, 0, 0,
                             FALSE);


    gwy_data_field_filter_canny(edgefield, 0.1);
    gwy_data_field_filter_sobel(f1, GWY_ORIENTATION_HORIZONTAL);
    gwy_data_field_filter_sobel(f2, GWY_ORIENTATION_VERTICAL);
    gwy_data_field_hough_line(edgefield,
                              NULL,
                              NULL,
                              filtered,
                              1,
                              FALSE);
                
    water = gwy_data_field_duplicate(filtered);
    gwy_data_field_grains_splash_water(filtered, water, 2,
                                 0.005*(gwy_data_field_get_max(filtered) - gwy_data_field_get_min(filtered)));
    
    hmin = gwy_data_field_get_min(water);
    hmax = gwy_data_field_get_max(water);
    threshval = hmin + (hmax - hmin)*0.4;
    ndata = gwy_data_field_get_local_maxima_list(water,
                                         xdata,
                                         ydata,
                                         zdata,
                                         ndata,
                                         skip,
                                         threshval,
                                         TRUE);

    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_dline));
    gwy_selection_set_max_objects(selection, MAX(ndata, 1));
     
    for (i=0; i<ndata; i++)
    {
    
        if (zdata[i] < threshval) continue;

        printf("setup detected: %g %g\n", xdata[i], ydata[i]);

        rho = ((gdouble)xdata[i])*gwy_data_field_get_xreal(filtered)/((gdouble)gwy_data_field_get_xres(filtered))
            - gwy_data_field_get_xreal(filtered)/2.0;
        theta = ((gdouble)ydata[i])*G_PI/((gdouble)gwy_data_field_get_yres(filtered)) + G_PI/4;
       
        
        gwy_data_field_hough_polar_line_to_datafield(dfield, rho, theta,
                    &px1, &px2, &py1, &py2);
        
        printf("setup detected rho/theta: %g %g\n", rho, theta);
        
        seldata[0] = gwy_data_field_itor(dfield, px1);
        seldata[1] = gwy_data_field_jtor(dfield, py1);
        seldata[2] = gwy_data_field_itor(dfield, px2);
        seldata[3] = gwy_data_field_jtor(dfield, py2);
        printf("setup selection: %g %g %g %g\n", seldata[0], seldata[1], seldata[2], seldata[3]);
        
        
        gwy_selection_set_object(selection, i, seldata);
        g_array_append_val(controls->detected_line_chosen, notchosen);
    }
    
    g_object_unref(filtered);
    g_object_unref(edgefield);
    g_object_unref(f1);
    g_object_unref(f2);
        
}




static void        
preset_relative(EsetupControls *controls)
{
    GtkTreeIter iter;
    GwyDataField *dfield;
    GwySelection *selection;
    GwyFixedPoint *spset;
    gdouble seldata[2];
    
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                  controls->mydata, "/0/data"));
  
    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_fpoint));
    gwy_selection_set_max_objects(selection, 10);
    
    spset = gwy_fixed_point_new();
    spset->id = g_strdup_printf("fp%d", controls->args->evaluator->fixed_point_array->len);
    spset->xc = 0;
    spset->yc = 0;
    g_ptr_array_add(controls->args->evaluator->fixed_point_array, spset);

    
    seldata[0] = spset->xc;
    seldata[1] = spset->yc;
    gwy_selection_set_object(selection, 0, seldata);

    //printf("ulid: %s\n", spset.id);
    gtk_list_store_insert_with_values(controls->relative_list, &iter, -1,
                       NO_COLUMN, spset->id,
                       DESCRIPTION_COLUMN, "UL corner",
                       TYPE_COLUMN, "point",
                       -1);

    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_cpoint));
    gwy_selection_set_max_objects(selection, 10);
    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_fline));
    gwy_selection_set_max_objects(selection, 10);
   
}


static void
update_selected_points(EsetupControls *controls)
{
    GwySelection *selection;
    GwySearchPoint *spset;
    gint i;
    gdouble seldata[2];
   
    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_spoint));
    gwy_selection_set_max_objects(selection, MAX(1, controls->args->evaluator->detected_point_array->len));
    gwy_selection_clear(selection);
    
    for (i=0; i<controls->args->evaluator->detected_point_array->len; i++)
    {
        if (!gwy_selection_is_full(selection))
        {
        
            spset = g_ptr_array_index(controls->args->evaluator->detected_point_array, i);
            seldata[0] = spset->xc;
            seldata[1] = spset->yc;
            gwy_selection_set_object(selection, i, seldata);
        }
    }
}

static void
update_selected_lines(EsetupControls *controls)
{
    GwySelection *selection;
    GwySearchLine *slset;
    gint i;
    gdouble seldata[4];
    
    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_sline));
    gwy_selection_set_max_objects(selection, MAX(1, controls->args->evaluator->detected_line_array->len));
    gwy_selection_clear(selection);

    for (i=0; i<controls->args->evaluator->detected_line_array->len; i++)
    {
        slset = g_ptr_array_index(controls->args->evaluator->detected_line_array, i);
        seldata[0] = slset->xstart;
        seldata[1] = slset->ystart;
        seldata[2] = slset->xend;
        seldata[3] = slset->yend;
        gwy_selection_set_object(selection, i, seldata);
    }
}

static void
dpoints_object_chosen_cb(GwyVectorLayer *layer, gint i, EsetupControls *controls)
{
    GtkTreeIter iter;
    GwySearchPoint *spset;
    gdouble pointdata[2];
    GwyDataField *dfield;
    GwySelection *selection;

    if (g_array_index(controls->detected_point_chosen, gboolean, i)) return;
    
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                     controls->mydata, "/0/data"));

    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_dpoint));
 
    gwy_selection_get_object(selection, i, pointdata);
 
    
    spset = gwy_search_point_new();
    spset->id = g_strdup_printf("sp%d", i + 1);

    spset->xc = pointdata[0];
    spset->yc = pointdata[1];
    spset->x = gwy_data_field_rtoi(dfield, pointdata[0]) - DEFAULT_POINT_SIZE/2;
    spset->y = gwy_data_field_rtoj(dfield, pointdata[1]) - DEFAULT_POINT_SIZE/2;
    spset->width = DEFAULT_POINT_SIZE;
    spset->height = DEFAULT_POINT_SIZE;
    
    
    g_ptr_array_add(controls->args->evaluator->detected_point_array, spset);
    
    g_array_index(controls->detected_point_chosen, gboolean, i) = TRUE;
    gtk_list_store_insert_with_values(controls->detected_list, &iter, -1,
                       NO_COLUMN, spset->id,
                       DESCRIPTION_COLUMN, "des",
                       TYPE_COLUMN, "point",
                       -1);

    gtk_widget_set_sensitive(controls->detected_remove_button, TRUE);
    gtk_widget_set_sensitive(controls->detected_edit_button, TRUE);
  
    update_selected_points(controls);
}

static void
dlines_object_chosen_cb(GwyVectorLayer *layer, gint i, EsetupControls *controls)
{
    GtkTreeIter iter;
    GwySearchLine *slset;
    gdouble linedata[4];
    gdouble rho, theta;
    GwyDataField *dfield;
    GwySelection *selection;

    if (g_array_index(controls->detected_line_chosen, gboolean, i)) return;
    
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                     controls->mydata, "/0/data"));
    
    selection = gwy_container_get_object_by_name(controls->mydata,
                                     gwy_vector_layer_get_selection_key(controls->vlayer_dline));
    gwy_selection_get_object(selection, i, linedata);

    gwy_data_field_hough_datafield_line_to_polar(dfield,
                                                 (gint)gwy_data_field_rtoi(dfield, linedata[0]),
                                                 (gint)gwy_data_field_rtoi(dfield, linedata[2]),
                                                 (gint)gwy_data_field_rtoj(dfield, linedata[1]),
                                                 (gint)gwy_data_field_rtoj(dfield, linedata[3]),
                                                 &rho,
                                                 &theta);
    
    slset = gwy_search_line_new();
    slset->id = g_strdup_printf("sl%d", i + 1);
    slset->xstart = linedata[0];
    slset->ystart = linedata[1];
    slset->xend = linedata[2];
    slset->yend = linedata[3];
    slset->rhoc = rho;
    slset->thetac = theta;
    slset->rho = DEFAULT_RHO_SIZE;
    slset->theta = DEFAULT_THETA_SIZE;
    g_ptr_array_add(controls->args->evaluator->detected_line_array, slset);
    
    g_array_index(controls->detected_line_chosen, gboolean, i) = TRUE;
    gtk_list_store_insert_with_values(controls->detected_list, &iter, -1,
                       NO_COLUMN, slset->id,
                       DESCRIPTION_COLUMN, "les",
                       TYPE_COLUMN, "line",
                       -1);
    gtk_widget_set_sensitive(controls->detected_remove_button, TRUE);
    gtk_widget_set_sensitive(controls->detected_edit_button, TRUE);

    update_selected_lines(controls);
}


static gboolean
fixed_point_present(GwyDataField *dfield, gdouble x, gdouble y, GPtrArray *array)
{
    int i;
    GwyFixedPoint *arset;
    gdouble xdiff = gwy_data_field_get_xreal(dfield)/
                             gwy_data_field_get_xres(dfield);
    gdouble ydiff = gwy_data_field_get_yreal(dfield)/
                             gwy_data_field_get_yres(dfield);

    for (i=0; i<array->len; i++)
    {
        arset = g_ptr_array_index(array, i);
        if (fabs(x - arset->xc) < xdiff &&
            fabs(y - arset->yc) < ydiff) return 1;
    }
    
    return 0; 
}

static gboolean
correlation_point_present(GwyDataField *dfield, gdouble x, gdouble y, GPtrArray *array)
{
    int i;
    GwyCorrelationPoint *arset;
    gdouble xdiff = gwy_data_field_get_xreal(dfield)/
                             gwy_data_field_get_xres(dfield);
    gdouble ydiff = gwy_data_field_get_yreal(dfield)/
                             gwy_data_field_get_yres(dfield);

    for (i=0; i<array->len; i++)
    {
        arset = g_ptr_array_index(array, i);
        if (fabs(x - arset->xc) < xdiff &&
            fabs(y - arset->yc) < ydiff) return 1;
    }
    
    return 0; 
}

static void
fpoints_selection_changed_cb(GwySelection *selection, gint i, EsetupControls *controls)
{
    GtkTreeIter iter;
    GwyFixedPoint *pspset;
    gdouble pointdata[2];
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                     controls->mydata, "/0/data"));

    gwy_selection_get_object(selection, i, pointdata);
    if (fixed_point_present(dfield, pointdata[0], pointdata[1], controls->args->evaluator->fixed_point_array)) return;
    
    if (i == controls->args->evaluator->fixed_point_array->len) {
        /*new point added*/
        pspset = gwy_fixed_point_new();
        pspset->xc = pointdata[0];
        pspset->yc = pointdata[1];
        
        pspset->id = g_strdup_printf("fp%d", ++controls->fixed_point_max);     
        g_ptr_array_add(controls->args->evaluator->fixed_point_array, pspset);

        gtk_list_store_insert_with_values(controls->relative_list, &iter, -1,
                       NO_COLUMN, pspset->id,
                       DESCRIPTION_COLUMN, "des",
                       TYPE_COLUMN, "point",
                       -1);
    } else {
        /*old point moved*/
        pspset = g_ptr_array_index(controls->args->evaluator->fixed_point_array, i);
        pspset->xc = pointdata[0];
        pspset->yc = pointdata[1];

    }

    gtk_widget_set_sensitive(controls->relative_remove_button, TRUE);
 
}
static gboolean
fixed_line_present(GwyDataField *dfield, gdouble xstart, gdouble ystart, gdouble xend, gdouble yend, GPtrArray *array)
{
    int i;
    GwyFixedLine *arset;
    gdouble xdiff = gwy_data_field_get_xreal(dfield)/
                             gwy_data_field_get_xres(dfield);
    gdouble ydiff = gwy_data_field_get_yreal(dfield)/
                             gwy_data_field_get_yres(dfield);

    for (i=0; i<array->len; i++)
    {
        arset = g_ptr_array_index(array, i);
        if (fabs(xstart - arset->xstart) < xdiff &&
            fabs(xend - arset->xend) < xdiff &&
            fabs(ystart - arset->ystart) < ydiff &&
            fabs(yend - arset->yend) < ydiff) return 1;
    }
    
    return 0; 
}

static void
flines_selection_changed_cb(GwySelection *selection, gint i, EsetupControls *controls)
{
    GtkTreeIter iter;
    GwyFixedLine *pslset;
    gdouble linedata[4];
    GwyDataField *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                     controls->mydata, "/0/data"));
    
    gwy_selection_get_object(selection, i, linedata);

    if (fixed_line_present(dfield, linedata[0], linedata[1], linedata[2], linedata[3], 
                     controls->args->evaluator->fixed_line_array)) return;
   
    if (i == controls->args->evaluator->fixed_line_array->len) {
        pslset = gwy_fixed_line_new();  
        pslset->id = g_strdup_printf("fl%d", ++controls->fixed_line_max);
        pslset->xstart = linedata[0];
        pslset->ystart = linedata[1];
        pslset->xend = linedata[2];
        pslset->yend = linedata[3];

        g_ptr_array_add(controls->args->evaluator->fixed_line_array, pslset);

        gtk_list_store_insert_with_values(controls->relative_list, &iter, -1,
                       NO_COLUMN, pslset->id,
                       DESCRIPTION_COLUMN, "les",
                       TYPE_COLUMN, "line",
                       -1);
    } else {
        pslset = g_ptr_array_index(controls->args->evaluator->fixed_line_array, i);
        pslset->xstart = linedata[0];
        pslset->ystart = linedata[1];
        pslset->xend = linedata[2];
        pslset->yend = linedata[3];

    }
    gtk_widget_set_sensitive(controls->relative_remove_button, TRUE);
 }


static void
cpoints_selection_changed_cb(GwySelection *selection, gint i, EsetupControls *controls)
{
    GtkTreeIter iter;
    GwyCorrelationPoint *pspset;
    gdouble pointdata[2];
    GwyDataField *dfield;
    gint xstart, ystart;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                                     controls->mydata, "/0/data"));

    gwy_selection_get_object(selection, i, pointdata);
    
    if (i == controls->args->evaluator->correlation_point_array->len) {
        /*new point added*/
        pspset = gwy_correlation_point_new();
        pspset->xc = pointdata[0];
        pspset->yc = pointdata[1];
        pspset->height = 50;
        pspset->width = 50;
        pspset->sheight = 100;
        pspset->swidth = 100;
        xstart = gwy_data_field_rtoi(dfield, pspset->xc) - pspset->width/2;
        ystart = gwy_data_field_rtoj(dfield, pspset->yc) - pspset->height/2;

        xstart = GWY_CLAMP(xstart, 0, gwy_data_field_get_xres(dfield) - pspset->width - 1);
        ystart = GWY_CLAMP(ystart, 0, gwy_data_field_get_yres(dfield) - pspset->height - 1);
        
        pspset->pattern = gwy_data_field_area_extract(dfield, xstart, ystart, 
                                                      pspset->width, pspset->height);
        
        pspset->id = g_strdup_printf("cp%d", ++controls->correlation_point_max);     
        g_ptr_array_add(controls->args->evaluator->correlation_point_array, pspset);

        gtk_list_store_insert_with_values(controls->correlation_list, &iter, -1,
                       NO_COLUMN, pspset->id,
                       DESCRIPTION_COLUMN, "des",
                       TYPE_COLUMN, "point",
                       -1);
    } else {
        /*old point moved*/
        pspset = g_ptr_array_index(controls->args->evaluator->correlation_point_array, i);
        pspset->xc = pointdata[0];
        pspset->yc = pointdata[1];
    }
    gtk_widget_set_sensitive(controls->correlation_remove_button, TRUE);
    gtk_widget_set_sensitive(controls->correlation_edit_button, TRUE);
 
}

static void        
selections_unref(EsetupControls *controls)
{
    g_object_unref(controls->vlayer_dpoint);
    g_object_unref(controls->vlayer_dline);
    g_object_unref(controls->vlayer_spoint);
    g_object_unref(controls->vlayer_sline);
    g_object_unref(controls->vlayer_fpoint);
    g_object_unref(controls->vlayer_fline);
    g_object_unref(controls->vlayer_cpoint);
}


static gint
get_num_from_id(gchar *id, gchar *initid)
{
    return (gint)g_ascii_strtod(id + strlen(initid), NULL);
}

static void        
update_after_load(EsetupControls *controls)
{
    GwySearchPoint *spset;
    GwySearchLine *slset;
    GwyFixedPoint *fpset;
    GwyFixedLine *flset;
    GwyCorrelationPoint *cpset;
    GwyEvaluatorTask *etset;
    gint i, num; 
    GtkTreeIter iter;
    gdouble pointseldata[2], lineseldata[4];
    GwySelection *selection;
    
    update_selected_points(controls);
    update_selected_lines(controls);

    for (i=0; i<controls->args->evaluator->detected_point_array->len; i++)
    {
        spset = g_ptr_array_index(controls->args->evaluator->detected_point_array, i);
        
        num = get_num_from_id(spset->id, "sp");
        g_array_index(controls->detected_point_chosen, gboolean, num) = TRUE;
        
        gtk_list_store_insert_with_values(controls->detected_list, &iter, -1,
                                           NO_COLUMN, spset->id,
                                           DESCRIPTION_COLUMN, "des",
                                           TYPE_COLUMN, "point",
                                           -1);
    }
    for (i=0; i<controls->args->evaluator->detected_line_array->len; i++)
    {
        slset = g_ptr_array_index(controls->args->evaluator->detected_line_array, i);
        
        num = get_num_from_id(spset->id, "sl");
        g_array_index(controls->detected_line_chosen, gboolean, num) = TRUE;
        
        gtk_list_store_insert_with_values(controls->detected_list, &iter, -1,
                                           NO_COLUMN, slset->id,
                                           DESCRIPTION_COLUMN, "des",
                                           TYPE_COLUMN, "line",
                                           -1);
    }

    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_fpoint));
    gwy_selection_set_max_objects(selection, MAX(1, controls->args->evaluator->fixed_point_array->len));
    gwy_selection_clear(selection);
    for (i=0; i<controls->args->evaluator->fixed_point_array->len; i++)
    {
        fpset = g_ptr_array_index(controls->args->evaluator->fixed_point_array, i);
        
        
        gtk_list_store_insert_with_values(controls->relative_list, &iter, -1,
                                           NO_COLUMN, fpset->id,
                                           DESCRIPTION_COLUMN, "des",
                                           TYPE_COLUMN, "point",
                                           -1);
        if (!gwy_selection_is_full(selection))
        {
            pointseldata[0] = fpset->xc;
            pointseldata[1] = fpset->yc;
            gwy_selection_set_object(selection, i, pointseldata);
        }
    }

    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_fline));
    gwy_selection_set_max_objects(selection, MAX(1, controls->args->evaluator->fixed_line_array->len));
    gwy_selection_clear(selection);
    for (i=0; i<controls->args->evaluator->fixed_line_array->len; i++)
    {
        flset = g_ptr_array_index(controls->args->evaluator->fixed_line_array, i);
        
        
        gtk_list_store_insert_with_values(controls->relative_list, &iter, -1,
                                           NO_COLUMN, flset->id,
                                           DESCRIPTION_COLUMN, "des",
                                           TYPE_COLUMN, "line",
                                           -1);
        if (!gwy_selection_is_full(selection))
        {
            lineseldata[0] = flset->xstart;
            lineseldata[1] = flset->ystart;
            lineseldata[2] = flset->xend;
            lineseldata[3] = flset->xend;
            gwy_selection_set_object(selection, i, lineseldata);
        }
    }
    
    selection = gwy_container_get_object_by_name(controls->mydata,
                                       gwy_vector_layer_get_selection_key(controls->vlayer_cpoint));
    gwy_selection_set_max_objects(selection, MAX(1, controls->args->evaluator->correlation_point_array->len));
    gwy_selection_clear(selection);
    for (i=0; i<controls->args->evaluator->correlation_point_array->len; i++)
    {
        cpset = g_ptr_array_index(controls->args->evaluator->correlation_point_array, i);
        
        
        gtk_list_store_insert_with_values(controls->correlation_list, &iter, -1,
                                           NO_COLUMN, cpset->id,
                                           DESCRIPTION_COLUMN, "des",
                                           TYPE_COLUMN, "point",
                                           -1);
        if (!gwy_selection_is_full(selection))
        {
            pointseldata[0] = cpset->xc;
            pointseldata[1] = cpset->yc;
            gwy_selection_set_object(selection, i, pointseldata);
        }
    }

    for (i=0; i<controls->args->evaluator->expression_task_array->len; i++)
    {
        etset = g_ptr_array_index(controls->args->evaluator->expression_task_array, i);
        
        gtk_list_store_insert_with_values(controls->evaluator_list, &iter, -1,
                                           NO_COLUMN, etset->id,
                                           TYPE_COLUMN, etset->expression,
                                           DESCRIPTION_COLUMN, etset->threshold,
                                           -1);
    }


 
}


static void        
evaluator_load_cb(EsetupControls *controls)
{
    GtkDialog *filedialog;
    gchar *filename;
    GError *err = NULL;
    guchar *buffer = NULL;
    gsize size = 0;        
    gsize pos = 0;
    GString *string = g_string_new("");

    filedialog = GTK_DIALOG(gtk_file_chooser_dialog_new ("Load evaluator",
                                                         GTK_WINDOW(GTK_DIALOG(controls->pdialog)),
                                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                         NULL));
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filedialog),
                                         gwy_app_get_current_directory());
    if (gtk_dialog_run (GTK_DIALOG (filedialog)) == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filedialog));
        if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
            printf("get contetns failed\n");
                    return;
        }

        controls->args->evaluator = GWY_EVALUATOR(gwy_serializable_deserialize(buffer, size, &pos));
        if (!controls->args->evaluator) {
            printf("deserialize failed\n");
            return;
        
        }
        update_after_load(controls);

        
    }
    gtk_widget_destroy(GTK_WIDGET(filedialog));
}

static void        
evaluator_save_cb(EsetupControls *controls)
{
    GtkDialog *filedialog;
    GError *err = NULL;
    gchar *filename;
    GByteArray *buffer;
    GString *string = g_string_new("");
    FILE *fh;

    filedialog = GTK_DIALOG(gtk_file_chooser_dialog_new ("Export evaluator",
                                                         GTK_WINDOW(GTK_DIALOG(controls->pdialog)),
                                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                         GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                         NULL));
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filedialog),
                                         gwy_app_get_current_directory());
    if (gtk_dialog_run (GTK_DIALOG (filedialog)) == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filedialog));
        if (gwy_app_file_confirm_overwrite(GTK_WIDGET(filedialog)))
        {
            if (!(fh = g_fopen(filename, "wb"))) {
                printf("open failed\n");
                return;
            }
            buffer = gwy_serializable_serialize(G_OBJECT(controls->args->evaluator), NULL);
            if (fwrite(buffer->data, 1, buffer->len, fh) != buffer->len) {
                printf("write failed\n");
                return;
            }
            fclose(fh);
            g_byte_array_free(buffer, TRUE);
        }
    }
    gtk_widget_destroy(GTK_WIDGET(filedialog));
}


static void
esetup_load_args(GwyContainer *container,
               EsetupArgs *args)
{
    *args = esetup_defaults;
}

static void
esetup_save_args(GwyContainer *container,
               EsetupArgs *args)
{
}




/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
