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


typedef struct _GwySerializableIface GwySerializableIface;
typedef struct _GwySerializable      GwySerializable;        /* dummy */

typedef GByteArray* (*GwySerializeFunc)(GObject *serializable,
                                        GByteArray *buffer);
typedef GObject* (*GwyDeserializeFunc)(const guchar *buffer,
                                       gsize size,
                                       gsize *position);
typedef GObject* (*GwyDuplicateFunc)(GObject *object);

struct _GwySerializableIface {
    GTypeInterface parent_class;

    GwySerializeFunc serialize;
    GwyDeserializeFunc deserialize;
    GwyDuplicateFunc duplicate;
};

typedef struct _GwySerializeSpec GwySerializeSpec;

struct _GwySerializeSpec {
    guchar ctype;
    const guchar *name;
    gpointer value;
    guint32 *array_size;
};


GType       gwy_serializable_get_type            (void) G_GNUC_CONST;
GByteArray* gwy_serializable_serialize           (GObject *serializable,
                                                  GByteArray *buffer);
GObject*    gwy_serializable_deserialize         (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
GObject*    gwy_serializable_duplicate           (GObject *object);

#ifndef GWY_DISABLE_DEPRECATED
GByteArray* gwy_serialize_pack                   (GByteArray *buffer,
                                                  const gchar *templ,
                                                  ...);
void        gwy_serialize_store_int32            (GByteArray *buffer,
                                                  gsize position,
                                                  guint32 value);
gboolean    gwy_serialize_unpack_boolean         (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
guchar      gwy_serialize_unpack_char            (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
guchar*     gwy_serialize_unpack_char_array      (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position,
                                                  gsize *asize);
gint32      gwy_serialize_unpack_int32           (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
gint32*     gwy_serialize_unpack_int32_array     (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position,
                                                  gsize *asize);
gint64      gwy_serialize_unpack_int64           (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
gint64*     gwy_serialize_unpack_int64_array     (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position,
                                                  gsize *asize);
gdouble     gwy_serialize_unpack_double          (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
gdouble*    gwy_serialize_unpack_double_array    (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position,
                                                  gsize *asize);
guchar*     gwy_serialize_unpack_string          (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
gsize       gwy_serialize_check_string           (const guchar *buffer,
                                                  gsize size,
                                                  gsize position,
                                                  const guchar *compare_to);
GByteArray* gwy_serialize_pack_struct            (GByteArray *buffer,
                                                  gsize nspec,
                                                  const GwySerializeSpec *spec);
gboolean    gwy_serialize_unpack_struct          (const guchar *buffer,
                                                  gsize size,
                                                  gsize nspec,
                                                  const GwySerializeSpec *spec);
#endif
GByteArray* gwy_serialize_pack_object_struct     (GByteArray *buffer,
                                                  const guchar *object_name,
                                                  gsize nspec,
                                                  const GwySerializeSpec *spec);
gboolean    gwy_serialize_unpack_object_struct   (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position,
                                                  const guchar *object_name,
                                                  gsize nspec,
                                                  const GwySerializeSpec *spec);
GwySerializeSpec* gwy_serialize_unpack_object    (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position,
                                                  const guchar *object_name,
                                                  gsize *nspec);

G_END_DECLS

#endif /* __GWY_SERIALIZABLE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
