/* @(#) $Id$ */

#ifndef __GWY_MODULE_H__
#define __GWY_MODULE_H__

#include <gmodule.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_MODULE_ABI_VERSION 0

/**
 * GWY_MODULE_REGISTER_FUNC:
 * The declaration of a module registration function (the only exported
 * function).
 **/
#define _GWY_MODULE_REGISTER_FUNC gwy_module_register
#define GWY_MODULE_REGISTER_FUNC_NAME G_STRINGIFY(_GWY_MODULE_REGISTER_FUNC)
#define GWY_MODULE_REGISTER_FUNC(m) \
    G_MODULE_EXPORT G_CONST_RETURN GwyModuleInfo* \
    _GWY_MODULE_REGISTER_FUNC(GModule *m)

typedef struct _GwyModuleInfo GwyModuleInfo;

struct _GwyModuleInfo {
    guint32 abi_version;
    const gchar *name;
    const gchar *blurb;
    const gchar *author;
    const gchar *version;
    const gchar *copyright;
    const gchar *date;
};

typedef GwyModuleInfo* (*GwyModuleRegisterFunc)(GModule *module);

void gwy_module_register_modules(const gchar **paths);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_MODULE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
