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

#ifndef __GWY_VECTOR_SHADE_H__
#define __GWY_VECTOR_SHADE_H__

#include <gtk/gtk.h>

#ifndef GWY_TYPE_GRAD_SPHERE
#  include <libgwydgets/gwygradsphere.h>
#endif /* no GWY_TYPE_GRAD_SPHERE */

G_BEGIN_DECLS

#define GWY_TYPE_VECTOR_SHADE            (gwy_vector_shade_get_type())
#define GWY_VECTOR_SHADE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_VECTOR_SHADE, GwyVectorShade))
#define GWY_VECTOR_SHADE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_VECTOR_SHADE, GwyVectorShadeClass))
#define GWY_IS_VECTOR_SHADE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_VECTOR_SHADE))
#define GWY_IS_VECTOR_SHADE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_VECTOR_SHADE))
#define GWY_VECTOR_SHADE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_VECTOR_SHADE, GwyVectorShadeClass))

typedef struct _GwyVectorShade      GwyVectorShade;
typedef struct _GwyVectorShadeClass GwyVectorShadeClass;

struct _GwyVectorShade {
    GtkTable table;

    /* The gradient sphere */
    GwyGradSphere *grad_sphere;

    /* The spinbuttion */
    GtkAdjustment *adj_theta;
    GtkAdjustment *adj_phi;
    GtkWidget *spin_theta;
    GtkWidget *spin_phi;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyVectorShadeClass {
    GtkTableClass parent_class;
    void (*vector_shade)(GwyVectorShade *vector_shade);

    gpointer reserved1;
    gpointer reserved2;
};

GType            gwy_vector_shade_get_type          (void) G_GNUC_CONST;
GtkWidget*       gwy_vector_shade_new               (GwySphereCoords *sphere_coords);
GtkWidget*       gwy_vector_shade_get_grad_sphere   (GwyVectorShade *vector_shade);
GwySphereCoords* gwy_vector_shade_get_sphere_coords (GwyVectorShade *vector_shade);
void             gwy_vector_shade_set_sphere_coords (GwyVectorShade *vector_shade,
                                                     GwySphereCoords *sphere_coords);

G_END_DECLS

#endif /* __GWY_VECTOR_SHADE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
