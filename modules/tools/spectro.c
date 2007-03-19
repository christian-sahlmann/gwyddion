/*
 *  @(#) $Id: spectro.c 6785 2006-10-15 22:20:49Z yeti-dn $
 *  Copyright (C) 2003-2006 Owain Davies, David Necas (Yeti), Petr Klapetek.
 *  E-mail: owain.davies@blueyonder.co.uk, yeti@gwyddion.net,
 *          klapetek@gwyddion.net.
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
#define DEBUG


#include "config.h"
#include <string.h>
#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/datafield.h>
#include <libprocess/linestats.h>
#include <libprocess/spectra.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_SPECTRO            (gwy_tool_spectro_get_type())
#define GWY_TOOL_SPECTRO(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_SPECTRO, GwyToolSpectro))
#define GWY_IS_TOOL_SPECTRO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_SPECTRO))
#define GWY_TOOL_SPECTRO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_SPECTRO, GwyToolSpectroClass))

enum {
    NLINES = 18,
    MAX_THICKNESS = 128,
    MIN_RESOLUTION = 4,
    MAX_RESOLUTION = 1024
};

enum {
    COLUMN_I, COLUMN_X, COLUMN_Y, NCOLUMNS
};

typedef struct _GwyToolSpectro      GwyToolSpectro;
typedef struct _GwyToolSpectroClass GwyToolSpectroClass;

typedef struct {
    gboolean options_visible;
    gint thickness;
    gint resolution;
    gboolean fixres;
    GwyInterpolationType interpolation;
    gboolean separate;
} ToolArgs;

struct _GwyToolSpectro {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkTreeView *treeview;
    GtkTreeModel *model;

    GwyDataLine *line;
    GtkWidget *graph;
    GwyGraphModel *gmodel;
    GwySpectra *spectra;

    GtkWidget *options;
    GtkObject *thickness;
    GtkObject *resolution;
    GtkWidget *fixres;
    GtkWidget *interpolation;
    GtkWidget *separate;
    GtkWidget *apply;
    GType layer_type;
    gulong layer_object_chosen_id;

    /* potential class data */
    GwySIValueFormat *coord_format;

};

struct _GwyToolSpectroClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType  gwy_tool_spectro_get_type             (void) G_GNUC_CONST;
static void   gwy_tool_spectro_finalize             (GObject *object);
static void   gwy_tool_spectro_init_dialog          (GwyToolSpectro *tool);
static void   gwy_tool_spectro_data_switched        (GwyTool *gwytool,
                                                     GwyDataView *data_view);
static void   gwy_tool_spectro_response             (GwyTool *tool,
                                                     gint response_id);
static void   gwy_tool_spectro_data_changed         (GwyPlainTool *plain_tool);
static void   gwy_tool_spectro_selection_changed    (GwyPlainTool *plain_tool,
                                                     gint hint);
static void   gwy_tool_spectro_tree_sel_changed     (GtkTreeSelection *selection,
                                                     gpointer data);
static void   gwy_tool_spectro_object_chosen        (GwyVectorLayer *gwyvectorlayer,
                                                     gint i,
                                                     gpointer *data);
static void   gwy_tool_spectro_show_curve           (GwyToolSpectro *tool,
                                                     gint i);
static void   gwy_tool_spectro_update_all_curves    (GwyToolSpectro *tool);
static void   gwy_tool_spectro_render_cell          (GtkCellLayout *layout,
                                                     GtkCellRenderer *renderer,
                                                     GtkTreeModel *model,
                                                     GtkTreeIter *iter,
                                                     gpointer user_data);
static void   gwy_tool_spectro_options_expanded     (GtkExpander *expander,
                                                     GParamSpec *pspec,
                                                     GwyToolSpectro *tool);
static void   gwy_tool_spectro_thickness_changed    (GwyToolSpectro *tool,
                                                     GtkAdjustment *adj);
static void   gwy_tool_spectro_resolution_changed   (GwyToolSpectro *tool,
                                                     GtkAdjustment *adj);
static void   gwy_tool_spectro_fixres_changed       (GtkToggleButton *check,
                                                     GwyToolSpectro *tool);
