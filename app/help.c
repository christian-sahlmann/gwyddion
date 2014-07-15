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
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#ifdef G_OS_WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#define UG_ONLINE_BASE "http://gwyddion.net/documentation/user-guide-"

typedef struct {
    const gchar *fulluri;   /* Use for external mapping. */
    const gchar *filename;
    const gchar *fragment;
} HelpURI;

typedef gboolean (*ShowUriFunc)(const gchar *uri);

static gboolean
show_uri_win32(G_GNUC_UNUSED const gchar *uri)
{
#ifdef G_OS_WIN32
    static gboolean initialised_com = FALSE;
    gint status;

    if (G_UNLIKELY(!initialised_com)) {
        initialised_com = TRUE;
        /* Not actually sure what it does but MSDN says it is good for us. */
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    }

    /* XXX: The first arg is handle to the window.  May want to pass it.
     * Apparenly gdk_win32_window_get_impl_hwnd() can provide it but this is
     * a late addition to Gdk, must check availability properly. */
    status = ShellExecute(NULL, NULL, uri, NULL, NULL, SW_SHOWNORMAL);
    return status > 32;  /* Otherwise it's the error code. */
#else
    return FALSE;
#endif
}

static gboolean
show_uri_gtk(G_GNUC_UNUSED const gchar *uri)
{
#if GTK_CHECK_VERSION(2,14,0)
    return gtk_show_uri(NULL, uri, GDK_CURRENT_TIME, NULL);
#else
    return FALSE;
#endif
}

static gboolean
show_uri_spawn(G_GNUC_UNUSED const gchar *uri)
{
#ifndef G_OS_WIN32
    static const gchar *programs[] = { "xdg-open", "htmlview" };

    gchar **args;
    gchar *fullpath = NULL;
    gboolean ok;
    guint i;

    for (i = 0; i < G_N_ELEMENTS(programs); i++) {
        if ((fullpath = g_find_program_in_path(programs[i])))
            break;
    }
    if (!fullpath)
        return FALSE;

    args = g_new(gchar*, 3);
    args[0] = fullpath;
    args[1] = g_strdup(uri);
    args[2] = NULL;
    ok = g_spawn_async(NULL, args, NULL, 0, NULL, NULL, NULL, NULL);
    g_strfreev(args);

    return ok;
#else
    return FALSE;
#endif
}

static gboolean
show_help_uri(const gchar *uri)
{
    static const ShowUriFunc backends[] = {
        &show_uri_win32,
        &show_uri_gtk,
        &show_uri_spawn,
    };

    guint i;

    for (i = 0; i < G_N_ELEMENTS(backends); i++) {
        if (backends[i](uri))
            return TRUE;
    }

    return FALSE;
}

static void
add_module_functions(gpointer hkey,
                     G_GNUC_UNUSED gpointer hvalue,
                     gpointer user_data)
{
    GHashTable *cache = (GHashTable*)user_data;
    const gchar *modname = (const gchar*)hkey;
    GSList *modfunc;

    modfunc = gwy_module_get_functions(modname);
    while (modfunc) {
        g_hash_table_insert(cache, modfunc->data, (gpointer)modname);
        modfunc = g_slist_next(modfunc);
    }
}

static GHashTable*
build_function_module_map(void)
{
    GHashTable *cache = g_hash_table_new(g_str_hash, g_str_equal);

    gwy_module_foreach(&add_module_functions, cache);
    return cache;
}

static GHashTable*
build_user_guide_map(void)
{
    GHashTable *cache = g_hash_table_new(g_str_hash, g_str_equal);
    gchar *p, *q, *text, *line;

    q = gwy_find_self_dir("data");
    p = g_build_filename(q, "user-guide-modules", NULL);
    g_free(q);
    if (g_file_get_contents(p, &text, NULL, NULL)) {
        q = text;
        while ((line = gwy_str_next_line(&q))) {
            HelpURI *helpuri;
            gchar **fields;

            g_strstrip(line);
            if (!*line || line[0] == '#')
                continue;

            fields = g_strsplit(line, "\t", 0);
            if (g_strv_length(fields) == 2 || g_strv_length(fields) == 3) {
                helpuri = g_slice_new(HelpURI);
                helpuri->fulluri = NULL;
                helpuri->filename = fields[1];
                helpuri->fragment = fields[2];   /* May be NULL which is OK. */
                g_hash_table_insert(cache, fields[0], helpuri);
                g_free(fields);
            }
            else {
                g_warning("Malformed user-guide-modules line: %s", line);
                g_strfreev(fields);
            }
        }
    }
    g_free(p);
    g_free(text);

    return cache;
}

