/*
 *  $Id: spectra.c 6957 2006-11-08 14:44:11Z owaind $
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
#define DEBUG

#include "config.h"
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
    DATA_CHANGED,
    LAST_SIGNAL
};

static void        gwy_spectra_finalize           (GObject *object);
static void        gwy_spectra_serializable_init  (GwySerializableIface *iface);
static GByteArray* gwy_spectra_serialize          (GObject *obj,
                                                   GByteArray *buffer);
static gsize       gwy_spectra_get_size           (GObject *obj);
static GObject*    gwy_spectra_deserialize        (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject*    gwy_spectra_duplicate_real     (GObject *object);
static void        gwy_spectra_clone_real         (GObject *source,
                                                   GObject *copy);

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

/**
 * GwySpectra::data-changed:
 * @gwydataline: The #GwySpectra which received the signal.
 *
 * The ::data-changed signal is never emitted by the spectra itself.  It
 * is intended as a means to notify other spectra users they should
 * update themselves.
 */
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
}

static void
gwy_spectra_finalize(GObject *object)
{
    gint i;
    GwySpectra *spectra = (GwySpectra*)object;
    GwyDataLine *spec;

    gwy_object_unref(spectra->si_unit_xy);
    gwy_debug("");

    for (i = 0; i < spectra->ncurves; i++) {
        spec = spectra->data[i];
        gwy_object_unref(spec);
    }
    g_free(spectra->data);
    g_free(spectra->coords);
    g_free(spectra->title);
    G_OBJECT_CLASS(gwy_spectra_parent_class)->finalize(object);
}

/**
 * gwy_spectra_new:
 *
 * Creates a new Spectra object containing zero spectra.
 *
 * Returns: A newly created spectra.
 **/

GwySpectra*
gwy_spectra_new() {

    GwySpectra *spectra;

    gwy_debug("");
    spectra = g_object_new(GWY_TYPE_SPECTRA, NULL);
    spectra->data = g_new0(GwyDataLine*, DEFAULT_ALLOC_SIZE);
    spectra->coords =  g_new0(gdouble, 2*DEFAULT_ALLOC_SIZE);
    spectra->nalloc = DEFAULT_ALLOC_SIZE;
    spectra->ncurves = 0;
    spectra->title = NULL;

    return spectra;
}

/**
 * gwy_spectra_new_alike:
 * @model: A Spectra object to take units from.
 *
 * Creates a new Spectra object similar to an existing one, but containing zero
 * spectra. The same amount of memory preallocated to the arrays, but they
 * contain zero.
 *
 * Use gwy_spectra_duplicate() if you want to copy a spectra object including
 * the spectra in it.
 *
 * Returns: A newly created Spectra object.
 **/
