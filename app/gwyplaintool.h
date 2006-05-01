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

#ifndef __GWY_PLAIN_TOOL_H__
#define __GWY_PLAIN_TOOL_H__

#include <app/gwytool.h>

G_BEGIN_DECLS

#define GWY_TYPE_PLAIN_TOOL             (gwy_plain_tool_get_type())
#define GWY_PLAIN_TOOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_PLAIN_TOOL, GwyPlainTool))
#define GWY_PLAIN_TOOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_PLAIN_TOOL, GwyPlainToolClass))
#define GWY_IS_PLAIN_TOOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_PLAIN_TOOL))
#define GWY_IS_PLAIN_TOOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_PLAIN_TOOL))
#define GWY_PLAIN_TOOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_PLAIN_TOOL, GwyPlainToolClass))

typedef struct _GwyPlainTool      GwyPlainTool;
typedef struct _GwyPlainToolClass GwyPlainToolClass;

typedef enum {
    GWY_PLAIN_TOOL_CHANGED_DATA       = 1 << 0,
    GWY_PLAIN_TOOL_CHANGED_MASK       = 1 << 1,
    GWY_PLAIN_TOOL_CHANGED_SHOW       = 1 << 2,
    GWY_PLAIN_TOOL_CHANGED_SELECTION  = 1 << 3,
    GWY_PLAIN_TOOL_FINISHED_SELECTION = 1 << 4,
} GwyPlainToolChanged;

struct _GwyPlainTool {
    GwyTool parent_instance;

    gboolean init_failed;

    gboolean lazy_updates;
    GwyPlainToolChanged pending_updates;

    GwySIUnitFormatStyle unit_style;
    GwySIValueFormat *coord_format;
    GwySIValueFormat *value_format;

    GwyDataView *data_view;

    GwyContainer *container;
    gulong data_item_id;
    gulong mask_item_id;
    gulong show_item_id;
    gint id;

    GwyDataField *data_field;
    gulong data_id;

    GwyDataField *mask_field;
    gulong mask_id;

    GwyDataField *show_field;
    gulong show_id;

    GwyVectorLayer *layer;
    GwySelection *selection;
    gchar *selection_bname;
    GType layer_type;
    gulong selection_item_id;
    gulong selection_cid;
    gulong selection_fid;

    GtkWidget *clear;
};

struct _GwyPlainToolClass {
    GwyToolClass parent_class;

    void (*data_changed)(GwyPlainTool *plain_tool);
    void (*mask_changed)(GwyPlainTool *plain_tool);
    void (*show_changed)(GwyPlainTool *plain_tool);
    void (*selection_changed)(GwyPlainTool *plain_tool,
                              gint hint);
    void (*selection_finished)(GwyPlainTool *plain_tool);
};

GType        gwy_plain_tool_get_type         (void) G_GNUC_CONST;
GType        gwy_plain_tool_check_layer_type (GwyPlainTool *plain_tool,
                                              const gchar *name);
void         gwy_plain_tool_connect_selection(GwyPlainTool *plain_tool,
                                              GType layer_type,
                                              const gchar *bname);
void         gwy_plain_tool_assure_layer     (GwyPlainTool *plain_tool,
                                              GType layer_type);
const gchar* gwy_plain_tool_set_selection_key(GwyPlainTool *plain_tool,
                                              const gchar *bname);
GtkWidget*   gwy_plain_tool_add_clear_button (GwyPlainTool *plain_tool);

gdouble gwy_plain_tool_get_z_average(GwyDataField *data_field,
                                     const gdouble *point,
                                     gint radius);

typedef struct _GwyRectSelectionLabels GwyRectSelectionLabels;

GwyRectSelectionLabels* gwy_rect_selection_labels_new(gboolean none_is_full,
                                                      GCallback callback,
                                                      gpointer cbdata);
GtkWidget* gwy_rect_selection_labels_get_table(GwyRectSelectionLabels *rlabels);
void       gwy_rect_selection_labels_select   (GwyRectSelectionLabels *rlabels,
                                               GwySelection *selection,
                                               GwyDataField *dfield);
gboolean   gwy_rect_selection_labels_fill     (GwyRectSelectionLabels *rlabels,
                                               GwySelection *selection,
                                               GwyDataField *dfield,
                                               gdouble *selreal,
                                               gint *selpix);

G_END_DECLS

#endif /* __GWY_PLAIN_TOOL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
