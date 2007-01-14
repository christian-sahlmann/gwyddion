/*
 *  @(#) $Id$
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include "convolutionfilterpreset.h"

#define CONVOLUTION_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400
};

typedef struct {
    GwyConvolutionFilterPreset *preset;
} ConvolutionArgs;

typedef struct {
    ConvolutionArgs *args;
    GwyContainer *mydata;
    GSList *sizes;
    GSList *hsym;
    GSList *vsym;
    GtkWidget *dialog;
    GtkWidget *filter_page;
    GtkWidget *view;
    GtkWidget *matrix_parent;
    GtkWidget *matrix;
    GtkWidget **coeff;
    GtkWidget *divisor;
    GtkWidget *divisor_auto;
    GtkWidget *delete_preset;
    GtkTreeSelection *selection;
    GwyInventoryStore *presets;
    gboolean in_update;
    GQuark position_quark;
    gboolean computed;
} ConvolutionControls;

static gboolean module_register                 (void);
static void convolution_filter                  (GwyContainer *data,
                                                 GwyRunType run);
static void convolution_filter_dialog           (ConvolutionArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 gint id,
                                                 GQuark dquark);
static GtkWidget* convolution_filter_create_filter_tab(ConvolutionControls *controls);
static GtkWidget* convolution_filter_create_preset_tab(ConvolutionControls *controls);
static void convolution_filter_switch_preset    (GtkTreeSelection *selection,
                                                 ConvolutionControls *controls);
static void convolution_filter_preset_delete    (ConvolutionControls *controls);
static void convolution_filter_preset_duplicate (ConvolutionControls *controls);
static void convolution_filter_preset_new       (ConvolutionControls *controls);
static void convolution_filter_preset_copy      (ConvolutionControls *controls,
                                                 const gchar *name,
                                                 const gchar *newname);
static gboolean convolution_filter_preset_save  (GwyConvolutionFilterPreset *preset);
static void convolution_filter_preset_name_edited(ConvolutionControls *controls,
                                                  const gchar *strpath,
                                                  const gchar *text);
static void convolution_filter_preview          (ConvolutionControls *controls);
static void convolution_filter_run_noninteractive(ConvolutionArgs *args,
                                                  GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  GQuark dquark);
static void convolution_filter_fetch_coeff      (ConvolutionControls *controls);
static void convolution_filter_hsym_changed     (GtkToggleButton *button,
                                                 ConvolutionControls *controls);
static void convolution_filter_vsym_changed     (GtkToggleButton *button,
                                                 ConvolutionControls *controls);
static void convolution_filter_size_changed     (GtkToggleButton *button,
                                                 ConvolutionControls *controls);
static void convolution_filter_divisor_changed  (GtkEntry *entry,
                                                 ConvolutionControls *controls);
static void convolution_filter_autodiv_changed  (GtkToggleButton *check,
                                                 ConvolutionControls *controls);
static void convolution_filter_update_divisor   (ConvolutionControls *controls);
static void convolution_filter_resize_matrix    (ConvolutionControls *controls);
static void convolution_filter_update_symmetry  (ConvolutionControls *controls);
static void convolution_filter_update_matrix    (ConvolutionControls *controls);
static gboolean convolution_filter_coeff_unfocus(GtkEntry *entry,
                                                 GdkEventFocus *event,
                                                 ConvolutionControls *controls);
static void convolution_filter_coeff_changed    (GtkEntry *entry,
                                                 ConvolutionControls *controls);
static void convolution_filter_symmetrize       (ConvolutionControls *controls);
static void convolution_filter_set_value        (ConvolutionControls *controls,
                                                 guint j,
                                                 guint i,
                                                 gdouble val);
static void convolution_filter_load_args        (GwyContainer *settings,
                                                 ConvolutionArgs *args);
static void convolution_filter_save_args        (GwyContainer *settings,
                                                 ConvolutionArgs *args);

static const GwyEnum symmetries[] = {
    { N_("None"), CONVOLUTION_FILTER_SYMMETRY_NONE, },
    { N_("Even"), CONVOLUTION_FILTER_SYMMETRY_EVEN, },
    { N_("Odd"),  CONVOLUTION_FILTER_SYMMETRY_ODD,  },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generic convolution filter with a user-defined matrix."),
    "Yeti <yeti@gwyddion.net>",
    "1.1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    static gint types_initialized = 0;

    if (!types_initialized) {
        GwyResourceClass *klass;

        types_initialized += gwy_convolution_filter_preset_get_type();
        gwy_convolution_filter_preset_class_setup_presets();
        klass = g_type_class_peek(GWY_TYPE_CONVOLUTION_FILTER_PRESET);
        gwy_resource_class_load(klass);
    }

    gwy_process_func_register("convolution_filter",
                              (GwyProcessFunc)&convolution_filter,
                              N_("/_Integral Transforms/Con_volution Filter..."),
                              NULL,
                              CONVOLUTION_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("General convolution filter"));

    return TRUE;
}

static void
use_filter(G_GNUC_UNUSED gpointer i,
           gpointer item,
           G_GNUC_UNUSED gpointer user_data)
{
    gwy_resource_use(GWY_RESOURCE(item));
}

static void
release_filter(G_GNUC_UNUSED gpointer i,
               gpointer item,
               G_GNUC_UNUSED gpointer user_data)
{
    gwy_resource_release(GWY_RESOURCE(item));
}

static void
convolution_filter(GwyContainer *data,
                   GwyRunType run)
{
    ConvolutionArgs args;
    GwyResourceClass *rklass;
    GwyDataField *dfield;
    GQuark dquark;
    gint id;

    g_return_if_fail(run & CONVOLUTION_RUN_MODES);

    rklass = g_type_class_peek(GWY_TYPE_CONVOLUTION_FILTER_PRESET);
    gwy_resource_class_mkdir(rklass);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && dquark);

    convolution_filter_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        gwy_inventory_foreach(gwy_convolution_filter_presets(), use_filter,
                              NULL);
        convolution_filter_dialog(&args, data, dfield, id, dquark);
        convolution_filter_save_args(gwy_app_settings_get(), &args);
        gwy_inventory_foreach(gwy_convolution_filter_presets(), release_filter,
                              NULL);
    }
    else
        convolution_filter_run_noninteractive(&args, data, dfield, dquark);
}

static void
convolution_filter_dialog(ConvolutionArgs *args,
                          GwyContainer *data,
                          GwyDataField *dfield,
                          gint id,
                          GQuark dquark)
{
    enum { RESPONSE_PREVIEW = 1 };

    ConvolutionControls controls;
    GwyPixmapLayer *layer;
    GtkWidget *dialog, *hbox, *vbox, *notebook, *label;
    GtkWidget *align;
    gdouble zoomval;
    gint response;

    controls.args = args;
    controls.computed = FALSE;
    controls.position_quark = g_quark_from_static_string("position");

    dialog = gtk_dialog_new_with_buttons(_("Convolution Filter"), NULL, 0,
                                         NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       TRUE, TRUE, 4);

    /* Preview */
    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);
    gtk_container_add(GTK_CONTAINER(align), controls.view);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(hbox), notebook, TRUE, TRUE, 4);

    /* Filter */
    vbox = convolution_filter_create_filter_tab(&controls);
    label = gtk_label_new(_("Filter"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
    controls.filter_page = vbox;

    /* Presets */
    vbox = convolution_filter_create_preset_tab(&controls);
    label = gtk_label_new(_("Presets"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        convolution_filter_fetch_coeff(&controls);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_PREVIEW:
            convolution_filter_preview(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    if (controls.computed) {
        dfield = gwy_container_get_object_by_name(controls.mydata, "/0/data");
        gwy_app_undo_qcheckpointv(data, 1, &dquark);
        gwy_container_set_object(data, dquark, dfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        convolution_filter_run_noninteractive(args, data, dfield, dquark);
    }
}

static GtkWidget*
convolution_filter_create_filter_tab(ConvolutionControls *controls)
{
    GtkWidget *table, *vbox, *hbox2, *vbox2, *label;
    GwyEnum *sizes;
    guint i, nsizes;
    gchar buf[16];

    nsizes = (CONVOLUTION_MAX_SIZE - CONVOLUTION_MIN_SIZE)/2 + 1;
    sizes = g_new0(GwyEnum, nsizes + 1);
    for (i = 0; i < nsizes; i++) {
        sizes[i].value = CONVOLUTION_MIN_SIZE + 2*i;
        sizes[i].name = g_strdup_printf("%s%d × %d",
                                        sizes[i].value < 11 ? "_" : "",
                                        sizes[i].value, sizes[i].value);
    }

    vbox = gtk_vbox_new(FALSE, 0);

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    table = gtk_table_new(1 + G_N_ELEMENTS(sizes), 1, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox2), table, FALSE, FALSE, 0);

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Size")),
                     0, 1, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls->sizes
        = gwy_radio_buttons_create(sizes, nsizes,
                                   G_CALLBACK(convolution_filter_size_changed),
                                   controls,
                                   controls->args->preset->data.size);
    gwy_radio_buttons_attach_to_table(controls->sizes, GTK_TABLE(table), 1, 1);
    g_signal_connect_swapped(GTK_WIDGET(controls->sizes->data), "destroy",
                             G_CALLBACK(gwy_enum_freev), sizes);

    vbox2 = gtk_vbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 4);
    gtk_box_pack_start(GTK_BOX(hbox2), vbox2, TRUE, TRUE, 0);
    controls->matrix_parent = vbox2;

    gtk_box_pack_start(GTK_BOX(vbox2),
                       gwy_label_new_header(_("Coefficient Matrix")),
                       FALSE, FALSE, 0);

    controls->matrix = gtk_table_new(1, 1, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox2), controls->matrix, TRUE, TRUE, 0);

    table = gtk_table_new(1, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

    controls->divisor = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(controls->divisor), 8);
    g_snprintf(buf, sizeof(buf), "%.8g", controls->args->preset->data.divisor);
    gtk_entry_set_text(GTK_ENTRY(controls->divisor), buf);
    gtk_table_attach(GTK_TABLE(table), controls->divisor,
                     1, 2, 0, 1, 0, 0, 0, 0);
    g_signal_connect(controls->divisor, "changed",
                     G_CALLBACK(convolution_filter_divisor_changed), controls);

    label = gtk_label_new_with_mnemonic(_("_Divisor:"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, 0, 1, 0, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), label);

    controls->divisor_auto = gtk_check_button_new_with_mnemonic(_("_automatic"));
    gtk_table_attach(GTK_TABLE(table), controls->divisor_auto,
                     2, 3, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls->divisor_auto, "toggled",
                     G_CALLBACK(convolution_filter_autodiv_changed), controls);

    vbox2 = gtk_vbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 4);
    gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox2), gwy_label_new_header(_("Symmetry")),
                       FALSE, FALSE, 0);

    hbox2 = gtk_hbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox2, TRUE, TRUE, 0);

    table = gtk_table_new(4, 1, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(hbox2), table, FALSE, FALSE, 0);

    label = gtk_label_new(_("Horizontal"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

    controls->hsym
        = gwy_radio_buttons_create(symmetries, G_N_ELEMENTS(symmetries),
                                   G_CALLBACK(convolution_filter_hsym_changed),
                                   controls,
                                   controls->args->preset->hsym);
    gwy_radio_buttons_attach_to_table(controls->hsym, GTK_TABLE(table), 1, 1);

    table = gtk_table_new(4, 1, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(hbox2), table, FALSE, FALSE, 0);

    label = gtk_label_new(_("Vertical"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

    controls->vsym
        = gwy_radio_buttons_create(symmetries, G_N_ELEMENTS(symmetries),
                                   G_CALLBACK(convolution_filter_vsym_changed),
                                   controls,
                                   controls->args->preset->vsym);
    gwy_radio_buttons_attach_to_table(controls->vsym, GTK_TABLE(table), 1, 1);

    return vbox;
}

static void
render_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer data)
{
    GwyResource *resource;

    gtk_tree_model_get(model, iter, 0, &resource, -1);
    g_object_set(renderer,
                 "editable", gwy_resource_get_is_modifiable(resource),
                 NULL);
}

static void
render_size(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer data)
{
    GwyConvolutionFilterPreset *preset;
    gchar buf[16];

    gtk_tree_model_get(model, iter, 0, &preset, -1);
    g_snprintf(buf, sizeof(buf), "%u", preset->data.size);
    g_object_set(renderer, "text", buf, NULL);
}

static void
render_symmetry(G_GNUC_UNUSED GtkTreeViewColumn *column,
                GtkCellRenderer *renderer,
                GtkTreeModel *model,
                GtkTreeIter *iter,
                gpointer data)
{
    GwyConvolutionFilterPreset *preset;
    ConvolutionFilterSymmetryType sym;
    const gchar *str;

    gtk_tree_model_get(model, iter, 0, &preset, -1);
    if (GPOINTER_TO_INT(data) == GWY_ORIENTATION_HORIZONTAL)
        sym = preset->hsym;
    else
        sym = preset->vsym;
    str = gwy_enum_to_string(sym, symmetries, G_N_ELEMENTS(symmetries));
    g_object_set(renderer, "text", str, NULL);
}

static GtkWidget*
convolution_filter_create_preset_tab(ConvolutionControls *controls)
{
    static const struct {
        const gchar *stock_id;
        const gchar *tooltip;
        GCallback callback;
    }
    toolbar_buttons[] = {
        {
            GTK_STOCK_NEW,
            N_("Create a new item"),
            G_CALLBACK(convolution_filter_preset_new),
        },
        {
            GTK_STOCK_COPY,
            N_("Create a new item based on selected one"),
            G_CALLBACK(convolution_filter_preset_duplicate),
        },
        {
            GTK_STOCK_DELETE,
            N_("Delete selected item"),
            G_CALLBACK(convolution_filter_preset_delete),
        },
    };

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkWidget *vbox, *treeview, *hbox, *image, *button;
    GtkTreeIter iter;
    GtkTooltips *tooltips;
    const gchar *name;
    guint i;

    vbox = gtk_vbox_new(FALSE, 0);

    controls->presets
        = gwy_inventory_store_new(gwy_convolution_filter_presets());
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(controls->presets));
    g_object_unref(controls->presets);
    gtk_box_pack_start(GTK_BOX(vbox), treeview, TRUE, TRUE, 0);

    /* Name */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable-set", TRUE, NULL);
    i = gwy_inventory_store_get_column_by_name(controls->presets, "name");
    column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer,
                                                      "text", i,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_name, NULL, NULL);
    g_signal_connect_swapped(renderer, "edited",
                             G_CALLBACK(convolution_filter_preset_name_edited),
                             controls);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Size */
    /* GwyConvolutionFilterPreset could register size as a property, but
     * that would be a bit of overkill. */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Size"), renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_size, NULL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Horizontal symmetry */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("HSym"), renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
                (column, renderer,
                 render_symmetry, GINT_TO_POINTER(GWY_ORIENTATION_HORIZONTAL),
                 NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Vertical symmetry */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("VSym"), renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
                (column, renderer,
                 render_symmetry, GINT_TO_POINTER(GWY_ORIENTATION_VERTICAL),
                 NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* Selection */
    controls->selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(controls->selection, GTK_SELECTION_BROWSE);
    name = gwy_resource_get_name(GWY_RESOURCE(controls->args->preset));
    gwy_inventory_store_get_iter(controls->presets, name, &iter);
    g_signal_connect(controls->selection, "changed",
                     G_CALLBACK(convolution_filter_switch_preset), controls);

    /* Controls */
    tooltips = gwy_app_get_tooltips();
    hbox = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    for (i = 0; i < G_N_ELEMENTS(toolbar_buttons); i++) {
        image = gtk_image_new_from_stock(toolbar_buttons[i].stock_id,
                                         GTK_ICON_SIZE_LARGE_TOOLBAR);
        button = gtk_button_new();
        gtk_container_add(GTK_CONTAINER(button), image);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_tooltips_set_tip(tooltips, button,
                             toolbar_buttons[i].tooltip, NULL);
        g_signal_connect_swapped(button, "clicked",
                                 toolbar_buttons[i].callback, controls);

        if (toolbar_buttons[i].callback
            == G_CALLBACK(convolution_filter_preset_delete))
            controls->delete_preset = button;
    }

    /* This causes sync of all other controls */
    gtk_tree_selection_select_iter(controls->selection, &iter);

    return vbox;
}

static void
convolution_filter_switch_preset(GtkTreeSelection *selection,
                                 ConvolutionControls *controls)
{
    GwyConvolutionFilterPreset *preset;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gboolean sensitive;

    /* Can hapen in a transitional state? */
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    convolution_filter_preset_save(controls->args->preset);

    gtk_tree_model_get(model, &iter, 0, &preset, -1);
    controls->args->preset = preset;

    controls->in_update = TRUE;
    gwy_radio_buttons_set_current(controls->sizes, preset->data.size);
    controls->in_update = FALSE;
    convolution_filter_resize_matrix(controls);
    convolution_filter_update_matrix(controls);
    convolution_filter_update_symmetry(controls);
    controls->in_update = TRUE;
    gwy_radio_buttons_set_current(controls->hsym, preset->hsym);
    gwy_radio_buttons_set_current(controls->vsym, preset->vsym);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->divisor_auto),
                                 preset->data.auto_divisor);
    controls->in_update = FALSE;
    convolution_filter_update_divisor(controls);
    controls->computed = FALSE;

    sensitive = gwy_resource_get_is_modifiable(GWY_RESOURCE(preset));
    gtk_widget_set_sensitive(controls->filter_page, sensitive);
    gtk_widget_set_sensitive(controls->delete_preset, sensitive);
}

