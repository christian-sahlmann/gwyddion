/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_CONTAINER_H__
#define __GWY_CONTAINER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_CONTAINER                  (gwy_container_get_type ())
#define GWY_CONTAINER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GWY_TYPE_CONTAINER, GwyContainer))
#define GWY_CONTAINER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GWY_TYPE_CONTAINER, GwyContainerClass))
#define GWY_IS_CONTAINER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GWY_TYPE_CONTAINER))
#define GWY_IS_CONTAINER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GWY_TYPE_CONTAINER))
#define GWY_CONTAINER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GWY_TYPE_CONTAINER, GwyContainerClass))

#define GWY_CONTAINER_PATHSEP      '/'
#define GWY_CONTAINER_PATHSEP_STR  "/"

typedef struct _GwyContainer GwyContainer;
typedef struct _GwyContainerClass GwyContainerClass;

typedef void (*GwyContainerNotifyFunc)(GwyContainer *container,
                                       const guchar *path,
                                       gpointer user_data);

typedef struct {
    GQuark key;
    GValue *value;
    gboolean changed;
} GwyKeyVal;

struct _GwyContainer {
    GObject parent_instance;

    GHashTable *values;
    GHashTable *watching;
    GHashTable *objects;
    gint watch_freeze;
    gulong last_wid;
};

struct _GwyContainerClass {
    GObjectClass parent_class;
};


GType         gwy_container_get_type              (void) G_GNUC_CONST;
GObject*      gwy_container_new                   (void);
GType         gwy_container_value_type            (GwyContainer *container,
                                                   GQuark key);
gboolean      gwy_container_contains              (GwyContainer *container,
                                                   GQuark key);
GValue        gwy_container_get_value             (GwyContainer *container,
                                                   GQuark key);
void          gwy_container_set_value             (GwyContainer *container,
                                                   ...);
void          gwy_container_set_value_by_name     (GwyContainer *container,
                                                   ...);
gboolean      gwy_container_remove                (GwyContainer *container,
                                                   GQuark key);
gsize         gwy_container_remove_by_prefix      (GwyContainer *container,
                                                   const gchar *prefix);
gboolean      gwy_container_rename                (GwyContainer *container,
                                                   GQuark key,
                                                   GQuark newkey,
                                                   gboolean force);
gsize         gwy_container_foreach               (GwyContainer *container,
                                                   const gchar *prefix,
                                                   GHFunc function,
                                                   gpointer user_data);
void          gwy_container_set_boolean           (GwyContainer *container,
                                                   GQuark key,
                                                   gboolean value);
gboolean      gwy_container_get_boolean           (GwyContainer *container,
                                                   GQuark key);
gboolean      gwy_container_gis_boolean           (GwyContainer *container,
                                                   GQuark key,
                                                   gboolean *value);
void          gwy_container_set_uchar             (GwyContainer *container,
                                                   GQuark key,
                                                   guchar value);
guchar        gwy_container_get_uchar             (GwyContainer *container,
                                                   GQuark key);
gboolean      gwy_container_gis_uchar             (GwyContainer *container,
                                                   GQuark key,
                                                   guchar *value);
void          gwy_container_set_int32             (GwyContainer *container,
                                                   GQuark key,
                                                   gint32 value);
gint32        gwy_container_get_int32             (GwyContainer *container,
                                                   GQuark key);
gboolean      gwy_container_gis_int32             (GwyContainer *container,
                                                   GQuark key,
                                                   gint32 *value);
void          gwy_container_set_enum              (GwyContainer *container,
                                                   GQuark key,
                                                   guint value);
guint         gwy_container_get_enum              (GwyContainer *container,
                                                   GQuark key);
gboolean      gwy_container_gis_enum              (GwyContainer *container,
                                                   GQuark key,
                                                   guint *value);
void          gwy_container_set_int64             (GwyContainer *container,
                                                   GQuark key,
                                                   gint64 value);
gint64        gwy_container_get_int64             (GwyContainer *container,
                                                   GQuark key);
gboolean      gwy_container_gis_int64             (GwyContainer *container,
                                                   GQuark key,
                                                   gint64 *value);
void          gwy_container_set_double            (GwyContainer *container,
                                                   GQuark key,
                                                   gdouble value);
gdouble       gwy_container_get_double            (GwyContainer *container,
                                                   GQuark key);
gboolean      gwy_container_gis_double            (GwyContainer *container,
                                                   GQuark key,
                                                   gdouble *value);
void          gwy_container_set_string            (GwyContainer *container,
                                                   GQuark key,
                                                   const guchar *value);
