/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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
#include <string.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <app/gwyapp.h>
#include "gwyappinternal.h"

/* Our handler should not get G_LOG_LEVEL_ERROR errors but it does not hurt. */
enum {
    ALERT_LEVELS = (G_LOG_LEVEL_ERROR
                    | G_LOG_LEVEL_CRITICAL
                    | G_LOG_LEVEL_WARNING)
};

typedef struct {
    FILE *file;
    GString *str;
    GString *last;
    guint last_count;
    GLogLevelFlags last_level;
    GwyAppLoggingFlags flags;
    gboolean to_file;
    gboolean to_console;
} LoggingSetup;

static void  logger             (const gchar *log_domain,
                                 GLogLevelFlags log_level,
                                 const gchar *message,
                                 gpointer user_data);
static void  format_log_message (GString *str,
                                 const gchar *log_domain,
                                 GLogLevelFlags log_level,
                                 const gchar *message);
static void  emit_log_message   (LoggingSetup *setup,
                                 GLogLevelFlags log_level);
static void  append_level_prefix(GString *str,
                                 GLogLevelFlags log_level);
static FILE* get_console_stream (GLogLevelFlags log_level);

static gboolean log_capturing_now = FALSE;
static GPtrArray *log_captured_messages = NULL;

/**
 * gwy_app_setup_logging:
 * @flags: Flags controlling how messages are handled.
 *
 * Sets up Gwyddion GLib log handler.
 *
 * The log handler sends the messages to a log file or console, as Gwyddion
 * usually does.  This function may not be useful in Gwyddion-based programs
 * unless they try to emulate Gwyddion behaviour closely.
 *
 * Since: 2.45
 **/
void
gwy_app_setup_logging(GwyAppLoggingFlags flags)
{
    static const gchar *domains[] = {
        "GLib", "GLib-GObject", "GLib-GIO", "GModule", "GThread",
        "GdkPixbuf", "Gdk", "Gtk",
        "GdkGLExt", "GtkGLExt",
        "Pango", "Unique",
        "Gwyddion", "GwyProcess", "GwyDraw", "Gwydgets", "GwyModule", "GwyApp",
        "Module", NULL
    };

    static gboolean logging_set_up = FALSE;

    LoggingSetup *setup;
    guint i;

    if (logging_set_up) {
        g_warning("Logging has already been set up.");
        return;
    }

    logging_set_up = TRUE;
    setup = g_new0(LoggingSetup, 1);
    setup->flags = flags;

    setup->to_console = (flags & GWY_APP_LOGGING_TO_CONSOLE);
    if (flags & GWY_APP_LOGGING_TO_FILE) {
        const gchar *log_filename = gwy_app_settings_get_log_filename();
        setup->file = gwy_fopen(log_filename, "w");
        setup->to_file = !!setup->file;
    }

    setup->str = g_string_new(NULL);
    setup->last = g_string_new(NULL);

    for (i = 0; i < G_N_ELEMENTS(domains); i++) {
        g_log_set_handler(domains[i],
                          G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_MESSAGE
                          | G_LOG_LEVEL_INFO | G_LOG_LEVEL_WARNING
                          | G_LOG_LEVEL_CRITICAL,
                          logger, setup);
    }
}

void
_gwy_app_log_start_message_capture(void)
{
    g_return_if_fail(!log_capturing_now);
    log_capturing_now = TRUE;
    if (G_UNLIKELY(!log_captured_messages))
        log_captured_messages = g_ptr_array_new();
}

gchar**
_gwy_app_log_get_captured_messages(void)
{
    gchar **messages;

    g_return_val_if_fail(log_capturing_now, NULL);
    log_capturing_now = FALSE;

    if (!log_captured_messages->len)
        return NULL;

    g_ptr_array_add(log_captured_messages, NULL);
    messages = g_memdup(log_captured_messages->pdata,
                        log_captured_messages->len*sizeof(gchar*));
    g_ptr_array_set_size(log_captured_messages, 0);
    return messages;
}

