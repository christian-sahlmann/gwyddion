/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <stdarg.h>
#include <string.h>
#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

enum {
    LOG_TYPE,
    LOG_FUNCNAME,
    LOG_PARAMETERS,
    LOG_TIME,
};

typedef enum {
    BROWSER_DATA_CHANNEL,
    BROWSER_DATA_VOLUME,
} BrowserDataType;


typedef GQuark (*LogKeyFync)(gint id);

typedef struct {
    GwyContainer *container;
    GwyStringList *log;
    GString *buf;
    gulong changed_id;
    gulong destroy_id;
    GtkWidget *window;
    GtkWidget *treeview;
    GtkWidget *save;
    GtkWidget *clear;
    GtkWidget *close;
} LogBrowser;

void                  data_log_add_valist   (GwyContainer *data,
                                             LogKeyFync log_key,
                                             gint previd,
                                             gint newid,
                                             const gchar *function,
                                             va_list ap);
static GwyStringList* get_data_log          (GwyContainer *data,
                                             GQuark quark,
                                             gboolean create);
static GtkWidget*     get_log_browser       (GwyContainer *data,
                                             BrowserDataType type,
                                             gint id);
static LogBrowser*    log_browser_new       (GwyContainer *data,
                                             BrowserDataType type,
                                             gint id,
                                             const gchar *key);
static void           log_browser_construct (LogBrowser *browser);
static void           log_cell_renderer     (GtkTreeViewColumn *column,
                                             GtkCellRenderer *renderer,
                                             GtkTreeModel *model,
                                             GtkTreeIter *iter,
                                             gpointer userdata);
static void           gwy_log_export        (LogBrowser *browser);
static void           gwy_log_clear         (LogBrowser *browser);
static void           log_changed           (GwyStringList *slog,
                                             LogBrowser *browser);
static void           gwy_log_destroy       (LogBrowser *browser);
static void           gwy_log_data_finalized(LogBrowser *browser);
static gchar*         format_args           (const gchar *prefix);
static void           format_arg            (gpointer hkey,
                                             gpointer hvalue,
                                             gpointer user_data);
static GQuark         channel_log_key       (gint id);
static GQuark         volume_log_key        (gint id);
static gboolean       find_settings_prefix  (const gchar *function,
                                             const gchar *settings_name,
                                             GString *prefix);

static gboolean log_disabled = FALSE;

/**
 * gwy_app_channel_log_add:
 * @data: A data container.
 * @previd: Identifier of the previous (source) data channel in the container.
 *          Pass -1 for a no-source (or unclear source) operation.
 * @newid: Identifier of the new (target) data channel in the container.
 * @function: Quailified name of the function applied as shown by the module
 *            browser.  For instance "proc::facet-level" or
 *            "tool::GwyToolCrop".
 * @...: Logging options as a %NULL-terminated list of pairs name, value.
 *
 * Adds an entry to the log of data processing operations for a channel.
 *
 * See the introduction for a description of valid @previd and @newid.
 *
 * It is possible to pass %NULL as @function.  In this case the log is just
 * copied from source to target without adding any entries.  This can be useful
 * to prevent duplicate log entries in modules that modify a data field and
 * then can also create secondary outputs.  Note you still need to pass a
 * second %NULL argument as the option terminator.
 *
 * Since: 2.35
 **/
void
gwy_app_channel_log_add(GwyContainer *data,
                        gint previd,
                        gint newid,
                        const gchar *function,
                        ...)
{
    va_list ap;
    va_start(ap, function);
    data_log_add_valist(data, channel_log_key, previd, newid, function, ap);
    va_end(ap);
}

/**
 * gwy_app_volume_log_add:
 * @data: A data container.
 * @previd: Identifier of the previous (source) volume data in the container.
 *          Pass -1 for a no-source (or unclear source) operation.
 * @newid: Identifier of the new (target) volume data in the container.
 * @function: Quailified name of the function applied as shown by the module
 *            browser.  For instance "proc::facet-level" or
 *            "tool::GwyToolCrop".
 * @...: Logging options as a %NULL-terminated list of pairs name, value.
 *
 * Adds an entry to the log of data processing operations for volume data.
 *
 * See the introduction for a description of valid @previd and @newid.
 *
 * It is possible to pass %NULL as @function.  In this case the log is just
 * copied from source to target without adding any entries.  This can be useful
 * to prevent duplicate log entries in modules that modify a data field and
 * then can also create secondary outputs.  Note you still need to pass a
 * second %NULL argument as the option terminator.
 *
 * Since: 2.35
 **/
