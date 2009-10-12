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

/**
 * [FILE-MAGIC-USERGUIDE]
 * Graph text data (raw)
 * any
 * Read[1]
 * [1] At present, only simple two-column data, imported as graph curves, are
 * supported.
 **/

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <app/settings.h>
#include <app/data-browser.h>

#include "err.h"

typedef struct {
    gchar *title;
    gchar *x_label;
    gchar *y_label;
    gchar *x_units;
    gchar *y_units;
    /* Interface only */
    guint ncols;
    GArray *data;
    gdouble *xdata;
    gdouble *ydata;
} RawGraphArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *title;
    GtkWidget *x_label;
    GtkWidget *y_label;
    GtkWidget *x_units;
    GtkWidget *y_units;
} RawGraphControls;

static gboolean       module_register   (void);
static gint           rawgraph_detect   (const GwyFileDetectInfo *fileinfo,
                                         gboolean only_name);
static GwyContainer*  rawgraph_load     (const gchar *filename,
                                         GwyRunType mode,
                                         GError **error);
static gboolean       rawgraph_dialog   (RawGraphArgs *args,
                                         GwyGraphModel *gmodel);
static GtkWidget*     attach_entry      (GtkTable *table,
                                         const gchar *description,
                                         gint *row);
static void           update_string     (GtkWidget *entry,
                                         gchar **text);
static void           update_property   (GtkEntry *entry,
                                         GObject *object);
static void           update_units      (GtkEntry *entry,
                                         GObject *object);
static GwyGraphModel* rawgraph_parse    (gchar *buffer,
                                         RawGraphArgs *args,
                                         GError **error);
static void           fill_data         (GwyGraphModel *gmodel,
                                         RawGraphArgs *args);
static int            compare_double    (gconstpointer a,
                                         gconstpointer b);
static void           rawgraph_load_args(GwyContainer *container,
                                         RawGraphArgs *args);
static void           rawgraph_save_args(GwyContainer *container,
                                         RawGraphArgs *args);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports simple text files as graph curves."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("rawgraph",
                           N_("ASCII graph curve files"),
                           (GwyFileDetectFunc)&rawgraph_detect,
                           (GwyFileLoadFunc)&rawgraph_load,
                           NULL,
                           NULL);
    /* We provide a detection function, but the loading method tries a bit
     * harder, so let the user choose explicitly. */
    gwy_file_func_set_is_detectable("rawgraph", FALSE);

    return TRUE;
}

static gint
rawgraph_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    const gchar *s;
    gchar *end;
    guint i;

    if (only_name)
        return 0;

    s = fileinfo->head;
    for (i = 0; i < 6; i++) {
        g_ascii_strtod(s, &end);
        if (end == s) {
            /* If we encounter garbage at the first line, give it a one more
             * chance. */
            if (i || !(s = strchr(s, '\n')))
                return 0;
            goto next_line;
        }
        s = end;
        g_ascii_strtod(s, &end);
        if (end == s)
            return 0;

        s = end;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s != '\n' && *s != '\r')
            return 0;

next_line:
        do {
            s++;
        } while (g_ascii_isspace(*s));
    }

    return 50;
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
    if (!(gmodel = rawgraph_parse(buffer, &args, error)))
        goto fail;

    rawgraph_load_args(gwy_app_settings_get(), &args);
    fill_data(gmodel, &args);
    if (!rawgraph_dialog(&args, gmodel)) {
        err_CANCELLED(error);
        goto fail;
    }
    rawgraph_save_args(gwy_app_settings_get(), &args);

    container = gwy_container_new();
    gwy_container_set_object(container, gwy_app_get_graph_key_for_id(1),
                             gmodel);

fail:
    g_free(buffer);
    gwy_object_unref(gmodel);
    g_free(args.title);
    g_free(args.x_label);
    g_free(args.y_label);
    g_free(args.x_units);
    g_free(args.y_units);
    g_free(args.xdata);
    g_free(args.ydata);
    g_array_free(args.data, TRUE);

    return container;
}

