/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwymodule/gwymodule-graph.h>
#include <app/gwyapp.h>

static gboolean module_register(void);
static void     export         (GwyGraph *graph);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Export graph in postscript"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_graph_func_register("graph_export_vector",
                            (GwyGraphFunc)&export,
                            N_("/Export _PostScript"),
                            NULL,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Export graph to a vector image"));

    return TRUE;
}

static void
export(GwyGraph *graph)
{
    GtkWidget *dialog;
    gchar *filename;
    GString *str;
    FILE *fw;

    dialog = gtk_file_chooser_dialog_new ("Export to postscript",
                                          NULL,
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                          NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        gwy_app_get_current_directory());
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        if (gwy_app_file_confirm_overwrite(GTK_WIDGET(dialog))) {
            filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            str = gwy_graph_export_postscript(graph, TRUE, TRUE, TRUE, NULL);

            /* FIXME: Must check success */
            fw = g_fopen(filename, "w");
            fwrite(str->str, str->len, 1, fw);
            fclose(fw);
            g_string_free(str, TRUE);
            g_free(filename);
        }
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