void
gwy_app_volume_log_add(GwyContainer *data,
                       gint previd,
                       gint newid,
                       const gchar *function,
                       ...)
{
    va_list ap;
    va_start(ap, function);
    data_log_add_valist(data, volume_log_key, previd, newid, function, ap);
    va_end(ap);
}

void
data_log_add_valist(GwyContainer *data,
                    LogKeyFync log_key,
                    gint previd,
                    gint newid,
                    const gchar *function,
                    va_list ap)
{
    GString *str = NULL;
    const gchar *key, *settings_name = NULL;
    gchar *args, *optime, *s;
    GwyStringList *sourcelog = NULL, *targetlog = NULL;
    GQuark newquark;
    GTimeVal t;

    g_return_if_fail(GWY_IS_CONTAINER(data));
    g_return_if_fail(newid >= 0);

    if (log_disabled)
        return;

    while ((key = va_arg(ap, const gchar*))) {
        if (gwy_strequal(key, "settings-name")) {
            settings_name = va_arg(ap, const gchar*);
        }
        else {
            g_warning("Invalid logging option %s.", key);
            return;
        }
    }

    if (function) {
        str = g_string_new(NULL);
        if (!find_settings_prefix(function, settings_name, str)) {
            g_string_free(str, TRUE);
            return;
        }
    }

    if (previd != -1)
        sourcelog = get_data_log(data, log_key(previd), FALSE);

    newquark = log_key(newid);
    if (newid == previd)
        targetlog = sourcelog;
    else
        targetlog = get_data_log(data, newquark, FALSE);

    if (targetlog && targetlog != sourcelog) {
        g_warning("Target log must not exist when replicating logs.");
        /* Fix the operation to simple log-append. */
        sourcelog = targetlog;
        previd = newid;
    }

    if (!targetlog) {
        if (sourcelog)
            targetlog = gwy_string_list_duplicate(sourcelog);
        else {
            if (!function)
                return;
            targetlog = gwy_string_list_new();
        }

        gwy_container_set_object(data, newquark, targetlog);
        g_object_unref(targetlog);
    }

    if (!function)
        return;

    g_get_current_time(&t);
    optime = g_time_val_to_iso8601(&t);
    s = strchr(optime, 'T');
    if (s)
        *s = ' ';

    args = format_args(str->str);
    if (!str)
        str = g_string_new(NULL);
    g_string_printf(str, "%s(%s)@%s", function, args, optime);
    gwy_string_list_append_take(targetlog, g_string_free(str, FALSE));
    g_free(optime);
}

static GwyStringList*
get_data_log(GwyContainer *data,
             GQuark quark,
             gboolean create)
{
    GwyStringList *slog = NULL;

    gwy_container_gis_object(data, quark, &slog);

    if (slog || !create)
        return slog;

    slog = gwy_string_list_new();
    gwy_container_set_object(data, quark, slog);
    g_object_unref(slog);

    return slog;
}

/**
 * gwy_app_log_browser_for_channel:
 * @data: A data container.
 * @id: Id of a channel in @data to show log for.
 *
 * Shows a simple log browser for a channel.
 *
 * If the log browser is already shown for this channel it is just raised
 * and given focus.  Otherwise, a new window is created.
 *
 * Returns: The log browser (owned by the library).  Usually, you can
 *          ignore the return value.
 *
 * Since: 2.35
 **/
GtkWidget*
gwy_app_log_browser_for_channel(GwyContainer *data,
                                gint id)
{
    return get_log_browser(data, BROWSER_DATA_CHANNEL, id);
}

/**
 * gwy_app_log_browser_for_volume:
 * @data: A data container.
 * @id: Id of volume data in @data to show log for.
 *
 * Shows a simple log browser for volume data.
 *
 * If the log browser is already shown for this volume data it is just raised
 * and given focus.  Otherwise, a new window is created.
 *
 * Returns: The log browser (owned by the library).  Usually, you can
 *          ignore the return value.
 *
 * Since: 2.35
 **/
