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

#ifndef __GWY_EVALUATOR_H__
#define __GWY_EVALUATOR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_TYPE_EVALUATOR                  (gwy_evaluator_get_type())
#define GWY_EVALUATOR(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_EVALUATOR, GwyEvaluator))
#define GWY_EVALUATOR_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_EVALUATOR, GwyEvaluatorClass))
#define GWY_IS_EVALUATOR(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_EVALUATOR))
#define GWY_IS_EVALUATOR_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_EVALUATOR))
#define GWY_EVALUATOR_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_EVALUATOR, GwyEvaluatorClass))

typedef struct {
    gchar *id;
    gdouble xc;
    gdouble yc;
    gint x;
    gint y;
    gint width;
    gint height;
} SearchPointSettings;

typedef struct {
    gchar *id;
    gdouble xc;
    gdouble yc;
} PointSettings;

typedef struct {
    gchar *id;
    gdouble xc;
    gdouble yc;
} CorrelationPointSettings;

typedef struct {
    gchar *id;
    gdouble xstart;
    gdouble ystart;
    gdouble xend;
    gdouble yend;
    gdouble rho_min;
    gdouble theta_min;
    gdouble rho_max;
    gdouble theta_max;
} SearchLineSettings;

typedef struct {
    gchar *id;
    gdouble xstart;
    gdouble ystart;
    gdouble xend;
    gdouble yend;
} LineSettings;

typedef struct {
    gchar *id;
    SearchLineSettings line_1;
    SearchLineSettings line_2;
} SearchIntersectionSettings;

typedef struct {
    gchar *id;
    gchar *expression;
} ExpressionTask;

typedef struct _GwyEvaluator GwyEvaluator;
typedef struct _GwyEvaluatorClass GwyEvaluatorClass;

struct _GwyEvaluator {
    GObject parent_instance;

    GArray *detected_point_array;
    GArray *detected_line_array;
    GArray *fixed_point_array;
    GArray *fixed_line_array;
    GArray *correlation_point_array;
    GArray *expression_task_array;
    
    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyEvaluatorClass {
    GObjectClass parent_class;

    void (*layout_updated)(GwyEvaluator *gmodel);

    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
    gpointer reserved5;
};

#define gwy_evaluator_duplicate(gmodel) \
        (GWY_EVALUATOR(gwy_serializable_duplicate(G_OBJECT(gmodel))))

GType          gwy_evaluator_get_type                 (void) G_GNUC_CONST;
GwyEvaluator* gwy_evaluator_new                      (void);

static void     gwy_evaluator_finalize         (GObject *object);
static void     gwy_evaluator_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_evaluator_serialize     (GObject *obj,
                                                  GByteArray*buffer);
static GObject* gwy_evaluator_duplicate_real   (GObject *object);
static gsize    gwy_evaluator_get_size         (GObject *obj);
static GObject* gwy_evaluator_deserialize      (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);


G_DEFINE_TYPE_EXTENDED
    (GwyEvaluator, gwy_evaluator, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_evaluator_serializable_init))

static void
gwy_evaluator_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_evaluator_serialize;
    iface->deserialize = gwy_evaluator_deserialize;
    iface->get_size = gwy_evaluator_get_size;
    iface->duplicate = gwy_evaluator_duplicate_real;
}

static void
gwy_evaluator_class_init(GwyEvaluatorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_evaluator_finalize;
}

static void
gwy_evaluator_init(GwyEvaluator *gmodel)
{
}

GwyEvaluator*
gwy_evaluator_new(void)
{
    GwyEvaluator *evaluator;

    gwy_debug("");
    evaluator = g_object_new(GWY_TYPE_EVALUATOR, NULL);

    evaluator->detected_point_array = g_array_new (FALSE, FALSE,
                                                       sizeof (SearchPointSettings));
    evaluator->detected_line_array = g_array_new (FALSE, FALSE,
                                                       sizeof (SearchLineSettings));
    evaluator->fixed_point_array = g_array_new (FALSE, FALSE,
                                                       sizeof (PointSettings));
    evaluator->fixed_line_array = g_array_new (FALSE, FALSE,
                                                       sizeof (LineSettings));
    evaluator->correlation_point_array = g_array_new (FALSE, FALSE,
                                                       sizeof (CorrelationPointSettings));

    evaluator->expression_task_array = g_array_new(FALSE, FALSE,
                                                            sizeof (ExpressionTask));

    return evaluator;
}

static void
gwy_evaluator_finalize(GObject *object)
{
    GwyEvaluator *evaluator;
    gint i;

    gwy_debug("");

    evaluator = GWY_EVALUATOR(object);

    G_OBJECT_CLASS(gwy_evaluator_parent_class)->finalize(object);
}

static GByteArray*
gwy_evaluator_serialize(GObject *obj,
                          GByteArray *buffer)
{
    GwyEvaluator *evaluator;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_EVALUATOR(obj), NULL);

    evaluator = GWY_EVALUATOR(obj);
    {
    }
}

static gsize
gwy_evaluator_get_size(GObject *obj)
{
    GwyEvaluator *evaluator;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_EVALUATOR(obj), 0);

    evaluator = GWY_EVALUATOR(obj);
    {
    }
}

static GObject*
gwy_evaluator_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    GwyEvaluator *evaluator;

    g_return_val_if_fail(buffer, NULL);

    evaluator = gwy_evaluator_new();

    return (GObject*)evaluator;
}

static GObject*
gwy_evaluator_duplicate_real(GObject *object)
{
    GwyEvaluator *evaluator, *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_EVALUATOR(object), NULL);

    evaluator = GWY_EVALUATOR(object);
    duplicate = gwy_evaluator_new();

    return (GObject*)duplicate;
}



G_END_DECLS

#endif /* __GWY_EVALUATOR_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