G_CONST_RETURN
guchar*       gwy_container_get_string            (GwyContainer *container,
                                                   GQuark key);
gboolean      gwy_container_gis_string            (GwyContainer *container,
                                                   GQuark key,
                                                   const guchar **value);
void          gwy_container_set_object            (GwyContainer *container,
                                                   GQuark key,
                                                   GObject *value);
GObject*      gwy_container_get_object            (GwyContainer *container,
                                                   GQuark key);
gboolean      gwy_container_gis_object            (GwyContainer *container,
                                                   GQuark key,
                                                   GObject **value);

GPtrArray*    gwy_container_serialize_to_text     (GwyContainer *container);
GwyContainer* gwy_container_deserialize_from_text (const gchar *text);

/*
gulong     gwy_container_watch              (GwyContainer *container,
                                             const guchar *path,
                                             GwyContainerNotifyFunc callback,
                                             gpointer user_data);
void       gwy_container_freeze_watch       (GwyContainer *container);
void       gwy_container_thaw_watch         (GwyContainer *container);
*/

#define gwy_container_value_type_by_name(c,n) gwy_container_value_type(c,g_quark_try_string(n))
#define gwy_container_contains_by_name(c,n) gwy_container_contains(c,g_quark_try_string(n))
#define gwy_container_get_value_by_name(c,n) gwy_container_get_value(c,g_quark_try_string(n))
#define gwy_container_remove_by_name(c,n) gwy_container_remove(c,g_quark_try_string(n))
#define gwy_container_rename_by_name(c,n,nn,f) gwy_container_rename(c,g_quark_try_string(n),g_quark_from_string(nn),f)
#define gwy_container_set_boolean_by_name(c,n,v) gwy_container_set_boolean(c,g_quark_from_string(n),v)
#define gwy_container_get_boolean_by_name(c,n) gwy_container_get_boolean(c,g_quark_try_string(n))
#define gwy_container_gis_boolean_by_name(c,n,v) gwy_container_gis_boolean(c,g_quark_from_string(n),v)
#define gwy_container_set_uchar_by_name(c,n,v) gwy_container_set_uchar(c,g_quark_from_string(n),v)
#define gwy_container_get_uchar_by_name(c,n) gwy_container_get_uchar(c,g_quark_try_string(n))
#define gwy_container_gis_uchar_by_name(c,n,v) gwy_container_gis_uchar(c,g_quark_from_string(n),v)
#define gwy_container_set_int32_by_name(c,n,v) gwy_container_set_int32(c,g_quark_from_string(n),v)
#define gwy_container_get_int32_by_name(c,n) gwy_container_get_int32(c,g_quark_try_string(n))
#define gwy_container_gis_int32_by_name(c,n,v) gwy_container_gis_int32(c,g_quark_from_string(n),v)
#define gwy_container_set_enum_by_name(c,n,v) gwy_container_set_enum(c,g_quark_from_string(n),v)
#define gwy_container_get_enum_by_name(c,n) gwy_container_get_enum(c,g_quark_try_string(n))
#define gwy_container_gis_enum_by_name(c,n,v) gwy_container_gis_enum(c,g_quark_from_string(n),v)
#define gwy_container_set_int64_by_name(c,n,v) gwy_container_set_int64(c,g_quark_from_string(n),v)
#define gwy_container_get_int64_by_name(c,n) gwy_container_get_int64(c,g_quark_try_string(n))
#define gwy_container_gis_int64_by_name(c,n,v) gwy_container_gis_int64(c,g_quark_from_string(n),v)
#define gwy_container_set_double_by_name(c,n,v) gwy_container_set_double(c,g_quark_from_string(n),v)
#define gwy_container_get_double_by_name(c,n) gwy_container_get_double(c,g_quark_try_string(n))
#define gwy_container_gis_double_by_name(c,n,v) gwy_container_gis_double(c,g_quark_from_string(n),v)
#define gwy_container_set_string_by_name(c,n,v) gwy_container_set_string(c,g_quark_from_string(n),v)
#define gwy_container_get_string_by_name(c,n) gwy_container_get_string(c,g_quark_try_string(n))
#define gwy_container_gis_string_by_name(c,n,v) gwy_container_gis_string(c,g_quark_from_string(n),v)
#define gwy_container_set_object_by_name(c,n,v) gwy_container_set_object(c,g_quark_from_string(n),v)
#define gwy_container_get_object_by_name(c,n) gwy_container_get_object(c,g_quark_try_string(n))
#define gwy_container_gis_object_by_name(c,n,v) gwy_container_gis_object(c,g_quark_from_string(n),v)

G_END_DECLS

#endif /* __GWY_CONTAINER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
