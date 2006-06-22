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

#define GWY_TYPE_SEARCH_POINT                  (gwy_search_point_get_type())
#define GWY_SEARCH_POINT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SEARCH_POINT, GwySearchPoint))
#define GWY_SEARCH_POINT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SEARCH_POINT, GwySearchPointClass))
#define GWY_IS_SEARCH_POINT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SEARCH_POINT))
#define GWY_IS_SEARCH_POINT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SEARCH_POINT))
#define GWY_SEARCH_POINT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SEARCH_POINT, GwySearchPointClass))


typedef struct _GwySearchPoint GwySearchPoint;
typedef struct _GwySearchPointClass GwySearchPointClass;

struct _GwySearchPoint {
    GObject parent_instance;
        
    gchar *id;
    gdouble xc;
    gdouble yc;
    gint x;
    gint y;
    gint width;
    gint height;
};

struct _GwySearchPointClass {
    GObjectClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
};

#define gwy_search_point_duplicate(gmodel) \
        (GWY_SEARCH_POINT(gwy_serializable_duplicate(G_OBJECT(gmodel))))
#define GWY_SEARCH_POINT_TYPE_NAME "GwySearchPoint"

GType          gwy_search_point_get_type                 (void) G_GNUC_CONST;
GwySearchPoint* gwy_search_point_new                      (void);

static void     gwy_search_point_finalize         (GObject *object);
static void     gwy_search_point_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_search_point_serialize     (GObject *obj,
                                                  GByteArray*buffer);
static GObject* gwy_search_point_duplicate_real   (GObject *object);
static gsize    gwy_search_point_get_size         (GObject *obj);
static GObject* gwy_search_point_deserialize      (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);


G_DEFINE_TYPE_EXTENDED
    (GwySearchPoint, gwy_search_point, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_search_point_serializable_init))

static void
gwy_search_point_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_search_point_serialize;
    iface->deserialize = gwy_search_point_deserialize;
    iface->get_size = gwy_search_point_get_size;
    iface->duplicate = gwy_search_point_duplicate_real;
}

static void
gwy_search_point_class_init(GwySearchPointClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_search_point_finalize;
}

static void
gwy_search_point_init(GwySearchPoint *search_point)
{
    search_point->xc = 0;
    search_point->yc = 0;
}

GwySearchPoint*
gwy_search_point_new(void)
{
    GwySearchPoint *search_point;

    gwy_debug("");
        search_point = g_object_new(GWY_TYPE_SEARCH_POINT, NULL);

    return search_point;
}

static void
gwy_search_point_finalize(GObject *object)
{
    GwySearchPoint *search_point;

    gwy_debug("");

    search_point = GWY_SEARCH_POINT(object);

    G_OBJECT_CLASS(gwy_search_point_parent_class)->finalize(object);
}

static GByteArray*
gwy_search_point_serialize(GObject *obj,
                          GByteArray *buffer)
{
    GwySearchPoint *search_point;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SEARCH_POINT(obj), NULL);

    search_point = GWY_SEARCH_POINT(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &search_point->id, NULL },
            { 'd', "xc", &search_point->xc, NULL },
            { 'd', "yc", &search_point->yc, NULL },
            { 'i', "x", &search_point->x, NULL },
            { 'i', "y", &search_point->y, NULL },
            { 'i', "width", &search_point->width, NULL },
            { 'i', "height", &search_point->height, NULL },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_SEARCH_POINT_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_search_point_get_size(GObject *obj)
{
    GwySearchPoint *search_point;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SEARCH_POINT(obj), 0);

    search_point = GWY_SEARCH_POINT(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &search_point->id, NULL },
            { 'd', "xc", &search_point->xc, NULL },
            { 'd', "yc", &search_point->yc, NULL },
            { 'i', "x", &search_point->x, NULL },
            { 'i', "y", &search_point->y, NULL },
            { 'i', "width", &search_point->width, NULL },
            { 'i', "height", &search_point->height, NULL },
        };
        return gwy_serialize_get_struct_size(GWY_SEARCH_POINT_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
     }
}

static GObject*
gwy_search_point_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    GwySearchPoint *search_point;

    g_return_val_if_fail(buffer, NULL);

    search_point = gwy_search_point_new();
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &search_point->id, NULL },
            { 'd', "xc", &search_point->xc, NULL },
            { 'd', "yc", &search_point->yc, NULL },
            { 'i', "x", &search_point->x, NULL },
            { 'i', "y", &search_point->y, NULL },
            { 'i', "width", &search_point->width, NULL },
            { 'i', "height", &search_point->height, NULL },
        };
        gwy_serialize_unpack_object_struct(buffer, size, position,
                                           GWY_SEARCH_POINT_TYPE_NAME,
                                           G_N_ELEMENTS(spec), spec);
    }
    
    return (GObject*)search_point;
}

