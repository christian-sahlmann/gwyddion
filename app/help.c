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

/* The function is expected to just return TRUE if @uri is NULL but the backend
 * seems to be available. */
typedef gboolean (*ShowUriFunc)(const gchar *uri);

static gboolean
show_uri_win32(G_GNUC_UNUSED const gchar *uri)
{
#ifdef G_OS_WIN32
    static gboolean initialised_com = FALSE;
    gint status;

    if (!uri)
        return TRUE;

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

/* The show-uri thing seems to have an unfortunate side-effect of switching to
 * the busy mouse cursor for a while. */
static gboolean
show_uri_gtk(G_GNUC_UNUSED const gchar *uri)
{
#if GTK_CHECK_VERSION(2,14,0)
    /* This may not be the whole story because GVfs may not support our URI
     * scheme.  But leave this to the diagnosis when things fail to work. */
    if (!uri)
        return TRUE;

    return gtk_show_uri(NULL, uri, GDK_CURRENT_TIME, NULL);
#else
    return FALSE;
#endif
}

static gboolean
show_uri_spawn(G_GNUC_UNUSED const gchar *uri)
{
#ifndef G_OS_WIN32
    static const gchar *programs[] = {
        /* OS X has this little program called "open".  Sure, other systems
         * have it too but it does something completely different.  So only
         * use "open" on OS X. */
#ifdef __APPLE__
        "open",
#endif
        "xdg-open", "htmlview",
        "chrome", "chromium",  /* Normally installed by people who want them. */
        "firefox", "seamonkey",
        "konqueror",
        "midori", "epiphany",
    };

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

    if (!uri) {
        g_free(fullpath);
        return TRUE;
    }

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
    /* The platform-specific ones go first. */
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

    /* TODO: Show some error instead of a silent failure.  Possibly offer
     * some advice what may be broken dependin on the base prefix: for
     * http(s):// we need network connection; for file:// and paths we need the
     * guide to be present locally.  At least do this the first time this
     * happens (may want to disable help for the rest of the session). */
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
       de, ...  even if the corresponding guide does not exist. */
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

static void
add_help_to_window(GtkWindow *window,
                   gchar *uri,
                   GwyHelpFlags flags)
{
    if (g_object_get_data(G_OBJECT(window), "gwy-help-uri")) {
        g_warning("Window %p already has help URI: %s",
                  window,
                  (gchar*)g_object_get_data(G_OBJECT(window), "gwy-help-uri"));
        return;
    }
    g_object_set_data(G_OBJECT(window), "gwy-help-uri", uri);

    if (!(flags & GWY_HELP_NO_BUTTON) && GTK_IS_DIALOG(window)) {
        gtk_dialog_add_button(GTK_DIALOG(window),
                              GTK_STOCK_HELP, GTK_RESPONSE_HELP);
        g_signal_connect(window, "response",
                         G_CALLBACK(dialog_response), uri);
    }
    g_signal_connect(window, "key-press-event",
                     G_CALLBACK(key_press_event), uri);
    g_object_weak_ref(G_OBJECT(window), (GWeakNotify)g_free, uri);
}

/**
 * gwy_help_add_to_proc_dialog:
 * @dialog: Main dialog for a data processing function.
 * @flags: Flags allowing to modify the help setup.
 *
 * Adds help to a data processing function dialog.
 *
 * Note the help button will not be added if no help URI is found for the
 * currently running function.
 *
 * Since: 2.38
 **/
void
gwy_help_add_to_proc_dialog(GtkDialog *dialog, GwyHelpFlags flags)
{
    const gchar *funcname;
    gchar *uri;

    g_return_if_fail(GTK_IS_DIALOG(dialog));
    funcname = gwy_process_func_current();
    g_return_if_fail(funcname);

    if (!(uri = build_uri_for_function("proc", funcname)))
        return;

    add_help_to_window(GTK_WINDOW(dialog), uri, flags);
}

/**
 * gwy_help_add_to_graph_dialog:
 * @dialog: Main dialog for a graph function.
 * @flags: Flags allowing to modify the help setup.
 *
 * Adds help to a graph function dialog.
 *
 * Note the help button will not be added if no help URI is found for the
 * currently running function.
 *
 * Since: 2.38
 **/
void
gwy_help_add_to_graph_dialog(GtkDialog *dialog, GwyHelpFlags flags)
{
    const gchar *funcname;
    gchar *uri;

    g_return_if_fail(GTK_IS_DIALOG(dialog));
    funcname = gwy_graph_func_current();
    g_return_if_fail(funcname);

    if (!(uri = build_uri_for_function("graph", funcname)))
        return;

    add_help_to_window(GTK_WINDOW(dialog), uri, flags);
}

/**
 * gwy_help_add_to_volume_dialog:
 * @dialog: Main dialog for a volume data processing function.
 * @flags: Flags allowing to modify the help setup.
 *
 * Adds help to a volume data processing function dialog.
 *
 * Note the help button will not be added if no help URI is found for the
 * currently running function.
 *
 * Since: 2.38
 **/
void
gwy_help_add_to_volume_dialog(GtkDialog *dialog, GwyHelpFlags flags)
{
    const gchar *funcname;
    gchar *uri;

    g_return_if_fail(GTK_IS_DIALOG(dialog));
    funcname = gwy_volume_func_current();
    g_return_if_fail(funcname);

    if (!(uri = build_uri_for_function("volume", funcname)))
        return;

    add_help_to_window(GTK_WINDOW(dialog), uri, flags);
}

/**
 * gwy_help_add_to_file_dialog:
 * @dialog: Main dialog for a file function.
 * @flags: Flags allowing to modify the help setup.
 *
 * Adds help to a file function dialog.
 *
 * Note the help button will not be added if no help URI is found for the
 * currently running function.
 *
 * Since: 2.38
 **/
void
gwy_help_add_to_file_dialog(GtkDialog *dialog, GwyHelpFlags flags)
{
    const gchar *funcname;
    gchar *uri;

    g_return_if_fail(GTK_IS_DIALOG(dialog));
    funcname = gwy_file_func_current();
    g_return_if_fail(funcname);

    if (!(uri = build_uri_for_function("file", funcname)))
        return;

    add_help_to_window(GTK_WINDOW(dialog), uri, flags);
}

/**
 * gwy_help_add_to_tool_dialog:
 * @dialog: Main dialog for a tool function.
 * @tool: The tool.
 * @flags: Flags allowing to modify the help setup.
 *
 * Adds help to a tool dialog.
 *
 * Note the help button will not be added if no help URI is found for the
 * currently running function.
 *
 * Since: 2.38
 **/
void
gwy_help_add_to_tool_dialog(GtkDialog *dialog,
                            GwyTool *tool,
                            GwyHelpFlags flags)
{
    const gchar *funcname;
    gchar *uri;

    g_return_if_fail(GTK_IS_DIALOG(dialog));
    g_return_if_fail(GWY_IS_TOOL(tool));
    funcname = G_OBJECT_TYPE_NAME(tool);
    g_return_if_fail(funcname);

    if (!(uri = build_uri_for_function("tool", funcname)))
        return;

    add_help_to_window(GTK_WINDOW(dialog), uri, flags);
}

/**
 * gwy_help_add_to_window:
 * @window: A window.
 * @filename: Base file name in the user guide without any path or extensions,
 *            for instance "statistical-analysis".
 * @fragment: Fragment identifier (without "#"), or possibly %NULL.
 * @flags: Flags allowing to modify the help setup.
 *
 * Adds help to a window pointing to the user guide.
 *
 * If the window is a #GtkDialog a help button will be added by default (this
 * can be modified with @flags).  Normal windows do not get help buttons.
 *
 * This is a relatively low-level function and should not be necessary in
 * modules.  An exception may be modules with multiple user interfaces
 * described in different parts of the guide – but this should be rare.
 *
 * It is a suitable functions for adding help to base application windows,
 * such as channel or volume windows.
 *
 * Since: 2.38
 **/
void
gwy_help_add_to_window(GtkWindow *window,
                       const gchar *filename,
                       const gchar *fragment,
                       GwyHelpFlags flags)
{
    const gchar *ugbase;
    gchar *uri;

    g_return_if_fail(GTK_IS_WINDOW(window));
    g_return_if_fail(filename);

    ugbase = get_user_guide_base();
    uri = g_strconcat(ugbase, "/",
                      filename, ".html",
                      fragment ? "#" : NULL, fragment, NULL);
    add_help_to_window(window, uri, flags);
}

/**
 * gwy_help_add_to_window:
 * @window: A window.
 * @uri: Full URI pointing to the help for @window.
 * @flags: Flags allowing to modify the help setup.
 *
 * Adds help to a window pointing to an arbitrary URI.
 *
 * If the window is a #GtkDialog a help button will be added by default (this
 * can be modified with @flags).  Normal windows do not get help buttons.
 *
 * This function should not be necessary anywhere within Gwyddion itself.
 * Use the functions pointing to the user guide instead as they can handle
 * language versions or changing the user guide base location.
 *
 * It may be useful for third-party modules if they wish to add a help facility
 * behaving similarly to built-in modules.
 *
 * Since: 2.38
 **/
void
gwy_help_add_to_window_uri(GtkWindow *window,
                           const gchar *uri,
                           GwyHelpFlags flags)
{
    g_return_if_fail(GTK_IS_WINDOW(window));
    g_return_if_fail(uri);
    add_help_to_window(window, g_strdup(uri), flags);
}

/**
 * gwy_help_show:
 * @filename: Base file name in the user guide without any path or extensions,
 *            for instance "statistical-analysis".
 * @fragment: Fragment identifier (without "#"), or possibly %NULL.
 *
 * Immediately shows a specific help location.
 *
 * This function should be rarely needed.
 *
 * Since: 2.38
 **/
void
gwy_help_show(const gchar *filename,
              const gchar *fragment)
{
    const gchar *ugbase;
    gchar *uri;

    g_return_if_fail(filename);

    ugbase = get_user_guide_base();
    uri = g_strconcat(ugbase, "/",
                      filename, ".html",
                      fragment ? "#" : NULL, fragment, NULL);
    show_help_uri(uri);
    g_free(uri);
}

/************************** Documentation ****************************/

/**
 * SECTION:help
 * @title: help
 * @short_description: User guide access
 *
 * Help functions add a Help button to dialogs and install a key press handler
 * responding to Help and F1 keys to all windows.  For built-in modules,
 * invoking the help opens a web browser (using some generic and
 * system-specific means) pointing to the corresponding location in the user
 * guide.  Third-party modules can use gwy_help_add_to_window_uri() to open a
 * web browser at an arbitrary URI.
 **/

/**
 * GwyHelpFlags:
 * @GWY_HELP_DEFAULT: No flags, the default behaviour.
 * @GWY_HELP_NO_BUTTON: Do not add a Help button, even to windows that are
 *                      dialogs.
 *
 * Flags controlling help setup and behaviour.
 *
 * Since: 2.38
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
