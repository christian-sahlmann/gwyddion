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
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "filters.h"
#include "datafield.h"
#include "tip.h"
#include <math.h>

static void
contact_guess (GwyDataField *data, gdouble height, gdouble radius, gdouble *params,
               gint *xres, gint *yres)
{
    *xres = 200;
    *yres = 200;;

}
static void
noncontact_guess (GwyDataField *data, gdouble height, gdouble radius, gdouble *params,
               gint *xres, gint *yres)
{
    *xres = 200;
    *yres = 200;
}
static void
sharpened_guess (GwyDataField *data, gdouble height, gdouble radius, gdouble *params,
               gint *xres, gint *yres)
{
    *xres = 200;
    *yres = 200;

}
static void
delta_guess (GwyDataField *data, gdouble height, gdouble radius, gdouble *params,
               gint *xres, gint *yres)
{
    *xres = 200;
    *yres = 200;

}

static void
create_pyramide(GwyDataField *tip, gdouble height, gint n)
{
    gint col, row;
    gdouble rcol, rrow;
    gdouble scol, srow;
    gdouble r, phi, phic;
    gdouble vm, radius;

    radius = sqrt(2)*tip->xres;

    scol = tip->xres/2;
    srow = tip->yres/2;

    printf("height=%g\n", height);
    for (col=0; col<tip->xres; col++)
    {
        for (row=0; row<tip->yres; row++)
        {
            rrow = row - srow;
            rcol = col - scol;
            phi = atan2(rrow, rcol) + G_PI;
            phic = floor(phi/(2*G_PI/n))*2*G_PI/n + G_PI/n;
            vm = rcol*cos(phic) + rrow*sin(phic);
            tip->data[col + tip->xres*row] = height*(1 + vm/(radius*cos(G_PI/n)));
        }
    }
}

static void
contact (GwyDataField *tip, gdouble height, gdouble radius, gdouble *params)
{
    create_pyramide(tip, height, 4);
    
}

static void
noncontact (GwyDataField *tip, gdouble height, gdouble radius, gdouble *params)
{
    create_pyramide(tip, height, 3);
}

static void
sharpened (GwyDataField *tip, gdouble height, gdouble radius, gdouble *params)
{
    tip->data[tip->xres*tip->yres/2] = 1;
}

static void
delta (GwyDataField *tip, gdouble height, gdouble radius, gdouble *params)
{
    gwy_data_field_fill(tip, 0);
    tip->data[tip->xres*tip->yres/2] = height;
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


gdouble **
datafield_to_field(GwyDataField *datafield, gboolean maxzero)
{
    gdouble **ret;
    gint col, row;
    gdouble max;
    
    if (maxzero) max = gwy_data_field_get_max(datafield);
    else max = 0;
    
    ret = allocmatrix(datafield->xres, datafield->yres);
    for (col=0; col<datafield->xres; col++)
    {
        for (row=0; row<datafield->yres; row++)
        {
            ret[col][row] = datafield->data[col + datafield->xres*row] - max;
        }
    }
    return ret;
}

GwyDataField*
field_to_datafield(gdouble **field, GwyDataField *ret)
{
    gint col, row;
    for (col=0; col<ret->xres; col++)
    {
        for (row=0; row<ret->yres; row++)
        {
            ret->data[col + ret->xres*row] = field[col][row];
        }
    }    
    return ret;
}

GwyDataField*   
gwy_tip_dilation(GwyDataField *tip, GwyDataField *surface, GwyDataField *result)
{
    gdouble **ftip;
    gdouble **fsurface;
    gdouble **fresult;
    gint k;
   
    ftip = datafield_to_field(tip, TRUE);    
    fsurface = datafield_to_field(surface, FALSE);

    fresult = idilation(fsurface, surface->yres, surface->xres,
                        ftip, tip->yres, tip->xres, tip->yres/2, tip->xres/2);
    
    result = field_to_datafield(fresult, result);

    freematrix(ftip, tip->xres);
    freematrix(fsurface, surface->xres);
    freematrix(fresult, result->xres);
    return result;
}

GwyDataField*
gwy_tip_erosion(GwyDataField *tip, GwyDataField *surface, GwyDataField *result)
{
    gdouble **ftip;
    gdouble **fsurface;
    gdouble **fresult;

   
    ftip = datafield_to_field(tip, TRUE);
    fsurface = datafield_to_field(surface, FALSE);

    fresult = ierosion(fsurface, surface->yres, surface->xres,
                        ftip, tip->yres, tip->xres, tip->yres/2, tip->xres/2);
    
    result = field_to_datafield(fresult, result);

    freematrix(ftip, tip->xres);
    freematrix(fsurface, surface->xres);
    freematrix(fresult, result->xres);
    return result;
    
}

GwyDataField*
gwy_tip_cmap(GwyDataField *tip, GwyDataField *surface, GwyDataField *result)
{
    gdouble **ftip;
    gdouble **fsurface;
    gdouble **rsurface;
    gdouble **fresult;

   
    ftip = datafield_to_field(tip, TRUE);
    fsurface = datafield_to_field(surface, FALSE);
    
    rsurface = ierosion(fsurface, surface->yres, surface->xres,
                        ftip, tip->yres, tip->xres, tip->yres/2, tip->xres/2);

    fresult = icmap(fsurface, surface->yres, surface->xres,
                        ftip, tip->yres, tip->xres, rsurface, tip->yres/2, tip->xres/2);
    
    result = field_to_datafield(fresult, result);

    freematrix(ftip, tip->xres);
    freematrix(fsurface, surface->xres);
    freematrix(rsurface, surface->xres);
    freematrix(fresult, result->xres);
    return result;
    
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
