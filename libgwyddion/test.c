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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "gwyserializable.h"
#include "gwycontainer.h"
#include "gwyentities.h"
#include "gwytestser.h"
#include "gwyutils.h"
#include "gwymath.h"
#include "gwysiunit.h"

int
main(int argc, char * argv[])
{
    GwySIUnit *si;
    GwySIValueFormat *vformat, *dformat;
    gchar prefix[20];
    gdouble div;
    gdouble value;

    g_type_init();
    
    si = gwy_si_unit_new("m");
    
    value = atof(argv[1]);
   
    vformat = gwy_si_unit_get_format_with_digits(si, value, 0, NULL);

    printf("%.*f %s\n", vformat->precision, value/vformat->magnitude, vformat->units);

    vformat = gwy_si_unit_get_format_with_resolution(si, value, 0.2, vformat);

    printf("%.*f %s\n", vformat->precision, value/vformat->magnitude, vformat->units);
  
    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
