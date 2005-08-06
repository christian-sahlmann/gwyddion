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

#ifndef __GWY_SI_UNIT_H__
#define __GWY_SI_UNIT_H__

#include <glib-object.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwyddionenums.h>

G_BEGIN_DECLS

#define GWY_TYPE_SI_UNIT                  (gwy_si_unit_get_type())
#define GWY_SI_UNIT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SI_UNIT, GwySIUnit))
#define GWY_SI_UNIT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SI_UNIT, GwySIUnitClass))
#define GWY_IS_SI_UNIT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SI_UNIT))
#define GWY_IS_SI_UNIT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SI_UNIT))
#define GWY_SI_UNIT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SI_UNIT, GwySIUnitClass))

typedef struct _GwySIUnit GwySIUnit;
typedef struct _GwySIUnitClass GwySIUnitClass;

typedef struct {
    gdouble magnitude;
    gint precision;
    gchar *units;
    GString *units_gstring;
} GwySIValueFormat;

struct _GwySIUnit {
    GObject parent_instance;

    gint power10;
    GArray *units;

    gpointer *reserved1;
};

struct _GwySIUnitClass {
    GObjectClass parent_class;

    void (*value_changed)(GwySIUnit *siunit);

    gpointer *reserved1;
    gpointer *reserved2;
};

#define gwy_si_unit_duplicate(siunit) \
        (GWY_SI_UNIT(gwy_serializable_duplicate(G_OBJECT(siunit))))

GType gwy_si_unit_get_type  (void) G_GNUC_CONST;

GwySIUnit*        gwy_si_unit_new                   (const gchar *unit_string);
GwySIUnit*        gwy_si_unit_new_parse             (const gchar *unit_string,
                                                     gint *power10);

void              gwy_si_unit_set_unit_string       (GwySIUnit *siunit,
                                                     const gchar *unit_string);
void              gwy_si_unit_set_unit_string_parse (GwySIUnit *siunit,
                                                     const gchar *unit_string,
                                                     gint *power10);
gchar*            gwy_si_unit_get_unit_string       (GwySIUnit *siunit);
GwySIUnit*        gwy_si_unit_multiply              (GwySIUnit *siunit1,
                                                     GwySIUnit *siunit2,
                                                     GwySIUnit *result);
GwySIUnit*        gwy_si_unit_divide                (GwySIUnit *siunit1,
                                                     GwySIUnit *siunit2,
                                                     GwySIUnit *result);
GwySIUnit*        gwy_si_unit_power                 (GwySIUnit *siunit,
                                                     gint power,
                                                     GwySIUnit *result);
gboolean          gwy_si_unit_equal                 (GwySIUnit *siunit1,
                                                     GwySIUnit *siunit2);

GwySIValueFormat* gwy_si_unit_get_format            (GwySIUnit *siunit,
                                                     GwySIUnitFormatStyle style,
                                                     gdouble value,
                                                     GwySIValueFormat *format);
GwySIValueFormat* gwy_si_unit_get_format_for_power10(GwySIUnit *siunit,
                                                     GwySIUnitFormatStyle style,
                                                     gint power10,
                                                     GwySIValueFormat *format);
GwySIValueFormat* gwy_si_unit_get_format_with_resolution(GwySIUnit *siunit,
                                                         GwySIUnitFormatStyle style,
                                                         gdouble maximum,
                                                         gdouble resolution,
                                                         GwySIValueFormat *format);
GwySIValueFormat* gwy_si_unit_get_format_with_digits(GwySIUnit *siunit,
                                                     GwySIUnitFormatStyle style,
                                                     gdouble maximum,
                                                     gint sdigits,
                                                     GwySIValueFormat *format);
void              gwy_si_unit_value_format_free     (GwySIValueFormat *format);


G_END_DECLS

#endif /* __GWY_SI_UNIT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