GwySpectra*
gwy_spectra_new_alike(GwySpectra *model)
{
    GwySpectra *spectra;

    g_return_val_if_fail(GWY_IS_SPECTRA(model), NULL);
    spectra = g_object_new(GWY_TYPE_SPECTRA, NULL);

    spectra->data = g_new0(GwyDataLine*, model->nalloc);
    spectra->coords = g_new0(gdouble, model->nalloc*2);
    spectra->nalloc = model->nalloc;
    spectra->ncurves = 0;
    if (model->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_duplicate(model->si_unit_xy);

    return spectra;
}


static GByteArray*
gwy_spectra_serialize(GObject *obj,
                        GByteArray *buffer)
{
    GwySpectra *spectra;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SPECTRA(obj), NULL);

    spectra = GWY_SPECTRA(obj);
    if (!spectra->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_new("");
    {
        guint ncoords = spectra->ncurves*2;
        GwySerializeSpec spec[] = {
            { 's', "title", &spectra->title, NULL, },
            { 'D', "coords", &spectra->coords, &ncoords, },
            { 'O', "data", &spectra->data, &spectra->ncurves, },
            { 'i', "ncurves", &spectra->ncurves, NULL, },
            { 'o', "si_unit_xy", &spectra->si_unit_xy, NULL, },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_SPECTRA_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_spectra_get_size(GObject *obj)
{
    GwySpectra *spectra;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SPECTRA(obj), 0);

    spectra = GWY_SPECTRA(obj);
    if (!spectra->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_new("");
    {
        guint ncoords = spectra->ncurves*2;

        GwySerializeSpec spec[] = {
            { 's', "title", &spectra->title, NULL, },
            { 'D', "coords", &spectra->coords, &ncoords, },
            { 'O', "data", &spectra->data, &spectra->ncurves, },
            { 'i', "ncurves", &spectra->ncurves, NULL, },
            { 'o', "si_unit_xy", &spectra->si_unit_xy, NULL, },
        };

        return gwy_serialize_get_struct_size(GWY_SPECTRA_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_spectra_deserialize(const guchar *buffer,
                        gsize size,
                        gsize *position)
{
    guint32 ncoords, ncurves, n_spectrum;
    gdouble *coords = NULL;
    GwySIUnit *si_unit_xy = NULL;
    GwyDataLine **data = NULL;
    GwySpectra *spectra;
    gchar* title = NULL;

    GwySerializeSpec spec[] = {
      { 's', "title", &title, NULL, },
      { 'D', "coords", &coords, &ncoords, },
      { 'O', "data", &data, &n_spectrum, },
      { 'i', "ncurves", &ncurves, NULL, },
      { 'o', "si_unit_xy", &si_unit_xy, NULL, },
    };

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_SPECTRA_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(coords);
        g_free(data);
        gwy_object_unref(si_unit_xy);
        return NULL;
    }
    if (!((2*ncurves == ncoords) && (ncurves == n_spectrum))) {
        g_critical("Serialized %s size mismatch (%u,%u,%u) not equal.",
              GWY_SPECTRA_TYPE_NAME, ncurves, ncoords, n_spectrum);
        g_free(coords);
        g_free(data);
        gwy_object_unref(si_unit_xy);
        return NULL;
    }

    spectra = gwy_spectra_new();

    spectra->title = title;
    g_free(spectra->data);
    spectra->data = data;
    g_free(spectra->coords);
    spectra->coords = coords;
    spectra->ncurves = ncurves;
    spectra->nalloc = ncurves;

    if (si_unit_xy) {
        if (spectra->si_unit_xy != NULL)
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
    GwyDataLine *data_line;

    g_return_val_if_fail(GWY_IS_DATA_LINE(object), NULL);
    spectra = GWY_SPECTRA(object);
    duplicate = gwy_spectra_new_alike(spectra);
    /* duplicate the co-ordinates of the spectra */
    memcpy(duplicate->coords,
           spectra->coords,
           spectra->ncurves*2*sizeof(gdouble));

    /* Duplicate the spectra themselves */
    for (i = 0; i < spectra->ncurves; i++) {
        data_line = spectra->data[i];
        /* following safe because the duplicate spectra obj was created
           with gwy_spectra_new_alike */
        duplicate->data[i] = gwy_data_line_duplicate(data_line);
    }
    duplicate->ncurves = spectra->ncurves;
    return (GObject*)duplicate;
}

static void
gwy_spectra_clone_real(GObject *source, GObject *copy)
{
    GwySpectra *spectra, *clone;
    GwyDataLine *data_line;
    guint i;


    g_return_if_fail(GWY_IS_SPECTRA(source));
    g_return_if_fail(GWY_IS_SPECTRA(copy));

    spectra = GWY_SPECTRA(source);
    clone = GWY_SPECTRA(copy);

    /* Remove any existing datalines in the clone */
    for (i = 0; i < clone->ncurves; i++) {
        g_object_unref(clone->data[i]);
    }

    /* Ensure there is space in clone and Grow the clone if necessary */
    if (clone->nalloc < spectra->ncurves) {
        clone->coords = g_renew(gdouble, clone->coords, spectra->nalloc*2);
        clone->data = g_renew(GwyDataLine*, clone->data, spectra->nalloc);
    }
    /* copy the spectra coordinate to clone */
    memcpy(clone->coords, spectra->coords, spectra->ncurves*sizeof(gdouble));
    clone->ncurves = spectra->ncurves;

    /* and then the spectra theselves */
    for (i = 0; i < spectra->ncurves; i++) {
        data_line = spectra->data[i];
        clone->data[i] = gwy_data_line_duplicate(data_line);
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
 **/
void
gwy_spectra_data_changed(GwySpectra *spectra)
{
    g_signal_emit(spectra, spectra_signals[DATA_CHANGED], 0);
}

/**
 * gwy_spectra_copy:
 * @a: Source Spectra object.
 * @b: Destination Spectra object.
 *
 * Copies the contents of a spectra object a to another already allocated
 * spectra object b. It employs the clone method, and any data in b is lost.
 *
 **/
void
gwy_spectra_copy(GwySpectra *a, GwySpectra *b)
{
    g_return_if_fail(GWY_IS_SPECTRA(a));
    g_return_if_fail(GWY_IS_SPECTRA(a));

    gwy_spectra_clone_real((GObject*)a, (GObject*)b);
}

/**
 * gwy_spectra_get_si_unit_xy:
 * @spectra: A spectra.
 *
 * Returns SI unit used for the location co-ordinates of spectra.
 *
 * Returns: SI unit corresponding to the  the location co-ordinates of spectra
 * object. Its reference count is not incremented.
 **/
GwySIUnit*
gwy_spectra_get_si_unit_xy(GwySpectra *spectra)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);

    if (!spectra->si_unit_xy)
        spectra->si_unit_xy = gwy_si_unit_new("");

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
 * @spectra: A Spectra Object
 * @i: index of a spectrum
 *
 * Gets the coordinates of the of the spectrum.
 *
 * Returns: An constant array of two elements. It is a refference to
            the raw data and does not need to be freed.
 **/
const gdouble*
gwy_spectra_itoxy (GwySpectra *spectra, guint i)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);

    if (i >= spectra->ncurves) {
        return NULL;
    }
    return spectra->coords + i*2;
}

/**
 * gwy_spectra_xytoi:
 * @spectra: A Spectra Object
 * @real_x: The x coordinate of the location of the spectrum.
 * @real_y: The y coordinate of the location of the spectrum.
 *
 * Finds the index of the spectrum closest to the location specified by
 * the coordinated x and y.
 *
 * Returns: The index of the nearest spectrum.
 **/
guint
gwy_spectra_xytoi (GwySpectra *spectra, gdouble real_x, gdouble real_y)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra),  0);

    if (spectra->ncurves == 0) {
        g_critical("Spectra Object contains no spectra.");
        return 0;
    }
    return gwy_math_find_nearest_point(real_x,
                                       real_y,
                                       NULL,
                                       spectra->ncurves,
                                       spectra->coords,
                                       NULL);
}

gint CompFunc_r(gconstpointer a, gconstpointer b) {
    coord_pos *A, *B;
    A = (coord_pos*)a;
    B = (coord_pos*)b;

    if (A->r < B->r)
        return -1;
    if (A->r > B->r)
        return 1;
    return 0; /* Equal */
}

/**
 * gwy_spectra_nearest:
 * @spectra: A Spectra Object
 * @plist:  pointer to a NULL pointer where the list will be allocated.
 * @real_x: The x coordinate.
 * @real_y: The y coordinate.
 *
 * Gets a list of the indices to spectra ordered by their
 * distance from x_real, y_real. A newly created array is allocated and the
 * list of indicies is stored there.The calling function must ensure
 * the memory is freed once the list is finished with.
 *
 * Returns: The number of elements in the @plist array.
 **/
gint
gwy_spectra_nearest (GwySpectra *spectra,
                     guint** plist,
                     gdouble real_x,
                     gdouble real_y)
{
    GArray* points;
    guint i = 0;
    coord_pos item;
    gdouble x, y;

    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), 0);
    g_return_val_if_fail(*(plist) == NULL, 0);

    if (spectra->ncurves == 0) {
        g_critical("Spectra Object contains no spectra.");
        return 0;
    }
    if (plist == NULL) {
        g_critical("Nowhere to create array");
        return 0;
    }

    points = g_array_sized_new(FALSE,
                               TRUE,
                               sizeof(coord_pos),
                               spectra->ncurves);
    for (i = 0; i < spectra->ncurves; i++) {
        x = spectra->coords[2*i];
        y = spectra->coords[2*i + 1];
        item.index = i;
        item.r = sqrt((x-real_x)*(x-real_x)+(y-real_y)*(y-real_y));
        g_array_append_val(points, item);
    }
    g_array_sort(points, CompFunc_r);
    *(plist) = g_new(guint, points->len);
    for (i = 0; i < points->len; i++)
        *(*(plist)+i) = (g_array_index(points, coord_pos, i)).r;

    g_array_free(points, TRUE);

    return points->len;
}

/**
 * gwy_spectra_setpos:
 * @spectra: A Spectra Object
 * @real_x: The new x coordinate of the location of the spectrum.
 * @real_y: The new y coordinate of the location of the spectrum.
 * @i: The index of a spectrum.
 *
 * Sets the location coordinates of a spectrum.
 *
 **/
void
gwy_spectra_setpos (GwySpectra *spectra,
                    gdouble real_x,
                    gdouble real_y, guint i)
{
    g_return_if_fail(GWY_IS_SPECTRA(spectra));

    if (i >= spectra->ncurves) {
        g_warning("setpos: Index i out of range.");
        return;
    }

    spectra->coords[2*i] = real_x;
    spectra->coords[2*i + 1] = real_y;
    return;
}

/**
 * gwy_spectra_get_spectrum:
 * @spectra: A Spectra object
 * @i: Index of a spectrum
 *
 * Gets a dataline that contains the spectrum at index i.
 *
 * Returns: A #GwyDataLine containing the spectrum, and increases
 * reference count.
 **/
GwyDataLine*
gwy_spectra_get_spectrum (GwySpectra *spectra, gint i)
{
    GwyDataLine* data_line;
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);

    if (i >= spectra->ncurves) {
        g_critical("Invalid spectrum index");
        return NULL;
    }
    data_line = spectra->data[i];
    g_object_ref(data_line);
    return data_line;
}