static void
convolution_filter_preset_delete(ConvolutionControls *controls)
{
    GwyResource *resource;
    GwyInventory *inventory;
    GtkTreePath *path;
    GtkTreeIter iter;
    const gchar *name;
    gchar *filename;
    gint result;

    resource = GWY_RESOURCE(controls->args->preset);
    inventory = gwy_convolution_filter_presets();
    name = gwy_resource_get_name(resource);

    /* Delete the resource file */
    filename = gwy_resource_build_filename(resource);
    result = g_remove(filename);
    if (result) {
        /* FIXME: GUIze this */
        g_warning("Resource (%s) could not be deleted.", name);
        g_free(filename);
        return;
    }
    g_free(filename);

    /* Delete the resource from the inventory */
    gwy_inventory_store_get_iter(controls->presets, name, &iter);
    path = gtk_tree_model_get_path(GTK_TREE_MODEL(controls->presets), &iter);
    gwy_inventory_delete_item(inventory, name);
    gtk_tree_selection_select_path(controls->selection, path);
    gtk_tree_path_free(path);
}

static void
convolution_filter_preset_new(ConvolutionControls *controls)
{
    convolution_filter_preset_copy(controls,
                                   GWY_CONVOLUTION_FILTER_PRESET_DEFAULT,
                                   _("Untitled"));
}

