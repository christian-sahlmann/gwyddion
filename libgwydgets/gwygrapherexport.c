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
#include "gwygrapher.h"

void
gwy_grapher_export_ascii(GwyGrapher *grapher, const char *filename)
{
    /*
    FILE *fw;
    gint i, j, maxj = 0;
    GwyGrapherAreaCurve *pcurve;

    if (!(fw = fopen(filename, "w"))) {
        g_warning("Could not open file for grapher export.");
        return;
    }


    for (i = 0; i < grapher->n_of_curves; i++) {
        pcurve = g_ptr_array_index(grapher->area->curves, i);
        if (maxj < pcurve->data.N)
            maxj = pcurve->data.N;
        fprintf(fw, "x%d ", i + 1);
        if (grapher->has_x_unit)
            fprintf(fw, "[%s]       ", grapher->x_unit);
        fprintf(fw, "y%d ", i + 1);
        if (grapher->has_y_unit)
            fprintf(fw, "[%s]       ", grapher->y_unit);

    }
    fprintf(fw, "\n");

    for (j = 0; j < maxj; j++) {
        for (i = 0; i < grapher->n_of_curves; i++) {
            pcurve = g_ptr_array_index(grapher->area->curves, i);
            if (j < pcurve->data.N) {
                fprintf(fw, "%e  %e ", pcurve->data.xvals[j],
                        pcurve->data.yvals[j]);
            }
            else
                fprintf(fw, "                           ");
        }
        fprintf(fw, "\n");
    }
    fclose(fw);
    */
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
