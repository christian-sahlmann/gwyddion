/* @(#) $Id$ */

/* XXX: This file is to be replaced. */
#ifndef __GWY_MODULE_H__
#define __GWY_MODULE_H__

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_MODULE                  (gwy_module_get_type())
#define GWY_MODULE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_MODULE, GwyModule))
#define GWY_MODULE_CLASS(klass)          (G_TYPE_CHECK_INSTANCE_CAST((klass), GWY_TYPE_MODULE, GwyModuleClass))
#define GWY_IS_MODULE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_MODULE))
#define GWY_IS_MODULE_CLASS(klass)       (G_TYPE_CHECK_INSTANCE_TYPE((klass), GWY_TYPE_MODULE))
#define GWY_MODULE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_INTERFACE((obj), GWY_TYPE_MODULE, GwyModuleClass))

#define GWY_MODULE_ABI_VERSION 0

typedef struct _GwyModuleInfo GwyModuleInfo;

struct _GwyModuleInfo {
    const gchar *name;
    const gchar *blurb;
    const gchar *author;
    const gchar *version;
    const gchar *copyright;
    const gchar *date;
};

typedef struct _GwyModule GwyModule;
typedef struct _GwyModuleClass GwyModuleClass;

struct _GwyModule {
    GObject parent_instance;

    guint32 abi_version;
    GwyModuleInfo *module_info;
};

struct _GwyModuleClass {
    GObjectClass parent_class;

    /* TODO */
};

GType      gwy_module_get_type          (void) G_GNUC_CONST;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_MODULE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
