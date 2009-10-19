/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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

/*
 * The X11 remote control code was more or less copied from:
 *
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimp-remote.c
 * Copyright (C) 2000-2004  Sven Neumann <sven@gimp.org>
 *                          Simon Budig <simon@gimp.org>
 *
 * Tells a running gimp to open files by creating a synthetic drop-event.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyenum.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/file.h>
#include "gwyddion.h"

#if (REMOTE_BACKEND == REMOTE_X11)
#include <gdk/gdkx.h>
#include <X11/Xmu/WinUtil.h>
#include <X11/Xatom.h>
#endif

#if (REMOTE_BACKEND == REMOTE_WIN32)
#include <gdk/gdkwin32.h>
#include <shlobj.h>
#endif

#if (REMOTE_BACKEND == REMOTE_UNIQUE)
#include <unique/unique.h>
#endif

static GdkWindow* remote_find_toolbox(GdkDisplay *display,
                                      guint32 *xid);
static gboolean   do_remote          (GdkDisplay *display,
                                      GdkWindow *toolbox,
                                      guint32 xid,
                                      int argc,
                                      char **argv);

G_GNUC_UNUSED
static gboolean
toolbox_timeout(G_GNUC_UNUSED gpointer data)
{
    g_printerr("Could not connect to the toolbox.\n");
    gtk_main_quit();

    return TRUE;
}

G_GNUC_UNUSED
static void
source_selection_get(G_GNUC_UNUSED GtkWidget *widget,
                     GtkSelectionData *selection_data,
                     G_GNUC_UNUSED guint info,
                     G_GNUC_UNUSED guint time_,
                     const gchar *uri)
{
    gtk_selection_data_set(selection_data, selection_data->target,
                           8, uri, strlen(uri));
    gtk_main_quit();
}

void
gwy_app_do_remote(GwyAppRemoteType type,
                  int argc,
                  char **argv)
{
    GdkDisplay *display;
    GdkWindow *toolbox;
    guint32 xid = 0;  /* Die, die, GCC! */

    if (type == GWY_APP_REMOTE_NONE)
        return;

    /* No args, nothing to do. Silly, but consistent. */
    if (type == GWY_APP_REMOTE_EXISTING && !argc)
        exit(EXIT_SUCCESS);

    display = gdk_display_get_default();
    toolbox = remote_find_toolbox(display, &xid);
    gwy_debug("Toolbox: %p, xid: 0x%08x", toolbox, xid);

    switch (type) {
        case GWY_APP_REMOTE_EXISTING:
        if (!toolbox) {
            g_printerr("No Gwyddion toolbox window found.\n");
            exit(EXIT_FAILURE);
        }
        break;

        case GWY_APP_REMOTE_NEW:
        /* Returning simply continues the execution of Gwyddion with the
         * file arguments we've got. */
        if (!toolbox)
            return;
        if (!argc)
            exit(EXIT_SUCCESS);
        break;

        case GWY_APP_REMOTE_QUERY:
        if (toolbox) {
            printf("0x%08x\n", xid);
            exit(EXIT_SUCCESS);
        }
        exit(EXIT_FAILURE);
        break;

        default:
        g_return_if_reached();
        break;
    }

    /* Call appropriate remote handler for X11 and win32 */
    if (!do_remote(display, toolbox, xid, argc, argv)) {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

#if (REMOTE_BACKEND == REMOTE_WIN32)
void
gwy_app_remote_setup(GtkWidget *toolbox)
{
    GdkWindow *gdk_win;
    HWND hwnd = 0;

    gdk_win = gtk_widget_get_root_window(toolbox);
    // Retrieve hWnd using gdk window
    hwnd = GDK_WINDOW_HWND(toolbox->window);
    // Create property and set it to 1
    SetProp(hwnd, GWY_TOOLBOX_WM_ROLE, (HANDLE)1);
    gwy_debug("SetProp to hWnd %d\n", hwnd);
}

void
gwy_app_remote_finalize(GtkWidget *toolbox)
{
    HWND hwnd = 0;

    // Retrieve hWnd
    hwnd = GDK_WINDOW_HWND(gtk_widget_get_root_window(toolbox));
    // Remove properties
    RemoveProp(hwnd, GWY_TOOLBOX_WM_ROLE);
    gwy_debug("RemoveProp to hWnd %d\n", hwnd);
}

/* Send WM_DROPFILES message to target window
   Return -1 when window could not be found or memory cannot be allocated
   for DnD operation */
static gboolean
do_remote(GdkDisplay *display,
          GdkWindow *toolbox,
          guint32 xid,
          int argc,
          char **argv)
{
    int iCurBytePos = sizeof(DROPFILES);
    LPDROPFILES pDropFiles;
    HGLOBAL hGlobal;
    HWND hWnd;
    gchar *fullFilename, *cwd;
    int i;

    hWnd = GDK_WINDOW_HWND(toolbox);
    if (!hWnd) {
        g_printerr("Cannot find target toolbox.\n");
        return FALSE;
    }
    // May use more memory than is needed... oh well.
    hGlobal = GlobalAlloc(GHND | GMEM_SHARE,
                          sizeof(DROPFILES) + (_MAX_PATH * argc) + 1);

    // memory failure?
    if (hGlobal == NULL) {
        g_printerr("Cannot allocate memory.\n");
        return FALSE;
    }

    // lock the memory
    pDropFiles = (LPDROPFILES)GlobalLock(hGlobal);

    // set offset where the file list begins
    pDropFiles->pFiles = sizeof(DROPFILES);

    // no wide chars and drop point is in client coordinates
    pDropFiles->fWide = FALSE;
    pDropFiles->pt.x = pDropFiles->pt.y = 0;
    pDropFiles->fNC = FALSE;

    cwd = g_get_current_dir();
    for (i = 0; i < argc; ++i) {
        // file location must be absolute
        if (g_path_is_absolute(argv[i])) {
            fullFilename = g_strdup(argv[i]);
        }
        else {
            fullFilename = g_build_filename(cwd, argv[i], NULL);
        }
        strcpy(((LPSTR)(pDropFiles) + iCurBytePos), fullFilename);
        // Move the current position beyond the file name copied.
        // +1 for including the NULL terminator
        iCurBytePos += strlen(fullFilename) +1;
        g_free(fullFilename);

    }
    // File list ends by double NULL (\o\o)
    // Add missing NULL
    ((LPSTR)(pDropFiles))[iCurBytePos+1] = 0;
    GlobalUnlock(hGlobal);
    // send DnD event
    PostMessage(hWnd, WM_DROPFILES, (WPARAM)hGlobal, 0);

    return TRUE;
}

static BOOL CALLBACK
FindGwyddionWindow(HWND hwnd, LPARAM lParam)
{
    if (GetProp(hwnd, GWY_TOOLBOX_WM_ROLE)) {
        *(HWND*)lParam = hwnd;
        return FALSE;
    }

    return TRUE;
}

static GdkWindow*
remote_find_toolbox(GdkDisplay *display,
                    guint32 *xid)
{
    HWND hwnd = 0;

    /* Iterate thru all windows and find window with gwyddion's attribute
       to identify gwyddion app */
    EnumWindows(FindGwyddionWindow, (LPARAM)&hwnd);
    if (hwnd != 0) {
        /* window found */
        gwy_debug("Drop window found, hwnd: %d", hwnd);
        *xid = (guint32)hwnd;
        return gdk_window_foreign_new_for_display(display,
                                                  (GdkNativeWindow)hwnd);
    }
    else {
        *xid = 0;
        return NULL;
    }
}
#endif

#if (REMOTE_BACKEND == REMOTE_X11)
void
gwy_app_remote_setup(G_GNUC_UNUSED GtkWidget *toolbox)
{
}

void
gwy_app_remote_finalize(G_GNUC_UNUSED GtkWidget *toolbox)
{
}

static gboolean
do_remote(GdkDisplay *display,
          GdkWindow *toolbox,
          guint32 xid,
          int argc,
          char **argv)
{

    GdkDragContext *context;
    GdkDragProtocol protocol;
    GtkWidget *source;
    GdkAtom sel_type, sel_id;
    GString *file_list;
    GList *targetlist;
    gchar *cwd;
    gint i;

    xid = gdk_drag_get_protocol_for_display(display, xid, &protocol);
    /* FIXME: Here we may need some platform-dependent protocol check.
     * protocol should be GDK_DRAG_PROTO_XDND on X11, but on win32
     * gdk_drag_get_protocol_for_display returns 0, which means there
     * is no DnD support for target window. */
    if (!xid) {
        g_printerr("Gwyddion window doesn't support DnD.\n");
        return FALSE;
    }

    /* Now we have the toolbox, it seems to support DnD and we have some files
     * to send to it.  So build the list. */
    cwd = g_get_current_dir();
    file_list = g_string_sized_new(32*argc);
    for (i = 0; i < argc; i++) {
        gchar *s, *t;

        if (i)
            g_string_append_c(file_list, '\n');

        if (g_path_is_absolute(argv[i]))
            s = g_filename_to_uri(argv[i], NULL, NULL);
        else {
            t = g_build_filename(cwd, argv[i], NULL);
            s = g_filename_to_uri(t, NULL, NULL);
            g_free(t);
        }
        g_string_append(file_list, s);
        g_free(s);
    }

    /* Don't hang when the toolbox is non-responsive.
     * This may not be necessary in Gwyddion, but it does not hurt either. */
    g_timeout_add(2000, toolbox_timeout, NULL);

    /* Set up an DnD-source. */
    source = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(source, "selection-get",
                     G_CALLBACK(source_selection_get), file_list->str);
    gtk_widget_realize(source);

    /* Specify the id and the content-type of the selection used to
     * pass the URIs to Gwyddion toolbox. */
    sel_id = gdk_atom_intern("XdndSelection", FALSE);
    sel_type = gdk_atom_intern("text/plain", FALSE);
    targetlist = g_list_prepend(NULL, GUINT_TO_POINTER(sel_type));

    /* Assign the selection to our DnD-source. */
    gtk_selection_owner_set(source, sel_id, GDK_CURRENT_TIME);
    gtk_selection_add_target(source, sel_id, sel_type, 0);

    /* Drag_begin/motion/drop. */
    context = gdk_drag_begin(source->window, targetlist);

    gdk_drag_motion(context, toolbox, protocol, 0, 0,
                    GDK_ACTION_COPY, GDK_ACTION_COPY, GDK_CURRENT_TIME);
    gdk_drag_drop(context, GDK_CURRENT_TIME);

    /* Finally enter the mainloop to handle the events. */
    gtk_main();
    gdk_notify_startup_complete();

    return TRUE;
}

static GdkWindow*
remote_find_toolbox(GdkDisplay *display,
                    guint32 *xid)
{
    GdkWindow *root, *result = NULL;
    Display *xdisplay;
    Window xroot, xparent, *xchildren;
    Atom role_xatom;
    guint nchildren;
    gint i;

    root = gdk_screen_get_root_window(gdk_screen_get_default());
    xdisplay = gdk_x11_display_get_xdisplay(display);

    if (!XQueryTree(xdisplay, GDK_WINDOW_XID(root),
                    &xroot, &xparent, &xchildren, &nchildren)
        || !nchildren
        || !xchildren)
        return NULL;

    role_xatom = gdk_x11_get_xatom_by_name_for_display(display,
                                                       "WM_WINDOW_ROLE");

    for (i = nchildren-1; !result && i >= 0; i--) {
        Window xwindow;
        Atom ret_type_xatom;
        gint ret_format;
        gulong bytes_after, nitems;
        guchar *data;

        /*
         * The XmuClientWindow() function finds a window at or below the
         * specified window, that has a WM_STATE property. If such a
         * window is found, it is returned; otherwise the argument window
         * is returned.
         */
        xwindow = XmuClientWindow(xdisplay, xchildren[i]);
        if (XGetWindowProperty(xdisplay, xwindow,
                               role_xatom, 0, 32, FALSE, XA_STRING,
                               &ret_type_xatom, &ret_format,
                               &nitems, &bytes_after, &data) == Success
            && ret_type_xatom) {
            if (gwy_strequal(data, GWY_TOOLBOX_WM_ROLE)) {
                *xid = xwindow;
                result = gdk_window_foreign_new_for_display(display, xwindow);
            }

            XFree(data);
        }
    }

    XFree(xchildren);

    return result;
}
#endif

#if (REMOTE_BACKEND == REMOTE_UNIQUE)
enum {
    REMOTECMD_QUERY = 1,
};

static const GwyEnum commands[] = {
    { "query-window", REMOTECMD_QUERY, },
};

static UniqueApp *uniqueapp = NULL;

static UniqueResponse
message_received(UniqueApp *uniqueapp_,
                 gint command,
                 UniqueMessageData *message_data,
                 G_GNUC_UNUSED guint time_,
                 G_GNUC_UNUSED gpointer user_data)
{
    gchar *filename, **uris;
    guint i, nok = 0;

    g_assert(uniqueapp_ == uniqueapp);
    if (command != UNIQUE_OPEN)
        return UNIQUE_RESPONSE_PASSTHROUGH;

    uris = unique_message_data_get_uris(message_data);
    for (i = 0; uris[i]; i++) {
        filename = g_filename_from_uri(uris[i], NULL, NULL);
        if (filename) {
            if (gwy_file_detect(filename, FALSE, GWY_FILE_OPERATION_LOAD)) {
                if (gwy_app_file_load(NULL, filename, NULL))
                    nok++;
            }
            g_free(filename);
        }
    }
    g_strfreev(uris);

    return nok ? UNIQUE_RESPONSE_OK : UNIQUE_RESPONSE_FAIL;
}

void
gwy_app_remote_setup(GtkWidget *toolbox)
{
    guint i;

    if (!uniqueapp) {
        uniqueapp = unique_app_new("net.gwyddion.Gwyddion", NULL);
        for (i = 0; i < G_N_ELEMENTS(commands); i++)
            unique_app_add_command(uniqueapp,
                                   commands[i].name, commands[i].value);
    }

    /* Only connect signals if run from GUI and we are the first instance.
     * The user can run more instances but these cannot be remotely
     * controlled. */
    if (toolbox) {
        if (unique_app_is_running(uniqueapp)) {
            gwy_object_unref(uniqueapp);
            return;
        }
        g_signal_connect(uniqueapp, "message-received",
                         G_CALLBACK(message_received), NULL);
    }
}

void
gwy_app_remote_finalize(G_GNUC_UNUSED GtkWidget *toolbox)
{
    gwy_object_unref(uniqueapp);
}

static gboolean
do_remote(G_GNUC_UNUSED GdkDisplay *display,
          G_GNUC_UNUSED GdkWindow *toolbox,
          G_GNUC_UNUSED guint32 xid,
          int argc,
          char **argv)
{
    UniqueMessageData *message;
    UniqueResponse response;
    GPtrArray *file_list;
    gchar *cwd;
    gint i;

    /* Build the file list. */
    cwd = g_get_current_dir();
    file_list = g_ptr_array_new();
    for (i = 0; i < argc; i++) {
        gchar *s, *t;

        if (g_path_is_absolute(argv[i]))
            s = g_filename_to_uri(argv[i], NULL, NULL);
        else {
            t = g_build_filename(cwd, argv[i], NULL);
            s = g_filename_to_uri(t, NULL, NULL);
            g_free(t);
        }
        g_ptr_array_add(file_list, s);
    }
    g_ptr_array_add(file_list, NULL);

    message = unique_message_data_new();
    if (unique_message_data_set_uris(message, (gchar**)file_list->pdata)) {
        response = unique_app_send_message(uniqueapp, UNIQUE_OPEN, message);
    }
    else {
        g_warning("unique_message_data_set_uris() failed.");
    }

    unique_message_data_free(message);
    g_ptr_array_foreach(file_list, (GFunc)g_free, NULL);
    g_ptr_array_free(file_list, TRUE);
    gdk_notify_startup_complete();
    gwy_app_remote_finalize(NULL);

    return response == UNIQUE_RESPONSE_OK;
}

static GdkWindow*
remote_find_toolbox(G_GNUC_UNUSED GdkDisplay *display,
                    guint32 *xid)
{
    /* XXX: This all is very silly, must refactor the code. */
    /* XXX: Also, if --remote-new starts the first instance, we create the
     * UniqueApp twice. */
    *xid = 0;
    gwy_app_remote_setup(NULL);

    return unique_app_is_running(uniqueapp) ? (GdkWindow*)uniqueapp : NULL;
}
#endif

#if (REMOTE_BACKEND == REMOTE_NONE)
void
gwy_app_remote_setup(G_GNUC_UNUSED GtkWidget *toolbox)
{
}

void
gwy_app_remote_finalize(G_GNUC_UNUSED GtkWidget *toolbox)
{
}

static gboolean
do_remote(G_GNUC_UNUSED GdkDisplay *display,
          G_GNUC_UNUSED GdkWindow *toolbox,
          G_GNUC_UNUSED guint32 xid,
          G_GNUC_UNUSED int argc,
          G_GNUC_UNUSED char **argv)
{
    /* We should not get here anyway, because remote_find_toolbox() returned
     * NULL. */
    g_printerr("Remote control not implemented for this platform.\n");
    return FALSE;
}

static GdkWindow*
remote_find_toolbox(G_GNUC_UNUSED GdkDisplay *display,
                    G_GNUC_UNUSED guint32 *xid)
{
    g_printerr("Remote control not available.\n");
    return NULL;
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
