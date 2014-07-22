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

#ifndef __GWY_APP_HELP_H__
#define __GWY_APP_HELP_H__

#include <gtk/gtk.h>
#include <app/gwytool.h>

G_BEGIN_DECLS

typedef enum {
    GWY_HELP_DEFAULT = 0,
    GWY_HELP_NO_BUTTON = 1 << 1,
} GwyHelpFlags;

void     gwy_help_add_to_proc_dialog  (GtkDialog *dialog,
                                       GwyHelpFlags flags);
void     gwy_help_add_to_graph_dialog (GtkDialog *dialog,
                                       GwyHelpFlags flags);
void     gwy_help_add_to_volume_dialog(GtkDialog *dialog,
                                       GwyHelpFlags flags);
void     gwy_help_add_to_file_dialog  (GtkDialog *dialog,
                                       GwyHelpFlags flags);
void     gwy_help_add_to_tool_dialog  (GtkDialog *dialog,
                                       GwyTool *tool,
                                       GwyHelpFlags flags);
void     gwy_help_add_to_window       (GtkWindow *window,
                                       const gchar *filename,
                                       const gchar *fragment,
                                       GwyHelpFlags flags);
void     gwy_help_add_to_window_uri   (GtkWindow *window,
                                       const gchar *uri,
                                       GwyHelpFlags flags);
void     gwy_help_show                (const gchar *filename,
                                       const gchar *fragment);
gboolean gwy_help_is_available        (void);

G_END_DECLS

#endif /* __GWY_APP_HELP_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
