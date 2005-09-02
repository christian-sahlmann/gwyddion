/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_SELECTION_H__
#define __GWY_SELECTION_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_SELECTION                  (gwy_selection_get_type())
#define GWY_SELECTION(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION, GwySelection))
#define GWY_SELECTION_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SELECTION, GwySelectionClass))
#define GWY_IS_SELECTION(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION))
#define GWY_IS_SELECTION_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SELECTION))
#define GWY_SELECTION_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION, GwySelectionClass))

typedef struct _GwySelection      GwySelection;
typedef struct _GwySelectionClass GwySelectionClass;

struct _GwySelection {
    GObject parent_instance;

    GArray *objects;
    guint n;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwySelectionClass {
    GObjectClass parent_class;

    guint object_size;

    /* Virtual table */
    void (*clear)(GwySelection *selection);
    gboolean(*get_object)(GwySelection *selection,
                          gint i,
                          gdouble *data);
    void (*set_object)(GwySelection *selection,
                       gint i,
                       const gdouble *data);
    gint (*get_data)(GwySelection *selection,
                     gdouble *data);
    void (*set_data)(GwySelection *selection,
                     gint nselected,
                     const gdouble *data);
    void (*set_max_objects)(GwySelection *selection,
                            gint max_objects);

    /* Signals */
    void (*changed)(GwySelection *selection,
                    gint i);
    void (*finished)(GwySelection *selection);
};

GType    gwy_selection_get_type       (void) G_GNUC_CONST;
gint     gwy_selection_get_object_size(GwySelection *selection);
void     gwy_selection_clear          (GwySelection *selection);
gboolean gwy_selection_get_object     (GwySelection *selection,
                                       gint i,
                                       gdouble *data);
void     gwy_selection_set_object     (GwySelection *selection,
                                       gint i,
                                       const gdouble *data);
gint     gwy_selection_get_data       (GwySelection *selection,
                                       gdouble *data);
void     gwy_selection_set_data       (GwySelection *selection,
                                       gint nselected,
                                       const gdouble *data);
gint     gwy_selection_get_max_objects(GwySelection *selection);
void     gwy_selection_set_max_objects(GwySelection *selection,
                                       gint max_objects);
void     gwy_selection_changed        (GwySelection *selection,
                                       gint i);
void     gwy_selection_finished       (GwySelection *selection);

G_END_DECLS

#endif /*__GWY_SELECTION_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