static void
logger(const gchar *log_domain,
       GLogLevelFlags log_level,
       const gchar *message,
       gpointer user_data)
{
    LoggingSetup *setup = (LoggingSetup*)user_data;
    gboolean to_file = setup->to_file;
    gboolean to_console = setup->to_console;
    GLogLevelFlags just_log_level = (log_level & G_LOG_LEVEL_MASK);
    GString *str = setup->str;
    GString *last = setup->last;

    if (!to_file && !to_console)
        return;

    if (log_level == setup->last_level && gwy_strequal(message, last->str)) {
        setup->last_count++;
        return;
    }

    if (setup->last_count) {
        g_string_printf(last, "Last message repeated %u times",
                        setup->last_count);
        if (log_capturing_now && (log_level & ~G_LOG_LEVEL_DEBUG))
            g_ptr_array_add(log_captured_messages, g_strdup(last->str));
        format_log_message(str, log_domain, just_log_level, last->str);
        emit_log_message(setup, just_log_level);
    }

    g_string_assign(last, message);
    setup->last_level = log_level;
    setup->last_count = 0;

    if (log_capturing_now && (log_level & ~G_LOG_LEVEL_DEBUG))
        g_ptr_array_add(log_captured_messages, g_strdup(message));
    format_log_message(str, log_domain, just_log_level, message);
    emit_log_message(setup, just_log_level);
}

static void
emit_log_message(LoggingSetup *setup, GLogLevelFlags log_level)
{
    if (setup->to_file) {
        fputs(setup->str->str, setup->file);
        fflush(setup->file);
    }

    if (setup->to_console) {
        FILE *console_stream = get_console_stream(log_level & G_LOG_LEVEL_MASK);
        fputs(setup->str->str, console_stream);
        fflush(console_stream);
    }
}

static void
format_log_message(GString *str,
                   const gchar *log_domain,
                   GLogLevelFlags log_level,
                   const gchar *message)
{
    g_string_truncate(str, 0);
    if (log_level & ALERT_LEVELS)
        g_string_append_c(str, '\n');
    if (!log_domain)
        g_string_append(str, "** ");

    /* GLib uses g_log_msg_prefix but we do not have access to it. */
    if (TRUE) {
        const gchar *prg_name = g_get_prgname();
        gulong pid = getpid();

        if (!prg_name)
            g_string_append_printf(str, "(process:%lu): ", pid);
        else
            g_string_append_printf(str, "(%s:%lu): ", prg_name, pid);
    }
    if (log_domain) {
        g_string_append(str, log_domain);
        g_string_append_c(str, '-');
    }

    append_level_prefix(str, log_level);
    if (log_level & ALERT_LEVELS)
        g_string_append(str, " **");

    g_string_append(str, ": ");
    if (!message)
        g_string_append(str, "(NULL) message");
    else {
        /* XXX: GLib does (a) escaping (b) conversion from UTF-8 here. */
        g_string_append(str, message);
    }
    g_string_append_c(str, '\n');
}

/* Similar to GLib's function but we do not handle recursive and fatal errors
 * so we do not have to handle the difficult cases and can use GLib functions.
 */
static void
append_level_prefix(GString *str,
                    GLogLevelFlags log_level)
{
    if (log_level == G_LOG_LEVEL_ERROR)
        g_string_append(str, "ERROR");
    else if (log_level == G_LOG_LEVEL_CRITICAL)
        g_string_append(str, "CRITICAL");
    else if (log_level == G_LOG_LEVEL_WARNING)
        g_string_append(str, "WARNING");
    else if (log_level == G_LOG_LEVEL_MESSAGE)
        g_string_append(str, "Message");
    else if (log_level == G_LOG_LEVEL_INFO)
        g_string_append(str, "INFO");
    else if (log_level == G_LOG_LEVEL_DEBUG)
        g_string_append(str, "DEBUG");
    else if (log_level)
        g_string_append_printf(str, "LOG-%u", log_level);
    else
        g_string_append(str, "LOG");
}

static FILE*
get_console_stream(GLogLevelFlags log_level)
{
    if (log_level == G_LOG_LEVEL_ERROR
        || log_level == G_LOG_LEVEL_CRITICAL
        || log_level == G_LOG_LEVEL_WARNING
        || log_level == G_LOG_LEVEL_MESSAGE)
        return stderr;
    return stdout;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
