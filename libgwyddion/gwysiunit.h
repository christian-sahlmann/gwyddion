/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#ifndef __GWY_SI_UNIT_H__
#define __GWY_SI_UNIT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct{
    char *unit;
} GwySIUnit;

void gwy_si_unit_new(char *unit_string);
void gwy_si_unit_free();

void gwy_si_unit_set_unit_string(GwySIUnit *siunit, char *unit_string);
void gwy_si_unit_get_unit_string(GwySIUnit *siunit, char *unit_string);

void gwy_si_unit_get_prefix(GwySIUnit *siunit, double value, gint precision, char *prefix, double *power);
void gwy_si_unit_get_prefixed(GwySIUnit *siunit, double value, gint precision, char *prefix, double *power);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_SI_UNIT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