static void   gwy_tool_spectro_separate_changed     (GtkToggleButton *check,
                                                     GwyToolSpectro *tool);
static void   gwy_tool_spectro_interpolation_changed(GtkComboBox *combo,
                                                     GwyToolSpectro *tool);
static void   gwy_tool_spectro_apply                (GwyToolSpectro *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Point Spectrum, extracts point spectra to a graph."),
    "Owain Davies <owain.davies@blueyonder.co.uk>",
    "0.1",
    "Owain Davies, David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

static const gchar fixres_key[]          = "/module/spectro/fixres";
static const gchar interpolation_key[]   = "/module/spectro/interpolation";
static const gchar options_visible_key[] = "/module/spectro/options_visible";
static const gchar resolution_key[]      = "/module/spectro/resolution";
static const gchar separate_key[]        = "/module/spectro/separate";
static const gchar thickness_key[]       = "/module/spectro/thickness";

static const ToolArgs default_args = {
    FALSE,
    1,
    120,
    FALSE,
    GWY_INTERPOLATION_BILINEAR,
    FALSE,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolSpectro, gwy_tool_spectro, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_SPECTRO);

    return TRUE;
}

static void
gwy_tool_spectro_class_init(GwyToolSpectroClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_spectro_finalize;

    tool_class->stock_id = GWY_STOCK_SPECTRUM;
    tool_class->title = _("Point Spectroscopy");
    tool_class->tooltip = _("Extract and View Point Spectroscopy Data.");
    tool_class->prefix = "/module/spectro";
    tool_class->default_width = 640;
    tool_class->default_height = 400;
    tool_class->data_switched = gwy_tool_spectro_data_switched;
    tool_class->response = gwy_tool_spectro_response;

    ptool_class->data_changed = gwy_tool_spectro_data_changed;
    ptool_class->selection_changed = gwy_tool_spectro_selection_changed;
}

static void
gwy_tool_spectro_finalize(GObject *object)
{
    GwyToolSpectro *tool;
    GwyPlainTool *plain_tool;
    GwyContainer *settings;
    
    plain_tool = GWY_PLAIN_TOOL(object);
    tool = GWY_TOOL_SPECTRO(object);

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_set_boolean_by_name(settings, options_visible_key,
                                      tool->args.options_visible);
    gwy_container_set_int32_by_name(settings, thickness_key,
                                    tool->args.thickness);
    gwy_container_set_int32_by_name(settings, resolution_key,
                                    tool->args.resolution);
    gwy_container_set_boolean_by_name(settings, fixres_key,
                                      tool->args.fixres);
    gwy_container_set_enum_by_name(settings, interpolation_key,
                                   tool->args.interpolation);
    gwy_container_set_boolean_by_name(settings, separate_key,
                                      tool->args.separate);

    if (tool->line)
        gwy_object_unref(tool->line);
    
    if (tool->model) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        gwy_object_unref(tool->model);
    }
    if (tool->coord_format)
        gwy_si_unit_value_format_free(tool->coord_format);
    if (tool->spectra)
        g_object_unref(tool->spectra);
    gwy_debug("");
    gwy_debug("id: %d", tool->layer_object_chosen_id);
    if (tool->layer_object_chosen_id>0){
        g_signal_handler_disconnect(plain_tool->layer,
                                    tool->layer_object_chosen_id);
        tool->layer_object_chosen_id=0;
    }
    G_OBJECT_CLASS(gwy_tool_spectro_parent_class)->finalize(object);
}

static void
gwy_tool_spectro_init(GwyToolSpectro *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type = gwy_plain_tool_check_layer_type(plain_tool,
                                                       "GwyLayerPoint");
    if (!tool->layer_type)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;
    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_boolean_by_name(settings, options_visible_key,
                                      &tool->args.options_visible);
    gwy_container_gis_int32_by_name(settings, thickness_key,
                                    &tool->args.thickness);
    gwy_container_gis_int32_by_name(settings, resolution_key,
                                    &tool->args.resolution);
    gwy_container_gis_boolean_by_name(settings, fixres_key,
                                      &tool->args.fixres);
    gwy_container_gis_enum_by_name(settings, interpolation_key,
                                   &tool->args.interpolation);
    gwy_container_gis_boolean_by_name(settings, separate_key,
                                      &tool->args.separate);

    tool->coord_format = NULL;
    tool->spectra = NULL;

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type,
                                     "spec");
    gwy_tool_spectro_init_dialog(tool);
}

