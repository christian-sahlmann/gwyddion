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

#ifndef __GWY_PROCESS_CDLINE_H__
#define __GWY_PROCESS_CDLINE_H__

#include <glib.h>
#include <libprocess/dataline.h>

G_BEGIN_DECLS

/* XXX: remove, or remove presets.  Both don't make sense */
typedef enum {
      GWY_CD_LINE_ULINEHEIGHT  = 0,
      GWY_CD_LINE_ULINEWIDTH   = 1,
      GWY_CD_LINE_DLINEHEIGHT  = 2,
      GWY_CD_LINE_DLINEWIDTH   = 3,
      GWY_CD_LINE_USTEPHEIGHT  = 4,
      GWY_CD_LINE_DSTEPHEIGHT  = 5,
      GWY_CD_LINE_RECTSIGNAL   = 6,
      GWY_CD_LINE_SAWSIGNAL    = 7,
      GWY_CD_LINE_SINSIGNAL    = 8,
      GWY_CD_LINE_PARTICLE     = 9,
      GWY_CD_LINE_HOLE         = 10
} GwyCDLineType;

typedef gdouble (*GwyCDLineFitFunc)(gdouble x,
                                    gint n_param,
                                    gdouble *param,
                                    gpointer user_data,
                                    gboolean *fres);

typedef void (*GwyCDLineCDFunc)(const gdouble *x,
                                const gdouble *y,
                                gint n_dat,
                                gdouble *param,
                                gdouble *err,
                                gpointer user_data,
                                gboolean *fres);

typedef struct _GwyCDLinePreset GwyCDLinePreset;
typedef struct _GwyCDLineParam GwyCDLineParam;

struct _GwyCDLinePreset {
    const gchar *function_name;
    const gchar *group_name;
    const gchar *function_formula;
    GwyCDLineFitFunc function;
    GwyCDLineCDFunc function_fit;
    gint nparams;
    const GwyCDLineParam *param;
    gpointer _reserved1;
};


gint            gwy_cdline_get_npresets      (void)
                                              G_GNUC_CONST;
G_CONST_RETURN
GwyCDLinePreset* gwy_cdline_get_preset        (gint preset_id)
                                               G_GNUC_CONST;
G_CONST_RETURN
GwyCDLinePreset* gwy_cdline_get_preset_by_name(const gchar *name);
gint            gwy_cdline_get_preset_id     (const GwyCDLinePreset* preset);
G_CONST_RETURN
gchar*          gwy_cdline_get_preset_name   (const GwyCDLinePreset* preset);
G_CONST_RETURN
gchar*          gwy_cdline_get_preset_formula(const GwyCDLinePreset* preset);
G_CONST_RETURN
gchar*          gwy_cdline_get_preset_param_name(const GwyCDLinePreset* preset,
                                                     gint param);
gdouble         gwy_cdline_get_preset_param_default(const GwyCDLinePreset* preset,
                                                    gint param);
gint            gwy_cdline_get_preset_nparams(const GwyCDLinePreset* preset);
void            gwy_cdline_fit_preset        (const GwyCDLinePreset* preset,
                                              gint n_dat,
                                              const gdouble *x,
                                              const gdouble *y,
                                              gint n_param,
                                              gdouble *param,
                                              gdouble *err,
                                              const gboolean *fixed_param,
                                              gpointer user_data);

G_END_DECLS

#endif /*__GWY_PROCESS_FRACTALS__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
