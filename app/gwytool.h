/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_TOOL_H__
#define __GWY_TOOL_H__

#include <libgwydgets/gwydataview.h>

G_BEGIN_DECLS

#define GWY_TYPE_TOOL             (gwy_tool_get_type())
#define GWY_TOOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL, GwyTool))
#define GWY_TOOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_TOOL, GwyToolClass))
#define GWY_IS_TOOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL))
#define GWY_IS_TOOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_TOOL))
#define GWY_TOOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL, GwyToolClass))

typedef enum {
    GWY_TOOL_RESPONSE_CLEAR  = 1,
    GWY_TOOL_RESPONSE_UPDATE = 2,
} GwyToolResponseType;

typedef struct _GwyTool      GwyTool;
typedef struct _GwyToolClass GwyToolClass;

struct _GwyTool {
    GObject parent_instance;

    GtkWidget *dialog;
    gboolean is_visible;

    gulong handler_id;
    gpointer reserved1;
    gpointer reserved2;
    gint int1;
};

struct _GwyToolClass {
    GObjectClass parent_class;

    const gchar *stock_id;
    const gchar *tooltip;
    const gchar *title;
    const gchar *prefix;

    gint default_width;
    gint default_height;

    void (*show)(GwyTool *tool);
    void (*hide)(GwyTool *tool);
    void (*data_switched)(GwyTool *tool,
                          GwyDataView *data_view);
    void (*response)(GwyTool *tool,
                     gint response_id);

    void (*reserved1)(void);
    void (*reserved2)(void);
    void (*reserved3)(void);
    void (*reserved4)(void);
};

GType        gwy_tool_get_type          (void) G_GNUC_CONST;

void         gwy_tool_add_hide_button   (GwyTool *tool,
                                         gboolean set_default);
void         gwy_tool_show              (GwyTool *tool);
void         gwy_tool_hide              (GwyTool *tool);
gboolean     gwy_tool_is_visible        (GwyTool *tool);
void         gwy_tool_data_switched     (GwyTool *tool,
                                         GwyDataView *data_view);

const gchar* gwy_tool_class_get_title   (GwyToolClass *klass);
const gchar* gwy_tool_class_get_stock_id(GwyToolClass *klass);
const gchar* gwy_tool_class_get_tooltip (GwyToolClass *klass);

G_END_DECLS

#endif /* __GWY_TOOL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
