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

#include <math.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>


/* Data for this function.*/

typedef struct {
    GtkWidget *preference;
    GtkWidget *check_labels;
    GtkWidget *check_units;
    GtkWidget *check_metadata;

    gboolean units;
    gboolean labels;
    gboolean metadata;
    GwyGraphModelExportStyle style;

    GwyGraphModel *model;
} ExportControls;


static gboolean module_register          (const gchar *name);
static gboolean export                   (GwyGraph *graph);
static gboolean export_dialog            (GwyGraph *graph);
static void     export_dialog_closed_cb  (GwyGraph *graph);
static void     export_dialog_response_cb(GtkDialog *pdialog,
                                          gint response,
                                          GwyGraph *graph);
static void     units_changed_cb         (ExportControls *pcontrols);
static void     labels_changed_cb        (ExportControls *pcontrols);
static void     metadata_changed_cb      (ExportControls *pcontrols);
static void     style_cb                 (GtkWidget *combo);
static void     load_args                (GwyContainer *container,
                                          ExportControls *pcontrols);
static void     save_args                (GwyContainer *container,
                                          ExportControls *pcontrols);


GwyEnum style_type[] = {
   { N_("Plain text"),             GWY_GRAPH_MODEL_EXPORT_ASCII_PLAIN,   },
   { N_("Gnuplot friendly"),       GWY_GRAPH_MODEL_EXPORT_ASCII_GNUPLOT, },
   { N_("Comma separated values"), GWY_GRAPH_MODEL_EXPORT_ASCII_CSV,     },
   { N_("Origin friendly"),        GWY_GRAPH_MODEL_EXPORT_ASCII_ORIGIN,  },
};

static GtkWidget *dialog = NULL;
static ExportControls controls;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Graph ASCII export."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyGraphFuncInfo export_func_info = {
        "graph_export_ascii",
        N_("/Export _ASCII"),
        (GwyGraphFunc)&export,
    };

    gwy_graph_func_register(name, &export_func_info);

    return TRUE;
}

static gboolean
export(GwyGraph *graph)
{
    GwyContainer *settings;

    if (!graph) {
        if (dialog)
            gtk_widget_destroy(dialog);
        dialog = NULL;
        return TRUE;
    }
    settings = gwy_app_settings_get();
    load_args(settings, &controls);


    if (!dialog)
        export_dialog(graph);

    return TRUE;
}


static gboolean
export_dialog(GwyGraph *graph)
{
    controls.model = graph->graph_model;

    dialog = gtk_dialog_new_with_buttons(_("Export ASCII"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    g_signal_connect_swapped(dialog, "delete_event",
                             G_CALLBACK(export_dialog_closed_cb), graph);
    g_signal_connect(dialog, "response",
                           G_CALLBACK(export_dialog_response_cb),
                           graph);

    g_signal_connect_swapped(graph, "destroy",
                             G_CALLBACK(export_dialog_closed_cb), graph);


    controls.preference = gwy_enum_combo_box_new(style_type, G_N_ELEMENTS(style_type),
                                                       G_CALLBACK(style_cb), dialog,
                                                       controls.style, TRUE);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                                                       controls.preference);


    controls.check_labels = gtk_check_button_new_with_mnemonic(_("Export _labels"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.check_labels), controls.labels);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                                             controls.check_labels);
    g_signal_connect_swapped(controls.check_labels, "clicked",
                                             G_CALLBACK(labels_changed_cb), &controls);
    controls.check_units = gtk_check_button_new_with_mnemonic(_("Export _units"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.check_units), controls.units);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                                                controls.check_units);
    g_signal_connect_swapped(controls.check_units, "clicked",
                                                G_CALLBACK(units_changed_cb), &controls);
    controls.check_metadata = gtk_check_button_new_with_mnemonic(_("Export _metadata"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.check_metadata), controls.metadata);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                                                controls.check_metadata);
    g_signal_connect_swapped(controls.check_metadata, "clicked",
                                                G_CALLBACK(metadata_changed_cb), &controls);


    gtk_widget_show_all(controls.preference);
    gtk_widget_show(controls.check_units);
    gtk_widget_show(controls.check_labels);
    gtk_widget_show(controls.check_metadata);

    gtk_widget_show_all(dialog);

    return TRUE;
}


static void
export_dialog_closed_cb(G_GNUC_UNUSED GwyGraph *graph)
{

    if (dialog) {
        gtk_widget_destroy(dialog);
        dialog = NULL;
    }
}


static void
export_dialog_response_cb(GtkDialog *pdialog, gint response, GwyGraph *graph)
{
    GtkDialog *filedialog;
    GwyContainer *settings;
    gchar *filename;

    if (response == GTK_RESPONSE_OK)
    {
        filedialog = GTK_DIALOG(gtk_file_chooser_dialog_new ("Export to ASCII File",
                                                             GTK_WINDOW(pdialog),
                                                             GTK_FILE_CHOOSER_ACTION_SAVE,
                                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                            GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                            NULL));
        if (gtk_dialog_run (GTK_DIALOG (filedialog)) == GTK_RESPONSE_ACCEPT)
        {
            filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filedialog));
            gwy_graph_model_export_ascii(graph->graph_model, filename,
                                         controls.units, controls.labels, controls.metadata,
                                         controls.style);
        }
        gtk_widget_destroy(GTK_WIDGET(filedialog));
        settings = gwy_app_settings_get();
        save_args(settings, &controls);
    }

    export_dialog_closed_cb(graph);
}

static void
units_changed_cb(ExportControls *pcontrols)
{
    pcontrols->units = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pcontrols->check_units));
}

static void
labels_changed_cb(ExportControls *pcontrols)
{
    pcontrols->labels = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pcontrols->check_labels));
}

static void
metadata_changed_cb(ExportControls *pcontrols)
{
    pcontrols->metadata = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pcontrols->check_metadata));
}

static void
style_cb(GtkWidget *combo)
{
    controls.style = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}



static const gchar style_key[]    = "/module/graph_export_ascii/style";
static const gchar labels_key[]   = "/module/graph_export_ascii/labels";
static const gchar units_key[]    = "/module/graph_export_ascii/units";
static const gchar metadata_key[] = "/module/graph_export_ascii/metadata";

static void
load_args(GwyContainer *container,
          ExportControls *pcontrols)
{
    gwy_container_gis_boolean_by_name(container, labels_key, &pcontrols->labels);
    gwy_container_gis_boolean_by_name(container, units_key, &pcontrols->units);
    gwy_container_gis_boolean_by_name(container, metadata_key, &pcontrols->metadata);
    gwy_container_gis_enum_by_name(container, style_key, &pcontrols->style);

}

static void
save_args(GwyContainer *container,
          ExportControls *pcontrols)
{
    gwy_container_set_boolean_by_name(container, labels_key, pcontrols->labels);
    gwy_container_set_boolean_by_name(container, units_key, pcontrols->units);
    gwy_container_set_boolean_by_name(container, metadata_key, pcontrols->metadata);
    gwy_container_set_enum_by_name(container, style_key, pcontrols->style);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
