/* @(#) $Id$ */

#ifndef __GWY_SIMPLEFFT_H__
#define __GWY_SIMPLEFFT_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* TODO: change gint arguments to GwyWindowingType (and probably change
 * GWY_WINDOW -> GWY_WINDOWING, GWY_WINDOW could be a widget...) */
typedef enum {
  GWY_WINDOWING_NONE       = 0,
  GWY_WINDOWING_HANN       = 1,
  GWY_WINDOWING_HAMMING    = 2,
  GWY_WINDOWING_BLACKMANN  = 3,
  GWY_WINDOWING_LANCZOS    = 4,
  GWY_WINDOWING_WELCH      = 5,
  GWY_WINDOWING_RECT       = 6
} GwyWindowingType;

/*2^N fft algorithm*/
gint gwy_fft_hum(gint dir, gdouble *re_in, gdouble *im_in,
                 gdouble *re_out, gdouble *im_out, gint n);

/*apply windowing*/
void gwy_fft_window(gdouble *data, gint n, GwyWindowingType windowing);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*__GWY_SIPLEFFT__*/
