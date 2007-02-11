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

#ifdef HAVE_REMOTE_X11
#include <gdk/gdkx.h>
#include <X11/Xmu/WinUtil.h>
#include <X11/Xatom.h>
#endif

#include <libgwyddion/gwymacros.h>
#include "gwyddion.h"

static GdkWindow* remote_find_toolbox      (GdkDisplay *display,
                                            guint32 *xid);

static gboolean
toolbox_timeout(G_GNUC_UNUSED gpointer data)
{
    g_printerr("Could not connect to the toolbox.\n");
    gtk_main_quit();

    return TRUE;
}

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
    GdkDragContext *context;
    GdkDragProtocol protocol;
    GtkWidget *source;
    GdkAtom sel_type, sel_id;
    GString *file_list;
    GList *targetlist;
    guint32 xid = 0;
    gchar *cwd;
    gint i;

    if (type == GWY_APP_REMOTE_NONE)
        return;

    /* No args, nothing to do. Silly, but consistent. */
    if (type == GWY_APP_REMOTE_EXISTING && !argc)
        exit(EXIT_SUCCESS);

    display = gdk_display_get_default();
    toolbox = remote_find_toolbox(display, &xid);
    gwy_debug("Toolbox: %p", toolbox);

    switch (type) {
        case GWY_APP_REMOTE_EXISTING:
        if (!toolbox) {
            g_printerr("No Gwyddion toolbox window found.\n");
            exit(EXIT_FAILURE);
        }
        break;

        case GWY_APP_REMOTE_NEW:
        /* Returning simply continues execution of Gwyddion. */
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

    /* Now we have the toolbox and have some files to send to it. */
    cwd = g_get_current_dir();
    file_list = g_string_new(NULL);
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

    xid = gdk_drag_get_protocol_for_display (display, xid, &protocol);
    if (!xid) {
        g_printerr("Gwyddion window doesn't support DnD.\n");
        exit(EXIT_FAILURE);
    }

    /* This may not be necessary in Gwyddion.  Fixes non-responsive toolbox. */
    g_timeout_add(2000, toolbox_timeout, NULL);

    /* Set up an DND-source. */
    source = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(source, "selection_get",
                     G_CALLBACK(source_selection_get), file_list->str);
    gtk_widget_realize (source);

    /* Specify the id and the content-type of the selection used to
     * pass the URIs to Gwyddion toolbox. */
    /* FIXME: I doubt this works on Win32 target, may need to factor out
     * platform-dependent code also here. */
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

    exit(EXIT_SUCCESS);
}

#ifdef HAVE_REMOTE_X11
#define GWY_REMOTE_IMPLEMENTED 1
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

#ifndef GWY_REMOTE_IMPLEMENTED
static GdkWindow*
remote_find_toolbox(G_GNUC_UNUSED GdkDisplay *display,
                    G_GNUC_UNUSED guint32 *xid)
{
    g_printerr("Remote control not available.\n");
    return NULL;
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

