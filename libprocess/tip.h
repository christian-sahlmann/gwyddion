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

#ifndef __GWY_PROCESS_TIP_H__
#define __GWY_PROCESS_TIP_H__

G_BEGIN_DECLS

/* XXX: never used in libprocess itself, there should be probably some header
 * file with common enums... */
typedef enum {
    GWY_TIP_CONTACT        = 0, 
    GWY_TIP_NONCONTACT     = 1,
    GWY_TIP_SHARPENED      = 2,
    GWY_TIP_DELTA          = 3
} GwyTipType;


G_END_DECLS

typedef void (*GwyTipModelFunc)(GwyDataField *tip,			
				gdouble height,
				gdouble radius,
				gdouble *params);

typedef void (*GwyTipGuessFunc)(GwyDataField *data,
				gdouble height,
				gdouble radius,
				gdouble *params,
				gint *xres,
				gint *yres);

typedef struct _GwyTipModelPreset GwyTipModelPreset;

struct _GwyTipModelPreset {
    const gchar *tip_name;
    const gchar *group_name;
    GwyTipModelFunc func;
    GwyTipGuessFunc guess;
    gint nparams;    
};


gint             gwy_tip_model_get_npresets(void);

G_CONST_RETURN
GwyTipModelPreset* gwy_tip_model_get_preset(gint preset_id);

G_CONST_RETURN
GwyTipModelPreset* gwy_tip_model_get_preset_by_name(const gchar *name);

gint            gwy_tip_model_get_preset_id     (const GwyTipModelPreset* preset);

G_CONST_RETURN
gchar*          gwy_tip_model_get_preset_tip_name   (const GwyTipModelPreset* preset);

G_CONST_RETURN
gchar*          gwy_tip_model_get_preset_group_name   (const GwyTipModelPreset* preset);

gint            gwy_tip_model_get_preset_nparams(const GwyTipModelPreset* preset);

	      
	  

#endif /*__GWY_PROCESS_GRAINS__*/

