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

#include <string.h>
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "filters.h"
#include "datafield.h"
#include "tip.h"

static void
contact_guess (GwyDataField *data, gdouble height, gdouble radius, gdouble *params,
               gdouble *xres, gdouble *yres)
{
    *xres = 200;
    *yres = 200;

}
static void
noncontact_guess (GwyDataField *data, gdouble height, gdouble radius, gdouble *params,
               gdouble *xres, gdouble *yres)
{
    *xres = 200;
    *yres = 200;
}
static void
sharpened_guess (GwyDataField *data, gdouble height, gdouble radius, gdouble *params,
               gdouble *xres, gdouble *yres)
{
    *xres = 200;
    *yres = 200;

}
static void
delta_guess (GwyDataField *data, gdouble height, gdouble radius, gdouble *params,
               gdouble *xres, gdouble *yres)
{
    *xres = 200;
    *yres = 200;

}

static void
contact (GwyDataField *tip, gdouble height, gdouble radius, gdouble *params)
{
    
}

static void
noncontact (GwyDataField *tip, gdouble height, gdouble radius, gdouble *params)
{
    
}

static void
sharpened (GwyDataField *tip, gdouble height, gdouble radius, gdouble *params)
{
    
}

static void
delta (GwyDataField *tip, gdouble height, gdouble radius, gdouble *params)
{
    
}

static const GwyTipModelPreset tip_presets[] = {
    {
        "Contact",
        "Pyramidal",
        &contact,
        &contact_guess,
        0
    },
    {
        "Noncontact",
        "Pyramidal",
        &noncontact,
        &noncontact_guess,
        0
    },
    {
        "Sharpened",
        "Pyramidal",
        &sharpened,
        &sharpened_guess,
        0
    },
     {
        "Delta function",
        "Analytical",
        &delta,
        &delta_guess,
        0
    },
};


gint
gwy_tip_model_get_npresets(void)
{
    return (gint)G_N_ELEMENTS(tip_presets);
}

G_CONST_RETURN GwyTipModelPreset*
gwy_tip_model_get_preset(gint preset_id)
{
    g_return_val_if_fail(preset_id >= 0
                         && preset_id < (gint)G_N_ELEMENTS(tip_presets),
                         NULL);

    return tip_presets + preset_id;
}
        
G_CONST_RETURN GwyTipModelPreset*
gwy_tip_model_get_preset_by_name(const gchar *name)
{
    gsize i;

    for (i = 0; i < G_N_ELEMENTS(tip_presets); i++) {
        if (strcmp(name, tip_presets[i].tip_name) == 0)
            return tip_presets + i;
    }
    return NULL;
}

gint
gwy_tip_model_get_preset_id(const GwyTipModelPreset* preset)
{
    return preset - tip_presets;
}

G_CONST_RETURN gchar*
gwy_tip_model_get_preset_tip_name(const GwyTipModelPreset* preset)
{
    return preset->tip_name;
}

G_CONST_RETURN gchar*
gwy_tip_model_get_preset_group_name(const GwyTipModelPreset* preset)
{
    return preset->group_name;
}

gint
gwy_tip_model_get_preset_nparams(const GwyTipModelPreset* preset)
{
    return preset->nparams;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
