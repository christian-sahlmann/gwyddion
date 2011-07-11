/*
 *  @(#) $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*< private_header >*/

#ifndef __GWY_PROCESS_MONTE_CARLO_UNC_H__
#define __GWY_PROCESS_MONTE_CARLO_UNC_H__

#include <libprocess/datafield.h>
#include "gwyprocessinternal.h"

G_BEGIN_DECLS

typedef struct {
    /* XXX: Replace this with actual uncertainty data. */
    gdouble sigma;
} GwyFieldUncertainties;

G_GNUC_INTERNAL
void _gwy_data_field_unc_scalar(GwyDataField *field,
                                GwyDataField *mask,
                                GwyMaskingType mode,
                                const GwyFieldPart *fpart,
                                gpointer params,
                                gdouble *results,
                                guint nresults,
                                GwyDataFieldScalarFunc func,
                                const GwyFieldUncertainties *unc);

G_END_DECLS

#endif /* __GWY_PROCESS_MONTE_CARLO_UNC_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
