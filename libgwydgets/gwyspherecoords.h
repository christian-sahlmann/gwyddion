#ifndef __GWY_SPHERE_COORDS_H__
#define __GWY_SPHERE_COORDS_H__

#include <gdk/gdk.h>
#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_SPHERE_COORDS                  (gwy_sphere_coords_get_type())
#define GWY_SPHERE_COORDS(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SPHERE_COORDS, GwySphereCoords))
#define GWY_SPHERE_COORDS_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SPHERE_COORDS, GwySphereCoordsClass))
#define GWY_IS_SPHERE_COORDS(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SPHERE_COORDS))
#define GWY_IS_SPHERE_COORDS_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SPHERE_COORDS))
#define GWY_SPHERE_COORDS_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SPHERE_COORDS, GwySphereCoordsClass))


typedef struct _GwySphereCoords GwySphereCoords;
typedef struct _GwySphereCoordsClass GwySphereCoordsClass;

struct _GwySphereCoords {
    GtkObject parent_instance;

    gdouble theta;
    gdouble phi;
};

struct _GwySphereCoordsClass {
    GtkObjectClass parent_class;

    void (*value_changed)(GwySphereCoords *sphere_coords);
};


GType      gwy_sphere_coords_get_type       (void) G_GNUC_CONST;
GtkObject* gwy_sphere_coords_new            (gdouble theta,
                                             gdouble phi);
gdouble    gwy_sphere_coords_get_theta      (GwySphereCoords *sphere_coords);
gdouble    gwy_sphere_coords_get_phi        (GwySphereCoords *sphere_coords);
void       gwy_sphere_coords_set_value      (GwySphereCoords *sphere_coords,
                                             gdouble theta,
                                             gdouble phi);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_SPHERE_COORDS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