GtkWidget*
gwy_app_log_browser_for_volume(GwyContainer *data,
                               gint id)
{
    return get_log_browser(data, BROWSER_DATA_VOLUME, id);
}

static GtkWidget*
get_log_browser(GwyContainer *data,
                BrowserDataType type,
                gint id)
{
    gchar key[32];
    LogBrowser *browser;
    GwyStringList *slog;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);
    g_return_val_if_fail(id >= 0, NULL);

    if (type == BROWSER_DATA_CHANNEL)
        g_snprintf(key, sizeof(key), "/%d/data/log", id);
    else if (type == BROWSER_DATA_VOLUME)
        g_snprintf(key, sizeof(key), "/brick/%d/log", id);
    else {
        g_return_val_if_reached(NULL);
    }

    if (gwy_container_gis_object_by_name(data, key, &slog)) {
        browser = g_object_get_data(G_OBJECT(slog), "log-browser");
        if (browser) {
            gtk_window_present(GTK_WINDOW(browser->window));
            return browser->window;
        }
    }

    browser = log_browser_new(data, type, id, key);
    gtk_widget_show_all(browser->window);
    return browser->window;
}

static LogBrowser*
log_browser_new(GwyContainer *data,
                BrowserDataType type,
                gint id,
                const gchar *key)
{
    LogBrowser *browser;
    GtkWidget *scroll, *vbox, *hbox;
    GtkTreeView *treeview;
    GwyNullStore *store;
    GtkRequisition request;
    GwyStringList *slog = NULL;
    gchar *title, *dataname;
    guint n;

    browser = g_new0(LogBrowser, 1);
    browser->buf = g_string_new(NULL);
    log_browser_construct(browser);
    browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    if (type == BROWSER_DATA_CHANNEL)
        dataname = gwy_app_get_data_field_title(data, id);
    else if (type == BROWSER_DATA_VOLUME)
        dataname = gwy_app_get_brick_title(data, id);
    else {
        g_return_val_if_reached(browser);
    }

    title = g_strdup_printf(_("Log of %s (%s)"),
                            dataname, g_get_application_name());
    gtk_window_set_title(GTK_WINDOW(browser->window), title);
    g_free(title);
    g_free(dataname);

    if (!(gwy_container_gis_object_by_name(data, key, &slog))) {
        slog = gwy_string_list_new();
        gwy_container_set_object_by_name(data, key, slog);
        g_object_unref(slog);
    }

    browser->log = slog;
    g_object_set_data(G_OBJECT(slog), "log-browser", browser);
    g_object_weak_ref(G_OBJECT(slog),
                      (GWeakNotify)&gwy_log_data_finalized,
                      browser);

    browser->changed_id = g_signal_connect(slog, "value-changed",
                                           G_CALLBACK(log_changed), browser);

    n = gwy_string_list_get_length(slog);
    store = gwy_null_store_new(n);
    treeview = GTK_TREE_VIEW(browser->treeview);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store));
    g_object_unref(store);

    gtk_widget_size_request(browser->treeview, &request);
    request.width = MAX(request.width, 320);
    request.height = MAX(request.height, 400);
    gtk_window_set_default_size(GTK_WINDOW(browser->window),
                                MIN(request.width + 24, 2*gdk_screen_width()/3),
                                MIN(request.height + 32,
                                    2*gdk_screen_height()/3));
    gwy_app_add_main_accel_group(GTK_WINDOW(browser->window));

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(browser->window), vbox);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(scroll), browser->treeview);
    browser->destroy_id
        = g_signal_connect_swapped(browser->window, "destroy",
                                   G_CALLBACK(gwy_log_destroy), browser);

    hbox = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    browser->save = gwy_stock_like_button_new(_("_Export"), GTK_STOCK_SAVE);
    gtk_box_pack_start(GTK_BOX(hbox), browser->save, TRUE, TRUE, 0);
    g_signal_connect_swapped(browser->save, "clicked",
                             G_CALLBACK(gwy_log_export), browser);
    gtk_widget_set_sensitive(browser->save, n != 0);

    browser->clear = gwy_stock_like_button_new(_("Clea_r"), GTK_STOCK_CLEAR);
    gtk_box_pack_start(GTK_BOX(hbox), browser->clear, TRUE, TRUE, 0);
    g_signal_connect_swapped(browser->clear, "clicked",
                             G_CALLBACK(gwy_log_clear), browser);

    browser->close = gwy_stock_like_button_new(_("_Close"), GTK_STOCK_CLOSE);
    gtk_box_pack_start(GTK_BOX(hbox), browser->close, TRUE, TRUE, 0);
    g_signal_connect_swapped(browser->close, "clicked",
                             G_CALLBACK(gtk_widget_destroy), browser->window);

    return browser;
}

