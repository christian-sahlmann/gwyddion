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

#include "config.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

static gboolean module_register(const gchar *name);
static void     export         (GwyGraph *graph);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Export graph into bitmap"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_graph_func_register("graph_export_bitmap",
                            (GwyGraphFunc)&export,
                            N_("/Export _bitmap"),
                            GWY_STOCK_GRAPH_BITMAP,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Export graph to a raster image"));

    return TRUE;
}

static void
export(GwyGraph *graph)
{
    GtkDialog *filedialog;
    gchar *filename;
    
    filedialog = GTK_DIALOG(gtk_file_chooser_dialog_new ("Export to bitmap",
                                                          NULL,
                                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                          NULL));
    if (gtk_dialog_run (GTK_DIALOG (filedialog)) == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filedialog));
        gwy_graph_export_pixmap(graph, filename,
                                         TRUE, TRUE, TRUE);
    }
    gtk_widget_destroy(GTK_WIDGET(filedialog));
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
