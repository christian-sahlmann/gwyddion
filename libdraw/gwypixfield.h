
/*PROVISORY routines to debug datafield procedures*/

#ifndef __GWY_PIXFIELD__
#define __GWY_PIXFIELD__

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libprocess/datafield.h>
#include <libdraw/gwypalette.h>

/*make false color gtk-pixbuf from datafield with a given palette*/
void gwy_pixfield_do(GdkPixbuf *g, GwyDataField *f, GwyPalette *pal);


#endif /*__GWY_PIXFIELD__*/
