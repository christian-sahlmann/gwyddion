/*
 *  @(#) $Id: gwygraphmodel.c 7159 2006-12-09 22:12:13Z yeti-dn $
 *  Copyright (C) 2006 Owain Davies, David Necas (Yeti), Petr Klapetek.
 *  E-mail: owain.davies@blueyonder.co.uk
 *          yeti@gwyddion.net, klapetek@gwyddion.net.
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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/spectra.h>
#include <libprocess/linestats.h>
#include <libprocess/interpolation.h>

#define GWY_SPECTRA_TYPE_NAME "GwySpectra"
/* default number number of spectra allocated to data and coords */
#define DEFAULT_ALLOC_SIZE 5

enum {
    PROP_0,
    PROP_TITLE,
    PROP_LAST
};

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

typedef struct {
    guint index;
    gdouble r;
} CoordPos;

typedef struct {
    gdouble x;
    gdouble y;
    GwyDataLine *ydata;
    gboolean selected;
} GwySpectrum;

static void        gwy_spectra_finalize         (GObject *object);
static void        gwy_spectra_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_spectra_serialize        (GObject *obj,
                                                 GByteArray *buffer);
static gsize       gwy_spectra_get_size         (GObject *obj);
static GObject*    gwy_spectra_deserialize      (const guchar *buffer,
                                                 gsize size,
                                                 gsize *position);
static GObject*    gwy_spectra_duplicate_real   (GObject *object);
static void        gwy_spectra_clone_real       (GObject *source,
                                                 GObject *copy);
static void        gwy_spectra_set_property     (GObject *object,
                                                 guint prop_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec);
static void        gwy_spectra_get_property     (GObject *object,
                                                 guint prop_id,
                                                 GValue *value,
                                                 GParamSpec *pspec);

static guint spectra_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwySpectra, gwy_spectra, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_spectra_serializable_init))

static void
gwy_spectra_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    iface->serialize = gwy_spectra_serialize;
    iface->deserialize = gwy_spectra_deserialize;
    iface->get_size = gwy_spectra_get_size;
    iface->duplicate = gwy_spectra_duplicate_real;
    iface->clone = gwy_spectra_clone_real;
}