static gboolean
rawgraph_dialog(RawGraphArgs *args,
                GwyGraphModel *gmodel)
{
    RawGraphControls controls;
    GwyGraphCurveModel *gcmodel;
    GtkWidget *dialog, *hbox, *align, *graph;
    GtkTable *table;
    gint row, response;

    gcmodel = gwy_graph_model_get_curve(gmodel, 0);

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

    controls.title = attach_entry(table, _("_Title:"), &row);
    g_object_set_data(G_OBJECT(controls.title), "id", "title");
    g_signal_connect(controls.title, "changed",
                     G_CALLBACK(update_property), gmodel);
    g_signal_connect(controls.title, "changed",
                     G_CALLBACK(update_property), gcmodel);

    controls.x_label = attach_entry(table, _("_X label:"), &row);
    g_object_set_data(G_OBJECT(controls.x_label), "id", "axis-label-bottom");
    g_signal_connect(controls.x_label, "changed",
                     G_CALLBACK(update_property), gmodel);

    controls.y_label = attach_entry(table, _("_Y label:"), &row);
    g_object_set_data(G_OBJECT(controls.y_label), "id", "axis-label-left");
    g_signal_connect(controls.y_label, "changed",
                     G_CALLBACK(update_property), gmodel);

    controls.x_units = attach_entry(table, _("X _units:"), &row);
    g_object_set_data(G_OBJECT(controls.x_units), "id", "si-unit-x");
    g_object_set_data(G_OBJECT(controls.x_units), "string", &args->x_units);
    g_object_set_data(G_OBJECT(controls.x_units), "args", args);
    g_signal_connect(controls.x_units, "changed",
                     G_CALLBACK(update_units), gmodel);

    controls.y_units = attach_entry(table, _("Y un_its:"), &row);
    g_object_set_data(G_OBJECT(controls.y_units), "id", "si-unit-y");
    g_object_set_data(G_OBJECT(controls.y_units), "string", &args->y_units);
    g_object_set_data(G_OBJECT(controls.y_units), "args", args);
    g_signal_connect(controls.y_units, "changed",
                     G_CALLBACK(update_units), gmodel);

    /********************************************************************
     * Right column
     ********************************************************************/

    graph = gwy_graph_new(gmodel);
    gtk_widget_set_size_request(graph, 320, 240);
    gtk_box_pack_start(GTK_BOX(hbox), graph, TRUE, TRUE, 0);

    gtk_entry_set_text(GTK_ENTRY(controls.title), args->title);
    gtk_entry_set_text(GTK_ENTRY(controls.x_label), args->x_label);
    gtk_entry_set_text(GTK_ENTRY(controls.y_label), args->y_label);
    gtk_entry_set_text(GTK_ENTRY(controls.y_units), args->y_units);
    gtk_entry_set_text(GTK_ENTRY(controls.y_units), args->y_units);

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
             gint *row)
{
    GtkWidget *label, *entry;

    label = gtk_label_new_with_mnemonic(description);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, *row, *row + 1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    entry = gtk_entry_new();
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

static void
update_property(GtkEntry *entry,
                GObject *object)
{
    const gchar *name = g_object_get_data(G_OBJECT(entry), "id");

    if (gwy_strequal(name, "title") && GWY_IS_GRAPH_CURVE_MODEL(object))
        name = "description";
    g_object_set(object, name, gtk_entry_get_text(entry), NULL);
}

static void
update_units(GtkEntry *entry,
             GObject *object)
{
    RawGraphArgs *args;
    GwySIUnit *unit;
    const gchar *unitstr;
    gchar **s;

    unitstr = gtk_entry_get_text(entry);
    unit = gwy_si_unit_new(unitstr);
    g_object_set(object, g_object_get_data(G_OBJECT(entry), "id"), unit, NULL);
    s = g_object_get_data(G_OBJECT(entry), "string");
    g_free(*s);
    *s = g_strdup(unitstr);
    g_object_unref(unit);

    args = g_object_get_data(G_OBJECT(entry), "args");
    fill_data(GWY_GRAPH_MODEL(object), args);
}

static GwyGraphModel*
rawgraph_parse(gchar *buffer,
               RawGraphArgs *args,
               GError **error)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GArray *data = NULL;
    guint i, ncols = 0;
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
        err_NO_DATA(error);
        return NULL;
    }

    g_array_sort(data, compare_double);
    args->data = data;
    args->xdata = g_new(gdouble, data->len);
    args->ydata = g_new(gdouble, data->len);
    args->ncols = ncols;

    gmodel = gwy_graph_model_new();
    for (i = 1; i < ncols; i++) {
        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }

    return gmodel;
}

