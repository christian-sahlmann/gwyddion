/*
 *  @(#) $Id: gwygraphmodel.c 7159 2006-12-09 22:12:13Z yeti-dn $
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

struct _GwySpectra {
    GObject parent_instance;

    GArray      *spectra;

    gchar       *title;
    GwySIUnit   *si_unit_xy;

    gdouble     double1;
    gdouble     double2;
    gdouble     double3;
    gdouble     double4;
    gpointer    reserved1;
    gpointer    reserved2;
    gpointer    reserved3;
    gpointer    reserved4;
    gint        int1;
    gint        int2;
    gint        int3;
    gint        int4;
};

struct _GwySpectraClass {
    GObjectClass parent_class;

    void (*data_changed)(GwySpectra *spectra);
    void (*reserved1)(void);
    void (*reserved2)(void);
    void (*reserved3)(void);
};

#define gwy_spectra_duplicate(spectra) \
        (GWY_SPECTRA(gwy_serializable_duplicate(G_OBJECT(spectra))))

GType        gwy_spectra_get_type             (void) G_GNUC_CONST;
GwySpectra*  gwy_spectra_new                  (void);
GwySpectra*  gwy_spectra_new_alike            (GwySpectra *model);
void         gwy_spectra_data_changed         (GwySpectra *spectra);
GwySIUnit*   gwy_spectra_get_si_unit_xy       (GwySpectra *spectra);
void         gwy_spectra_set_si_unit_xy       (GwySpectra *spectra,
                                               GwySIUnit *si_unit);
void         gwy_spectra_itoxy                (GwySpectra *spectra,
                                               guint i,
                                               gdouble *x,
                                               gdouble *y);
gint         gwy_spectra_xytoi                (GwySpectra *spectra,
                                               gdouble x,
                                               gdouble y);
void         gwy_spectra_setpos               (GwySpectra *spectra,
                                               guint i,
                                               gdouble x,
                                               gdouble y);
guint        gwy_spectra_get_n_spectra        (GwySpectra *spectra);
GwyDataLine* gwy_spectra_get_spectrum         (GwySpectra *spectra,
                                               gint i);
void         gwy_spectra_set_spectrum         (GwySpectra *spectra,
                                               guint i,
                                               GwyDataLine *new_spectrum);
void         gwy_spectra_set_spectrum_selected(GwySpectra *spectra,
                                               guint i,
                                               gboolean selected);
gboolean     gwy_spectra_get_spectrum_selected(GwySpectra *spectra,
                                               guint i);
void         gwy_spectra_find_nearest         (GwySpectra *spectra,
                                               gdouble x,
                                               gdouble y,
                                               guint n,
                                               guint *ilist);
void         gwy_spectra_add_spectrum         (GwySpectra *spectra,
                                               GwyDataLine *new_spectrum,
                                               gdouble x,
                                               gdouble y);
void         gwy_spectra_remove_spectrum      (GwySpectra *spectra,
                                               guint i);
const gchar* gwy_spectra_get_title            (GwySpectra *spectra);
void         gwy_spectra_set_title            (GwySpectra *spectra,
                                               const gchar *title);
void         gwy_spectra_clear                (GwySpectra *spectra);

G_END_DECLS

#endif /* __GWY_DATALINE_H__ */


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
