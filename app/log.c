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
#include <libgwyddion/gwymacros.h>
#include <app/gwytool.h>
#include <app/settings.h>
#include <app/log.h>

static gchar* format_args(const gchar *prefix);
static void format_arg(gpointer hkey, gpointer hvalue, gpointer user_data);
static const gchar* channel_log_key(gint id);
static gboolean find_settings_prefix(const gchar *function,
                                     const gchar *settings_name,
                                     GString *prefix);

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
    GString *str;
    const gchar *key, *settings_name = NULL;
    gchar *args;
    GwyStringList *sourcelog = NULL, *targetlog = NULL;

    g_return_if_fail(GWY_IS_CONTAINER(data));
    g_return_if_fail(newid >= 0);
    g_return_if_fail(function);

    va_start(ap, function);
    while ((key = va_arg(ap, const gchar*))) {
        if (gwy_strequal(key, "settings-name")) {
            settings_name = va_arg(ap, const gchar*);
        }
        else {
            g_warning("Invalid logging option %s.", key);
            return;
        }
    }
    va_end(ap);

    str = g_string_new(NULL);
    if (!find_settings_prefix(function, settings_name, str)) {
        g_string_free(str, TRUE);
        return;
    }

    if (previd != -1)
        sourcelog = gwy_app_get_channel_log(data, previd, FALSE);

    if (newid == previd)
        targetlog = sourcelog;
    else
        targetlog = gwy_app_get_channel_log(data, newid, FALSE);

    if (targetlog && targetlog != sourcelog) {
        g_warning("Target log must not exist when replicating logs.");
        /* Fix the operation to simple log-append. */
        sourcelog = targetlog;
        previd = newid;
    }

    if (!targetlog) {
        if (sourcelog)
            targetlog = gwy_string_list_duplicate(sourcelog);
        else
            targetlog = gwy_string_list_new();

        gwy_container_set_object_by_name(data, channel_log_key(newid),
                                         targetlog);
        g_object_unref(targetlog);
    }

    args = format_args(str->str);
    g_string_printf(str, "%s(%s)", function, args);
    gwy_string_list_append_take(targetlog, g_string_free(str, FALSE));
}

/**
 * gwy_app_get_channel_log:
 * @data: A data container.
 * @id: Channel identifier.
 * @create: Pass %TRUE to create the log if it does not exist.
 *
 * Obtains the data processing operation log for a channel.
 *
 * Returns: The log.  It may be %NULL if no log exists and @create is %FALSE.
 *
 * Since: 2.35
 **/
GwyStringList*
gwy_app_get_channel_log(GwyContainer *data,
                        gint id,
                        gboolean create)
{
    GwyStringList *log = NULL;
    const gchar *strkey = channel_log_key(id);

    gwy_container_gis_object_by_name(data, strkey, &log);

    if (log || !create)
        return log;

    log = gwy_string_list_new();
    gwy_container_set_object_by_name(data, strkey, log);
    g_object_unref(log);

    return log;
}

static gchar*
format_args(const gchar *prefix)
{
    GwyContainer *settings = gwy_app_settings_get();
    GPtrArray *values = g_ptr_array_new();
    gchar *retval;

    gwy_container_foreach(settings, prefix, format_arg, values);
    g_ptr_array_add(values, NULL);
    retval = g_strjoinv(",", (gchar**)values->pdata);
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

static const gchar*
channel_log_key(gint id)
{
    static gchar buf[32];
    g_snprintf(buf, sizeof(buf), "/%d/data/log", id);
    return buf;
}

static gboolean
find_settings_prefix(const gchar *function,
                     const gchar *settings_name,
                     GString *prefix)
{
    g_string_assign(prefix, "/module/");

    if (settings_name)
        g_string_append(prefix, settings_name);
    else if (g_str_has_prefix(function, "proc::"))
        g_string_append(prefix, function + 6);
    else if (g_str_has_prefix(function, "file::"))
        g_string_append(prefix, function + 6);
    else if (g_str_has_prefix(function, "graph::"))
        g_string_append(prefix, function + 7);
    else if (g_str_has_prefix(function, "volume::"))
        g_string_append(prefix, function + 8);
    else if (g_str_has_prefix(function, "tool::")) {
        GType type = g_type_from_name(function + 6);
        GwyToolClass *klass;

        if (!type) {
            g_warning("Invalid tool name %s.", function + 6);
            return FALSE;
        }

        if (!(klass = g_type_class_ref(type))) {
            g_warning("Invalid tool name %s.", function + 6);
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
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
