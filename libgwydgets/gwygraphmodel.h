/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_GRAPH_EPITOME_H__
#define __GWY_GRAPH_EPITOME_H__

#include <libgwydgets/gwygraph.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH_EPITOME                  (gwy_graph_epitome_get_type())
#define GWY_GRAPH_EPITOME(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_EPITOME, GwyGraphEpitome))
#define GWY_GRAPH_EPITOME_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_EPITOME, GwyGraphEpitomeClass))
#define GWY_IS_GRAPH_EPITOME(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_EPITOME))
#define GWY_IS_GRAPH_EPITOME_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_EPITOME))
#define GWY_GRAPH_EPITOME_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_EPITOME, GwyGraphEpitomeClass))


typedef struct _GwyGraphEpitome GwyGraphEpitome;
typedef struct _GwyGraphEpitomeClass GwyGraphEpitomeClass;

typedef struct {
    gint n;
    gdouble *xdata;
    gdouble *ydata;
    GwyGraphAreaCurveParams *params;

    /* TODO: add some reserved stuff */
} GwyGraphEpitomeCurve;

/* XXX: really NEVER access these fields directly. They don't have to reflect
 * the graph values! */
struct _GwyGraphEpitome {
    GObject parent_instance;

    GwyGraph *graph;
    gulong graph_destroy_hid;

    gdouble x_reqmax;
    gdouble x_reqmin;
    gdouble y_reqmax;
    gdouble y_reqmin;

    gboolean has_x_unit;
    gboolean has_y_unit;
    GString *x_unit;
    GString *y_unit;

    GString *top_label;
    GString *bottom_label;
    GString *left_label;
    GString *right_label;

    gint ncurves;
    GwyGraphEpitomeCurve *curves;

    /* TODO: add some reserved stuff */
};

struct _GwyGraphEpitomeClass {
    GObjectClass parent_class;

    void (*value_changed)(GwyGraphEpitome *graph_epitome);
};


GType      gwy_graph_epitome_get_type       (void) G_GNUC_CONST;
GObject*   gwy_graph_epitome_new            (GwyGraph *graph);

G_END_DECLS

#endif /* __GWY_GRAPH_EPITOME_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