static void
gwy_tool_spectro_init_dialog(GwyToolSpectro *tool)
{
    static const gchar *column_titles[] = {
        "<b>n</b>",
        "<b>x</b>",
        "<b>y</b>",
        "<b>visible</b>",
    };
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkDialog *dialog;
    GtkWidget *scwin, *label, *hbox, *vbox, *hbox2;
    GtkTable *table;
    GtkListStore *store;
    GtkTreeSelection *select;
    guint i, row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, TRUE, TRUE, 0);

    /* Left pane */
    vbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    /* Point coordinates */
    store = gtk_list_store_new(NCOLUMNS, 
                               G_TYPE_UINT,
                               G_TYPE_DOUBLE,
                               G_TYPE_DOUBLE);
    tool->model = GTK_TREE_MODEL(store);
    tool->treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(tool->model));
    
    for (i = 0; i < NCOLUMNS; i++) {
        column = gtk_tree_view_column_new();
        gtk_tree_view_column_set_expand(column, TRUE);
        gtk_tree_view_column_set_alignment(column, 0.5);
        g_object_set_data(G_OBJECT(column), "id", GUINT_TO_POINTER(i));
        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "xalign", 1.0, NULL);
        if (i == COLUMN_I)
            g_object_set(renderer, "foreground-set", TRUE, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                           gwy_tool_spectro_render_cell, tool,
                                           NULL);
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), column_titles[i]);
        gtk_tree_view_column_set_widget(column, label);
        gtk_widget_show(label);
        gtk_tree_view_append_column(tool->treeview, column);
    }

    select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tool->treeview));
    gtk_tree_selection_set_mode (select, GTK_SELECTION_MULTIPLE);
    g_signal_connect (G_OBJECT (select), "changed",
                      G_CALLBACK (gwy_tool_spectro_tree_sel_changed),
                      tool);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scwin), GTK_WIDGET(tool->treeview));
    gtk_box_pack_start(GTK_BOX(vbox), scwin, TRUE, TRUE, 0);

    /*  */

    /* Options */
    tool->options = gtk_expander_new(_("<b>Options</b>"));
    gtk_expander_set_use_markup(GTK_EXPANDER(tool->options), TRUE);
    gtk_expander_set_expanded(GTK_EXPANDER(tool->options),
                              tool->args.options_visible);
    g_signal_connect(tool->options, "notify::expanded",
                     G_CALLBACK(gwy_tool_spectro_options_expanded), tool);
    gtk_box_pack_start(GTK_BOX(vbox), tool->options, FALSE, FALSE, 0);

    table = GTK_TABLE(gtk_table_new(5, 4, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(tool->options), GTK_WIDGET(table));
    row = 0;

    tool->thickness = gtk_adjustment_new(tool->args.thickness,
                                         1, MAX_THICKNESS, 1, 10, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Thickness:"), NULL,
                            tool->thickness, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(tool->thickness, "value-changed",
                             G_CALLBACK(gwy_tool_spectro_thickness_changed),
                             tool);
    row++;

    tool->resolution = gtk_adjustment_new(tool->args.resolution,
                                          MIN_RESOLUTION, MAX_RESOLUTION,
                                          1, 10, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Fix res.:"), NULL,
                            tool->resolution,
                            GWY_HSCALE_CHECK | GWY_HSCALE_SQRT);
    g_signal_connect_swapped(tool->resolution, "value-changed",
                             G_CALLBACK(gwy_tool_spectro_resolution_changed),
                             tool);
    tool->fixres = gwy_table_hscale_get_check(tool->resolution);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->fixres),
                                 tool->args.fixres);
    g_signal_connect(tool->fixres, "toggled",
                     G_CALLBACK(gwy_tool_spectro_fixres_changed), tool);
    gwy_table_hscale_set_sensitive(tool->resolution, tool->args.fixres);
    row++;

    tool->separate
        = gtk_check_button_new_with_mnemonic(_("_Separate spectros"));
    gtk_table_attach(table, tool->separate,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->separate),
                                 tool->args.separate);
    g_signal_connect(tool->separate, "toggled",
                     G_CALLBACK(gwy_tool_spectro_separate_changed), tool);
    row++;

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_table_attach(table, hbox2,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    tool->interpolation = gwy_enum_combo_box_new
                            (gwy_interpolation_type_get_enum(), -1,
                             G_CALLBACK(gwy_tool_spectro_interpolation_changed),
                             tool,
                             tool->args.interpolation, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), tool->interpolation);
    gtk_box_pack_end(GTK_BOX(hbox2), tool->interpolation, FALSE, FALSE, 0);
    row++;

    tool->gmodel = gwy_graph_model_new();
    g_object_set(tool->gmodel, "title", _("Spectroscopy"), NULL);

    tool->graph = gwy_graph_new(tool->gmodel);
    g_object_unref(tool->gmodel); /* The GwyGraph takes a ref */

    gwy_graph_enable_user_input(GWY_GRAPH(tool->graph), FALSE);
    g_object_set(tool->gmodel, "label-visible", FALSE, NULL);
    gtk_box_pack_start(GTK_BOX(hbox), tool->graph, TRUE, TRUE, 2);

    //gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_spectro_data_switched(GwyTool *gwytool,
                               GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolSpectro *tool;
    GwyPixmapLayer *blayer;
    GwySpectra *spectra;
    GtkListStore *store;
    GtkTreeViewColumn *column;
    GtkLabel *label;
    GwySIUnit *siunit;
    GwySIValueFormat *vf;
    guint len, i;
    const gchar *data_key;
    gchar *key, *column_title;
    gchar ext[]="spec/0";
    const gdouble *coords=NULL;
    gboolean spec_found = FALSE;

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    tool = GWY_TOOL_SPECTRO(gwytool);
        
    if (plain_tool->init_failed)
        return;

    if(data_view==plain_tool->data_view) {
        GWY_TOOL_CLASS(gwy_tool_spectro_parent_class)->data_switched(gwytool,
                                                                     data_view);
        return;
    }

    gwy_debug("disconect obj-chosen handler: %d",
              tool->layer_object_chosen_id);
    if (tool->layer_object_chosen_id > 0) {
        g_signal_handler_disconnect(plain_tool->layer,
                                    tool->layer_object_chosen_id);
        tool->layer_object_chosen_id = 0;
    }

    GWY_TOOL_CLASS(gwy_tool_spectro_parent_class)->data_switched(gwytool,
                                                                 data_view);
    if (plain_tool->layer) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type,
                                "editable", FALSE,
                                "focus", -1,
                                NULL);
    }
    if (data_view) {
        
        
        tool->layer_object_chosen_id =
                g_signal_connect(G_OBJECT (plain_tool->layer),
                                 "object-chosen",
                                 G_CALLBACK (gwy_tool_spectro_object_chosen),
                                 tool);
        
        blayer = gwy_data_view_get_base_layer(data_view);
        data_key = gwy_pixmap_layer_get_data_key(blayer);
        len=strlen(data_key);
        /* FIXME: This is going to need to be able to deal with more than just
                  "spec/0" */
        len+=2; /* space to add "/0" */
        key = g_new0(gchar, len+1);
        strcpy(key, data_key);
        gwy_debug("key: %s",key);
        strcpy(key+len-6, ext);
        gwy_debug("key: %s",key);
        spec_found = gwy_container_gis_object_by_name(plain_tool->container,
                                                      key,
                                                      &spectra);
        gwy_debug("Spectra %sfound @ %s", spec_found?"":" not", key);
    }
    if (spec_found) {
        g_return_if_fail(GWY_IS_SPECTRA(spectra));
        g_object_ref(spectra);
        if (tool->spectra)
            g_object_unref(tool->spectra);
        tool->spectra=spectra;

        gwy_selection_set_max_objects(plain_tool->selection, spectra->ncurves);
        coords=gwy_spectra_itoxy(spectra, 0);
        gwy_selection_set_data(plain_tool->selection,
                               gwy_spectra_n_spectra(spectra),
                               spectra->coords);
        siunit = gwy_spectra_get_si_unit_xy(spectra);

        /* XXX: Tidy this up. */
        vf = gwy_si_unit_get_format_with_resolution(siunit,
                                                    GWY_SI_UNIT_FORMAT_PLAIN,
                                                    gwy_data_field_get_xreal(plain_tool->data_field),
                                                    gwy_data_field_get_xreal(plain_tool->data_field)/gwy_data_field_get_xres(plain_tool->data_field),
                                                    tool->coord_format);

        tool->coord_format = vf;

        store = GTK_LIST_STORE(tool->model);
        gtk_tree_view_set_model(tool->treeview, NULL);
        gtk_list_store_clear(store);
        for (i=0; i < gwy_spectra_n_spectra(tool->spectra); i++) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               COLUMN_I, i,
                               COLUMN_X, coords[2*i],
                               COLUMN_Y, coords[2*i+1],
                               -1);
           gwy_debug("Added: %d,%g,%g", i, coords[2*i], coords[2*i+1]);
        }
        gtk_tree_view_set_model(tool->treeview, tool->model);

        column_title = g_strconcat("<b>x</b> [",vf->units,"]",NULL);
        gwy_debug("%s", column_title);    
        column = gtk_tree_view_get_column(tool->treeview, COLUMN_X);
        label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
        gtk_label_set_markup(label, column_title);
        g_free(column_title);

        column_title = g_strconcat("<b>y</b> [",vf->units,"]",NULL);
        column = gtk_tree_view_get_column(tool->treeview, COLUMN_Y);
        label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
        gtk_label_set_markup(label, column_title);
        g_free(column_title);

    } 

    if (!spec_found) {
        if (tool->spectra) {
            g_object_unref(tool->spectra);
            tool->spectra = NULL;
        }
        if (tool->coord_format)
            gwy_si_unit_value_format_free(tool->coord_format);
        tool->coord_format = NULL;
        store = GTK_LIST_STORE(tool->model);
        gtk_tree_view_set_model(tool->treeview, NULL);
        gtk_list_store_clear(store);
        gtk_tree_view_set_model(tool->treeview, tool->model);
    }

    gwy_graph_model_remove_all_curves(tool->gmodel);
    gwy_tool_spectro_update_all_curves(tool);
}

