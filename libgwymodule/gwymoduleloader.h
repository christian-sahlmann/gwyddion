/* @(#) $Id$ */

#ifndef __GWY_MODULE_LOADER_H__
#define __GWY_MODULE_LOADER_H__

#include <gmodule.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_MODULE_ABI_VERSION 0

#define _GWY_MODULE_QUERY _gwy_module_query
#define GWY_MODULE_QUERY(mod_info) \
    G_MODULE_EXPORT GwyModuleInfo* \
    _GWY_MODULE_QUERY(void) { return &mod_info; }

typedef enum {
    GWY_RUN_NONE           = 0,
    GWY_RUN_WITH_DEFAULTS  = 1 << 0,
    GWY_RUN_NONINTERACTIVE = 1 << 1,
    GWY_RUN_MODAL          = 1 << 2,
    GWY_RUN_INTERACTIVE    = 1 << 3,
    GWY_RUN_MASK           = 0x0f
} GwyRunType;

typedef struct _GwyModuleInfo GwyModuleInfo;
typedef struct _GwyModuleInfoInternal GwyModuleInfoInternal;

typedef gboolean       (*GwyModuleRegisterFunc) (const gchar *name);
typedef GwyModuleInfo* (*GwyModuleQueryFunc)    (void);

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

struct _GwyModuleInfoInternal {
    GwyModuleInfo *mod_info;
    gchar *file;
    gboolean loaded;
};

void                   gwy_modules_init            (void);
void                   gwy_module_register_modules (const gchar **paths);
GwyModuleInfoInternal* gwy_module_get_module_info  (const gchar *name);
void                   gwy_module_foreach          (GHFunc function,
                                                    gpointer data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_MODULE_LOADER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