static GObject*
gwy_search_point_duplicate_real(GObject *object)
{
    GwySearchPoint *search_point, *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SEARCH_POINT(object), NULL);

    search_point = GWY_SEARCH_POINT(object);
    duplicate = gwy_search_point_new();
    
    duplicate->id = g_strdup(search_point->id);
    duplicate->xc = search_point->xc;
    duplicate->yc = search_point->yc;
    duplicate->x = search_point->x;
    duplicate->y = search_point->y;
    duplicate->width = search_point->width;
    duplicate->height = search_point->height;

    return (GObject*)duplicate;
}


#define GWY_TYPE_FIXED_POINT                  (gwy_fixed_point_get_type())
#define GWY_FIXED_POINT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FIXED_POINT, GwyFixedPoint))
#define GWY_FIXED_POINT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FIXED_POINT, GwyFixedPointClass))
#define GWY_IS_FIXED_POINT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FIXED_POINT))
#define GWY_IS_FIXED_POINT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FIXED_POINT))
#define GWY_FIXED_POINT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FIXED_POINT, GwyFixedPointClass))


typedef struct _GwyFixedPoint GwyFixedPoint;
typedef struct _GwyFixedPointClass GwyFixedPointClass;

struct _GwyFixedPoint {
    GObject parent_instance;
        
    gchar *id;
    gdouble xc;
    gdouble yc;
};

struct _GwyFixedPointClass {
    GObjectClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
};

#define gwy_fixed_point_duplicate(gmodel) \
        (GWY_FIXED_POINT(gwy_serializable_duplicate(G_OBJECT(gmodel))))
#define GWY_FIXED_POINT_TYPE_NAME "GwyFixedPoint"

GType          gwy_fixed_point_get_type                 (void) G_GNUC_CONST;
GwyFixedPoint* gwy_fixed_point_new                      (void);

static void     gwy_fixed_point_finalize         (GObject *object);
static void     gwy_fixed_point_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_fixed_point_serialize     (GObject *obj,
                                                  GByteArray*buffer);
static GObject* gwy_fixed_point_duplicate_real   (GObject *object);
static gsize    gwy_fixed_point_get_size         (GObject *obj);
static GObject* gwy_fixed_point_deserialize      (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);


G_DEFINE_TYPE_EXTENDED
    (GwyFixedPoint, gwy_fixed_point, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_fixed_point_serializable_init))

static void
gwy_fixed_point_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_fixed_point_serialize;
    iface->deserialize = gwy_fixed_point_deserialize;
    iface->get_size = gwy_fixed_point_get_size;
    iface->duplicate = gwy_fixed_point_duplicate_real;
}

static void
gwy_fixed_point_class_init(GwyFixedPointClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_fixed_point_finalize;
}

static void
gwy_fixed_point_init(GwyFixedPoint *fixed_point)
{
    fixed_point->xc = 0;
    fixed_point->yc = 0;
}

GwyFixedPoint*
gwy_fixed_point_new(void)
{
    GwyFixedPoint *fixed_point;

    gwy_debug("");
        fixed_point = g_object_new(GWY_TYPE_FIXED_POINT, NULL);

    return fixed_point;
}

static void
gwy_fixed_point_finalize(GObject *object)
{
    GwyFixedPoint *fixed_point;

    gwy_debug("");

    fixed_point = GWY_FIXED_POINT(object);

    G_OBJECT_CLASS(gwy_fixed_point_parent_class)->finalize(object);
}

static GByteArray*
gwy_fixed_point_serialize(GObject *obj,
                          GByteArray *buffer)
{
    GwyFixedPoint *fixed_point;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_FIXED_POINT(obj), NULL);

    fixed_point = GWY_FIXED_POINT(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &fixed_point->id, NULL },
            { 'd', "xc", &fixed_point->xc, NULL },
            { 'd', "yc", &fixed_point->yc, NULL },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_FIXED_POINT_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_fixed_point_get_size(GObject *obj)
{
    GwyFixedPoint *fixed_point;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_FIXED_POINT(obj), 0);

    fixed_point = GWY_FIXED_POINT(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &fixed_point->id, NULL },
            { 'd', "xc", &fixed_point->xc, NULL },
            { 'd', "yc", &fixed_point->yc, NULL },
        };
        return gwy_serialize_get_struct_size(GWY_FIXED_POINT_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
     }
}