/**
 * gwy_spectra_set_spectrum:
 * @spectra: A Spectra Object
 * @i: Index of a spectrum to replace
 * @new_spectrum: A #GwyDataLine Object containing the new spectrum.
 *
 * Replaces the ith spectrum in the spectra object with a the
 * supplied spectrum, new_spectrum. It takes its own reference
 * to the New_Spectrum dataline.
 *
 **/
void
gwy_spectra_set_spectrum (GwySpectra *spectra,
                          guint i,
                          GwyDataLine *new_spectrum)
{
    GwyDataLine* data_line;
    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(GWY_IS_DATA_LINE(new_spectrum));

    if (i >= spectra->ncurves) {
        g_warning("Invalid spectrum index");
        return;
    }

    g_object_ref(new_spectrum);
    data_line = spectra->data[i];
    g_object_unref(data_line);
    spectra->data[i] = new_spectrum;
}

/**
 * gwy_spectra_n_spectra:
 * @spectra: A Spectra Object
 *
 * Returns: The number of spectra in a spectra object.
 *
 **/
guint
gwy_spectra_n_spectra (GwySpectra *spectra)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), 0);
    return spectra->ncurves;
}

/**
 * gwy_spectra_add_spectrum:
 * @spectra: A Spectra Object
 * @new_spectrum: A GwyDataLine containing the spectrum to append.
 * @real_x: The x coordinate of the location of the spectrum.
 * @real_y: The y coordinate of the location of the spectrum.
 *
 * Appends a new_spectrum to the spectra collection with a position of x, y.
 * gwy_spectra_add takes a refference to the supplied spectrum.
 **/
