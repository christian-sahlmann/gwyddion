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

#ifndef __GWY_SIMPLEFFT_H__
#define __GWY_SIMPLEFFT_H__

#include <glib.h>

#define GWY_ENABLE_DEPRECATED
/*#warning GWY_ENABLE_DEPRECATED*/

G_BEGIN_DECLS

typedef enum {
  GWY_WINDOWING_NONE       = 0,
  GWY_WINDOWING_HANN       = 1,
  GWY_WINDOWING_HAMMING    = 2,
  GWY_WINDOWING_BLACKMANN  = 3,
  GWY_WINDOWING_LANCZOS    = 4,
  GWY_WINDOWING_WELCH      = 5,
  GWY_WINDOWING_RECT       = 6
} GwyWindowingType;

#ifdef GWY_ENABLE_DEPRECATED
/* XXX: never used in libprocess itself */
typedef enum {
  GWY_FFT_OUTPUT_REAL_IMG   = 0,
  GWY_FFT_OUTPUT_MOD_PHASE  = 1,
  GWY_FFT_OUTPUT_REAL       = 2,
  GWY_FFT_OUTPUT_IMG        = 3,
  GWY_FFT_OUTPUT_MOD        = 4,
  GWY_FFT_OUTPUT_PHASE      = 5
} GwyFFTOutputType;
#endif

/*2^N fft algorithm*/
gint gwy_fft_hum(gint dir, gdouble *re_in, gdouble *im_in,
                 gdouble *re_out, gdouble *im_out, gint n);

/*apply windowing*/
void gwy_fft_window(gdouble *data, gint n, GwyWindowingType windowing);


G_END_DECLS


#endif /*__GWY_SIPLEFFT__*/