static void
log_browser_construct(LogBrowser *browser)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { N_("Type"),       LOG_TYPE,       },
        { N_("Function"),   LOG_FUNCNAME,   },
        { N_("Parameters"), LOG_PARAMETERS, },
        { N_("Time"),       LOG_TIME,       },
    };

    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    gsize i;

    browser->treeview = gtk_tree_view_new();
    treeview = GTK_TREE_VIEW(browser->treeview);
    gtk_tree_view_set_rules_hint(treeview, TRUE);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_(columns[i].title),
                                                          renderer, NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                log_cell_renderer, browser,
                                                NULL);
        gtk_tree_view_append_column(treeview, column);
        g_object_set_data(G_OBJECT(renderer), "column",
                          GUINT_TO_POINTER(columns[i].id));

        if (columns[i].id == LOG_PARAMETERS) {
            gtk_tree_view_column_set_expand(column, TRUE);
            g_object_set(renderer,
                         "ellipsize", PANGO_ELLIPSIZE_END,
                         "ellipsize-set", TRUE,
                         NULL);
        }
    }

    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);
}

static void
log_cell_renderer(G_GNUC_UNUSED GtkTreeViewColumn *column,
                  GtkCellRenderer *renderer,
                  GtkTreeModel *model,
                  GtkTreeIter *iter,
                  gpointer userdata)
{
    LogBrowser *browser = (LogBrowser*)userdata;
    GString *buf = browser->buf;
    const gchar *s, *t;
    guint i;
    gulong id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(renderer), "column"));
    gtk_tree_model_get(model, iter, 0, &i, -1);
    s = gwy_string_list_get(browser->log, i);
    g_return_if_fail(s);

    g_string_truncate(buf, 0);
    switch (id) {
        case LOG_TYPE:
        t = strstr(s, "::");
        g_return_if_fail(t);
        g_string_append_len(buf, s, t-s);
        break;

        case LOG_FUNCNAME:
        t = strstr(s, "::");
        g_return_if_fail(t);
        s = t+2;
        t = strchr(s, '(');
        g_return_if_fail(t);
        g_string_append_len(buf, s, t-s);
        break;

        case LOG_PARAMETERS:
        t = strchr(s, '(');
        g_return_if_fail(t);
        s = t+1;
        t = strrchr(s, ')');
        g_return_if_fail(t);
        g_string_append_len(buf, s, t-s);
        break;

        case LOG_TIME:
        t = strrchr(s, '@');
        g_return_if_fail(t);
        g_string_append(buf, t+1);
        break;

        default:
        g_return_if_reached();
        break;
    }

    g_object_set(renderer, "text", buf->str, NULL);
}

static void
gwy_log_export(LogBrowser *browser)
{
    gchar *str_to_save;
    const gchar **entries;
    guint i, n;

    n = gwy_string_list_get_length(browser->log);
    entries = g_new(const gchar*, n+2);
    for (i = 0; i < n; i++)
        entries[i] = gwy_string_list_get(browser->log, i);
    entries[n] = "";
    entries[n+1] = NULL;

    str_to_save = g_strjoinv("\n", (gchar**)entries);
    g_free(entries);

    gwy_save_auxiliary_data(_("Export Log"),
                            GTK_WINDOW(browser->window),
                            -1,
                            str_to_save);

    g_free(str_to_save);
}

static void
gwy_log_clear(LogBrowser *browser)
{
    gwy_string_list_clear(browser->log);
}

static void
log_changed(GwyStringList *slog, LogBrowser *browser)
{
    GtkTreeView *treeview = GTK_TREE_VIEW(browser->treeview);
    GwyNullStore *store = GWY_NULL_STORE(gtk_tree_view_get_model(treeview));
    guint n = gwy_string_list_get_length(slog);

    // The log can be only:
    // - extended with new entries (data processing, redo)
    // - truncated (undo)
    // - cleared (clear)
    // In all cases simple gwy_null_store_set_n_rows() does the right thing.
    gwy_null_store_set_n_rows(store, n);
    gtk_widget_set_sensitive(browser->save, n != 0);
}

