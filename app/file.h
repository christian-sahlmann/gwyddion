/* @(#) $Id$ */

#ifndef __GWY_APP_FILE_H__
#define __GWY_APP_FILE_H__

#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void gwy_app_file_open_cb       (void);
void gwy_app_file_save_as_cb    (void);
void gwy_app_file_save_cb       (void);
void gwy_app_file_duplicate_cb  (void);
void gwy_app_file_close_cb      (void);

/* FIXME: to be moved somewhere? refactored? */
GtkWidget* gwy_app_data_window_create       (GwyContainer *data);
void       gwy_app_clean_up_data            (GwyContainer *data);
gint       gwy_app_data_window_set_untitled (GwyDataWindow *data_window);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_APP_FILE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