static void
gwy_spectra_class_init(GwySpectraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_spectra_finalize;
    gobject_class->get_property = gwy_spectra_get_property;
    gobject_class->set_property = gwy_spectra_set_property;

    g_object_class_install_property
        (gobject_class,
         PROP_TITLE,
         g_param_spec_string("title",
                             "Title",
                             "The spectra title",
                             "New spectra",
                             G_PARAM_READWRITE));

    /**
     * GwySpectra::data-changed:
     * @gwyspectra: The #GwySpectra which received the signal.
     *
     * The ::data-changed signal is never emitted by the spectra itself.  It
     * is intended as a means to notify other spectra users they should
     * update themselves.
     **/
    spectra_signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwySpectraClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_spectra_init(GwySpectra *spectra)
{
    gwy_debug_objects_creation(G_OBJECT(spectra));
    spectra->spectra = g_array_new(FALSE, FALSE, sizeof(GwySpectrum));
}

static void
gwy_spectra_finalize(GObject *object)
{
    GwySpectra *spectra = (GwySpectra*)object;
    guint i;

    gwy_debug("");

    gwy_object_unref(spectra->si_unit_xy);
    for (i = 0; i < spectra->spectra->len; i++) {
        GwySpectrum *spec = &g_array_index(spectra->spectra, GwySpectrum, i);

        gwy_object_unref(spec->ydata);
    }
    g_array_free(spectra->spectra, TRUE);
    spectra->spectra = NULL;

    g_free(spectra->title);

    G_OBJECT_CLASS(gwy_spectra_parent_class)->finalize(object);
}

static void
gwy_spectra_set_property(GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
    GwySpectra *spectra = GWY_SPECTRA(object);

    switch (prop_id) {
        case PROP_TITLE:
        gwy_spectra_set_title(spectra, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_spectra_get_property(GObject*object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
    GwySpectra *spectra = GWY_SPECTRA(object);

    switch (prop_id) {
        case PROP_TITLE:
        g_value_set_string(value, spectra->title);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_spectra_new:
 *
 * Creates a new Spectra object containing zero spectra.
 *
 * Returns: A newly created spectra.
 *
 * Since: 2.6
 **/
GwySpectra*
gwy_spectra_new(void)
{
    GwySpectra *spectra;

    gwy_debug("");
    spectra = g_object_new(GWY_TYPE_SPECTRA, NULL);

    return spectra;
}

/**
 * gwy_spectra_new_alike:
 * @model: A Spectra object to take units from.
 *
 * Creates a new Spectra object similar to an existing one, but containing zero
 * spectra.
 *
 * Use gwy_spectra_duplicate() if you want to copy a spectra object including
 * the spectra in it.
 *
 * Returns: A newly created Spectra object.
 *
 * Since: 2.6
 **/
GwySpectra*
gwy_spectra_new_alike(GwySpectra *model)
{
    GwySpectra *spectra;

    g_return_val_if_fail(GWY_IS_SPECTRA(model), NULL);
    spectra = g_object_new(GWY_TYPE_SPECTRA, NULL);

    if (model->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_duplicate(model->si_unit_xy);

    return spectra;
}

static void
separate_arrays(GArray *spectra,
                guint *ncurves,
                GwyDataLine ***curves,
                guint *ncoords,
                gdouble **coords,
                guint *nselected,
                guint32 **selected)
{
    guint isize, i;

    *ncurves = spectra->len;
    *curves = g_new(GwyDataLine*, *ncurves);

    *ncoords = 2*spectra->len;
    *coords = g_new(gdouble, *ncoords);

    isize = 8*sizeof(guint32);
    *nselected = (spectra->len + isize-1)/isize;
    *selected = g_new0(guint32, *nselected);

    for (i = 0; i < *ncurves; i++) {
        GwySpectrum *spec = &g_array_index(spectra, GwySpectrum, i);

        (*curves)[i] = spec->ydata;
        (*coords)[2*i + 0] = spec->y;
        (*coords)[2*i + 1] = spec->x;
        if (spec->selected)
            (*selected)[i/isize] |= 1 << (i % isize);
    }
}

static GByteArray*
gwy_spectra_serialize(GObject *obj,
                      GByteArray *buffer)
{
    GwySpectra *spectra;
    GwyDataLine **curves;
    gdouble *coords;
    guint32 *selected;
    guint32 ncurves, ncoords, nselected;
    GByteArray *retval;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SPECTRA(obj), NULL);

    spectra = GWY_SPECTRA(obj);
    if (!spectra->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_new(NULL);

    separate_arrays(spectra->spectra,
                    &ncurves, &curves,
                    &ncoords, &coords,
                    &nselected, &selected);

    {
        GwySerializeSpec spec[] = {
            { 's', "title",      &spectra->title,      NULL,       },
            { 'o', "si_unit_xy", &spectra->si_unit_xy, NULL,       },
            { 'D', "coords",     &coords,              &ncoords,   },
            { 'I', "selected",   &selected,            &nselected, },
            { 'O', "data",       &curves,              &ncurves,   },
        };

        retval = gwy_serialize_pack_object_struct(buffer,
                                                  GWY_SPECTRA_TYPE_NAME,
                                                  G_N_ELEMENTS(spec), spec);
    }
    g_free(curves);
    g_free(coords);
    g_free(selected);

    return retval;
}

static gsize
gwy_spectra_get_size(GObject *obj)
{
    GwySpectra *spectra;
    GwyDataLine **curves;
    gdouble *coords;
    guint32 *selected;
    guint32 ncurves, ncoords, nselected;
    gsize retval;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SPECTRA(obj), 0);

    spectra = GWY_SPECTRA(obj);
    if (!spectra->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_new(NULL);

    separate_arrays(spectra->spectra,
                    &ncurves, &curves,
                    &ncoords, &coords,
                    &nselected, &selected);

    {
        GwySerializeSpec spec[] = {
            { 's', "title",      &spectra->title,      NULL,       },
            { 'o', "si_unit_xy", &spectra->si_unit_xy, NULL,       },
            { 'D', "coords",     &coords,              &ncoords,   },
            { 'I', "selected",   &selected,            &nselected, },
            { 'O', "data",       &curves,              &ncurves,   },
        };

        retval = gwy_serialize_get_struct_size(GWY_SPECTRA_TYPE_NAME,
                                               G_N_ELEMENTS(spec), spec);
    }
    g_free(curves);
    g_free(coords);
    g_free(selected);

    return retval;
}

static GObject*
gwy_spectra_deserialize(const guchar *buffer,
                        gsize size,
                        gsize *position)
{
    guint32 ncoords = 0, ncurves = 0, nselected = 0;
    gdouble *coords = NULL;
    guint32 *selected = NULL;
    GwySIUnit *si_unit_xy = NULL;
    GwyDataLine **curves = NULL;
    GwySpectra *spectra;
    gchar* title = NULL;
    guint isize, i;

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    {
        GwySerializeSpec spec[] = {
            { 's', "title",      &title,      NULL,       },
            { 'o', "si_unit_xy", &si_unit_xy, NULL,       },
            { 'D', "coords",     &coords,     &ncoords,   },
            { 'I', "selected",   &selected,   &nselected, },
            { 'O', "data",       &curves,     &ncurves,   },
        };

        if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                                GWY_SPECTRA_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec)) {
            for (i = 0; i < ncurves; i++)
                gwy_object_unref(curves[i]);
            g_free(curves);
            g_free(coords);
            g_free(selected);
            gwy_object_unref(si_unit_xy);

            return NULL;
        }
    }

    isize = 8*sizeof(guint32);
    if (2*ncurves != ncoords
        || (nselected && (nselected + isize-1)/isize != ncurves)) {
        g_critical("Serialized coordinate, data and selection array size "
                   "mismatch");
        for (i = 0; i < ncurves; i++)
            gwy_object_unref(curves[i]);
        g_free(curves);
        g_free(coords);
        g_free(selected);
        gwy_object_unref(si_unit_xy);

        return NULL;
    }

    spectra = gwy_spectra_new();

    g_free(spectra->title);
    spectra->title = title;
    /* Preallocate ncurves items */
    g_array_set_size(spectra->spectra, ncurves);
    g_array_set_size(spectra->spectra, 0);
    for (i = 0; i < ncurves; i++) {
        GwySpectrum spec;

        spec.x = coords[2*i + 0];
        spec.y = coords[2*i + 1];
        spec.ydata = curves[i];
        if (nselected)
            spec.selected = !!(selected[i/isize] & (1 << (i % isize)));
        g_array_append_val(spectra->spectra, spec);
    }
    g_free(curves);
    g_free(coords);
    g_free(selected);

    if (si_unit_xy) {
        gwy_object_unref(spectra->si_unit_xy);
        spectra->si_unit_xy = si_unit_xy;
    }

    return (GObject*)spectra;
}

static GObject*
gwy_spectra_duplicate_real(GObject *object)
{
    gint i;
    GwySpectra *spectra, *duplicate;
    GwyDataLine *dataline;

    g_return_val_if_fail(GWY_IS_SPECTRA(object), NULL);
    spectra = GWY_SPECTRA(object);
    duplicate = gwy_spectra_new_alike(spectra);
    duplicate->title = g_strdup(spectra->title);
    if (spectra->si_unit_xy)
        duplicate->si_unit_xy = gwy_si_unit_duplicate(spectra->si_unit_xy);
    g_array_append_vals(duplicate->spectra, spectra->spectra->data,
                        spectra->spectra->len);

    /* Duplicate the spectra themselves */
    for (i = 0; i < duplicate->spectra->len; i++) {
        dataline = g_array_index(duplicate->spectra, GwySpectrum, i).ydata;
        g_array_index(duplicate->spectra, GwySpectrum, i).ydata
            = gwy_data_line_duplicate(dataline);
    }

    return (GObject*)duplicate;
}

static void
gwy_spectra_clone_real(GObject *source, GObject *copy)
{
    GwySpectra *spectra, *clone;
    GwyDataLine *dataline;
    guint i;

    g_return_if_fail(GWY_IS_SPECTRA(source));
    g_return_if_fail(GWY_IS_SPECTRA(copy));

    spectra = GWY_SPECTRA(source);
    clone = GWY_SPECTRA(copy);

    /* Title */
    g_free(clone->title);
    clone->title = g_strdup(spectra->title);

    /* Remove any existing datalines in the clone */
    for (i = 0; i < clone->spectra->len; i++) {
        g_object_unref(g_array_index(clone->spectra, GwySpectrum, i).ydata);
    }

    /* Copy the spectra to clone */
    g_array_set_size(clone->spectra, 0);
    g_array_append_vals(clone->spectra, spectra->spectra->data,
                        spectra->spectra->len);

    /* Clone the spectra theselves */
    for (i = 0; i < spectra->spectra->len; i++) {
        dataline = g_array_index(clone->spectra, GwySpectrum, i).ydata;
        g_array_index(clone->spectra, GwySpectrum, i).ydata
            = gwy_data_line_duplicate(dataline);
    }

    /* SI Units can be NULL */
    if (spectra->si_unit_xy && clone->si_unit_xy)
        gwy_serializable_clone(G_OBJECT(spectra->si_unit_xy),
                               G_OBJECT(clone->si_unit_xy));
    else if (spectra->si_unit_xy && !clone->si_unit_xy)
        clone->si_unit_xy = gwy_si_unit_duplicate(spectra->si_unit_xy);
    else if (!spectra->si_unit_xy && clone->si_unit_xy)
        gwy_object_unref(clone->si_unit_xy);
}

/**
 * gwy_spectra_data_changed:
 * @spectra: A spectra object.
 *
 * Emits signal "data_changed" on a spectra object.
 *
 * Since: 2.6
 **/
void
gwy_spectra_data_changed(GwySpectra *spectra)
{
    g_signal_emit(spectra, spectra_signals[DATA_CHANGED], 0);
}

/**
 * gwy_spectra_get_si_unit_xy:
 * @spectra: A spectra.
 *
 * Gets SI unit used for the location co-ordinates of spectra.
 *
 * Returns: SI unit corresponding to the  the location co-ordinates of spectra
 *          object. Its reference count is not incremented.
 *
 * Since: 2.6
 **/
GwySIUnit*
gwy_spectra_get_si_unit_xy(GwySpectra *spectra)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);

    if (!spectra->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_new(NULL);

    return spectra->si_unit_xy;
}

/**
 * gwy_spectra_set_si_unit_xy:
 * @spectra: A Spectra object.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the location co-ordinates of the spectra
 * object.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 *
 * Since: 2.6
 **/
void
gwy_spectra_set_si_unit_xy(GwySpectra *spectra,
                           GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (spectra->si_unit_xy == si_unit)
        return;

    gwy_object_unref(spectra->si_unit_xy);
    g_object_ref(si_unit);
    spectra->si_unit_xy = si_unit;
}

/**
 * gwy_spectra_itoxy:
 * @spectra: A spectra object.
 * @i: Index of a spectrum.
 * @x: Location to store the physical x coordinate of the spectrum.
 * @y: Location to store the physical x coordinate of the spectrum.
 *
 * Gets the coordinates of one spectrum.
 *
 * Since: 2.6
 **/
void
gwy_spectra_itoxy(GwySpectra *spectra,
                  guint i,
                  gdouble *x,
                  gdouble *y)
{
    GwySpectrum *spec;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(i < spectra->spectra->len);

    spec = &g_array_index(spectra->spectra, GwySpectrum, i);
    if (x)
        *x = spec->x;
    if (y)
        *y = spec->y;
}

/**
 * gwy_spectra_xytoi:
 * @spectra: A spectra object.
 * @x: The x coordinate of the location of the spectrum.
 * @y: The y coordinate of the location of the spectrum.
 *
 * Finds the index of the spectrum closest to the location specified by
 * the coordinates x and y.
 *
 * Returns: The index of the nearest spectrum.  If there are no curves in the
 *          spectra, -1 is returned.
 *
 * Since: 2.6
 **/
gint
gwy_spectra_xytoi(GwySpectra *spectra, gdouble x, gdouble y)
{
    guint i;

    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), -1);

    if (!spectra->spectra->len)
        return -1;

    gwy_spectra_find_nearest(spectra, x, y, 1, &i);

    return i;
}

static gint
compare_coord_pos(gconstpointer a, gconstpointer b)
{
    const CoordPos *acp = (const CoordPos*)a;
    const CoordPos *bcp = (const CoordPos*)b;

    if (acp->r < bcp->r)
        return -1;
    if (acp->r > bcp->r)
        return 1;
    return 0;
}

/**
 * gwy_spectra_find_nearest:
 * @spectra: A spectra object.
 * @x: Point x-coordinate.
 * @y: Point y-coordinate.
 * @n: Number of indices to find.  Array @ilist must have at least this
 *     number of items.
 * @ilist: Array to place the spectra indices to.  They will be sorted by the
 *         distance from (@x, @y).  Positions after the number of spectra
 *         in @spectra will be left untouched.
 *
 * Gets the list of the indices to spectra ordered by their distance from a
 * given point.
 *
 * List positions
 *
 * Since: 2.6
 **/
void
gwy_spectra_find_nearest(GwySpectra *spectra,
                         gdouble x,
                         gdouble y,
                         guint n,
                         guint *ilist)
{
    enum { DIRECT_LIMIT = 6 };

    CoordPos *items;
    guint nspec, i;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(ilist || !n);

    nspec = spectra->spectra->len;
    n = MIN(n, nspec);
    if (!n)
        return;

    if (n <= DIRECT_LIMIT) {
        items = g_newa(CoordPos, n);

        /* Initialize with sorted initial n items */
        for (i = 0; i < n; i++) {
            GwySpectrum *spec = &g_array_index(spectra->spectra, GwySpectrum,
                                               i);
            items[i].index = i;
            items[i].r = ((spec->x - x)*(spec->x - x)
                          + (spec->y - y)*(spec->y - y));
        }
        qsort(items, n, sizeof(CoordPos), compare_coord_pos);

        /* And let the remaining items compete for positions */
        for (i = n; i < nspec; i++) {
            guint j, k;
            gdouble r;
            GwySpectrum *spec = &g_array_index(spectra->spectra, GwySpectrum,
                                               i);

            r = ((spec->x - x)*(spec->x - x)
                 + (spec->y - y)*(spec->y - y));
            if (r < items[n-1].r) {
                for (j = 1; j < n && r < items[n-1 - j].r; j++)
                    ;
                for (k = 0; k < j-1; k++)
                    items[n-k] = items[n-1 - k];

                items[n-j].index = i;
                items[n-j].r = r;
            }
        }

        /* Move the results to ilist */
        for (i = 0; i < n; i++)
            ilist[i] = items[i].index;
    }
    else {
        /* Sort all items and take the head */
        items = g_new(CoordPos, spectra->spectra->len);
        for (i = 0; i < spectra->spectra->len; i++) {
            GwySpectrum *spec = &g_array_index(spectra->spectra, GwySpectrum,
                                               i);

            items[i].index = i;
            items[i].r = ((spec->x - x)*(spec->x - x)
                          + (spec->y - y)*(spec->y - y));
        }
        qsort(items, nspec, sizeof(CoordPos), compare_coord_pos);
        for (i = 0; i < n; i++)
            ilist[i] = items[i].index;

        g_free(items);
    }
}

/**
 * gwy_spectra_setpos:
 * @spectra: A spectra object.
 * @i: The index of a spectrum.
 * @x: The new x coordinate of the location of the spectrum.
 * @y: The new y coordinate of the location of the spectrum.
 *
 * Sets the location coordinates of a spectrum.
 *
 * Since: 2.6
 **/
void
gwy_spectra_setpos(GwySpectra *spectra,
                   guint i,
                   gdouble x, gdouble y)
{
    GwySpectrum *spec;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(i <= spectra->spectra->len);

    spec = &g_array_index(spectra->spectra, GwySpectrum, i);
    spec->x = x;
    spec->y = y;
}

/**
 * gwy_spectra_get_spectrum:
 * @spectra: A Spectra object
 * @i: Index of a spectrum
 *
 * Gets a dataline that contains the spectrum at index i.
 *
 * Returns: A #GwyDataLine containing the spectrum, owned by @spectra.
 *
 * Since: 2.6
 **/
GwyDataLine*
gwy_spectra_get_spectrum(GwySpectra *spectra, gint i)
{
    GwySpectrum *spec;

    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);
    g_return_val_if_fail(i <= spectra->spectra->len, NULL);

    spec = &g_array_index(spectra->spectra, GwySpectrum, i);

    return spec->ydata;
}

/**
 * gwy_spectra_set_spectrum:
 * @spectra: A spectra object.
 * @i: Index of a spectrum to replace
 * @new_spectrum: A #GwyDataLine Object containing the new spectrum.
 *
 * Replaces the ith spectrum in the spectra object with a the
 * supplied spectrum, new_spectrum. It takes its own reference
 * to the New_Spectrum dataline.
 *
 * Since: 2.6
 **/
void
gwy_spectra_set_spectrum(GwySpectra *spectra,
                         guint i,
                         GwyDataLine *new_spectrum)
{
    GwySpectrum *spec;
    GwyDataLine* data_line;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(GWY_IS_DATA_LINE(new_spectrum));
    g_return_if_fail(i <= spectra->spectra->len);

    spec = &g_array_index(spectra->spectra, GwySpectrum, i);
    data_line = spec->ydata;
    g_object_ref(new_spectrum);
    g_object_unref(spec->ydata);
    spec->ydata = new_spectrum;
}

/**
 * gwy_spectra_get_n_spectra:
 * @spectra: A spectra object.
 *
 * Gets the number of spectra in a spectra object.
 *
 * Returns: The number of spectra.
 *
 * Since: 2.6
 **/
guint
gwy_spectra_get_n_spectra(GwySpectra *spectra)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), 0);

    return spectra->spectra->len;
}

