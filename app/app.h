/* @(#) $Id$ */

#ifndef __GWY_APP_APP_H__
#define __GWY_APP_APP_H__

#include <gtk/gtkwidget.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwydgets/gwydatawindow.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GtkWidget *gwy_app_main_window;

GwyDataWindow*  gwy_app_get_current_data_window  (void);
GwyContainer*   gwy_app_get_current_data         (void);
void            gwy_app_set_current_data_window  (GwyDataWindow *data_window);
void            gwy_app_quit                     (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_APP_APP_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