static void
gwy_log_destroy(LogBrowser *browser)
{
    gwy_signal_handler_disconnect(browser->log, browser->changed_id);
    g_object_set_data(G_OBJECT(browser->log), "log-browser", NULL);
    g_object_weak_unref(G_OBJECT(browser->log),
                        (GWeakNotify)&gwy_log_data_finalized,
                        browser);
    g_string_free(browser->buf, TRUE);
    g_free(browser);
}

static void
gwy_log_data_finalized(LogBrowser *browser)
{
    browser->changed_id = 0;
    g_signal_handler_disconnect(browser->window, browser->destroy_id);
    gtk_widget_destroy(browser->window);
    g_free(browser);
}

static gchar*
format_args(const gchar *prefix)
{
    GwyContainer *settings = gwy_app_settings_get();
    GPtrArray *values = g_ptr_array_new();
    gchar *retval;

    gwy_container_foreach(settings, prefix, format_arg, values);
    g_ptr_array_add(values, NULL);
    retval = g_strjoinv(", ", (gchar**)values->pdata);
    g_ptr_array_free(values, TRUE);

    return retval;
}

static void
format_arg(gpointer hkey, gpointer hvalue, gpointer user_data)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *gvalue = (GValue*)hvalue;
    GPtrArray *values = (GPtrArray*)user_data;
    gchar *formatted = NULL;
    const gchar *name = g_quark_to_string(key);

    name = strrchr(name, '/');
    g_return_if_fail(name);
    name++;

    if (G_VALUE_HOLDS_DOUBLE(gvalue))
        formatted = g_strdup_printf("%s=%g", name, g_value_get_double(gvalue));
    else if (G_VALUE_HOLDS_INT(gvalue))
        formatted = g_strdup_printf("%s=%d", name, g_value_get_int(gvalue));
    else if (G_VALUE_HOLDS_INT64(gvalue))
        formatted = g_strdup_printf("%s=%" G_GINT64_FORMAT, name,
                                    g_value_get_int64(gvalue));
    else if (G_VALUE_HOLDS_BOOLEAN(gvalue)) {
        formatted = g_strdup_printf("%s=%s", name,
                                    g_value_get_boolean(gvalue)
                                    ? "True"
                                    : "False");
    }
    else if (G_VALUE_HOLDS_STRING(gvalue)) {
        gchar *s = g_strescape(g_value_get_string(gvalue), NULL);
        formatted = g_strdup_printf("%s=\"%s\"", name, s);
        g_free(s);
    }
    else if (G_VALUE_HOLDS_UCHAR(gvalue)) {
        gint c = g_value_get_uchar(gvalue);
        if (g_ascii_isprint(c) && !g_ascii_isspace(c))
            formatted = g_strdup_printf("%s='%c'", name, c);
        else
            formatted = g_strdup_printf("%s=0x%02x", name, c);
    }
    else {
        g_warning("Cannot format argument of type %s.",
                  g_type_name(G_VALUE_TYPE(gvalue)));
        return;
    }

    g_ptr_array_add(values, formatted);
}

static GQuark
channel_log_key(gint id)
{
    static gchar buf[32];
    g_snprintf(buf, sizeof(buf), "/%d/data/log", id);
    return g_quark_from_string(buf);
}

static GQuark
volume_log_key(gint id)
{
    static gchar buf[32];
    g_snprintf(buf, sizeof(buf), "/brick/%d/log", id);
    return g_quark_from_string(buf);
}

