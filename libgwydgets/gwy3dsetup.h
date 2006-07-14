/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_3D_SETUP_H__
#define __GWY_3D_SETUP_H__

#include <glib-object.h>
#include <libgwydgets/gwydgetenums.h>

G_BEGIN_DECLS

#define GWY_TYPE_3D_SETUP             (gwy_3d_setup_get_type())
#define GWY_3D_SETUP(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_3D_SETUP, Gwy3DSetup))
#define GWY_3D_SETUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_3D_SETUP, Gwy3DSetupClass))
#define GWY_IS_3D_SETUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_3D_SETUP))
#define GWY_IS_3D_SETUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_3D_SETUP))
#define GWY_3D_SETUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_3D_SETUP, Gwy3DSetupClass))

typedef struct _Gwy3DSetup      Gwy3DSetup;
typedef struct _Gwy3DSetupClass Gwy3DSetupClass;

struct _Gwy3DSetup {
    GObject parent_instance;

    Gwy3DProjection projection;
    Gwy3DVisualization visualization;
    gint e_reserved_1;
    gint e_reserved_2;

    gboolean axes_visible;
    gboolean labels_visible;
    gboolean b_reserved1;
    gboolean b_reserved2;
    gboolean b_reserved3;
    gboolean b_reserved4;

    guint i_reserved1;
    guint i_reserved2;

    gdouble rotation_x;
    gdouble rotation_y;
    gdouble scale;
    gdouble z_scale;
    gdouble light_phi;
    gdouble light_theta;
    gdouble d_reserved_1;
    gdouble d_reserved_2;
    gdouble d_reserved_3;
    gdouble d_reserved_4;
};

struct _Gwy3DSetupClass {
    GObjectClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};

GType       gwy_3d_setup_get_type(void) G_GNUC_CONST;

Gwy3DSetup* gwy_3d_setup_new     (void);

G_END_DECLS

#endif  /* __GWY_3D_SETUP_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