static void
convolution_filter_preset_duplicate(ConvolutionControls *controls)
{
    const gchar *name;

    name = gwy_resource_get_name(GWY_RESOURCE(controls->args->preset));
    convolution_filter_preset_copy(controls, name, NULL);
}

static void
convolution_filter_preset_copy(ConvolutionControls *controls,
                               const gchar *name,
                               const gchar *newname)
{
    GwyResource *resource;
    GtkTreeIter iter;

    gwy_debug("<%s> -> <%s>", name, newname);
    resource = gwy_inventory_new_item(gwy_convolution_filter_presets(),
                                      name, newname);
    gwy_resource_use(resource);
    gwy_inventory_store_get_iter(controls->presets,
                                 gwy_resource_get_name(resource), &iter);
    gtk_tree_selection_select_iter(controls->selection, &iter);
}

static gboolean
convolution_filter_preset_save(GwyConvolutionFilterPreset *preset)
{
    GwyResource *resource;
    GString *str;
    gchar *filename;
    FILE *fh;

    resource = GWY_RESOURCE(preset);
    if (!resource->is_modified)
        return TRUE;

    if (!gwy_resource_get_is_modifiable(resource)) {
        g_warning("Non-modifiable resource was modified and is about to be "
                  "saved");
        return FALSE;
    }

    filename = gwy_resource_build_filename(resource);
    fh = g_fopen(filename, "w");
    if (!fh) {
        /* FIXME: GUIze this */
        g_warning("Cannot save resource file: %s", filename);
        g_free(filename);
        return FALSE;
    }
    g_free(filename);

    str = gwy_resource_dump(resource);
    fwrite(str->str, 1, str->len, fh);
    fclose(fh);
    g_string_free(str, TRUE);

    return TRUE;
}