static GObject*
gwy_fixed_point_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    GwyFixedPoint *fixed_point;

    g_return_val_if_fail(buffer, NULL);

    fixed_point = gwy_fixed_point_new();
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &fixed_point->id, NULL },
            { 'd', "xc", &fixed_point->xc, NULL },
            { 'd', "yc", &fixed_point->yc, NULL },
        };
        gwy_serialize_unpack_object_struct(buffer, size, position,
                                           GWY_FIXED_POINT_TYPE_NAME,
                                           G_N_ELEMENTS(spec), spec);
    }
    
    return (GObject*)fixed_point;
}

static GObject*
gwy_fixed_point_duplicate_real(GObject *object)
{
    GwyFixedPoint *fixed_point, *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_FIXED_POINT(object), NULL);

    fixed_point = GWY_FIXED_POINT(object);
    duplicate = gwy_fixed_point_new();
    
    duplicate->id = g_strdup(fixed_point->id);
    duplicate->xc = fixed_point->xc;
    duplicate->yc = fixed_point->yc;

    return (GObject*)duplicate;
}

#define GWY_TYPE_CORRELATION_POINT                  (gwy_correlation_point_get_type())
#define GWY_CORRELATION_POINT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CORRELATION_POINT, GwyCorrelationPoint))
#define GWY_CORRELATION_POINT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CORRELATION_POINT, GwyCorrelationPointClass))
#define GWY_IS_CORRELATION_POINT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CORRELATION_POINT))
#define GWY_IS_CORRELATION_POINT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CORRELATION_POINT))
#define GWY_CORRELATION_POINT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CORRELATION_POINT, GwyCorrelationPointClass))


typedef struct _GwyCorrelationPoint GwyCorrelationPoint;
typedef struct _GwyCorrelationPointClass GwyCorrelationPointClass;

struct _GwyCorrelationPoint {
    GObject parent_instance;
        
    gchar *id;
    gdouble xc;
    gdouble yc;
};

struct _GwyCorrelationPointClass {
    GObjectClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
};

#define gwy_correlation_point_duplicate(gmodel) \
        (GWY_CORRELATION_POINT(gwy_serializable_duplicate(G_OBJECT(gmodel))))
#define GWY_CORRELATION_POINT_TYPE_NAME "GwyCorrelationPoint"

GType          gwy_correlation_point_get_type                 (void) G_GNUC_CONST;
GwyCorrelationPoint* gwy_correlation_point_new                      (void);

static void     gwy_correlation_point_finalize         (GObject *object);
static void     gwy_correlation_point_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_correlation_point_serialize     (GObject *obj,
                                                  GByteArray*buffer);
static GObject* gwy_correlation_point_duplicate_real   (GObject *object);
static gsize    gwy_correlation_point_get_size         (GObject *obj);
static GObject* gwy_correlation_point_deserialize      (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);


G_DEFINE_TYPE_EXTENDED
    (GwyCorrelationPoint, gwy_correlation_point, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_correlation_point_serializable_init))

static void
gwy_correlation_point_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_correlation_point_serialize;
    iface->deserialize = gwy_correlation_point_deserialize;
    iface->get_size = gwy_correlation_point_get_size;
    iface->duplicate = gwy_correlation_point_duplicate_real;
}

static void
gwy_correlation_point_class_init(GwyCorrelationPointClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_correlation_point_finalize;
}

static void
gwy_correlation_point_init(GwyCorrelationPoint *correlation_point)
{
    correlation_point->xc = 0;
    correlation_point->yc = 0;
}

GwyCorrelationPoint*
gwy_correlation_point_new(void)
{
    GwyCorrelationPoint *correlation_point;

    gwy_debug("");
        correlation_point = g_object_new(GWY_TYPE_CORRELATION_POINT, NULL);

    return correlation_point;
}

static void
gwy_correlation_point_finalize(GObject *object)
{
    GwyCorrelationPoint *correlation_point;

    gwy_debug("");

    correlation_point = GWY_CORRELATION_POINT(object);

    G_OBJECT_CLASS(gwy_correlation_point_parent_class)->finalize(object);
}

static GByteArray*
gwy_correlation_point_serialize(GObject *obj,
                          GByteArray *buffer)
{
    GwyCorrelationPoint *correlation_point;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CORRELATION_POINT(obj), NULL);

    correlation_point = GWY_CORRELATION_POINT(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &correlation_point->id, NULL },
            { 'd', "xc", &correlation_point->xc, NULL },
            { 'd', "yc", &correlation_point->yc, NULL },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_CORRELATION_POINT_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_correlation_point_get_size(GObject *obj)
{
    GwyCorrelationPoint *correlation_point;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CORRELATION_POINT(obj), 0);

    correlation_point = GWY_CORRELATION_POINT(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &correlation_point->id, NULL },
            { 'd', "xc", &correlation_point->xc, NULL },
            { 'd', "yc", &correlation_point->yc, NULL },
        };
        return gwy_serialize_get_struct_size(GWY_CORRELATION_POINT_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
     }
}