static gboolean
find_settings_prefix(const gchar *function,
                     const gchar *settings_name,
                     GString *prefix)
{
    g_string_assign(prefix, "/module/");

    if (settings_name)
        g_string_append(prefix, settings_name);

    if (g_str_has_prefix(function, "builtin::")) {
        if (!settings_name)
            g_string_assign(prefix, "/NO-SUCH-FUNCTION");
    }
    else if (g_str_has_prefix(function, "proc::")) {
        const gchar *name = function + 6;
        if (!gwy_process_func_exists(name)) {
            g_warning("Invalid data processing function name %s.", name);
            return FALSE;
        }
        if (!settings_name)
            g_string_append(prefix, name);
    }
    else if (g_str_has_prefix(function, "file::")) {
        const gchar *name = function + 6;
        if (!gwy_file_func_exists(name)) {
            g_warning("Invalid file function name %s.", name);
            return FALSE;
        }
        if (!settings_name)
            g_string_append(prefix, name);
    }
    else if (g_str_has_prefix(function, "graph::")) {
        const gchar *name = function + 7;
        if (!gwy_graph_func_exists(name)) {
            g_warning("Invalid graph function name %s.", name);
            return FALSE;
        }
        if (!settings_name)
            g_string_append(prefix, name);
    }
    else if (g_str_has_prefix(function, "volume::")) {
        const gchar *name = function + 8;
        if (!gwy_volume_func_exists(name)) {
            g_warning("Invalid volume function name %s.", name);
            return FALSE;
        }
        if (!settings_name)
            g_string_append(prefix, name);
    }
    else if (g_str_has_prefix(function, "tool::")) {
        const gchar *name = function + 6;
        GType type = g_type_from_name(name);
        GwyToolClass *klass;

        if (!type) {
            g_warning("Invalid tool name %s.", name);
            return FALSE;
        }

        if (!(klass = g_type_class_ref(type))) {
            g_warning("Invalid tool name %s.", name);
            return FALSE;
        }

        g_string_assign(prefix, klass->prefix);
        g_type_class_unref(klass);
    }
    else {
        g_warning("Invalid function name %s.", function);
        return FALSE;
    }

    return TRUE;
}

/**
 * gwy_log_get_enabled:
 *
 * Reports whether logging of data processing operations is globally enabled.
 *
 * Returns: %TRUE if logging is enabled, %FALSE if it is disabled.
 *
 * Since: 2.35
 **/
gboolean
gwy_log_get_enabled(void)
{
    return !log_disabled;
}

/**
 * gwy_log_set_enabled:
 * @setting: %TRUE to enable logging, %FALSE to disable it.
 *
 * Globally enables or disables logging of data processing operations.
 *
 * By default, logging is enabled.  Non-GUI applications that run module
 * functions may wish to disable it.  Of course, the log will presist only if
 * the data container is saved into a GWY file.
 *
 * If logging is disabled logging functions such as gwy_app_channel_log_add()
 * become no-op.  It is possible to run the log viewer with
 * gwy_app_log_browser_for_channel() to see log entries created when logging
 * was enabled.
 *
 * Since: 2.35
 **/
void
gwy_log_set_enabled(gboolean setting)
{
    log_disabled = !setting;
}

/************************** Documentation ****************************/

/**
 * SECTION:log
 * @title: log
 * @short_description: Logging data processing operations
 *
 * The data processing operation log is a linear sequence of operations applied
 * to a channel or volume data.  The log is informative and not meant to
 * capture all information necessary to reply the operations, even though it
 * can be sufficient for this purpose in simple cases.
 *
 * The log is a linear sequence.  This is only an approximation of the actual
 * flow of information in the data processing, which corresponds to an acyclic
 * directed graph (not necessarily connected as data, masks and presentations
 * can have distinct sources).  The following rules thus apply to make it
 * meaningful and useful.
 *
 * Each logging function takes two data identifiers: source and target. The
 * source corresponds to the operation input, the target corresponds to the
 * data whose log is being updated.  The target may have already a log only if
 * it is the same as the source (which corresponds to simple data modification
 * such as levelling or grain marking).  In all other cases the target must not
 * have a log yet – they represent the creation of new data either from scratch
 * or based on existing data (in the latter case the log of the existing data
 * is replicated to the new one).
 *
 * Complex multi-data operations are approximated by one of the simple
 * operations.  For instance, data arithmetic can be treated as the
 * construction of a new channel from scratch as it is unclear which input data
 * the output channel is actually based on, if any at all.  Modifications that
 * use data from other channels, such as masking using another data or tip
 * convolution, should be represented as simple modifications of the primary
 * channel.
 *
 * Logging functions such as gwy_app_channel_log_add() take settings values
 * corresponding to the function name and store them in the log entry.
 * If the settings are stored under a different name, use the "settings-name"
 * logging option to set the correct name.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