static void
gwy_tool_spectro_response(GwyTool *tool,
                          gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_spectro_parent_class)->response(tool, response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_spectro_apply(GWY_TOOL_SPECTRO(tool));
}

static void
gwy_tool_spectro_data_changed(GwyPlainTool *plain_tool)
{
/*    GwyToolSpectro *tool;
    GtkWidget *spin;
    gint m;

    tool = GWY_TOOL_SPECTRO(plain_tool);
    if (plain_tool->data_field) {
        spin = gwy_table_hscale_get_middle_widget(tool->resolution);
        m = MAX(gwy_data_field_get_xres(plain_tool->data_field),
                gwy_data_field_get_yres(plain_tool->data_field));
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(spin), MIN_RESOLUTION, m);
    }

    gwy_tool_spectro_update_all_curves(tool);*/
}

static void
gwy_tool_spectro_selection_changed(GwyPlainTool *plain_tool,
                                   gint hint)
{
/*    GwyToolSpectro *tool;
    GtkListStore *store;
    gint n;

    tool = GWY_TOOL_SPECTRO(plain_tool);
    store = GTK_LIST_STORE(tool->model);
    n = gwy_null_store_get_n_rows(store);
    g_return_if_fail(hint <= n);

    if (hint < 0) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        if (plain_tool->selection)
            n = gwy_selection_get_data(plain_tool->selection, NULL);
        else
            n = 0;
        gwy_null_store_set_n_rows(store, n);
        gtk_tree_view_set_model(tool->treeview, tool->model);
        gwy_graph_model_remove_all_curves(tool->gmodel);
        gwy_tool_spectro_update_all_curves(tool);
    }
    else {
        if (hint < n)
            gwy_null_store_row_changed(store, hint);
        else
            gwy_null_store_set_n_rows(store, n+1);
        gwy_tool_spectro_update_curve(tool, hint);
        n++;
    }

    gtk_widget_set_sensitive(tool->apply, n > 0);
*/
}

