/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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
#include <libgwymodule/gwymoduleenums.h>

G_BEGIN_DECLS

#define GWY_FILE_DETECT_BUFFER_SIZE 4096U

typedef struct {
    const gchar *name;
    const gchar *name_lowercase;
    gsize file_size;
    guint buffer_len;
    const guchar *buffer;
} GwyFileDetectInfo;

typedef struct _GwyFileFuncInfo GwyFileFuncInfo;

typedef gint           (*GwyFileDetectFunc) (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name,
                                             const gchar *name);
typedef GwyContainer*  (*GwyFileLoadFunc)   (const gchar *filename,
                                             const gchar *name);
typedef gboolean       (*GwyFileSaveFunc)   (GwyContainer *data,
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
const gchar*      gwy_file_func_get_description (const gchar *name);
/* high-level interface */
G_CONST_RETURN
gchar*          gwy_file_detect             (const gchar *filename,
                                             gboolean only_name,
                                             GwyFileOperation operations);
GwyContainer*   gwy_file_load               (const gchar *filename);
gboolean        gwy_file_save               (GwyContainer *data,
                                             const gchar *filename);

GtkObject*      gwy_file_func_build_menu    (GtkObject *item_factory,
                                             const gchar *prefix,
                                             GCallback item_callback,
                                             GwyFileOperation type);

G_END_DECLS

#endif /* __GWY_MODULE_FILE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
