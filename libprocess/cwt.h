/* @(#) $Id$ */

#ifndef __GWY_CWT_H__
#define __GWY_CWT_H__

#include <glib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
  GWY_2DCWT_GAUSS       = 0,
  GWY_2DCWT_HAT         = 1,
} Gwy2DCWTWaveletType;

typedef enum {
  GWY_CWT_GAUSS       = 0,
  GWY_CWT_HAT         = 1,
  GWY_CWT_MORLET      = 2,
} GwyCWTWaveletType;


gdouble wfunc_2d(gdouble scale, gdouble mval, gint xres, Gwy2DCWTWaveletType wtype);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*__GWY_CWT__*/
