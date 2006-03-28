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

#ifndef __GWY_STRING_LIST_H__
#define __GWY_STRING_LIST_H__

#include <glib-object.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwyddionenums.h>

G_BEGIN_DECLS

#define GWY_TYPE_STRING_LIST                  (gwy_string_list_get_type())
#define GWY_STRING_LIST(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_STRING_LIST, GwyStringList))
#define GWY_STRING_LIST_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_STRING_LIST, GwyStringListClass))
#define GWY_IS_STRING_LIST(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_STRING_LIST))
#define GWY_IS_STRING_LIST_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_STRING_LIST))
#define GWY_STRING_LIST_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_STRING_LIST, GwyStringListClass))

typedef struct _GwyStringList GwyStringList;
typedef struct _GwyStringListClass GwyStringListClass;

struct _GwyStringList {
    GObject parent_instance;

    gpointer *strings;    /* An opaque type */

    gpointer *reserved1;
    gpointer *reserved2;
};

struct _GwyStringListClass {
    GObjectClass parent_class;

    void (*value_changed)(GwyStringList *strlist);

    gpointer *reserved1;
    gpointer *reserved2;
    gpointer *reserved3;
    gpointer *reserved4;
};

#define gwy_string_list_duplicate(strlist) \
        (GWY_STRING_LIST(gwy_serializable_duplicate(G_OBJECT(strlist))))

GType gwy_string_list_get_type  (void) G_GNUC_CONST;

GwyStringList* gwy_string_list_new        (void);
void           gwy_string_list_append     (GwyStringList *strlist,
                                           const gchar *string);
guint          gwy_string_list_get_length (GwyStringList *strlist);
const gchar*   gwy_string_list_get        (GwyStringList *strlist,
                                           guint i);

G_END_DECLS

#endif /* __GWY_STRING_LIST_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
