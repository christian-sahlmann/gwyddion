/* @(#) $Id$ */

#ifndef __GWY_INTERPOLATION_H__
#define __GWY_INTERPOLATION_H__
#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* TODO: change gint arguments to GwyInterpolationType */
typedef enum {
  GWY_INTERPOLATION_NONE      = 0,
  GWY_INTERPOLATION_ROUND     = 1,
  GWY_INTERPOLATION_BILINEAR  = 2,
  GWY_INTERPOLATION_KEY       = 3,
  GWY_INTERPOLATION_BSPLINE   = 4,
  GWY_INTERPOLATION_OMOMS     = 5,
  GWY_INTERPOLATION_NNA       = 6,
} GwyInterpolationType;

/*simple interpolation of non-equidistant values using two neighbour values*/
gdouble gwy_interpolation_get_dval(gdouble x, gdouble x1, gdouble y1, gdouble x2, gdouble y2, gint interpolation);

/*NOTE: quick interpolation of equidistant values is implemented
 in dataline and datafield classes separately.*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_INTERPOLATION_H__*/




