/* @(#) $Id$ */

#ifndef __GWY_MODULE_H__
#define __GWY_MODULE_H__

#include <gmodule.h>
#include <gtk/gtkitemfactory.h>
#include <libgwyddion/gwycontainer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_MODULE_ABI_VERSION 0

/**
 * GWY_MODULE_QUERY:
 * The declaration of module info query (the ONLY exported symbol from
 * a module).
 **/
#define _GWY_MODULE_QUERY _gwy_module_query
#define GWY_MODULE_QUERY_NAME G_STRINGIFY(_GWY_MODULE_QUERY)
#define GWY_MODULE_QUERY(mod_info) \
    G_MODULE_EXPORT GwyModuleInfo* \
    _GWY_MODULE_QUERY(void) { return &mod_info; }

typedef enum {
    GWY_RUN_INTERACTIVE    = 1 << 0,
    GWY_RUN_NONINTERACTIVE = 1 << 1,
    GWY_RUN_WITH_DEFAULTS  = 1 << 2,
    GWY_RUN_MASK           = 0x07
} GwyRunType;

typedef gboolean (*GwyModuleRegisterFunc)(const gchar *name);
typedef gboolean (*GwyProcessFunc)(GwyContainer *data, GwyRunType run);

typedef struct _GwyModuleInfo GwyModuleInfo;

struct _GwyModuleInfo {
    guint32 abi_version;
    GwyModuleRegisterFunc register_func;
    const gchar *name;
    const gchar *blurb;
    const gchar *author;
    const gchar *version;
    const gchar *copyright;
    const gchar *date;
};

typedef GwyModuleInfo* (*GwyModuleQueryFunc)(void);

typedef struct _GwyProcessFuncInfo GwyProcessFuncInfo;

struct _GwyProcessFuncInfo {
    const gchar *name;
    GwyProcessFunc function;
    GwyRunType run;
    const gchar *menu_path;
};

void            gwy_module_register_modules (const gchar **paths);
gboolean        gwy_register_process_func   (const gchar *modname,
                                             GwyProcessFuncInfo *func_info);
GtkItemFactory* gwy_build_process_menu      (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_MODULE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
