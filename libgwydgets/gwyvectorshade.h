/* @(#) $Id$ */

#ifndef __GWY_VECTOR_SHADE_H__
#define __GWY_VECTOR_SHADE_H__

#include <gtk/gtk.h>

#ifndef GWY_TYPE_GRAD_SPHERE
#  include <libgwydgets/gwygradsphere.h>
#endif /* no GWY_TYPE_GRAD_SPHERE */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
};

struct _GwyVectorShadeClass {
    GtkTableClass parent_class;
    void (*vector_shade)(GwyVectorShade *vector_shade);
};

GType            gwy_vector_shade_get_type          (void) G_GNUC_CONST;
GtkWidget*       gwy_vector_shade_new               (GwySphereCoords *sphere_coords);
GtkWidget*       gwy_vector_shade_get_grad_sphere   (GwyVectorShade *vector_shade);
GwySphereCoords* gwy_vector_shade_get_sphere_coords (GwyVectorShade *vector_shade);
void             gwy_vector_shade_set_sphere_coords (GwyVectorShade *vector_shade,
                                                     GwySphereCoords *sphere_coords);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_VECTOR_SHADE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
