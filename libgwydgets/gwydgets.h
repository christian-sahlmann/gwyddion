/* @(#) $Id$ */

#ifndef __GWY_GWYDGETS_H__
#define __GWY_GWYDGETS_H__

#include <libgwydgets/gwyaxis.h>
#include <libgwydgets/gwyspherecoords.h>
#include <libgwydgets/gwygradsphere.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygrapharea.h>
#include <libgwydgets/gwygraphcorner.h>
#include <libgwydgets/gwygraphlabel.h>
#include <libgwydgets/gwyvectorshade.h>
#include <libgwydgets/gwyruler.h>
#include <libgwydgets/gwyhruler.h>
#include <libgwydgets/gwyvruler.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydataviewlayer.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwylayer-lines.h>
#include <libgwydgets/gwylayer-points.h>
#include <libgwydgets/gwylayer-select.h>
#include <libgwydgets/gwydatawindow.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GtkWidget* gwy_palette_option_menu(GCallback callback,
                                   gpointer cbdata,
                                   const gchar *current);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_GWYDGETS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
