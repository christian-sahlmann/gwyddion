/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include <math.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <app/settings.h>
#include <app/app.h>


static gboolean    module_register            (const gchar *name);
static gboolean    ascii                      (GwyGraph *graph);
void store_filename (GtkWidget *widget, gpointer user_data);
void create_file_selection (void);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Export graph curves to ASCII file"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyGraphFuncInfo read_func_info = {
        "graph_ascii_export",
        N_("/_Export ASCII..."),
        (GwyGraphFunc)&ascii,
    };

    gwy_graph_func_register(name, &read_func_info);

    return TRUE;
}

static gboolean
ascii(GwyGraph *graph)
{
    GtkWidget *dialog;
    const gchar *selected_filename = NULL;
    gint response;

    if (!graph) {
        return TRUE;
    }

    dialog = gtk_file_selection_new(_("Export Graph to File"));
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog),
                                    gwy_app_get_current_directory());
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK) {
        selected_filename
            = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
        gtk_widget_destroy(dialog);
    }
    else {
        gtk_widget_destroy(dialog);
        return TRUE;
    }

    if (selected_filename != NULL)
        gwy_graph_export_ascii(graph, selected_filename);

    return TRUE;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