static void
gwy_tool_spectro_tree_sel_changed (GtkTreeSelection *selection,
                                   gpointer data)
{
    GwyToolSpectro *tool;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GList *selected, *item;
    guint i;

    g_return_if_fail(GWY_IS_TOOL_SPECTRO(data));
    tool=GWY_TOOL_SPECTRO(data);

    gwy_debug("");
    gwy_graph_model_remove_all_curves(tool->gmodel);
    selected = gtk_tree_selection_get_selected_rows(selection, &model);
    item = selected;
    while (item) {
        gtk_tree_model_get_iter(model, &iter, (GtkTreePath*)item->data);
        gtk_tree_model_get (model, &iter, COLUMN_I, &i, -1);
        gwy_tool_spectro_show_curve(tool, i);
        item = g_list_next(item);
    }
    g_list_foreach (selected, gtk_tree_path_free, NULL);
    g_list_free (selected);

}

static void
gwy_tool_spectro_object_chosen (GwyVectorLayer *gwyvectorlayer,
                                gint i,
                                gpointer *data)
{
    GwyToolSpectro *tool;
    GtkTreePath *treepath = NULL;
    GtkTreeSelection *selection;

    g_return_if_fail(GWY_IS_TOOL_SPECTRO(data));
    tool = GWY_TOOL_SPECTRO(data);
    
    if (i < 0) 
        return;
    gwy_debug("obj-chosen: %d", i);
    
    treepath = gtk_tree_path_new_from_indices(i,-1);
    selection = gtk_tree_view_get_selection(tool->treeview);
    gtk_tree_selection_select_path (selection, treepath);
    gtk_tree_path_free(treepath);
}