static GObject*
gwy_correlation_point_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    GwyCorrelationPoint *correlation_point;

    g_return_val_if_fail(buffer, NULL);

    correlation_point = gwy_correlation_point_new();
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &correlation_point->id, NULL },
            { 'd', "xc", &correlation_point->xc, NULL },
            { 'd', "yc", &correlation_point->yc, NULL },
        };
        gwy_serialize_unpack_object_struct(buffer, size, position,
                                           GWY_CORRELATION_POINT_TYPE_NAME,
                                           G_N_ELEMENTS(spec), spec);
    }
    
    return (GObject*)correlation_point;
}

static GObject*
gwy_correlation_point_duplicate_real(GObject *object)
{
    GwyCorrelationPoint *correlation_point, *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CORRELATION_POINT(object), NULL);

    correlation_point = GWY_CORRELATION_POINT(object);
    duplicate = gwy_correlation_point_new();
    
    duplicate->id = g_strdup(correlation_point->id);
    duplicate->xc = correlation_point->xc;
    duplicate->yc = correlation_point->yc;

    return (GObject*)duplicate;
}



#define GWY_TYPE_SEARCH_LINE                  (gwy_search_line_get_type())
#define GWY_SEARCH_LINE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SEARCH_LINE, GwySearchLine))
#define GWY_SEARCH_LINE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SEARCH_LINE, GwySearchLineClass))
#define GWY_IS_SEARCH_LINE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SEARCH_LINE))
#define GWY_IS_SEARCH_LINE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SEARCH_LINE))
#define GWY_SEARCH_LINE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SEARCH_LINE, GwySearchLineClass))


typedef struct _GwySearchLine GwySearchLine;
typedef struct _GwySearchLineClass GwySearchLineClass;

struct _GwySearchLine {
    GObject parent_instance;
        
    gchar *id;
    gdouble xstart;
    gdouble ystart;
    gdouble xend;
    gdouble yend;
    gdouble rho_min;
    gdouble theta_min;
    gdouble rho_max;
    gdouble theta_max;
};

struct _GwySearchLineClass {
    GObjectClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
};

#define gwy_search_line_duplicate(gmodel) \
        (GWY_SEARCH_LINE(gwy_serializable_duplicate(G_OBJECT(gmodel))))
#define GWY_SEARCH_LINE_TYPE_NAME "GwySearchLine"

GType          gwy_search_line_get_type                 (void) G_GNUC_CONST;
GwySearchLine* gwy_search_line_new                      (void);

static void     gwy_search_line_finalize         (GObject *object);
static void     gwy_search_line_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_search_line_serialize     (GObject *obj,
                                                  GByteArray*buffer);
static GObject* gwy_search_line_duplicate_real   (GObject *object);
static gsize    gwy_search_line_get_size         (GObject *obj);
static GObject* gwy_search_line_deserialize      (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);


G_DEFINE_TYPE_EXTENDED
    (GwySearchLine, gwy_search_line, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_search_line_serializable_init))

static void
gwy_search_line_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_search_line_serialize;
    iface->deserialize = gwy_search_line_deserialize;
    iface->get_size = gwy_search_line_get_size;
    iface->duplicate = gwy_search_line_duplicate_real;
}

static void
gwy_search_line_class_init(GwySearchLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_search_line_finalize;
}

static void
gwy_search_line_init(GwySearchLine *search_line)
{
    search_line->xstart = 0;
    search_line->ystart = 0;
    search_line->xend = 0;
    search_line->yend = 0;
}

GwySearchLine*
gwy_search_line_new(void)
{
    GwySearchLine *search_line;

    gwy_debug("");
        search_line = g_object_new(GWY_TYPE_SEARCH_LINE, NULL);

    return search_line;
}

static void
gwy_search_line_finalize(GObject *object)
{
    GwySearchLine *search_line;

    gwy_debug("");

    search_line = GWY_SEARCH_LINE(object);

    G_OBJECT_CLASS(gwy_search_line_parent_class)->finalize(object);
}

