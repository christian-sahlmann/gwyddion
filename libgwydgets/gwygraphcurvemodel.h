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

#ifndef __GWY_GRAPH_CURVE_MODEL_H__
#define __GWY_GRAPH_CURVE_MODEL_H__

#include <libgwydgets/gwygraph.h>
#include <libdraw/gwyrgba.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH_CURVE_MODEL                  (gwy_graph_curve_model_get_type())
#define GWY_GRAPH_CURVE_MODEL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_CURVE_MODEL, GwyGraphCurveModel))
#define GWY_GRAPH_CURVE_MODEL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_CURVE_MODEL, GwyGraphCurveModelClass))
#define GWY_IS_GRAPH_CURVE_MODEL(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_CURVE_MODEL))
#define GWY_IS_GRAPH_CURVE_MODEL_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_CURVE_MODEL))
#define GWY_GRAPH_CURVE_MODEL_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_CURVE_MODEL, GwyGraphCurveModelClass))


typedef struct _GwyGraphCurveModel GwyGraphCurveModel;
typedef struct _GwyGraphCurveModelClass GwyGraphCurveModelClass;

/* XXX: really NEVER access these fields directly. They don't have to reflect
 * the graph values! */
struct _GwyGraphCurveModel {
    GObject parent_instance;

    /* data */
    gint n;
    gdouble *xdata;
    gdouble *ydata;

    /* like GwyGraphAreaCurveParams, but with proper field types */
    GString *description;
    GwyRGBA color;

    gboolean is_point;
    GwyGraphPointType point_type;
    gint point_size;

    gboolean is_line;
    GdkLineStyle line_style;
    gint line_size;

    /* reserved stuff */
    gint int1;
    gint int2;
    GwyGraphPointType enum1;
    GwyGraphPointType enum2;
    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

struct _GwyGraphCurveModelClass {
    GObjectClass parent_class;

    void (*value_changed)(GwyGraphCurveModel *gcmodel);  /* XXX: only formal */

    gpointer reserved1;
    gpointer reserved2;
};


GType      gwy_graph_curve_model_get_type       (void) G_GNUC_CONST;
GObject*   gwy_graph_curve_model_new            (void);
gboolean   gwy_graph_curve_model_save_curve     (GwyGraphCurveModel *gcmodel,
                                                 GwyGraph *graph,
                                                 gint index_);
void       gwy_graph_add_curve_from_model       (GwyGraph *graph,
                                                 GwyGraphCurveModel *gcmodel);

G_END_DECLS

#endif /* __GWY_GRAPH_CURVE_MODEL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
