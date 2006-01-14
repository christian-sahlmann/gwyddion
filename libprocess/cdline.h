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
#include <libgwyddion/gwyresource.h>


G_BEGIN_DECLS

#define GWY_TYPE_CDLINE             (gwy_cdline_get_type())
#define GWY_CDLINE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CDLINE, GwyCDLine))
#define GWY_CDLINE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CDLINE, GwyCDLineClass))
#define GWY_IS_CDLINE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CDLINE))
#define GWY_IS_CDLINE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CDLINE))
#define GWY_CDLINE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CDLINE, GwyCDLineClass))


typedef struct _GwyCDLineBuiltin  GwyCDLineBuiltin;
typedef struct _GwyCDLine GwyCDLine;
typedef struct _GwyCDLineClass GwyCDLineClass;


struct _GwyCDLine {
    GwyResource parent_instance;

   const GwyCDLineBuiltin *builtin;

   gpointer reserved1;
   gpointer reserved2;
};

struct _GwyCDLineClass {
    GwyResourceClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};


const
gchar*          gwy_cdline_get_name   (GwyCDLine* preset);
const
gchar*          gwy_cdline_get_definition(GwyCDLine* preset);
const
gchar*          gwy_cdline_get_param_name(GwyCDLine* preset,
                                                     gint param);
gdouble         gwy_cdline_get_param_default(GwyCDLine* preset,
                                                    gint param);
gint            gwy_cdline_get_nparams(GwyCDLine* preset);
void            gwy_cdline_fit        (GwyCDLine* preset,
                                              gint n_dat,
                                              const gdouble *x,
                                              const gdouble *y,
                                              gint n_param,
                                              gdouble *param,
                                              gdouble *err,
                                              const gboolean *fixed_param,
                                              gpointer user_data);
gdouble         gwy_cdline_get_value         (GwyCDLine *preset,
                                              gdouble x,
                                              const gdouble *params,
                                              gboolean *fres);

GwyInventory*   gwy_cdlines              (void);

GType           gwy_cdline_get_type      (void) G_GNUC_CONST;

G_END_DECLS

#endif /*__GWY_PROCESS_FRACTALS__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
