/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_MODULE_FILE_H__
#define __GWY_MODULE_FILE_H__

#include <gtk/gtkobject.h>
#include <libgwyddion/gwycontainer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    GWY_FILE_NONE   = 0,
    GWY_FILE_LOAD   = 1 << 0,
    GWY_FILE_SAVE   = 1 << 1,
    GWY_FILE_DETECT = 1 << 2,
    GWY_FILE_MASK   = 0x07
} GwyFileOperation;

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

gboolean          gwy_file_func_register        (const gchar *modname,
                                                 GwyFileFuncInfo *func_info);
gint              gwy_file_func_run_detect      (const gchar *name,
                                                 const gchar *filename,
                                                 gboolean only_name);
GwyContainer*     gwy_file_func_run_load        (const gchar *name,
                                                 const gchar *filename);
gboolean          gwy_file_func_run_save        (const gchar *name,
                                                 GwyContainer *data,
                                                 const gchar *filename);
GwyFileOperation  gwy_file_func_get_operations  (const gchar *name);
/* high-level interface */
G_CONST_RETURN
gchar*          gwy_file_detect             (const gchar *filename);
GwyContainer*   gwy_file_load               (const gchar *filename);
gboolean        gwy_file_save               (GwyContainer *data,
                                             const gchar *filename);

GtkObject*      gwy_build_file_menu         (GtkObject *item_factory,
                                             const gchar *prefix,
                                             GCallback item_callback,
                                             GwyFileOperation type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_MODULE_FILE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
