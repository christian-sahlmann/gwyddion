/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <app/data-browser.h>

#include "err.h"

typedef struct {
    gchar *title;
    gchar *x_label;
    gchar *y_label;
    gchar *x_units;
    gchar *y_units;
} RawGraphArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *title;
    GtkWidget *x_label;
    GtkWidget *y_label;
    GtkWidget *x_units;
    GtkWidget *y_units;
} RawGraphControls;

static gboolean       module_register(void);
static GwyContainer*  rawgraph_load  (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static gboolean       rawgraph_dialog(RawGraphArgs *args,
                                      GwyGraphModel *gmodel);
static GtkWidget*     attach_entry   (GtkTable *table,
                                      const gchar *description,
                                      const gchar *contents,
                                      gint *row);
static void           update_string  (GtkWidget *entry,
                                      gchar **text);
static GwyGraphModel* rawgraph_parse (gchar *buffer,
                                      GError **error);
static int            compare_double (gconstpointer a,
                                      gconstpointer b);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports simple text files as graph curves."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("rawgraph",
                           N_("ASCII graph curve files"),
                           NULL,
                           (GwyFileLoadFunc)&rawgraph_load,
                           NULL,
                           NULL);

    return TRUE;
}

static GwyContainer*
rawgraph_load(const gchar *filename,
              GwyRunType mode,
              GError **error)
{
    GError *err = NULL;
    GwyContainer *container = NULL;
    GwyGraphModel *gmodel = NULL;
    RawGraphArgs args;
    gchar *buffer;

    if (mode != GWY_RUN_INTERACTIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_INTERACTIVE,
                    _("ASCII graph import must be run as interactive."));
        return NULL;
    }

    if (!g_file_get_contents(filename, &buffer, NULL, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    gwy_clear(&args, 1);
    if (!(gmodel = rawgraph_parse(buffer, error)))
        goto fail;

    args.title = g_strdup("Title");
    args.x_label = g_strdup("X label");
    args.y_label = g_strdup("Y label");
    args.x_units = g_strdup("X units");
    args.y_units = g_strdup("Y units");
    if (!rawgraph_dialog(&args, gmodel)) {
        err_CANCELLED(error);
        goto fail;
    }

    err_NO_DATA(error);

fail:
    g_free(buffer);
    gwy_object_unref(gmodel);
    g_free(args.title);
    g_free(args.x_label);
    g_free(args.y_label);
    g_free(args.x_units);
    g_free(args.y_units);

    return container;
}

static gboolean
rawgraph_dialog(RawGraphArgs *args,
                GwyGraphModel *gmodel)
{
    RawGraphControls controls;
    GtkWidget *dialog, *hbox, *align, *graph;
    GtkTable *table;
    gint row, response;

    dialog = gtk_dialog_new_with_buttons(_("Import Graph Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    /********************************************************************
     * Left column
     ********************************************************************/

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);

    table = GTK_TABLE(gtk_table_new(5, 2, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(table));
    row = 0;

    controls.title = attach_entry(table, _("Graph _title:"), args->title, &row);
    controls.x_label = attach_entry(table, _("_X label:"), args->x_label, &row);
    controls.y_label = attach_entry(table, _("_Y label:"), args->y_label, &row);
    controls.x_units = attach_entry(table, _("X _units:"), args->x_units, &row);
    controls.y_units = attach_entry(table, _("Y un_its:"), args->y_units, &row);

    /********************************************************************
     * Right column
     ********************************************************************/

    graph = gwy_graph_new(gmodel);
    gtk_widget_set_size_request(graph, 320, 240);
    gtk_box_pack_start(GTK_BOX(hbox), graph, TRUE, TRUE, 0);

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
            update_string(controls.title, &args->title);
            update_string(controls.x_label, &args->x_label);
            update_string(controls.y_label, &args->y_label);
            update_string(controls.x_units, &args->x_units);
            update_string(controls.y_units, &args->y_units);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static GtkWidget*
attach_entry(GtkTable *table,
             const gchar *description,
             const gchar *contents,
             gint *row)
{
    GtkWidget *label, *entry;

    label = gtk_label_new_with_mnemonic(description);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, *row, *row + 1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), contents);
    gtk_table_attach(table, entry, 1, 2, *row, *row + 1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

    (*row)++;

    return entry;
}

static void
update_string(GtkWidget *entry,
              gchar **text)
{
    const gchar *s;

    s = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!gwy_strequal(s, *text)) {
        g_free(*text);
        *text = g_strdup(s);
    }
}

static GwyGraphModel*
rawgraph_parse(gchar *buffer,
               GError **error)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GArray *data = NULL;
    guint i, j, ncols = 0;
    gdouble *xdata, *ydata;
    gchar *line, *end;

    for (line = gwy_str_next_line(&buffer);
         line;
         line = gwy_str_next_line(&buffer)) {
        gdouble *dd;
        g_strstrip(line);

        if (!line[0] || line[0] == '#')
            continue;

        if (!ncols) {
            gchar *orig_line = line;

            while (g_ascii_strtod(line, &end) || end > line) {
                line = end;
                ncols++;
            }

            /* Skip arbitrary rubbish at the begining */
            if (!ncols) {
                continue;
            }

            /* FIXME: We could support more columns, but it quickly gets
             * complicated. */
            if (ncols != 2) {
                 g_set_error(error, GWY_MODULE_FILE_ERROR,
                             GWY_MODULE_FILE_ERROR_DATA,
                             _("Only files with two columns can be imported."));
                return NULL;
            }

            data = g_array_new(FALSE, FALSE, sizeof(gdouble)*ncols);
            line = orig_line;
        }

        g_array_set_size(data, data->len + 1);
        dd = &g_array_index(data, gdouble, ncols*(data->len - 1));
        /* FIXME: Check whether we actually read data and abort on rubbish. */
        for (i = 0; i < ncols; i++) {
            dd[i] = g_ascii_strtod(line, &end);
            line = end;
        }
    }

    if (!data) {
        err_NO_DATA(error);
        return NULL;
    }

    if (!data->len) {
        g_array_free(data, TRUE);
        err_NO_DATA(error);
        return NULL;
    }

    g_array_sort(data, compare_double);
    xdata = g_new(gdouble, data->len);
    ydata = g_new(gdouble, data->len);

    for (j = 0; j < data->len; j++)
        xdata[j] = g_array_index(data, gdouble, j*ncols);

    gmodel = gwy_graph_model_new();
    for (i = 1; i < ncols; i++) {
        for (j = 0; j < data->len; j++)
            ydata[j] = g_array_index(data, gdouble, j*ncols + i);

        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, data->len);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }

    g_free(xdata);
    g_free(ydata);
    g_array_free(data, TRUE);

    return gmodel;
}

static int
compare_double(gconstpointer a, gconstpointer b)
{
    const double *da = (const double*)a;
    const double *db = (const double*)b;

    if (da < db)
        return -1;
    if (da > db)
        return 1;
    return 0.0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
