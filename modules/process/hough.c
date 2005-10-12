/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/hough.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/gwyapp.h>

#define HOUGH_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)


static gboolean    module_register            (const gchar *name);
static gboolean    hough                   (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates mask of hough."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo hough_func_info = {
        "hough",
        N_("/_Integral Transforms/_Hough"),
        (GwyProcessFunc)&hough,
        HOUGH_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &hough_func_info);

    return TRUE;
}

static gboolean
hough(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *edgefield, *result;
    GwyContainer *resdata;
    GtkWidget *data_window;
    
    gdouble thresh;

    g_assert(run & HOUGH_RUN_MODES);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    resdata = gwy_container_duplicate_by_prefix(data,
				               "/0/data",
					       "/0/base/palette",
					       NULL);
    
    result = GWY_DATA_FIELD(gwy_container_get_object_by_name(resdata, "/0/data"));
    gwy_data_field_resample(result, 
			    sqrt(gwy_data_field_get_xres(result)*gwy_data_field_get_xres(result)
				 +gwy_data_field_get_yres(result)*gwy_data_field_get_yres(result)), 
			    360,
			    GWY_INTERPOLATION_NONE);
    
    edgefield = gwy_data_field_duplicate(dfield);
    
    gwy_data_field_filter_canny(edgefield, 0.1);
    gwy_data_field_hough_line(edgefield,
			      NULL,
			      NULL,
			      result,
			      1);
    
    data_window = gwy_app_data_window_create(resdata);

    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                                _("Hough transform"));
	    

    return TRUE;
}