static void
convolution_filter_preset_name_edited(ConvolutionControls *controls,
                                      const gchar *strpath,
                                      const gchar *text)
{
    GwyResource *resource, *item;
    GwyInventory *inventory;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    const gchar *s;
    gchar *oldname, *newname, *oldfilename, *newfilename;

    gwy_debug("path: <%s>, text: <%s>", strpath, text);
    newname = g_newa(gchar, strlen(text)+1);
    strcpy(newname, text);
    g_strstrip(newname);
    gwy_debug("newname: <%s>", newname);

    model = GTK_TREE_MODEL(controls->presets);
    path = gtk_tree_path_new_from_string(strpath);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);
    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    s = gwy_resource_get_name(resource);
    oldname = g_newa(gchar, strlen(s)+1);
    strcpy(oldname, s);
    gwy_debug("oldname: <%s>", oldname);
    if (gwy_strequal(newname, oldname))
        return;

    inventory = gwy_convolution_filter_presets();
    item = gwy_inventory_get_item(inventory, newname);
    if (item)
        return;

    convolution_filter_preset_save(GWY_CONVOLUTION_FILTER_PRESET(resource));
    oldfilename = gwy_resource_build_filename(resource);
    gwy_inventory_rename_item(inventory, oldname, newname);
    newfilename = gwy_resource_build_filename(resource);
    if (g_rename(oldfilename, newfilename) != 0) {
        /* FIXME: GUIze this */
        g_warning("Cannot rename resource file: %s to %s",
                  oldfilename, newfilename);
        gwy_inventory_rename_item(inventory, newname, oldname);
    }
    g_free(oldfilename);
    g_free(newfilename);

    gwy_inventory_store_get_iter(controls->presets, newname, &iter);
    gtk_tree_selection_select_iter(controls->selection, &iter);
}

