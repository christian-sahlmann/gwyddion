/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Jan Horak.
 *  E-mail: yeti@gwyddion.net, xhorak@gmail.com.
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
#include "gwyddion.h"
#include <libgwyddion/gwymacros.h>
#if (REMOTE_BACKEND == REMOTE_WIN32)
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>
#include <shlobj.h>

struct _GwyRemote {
    GdkNativeWindow winid;
};

void
gwy_remote_setup(GtkWidget *toolbox)
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
gwy_remote_finalize(GtkWidget *toolbox)
{
    HWND hwnd = 0;

    // Retrieve hWnd
    hwnd = GDK_WINDOW_HWND(gtk_widget_get_root_window(toolbox));
    // Remove properties
    RemoveProp(hwnd, GWY_TOOLBOX_WM_ROLE);
    gwy_debug("RemoveProp to hWnd %d\n", hwnd);
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

GwyRemote*
gwy_remote_get(void)
{
    GwyRemote *remote;
    HWND hwnd = 0;

    remote = g_new0(GwyRemote, 1);
    /* Iterate thru all windows and find window with gwyddion's attribute
       to identify gwyddion app */
    EnumWindows(FindGwyddionWindow, (LPARAM)&hwnd);
    if (hwnd != 0) {
        /* window found */
        gwy_debug("Drop window found, hwnd: %d", hwnd);
        remote->winid = (GdkNativeWindow)hwnd;
        return remote;
    }
    else {
        gwy_remote_free(remote);
        return NULL;
    }
}

void
gwy_remote_free(GwyRemote *remote)
{
    g_free(remote);
}

/* Send WM_DROPFILES message to target window
   Return -1 when window could not be found or memory cannot be allocated
   for DnD operation */
gboolean
gwy_remote_open_files(GwyRemote *remote,
                      int argc,
                      char **argv)
{
    int iCurBytePos = sizeof(DROPFILES);
    LPDROPFILES pDropFiles;
    HGLOBAL hGlobal;
    gchar *fullFilename, *cwd;
    int i;

    if (!remote)
        return FALSE;

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
    PostMessage((HWND)remote->winid, WM_DROPFILES, (WPARAM)hGlobal, 0);

    return TRUE;
}

void
gwy_remote_print(GwyRemote *remote)
{
    if (remote)
        g_print("%08x\n", (guint32)remote->winid);
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
