/* @(#) $Id$ */

#ifndef __GWY_TOOLS_H__
#define __GWY_TOOLS_H__

#include <libgwydgets/gwydatawindow.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*GwyToolUseFunc)(GwyDataWindow *data_window);

void gwy_tool_crop_use    (GwyDataWindow *data_window);
void gwy_tool_level3_use  (GwyDataWindow *data_window);
void gwy_tool_pointer_use (GwyDataWindow *data_window);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_TOOLS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

