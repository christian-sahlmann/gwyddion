/* @(#) $Id$ */

#ifndef __GWY_MODULE_PROCESS_H__
#define __GWY_MODULE_PROCESS_H__

#include <gtk/gtkobject.h>
#include <gtk/gtkaccelgroup.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymoduleloader.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GwyProcessFuncInfo GwyProcessFuncInfo;

typedef gboolean       (*GwyProcessFunc)        (GwyContainer *data,
                                                 GwyRunType run);

struct _GwyProcessFuncInfo {
    const gchar *name;
    const gchar *menu_path;
    GwyProcessFunc process;
    GwyRunType run;
};

gboolean        gwy_register_process_func   (const gchar *modname,
                                             GwyProcessFuncInfo *func_info);
GtkObject*      gwy_build_process_menu      (GtkAccelGroup *accel_group);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_MODULE_PROCESS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