static void
fill_data(GwyGraphModel *gmodel,
          RawGraphArgs *args)
{
    GwyGraphCurveModel *gcmodel;
    GwySIUnit *unit;
    gint power10;
    guint i, j;
    gdouble q;

    unit = gwy_si_unit_new_parse(args->x_units, &power10);
    q = pow10(power10);

    for (j = 0; j < args->data->len; j++)
        args->xdata[j] = q*g_array_index(args->data, gdouble, j*args->ncols);

    gwy_si_unit_set_from_string_parse(unit, args->y_units, &power10);
    q = pow10(power10);

    for (i = 1; i < args->ncols; i++) {
        for (j = 0; j < args->data->len; j++)
            args->ydata[j] = q*g_array_index(args->data, gdouble,
                                             j*args->ncols + i);

        gcmodel = gwy_graph_model_get_curve(gmodel, i-1);
        gwy_graph_curve_model_set_data(gcmodel,
                                       args->xdata, args->ydata,
                                       args->data->len);
    }

    g_object_unref(unit);
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

static const gchar title_key[]   = "/module/rawgraph/title";
static const gchar x_label_key[] = "/module/rawgraph/x-label";
static const gchar y_label_key[] = "/module/rawgraph/y-label";
static const gchar x_units_key[] = "/module/rawgraph/x-units";
static const gchar y_units_key[] = "/module/rawgraph/y-units";

static void
rawgraph_load_args(GwyContainer *container,
                   RawGraphArgs *args)
{
    args->title = NULL;
    args->x_label = NULL;
    args->y_label = NULL;
    args->x_units = NULL;
    args->y_units = NULL;

    gwy_container_gis_string_by_name(container, title_key,
                                     (const guchar**)&args->title);
    gwy_container_gis_string_by_name(container, x_label_key,
                                     (const guchar**)&args->x_label);
    gwy_container_gis_string_by_name(container, y_label_key,
                                     (const guchar**)&args->y_label);
    gwy_container_gis_string_by_name(container, x_units_key,
                                     (const guchar**)&args->x_units);
    gwy_container_gis_string_by_name(container, y_units_key,
                                     (const guchar**)&args->y_units);

    args->title = g_strdup(args->title ? args->title : _("Curve"));
    args->x_label = g_strdup(args->x_label ? args->x_label : "x");
    args->y_label = g_strdup(args->y_label ? args->y_label : "y");
    args->x_units = g_strdup(args->x_units ? args->x_units : "");
    args->y_units = g_strdup(args->y_units ? args->y_units : "");
}

static void
rawgraph_save_args(GwyContainer *container,
                   RawGraphArgs *args)
{
    gwy_container_set_string_by_name(container, title_key,
                                     (guchar*)args->title);
    gwy_container_set_string_by_name(container, x_label_key,
                                     (guchar*)args->x_label);
    gwy_container_set_string_by_name(container, y_label_key,
                                     (guchar*)args->y_label);
    gwy_container_set_string_by_name(container, x_units_key,
                                     (guchar*)args->x_units);
    gwy_container_set_string_by_name(container, y_units_key,
                                     (guchar*)args->y_units);

    args->title = NULL;
    args->x_label = NULL;
    args->y_label = NULL;
    args->x_units = NULL;
    args->y_units = NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
