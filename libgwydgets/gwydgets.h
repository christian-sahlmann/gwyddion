/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_GWYDGETS_H__
#define __GWY_GWYDGETS_H__

#include <libgwydgets/gwyaxis.h>
#include <libgwydgets/gwyaxisdialog.h>
#include <libgwydgets/gwycoloraxis.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydataviewlayer.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwygradsphere.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygrapharea.h>
#include <libgwydgets/gwygraphcorner.h>
#include <libgwydgets/gwygraphlabel.h>
#include <libgwydgets/gwyhruler.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwypixmaplayer.h>
#include <libgwydgets/gwyruler.h>
#include <libgwydgets/gwyscitext.h>
#include <libgwydgets/gwyspherecoords.h>
#include <libgwydgets/gwystatusbar.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwytoolbox.h>
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwydgets/gwyvectorshade.h>
#include <libgwydgets/gwyvruler.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GtkWidget* gwy_table_attach_spinbutton    (GtkWidget *table,
                                           gint row,
                                           const gchar *name,
                                           const gchar *units,
                                           GtkObject *adj);
void       gwy_table_attach_row           (GtkWidget *table,
                                           gint row,
                                           const gchar *name,
                                           const gchar *units,
                                           GtkWidget *middle_widget);

gboolean   gwy_dialog_prevent_delete_cb   (void);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_GWYDGETS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