static void
convolution_filter_preview(ConvolutionControls *controls)
{
    GwyDataField *original, *preview, *kernel;
    GwyConvolutionFilterPresetData *pdata;

    /* Avoid temporary data fields until the user actually clicks on Update,
     * then move the original from /0/data to /1/data and create a preview
     * data field at /0/data. */
    if (!gwy_container_gis_object_by_name(controls->mydata, "/1/data",
                                          &original)) {
        original = gwy_container_get_object_by_name(controls->mydata,
                                                    "/0/data");
        gwy_container_set_object_by_name(controls->mydata, "/1/data", original);
        preview = gwy_data_field_duplicate(original);
        gwy_container_set_object_by_name(controls->mydata, "/0/data", preview);
        g_object_unref(preview);
    }
    else {
        preview = gwy_container_get_object_by_name(controls->mydata, "/0/data");
        gwy_data_field_copy(original, preview, FALSE);
    }

    pdata = &controls->args->preset->data;
    kernel = gwy_data_field_new(pdata->size, pdata->size, 1.0, 1.0, FALSE);
    memcpy(gwy_data_field_get_data(kernel), pdata->matrix,
           pdata->size*pdata->size*sizeof(gdouble));
    gwy_data_field_convolve(preview, kernel);
    g_object_unref(kernel);

    gwy_data_field_data_changed(preview);
}