/**
 * gwy_spectra_add_spectrum:
 * @spectra: A spectra object.
 * @new_spectrum: A GwyDataLine containing the spectrum to append.
 * @x: The physical x coordinate of the location of the spectrum.
 * @y: The physical y coordinate of the location of the spectrum.
 *
 * Appends a new_spectrum to the spectra collection with a position of x, y.
 * gwy_spectra_add takes a refference to the supplied spectrum.
 *
 * Since: 2.6
 **/
void
gwy_spectra_add_spectrum(GwySpectra *spectra,
                         GwyDataLine *new_spectrum,
                         gdouble x, gdouble y)
{
    GwySpectrum spec;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(GWY_IS_DATA_LINE(new_spectrum));

    g_object_ref(new_spectrum);
    spec.x = x;
    spec.y = y;
    spec.ydata = new_spectrum;
    g_array_append_val(spectra->spectra, spec);
}

/**
 * gwy_spectra_remove:
 * @spectra: A spectra object.
 * @i: Index of spectrum to remove.
 *
 * Removes the ith spectrum from the Spectra collection. The subsequent
 * spectra are moved down one place.
 *
 * Since: 2.6
 **/
void
gwy_spectra_remove_spectrum(GwySpectra *spectra,
                            guint i)
{
    GwySpectrum *spec;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(i <= spectra->spectra->len);

    spec = &g_array_index(spectra->spectra, GwySpectrum, i);
    g_object_unref(spec->ydata);

    g_array_remove_index(spectra->spectra, i);
}