static void
gwy_tool_spectro_show_curve(GwyToolSpectro *tool,
                              gint id)
{
    const GwyRGBA *rgba;
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *gcmodel=NULL;
    gint i, n, lineres;
    gchar *desc;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);
    
    tool->line = gwy_spectra_get_spectrum(tool->spectra, id);

    if (!tool->args.fixres) {
        lineres = gwy_data_line_get_res(tool->line);
        lineres = MAX(lineres, MIN_RESOLUTION);
    }
    else
        lineres = tool->args.resolution;

    n = gwy_graph_model_get_n_curves(tool->gmodel);

    for(i = 0; i < n; i++){
        guint idx;
        gcmodel = gwy_graph_model_get_curve(tool->gmodel, i);
        idx = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(gcmodel), "sid"));
        if (idx==id)
            break;
        else
            gcmodel = NULL;
    }

    if (gcmodel) {
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line, 0, 0);
    } else {
        gcmodel = gwy_graph_curve_model_new();
        g_object_set_data(G_OBJECT(gcmodel), "sid", GUINT_TO_POINTER(id));
        desc = g_strdup_printf(_("Spectrum %d"), id+1);
        rgba = gwy_graph_get_preset_color(n);
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "description", desc,
                     "color", rgba,
                     NULL);
        g_free(desc);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, tool->line, 0, 0);
        gwy_graph_model_add_curve(tool->gmodel, gcmodel);
        g_object_unref(gcmodel);

        if (n == 0)
            gwy_graph_model_set_units_from_data_line(tool->gmodel, tool->line);
    }
    g_object_unref(tool->line);
    tool->line = NULL;
}

static void
gwy_tool_spectro_update_all_curves(GwyToolSpectro *tool)
{
/*    GwyPlainTool *plain_tool;
    gint n, i;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (!plain_tool->selection
        || !(n = gwy_selection_get_data(plain_tool->selection, NULL))) {
        gwy_graph_model_remove_all_curves(tool->gmodel);
        return;
    }

    for (i = 0; i < n; i++)
        gwy_tool_spectro_update_curve(tool, i);*/
}