static void
convolution_filter_run_noninteractive(ConvolutionArgs *args,
                                      GwyContainer *data,
                                      GwyDataField *dfield,
                                      GQuark dquark)
{
    GwyConvolutionFilterPresetData *pdata;
    GwyDataField *kernel;

    gwy_app_undo_qcheckpointv(data, 1, &dquark);

    pdata = &args->preset->data;
    kernel = gwy_data_field_new(pdata->size, pdata->size, 1.0, 1.0, FALSE);
    memcpy(gwy_data_field_get_data(kernel), pdata->matrix,
           pdata->size*pdata->size*sizeof(gdouble));
    gwy_data_field_convolve(dfield, kernel);
    g_object_unref(kernel);
    gwy_data_field_data_changed(dfield);
}

static void
convolution_filter_fetch_coeff(ConvolutionControls *controls)
{
    GtkWidget *entry;

    entry = gtk_window_get_focus(GTK_WINDOW(controls->dialog));
    if (entry
        && GTK_IS_ENTRY(entry)
        && gtk_widget_get_parent(entry) == controls->matrix)
        convolution_filter_coeff_changed(GTK_ENTRY(entry), controls);
}

static void
convolution_filter_hsym_changed(G_GNUC_UNUSED GtkToggleButton *button,
                                ConvolutionControls *controls)
{
    if (controls->in_update)
        return;

    controls->args->preset->hsym
        = gwy_radio_buttons_get_current(controls->hsym);
    convolution_filter_symmetrize(controls);
    convolution_filter_update_symmetry(controls);
    controls->computed = FALSE;
    gwy_resource_data_changed(GWY_RESOURCE(controls->args->preset));
}

static void
convolution_filter_vsym_changed(G_GNUC_UNUSED GtkToggleButton *button,
                                ConvolutionControls *controls)
{
    if (controls->in_update)
        return;

    controls->args->preset->vsym
        = gwy_radio_buttons_get_current(controls->vsym);
    convolution_filter_symmetrize(controls);
    convolution_filter_update_symmetry(controls);
    controls->computed = FALSE;
    gwy_resource_data_changed(GWY_RESOURCE(controls->args->preset));
}

static void
convolution_filter_size_changed(G_GNUC_UNUSED GtkToggleButton *button,
                                ConvolutionControls *controls)
{
    guint newsize;

    if (controls->in_update)
        return;

    newsize = gwy_radio_buttons_get_current(controls->sizes);
    gwy_convolution_filter_preset_data_resize(&controls->args->preset->data,
                                              newsize);
    convolution_filter_resize_matrix(controls);
    convolution_filter_update_matrix(controls);
    convolution_filter_update_symmetry(controls);
    controls->computed = FALSE;
    gwy_resource_data_changed(GWY_RESOURCE(controls->args->preset));
}

static void
convolution_filter_divisor_changed(GtkEntry *entry,
                                   ConvolutionControls *controls)
{
    if (controls->in_update)
        return;

    controls->args->preset->data.divisor = g_strtod(gtk_entry_get_text(entry),
                                                    NULL);
    controls->computed = FALSE;
    gwy_resource_data_changed(GWY_RESOURCE(controls->args->preset));
}

static void
convolution_filter_autodiv_changed(GtkToggleButton *check,
                                   ConvolutionControls *controls)
{
    gboolean autodiv;

    if (controls->in_update)
        return;

    autodiv = gtk_toggle_button_get_active(check);
    controls->args->preset->data.auto_divisor = autodiv;
    gtk_widget_set_sensitive(controls->divisor, !autodiv);
    if (!autodiv)
        return;

    gwy_convolution_filter_preset_data_autodiv(&controls->args->preset->data);
    convolution_filter_update_divisor(controls);
    controls->computed = FALSE;
    gwy_resource_data_changed(GWY_RESOURCE(controls->args->preset));
}

static void
convolution_filter_update_divisor(ConvolutionControls *controls)
{
    gchar buf[16];

    controls->in_update = TRUE;
    g_snprintf(buf, sizeof(buf), "%.8g", controls->args->preset->data.divisor);
    gtk_entry_set_text(GTK_ENTRY(controls->divisor), buf);
    controls->in_update = FALSE;
}

