/* @(#) $Id$ */

#ifndef __GWY_MODULE_FILE_H__
#define __GWY_MODULE_FILE_H__

#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymoduleloader.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GwyFileFuncInfo GwyFileFuncInfo;

typedef gint           (*GwyFileDetectFunc)     (const gchar *filename,
                                                 gboolean only_name,
                                                 const gchar *name);
typedef GwyContainer*  (*GwyFileLoadFunc)       (const gchar *filename,
                                                 const gchar *name);
typedef gboolean       (*GwyFileSaveFunc)       (GwyContainer *data,
                                                 const gchar *filename,
                                                 const gchar *name);

struct _GwyFileFuncInfo {
    const gchar *name;
    const gchar *file_desc;
    GwyFileDetectFunc detect;
    GwyFileLoadFunc load;
    GwyFileSaveFunc save;
};

gboolean        gwy_file_func_register      (const gchar *modname,
                                             GwyFileFuncInfo *func_info);
gint            gwy_file_func_run_detect    (const gchar *name,
                                             const gchar *filename,
                                             gboolean only_name);
GwyContainer*   gwy_file_func_run_load      (const gchar *name,
                                             const gchar *filename);
gboolean        gwy_file_func_run_save      (const gchar *name,
                                             GwyContainer *data,
                                             const gchar *filename);
/* high-level interface */
G_CONST_RETURN
gchar*          gwy_file_detect             (const gchar *filename);
GwyContainer*   gwy_file_load               (const gchar *filename);
gboolean        gwy_file_save               (GwyContainer *data,
                                             const gchar *filename);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_MODULE_FILE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
