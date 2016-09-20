/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/*< private_header >*/

#ifndef __GWYDDION_TOOLBOX_H__
#define __GWYDDION_TOOLBOX_H__

#include <gtk/gtkwidget.h>
#include <libgwyddion/gwyenum.h>
#include <libgwymodule/gwymoduleloader.h>

G_BEGIN_DECLS

#define GWY_TOOLBOX_WM_ROLE "gwyddion-toolbox"

typedef enum {
    GWY_APP_ACTION_TYPE_GROUP = -2,    /* Only used in the editor. */
    GWY_APP_ACTION_TYPE_NONE = -1,
    GWY_APP_ACTION_TYPE_PLACEHOLDER = 0,
    GWY_APP_ACTION_TYPE_BUILTIN,
    GWY_APP_ACTION_TYPE_PROC,
    GWY_APP_ACTION_TYPE_GRAPH,
    GWY_APP_ACTION_TYPE_TOOL,
    GWY_APP_ACTION_TYPE_VOLUME,
    GWY_APP_ACTION_TYPE_XYZ,
} GwyAppActionType;

typedef struct {
    const gchar *name;
    const gchar *stock_id;
    GCallback callback;
    const gchar *nice_name;  /* Menu path? */
    const gchar *tooltip;
    /* TODO: sens flags? */
} GwyToolboxBuiltinSpec;

/* Representation of the toolbox as given in toolbox.xml.
 * This is something we do not modify, except:
 * (a) in the editor
 * (b) by removal of invalid stuff (during construction).
 * The on-disk file is only written by us when the users uses the editor. */
typedef struct {
    GwyAppActionType type;
    GQuark function;
    GQuark icon;
    GwyRunType mode;
} GwyToolboxItemSpec;

typedef struct {
    GArray *item;
    gchar *name;
    GQuark id;
} GwyToolboxGroupSpec;

typedef struct {
    GArray *group;
    guint width;
    /* Auxiliary data. */
    GString *path;
    gboolean seen_tool_placeholder;
} GwyToolboxSpec;

extern const GwyEnum *gwy_toolbox_action_types;
extern const GwyEnum *gwy_toolbox_mode_types;

GwyToolboxSpec*              gwy_parse_toolbox_ui            (void);
GwyToolboxSpec*              gwy_app_toolbox_parse           (const gchar *ui,
                                                              gsize ui_len,
                                                              GError **error);
void                         gwy_app_toolbox_spec_free       (GwyToolboxSpec *spec);
void                         gwy_app_toolbox_spec_remove_item(GwyToolboxSpec *spec,
                                                              guint i,
                                                              guint j);
void                         gwy_toolbox_editor              (void);
const GwyToolboxBuiltinSpec* gwy_toolbox_get_builtins        (guint *nspec);
const GwyToolboxBuiltinSpec* gwy_toolbox_find_builtin_spec   (const gchar *name);

G_END_DECLS

#endif /* __GWYDDION_TOOLBOX_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
