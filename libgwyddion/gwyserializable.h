#ifndef __GWY_SERIALIZABLE_H__
#define __GWY_SERIALIZABLE_H__

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_SERIALIZABLE                  (gwy_serializable_get_type())
#define GWY_SERIALIZABLE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SERIALIZABLE, GwySerializable))
#define GWY_SERIALIZABLE_CLASS(klass)          (G_TYPE_CHECK_INSTANCE_CAST((klass), GWY_TYPE_SERIALIZABLE, GwySerializableClass))
#define GWY_IS_SERIALIZABLE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SERIALIZABLE))
#define GWY_IS_SERIALIZABLE_CLASS(klass)       (G_TYPE_CHECK_INSTANCE_TYPE((klass), GWY_TYPE_SERIALIZABLE))
#define GWY_SERIALIZABLE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_INTERFACE((obj), GWY_TYPE_SERIALIZABLE, GwySerializableClass))


typedef struct _GwySerializable GwySerializable;
typedef struct _GwySerializableClass GwySerializableClass;

/**
 * GwySerializeFunc:
 * @serializable: The object.
 * @buffer: The buffer.
 * @size: The size of @buffer.
 *
 * The type of serialization method, see gwy_serializable_serialize() for
 * argument description.
 *
 * Returns: @buffer with serialized object appended.
 */
typedef guchar* (*GwySerializeFunc)(GObject *serializable,
                                    guchar *buffer,
                                    gsize *size);

/**
 * GwyDeserializeFunc:
 * @buffer: The buffer.
 * @size: The size of @buffer.
 * @position: The current position in @buffer.
 *
 * The type of deserialization method, see gwy_serializable_deserialize() for
 * argument description.
 *
 * Returns: A newly created (restored) object.
 */
typedef GObject* (*GwyDeserializeFunc)(const guchar *buffer,
                                       gsize size,
                                       gsize *position);

struct _GwySerializable {
    GObject parent_instance;
};

struct _GwySerializableClass {
    GObjectClass parent_class;

    GwySerializeFunc serialize;
    GwyDeserializeFunc deserialize;
};

typedef struct _GwySerializeSpec GwySerializeSpec;

struct _GwySerializeSpec {
    guchar ctype;
    const guchar *name;
    gpointer value;
    guint32 *array_size;
};


GType      gwy_serializable_get_type         (void) G_GNUC_CONST;
guchar*    gwy_serializable_serialize        (GObject *serializable,
                                              guchar *buffer,
                                              gsize *size);
GObject*   gwy_serializable_deserialize      (const guchar *buffer,
                                              gsize size,
                                              gsize *position);

void       gwy_serialize_store_int32         (guchar *buffer,
                                              guint32 value);
guchar*    gwy_serialize_pack                (guchar *buffer,
                                              gsize *size,
                                              const gchar *templ,
                                              ...);
gboolean   gwy_serialize_unpack_boolean      (const guchar *buffer,
                                              gsize size,
                                              gsize *position);
guchar     gwy_serialize_unpack_char         (const guchar *buffer,
                                              gsize size,
                                              gsize *position);
guchar*    gwy_serialize_unpack_char_array   (const guchar *buffer,
                                              gsize size,
                                              gsize *position,
                                              gsize *asize);
gint32     gwy_serialize_unpack_int32        (const guchar *buffer,
                                              gsize size,
                                              gsize *position);
gint32*    gwy_serialize_unpack_int32_array  (const guchar *buffer,
                                              gsize size,
                                              gsize *position,
                                              gsize *asize);
gint64     gwy_serialize_unpack_int64        (const guchar *buffer,
                                              gsize size,
                                              gsize *position);
gint64*    gwy_serialize_unpack_int64_array  (const guchar *buffer,
                                              gsize size,
                                              gsize *position,
                                              gsize *asize);
gdouble    gwy_serialize_unpack_double       (const guchar *buffer,
                                              gsize size,
                                              gsize *position);
gdouble*   gwy_serialize_unpack_double_array (const guchar *buffer,
                                              gsize size,
                                              gsize *position,
                                              gsize *asize);
guchar*    gwy_serialize_unpack_string       (const guchar *buffer,
                                              gsize size,
                                              gsize *position);

gsize      gwy_serialize_check_string        (const guchar *buffer,
                                              gsize size,
                                              gsize position,
                                              const guchar *compare_to);
guchar*    gwy_serialize_pack_struct         (guchar *buffer,
                                              gsize *size,
                                              gsize nspec,
                                              const GwySerializeSpec *spec);
gboolean   gwy_serialize_unpack_struct       (const guchar *buffer,
                                              gsize size,
                                              gsize nspec,
                                              const GwySerializeSpec *spec);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_SERIALIZABLE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
