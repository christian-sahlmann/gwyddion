/* @(#) $Id$ */

#ifndef __GWY_MODULE_H__
#define __GWY_MODULE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_MODULE_ABI_VERSION 0

#define GWY_MODULE_REGISTER_FUNC gwy_module_register
#define GWY_MODULE_REGISTER_FUNC_NAME G_STRINGIFY(GWY_MODULE_REGISTER_FUNC)

typedef struct _GwyModuleInfo GwyModuleInfo;
typedef struct _GwyModuleFuncInfo GwyModuleFuncInfo;

typedef enum {
    GWY_FUNC_ROLE_NONE,
    GWY_FUNC_ROLE_CLASS,
    GWY_FUNC_ROLE_PROCESS,
    GWY_FUNC_ROLE_LOAD,
    GWY_FUNC_ROLE_SAVE
} GwyModuleFuncRole;

struct _GwyModuleFuncInfo {
    const gchar *name;
    const gchar *blurb;
    GwyModuleFuncRole role;
};

struct _GwyModuleInfo {
    guint32 abi_version;
    const gchar *name;
    const gchar *blurb;
    const gchar *author;
    const gchar *version;
    const gchar *copyright;
    const gchar *date;
    gsize nfuncs;
    GwyModuleFuncInfo *func_info;
};

typedef GwyModuleInfo* (*GwyModuleRegisterFunc)(GModule *mod);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_MODULE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
