/* @(#) $Id$ */

#ifndef __GWY_MENU_H__
#define __GWY_MENU_H__

#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


enum {
    GWY_MENU_FLAG_DATA = 1 << 0,
    GWY_MENU_FLAG_UNDO = 1 << 1,
    GWY_MENU_FLAG_REDO = 1 << 2
};

typedef struct _GwyMenuSensitiveData GwyMenuSensitiveData;

struct _GwyMenuSensitiveData {
    guint set_to;
    guint flags;
};

GtkWidget* gwy_menu_create_xtns_menu         (GtkAccelGroup *accel_group);
GtkWidget* gwy_menu_create_proc_menu         (GtkAccelGroup *accel_group);
GtkWidget* gwy_menu_create_file_menu         (GtkAccelGroup *accel_group);
GtkWidget* gwy_menu_create_edit_menu         (GtkAccelGroup *accel_group);

void       gwy_menu_set_sensitive_recursive  (GtkWidget *widget,
                                              GwyMenuSensitiveData *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_MENU_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
