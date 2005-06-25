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
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <stdio.h>

#include <libgwyddion/gwymacros.h>
#include "gwygraph.h"

void       gwy_graph_export_pixmap(GwyGraph *grapher, const gchar *filename,
                                     gboolean export_title, gboolean export_axis,
                                     gboolean export_labels)
{
    /*create pixmap*/
    
    /*plot area*/
    
    /*plot axis*/

    /*plot label*/

    /*save pixmap*/
    
}

void       gwy_graph_export_postscript(GwyGraph *grapher, const gchar *filename,
                                         gboolean export_title, gboolean export_axis,
                                         gboolean export_labels)
{
    /*create stream*/

    /*write header*/

    /*write label*/
    
    /*write axis*/

    /*write area*/

    /*save stream*/

}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