static void
convolution_filter_resize_matrix(ConvolutionControls *controls)
{
    GtkTable *table;
    guint size, cols, i;

    size = controls->args->preset->data.size;
    g_object_get(controls->matrix, "n-columns", &cols, NULL);
    if (cols == size)
        return;

    gtk_widget_destroy(controls->matrix);
    controls->matrix = gtk_table_new(size, size, TRUE);
    controls->coeff = g_new(GtkWidget*, size*size);
    g_signal_connect_swapped(controls->matrix, "destroy",
                             G_CALLBACK(g_free), controls->coeff);
    table = GTK_TABLE(controls->matrix);
    for (i = 0; i < size*size; i++) {
        controls->coeff[i] = gtk_entry_new();
        g_object_set_qdata(G_OBJECT(controls->coeff[i]),
                           controls->position_quark, GUINT_TO_POINTER(i));
        gtk_entry_set_width_chars(GTK_ENTRY(controls->coeff[i]), 5);
        gtk_table_attach(table, controls->coeff[i],
                         i % size, i % size + 1, i/size, i/size + 1,
                         GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
        g_signal_connect(controls->coeff[i], "activate",
                         G_CALLBACK(convolution_filter_coeff_changed),
                         controls);
        g_signal_connect(controls->coeff[i], "focus-out-event",
                         G_CALLBACK(convolution_filter_coeff_unfocus),
                         controls);
    }
    gtk_box_pack_start(GTK_BOX(controls->matrix_parent), controls->matrix,
                       TRUE, TRUE, 0);
    gtk_widget_show_all(controls->matrix);
}

static void
convolution_filter_update_symmetry(ConvolutionControls *controls)
{
    ConvolutionArgs *args;
    guint i, size;
    gboolean sensitive;

    args = controls->args;
    size = args->preset->data.size;

    sensitive = (args->preset->vsym != CONVOLUTION_FILTER_SYMMETRY_ODD);
    for (i = 0; i < size; i++)
        gtk_widget_set_sensitive(controls->coeff[size/2*size + i], sensitive);

    sensitive = (args->preset->hsym != CONVOLUTION_FILTER_SYMMETRY_ODD);
    for (i = 0; i < size; i++)
        gtk_widget_set_sensitive(controls->coeff[i*size + size/2], sensitive);

    sensitive = (args->preset->vsym != CONVOLUTION_FILTER_SYMMETRY_ODD)
                && (args->preset->hsym != CONVOLUTION_FILTER_SYMMETRY_ODD);
    gtk_widget_set_sensitive(controls->coeff[(size/2)*size + size/2],
                             sensitive);
}

static gboolean
convolution_filter_coeff_unfocus(GtkEntry *entry,
                                 G_GNUC_UNUSED GdkEventFocus *event,
                                 ConvolutionControls *controls)
{
    convolution_filter_coeff_changed(entry, controls);
    return FALSE;
}

static void
convolution_filter_coeff_changed(GtkEntry *entry,
                                 ConvolutionControls *controls)
{
    guint size, i;
    gchar *end;
    gdouble val;

    if (controls->in_update)
        return;

    i = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(entry),
                                            controls->position_quark));
    val = g_strtod(gtk_entry_get_text(entry), &end);
    if (val == controls->args->preset->data.matrix[i])
        return;

    controls->in_update = TRUE;
    size = controls->args->preset->data.size;
    convolution_filter_set_value(controls, i % size, i/size, val);
    controls->in_update = FALSE;
    controls->computed = FALSE;
    gwy_resource_data_changed(GWY_RESOURCE(controls->args->preset));

    if (!controls->args->preset->data.auto_divisor)
        return;

    gwy_convolution_filter_preset_data_autodiv(&controls->args->preset->data);
    convolution_filter_update_divisor(controls);
}

static void
convolution_filter_symmetrize(ConvolutionControls *controls)
{
    ConvolutionFilterSymmetryType hsym, vsym;
    const gdouble *matrix;
    gdouble val;
    guint i, j, size;

    matrix = controls->args->preset->data.matrix;
    size = controls->args->preset->data.size;
    hsym = controls->args->preset->hsym;
    vsym = controls->args->preset->vsym;

    controls->in_update = TRUE;
    if (hsym) {
        if (vsym) {
            for (i = 0; i <= size/2; i++) {
                for (j = 0; j <= size/2; j++) {
                    val = matrix[i*size + j];
                    if (hsym == CONVOLUTION_FILTER_SYMMETRY_ODD
                        && j == size/2)
                        val = 0.0;
                    if (vsym == CONVOLUTION_FILTER_SYMMETRY_ODD
                        && i == size/2)
                        val = 0.0;
                    convolution_filter_set_value(controls, j, i, val);
                }
            }
        }
        else {
            for (i = 0; i < size; i++) {
                for (j = 0; j <= size/2; j++) {
                    val = matrix[i*size + j];
                    if (hsym == CONVOLUTION_FILTER_SYMMETRY_ODD
                        && j == size/2)
                        val = 0.0;
                    convolution_filter_set_value(controls, j, i, val);
                }
            }
        }
    }
    else {
        if (vsym) {
            for (i = 0; i <= size/2; i++) {
                for (j = 0; j < size; j++) {
                    val = matrix[i*size + j];
                    if (vsym == CONVOLUTION_FILTER_SYMMETRY_ODD
                        && i == size/2)
                        val = 0.0;
                    convolution_filter_set_value(controls, j, i, val);
                }
            }
        }
        else {
            /* Do nothing */
        }
    }
    controls->in_update = FALSE;
}