static GByteArray*
gwy_search_line_serialize(GObject *obj,
                          GByteArray *buffer)
{
    GwySearchLine *search_line;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SEARCH_LINE(obj), NULL);

    search_line = GWY_SEARCH_LINE(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &search_line->id, NULL },
            { 'd', "xstart", &search_line->xstart, NULL },
            { 'd', "ystart", &search_line->ystart, NULL },
            { 'd', "xend", &search_line->xend, NULL },
            { 'd', "yend", &search_line->yend, NULL },
            { 'd', "rho_min", &search_line->rho_min, NULL },
            { 'd', "theta_min", &search_line->theta_min, NULL },
            { 'd', "rho_max", &search_line->rho_max, NULL },
            { 'd', "theta_max", &search_line->theta_max, NULL },
         };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_SEARCH_LINE_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_search_line_get_size(GObject *obj)
{
    GwySearchLine *search_line;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SEARCH_LINE(obj), 0);

    search_line = GWY_SEARCH_LINE(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &search_line->id, NULL },
            { 'd', "xstart", &search_line->xstart, NULL },
            { 'd', "ystart", &search_line->ystart, NULL },
            { 'd', "xend", &search_line->xend, NULL },
            { 'd', "yend", &search_line->yend, NULL },
            { 'd', "rho_min", &search_line->rho_min, NULL },
            { 'd', "theta_min", &search_line->theta_min, NULL },
            { 'd', "rho_max", &search_line->rho_max, NULL },
            { 'd', "theta_max", &search_line->theta_max, NULL },
         };
        return gwy_serialize_get_struct_size(GWY_SEARCH_LINE_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
     }
}

static GObject*
gwy_search_line_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    GwySearchLine *search_line;

    g_return_val_if_fail(buffer, NULL);

    search_line = gwy_search_line_new();
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &search_line->id, NULL },
            { 'd', "xstart", &search_line->xstart, NULL },
            { 'd', "ystart", &search_line->ystart, NULL },
            { 'd', "xend", &search_line->xend, NULL },
            { 'd', "yend", &search_line->yend, NULL },
            { 'd', "rho_min", &search_line->rho_min, NULL },
            { 'd', "theta_min", &search_line->theta_min, NULL },
            { 'd', "rho_max", &search_line->rho_max, NULL },
            { 'd', "theta_max", &search_line->theta_max, NULL },
         };
        gwy_serialize_unpack_object_struct(buffer, size, position,
                                           GWY_SEARCH_LINE_TYPE_NAME,
                                           G_N_ELEMENTS(spec), spec);
    }
    
    return (GObject*)search_line;
}

static GObject*
gwy_search_line_duplicate_real(GObject *object)
{
    GwySearchLine *search_line, *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SEARCH_LINE(object), NULL);

    search_line = GWY_SEARCH_LINE(object);
    duplicate = gwy_search_line_new();
    
    duplicate->id = g_strdup(search_line->id);
    duplicate->xstart = search_line->xstart;
    duplicate->ystart = search_line->ystart;
    duplicate->xend = search_line->xend;
    duplicate->yend = search_line->yend;
    duplicate->rho_min = search_line->rho_min;
    duplicate->theta_min = search_line->theta_min;
    duplicate->rho_max = search_line->rho_max;
    duplicate->theta_max = search_line->theta_max;

    return (GObject*)duplicate;
}


#define GWY_TYPE_FIXED_LINE                  (gwy_fixed_line_get_type())
#define GWY_FIXED_LINE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FIXED_LINE, GwyFixedLine))
#define GWY_FIXED_LINE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FIXED_LINE, GwyFixedLineClass))
#define GWY_IS_FIXED_LINE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FIXED_LINE))
#define GWY_IS_FIXED_LINE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FIXED_LINE))
#define GWY_FIXED_LINE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FIXED_LINE, GwyFixedLineClass))


typedef struct _GwyFixedLine GwyFixedLine;
typedef struct _GwyFixedLineClass GwyFixedLineClass;

struct _GwyFixedLine {
    GObject parent_instance;
        
    gchar *id;
    gdouble xstart;
    gdouble ystart;
    gdouble xend;
    gdouble yend;
};

struct _GwyFixedLineClass {
    GObjectClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
};

#define gwy_fixed_line_duplicate(gmodel) \
        (GWY_FIXED_LINE(gwy_serializable_duplicate(G_OBJECT(gmodel))))
#define GWY_FIXED_LINE_TYPE_NAME "GwyFixedLine"

GType          gwy_fixed_line_get_type                 (void) G_GNUC_CONST;
GwyFixedLine* gwy_fixed_line_new                      (void);

static void     gwy_fixed_line_finalize         (GObject *object);
static void     gwy_fixed_line_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_fixed_line_serialize     (GObject *obj,
                                                  GByteArray*buffer);
static GObject* gwy_fixed_line_duplicate_real   (GObject *object);
static gsize    gwy_fixed_line_get_size         (GObject *obj);
static GObject* gwy_fixed_line_deserialize      (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);


