/* @(#) $Id$ */

#ifndef __GWY_APP_SETTINGS_H__
#define __GWY_APP_SETTINGS_H__

#include <libgwyddion/gwycontainer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GwyContainer* gwy_app_settings_get  (void);
void          gwy_app_settings_free (void);
gboolean      gwy_app_settings_save (const gchar *filename);
gboolean      gwy_app_settings_load (const gchar *filename);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_APP_SETTINGS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