void
gwy_spectra_add_spectrum (GwySpectra *spectra, GwyDataLine *new_spectrum,
                          gdouble x, gdouble y)
{
    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    g_return_if_fail(GWY_IS_DATA_LINE(new_spectrum));

    if (spectra->ncurves == spectra->nalloc) {
        spectra->coords = g_renew(gdouble, spectra->coords,
                                  (spectra->nalloc + DEFAULT_ALLOC_SIZE)*2);
        spectra->data = g_renew(GwyDataLine*, spectra->data,
                                spectra->nalloc + DEFAULT_ALLOC_SIZE);
        spectra->nalloc += DEFAULT_ALLOC_SIZE;
    }

    spectra->coords[spectra->ncurves*2] = x;
    spectra->coords[spectra->ncurves*2 + 1] = y;
    spectra->data[spectra->ncurves] = new_spectrum;
    g_object_ref(new_spectrum);
    spectra->ncurves++;
}

/**
 * gwy_spectra_remove:
 * @spectra: A Spectra Object
 * @i: Index of spectrum to remove.
 *
 * Removes the ith spectrum from the Spectra collection. The subsequent
 * spectra are suffled up one place.
 **/
void gwy_spectra_remove_spectrum (GwySpectra *spectra, guint i)
{
    GwyDataLine* data_line;
    guint j;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));
    if (i >= spectra->ncurves) {
        g_critical("Invalid spectrum index");
        return;
    }

    data_line = spectra->data[i];
    g_object_unref(data_line);

    for (j = i+1; j < spectra->ncurves; j++) {
        spectra->data[j-1] = spectra->data[j];
        spectra->coords[2*(j-1)] = spectra->coords[2*j];
        spectra->coords[2*(j-1)+1] = spectra->coords[2*j+1];
    }
    spectra->ncurves--;

}


