
/*PROVISORY routines to debug datafield procedures*/

#ifndef __GWY_PIXFIELD__
#define __GWY_PIXFIELD__

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "datafield.h"

#define GWY_PAL_BLACK  0 
#define GWY_PAL_RED    1
#define GWY_PAL_GREEN  2
#define GWY_PAL_BLUE   3
#define GWY_PAL_YELLOW 4
#define GWY_PAL_PINK   5
#define GWY_PAL_BROWN  6
#define GWY_PAL_NAVY   7
#define GWY_PAL_OLIVE  8

typedef struct{
	gint r[512];
	gint g[512];
	gint b[512];
} PixPalette;

/*make false color gtk-pixbuf from datafield with a given palette*/
void gwy_pixfield_do(GdkPixbuf *g, GwyDataField *f, PixPalette *pal);

void gwy_pixfield_do_log(GdkPixbuf *g, GwyDataField *f, PixPalette *pal);

/*set simple palette*/
void gwy_pixfield_presetpal(PixPalette *pal, gint type);

#endif /*__GWY_PIXFIELD__*/
