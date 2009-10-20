/*
 *  @(#) $Id$
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
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include "gwyddion.h"

void
gwy_remote_do(GwyAppRemoteType type,
              int argc,
              char **argv)
{
    GwyRemote *remote;
    gboolean ok;

    if (type == GWY_APP_REMOTE_NONE)
        return;

    /* No args, nothing to do. Silly, but consistent. */
    if (type == GWY_APP_REMOTE_EXISTING && !argc)
        exit(EXIT_SUCCESS);

    remote = gwy_remote_get();
    switch (type) {
        case GWY_APP_REMOTE_EXISTING:
        if (!remote) {
            g_printerr("No running Gwyddion instance found.\n");
            exit(EXIT_FAILURE);
        }
        break;

        case GWY_APP_REMOTE_NEW:
        /* Returning simply continues the execution of Gwyddion with the
         * file arguments we've got. */
        if (!remote)
            return;
        if (!argc)
            exit(EXIT_SUCCESS);
        break;

        case GWY_APP_REMOTE_QUERY:
        if (remote) {
            gwy_remote_print(remote);
            gwy_remote_free(remote);
            exit(EXIT_SUCCESS);
        }
        exit(EXIT_FAILURE);
        break;

        default:
        g_return_if_reached();
        break;
    }

    /* Call the appropriate remote handler for X11 and win32 */
    ok = gwy_remote_open_files(remote, argc, argv);
    gwy_remote_free(remote);
    exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* Null implementation when no remote control is available. */
#if (REMOTE_BACKEND == REMOTE_NONE)
void
gwy_remote_setup(G_GNUC_UNUSED GtkWidget *toolbox)
{
}

void
gwy_remote_finalize(G_GNUC_UNUSED GtkWidget *toolbox)
{
}

GwyRemote*
gwy_remote_get(void)
{
    g_printerr("Remote control not available.\n");
    return NULL;
}

void
gwy_remote_free(G_GNUC_UNUSED GwyRemote *remote)
{
}

gboolean
gwy_remote_open_files(G_GNUC_UNUSED GwyRemote *remote,
                      G_GNUC_UNUSED int argc,
                      G_GNUC_UNUSED char **argv)
{
    /* We should not get here anyway, because remote_find_toolbox() returned
     * NULL. */
    g_printerr("Remote control not implemented for this platform.\n");
    return FALSE;
}

void
gwy_remote_print(G_GNUC_UNUSED GwyRemote *remote)
{
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