static void
convolution_filter_do_set_value(ConvolutionControls *controls,
                                guint j,
                                guint i,
                                gdouble val)
{
    gchar buf[16];
    GwyConvolutionFilterPresetData *pdata;

    pdata = &controls->args->preset->data;
    g_return_if_fail(i < pdata->size);
    g_return_if_fail(j < pdata->size);

    pdata->matrix[i*pdata->size + j] = val;
    /* Fix `negative zeroes' */
    if (val == 0.0)
        val = fabs(val);
    g_snprintf(buf, sizeof(buf), "%.8g", val);
    gtk_entry_set_text(GTK_ENTRY(controls->coeff[i*pdata->size + j]), buf);
}

static void
convolution_filter_update_matrix(ConvolutionControls *controls)
{
    GwyConvolutionFilterPresetData *pdata;
    guint i, j;

    controls->in_update = TRUE;
    pdata = &controls->args->preset->data;
    for (i = 0; i < pdata->size; i++) {
        for (j = 0; j < pdata->size; j++) {
            convolution_filter_do_set_value(controls, j, i,
                                            pdata->matrix[pdata->size*i + j]);
        }
    }
    controls->in_update = FALSE;
}

static void
convolution_filter_set_value(ConvolutionControls *controls,
                             guint j,
                             guint i,
                             gdouble val)
{
    ConvolutionFilterSymmetryType hsym, vsym;
    guint size;

    size = controls->args->preset->data.size;
    hsym = controls->args->preset->hsym;
    vsym = controls->args->preset->vsym;

    convolution_filter_do_set_value(controls, j, i, val);
    if (hsym == CONVOLUTION_FILTER_SYMMETRY_EVEN) {
        convolution_filter_do_set_value(controls, size-1-j, i, val);
        if (vsym == CONVOLUTION_FILTER_SYMMETRY_EVEN) {
            convolution_filter_do_set_value(controls, j, size-1-i, val);
            convolution_filter_do_set_value(controls, size-1-j, size-1-i, val);
        }
        else if (vsym == CONVOLUTION_FILTER_SYMMETRY_ODD) {
            convolution_filter_do_set_value(controls, j, size-1-i, -val);
            convolution_filter_do_set_value(controls, size-1-j, size-1-i, -val);
        }
    }
    else if (hsym == CONVOLUTION_FILTER_SYMMETRY_ODD) {
        convolution_filter_do_set_value(controls, size-1-j, i, -val);
        if (vsym == CONVOLUTION_FILTER_SYMMETRY_EVEN) {
            convolution_filter_do_set_value(controls, j, size-1-i, val);
            convolution_filter_do_set_value(controls, size-1-j, size-1-i, -val);
        }
        else if (vsym == CONVOLUTION_FILTER_SYMMETRY_ODD) {
            convolution_filter_do_set_value(controls, j, size-1-i, -val);
            convolution_filter_do_set_value(controls, size-1-j, size-1-i, val);
        }
    }
    else {
        if (vsym == CONVOLUTION_FILTER_SYMMETRY_EVEN)
            convolution_filter_do_set_value(controls, j, size-1-i, val);
        else if (vsym == CONVOLUTION_FILTER_SYMMETRY_ODD)
            convolution_filter_do_set_value(controls, j, size-1-i, -val);
    }
}

static const gchar preset_key[] = "/module/convolution_filter/preset";

static void
convolution_filter_load_args(GwyContainer *settings,
                             ConvolutionArgs *args)
{
    GwyInventory *presets;
    const guchar *name;

    memset(args, 0, sizeof(ConvolutionArgs));
    presets = gwy_convolution_filter_presets();
    if (gwy_container_gis_string_by_name(settings, preset_key, &name)) {
        if ((args->preset = gwy_inventory_get_item(presets, name)))
            return;
    }

    name = GWY_CONVOLUTION_FILTER_PRESET_DEFAULT;
    args->preset = gwy_inventory_get_item(presets, name);
}

static void
convolution_filter_save_args(GwyContainer *settings,
                             ConvolutionArgs *args)
{
    gchar *name;

    convolution_filter_preset_save(args->preset);
    name = g_strdup(gwy_resource_get_name(GWY_RESOURCE(args->preset)));
    gwy_container_set_string_by_name(settings, preset_key, name);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