/* FIXME: Uncomment once it does anything. */
#if 0
/* function to calculate the angle A ^B^ C
 * points is an array in the format:
 * Ax, Ay, Bx, By, Cx, Cy
 */
static gdouble
calc_angle(const gdouble* points)
{
    gdouble Ax, Ay, Bx, By, Cx, Cy;
    gdouble aa, bb, cc, beta;

    Ax = points[0];
    Ay = points[1];
    Bx = points[2];
    By = points[3];
    Cx = points[4];
    Cy = points[5];

    aa = (Bx-Cx)*(Bx-Cx)+(By-Cy)*(By-Cy);
    bb = (Ax-Cx)*(Ax-Cx)+(Ay-Cy)*(Ay-Cy);
    cc = (Ax-Bx)*(Ax-Bx)+(Ay-By)*(Ay-By);

    beta = acos((aa+cc-bb)/(sqrt(4*aa*cc)));
    return beta;
}

static gdouble*
tri_interp_weight(gdouble x, gdouble y,
                  const gdouble* triangle)
{
    /* TODO: Function to get the interpolation weightings
             between three points*/
    return NULL;
}

/**
 * gwy_spectra_get_spectra_interp:
 * @spectra: A spectra object.
 * @x: x coordinate for spectrum
 * @y: y coordinate fro spectrum
 *
 * In future will:
 * Interpolates a spectrum at some arbitary point specified by x,y.
 * If the point is within the triangle made by three measured spectra
 * then these are used else it is interpolated from the nearest two or one.
 * But currently:
 * Returns a newly created dataline, corresponding to the nearest spectrum.
 *
 * Returns: A newly created dataline.
 **/
