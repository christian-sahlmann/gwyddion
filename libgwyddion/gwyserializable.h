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

#ifndef __GWY_SERIALIZABLE_H__
#define __GWY_SERIALIZABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_SERIALIZABLE           (gwy_serializable_get_type())
#define GWY_SERIALIZABLE(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SERIALIZABLE, GwySerializable))
#define GWY_IS_SERIALIZABLE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SERIALIZABLE))
#define GWY_SERIALIZABLE_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), GWY_TYPE_SERIALIZABLE, GwySerializableIface))

#define GWY_IMPLEMENT_SERIALIZABLE(iface_init) \
    { \
        static const GInterfaceInfo gwy_serializable_iface_info = { \
            (GInterfaceInitFunc)iface_init, NULL, NULL \
        }; \
        g_type_add_interface_static(g_define_type_id, \
                                    GWY_TYPE_SERIALIZABLE, \
                                    &gwy_serializable_iface_info); \
    }

typedef struct _GwySerializableIface GwySerializableIface;
typedef struct _GwySerializable      GwySerializable;        /* dummy */

typedef GByteArray* (*GwySerializeFunc)(GObject *serializable,
                                        GByteArray *buffer);
typedef GObject* (*GwyDeserializeFunc)(const guchar *buffer,
                                       gsize size,
                                       gsize *position);

struct _GwySerializableIface {
    GTypeInterface parent_class;

    GwySerializeFunc serialize;
    GwyDeserializeFunc deserialize;
    void (*clone)(GObject *source, GObject *copy);
    GObject* (*duplicate)(GObject *object);
};

typedef struct {
    guchar ctype;
    const guchar *name;
    gpointer value;
    guint32 *array_size;
} GwySerializeSpec;

typedef union {
    gboolean v_boolean;
    guchar v_char;
    guint32 v_int32;
    guint64 v_int64;
    gdouble v_double;
    guchar *v_string;
    GObject *v_object;
    gboolean *v_boolean_array;
    guchar *v_char_array;
    guint32 *v_int32_array;
    guint64 *v_int64_array;
    gdouble *v_double_array;
    guchar **v_string_array;
    GObject **v_object_array;
} GwySerializeValue;

typedef struct {
    guchar ctype;
    const guchar *name;
    GwySerializeValue value;
    guint32 array_size;
} GwySerializeItem;

GType       gwy_serializable_get_type            (void) G_GNUC_CONST;
GByteArray* gwy_serializable_serialize           (GObject *serializable,
                                                  GByteArray *buffer);
GObject*    gwy_serializable_deserialize         (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
GObject*    gwy_serializable_duplicate           (GObject *object);
void        gwy_serializable_clone               (GObject *source,
                                                  GObject *copy);

GByteArray* gwy_serialize_pack_object_struct     (GByteArray *buffer,
                                                  const guchar *object_name,
                                                  gsize nspec,
                                                  const GwySerializeSpec *spec);
gboolean    gwy_serialize_unpack_object_struct   (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position,
                                                  const guchar *object_name,
                                                  gsize nspec,
                                                  GwySerializeSpec *spec);
gsize       gwy_serialize_check_string           (const guchar *buffer,
                                                  gsize size,
                                                  gsize position,
                                                  const guchar *compare_to);

GByteArray*       gwy_serialize_object_items    (GByteArray *buffer,
                                                 const guchar *object_name,
                                                 gsize nitems,
                                                 const GwySerializeItem *items);
GwySerializeItem* gwy_deserialize_object_hash   (const guchar *buffer,
                                                 gsize size,
                                                 gsize *position,
                                                 const guchar *object_name,
                                                 gsize *nitems);


G_END_DECLS

#endif /* __GWY_SERIALIZABLE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