static const HelpURI*
get_uri_path_for_module(const gchar *modname)
{
    static GHashTable *userguidemap = NULL;

    if (G_UNLIKELY(!userguidemap))
        userguidemap = build_user_guide_map();

    return g_hash_table_lookup(userguidemap, (gpointer)modname);
}

static gchar*
get_user_guide_online_base(void)
{
    /* We know we have these guides available. */
    static const gchar *user_guides[] = { "en", "fr", "ru" };

    /* TRANSLATORS: For user guide location.  Translate this to fr, ru, cs,
     * de, ...  even if the corresponding guide does not exist. */
    const gchar *lang = gwy_sgettext("current-language-code|en");
    guint i;

    for (i = 0; i < G_N_ELEMENTS(user_guides); i++) {
        if (gwy_strequal(user_guides[i], lang))
            break;
    }
    lang = user_guides[(i == G_N_ELEMENTS(user_guides)) ? 0 : i];
    return g_strconcat(UG_ONLINE_BASE, lang, NULL);
}

static gchar*
get_user_guide_settings_base(void)
{
    GwyContainer *settings = gwy_app_settings_get();
    const guchar *settingsugbase = NULL;
    guint len;
    gchar *base;

    if (!gwy_container_gis_string_by_name(settings,
                                          "/app/help/user-guide-base",
                                          &settingsugbase))
        return NULL;

    base = g_strdup(settingsugbase);
    len = strlen(base);
    while (len && base[len-1] == '/')
        len--;
    base[len] = '\0';

    return base;
}

static const gchar*
get_user_guide_base(void)
{
    gchar *base = NULL;

    if (G_UNLIKELY(!base)) {
        if (!(base = get_user_guide_settings_base()))
            base = get_user_guide_online_base();
    }

    return base;
}

static gchar*
build_uri_for_function(const gchar *type, const gchar *funcname)
{
    static GHashTable *userguidemap = NULL;

    const HelpURI *helpuri;
    const gchar *modname, *ugbase;
    gchar *qname, *uri;

    if (G_UNLIKELY(!userguidemap))
        userguidemap = build_function_module_map();

    qname = g_strconcat(type, "::", funcname, NULL);
    modname = g_hash_table_lookup(userguidemap, qname);
    g_free(qname);
    g_return_val_if_fail(modname, NULL);

    helpuri = get_uri_path_for_module(modname);
    if (!helpuri)
        return NULL;

    if (helpuri->fulluri)
        return g_strdup(helpuri->fulluri);

    ugbase = get_user_guide_base();
    uri = g_strconcat(ugbase, "/",
                      helpuri->filename, ".html",
                      helpuri->fragment ? "#" : NULL, helpuri->fragment, NULL);

    return uri;
}

static void
dialog_response(GtkDialog *dialog,
                gint response_id,
                gpointer user_data)
{
    static guint signal_id = 0;

    const gchar *uri = (const gchar*)user_data;

    if (response_id != GTK_RESPONSE_HELP)
        return;

    if (G_UNLIKELY(!signal_id))
        signal_id = g_signal_lookup("response", GTK_TYPE_DIALOG);

    g_signal_stop_emission(dialog, signal_id, 0);
    g_return_if_fail(uri);

    show_help_uri(uri);
}

static gboolean
key_press_event(G_GNUC_UNUSED GtkWidget *widget,
                GdkEventKey *event,
                gpointer user_data)
{
    const gchar *uri = (const gchar*)user_data;

    if ((event->keyval != GDK_Help
         && event->keyval != GDK_F1)
        || (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)))
        return FALSE;

    show_help_uri(uri);
    return TRUE;
}

/**
 * gwy_help_add_proc_dialog_button:
 * @dialog: Main dialog for a data processing function.
 *
 * Adds a Help button to a data processing function dialog.
 *
 * Note the help button may not be added if no help URI is found for the
 * currently running function.
 *
 * Since: 2.38
 **/
void
gwy_help_add_proc_dialog_button(GtkDialog *dialog)
{
    const gchar *funcname;
    gchar *uri;

    g_return_if_fail(GTK_IS_DIALOG(dialog));
    funcname = gwy_process_func_current();
    g_return_if_fail(funcname);

    uri = build_uri_for_function("proc", funcname);
    if (!uri)
        return;

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_HELP, GTK_RESPONSE_HELP);
    g_signal_connect(dialog, "response",
                     G_CALLBACK(dialog_response), uri);
    g_signal_connect(dialog, "key-press-event",
                     G_CALLBACK(key_press_event), uri);
    g_object_weak_ref(G_OBJECT(dialog), (GWeakNotify)g_free, uri);
}

/************************** Documentation ****************************/

/**
 * SECTION:help
 * @title: help
 * @short_description: User guide access
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