static void
gwy_tool_spectro_render_cell(GtkCellLayout *layout,
                             GtkCellRenderer *renderer,
                             GtkTreeModel *model,
                             GtkTreeIter *iter,
                             gpointer user_data)
{
    GwyToolSpectro *tool = (GwyToolSpectro*)user_data;
    const GwySIValueFormat *vf;
    gchar buf[32];
    gdouble val;
    guint idx, id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(layout), "id"));
    gtk_tree_model_get(model, iter, 0, &idx, -1);
    if (id == COLUMN_I) {
        g_snprintf(buf, sizeof(buf), "%d", idx + 1);
        g_object_set(renderer, "text", buf, NULL);
        return;
    }

    vf = tool->coord_format;
    switch (id) {
        case COLUMN_X:
        case COLUMN_Y:
        gtk_tree_model_get(model, iter, id, &val, -1);        
        break;

        default:
        g_return_if_reached();
        break;
    }

    if (vf) {
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, val/(vf->magnitude));
    }
    else
        g_snprintf(buf, sizeof(buf), "%.3g", val);

    g_object_set(renderer, "text", buf, NULL);
}

static void
gwy_tool_spectro_options_expanded(GtkExpander *expander,
                                  G_GNUC_UNUSED GParamSpec *pspec,
                                  GwyToolSpectro *tool)
{
    tool->args.options_visible = gtk_expander_get_expanded(expander);
}

static void
gwy_tool_spectro_thickness_changed(GwyToolSpectro *tool,
                                   GtkAdjustment *adj)
{
    tool->args.thickness = gwy_adjustment_get_int(adj);
    gwy_tool_spectro_update_all_curves(tool);
}

static void
gwy_tool_spectro_resolution_changed(GwyToolSpectro *tool,
                                    GtkAdjustment *adj)
{
    tool->args.resolution = gwy_adjustment_get_int(adj);
    /* Resolution can be changed only when fixres == TRUE */
    gwy_tool_spectro_update_all_curves(tool);
}

static void
gwy_tool_spectro_fixres_changed(GtkToggleButton *check,
                                GwyToolSpectro *tool)
{
    tool->args.fixres = gtk_toggle_button_get_active(check);
    gwy_tool_spectro_update_all_curves(tool);
}

static void
gwy_tool_spectro_separate_changed(GtkToggleButton *check,
                                  GwyToolSpectro *tool)
{
    tool->args.separate = gtk_toggle_button_get_active(check);
}

static void
gwy_tool_spectro_interpolation_changed(GtkComboBox *combo,
                                       GwyToolSpectro *tool)
{
    tool->args.interpolation = gwy_enum_combo_box_get_active(combo);
    gwy_tool_spectro_update_all_curves(tool);
}

static void
gwy_tool_spectro_apply(GwyToolSpectro *tool)
{
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *gcmodel;
    GwyGraphModel *gmodel;
    gchar *s;
    gint i, n;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->selection);
    n = gwy_graph_model_get_n_curves(tool->gmodel);
    g_return_if_fail(n);

    if (!tool->args.separate) {
        gmodel = gwy_graph_model_duplicate(tool->gmodel);
        g_object_set(gmodel, "label-visible", TRUE, NULL);
        gwy_app_data_browser_add_graph_model(gmodel, plain_tool->container,
                                             TRUE);
        g_object_unref(gmodel);
        return;
    }

    for (i = 0; i < n; i++) {
        gmodel = gwy_graph_model_new_alike(tool->gmodel);
        g_object_set(gmodel, "label-visible", TRUE, NULL);
        gcmodel = gwy_graph_model_get_curve(tool->gmodel, i);
        gcmodel = gwy_graph_curve_model_duplicate(gcmodel);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
        g_object_get(gcmodel, "description", &s, NULL);
        g_object_set(gmodel, "title", s, NULL);
        g_free(s);
        gwy_app_data_browser_add_graph_model(gmodel, plain_tool->container,
                                             TRUE);
        g_object_unref(gmodel);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

