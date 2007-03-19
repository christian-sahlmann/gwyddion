/*
 *  @(#) $Id:$
 *  Copyright (C) 2006 Owain Davies, David Necas (Yeti), Petr Klapetek.
 *  E-mail: owain.davies@blueyonder.co.uk
 *               yeti@gwyddion.net, klapetek@gwyddion.net.
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

#ifndef __GWY_SPECTRA_H__
#define __GWY_SPECTRA_H__

#include <libgwyddion/gwysiunit.h>
#include <libprocess/gwyprocessenums.h>
#include <libprocess/dataline.h>
#include <glib.h>

G_BEGIN_DECLS

#define GWY_TYPE_SPECTRA            (gwy_spectra_get_type())
#define GWY_SPECTRA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SPECTRA, GwySpectra))
#define GWY_SPECTRA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SPECTRA, GwySpectraClass))
#define GWY_IS_SPECTRA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SPECTRA))
#define GWY_IS_SPECTRA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SPECTRA))
#define GWY_SPECTRA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SPECTRA, GwySpectraClass))

typedef struct _GwySpectra      GwySpectra;
typedef struct _GwySpectraClass GwySpectraClass;
typedef struct _coord_pos       coord_pos;

struct _coord_pos {
    guint index;
    gdouble r;
};

struct _GwySpectra {
    GObject parent_instance;

    gdouble     *coords;
    GwyDataLine **data;  /* Array of GwySpectrum */
    guint       ncurves; /* Number of spectra */
    guint       nalloc;  /* Allocated space */
    
    gchar       *title;
    GwySIUnit   *si_unit_xy;

    gdouble     double1;
    gpointer    reserved1;
    gint        int1;
};

struct _GwySpectraClass {
    GObjectClass parent_class;

    void (*data_changed)(GwySpectra *spectra);
    void (*reserved1)(void);
};

#define gwy_spectra_duplicate(spectra) \
        (GWY_SPECTRA(gwy_serializable_duplicate(G_OBJECT(spectra))))

GType           gwy_spectra_get_type  (void) G_GNUC_CONST;

GwySpectra*     gwy_spectra_new                    ();
GwySpectra*     gwy_spectra_new_alike              (GwySpectra *model);
void            gwy_spectra_data_changed           (GwySpectra *spectra);
void            gwy_spectra_copy                   (GwySpectra *a,
                                                    GwySpectra *b);
GwyDataLine**   gwy_spectra_get_data               (GwySpectra *spectra);
const GwyDataLine** gwy_spectra_get_data_const     (GwySpectra *spectra);
GwySIUnit*      gwy_spectra_get_si_unit_xy         (GwySpectra *spectra);
void            gwy_spectra_set_si_unit_xy         (GwySpectra *spectra,
                                                    GwySIUnit *si_unit);
GwySIValueFormat* gwy_spectra_get_value_format_xy  (GwySpectra *spectra,
                                                    GwySIUnitFormatStyle style,
                                                    GwySIValueFormat *format);
const gdouble*  gwy_spectra_itoxy                  (GwySpectra *spectra,
                                                    guint i);
guint           gwy_spectra_xytoi                  (GwySpectra *spectra,
                                                    gdouble real_x,
                                                    gdouble real_y);
void            gwy_spectra_setpos                 (GwySpectra *spectra,
                                                    gdouble real_x,
                                                    gdouble real_y,
                                                    guint i);
GwyDataLine*    gwy_spectra_get_spectrum           (GwySpectra *spectra,
                                                    gint i);
void            gwy_spectra_set_spectrum           (GwySpectra *spectra,
                                                    guint i,
                                                    GwyDataLine *new_spectrum);
guint           gwy_spectra_n_spectra              (GwySpectra *spectra); 
gint            gwy_spectra_nearest                (GwySpectra *spectra,
                                                    guint** plist,
                                                    gdouble real_x,
                                                    gdouble real_y);
void            gwy_spectra_add_spectrum           (GwySpectra *spectra,
                                                    GwyDataLine *new_spectrum,
                                                    gdouble x,
                                                    gdouble y);
void            gwy_spectra_remove_spectrum        (GwySpectra *spectra,
                                                    guint i);
GwyDataLine*    gwy_spectra_get_spectra_interp     (GwySpectra *spectra,
                                                    gdouble x,
                                                    gdouble y);
const gchar*    gwy_spectra_get_title              (GwySpectra *spectra);
void            gwy_spectra_set_title              (GwySpectra *spectra, gchar *new_title);
void            gwy_spectra_clear                  (GwySpectra *spectra);

G_END_DECLS

#endif /* __GWY_DATALINE_H__ */


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
