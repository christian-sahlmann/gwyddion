/* @(#) $Id$ */

#ifndef __GWY_GRADSPHERE_H__
#define __GWY_GRADSPHERE_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>

#ifndef GWY_TYPE_SPHERE_COORDS
#  include <libgwydgets/gwyspherecoords.h>
#endif /* no GWY_TYPE_SPHERE_COORDS */

#ifndef GWY_TYPE_PALETTE
#  include <libdraw/gwypalette.h>
#endif /* no GWY_TYPE_PALETTE */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_GRAD_SPHERE            (gwy_grad_sphere_get_type())
#define GWY_GRAD_SPHERE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAD_SPHERE, GwyGradSphere))
#define GWY_GRAD_SPHERE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAD_SPHERE, GwyGradSphereClass))
#define GWY_IS_GRAD_SPHERE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAD_SPHERE))
#define GWY_IS_GRAD_SPHERE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAD_SPHERE))
#define GWY_GRAD_SPHERE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAD_SPHERE, GwyGradSphereClass))

typedef struct _GwyGradSphere      GwyGradSphere;
typedef struct _GwyGradSphereClass GwyGradSphereClass;

struct _GwyGradSphere {
    GtkWidget widget;

    guint update_policy : 2;    /* Update policy */
    guint8 button;    /* Whether we have grabbed focus (mouse button pressed) */
    gint radius;    /* Dimensions */
    GwyPalette *palette;    /* The gradient */
    guint32 timer;    /* ID of update timer, or 0 if none */

    /* Current angles */
    gdouble theta;
    gdouble phi;

    /* Old adjustment values */
    gdouble old_theta;
    gdouble old_phi;

    /* The adjustments */
    GwySphereCoords *sphere_coords;

    /* The sphere */
    GdkPixbuf *sphere_pixbuf;
};

struct _GwyGradSphereClass {
    GtkWidgetClass parent_class;
};

GtkWidget*       gwy_grad_sphere_new               (GwySphereCoords *sphere_coords);
GType            gwy_grad_sphere_get_type          (void) G_GNUC_CONST;
GwySphereCoords* gwy_grad_sphere_get_sphere_coords (GwyGradSphere *grad_sphere);
void             gwy_grad_sphere_set_sphere_coords (GwyGradSphere *grad_sphere,
                                                    GwySphereCoords *sphere_coords);
GwyPalette*      gwy_grad_sphere_get_palette       (GwyGradSphere *grad_sphere);
void             gwy_grad_sphere_set_palette       (GwyGradSphere *grad_sphere,
                                                    GwyPalette *palette);
GtkUpdateType    gwy_grad_sphere_get_update_policy (GwyGradSphere *grad_sphere);
void             gwy_grad_sphere_set_update_policy (GwyGradSphere *grad_sphere,
                                                    GtkUpdateType update_policy);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_GRADSPHERE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
