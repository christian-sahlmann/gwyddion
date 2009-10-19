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
#include "gwyddion.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyenum.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/file.h>
#if (REMOTE_BACKEND == REMOTE_UNIQUE)
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <unique/unique.h>

enum {
    REMOTECMD_QUERY = 1,
};

struct _GwyRemote {
    UniqueApp *uniqueapp;
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
gwy_remote_setup(GtkWidget *toolbox)
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
            gwy_remote_finalize(NULL);
            return;
        }
        g_signal_connect(uniqueapp, "message-received",
                         G_CALLBACK(message_received), NULL);
    }
}

void
gwy_remote_finalize(G_GNUC_UNUSED GtkWidget *toolbox)
{
    gwy_object_unref(uniqueapp);
}

GwyRemote*
gwy_remote_get(void)
{
    GwyRemote *remote;

    gwy_remote_setup(NULL);
    if (unique_app_is_running(uniqueapp)) {
        remote = g_new0(GwyRemote, 1);
        remote->uniqueapp = uniqueapp;
        return remote;
    }
    else
        return NULL;
}

void
gwy_remote_free(GwyRemote *remote)
{
    g_free(remote);
}

gboolean
gwy_remote_open_files(GwyRemote *remote,
                      int argc,
                      char **argv)
{
    UniqueMessageData *message;
    UniqueResponse response = UNIQUE_RESPONSE_INVALID;
    GPtrArray *file_list;
    gchar *cwd;
    gint i;

    if (!remote)
        return FALSE;

    g_return_val_if_fail(remote->uniqueapp == uniqueapp, FALSE);

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
    gwy_remote_finalize(NULL);

    return response == UNIQUE_RESPONSE_OK;
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