GwyDataLine*
gwy_spectra_get_spectra_interp(GwySpectra *spectra,
                               gdouble x, gdouble y)
{
    guint i;
    /* TODO: Function to do a proper interpolation. */
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);
    if (spectra->ncurves == 0) {
        g_critical("Contains no spectra");
        return NULL;
    }
    i = gwy_spectra_xytoi(spectra, x, y);

    return gwy_data_line_duplicate(gwy_spectra_get_spectrum(spectra, i));
}
#endif

/**
 * gwy_spectra_get_title:
 * @spectra: A spectra object.
 *
 * Gets the title of spectra.
 *
 * Returns: A pointer to the title string (owned by the spectra object).
 *
 * Since: 2.6
 **/
const gchar*
gwy_spectra_get_title(GwySpectra *spectra)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);
    return spectra->title;
}

/**
 * gwy_spectra_set_title:
 * @spectra: A spectra object.
 * @title: The new title string.
 *
 * Sets the title of the spectra collection.
 *
 * Since: 2.6
 **/
void
gwy_spectra_set_title(GwySpectra *spectra,
                      const gchar *title)
{
    gchar *old;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));

    old = spectra->title;
    spectra->title = g_strdup(title);
    g_free(old);

    g_object_notify(G_OBJECT(spectra), "title");
}

/**
 * gwy_spectra_clear:
 * @spectra: A spectra object.
 *
 * Removes all spectra from the collection.
 *
 * Since: 2.6
 **/
void
gwy_spectra_clear(GwySpectra *spectra)
{
    guint i;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));

    for (i = 0; i < spectra->spectra->len; i++) {
        GwySpectrum *spec = &g_array_index(spectra->spectra, GwySpectrum, i);

        g_object_unref(spec->ydata);
    }
    g_array_set_size(spectra->spectra, 0);
}

/************************** Documentation ****************************/

/**
 * SECTION:Spectra
 * @title: GwySpectra
 * @short_description: Collection of dataline representing point spectra.
 *
 * #GwySpectra contains an array of #GwyDataLine<!-- -->s and coordinates
 * representing where in a datafield the spectrum was aquired.
 **/

/**
 * GwySpectra:
 *
 * The #GwySpectra struct contains private data only and should be accessed
 * using the functions below.
 *
 * Since: 2.6
 **/

/**
 * gwy_spectra_duplicate:
 * @spectra: A Spectra object to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 *
 * Since: 2.6
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
