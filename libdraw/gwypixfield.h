/* @(#) $Id$ */

/*PROVISORY routines to debug datafield procedures*/

#ifndef __GWY_PIXFIELD__
#define __GWY_PIXFIELD__

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libprocess/datafield.h>
#include <libdraw/gwypalette.h>

void     gwy_pixfield_do         (GdkPixbuf *pixbuf,
                                  GwyDataField *data_field,
                                  GwyPalette *palette);
void     gwy_pixfield_mask       (GdkPixbuf *pixbuf,
                                  GwyDataField *data_field,
                                  GwyRGBA *color);


#endif /*__GWY_PIXFIELD__*/