/* function to calculate the angle A ^B^ C
 * points is an array in the format:
 * Ax, Ay, Bx, By, Cx, Cy
 */
gdouble calc_angle(const gdouble* points)
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

gdouble* tri_interp_weight(gdouble x, gdouble y,
                           const gdouble* triangle)
{
    /* TODO: Function to get the interpolation weightings
             between three points*/
    return NULL;
}

/**
 * gwy_spectra_get_spectra_interp:
 * @spectra: A Spectra Object
 * @x: x coordinate for spectrum
 * @y: y coordinate fro spectrum
 *
 * In future will:
 * Interpolates a spectrum at some arbitary point specified by x,y.
 * If the point is within the triangle made by three measured spectra
 * then these are used else it is interpolated from the nearest two or one.
 * But currently:
 * Returns a newly created dataline, corresponding to the nearest spectrum.
 * Returns: A newly created dataline.
 **/
GwyDataLine*
gwy_spectra_get_spectra_interp (GwySpectra *spectra, gdouble x, gdouble y)
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

/**
 * gwy_spectra_get_title:
 * @spectra: A Spectra Object
 *
 * Returns: A pointer to the title string.
 **/
const gchar*
gwy_spectra_get_title (GwySpectra *spectra)
{
    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), NULL);
    return spectra->title;
}
/**
 * gwy_spectra_set_title:
 * @spectra: A Spectra Object
 * @new_title: The title string
 *
 * Sets the title of the spectra collection. The object takes ownership
 * of the new string and it is freed when the object is finalised. Title
 * strings should be dynamically allocated.
 **/
void
gwy_spectra_set_title (GwySpectra *spectra, gchar *new_title)
{
    g_return_if_fail(GWY_IS_SPECTRA(spectra));

    g_free(spectra->title);
    spectra->title = new_title;
}

/**
 * gwy_spectra_clear:
 * @spectra: A Spectra Object
 *
 * Removes all spectra from the collection.
 * Returns: A newly created dataline.
 **/
void
gwy_spectra_clear (GwySpectra *spectra)
{
    int i;
    GwyDataLine* spec;

    g_return_if_fail(GWY_IS_SPECTRA(spectra));

    for (i = 0; i < spectra->ncurves; i++) {
        spec = spectra->data[i];
        gwy_object_unref(spec);
    }
    spectra->data = g_renew(GwyDataLine*,
                            spectra->data,
                            DEFAULT_ALLOC_SIZE);
    spectra->coords = g_renew(gdouble,
                              spectra->coords,
                              DEFAULT_ALLOC_SIZE);
    spectra->nalloc = DEFAULT_ALLOC_SIZE;
    spectra->ncurves = 0;

    return;
}

/************************** Documentation ****************************/

/**
 * SECTION:Spectra
 * @title: GwySpectra
 * @short_description: Collection of dataline representing point spectra.
 *
 * #GwySpectra contains an array of GwyDataLines and coordinates representing
 * where in a datafield the spectrum was aquired.
 **/

/**
 * GwySpectra:
 *
 * The #GwySpectra struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * gwy_spectra_duplicate:
 * @spectra: A Spectra object to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
