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

#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <app/file.h>
#include <app/gwymoduleutils.h>

typedef struct {
    const gchar *data;
    gssize len;
} GwySaveAuxiliaryData;

static gchar*
gwy_save_auxiliary_data_create(gpointer user_data,
                               gssize *data_len)
{
    GwySaveAuxiliaryData *savedata = (GwySaveAuxiliaryData*)user_data;

    *data_len = savedata->len;

    return (gchar*)savedata->data;
}

gboolean
gwy_save_auxiliary_data(const gchar *title,
                        GtkWindow *parent,
                        gssize data_len,
                        const gchar *data)
{
    GwySaveAuxiliaryData savedata;

    g_return_val_if_fail(data, FALSE);
    savedata.data = data;
    savedata.len = data_len;

    return gwy_save_auxiliary_with_callback(title, parent,
                                            &gwy_save_auxiliary_data_create,
                                            NULL,
                                            &savedata);
}

gboolean
gwy_save_auxiliary_with_callback(const gchar *title,
                                 GtkWindow *parent,
                                 GwySaveAuxiliaryCreate create,
                                 GwySaveAuxiliaryDestroy destroy,
                                 gpointer user_data)
{
    gchar *data;
    gchar *filename_sys, *filename_utf8;
    gssize data_len;
    gint myerrno;
    GtkWidget *dialog, *chooser;
    gint response;
    FILE *fh;

    g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), FALSE);
    g_return_val_if_fail(create, FALSE);

    chooser = gtk_file_chooser_dialog_new(title, parent,
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_OK);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser),
                                        gwy_app_get_current_directory());
    response = gtk_dialog_run(GTK_DIALOG(chooser));
    filename_sys = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));

    if (!gwy_app_file_confirm_overwrite(chooser))
        response = GTK_RESPONSE_CANCEL;

    gtk_widget_destroy(chooser);

    if (!filename_sys || response != GTK_RESPONSE_OK) {
        g_free(filename_sys);
        return FALSE;
    }

    data_len = -1;
    data = create(user_data, &data_len);
    g_return_val_if_fail(data, FALSE);

    if (data_len <= 0) {
        if ((fh = g_fopen(filename_sys, "w"))) {
            if (fputs(data, fh) == EOF) {
                myerrno = errno;
                /* This is just best-effort clean-up */
                fclose(fh);
                g_unlink(filename_sys);
                fh = NULL;
            }
        }
        else
            myerrno = errno;
    }
    else {
        if ((fh = g_fopen(filename_sys, "wb"))) {
            if (fwrite(data, data_len, 1, fh) != 1) {
                myerrno = errno;
                /* This is just best-effort clean-up */
                fclose(fh);
                g_unlink(filename_sys);
                fh = NULL;
            }
        }
        else
            myerrno = errno;
    }

    if (destroy)
        destroy(data, user_data);

    if (fh) {
        /* Everything went all right. */
        fclose(fh);
        g_free(filename_sys);
        return TRUE;
    }

    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    dialog = gtk_message_dialog_new(parent, 0, GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    _("Saving of `%s' failed"),
                                    filename_utf8);
    g_free(filename_sys);
    g_free(filename_utf8);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             _("Cannot write to file: %s."),
                                             g_strerror(myerrno));
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return FALSE;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymoduleutils
 * @title: module utils
 * @short_description: Module utility functions
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
