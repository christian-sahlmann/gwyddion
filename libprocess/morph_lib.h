/* This is a private header for now, may require clean-up before publishing */

#ifndef __GWY_MORPH_LIB_H__
#define __GWY_MORPH_LIB_H__

#include <libprocess/tip.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
gint **_gwy_morph_lib_iallocmatrix(gint ysiz,
                                   gint xsiz);

G_GNUC_INTERNAL
void _gwy_morph_lib_ifreematrix(gint **mptr,
                                gint ysiz);

G_GNUC_INTERNAL
gdouble **_gwy_morph_lib_dallocmatrix(gint ysiz,
                                      gint xsiz);

G_GNUC_INTERNAL
void _gwy_morph_lib_dfreematrix(gdouble **mptr,
                                gint ysiz);

/*simple routines - integer arithmetics*/
G_GNUC_INTERNAL
gint **_gwy_morph_lib_ireflect(gint **surface,
                               gint surf_xsiz,
                               gint surf_ysiz);

G_GNUC_INTERNAL
gint **_gwy_morph_lib_idilation(gint **surface,
                                gint surf_xsiz,
                                gint surf_ysiz,
                                gint **tip,
                                gint tip_xsiz,
                                gint tip_ysiz,
                                gint xc,
                                gint yc,
                                GwySetFractionFunc set_fraction,
                                GwySetMessageFunc set_message);

G_GNUC_INTERNAL
gint **_gwy_morph_lib_ierosion(gint **image,
                               gint im_xsiz,
                               gint im_ysiz,
                               gint **tip,
                               gint tip_xsiz,
                               gint tip_ysiz,
                               gint xc,
                               gint yc,
                               GwySetFractionFunc set_fraction,
                               GwySetMessageFunc set_message);

G_GNUC_INTERNAL
gint **_gwy_morph_lib_icmap(gint **image,
                            gint im_xsiz,
                            gint im_ysiz,
                            gint **tip,
                            gint tip_xsiz,
                            gint tip_ysiz,
                            gint **rsurf,
                            gint xc,
                            gint yc,
                            GwySetFractionFunc set_fraction,
                            GwySetMessageFunc set_message);

/*simple routines - double arithmetics (can be very slow)*/
G_GNUC_INTERNAL
gdouble **_gwy_morph_lib_dreflect(gdouble **surface,
                                  gint surf_xsiz,
                                  gint surf_ysiz);

G_GNUC_INTERNAL
gdouble **_gwy_morph_lib_ddilation(gdouble **surface,
                                   gint surf_xsiz,
                                   gint surf_ysiz,
                                   gdouble **tip,
                                   gint tip_xsiz,
                                   gint tip_ysiz,
                                   gint xc,
                                   gint yc,
                                   GwySetFractionFunc set_fraction,
                                   GwySetMessageFunc set_message);

G_GNUC_INTERNAL
gdouble **_gwy_morph_lib_derosion(gdouble **image,
                                  gint im_xsiz,
                                  gint im_ysiz,
                                  gdouble **tip,
                                  gint tip_xsiz,
                                  gint tip_ysiz,
                                  gint xc,
                                  gint yc,
                                  GwySetFractionFunc set_fraction,
                                  GwySetMessageFunc set_message);

/*tip estimation routines - all in integer artithmetics*/
G_GNUC_INTERNAL
gint _gwy_morph_lib_itip_estimate(gint **image,
                                  gint im_xsiz,
                                  gint im_ysiz,
                                  gint tip_xsiz,
                                  gint tip_ysiz,
                                  gint xc,
                                  gint yc,
                                  gint **tip0,
                                  gint thresh,
                                  gboolean use_edges,
                                  GwySetFractionFunc set_fraction,
                                  GwySetMessageFunc set_message);

G_GNUC_INTERNAL
gint _gwy_morph_lib_itip_estimate0(gint **image,
                                   gint im_xsiz,
                                   gint im_ysiz,
                                   gint tip_xsiz,
                                   gint tip_ysiz,
                                   gint xc,
                                   gint yc,
                                   gint **tip0,
                                   gint thresh,
                                   gboolean use_edges,
                                   GwySetFractionFunc set_fraction,
                                   GwySetMessageFunc set_message);


G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