G_DEFINE_TYPE_EXTENDED
    (GwyFixedLine, gwy_fixed_line, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_fixed_line_serializable_init))

static void
gwy_fixed_line_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_fixed_line_serialize;
    iface->deserialize = gwy_fixed_line_deserialize;
    iface->get_size = gwy_fixed_line_get_size;
    iface->duplicate = gwy_fixed_line_duplicate_real;
}

static void
gwy_fixed_line_class_init(GwyFixedLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_fixed_line_finalize;
}

static void
gwy_fixed_line_init(GwyFixedLine *fixed_line)
{
    fixed_line->xstart = 0;
    fixed_line->ystart = 0;
    fixed_line->xend = 0;
    fixed_line->yend = 0;
}

GwyFixedLine*
gwy_fixed_line_new(void)
{
    GwyFixedLine *fixed_line;

    gwy_debug("");
        fixed_line = g_object_new(GWY_TYPE_FIXED_LINE, NULL);

    return fixed_line;
}

static void
gwy_fixed_line_finalize(GObject *object)
{
    GwyFixedLine *fixed_line;

    gwy_debug("");

    fixed_line = GWY_FIXED_LINE(object);

    G_OBJECT_CLASS(gwy_fixed_line_parent_class)->finalize(object);
}

static GByteArray*
gwy_fixed_line_serialize(GObject *obj,
                          GByteArray *buffer)
{
    GwyFixedLine *fixed_line;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_FIXED_LINE(obj), NULL);

    fixed_line = GWY_FIXED_LINE(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &fixed_line->id, NULL },
            { 'd', "xstart", &fixed_line->xstart, NULL },
            { 'd', "ystart", &fixed_line->ystart, NULL },
            { 'd', "xend", &fixed_line->xend, NULL },
            { 'd', "yend", &fixed_line->yend, NULL },
         };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_FIXED_LINE_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_fixed_line_get_size(GObject *obj)
{
    GwyFixedLine *fixed_line;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_FIXED_LINE(obj), 0);

    fixed_line = GWY_FIXED_LINE(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &fixed_line->id, NULL },
            { 'd', "xstart", &fixed_line->xstart, NULL },
            { 'd', "ystart", &fixed_line->ystart, NULL },
            { 'd', "xend", &fixed_line->xend, NULL },
            { 'd', "yend", &fixed_line->yend, NULL },
         };
        return gwy_serialize_get_struct_size(GWY_FIXED_LINE_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
     }
}

static GObject*
gwy_fixed_line_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    GwyFixedLine *fixed_line;

    g_return_val_if_fail(buffer, NULL);

    fixed_line = gwy_fixed_line_new();
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &fixed_line->id, NULL },
            { 'd', "xstart", &fixed_line->xstart, NULL },
            { 'd', "ystart", &fixed_line->ystart, NULL },
            { 'd', "xend", &fixed_line->xend, NULL },
            { 'd', "yend", &fixed_line->yend, NULL },
         };
        gwy_serialize_unpack_object_struct(buffer, size, position,
                                           GWY_FIXED_LINE_TYPE_NAME,
                                           G_N_ELEMENTS(spec), spec);
    }
    
    return (GObject*)fixed_line;
}

static GObject*
gwy_fixed_line_duplicate_real(GObject *object)
{
    GwyFixedLine *fixed_line, *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_FIXED_LINE(object), NULL);

    fixed_line = GWY_FIXED_LINE(object);
    duplicate = gwy_fixed_line_new();
    
    duplicate->id = g_strdup(fixed_line->id);
    duplicate->xstart = fixed_line->xstart;
    duplicate->ystart = fixed_line->ystart;
    duplicate->xend = fixed_line->xend;
    duplicate->yend = fixed_line->yend;

    return (GObject*)duplicate;
}



#define GWY_TYPE_EVALUATOR_TASK                  (gwy_evaluator_task_get_type())
#define GWY_EVALUATOR_TASK(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_EVALUATOR_TASK, GwyEvaluatorTask))
#define GWY_EVALUATOR_TASK_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_EVALUATOR_TASK, GwyEvaluatorTaskClass))
#define GWY_IS_EVALUATOR_TASK(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_EVALUATOR_TASK))
#define GWY_IS_EVALUATOR_TASK_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_EVALUATOR_TASK))
#define GWY_EVALUATOR_TASK_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_EVALUATOR_TASK, GwyEvaluatorTaskClass))


typedef struct _GwyEvaluatorTask GwyEvaluatorTask;
typedef struct _GwyEvaluatorTaskClass GwyEvaluatorTaskClass;

