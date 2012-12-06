/*
 *  @(#) $Id: brick.h 12576 2011-07-11 14:51:57Z yeti-dn $
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_BRICK_H__
#define __GWY_BRICK_H__

#include <libgwyddion/gwysiunit.h>
#include <libprocess/gwyprocessenums.h>
#include <libprocess/datafield.h>
#include <libprocess/dataline.h>


G_BEGIN_DECLS

#define GWY_TYPE_BRICK            (gwy_brick_get_type())
#define GWY_BRICK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_BRICK, GwyBrick))
#define GWY_BRICK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_BRICK, GwyBrickClass))
#define GWY_IS_BRICK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_BRICK))
#define GWY_IS_BRICK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_BRICK))
#define GWY_BRICK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_BRICK, GwyBrickClass))

typedef struct _GwyBrick      GwyBrick;
typedef struct _GwyBrickClass GwyBrickClass;

typedef struct {
    guint col;
    guint row;
    guint level;
    guint width;
    guint height;
    guint depth;
} GwyBrickPart;


struct _GwyBrick {
    GObject parent_instance;

    gint xres;
    gint yres;
    gint zres;

    gdouble xreal;
    gdouble yreal;
    gdouble zreal;

    gdouble xoff;
    gdouble yoff;
    gdouble zoff;

    gdouble *data;

    GwySIUnit *si_unit_x;
    GwySIUnit *si_unit_y;
    GwySIUnit *si_unit_z;
    GwySIUnit *si_unit_w;

    gpointer reserved1;
    gint int1;
};

struct _GwyBrickClass {
    GObjectClass parent_class;

    void (*data_changed)(GwyBrick *brick);
    void (*reserved1)(void);
};

#define gwy_brick_duplicate(brick) \
        (GWY_BRICK(gwy_serializable_duplicate(G_OBJECT(brick))))

GType  gwy_brick_get_type  (void) G_GNUC_CONST;

GwyBrick*   gwy_brick_new                   (gint xres, gint yres, gint zres,
                                                    gdouble xreal, gdouble yreal, gdouble zreal,
                                                    gboolean nullme);
GwyBrick*   gwy_brick_new_alike             (GwyBrick *model,
                                             gboolean nullme);

GwyBrick*       gwy_brick_new_part    (const GwyBrick *brick,
                                       const GwyBrickPart *bpart,
                                       gboolean keep_offsets);
void            gwy_brick_set_size    (GwyBrick *brick,
                                       guint xres,
                                       guint yres,
                                       guint zres,
                                       gboolean clear);
void            gwy_brick_data_changed(GwyBrick *brick);
void            gwy_brick_copy        (const GwyBrick *src,
                                       const GwyBrickPart *srcpart,
                                       GwyBrick *dest,
                                       guint destcol,
                                       guint destrow,
                                       guint destlevel);
void            gwy_brick_copy_full   (const GwyBrick *src,
                                       GwyBrick *dest);
void            gwy_brick_invalidate  (GwyBrick *brick);

void            gwy_brick_resample    (GwyBrick *brick,
                                       gint xres,
                                       gint yres,
                                       gint zres,
                                       GwyInterpolationType interpolation);

gint            gwy_brick_get_xres   (GwyBrick *brick);
gint            gwy_brick_get_yres   (GwyBrick *brick);
gint            gwy_brick_get_zres   (GwyBrick *brick);

gdouble            gwy_brick_get_xreal   (GwyBrick *brick);
gdouble            gwy_brick_get_yreal   (GwyBrick *brick);
gdouble            gwy_brick_get_zreal   (GwyBrick *brick);



void            gwy_brick_set_xreal   (GwyBrick *brick,
                                       gdouble xreal);
void            gwy_brick_set_yreal   (GwyBrick *brick,
                                       gdouble yreal);
void            gwy_brick_set_zreal   (GwyBrick *brick,
                                       gdouble zreal);
void            gwy_brick_set_xoffset (GwyBrick *brick,
                                       gdouble xoffset);
void            gwy_brick_set_yoffset (GwyBrick *brick,
                                       gdouble yoffset);
void            gwy_brick_set_zoffset (GwyBrick *brick,
                                       gdouble zoffset);

GwySIUnit*        gwy_brick_get_si_unit_x(GwyBrick *brick);
GwySIUnit*        gwy_brick_get_si_unit_y(GwyBrick *brick);
GwySIUnit*        gwy_brick_get_si_unit_z(GwyBrick *brick);
GwySIUnit*        gwy_brick_get_si_unit_w(GwyBrick *brick);

gdouble         gwy_brick_get_min     (GwyBrick *brick);
gdouble         gwy_brick_get_max     (GwyBrick *brick);


GwySIValueFormat* gwy_brick_get_value_format_x (GwyBrick *brick,
                                                    GwySIUnitFormatStyle style,
                                                    GwySIValueFormat *format);
GwySIValueFormat* gwy_brick_get_value_format_y (GwyBrick *brick,
                                                    GwySIUnitFormatStyle style,
                                                    GwySIValueFormat *format);
GwySIValueFormat* gwy_brick_get_value_format_z (GwyBrick *brick,
                                                    GwySIUnitFormatStyle style,
                                                    GwySIValueFormat *format);
GwySIValueFormat* gwy_brick_get_value_format_w (GwyBrick *brick,
                                                    GwySIUnitFormatStyle style,
                                                    GwySIValueFormat *format);


gdouble*       gwy_brick_get_data              (GwyBrick *brick);
gdouble        gwy_brick_itor                  (GwyBrick *brick,
                                                    gdouble pixpos);
gdouble        gwy_brick_rtoi                  (GwyBrick *brick,
                                                    gdouble realpos);
gdouble        gwy_brick_jtor                  (GwyBrick *brick,
                                                    gdouble pixpos);
gdouble        gwy_brick_rtoj                  (GwyBrick *brick,
                                                    gdouble realpos);
gdouble        gwy_brick_ktor                  (GwyBrick *brick,
                                                    gdouble pixpos);
gdouble        gwy_brick_rtok                  (GwyBrick *brick,
                                                    gdouble realpos);

gdouble        gwy_brick_get_val               (GwyBrick *brick,
                                                    gint col, 
                                                    gint row, 
                                                    gint lev);
void           gwy_brick_set_val               (GwyBrick *brick,
                                                    gint col,
                                                    gint row,
                                                    gint lev,
                                                    gdouble value);
gdouble        gwy_brick_get_val_real               (GwyBrick *brick,
                                                     gdouble x, 
                                                     gdouble y, 
                                                     gdouble z);
void           gwy_brick_set_val_real               (GwyBrick *brick,
                                                     gdouble x,
                                                     gdouble y,
                                                     gdouble z,
                                                     gdouble value);
gdouble        gwy_brick_get_dval              (GwyBrick *brick,
                                                gdouble x,
                                                gdouble y,
                                                gdouble z,
                                                gint interpolation);
gdouble        gwy_brick_get_dval_real         (GwyBrick *brick,
                                                gdouble x,
                                                gdouble y,
                                                gdouble z,
                                                gint interpolation);
void           gwy_brick_clear                 (GwyBrick *brick);
void           gwy_brick_fill                  (GwyBrick *brick,
                                                    gdouble value);
void           gwy_brick_multiply              (GwyBrick *brick,
                                                    gdouble value);
void           gwy_brick_add                   (GwyBrick *brick,
                                                gdouble value);

void           gwy_brick_extract_plane(const GwyBrick *brick,
                                       GwyDataField *target,
                                       gint istart, 
                                       gint jstart,
                                       gint kstart,
                                       gint width,
                                       gint height,
                                       gint depth,
                                       gboolean keep_offsets);

void           gwy_brick_sum_plane(const GwyBrick *brick,
                                       GwyDataField *target,
                                       gint istart, 
                                       gint jstart,
                                       gint kstart,
                                       gint width,
                                       gint height,
                                       gint depth,
                                       gboolean keep_offsets);



G_END_DECLS

#endif /* __GWY_BRICK_H__ */


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

