
#ifndef __GWY_SIMPLEFFT_H__
#define __GWY_SIMPLEFFT_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* TODO: change gint arguments to GwyWindowingType (and probably change
 * GWY_WINDOW -> GWY_WINDOWING, GWY_WINDOW could be a widget...) */
enum {
  GWY_WINDOW_NONE       = 0,
  GWY_WINDOW_HANN       = 1,
  GWY_WINDOW_HAMMING    = 2,
  GWY_WINDOW_BLACKMANN  = 3,
  GWY_WINDOW_LANCZOS    = 4,
  GWY_WINDOW_WELCH      = 5,
  GWY_WINDOW_RECT       = 6,
} GwyWindowingType;

/*2^N fft algorithm*/
gint gwy_fft_hum(gint dir, gdouble *re_in, gdouble *im_in, gdouble *re_out, gdouble *im_out, gint n);

/*apply windowing*/
gint gwy_fft_window(gdouble *data, gint n, gint wintype);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*__GWY_SIPLEFFT__*/