struct _GwyEvaluatorTask {
    GObject parent_instance;
        
    gchar *id;
    gchar *expression;
    gchar *description;
    gdouble threshold;
};

struct _GwyEvaluatorTaskClass {
    GObjectClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
};

#define gwy_evaluator_task_duplicate(gmodel) \
        (GWY_EVALUATOR_TASK(gwy_serializable_duplicate(G_OBJECT(gmodel))))
#define GWY_EVALUATOR_TASK_TYPE_NAME "GwyEvaluatorTask"

GType          gwy_evaluator_task_get_type                 (void) G_GNUC_CONST;
GwyEvaluatorTask* gwy_evaluator_task_new                      (void);

static void     gwy_evaluator_task_finalize         (GObject *object);
static void     gwy_evaluator_task_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_evaluator_task_serialize     (GObject *obj,
                                                  GByteArray*buffer);
static GObject* gwy_evaluator_task_duplicate_real   (GObject *object);
static gsize    gwy_evaluator_task_get_size         (GObject *obj);
static GObject* gwy_evaluator_task_deserialize      (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);


G_DEFINE_TYPE_EXTENDED
    (GwyEvaluatorTask, gwy_evaluator_task, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_evaluator_task_serializable_init))

static void
gwy_evaluator_task_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_evaluator_task_serialize;
    iface->deserialize = gwy_evaluator_task_deserialize;
    iface->get_size = gwy_evaluator_task_get_size;
    iface->duplicate = gwy_evaluator_task_duplicate_real;
}

static void
gwy_evaluator_task_class_init(GwyEvaluatorTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_evaluator_task_finalize;
}

static void
gwy_evaluator_task_init(GwyEvaluatorTask *evaluator_task)
{
    evaluator_task->threshold = 0;
}

GwyEvaluatorTask*
gwy_evaluator_task_new(void)
{
    GwyEvaluatorTask *evaluator_task;

    gwy_debug("");
        evaluator_task = g_object_new(GWY_TYPE_EVALUATOR_TASK, NULL);

    return evaluator_task;
}

static void
gwy_evaluator_task_finalize(GObject *object)
{
    GwyEvaluatorTask *evaluator_task;

    gwy_debug("");

    evaluator_task = GWY_EVALUATOR_TASK(object);

    G_OBJECT_CLASS(gwy_evaluator_task_parent_class)->finalize(object);
}

static GByteArray*
gwy_evaluator_task_serialize(GObject *obj,
                          GByteArray *buffer)
{
    GwyEvaluatorTask *evaluator_task;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_EVALUATOR_TASK(obj), NULL);

    evaluator_task = GWY_EVALUATOR_TASK(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &evaluator_task->id, NULL },
            { 's', "expression", &evaluator_task->expression, NULL },
            { 's', "description", &evaluator_task->description, NULL },
            { 'd', "threshold", &evaluator_task->threshold, NULL },
         };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_EVALUATOR_TASK_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_evaluator_task_get_size(GObject *obj)
{
    GwyEvaluatorTask *evaluator_task;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_EVALUATOR_TASK(obj), 0);

    evaluator_task = GWY_EVALUATOR_TASK(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &evaluator_task->id, NULL },
            { 's', "expression", &evaluator_task->expression, NULL },
            { 's', "description", &evaluator_task->description, NULL },
            { 'd', "threshold", &evaluator_task->threshold, NULL },
         };
        return gwy_serialize_get_struct_size(GWY_EVALUATOR_TASK_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
     }
}

static GObject*
gwy_evaluator_task_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    GwyEvaluatorTask *evaluator_task;

    g_return_val_if_fail(buffer, NULL);

    evaluator_task = gwy_evaluator_task_new();
    {
        GwySerializeSpec spec[] = {
            { 's', "id", &evaluator_task->id, NULL },
            { 's', "expression", &evaluator_task->expression, NULL },
            { 's', "description", &evaluator_task->description, NULL },
            { 'd', "threshold", &evaluator_task->threshold, NULL },
         };
        gwy_serialize_unpack_object_struct(buffer, size, position,
                                           GWY_EVALUATOR_TASK_TYPE_NAME,
                                           G_N_ELEMENTS(spec), spec);
    }
    
    return (GObject*)evaluator_task;
}

static GObject*
gwy_evaluator_task_duplicate_real(GObject *object)
{
    GwyEvaluatorTask *evaluator_task, *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_EVALUATOR_TASK(object), NULL);

    evaluator_task = GWY_EVALUATOR_TASK(object);
    duplicate = gwy_evaluator_task_new();
    
    duplicate->id = g_strdup(evaluator_task->id);
    duplicate->expression = g_strdup(evaluator_task->expression);
    duplicate->description = g_strdup(evaluator_task->description);
    duplicate->threshold = evaluator_task->threshold;

    return (GObject*)duplicate;
}






