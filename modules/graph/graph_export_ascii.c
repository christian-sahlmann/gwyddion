/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#include <stdio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-graph.h>
#include <app/gwyapp.h>

typedef struct {
    gboolean units;
    gboolean labels;
    gboolean metadata;
    gboolean posix;
    GwyGraphModelExportStyle style;
} ExportParameters;

static gboolean module_register          (void);
static void     export                   (GwyGraph *graph);
static void     export_dialog            (GwyGraph *graph,
                                          ExportParameters *params);
static void     export_dialog_choose_file(GwyGraph *graph,
                                          ExportParameters *params);
static void     boolean_changed_cb       (GtkToggleButton *check,
                                          gboolean *value);
static void     load_args                (GwyContainer *settings,
                                          ExportParameters *params);
static void     save_args                (GwyContainer *settings,
                                          ExportParameters *params);

static const GwyEnum style_type[] = {
   { N_("Plain text"),             GWY_GRAPH_MODEL_EXPORT_ASCII_PLAIN,   },
   { N_("Gnuplot friendly"),       GWY_GRAPH_MODEL_EXPORT_ASCII_GNUPLOT, },
   { N_("Comma separated values"), GWY_GRAPH_MODEL_EXPORT_ASCII_CSV,     },
   { N_("Origin friendly"),        GWY_GRAPH_MODEL_EXPORT_ASCII_ORIGIN,  },
};

static const ExportParameters load_defaults = {
    TRUE,
    TRUE,
    TRUE,
    FALSE,
    GWY_GRAPH_MODEL_EXPORT_ASCII_PLAIN,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Exports graph data to text files."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_graph_func_register("graph_export_ascii",
                            (GwyGraphFunc)&export,
                            N_("/Export _Text..."),
                            NULL,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Export graph data to a text file"));

    return TRUE;
}

static void
export(GwyGraph *graph)
{
    ExportParameters params;
    GwyContainer *settings;

    settings = gwy_app_settings_get();
    load_args(settings, &params);
    export_dialog(graph, &params);
    save_args(settings, &params);
}

static void
export_dialog(GwyGraph *graph,
              ExportParameters *params)
{
    GtkWidget *dialog, *check, *combo;
    GtkBox *vbox;

    dialog = gtk_dialog_new_with_buttons(_("Export Text"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    vbox = GTK_BOX(GTK_DIALOG(dialog)->vbox);

    combo = gwy_enum_combo_box_new(style_type, G_N_ELEMENTS(style_type),
                                   G_CALLBACK(gwy_enum_combo_box_update_int),
                                   &params->style, params->style, TRUE);
    gtk_box_pack_start(vbox, combo, FALSE, FALSE, 0);

    check = gtk_check_button_new_with_mnemonic(_("POSIX _number format"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), params->posix);
    gtk_box_pack_start(vbox, check, FALSE, FALSE, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(boolean_changed_cb), &params->posix);

    check = gtk_check_button_new_with_mnemonic(_("Export _labels"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), params->labels);
    gtk_box_pack_start(vbox, check, FALSE, FALSE, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(boolean_changed_cb), &params->labels);

    check = gtk_check_button_new_with_mnemonic(_("Export _units"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), params->units);
    gtk_box_pack_start(vbox, check, FALSE, FALSE, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(boolean_changed_cb), &params->units);

    check = gtk_check_button_new_with_mnemonic(_("Export _metadata"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), params->metadata);
    gtk_box_pack_start(vbox, check, FALSE, FALSE, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(boolean_changed_cb), &params->metadata);

    gtk_widget_show_all(dialog);
    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
        gtk_widget_destroy(dialog);
        case GTK_RESPONSE_NONE:
        break;

        case GTK_RESPONSE_OK:
        gtk_widget_destroy(dialog);
        export_dialog_choose_file(graph, params);
        break;

        default:
        g_warning("Unhandled dialog response");
        break;
    }
}
static void
export_dialog_choose_file(GwyGraph *graph,
                          ExportParameters *params)
{
    GtkWidget *dialog;
    gchar *filename;
    GString *str;
    FILE *fw;

    dialog = gtk_file_chooser_dialog_new("Export to Text File", NULL,
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        gwy_app_get_current_directory());

    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
        gtk_widget_destroy(dialog);
        case GTK_RESPONSE_NONE:
        return;
        break;

        case GTK_RESPONSE_OK:
        /* pass */
        break;

        default:
        g_warning("Unhandled dialog response");
        return;
        break;
    }

    if (gwy_app_file_confirm_overwrite(dialog)) {
        GwyGraphModelExportStyle style;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        style = params->style | (params->posix
                                 ? GWY_GRAPH_MODEL_EXPORT_ASCII_POSIX
                                 : 0);
        str = gwy_graph_model_export_ascii(graph->graph_model,
                                           params->units,
                                           params->labels,
                                           params->metadata,
                                           style,
                                           NULL);

        /* FIXME: Must check success */
        fw = g_fopen(filename, "w");
        fwrite(str->str, str->len, 1, fw);
        fclose(fw);
        g_string_free(str, TRUE);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void
boolean_changed_cb(GtkToggleButton *check, gboolean *value)
{
    *value = gtk_toggle_button_get_active(check);
}

static const gchar style_key[]    = "/module/graph_export_ascii/style";
static const gchar labels_key[]   = "/module/graph_export_ascii/labels";
static const gchar units_key[]    = "/module/graph_export_ascii/units";
static const gchar metadata_key[] = "/module/graph_export_ascii/metadata";

static void
load_args(GwyContainer *settings,
          ExportParameters *params)
{
    *params = load_defaults;
    gwy_container_gis_boolean_by_name(settings, labels_key, &params->labels);
    gwy_container_gis_boolean_by_name(settings, units_key, &params->units);
    gwy_container_gis_boolean_by_name(settings, metadata_key, &params->metadata);
    gwy_container_gis_enum_by_name(settings, style_key, &params->style);

    params->posix = params->style & GWY_GRAPH_MODEL_EXPORT_ASCII_POSIX;
    params->style = params->style & ~GWY_GRAPH_MODEL_EXPORT_ASCII_POSIX;
    params->style = MIN(params->style, GWY_GRAPH_MODEL_EXPORT_ASCII_ORIGIN);
}

static void
save_args(GwyContainer *settings,
          ExportParameters *params)
{
    GwyGraphModelExportStyle style;

    style = params->style | (params->posix
                             ? GWY_GRAPH_MODEL_EXPORT_ASCII_POSIX
                             : 0);
    gwy_container_set_boolean_by_name(settings, labels_key, params->labels);
    gwy_container_set_boolean_by_name(settings, units_key, params->units);
    gwy_container_set_boolean_by_name(settings, metadata_key, params->metadata);
    gwy_container_set_enum_by_name(settings, style_key, style);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
