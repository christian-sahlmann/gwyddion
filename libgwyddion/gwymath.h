/* @(#) $Id$ */

#ifndef __GWY_MATH_H__
#define __GWY_MATH_H__

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

const gchar* gwy_math_SI_prefix          (gdouble magnitude);
gint         gwy_math_find_nearest_line  (gdouble x,
                                          gdouble y,
                                          gdouble *d2min,
                                          gint n,
                                          gdouble *coords);
gint         gwy_math_find_nearest_point (gdouble x,
                                          gdouble y,
                                          gdouble *d2min,
                                          gint n,
                                          gdouble *coords);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_MATH_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