#define GWY_TYPE_EVALUATOR                  (gwy_evaluator_get_type())
#define GWY_EVALUATOR(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_EVALUATOR, GwyEvaluator))
#define GWY_EVALUATOR_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_EVALUATOR, GwyEvaluatorClass))
#define GWY_IS_EVALUATOR(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_EVALUATOR))
#define GWY_IS_EVALUATOR_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_EVALUATOR))
#define GWY_EVALUATOR_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_EVALUATOR, GwyEvaluatorClass))


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
#define GWY_EVALUATOR_TYPE_NAME "GwyEvaluator"

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
gwy_evaluator_init(GwyEvaluator *evaluator)
{
}

GwyEvaluator*
gwy_evaluator_new(void)
{
    GwyEvaluator *evaluator;

    gwy_debug("");
    evaluator = g_object_new(GWY_TYPE_EVALUATOR, NULL);

    evaluator->detected_point_array = g_array_new (FALSE, FALSE,
                                                       sizeof (GwySearchPoint));
    evaluator->detected_line_array = g_array_new (FALSE, FALSE,
                                                       sizeof (GwySearchLine));
    evaluator->fixed_point_array = g_array_new (FALSE, FALSE,
                                                       sizeof (GwyFixedPoint));
    evaluator->fixed_line_array = g_array_new (FALSE, FALSE,
                                                       sizeof (GwyFixedLine));
    evaluator->correlation_point_array = g_array_new (FALSE, FALSE,
                                                       sizeof (GwyCorrelationPoint));

    evaluator->expression_task_array = g_array_new(FALSE, FALSE,
                                                            sizeof (GwyEvaluatorTask));

    return evaluator;
}

static void
gwy_evaluator_finalize(GObject *object)
{
    GwyEvaluator *evaluator;

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
        GwySerializeSpec spec[] = {
            { 'O', "detected_points", &evaluator->detected_point_array, &evaluator->detected_point_array->len },
            { 'O', "detected_lines", &evaluator->detected_line_array, &evaluator->detected_line_array->len },
            { 'O', "fixed_points", &evaluator->fixed_point_array, &evaluator->fixed_point_array->len },
            { 'O', "fixed_lines", &evaluator->fixed_line_array, &evaluator->fixed_line_array->len },
            { 'O', "correlation_points", &evaluator->correlation_point_array, &evaluator->correlation_point_array->len },
            { 'O', "evaluator_tasks", &evaluator->expression_task_array, &evaluator->expression_task_array->len },
         };
        return gwy_serialize_pack_object_struct(buffer, GWY_EVALUATOR_TYPE_NAME,
                                                   G_N_ELEMENTS(spec), spec);
        
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
        GwySerializeSpec spec[] = {
            { 'O', "detected_points", &evaluator->detected_point_array, &evaluator->detected_point_array->len },
            { 'O', "detected_lines", &evaluator->detected_line_array, &evaluator->detected_line_array->len },
            { 'O', "fixed_points", &evaluator->fixed_point_array, &evaluator->fixed_point_array->len },
            { 'O', "fixed_lines", &evaluator->fixed_line_array, &evaluator->fixed_line_array->len },
            { 'O', "correlation_points", &evaluator->correlation_point_array, &evaluator->correlation_point_array->len },
            { 'O', "evaluator_tasks", &evaluator->expression_task_array, &evaluator->expression_task_array->len },
         };
        return gwy_serialize_get_struct_size(GWY_EVALUATOR_TYPE_NAME,
                                                   G_N_ELEMENTS(spec), spec);
        
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
    {
        GwySerializeSpec spec[] = {
            { 'O', "detected_points", &evaluator->detected_point_array, &evaluator->detected_point_array->len },
            { 'O', "detected_lines", &evaluator->detected_line_array, &evaluator->detected_line_array->len },
            { 'O', "fixed_points", &evaluator->fixed_point_array, &evaluator->fixed_point_array->len },
            { 'O', "fixed_lines", &evaluator->fixed_line_array, &evaluator->fixed_line_array->len },
            { 'O', "correlation_points", &evaluator->correlation_point_array, &evaluator->correlation_point_array->len },
            { 'O', "evaluator_tasks", &evaluator->expression_task_array, &evaluator->expression_task_array->len },
         };
        gwy_serialize_unpack_object_struct(buffer, size, position,
                                           GWY_EVALUATOR_TYPE_NAME,
                                           G_N_ELEMENTS(spec), spec);
        
     }

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
