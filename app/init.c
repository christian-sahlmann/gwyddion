/* @(#) $Id$ */

#include "config.h"

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <libprocess/libgwyprocess.h>

static GType optimization_fooler = 0;

/**
 * gwy_type_init:
 *
 * Initializes all Gwyddion data types, i.e. types that may appear in
 * serialized data. GObject has to know about them when g_type_from_name()
 * is called.
 **/
void
gwy_type_init(void)
{
    optimization_fooler += gwy_sphere_coords_get_type();
    optimization_fooler += gwy_data_field_get_type();
    optimization_fooler += gwy_data_line_get_type();
    optimization_fooler += gwy_palette_get_type();
    optimization_fooler += gwy_palette_def_get_type();
    optimization_fooler += gwy_container_get_type();

    g_set_application_name(_(PACKAGE_NAME));
    gwy_palette_def_setup_presets();
    gwy_stock_register_stock_items();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
