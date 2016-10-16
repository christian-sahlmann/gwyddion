/*
 *  @(#) $Id$
 *  Copyright (C) 2004-2016 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/linestats.h>
#include <libprocess/stats.h>
#include <libprocess/grains.h>
#include <libprocess/correct.h>
#include <libprocess/interpolation.h>

enum { NONE = G_MAXUINT };

typedef enum {
    UP,
    RIGHT,
    DOWN,
    LEFT,
    NDIRECTIONS
} LaplaceDirection;

typedef struct {
    guint col;
    guint row;
    guint width;
    guint height;
} GwyDataFieldPart;

/*
 * @len: The number of blocks.  (The allocated size is usually a bit larger.)
 * @n: Start of each neighbour in @k and @w, size @len+1.
 * @k: Indices of neighbours for laplacian calculation, allocated size is a
 *     multiple of @len but blocks are variable-size, given by @n.  These
 *     indices refer to these arrays, not the original grid.
 * @w: Coefficient blocks (for calculation of the second derivative from
 *     neigbours), allocated size is a
 *     multiple of @len but blocks are variable-size, given by @n.
 * @z: Values of the points, size @len.
 * @rhs: Right-hand-sides of the points, size @len.
 * @f: The difference between the second derivative and the value, size @len.
 * @v: Conjugate-gradients auxiliary vector, size @len.
 * @t: Conjugate-gradients auxiliary vector, size @len.
 * @gindex: Where the point is placed in the original data, size @len.
 */
/* FIXME: Most of the coefficient sets in @w are repeated many times.  We could
 * consilidate them, replacing 3-5 doubles (typically) per iterator with one
 * integer.
 */
typedef struct {
    gsize len;
    gsize int_size;
    gsize float_size;
    guint *n;
    guint *k;
    gdouble *w;
    gdouble *z;
    gdouble *rhs;
    gdouble *f;
    gdouble *v;
    gdouble *t;
    guint *gindex;
} LaplaceIterators;

typedef struct {
    gboolean is_virtual : 1;
    gboolean is_boundary : 1;
    gboolean is_rhs : 1;

    guint bdist;     // Distance of the boundary line where ∂z/∂x = 0
    guint step;
    guint neighbour;
    guint neighbour2;

    gdouble rhs;     // Remember the exterior data used for rhs
    gdouble weight;  // Coefficient before (z_neighbour - z_0)
    gdouble weight2; // Coefficient before (z_neighbour2 - z_0)
} LaplaceNeighbour;

static void    gwy_data_field_distort_internal(GwyDataField *source,
                                               GwyDataField *dest,
                                               GwyInterpolationType interp,
                                               GwyExteriorType exterior,
                                               gdouble fill_value,
                                               const GwyXY *coords,
                                               GwyCoordTransform2DFunc invtrans,
                                               gpointer user_data);
static gdouble unrotate_refine_correction     (GwyDataLine *derdist,
                                               guint m,
                                               gdouble phi);
static void    compute_fourier_coeffs         (gint nder,
                                               const gdouble *der,
                                               guint symmetry,
                                               gdouble *st,
                                               gdouble *ct);
static void    interpolate_segment            (GwyDataLine *data_line,
                                               gint from,
                                               gint to);

/**
 * gwy_data_field_correct_laplace_iteration:
 * @data_field: Data field to be corrected.
 * @mask_field: Mask of places to be corrected.
 * @buffer_field: Initialized to same size as mask and data.
 * @error: Maximum change within last step.
 * @corrfactor: Correction factor within step.
 *
 * Performs one interation of Laplace data correction.
 *
 * Tries to remove all the points in mask off the data by using
 * iterative method similar to solving heat flux equation.
 *
 * Use this function repeatedly until reasonable @error is reached.
 *
 * <warning>For almost all purposes this function was superseded by
 * non-iteratie gwy_data_field_laplace_solve() which is simultaneously much
 * faster and more accurate.</warning>
 **/
void
gwy_data_field_correct_laplace_iteration(GwyDataField *data_field,
                                         GwyDataField *mask_field,
                                         GwyDataField *buffer_field,
                                         gdouble corrfactor,
                                         gdouble *error)
{
    gint xres, yres, i, j;
    const gdouble *mask, *data;
    gdouble *buffer;
    gdouble cor, cf, err;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(mask_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(buffer_field));
    g_return_if_fail(data_field->xres == mask_field->xres
                     && data_field->yres == mask_field->yres);

    xres = data_field->xres;
    yres = data_field->yres;

    /* check buffer field */
    if (buffer_field->xres != xres || buffer_field->yres != yres)
        gwy_data_field_resample(buffer_field, xres, yres,
                                GWY_INTERPOLATION_NONE);

    gwy_data_field_copy(data_field, buffer_field, FALSE);

    data = data_field->data;
    buffer = buffer_field->data;
    mask = mask_field->data;

    /* set boundary condition for masked boundary data */
    if (yres >= 2) {
        for (i = 0; i < xres; i++) {
            if (mask[i] > 0)
                buffer[i] = buffer[i + xres];
            if (mask[i + xres*(yres - 1)] > 0)
                buffer[i + xres*(yres - 1)] = buffer[i + xres*(yres - 2)];
        }
    }
    if (xres >= 2) {
        for (i = 0; i < yres; i++) {
            if (mask[xres*i] > 0)
                buffer[xres*i] = buffer[1 + xres*i];
            if (mask[xres - 1 + xres*i] > 0)
                buffer[xres - 1 + xres*i] = buffer[xres-2 + xres*i];
        }
    }

    /* iterate */
    err = 0.0;
    cf = corrfactor;
    for (i = 1; i < yres - 1; i++) {
        for (j = 1; j < xres - 1; j++) {
            if (mask[i*xres + j] > 0) {
                cor = cf*((data[(i - 1)*xres + j] + data[(i + 1)*xres + j]
                           - 2*data[i*xres + j])
                          + (data[i*xres + j - 1] + data[i*xres + j + 1]
                             - 2*data[i*xres + j]));

                buffer[i*xres + j] += cor;
                cor = fabs(cor);
                if (cor > err)
                    err = cor;
            }
        }
    }

    gwy_data_field_copy(buffer_field, data_field, FALSE);
    gwy_data_field_invalidate(data_field);

    if (error)
        *error = err;
}

/***************************************************************************
 *
 * Efficient Laplace interpolation.
 *
 ***************************************************************************/

static gboolean
promote(const guint *levels, guint *buffer,
        guint xres, guint yres,
        guint level, guint step)
{
    guint nx = (xres + step-1)/step, ny = (yres + step-1)/step;
    guint vstep = xres*step;
    gboolean ok = FALSE;
    guint i, j;

    if (nx < 3 || ny < 3)
        return ok;

    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            guint k = (i*xres + j)*step;

            if (levels[k] == level
                && (!i || levels[k-vstep] == level)
                && (!j || levels[k-step] == level)
                && (j == nx-1 || levels[k+step] == level)
                && (i == ny-1 || levels[k+vstep] == level)) {
                buffer[k] = level+1;
                ok = TRUE;
            }
        }
    }

    return ok;
}

static void
demote(const guint *levels, guint *buffer,
       guint xres, guint yres,
       guint level, guint step)
{
    guint nx = (xres + step-1)/step, ny = (yres + step-1)/step;
    guint vstep = xres*step;
    guint i, j;

    if (nx < 3 || ny < 3)
        return;

    for (i = 1; i < ny-1; i++) {
        for (j = 1; j < nx-1; j++) {
            guint k = (i*xres + j)*step;

            if (levels[k] == level
                && (levels[k-vstep-step] == level-1
                    || levels[k-vstep] == level-1
                    || levels[k-vstep+step] == level-1
                    || levels[k-step] == level-1
                    || levels[k+step] == level-1
                    || levels[k+vstep-step] == level-1
                    || levels[k+vstep] == level-1
                    || levels[k+vstep+step] == level-1)) {
                if (buffer[k-vstep-step] > level)
                    buffer[k-vstep-step] = level;
                if (buffer[k-vstep] > level)
                    buffer[k-vstep] = level;
                if (buffer[k-vstep+step] > level)
                    buffer[k-vstep+step] = level;
                if (buffer[k-step] > level)
                    buffer[k-step] = level;
                if (buffer[k+step] > level)
                    buffer[k+step] = level;
                if (buffer[k+vstep-step] > level)
                    buffer[k+vstep-step] = level;
                if (buffer[k+vstep] > level)
                    buffer[k+vstep] = level;
                if (buffer[k+vstep+step] > level)
                    buffer[k+vstep+step] = level;
            }
        }
    }
}

static gboolean
reduce(const guint *levels, guint *buffer,
       guint xres, guint yres,
       guint level, guint step)
{
    guint nx = (xres + step-1)/step, ny = (yres + step-1)/step;
    guint halfstep = step/2;
    guint vstep = xres*step, vhalfstep = xres*halfstep;
    gboolean ok = FALSE;
    gboolean right = (nx - 1)*step + halfstep < xres;
    gboolean down = (ny - 1)*step + halfstep < yres;
    guint i, j;

    g_return_val_if_fail(step % 2 == 0, FALSE);

    if (nx < 3 || ny < 3)
        return ok;

    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            guint k = (i*xres + j)*step;

            if (levels[k] == level
                && (!i || !j || levels[k-vstep-step] >= level)
                && (!i || levels[k-vstep] == level)
                && (!i || j == nx-1 || levels[k-vstep+step] >= level)
                && (!j || levels[k-step] == level)
                && (j == nx-1 || levels[k+step] == level)
                && (i == ny-1 || !j || levels[k+vstep-step] >= level)
                && (i == ny-1 || levels[k+vstep] == level)
                && (i == ny-1 || j == nx-1 || levels[k+vstep+step] >= level)) {
                buffer[k] = level+1;
                if (i && j)
                    buffer[k-vhalfstep-halfstep] = NONE;
                if (i)
                    buffer[k-vhalfstep] = NONE;
                if (i && (right || j < nx-1))
                    buffer[k-vhalfstep+halfstep] = NONE;
                if (j)
                    buffer[k-halfstep] = NONE;
                if (right || j < nx-1)
                    buffer[k+halfstep] = NONE;
                if ((down || i < ny-1) && j)
                    buffer[k+vhalfstep-halfstep] = NONE;
                if (down || i < ny-1)
                    buffer[k+vhalfstep] = NONE;
                if ((down || i < ny-1) && (right || j < nx-1))
                    buffer[k+vhalfstep+halfstep] = NONE;
                ok = TRUE;
            }
        }
    }

    return ok;
}

static void
remove_spikes(guint *levels,
              guint xres, guint yres,
              guint level, guint step)
{
    guint nx = (xres + step-1)/step, ny = (yres + step-1)/step;
    guint i, j;

    if (nx < 3 || ny < 3)
        return;

    for (i = 1; i < ny-1; i++) {
        for (j = 1; j < nx-1; j++) {
            guint k = (i*xres + j)*step;

            if (levels[k] == level) {
                guint su = (levels[k-xres*step] == NONE),
                      sd = (levels[k+xres*step] == NONE),
                      sl = (levels[k-step] == NONE),
                      sr = (levels[k+step] == NONE);

                if ((su & sd & ~sl & ~sr) || (~su & ~sd & sl & sr))
                    levels[k] = NONE;
            }
        }
    }
}

static guint
build_levels(guint *levels, guint *buffer,
             guint xres, guint yres)
{
    guint step = 1, level = 0;

    gwy_assign(buffer, levels, xres*yres);
    while (TRUE) {
        // Promote odd levels to one-greater even levels if they do not touch
        // lower-values levels.
        level++;
        if (!promote(levels, buffer, xres, yres, level, step))
            break;

        // Ensure a dense representation near the boundary.
        if (level == 1) {
            gwy_assign(levels, buffer, xres*yres);
            demote(levels, buffer, xres, yres, level, step);
        }

        gwy_assign(levels, buffer, xres*yres);
        // Clear the space around even levels and promote them to one-greater
        // odd levels if the do not touch lower levels.
        level++;
        step *= 2;
        if (!reduce(levels, buffer, xres, yres, level, step))
            break;

        // Remove even levels that would have to be interpolated from two
        // opposide sides (they appear when both sides of it are promoted but
        // not the point itself).
        if (level > 1)
            remove_spikes(buffer, xres, yres, level, step/2);

        gwy_assign(levels, buffer, xres*yres);
    }

    return level;
}

static guint
count_grid_points(const guint *levels,
                  guint xres, guint yres)
{
    guint k, npoints = 0;

    for (k = 0; k < xres*yres; k++) {
        if (levels[k] && levels[k] != NONE)
            npoints++;
    }

    return npoints;
}

static void
build_grid_index(const guint *levels,
                 guint xres, guint yres,
                 guint *gindex,
                 guint *revindex)
{
    guint k, n = 0;

    for (k = 0; k < xres*yres; k++) {
        if (levels[k] && levels[k] != NONE) {
            revindex[k] = n;
            gindex[n++] = k;
        }
        else
            revindex[k] = NONE;
    }
}

static void
laplace_iterators_setup(LaplaceIterators *iterators,
                        guint maxneighbours)
{
    gsize len = iterators->len;

    iterators->k = iterators->n + (len + 2);
    iterators->gindex = iterators->k + maxneighbours*len;

    iterators->rhs = iterators->z + len;
    iterators->f = iterators->rhs + len;
    iterators->v = iterators->f + len;
    iterators->t = iterators->v + len;
    iterators->w = iterators->t + len;
}

static void
laplace_iterators_resize(LaplaceIterators *iterators,
                         guint len,
                         guint maxneighbours)
{
    gsize int_size = (maxneighbours + 2)*len + 2;
    gsize float_size = (maxneighbours + 5)*len;

    iterators->len = len;

    if (int_size > iterators->int_size) {
        if (G_UNLIKELY(iterators->int_size))
            g_warning("Laplace iterators need to be enlarged (int).");
        GWY_FREE(iterators->n);
        iterators->n = g_new0(guint, int_size);
        iterators->int_size = int_size;
    }
    else
        gwy_clear(iterators->n, int_size);

    if (float_size > iterators->float_size) {
        if (G_UNLIKELY(iterators->float_size))
            g_warning("Laplace iterators need to be enlarged (float).");
        GWY_FREE(iterators->z);
        iterators->z = g_new0(gdouble, float_size);
        iterators->float_size = float_size;
    }
    else
        gwy_clear(iterators->z, float_size);

    laplace_iterators_setup(iterators, maxneighbours);
}

static LaplaceIterators*
laplace_iterators_new(guint len, guint maxneighbours)
{
    LaplaceIterators *iterators = g_slice_new0(LaplaceIterators);
    laplace_iterators_resize(iterators, len, maxneighbours);
    return iterators;
}

static void
laplace_iterators_free(LaplaceIterators *iterators)
{
    g_free(iterators->z);
    g_free(iterators->n);
    g_slice_free(LaplaceIterators, iterators);
}

static void
analyse_neighbour_direction(const guint *levels,
                            const gdouble *data,
                            gint xres, gint yres,
                            gint xstep, gint ystep,
                            gint j, gint i,
                            const guint *revindex,
                            LaplaceNeighbour *nd)
{
    gint ineigh, jneigh, step;
    gint xorthostep, yorthostep;
    guint kk;

    gwy_clear(nd, 1);
    step = MAX(ABS(xstep), ABS(ystep));
    ineigh = i + ystep;
    jneigh = j + xstep;

    // 1 Primary neighbour.
    // 1.a Neumann boundary.
    // The upper and left boundaries are always aligned.
    if (ineigh < 0) {
        g_assert(i == 0);
        nd->is_boundary = TRUE;
        nd->step = step;
        return;
    }
    if (jneigh < 0) {
        g_assert(j == 0);
        nd->is_boundary = TRUE;
        nd->step = step;
        return;
    }
    // The other boundaries can be unaligned.
    if (ineigh >= yres) {
        nd->is_boundary = TRUE;
        nd->bdist = yres-1 - i;
        nd->step = step;
        return;
    }
    if (jneigh >= xres) {
        nd->is_boundary = TRUE;
        nd->bdist = xres-1 - j;
        nd->step = step;
        return;
    }

    kk = ineigh*xres + jneigh;

    // 1.b Dirichlet boundary.
    if (!levels[kk]) {
        g_assert(step == 1);
        nd->is_rhs = TRUE;
        nd->step = step;
        nd->rhs = data[kk];
        return;
    }

    // 1.c Interior.
    if (levels[kk] != NONE) {
        nd->neighbour = revindex[kk];
        nd->step = step;
        return;
    }

    // 2 Secondary neighbour.
    ineigh = i + 2*ystep;
    jneigh = j + 2*xstep;

    // 2.a Neumann boundary.
    // The upper and left boundaries are always aligned.
    if (ineigh < 0) {
        g_assert(i == 0);
        nd->is_boundary = TRUE;
        nd->step = 2*step;
        return;
    }
    if (jneigh < 0) {
        g_assert(j == 0);
        nd->is_boundary = TRUE;
        nd->step = 2*step;
        return;
    }
    // The other boundaries can be unaligned.
    if (ineigh >= yres) {
        nd->is_boundary = TRUE;
        nd->bdist = yres-1 - i;
        nd->step = 2*step;
        return;
    }
    if (jneigh >= xres) {
        nd->is_boundary = TRUE;
        nd->bdist = xres-1 - j;
        nd->step = 2*step;
        return;
    }

    kk = ineigh*xres + jneigh;
    g_assert(levels[kk]);    // Dirichlet boundary is always at step one.

    // 2.b Interior.
    if (levels[kk] != NONE) {
        nd->neighbour = revindex[kk];
        nd->step = 2*step;
        return;
    }

    // 3 Virtual neighbour.
    xorthostep = xstep ? 0 : ABS(ystep);
    yorthostep = ystep ? 0 : ABS(xstep);
    ineigh = i + 2*ystep - yorthostep;
    jneigh = j + 2*xstep - xorthostep;
    // The upper and left boundaries are always aligned.
    g_assert(ineigh >= 0);
    g_assert(jneigh >= 0);
    kk = ineigh*xres + jneigh;

    nd->is_virtual = TRUE;
    nd->neighbour = revindex[kk];
    nd->step = 2*step;  // The long distance; the short is always half of that.

    ineigh = i + 2*ystep + yorthostep;
    jneigh = j + 2*xstep + xorthostep;
    g_assert(ineigh < yres || jneigh < xres);
    if (ineigh < yres && jneigh < xres) {
        kk = ineigh*xres + jneigh;
        nd->neighbour2 = revindex[kk];
        g_assert(nd->neighbour2 != NONE);
    }
    else {
        nd->is_boundary = TRUE;
        if (ineigh >= yres)
            nd->bdist = yres-1 - i;
        else
            nd->bdist = xres-1 - j;
    }
}

static void
calculate_weights(LaplaceNeighbour *nd)
{
    LaplaceDirection virtual_dir = NDIRECTIONS;
    guint i, ii, ileft, iright, j, boundary_dir;
    gboolean virtual_is_boundary;

    // At most one virtual direction.
    for (j = 0; j < NDIRECTIONS; j++) {
        if (nd[j].is_virtual) {
            g_assert(virtual_dir == NDIRECTIONS);
            virtual_dir = j;
        }
    }

    // No virtual, no mixing of z_xx and z_yy.
    if (virtual_dir == NDIRECTIONS) {
        for (j = 0; j < NDIRECTIONS; j++) {
            guint jj = (j + 2) % NDIRECTIONS;
            gdouble s, xs;

            if (nd[j].is_boundary)
                continue;

            s = nd[j].step;
            xs = nd[jj].is_boundary ? 2*nd[jj].bdist : nd[jj].step;
            nd[j].weight = 2.0/(s + xs)/s;
        }
        return;
    }

    // Virtual.
    i = virtual_dir;
    iright = (i + 1) % NDIRECTIONS;
    ii = (i + 2) % NDIRECTIONS;
    ileft = (i + 3) % NDIRECTIONS;
    boundary_dir = NDIRECTIONS;
    virtual_is_boundary = nd[i].is_boundary;

    // At most one boundary direction, except the boundary direction itself.
    for (j = 0; j < NDIRECTIONS; j++) {
        if (j != virtual_dir && nd[j].is_boundary) {
            g_assert(boundary_dir == NDIRECTIONS);
            boundary_dir = j;
        }
    }
    g_assert(!virtual_is_boundary || (boundary_dir == iright
                                      || boundary_dir == ileft));
    if (boundary_dir == NDIRECTIONS) {
        gdouble s = nd[i].step, ss = nd[ii].step;
        gdouble sleft = nd[ileft].step, sright = nd[iright].step;
        gdouble w = 1.0 - 0.25*s/(s + ss);
        nd[i].weight = nd[i].weight2 = 1.0/(s + ss)/s;
        nd[ii].weight = 2.0/(s + ss)/ss;
        nd[ileft].weight = 2.0*w/(sleft + sright)/sleft;
        nd[iright].weight = 2.0*w/(sleft + sright)/sright;
    }
    else if (boundary_dir == ii) {
        gdouble s = nd[i].step;
        gdouble sleft = nd[ileft].step, sright = nd[iright].step;
        gdouble b = nd[boundary_dir].bdist;
        gdouble w = 1.0 - 0.25*s/(s + 2*b);
        nd[i].weight = nd[i].weight2 = 1.0/(s + 2*b)/s;
        nd[ileft].weight = 2.0*w/(sleft + sright)/sleft;
        nd[iright].weight = 2.0*w/(sleft + sright)/sright;
    }
    else {
        guint irem = (boundary_dir + 2) % NDIRECTIONS;
        gdouble s = nd[i].step, ss = nd[ii].step;
        gdouble srem = nd[irem].step;
        gdouble b = nd[boundary_dir].bdist;
        gdouble w = 1.0 - 0.25*(s + 4*b)/(s + ss);
        nd[i].weight = 2.0/(s + ss)/s;
        nd[ii].weight = 2.0/(s + ss)/ss;
        nd[irem].weight = 2.0*w/(srem + 2*b)/srem;
    }
}

static void
build_iterator(LaplaceNeighbour *nd,
               LaplaceIterators *iterators,
               guint ipt,
               gdouble *nrhs,
               gdouble *rhssum)
{
    guint i, start, npt = 0;
    gdouble ws = 0.0, rs = 0.0;
    gboolean sorted = FALSE;
    guint *iter_k;
    gdouble *iter_w;

    // Figure out how many neighbours we have and sum the weights.
    for (i = 0; i < NDIRECTIONS; i++) {
        if (nd[i].weight) {
            ws += nd[i].weight;
            if (nd[i].is_rhs) {
                g_assert(!nd[i].is_virtual);
                g_assert(!nd[i].is_boundary);
                rs += nd[i].rhs;
                *nrhs += nd[i].weight;
            }
            else
                npt++;

            if (nd[i].weight2) {
                g_assert(nd[i].is_virtual);
                ws += nd[i].weight2;
                npt++;
            }
        }
    }
    g_assert(npt > 0 && npt <= 5);

    start = iterators->n[ipt];
    iterators->n[ipt+1] = start + npt;
    if (rs) {
        *rhssum += rs;
        iterators->rhs[ipt] = rs/ws;
    }

    // Create the iterators.
    iter_w = iterators->w + start;
    iter_k = iterators->k + start;
    for (i = 0; i < NDIRECTIONS; i++) {
        if (!nd[i].is_rhs && nd[i].weight) {
            *(iter_w++) = nd[i].weight/ws;
            *(iter_k++) = nd[i].neighbour;
            if (nd[i].weight2) {
                *(iter_w++) = nd[i].weight2/ws;
                *(iter_k++) = nd[i].neighbour2;
            }
        }
    }

    // Sort the segments by k.
    iter_w = iterators->w + start;
    iter_k = iterators->k + start;
    do {
        sorted = TRUE;
        for (i = 1; i < npt; i++) {
            if (iter_k[i-1] > iter_k[i]) {
                GWY_SWAP(guint, iter_k[i-1], iter_k[i]);
                GWY_SWAP(gdouble, iter_w[i-1], iter_w[i]);
                sorted = FALSE;
            }
        }
    } while (!sorted);
}

static void
build_sparse_iterators(LaplaceIterators *iterators,
                       guint *revindex,
                       const guint *levels,
                       const gdouble *data,
                       guint xres, guint yres)
{
    LaplaceNeighbour nd[NDIRECTIONS];
    guint len = count_grid_points(levels, xres, yres);
    gdouble rhssum = 0.0, nrhs = 0.0;
    const guint *gindex;
    guint ipt;

    laplace_iterators_resize(iterators, len, 5);
    build_grid_index(levels, xres, yres, iterators->gindex, revindex);

    gindex = iterators->gindex;

    for (ipt = 0; ipt < len; ipt++) {
        guint k = gindex[ipt], i = k/xres, j = k % xres;
        gint step = 1 << ((levels[k] - 1)/2);

        analyse_neighbour_direction(levels, data, xres, yres,
                                    0, -step, j, i, revindex, nd + UP);
        analyse_neighbour_direction(levels, data, xres, yres,
                                    step, 0, j, i, revindex, nd + RIGHT);
        analyse_neighbour_direction(levels, data, xres, yres,
                                    0, step, j, i, revindex, nd + DOWN);
        analyse_neighbour_direction(levels, data, xres, yres,
                                    -step, 0, j, i, revindex, nd + LEFT);

        calculate_weights(nd);
        build_iterator(nd, iterators, ipt, &nrhs, &rhssum);
    }

    // Initialise with the mean value of right hand sides, including
    // multiplicity.
    rhssum /= nrhs;
    for (ipt = 0; ipt < len; ipt++)
        iterators->z[ipt] = rhssum;
}

static void
build_dense_iterators(LaplaceIterators *iterators,
                      guint *revindex,
                      const guint *levels,
                      const gdouble *data,
                      guint xres, guint yres)
{
    guint len = count_grid_points(levels, xres, yres);
    const guint *gindex;
    guint ipt;

    laplace_iterators_resize(iterators, len, 4);
    build_grid_index(levels, xres, yres, iterators->gindex, revindex);

    gindex = iterators->gindex;
    for (ipt = 0; ipt < len; ipt++) {
        guint k = gindex[ipt], i = k/xres, j = k % xres, ws = 0, n = 0;
        gdouble rs = 0.0;
        guint *iter_k = iterators->k + iterators->n[ipt];
        gdouble *iter_w = iterators->w + iterators->n[ipt];

        if (i) {
            guint kk = k-xres;
            ws++;
            if (levels[kk]) {
                *(iter_k++) = revindex[kk];
                n++;
            }
            else
                rs += data[kk];
        }
        if (j) {
            guint kk = k-1;
            ws++;
            if (levels[kk]) {
                *(iter_k++) = revindex[kk];
                n++;
            }
            else
                rs += data[kk];
        }
        if (j < xres-1) {
            guint kk = k+1;
            ws++;
            if (levels[kk]) {
                *(iter_k++) = revindex[kk];
                n++;
            }
            else
                rs += data[kk];
        }
        if (i < yres-1) {
            guint kk = k+xres;
            ws++;
            if (levels[kk]) {
                *(iter_k++) = revindex[kk];
                n++;
            }
            else
                rs += data[kk];
        }

        iterators->z[ipt] = data[k];
        iterators->rhs[ipt] = rs/ws;
        iterators->n[ipt+1] = iterators->n[ipt] + n;
        while (n--)
            *(iter_w++) = 1.0/ws;
    }
}

static void
calculate_f(LaplaceIterators *iterators)
{
    const guint *n = iterators->n, *k = iterators->k;
    const gdouble *z = iterators->z, *iz = iterators->z,
                  *w = iterators->w, *rhs = iterators->rhs;
    gdouble *f = iterators->f;
    guint l, ipt;

    for (ipt = iterators->len; ipt; ipt--, n++, z++, rhs++, f++) {
        gdouble lhs = 0.0;
        for (l = *(n + 1) - *n; l; l--, k++, w++)
            lhs += iz[*k]*(*w);
        *f = (*z - lhs) - *rhs;
    }
}

static void
iterate_simple(LaplaceIterators *iterators)
{
    const gdouble *f = iterators->f;
    gdouble *z = iterators->z;
    guint ipt;

    for (ipt = iterators->len; ipt; ipt--, z++, f++)
        *z -= 0.8*(*f);
}

static void
matrix_multiply(LaplaceIterators *iterators, const gdouble *v, gdouble *r)
{
    const guint *n = iterators->n, *k = iterators->k;
    const gdouble *w = iterators->w, *iv = v;
    guint l, ipt;

    for (ipt = iterators->len; ipt; ipt--, n++, v++, r++) {
        gdouble s = 0.0;

        for (l = *(n + 1) - *n; l; l--, k++, w++)
            s += iv[*k]*(*w);

        *r = *v - s;
    }
}

static gboolean
iterate_conj_grad(LaplaceIterators *iterators)
{
    gdouble *z, *v = iterators->v, *t = iterators->t, *f = iterators->f;
    gdouble S = 0.0, phi = 0.0, phiS;
    guint ipt;

    // Temporary quantities: t = A.v, S = v.t, φ = v.f
    matrix_multiply(iterators, iterators->v, iterators->t);

    for (ipt = iterators->len; ipt; ipt--, v++, t++, f++) {
        S += (*v)*(*t);
        phi += (*v)*(*f);
    }

    if (S < 1e-16)
        return TRUE;

    // New value and f = A.z-b
    phiS = phi/S;
    z = iterators->z;
    v = iterators->v;
    f = iterators->f;
    t = iterators->t;
    for (ipt = iterators->len; ipt; ipt--, z++, v++, f++, t++) {
        *z -= phiS*(*v);
        *f -= phiS*(*t);
    }

    // New v
    phi = 0.0;
    f = iterators->f;
    t = iterators->t;
    for (ipt = iterators->len; ipt; ipt--, t++, f++)
        phi += (*t)*(*f);

    phiS = phi/S;
    v = iterators->v;
    f = iterators->f;
    for (ipt = iterators->len; ipt; ipt--, v++, f++)
        *v = *f - phiS*(*v);

    return FALSE;
}

static void
move_result_to_data(const LaplaceIterators *iterators, gdouble *data)
{
    guint ipt;

    for (ipt = 0; ipt < iterators->len; ipt++)
        data[iterators->gindex[ipt]] = iterators->z[ipt];
}

static void
interpolate(guint *levels, gdouble *data, guint xres, guint yres, guint step)
{
    guint nx = (xres + step-1)/step, ny = (yres + step-1)/step,
          vstep = xres*step;
    guint i, j;

    if (nx < 3 || ny < 3)
        return;

    // Six-point interpolation
    for (i = 0; i < ny; i++) {
        if (i % 2 == 0) {
            // Interpolated point horizontally in between two other points.
            for (j = 1; j < nx; j += 2) {
                guint k = (i*xres + j)*step;
                if (levels[k] != NONE)
                    continue;

                if (i >= 2 && i < ny-2 && j < nx-1) {
                    data[k] = (0.375*(data[k-step] + data[k+step])
                               + 0.0625*(data[k-2*vstep-step]
                                         + data[k-2*vstep+step]
                                         + data[k+2*vstep-step]
                                         + data[k+2*vstep+step]));
                    levels[k] = (levels[k-step] + levels[k+step])/2;
                }
                else if (j < nx-1 && i < ny-2) {
                    // Upper boundary is aligned.
                    data[k] = (0.375*(data[k-step] + data[k+step])
                               + 0.125*(data[k+2*vstep-step]
                                        + data[k+2*vstep+step]));
                    levels[k] = (levels[k-step] + levels[k+step])/2;
                }
                else if (j < nx-1 && i >= 2) {
                    // Lower boundary can be unaligned.
                    guint bdist = yres-1 - i*step;
                    guint a = 4*bdist + 3*step, b = step, d = 8*(bdist + step);
                    data[k] = (a*(data[k-step] + data[k+step])
                               + b*(data[k-2*vstep-step]
                                    + data[k-2*vstep+step]))/d;
                    levels[k] = (levels[k-step] + levels[k+step])/2;
                }
                else if (i >= 2 && i < ny-2) {
                    // Right boundary can be unaligned.
                    guint bdist = xres-1 - j*step;
                    guint a = 6*step - 4*bdist, b = 2*bdist + step, d = 8*step;
                    data[k] = (a*data[k-step]
                               + b*(data[k-2*vstep-step]
                                    + data[k+2*vstep-step]))/d;
                    levels[k] = levels[k-step];
                }
                else if (i < ny-2) {
                    // Upper boundary is aligned, right boundary can be
                    // unaligned.
                    guint bdist = xres-1 - j*step;
                    guint a = 3*step - 2*bdist, b = 2*bdist + step, d = 4*step;
                    data[k] = (a*data[k-step] + b*data[k-step+2*vstep])/d;
                    levels[k] = levels[k-step];
                }
                else if (i >= 2) {
                    // Lower and right boundaries can be both unaligned.
                    guint xbdist = xres-1 - j*step;
                    guint ybdist = yres-1 - i*step;
                    guint a = 3*step + 4*ybdist - 2*xbdist, b = 2*xbdist + step;
                    data[k] = (a*data[k-step] + b*data[k-2*vstep])/(a + b);
                    levels[k] = levels[k-step];
                }
                else {
                    g_assert_not_reached();
                }
            }
        }
        else {
            // Interpolated point vertically in between two other points.
            for (j = 0; j < nx; j += 2) {
                guint k = (i*xres + j)*step;
                if (levels[k] != NONE)
                    continue;

                if (j >= 2 && j < nx-2 && i < ny-1) {
                    data[k] = (0.375*(data[k-vstep] + data[k+vstep])
                               + 0.0625*(data[k-vstep-2*step]
                                         + data[k-vstep+2*step]
                                         + data[k+vstep-2*step]
                                         + data[k+vstep+2*step]));
                    levels[k] = (levels[k-vstep] + levels[k+vstep])/2;
                }
                else if (j < nx-2 && i < ny-1) {
                    // Left boundary is aligned.
                    data[k] = (0.375*(data[k-vstep] + data[k+vstep])
                               + 0.125*(data[k-vstep+2*step]
                                        + data[k+vstep+2*step]));
                    levels[k] = (levels[k-vstep] + levels[k+vstep])/2;
                }
                else if (j >= 2 && i < ny-1) {
                    // Right boundary can be unaligned.
                    guint bdist = xres-1 - j*step;
                    guint a = 4*bdist + 3*step, b = step, d = 8*(bdist + step);
                    data[k] = (a*(data[k-vstep] + data[k+vstep])
                               + b*(data[k-vstep-2*step]
                                    + data[k+vstep-2*step]))/d;
                    levels[k] = (levels[k-vstep] + levels[k+vstep])/2;
                }
                else if (j >= 2 && j < nx-2) {
                    // Lower boundary can be unaligned.
                    guint bdist = yres-1 - i*step;
                    guint a = 6*step - 4*bdist, b = 2*bdist + step, d = 8*step;
                    data[k] = (a*data[k-vstep]
                               + b*(data[k-vstep-2*step]
                                    + data[k-vstep+2*step]))/d;
                    levels[k] = levels[k-vstep];
                }
                else if (j < nx-2) {
                    // Left boundary is aligned, lower boundary can be
                    // unaligned.
                    guint bdist = yres-1 - i*step;
                    guint a = 3*step - 2*bdist, b = 2*bdist + step, d = 4*step;
                    data[k] = (a*data[k-vstep] + b*data[k+2*step-vstep])/d;
                    levels[k] = levels[k-vstep];
                }
                else if (j >= 2) {
                    // Lower and right boundaries can be both unaligned.
                    guint xbdist = xres-1 - j*step;
                    guint ybdist = yres-1 - i*step;
                    guint a = 3*step + 4*xbdist - 2*ybdist, b = 2*ybdist + step;
                    data[k] = (a*data[k-vstep] + b*data[k-2*step])/(a + b);
                    levels[k] = levels[k-vstep];
                }
                else {
                    g_assert_not_reached();
                }
            }
        }
    }

    // Four-point interpolation
    for (i = 1; i < ny; i += 2) {
        for (j = 1; j < nx; j += 2) {
            guint k = (i*xres + j)*step;
            if (levels[k] != NONE)
                continue;

            if (i < ny-1 && j < nx-1) {
                data[k] = 0.25*(data[k-vstep] + data[k+vstep]
                                + data[k-step] + data[k+step]);
                levels[k] = (levels[k-vstep] + levels[k+vstep]
                             + levels[k-step] + levels[k+step])/4;
            }
            else if (i < ny-1) {
                // Right boundary can be unaligned.
                guint bdist = xres-1 - j*step;
                guint a = 2*bdist + step, b = 2*step, d = 4*(bdist + step);
                data[k] = (a*(data[k-vstep] + data[k+vstep])
                           + b*data[k-step])/d;
                levels[k] = (levels[k-vstep] + levels[k+vstep])/2;
            }
            else if (j < nx-1) {
                // Lower boundary can be unaligned.
                guint bdist = yres-1 - i*step;
                guint a = 2*bdist + step, b = 2*step, d = 4*(bdist + step);
                data[k] = (a*(data[k-step] + data[k+step])
                           + b*data[k-vstep])/d;
                levels[k] = (levels[k-step] + levels[k+step])/2;
            }
            else {
                // Right and lower boundary can be unaligned both.
                guint xbdist = xres-1 - j*step;
                guint ybdist = yres-1 - i*step;
                guint a = 2*ybdist + step, b = 2*xbdist + step;
                data[k] = (a*data[k-step] + b*data[k-vstep])/(a + b);
                levels[k] = (levels[k-step] + levels[k-vstep])/2;
            }
        }
    }
}

static void
reconstruct(guint *levels, gdouble *data, guint xres, guint yres, guint level)
{
    guint step = 1 << ((level - 1)/2);

    while (step) {
        interpolate(levels, data, xres, yres, step);
        step /= 2;
    }
}

static void
init_data_simple(gdouble *data, guint *levels, guint xres, guint yres)
{
    guint i, j, kk, level = 1;
    gboolean finished = FALSE;

    for (kk = 0; kk < xres*yres; kk++)
        levels[kk] = !!levels[kk];

    while (!finished) {
        finished = TRUE;
        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                guint k = i*xres + j, n = 0;
                gdouble s = 0;

                if (levels[k] != level)
                    continue;

                if (i && levels[k-xres] < level) {
                    s += data[k-xres];
                    n++;
                }
                if (j && levels[k-1] < level) {
                    s += data[k-1];
                    n++;
                }
                if (j+1 < xres && levels[k+1] < level) {
                    s += data[k+1];
                    n++;
                }
                if (i+1 < yres && levels[k+xres] < level) {
                    s += data[k+xres];
                    n++;
                }

                if (n) {
                    data[k] = s/n;
                }
                else {
                    levels[k] = level+1;
                    finished = FALSE;
                }
            }
        }
        level++;
    }
}

static void
laplace_sparse(LaplaceIterators *iterators,
               guint *revindex,
               gdouble *data, guint *levels, guint xres, guint yres,
               guint nconjgrad, guint nsimple)
{
    // Revindex is filled later, use is as a temporary xres*yres-sized buffer.
    guint maxlevel = build_levels(levels, revindex, xres, yres);
    gboolean finished = FALSE;
    guint iter;

    if (maxlevel < 3) {
        // If the grain is nowhere thick just init the interior using boundary
        // conditions and continue with dense iteration.  Note for single-pixel
        // grains init_data_simple() already produces the solution.
        init_data_simple(data, levels, xres, yres);
        return;
    }

    build_sparse_iterators(iterators, revindex, levels, data, xres, yres);
    calculate_f(iterators),
    gwy_assign(iterators->v, iterators->f, iterators->len);
    for (iter = 0; iter < nconjgrad; iter++) {
        if ((finished = iterate_conj_grad(iterators)))
            break;
    }
    if (!finished) {
        for (iter = 0; iter < nsimple; iter++) {
            calculate_f(iterators);
            iterate_simple(iterators);
        }
    }
    move_result_to_data(iterators, data);
    reconstruct(levels, data, xres, yres, maxlevel);
}

static void
laplace_dense(LaplaceIterators *iterators,
              guint *revindex,
              gdouble *data, guint *levels, guint xres, guint yres,
              guint nconjgrad, guint nsimple)
{
    gboolean finished = FALSE;
    guint iter;

    build_dense_iterators(iterators, revindex, levels, data, xres, yres);
    calculate_f(iterators);
    gwy_assign(iterators->v, iterators->f, iterators->len);
    for (iter = 0; iter < nconjgrad; iter++) {
        if ((finished = iterate_conj_grad(iterators)))
            break;
    }
    if (!finished) {
        for (iter = 0; iter < nsimple; iter++) {
            calculate_f(iterators);
            iterate_simple(iterators);
        }
    }
    move_result_to_data(iterators, data);
}

// Extract grain data from full-sized @grains and @data to workspace-sized
// @levels and @z.
static void
extract_grain(const gint *grains,
              const gdouble *data,
              guint xres,
              const GwyDataFieldPart *fpart,
              gint grain_id,
              guint *levels,
              gdouble *z)
{
    guint i, j;
    const gint *grow;
    guint *lrow;

    for (i = 0; i < fpart->height; i++) {
        gwy_assign(z + i*fpart->width,
                   data + (i + fpart->row)*xres + fpart->col,
                   fpart->width);
        grow = grains + (i + fpart->row)*xres + fpart->col;
        lrow = levels + i*fpart->width;
        for (j = fpart->width; j; j--, lrow++, grow++)
            *lrow = (*grow == grain_id);
    }
}

// Put interpolated grain data @z back to @data.
static void
insert_grain(const gint *grains,
             gdouble *data,
             guint xres,
             const GwyDataFieldPart *fpart,
             gint grain_id,
             const gdouble *z)
{
    guint i, j;

    for (i = 0; i < fpart->height; i++) {
        const gdouble *zrow = z + i*fpart->width;
        const gint *grow = grains + (i + fpart->row)*xres + fpart->col;
        gdouble *drow = data + (i + fpart->row)*xres + fpart->col;
        for (j = fpart->width; j; j--, zrow++, drow++, grow++) {
            if (*grow == grain_id)
                *drow = *zrow;
        }
    }
}

static void
enlarge_field_part(GwyDataFieldPart *fpart, guint xres, guint yres)
{
    if (fpart->col) {
        fpart->col--;
        fpart->width++;
    }
    if (fpart->col + fpart->width < xres)
        fpart->width++;

    if (fpart->row) {
        fpart->row--;
        fpart->height++;
    }
    if (fpart->row + fpart->height < yres)
        fpart->height++;
}

/*
 * Find the largest
 * - grain size in the terms of pixels: this is the number of iterators for
 *   dense iteration
 * - grain size in the terms of extended bounding box (i.e. bounding box
 *   including one more line of pixels to each side, if possible): this is
 *   the size of levels, revindex and data arrays.
 */
static void
find_largest_sizes(guint xres, guint yres,
                   const GwyDataFieldPart *bboxes,
                   const guint *sizes,
                   guint gfrom, guint gto,
                   guint *size,
                   guint *bboxsize)
{
    guint gno, bs;
    GwyDataFieldPart bbox;

    *size = *bboxsize = 0;
    for (gno = gfrom; gno <= gto; gno++) {
        if (sizes[gno] > *size)
            *size = sizes[gno];

        bbox = bboxes[gno];
        enlarge_field_part(&bbox, xres, yres);
        bs = bbox.width * bbox.height;
        if (bs > *bboxsize)
            *bboxsize = bs;
    }
}

/**
 * gwy_data_field_laplace_solve:
 * @field: A two-dimensional data field.
 * @mask: A two-dimensional data field containing mask defining the areas to
 *        interpolate.
 * @grain_id: The id number of the grain to replace with the solution of
 *            Laplace equation, from 1 to @ngrains (see
 *            gwy_mask_field_grain_numbers()).  Passing 0 means to replace the
 *            entire empty space outside grains while passing a negative value
 *            means to replace the entire masked area.
 * @qprec: Speed-accuracy tuning parameter.  Pass 1.0 for the default that is
 *         fast and sufficiently precise.
 *
 * Replaces masked areas by the solution of Laplace equation.
 *
 * The boundary conditions on mask boundaries are Dirichlet with values given
 * by pixels on the outer boundary of the masked area.  Boundary conditions at
 * field edges are Neumann conditions ∂z/∂n=0 where n denotes the normal to the
 * edge.  If entire area of @field is to be replaced the problem is
 * underspecified; @field will be filled with zeroes.
 *
 * For the default value of @qprec the the result should be good enough for any
 * image processing purposes with the typical local error of order 10⁻⁵ for
 * very large grains and possibly much smaller for small grains.  You can lower
 * @qprec down to about 0.3 or even 0.2 if speed is crucial and some precision
 * can be sacrificed.  Below that the result just starts becoming somewhat
 * worse for not much speed increase.  Conversely, you may wish to increase
 * @qprec up to 3 or even 5 if accuracy is important and you can afford the
 * increased computation time.
 *
 * Since: 2.47
 **/
void
gwy_data_field_laplace_solve(GwyDataField *field,
                             GwyDataField *mask,
                             gint grain_id,
                             gdouble qprec)
{
    GwyDataField *ourmask;
    guint xres, yres, maxsize, maxbboxsize;
    gint ngrains, gfrom, gto;
    GwyDataFieldPart *bboxes;
    LaplaceIterators *iterators;
    guint *levels, *revindex;
    gint *grains;
    guint *sizes;
    gdouble *z;

    g_return_if_fail(GWY_IS_DATA_FIELD(mask));
    g_return_if_fail(GWY_IS_DATA_FIELD(field));
    g_return_if_fail(mask->xres == field->xres);
    g_return_if_fail(mask->yres == field->yres);

    // To fill the entire empty space we need to divide it to grains too so
    // work with the inverted mask.
    if (grain_id == 0) {
        ourmask = gwy_data_field_duplicate(mask);
        gwy_data_field_grains_invert(ourmask);
        grain_id = -1;
    }
    else
        ourmask = g_object_ref((gpointer)mask);

    xres = field->xres;
    yres = field->yres;
    grains = g_new0(gint, xres*yres);
    ngrains = gwy_data_field_number_grains(ourmask, grains);
    if (grain_id > ngrains) {
        g_free(grains);
        g_return_if_fail(grain_id <= ngrains);
    }

    bboxes = (GwyDataFieldPart*)gwy_data_field_get_grain_bounding_boxes(ourmask,
                                                                        ngrains,
                                                                        grains,
                                                                        NULL);
    sizes = (guint*)gwy_data_field_get_grain_sizes(ourmask, ngrains, grains,
                                                   NULL);

    // The underspecified case.
    if (ngrains == 1 && sizes[1] == xres*yres) {
        gwy_data_field_clear(field);
        g_object_unref(ourmask);
        g_free(sizes);
        g_free(bboxes);
        g_free(grains);
        return;
    }

    gfrom = (grain_id < 0) ? 1 : grain_id;
    gto = (grain_id < 0) ? ngrains : grain_id;

    // Allocate everything at the maximum size to avoid reallocations.
    find_largest_sizes(xres, yres, bboxes, sizes, gfrom, gto,
                       &maxsize, &maxbboxsize);

    levels = g_new(guint, maxbboxsize);
    revindex = g_new(guint, maxbboxsize);
    z = g_new(gdouble, maxbboxsize);
    iterators = laplace_iterators_new(maxsize, 5);

    for (grain_id = gfrom; grain_id <= gto; grain_id++) {
        GwyDataFieldPart bbox = bboxes[grain_id];
        enlarge_field_part(&bbox, xres, yres);
        extract_grain(grains, field->data, xres, &bbox, grain_id, levels, z);
        laplace_sparse(iterators, revindex, z, levels,
                       bbox.width, bbox.height, 60*qprec, 20*qprec);
        if (sizes[grain_id] > 1)
            laplace_dense(iterators, revindex, z, levels,
                          bbox.width, bbox.height, 60*qprec, 30*qprec);
        insert_grain(grains, field->data, xres, &bbox, grain_id, z);
    }

    laplace_iterators_free(iterators);
    g_free(z);
    g_free(levels);
    g_free(revindex);
    g_free(sizes);
    g_free(bboxes);
    g_free(grains);

    g_object_unref(ourmask);
    gwy_data_field_invalidate(field);
}






/**
 * gwy_data_field_mask_outliers:
 * @data_field: A data field.
 * @mask_field: A data field to be filled with mask.
 * @thresh: Threshold value.
 *
 * Creates mask of data that are above or below @thresh*sigma from average
 * height.
 *
 * Sigma denotes root-mean square deviation of heights. This criterium
 * corresponds to the usual Gaussian distribution outliers detection if
 * @thresh is 3.
 **/
void
gwy_data_field_mask_outliers(GwyDataField *data_field,
                             GwyDataField *mask_field,
                             gdouble thresh)
{
    gwy_data_field_mask_outliers2(data_field, mask_field, thresh, thresh);
}

/**
 * gwy_data_field_mask_outliers2:
 * @data_field: A data field.
 * @mask_field: A data field to be filled with mask.
 * @thresh_low: Lower threshold value.
 * @thresh_high: Upper threshold value.
 *
 * Creates mask of data that are above or below multiples of rms from average
 * height.
 *
 * Data that are below @mean-@thresh_low*@sigma or above
 * @mean+@thresh_high*@sigma are marked as outliers, where @sigma denotes the
 * root-mean square deviation of heights.
 *
 * Since: 2.26
 **/
void
gwy_data_field_mask_outliers2(GwyDataField *data_field,
                              GwyDataField *mask_field,
                              gdouble thresh_low,
                              gdouble thresh_high)
{
     gdouble avg, val;
     gdouble criterium_low, criterium_high;
     gint i;

     avg = gwy_data_field_get_avg(data_field);
     criterium_low = -gwy_data_field_get_rms(data_field) * thresh_low;
     criterium_high = gwy_data_field_get_rms(data_field) * thresh_high;

     for (i = 0; i < (data_field->xres * data_field->yres); i++) {
         val = data_field->data[i] - avg;
         mask_field->data[i] = (val < criterium_low || val > criterium_high);
     }

     gwy_data_field_invalidate(mask_field);
}

/**
 * gwy_data_field_correct_average:
 * @data_field: A data field.
 * @mask_field: Mask of places to be corrected.
 *
 * Fills data under mask with the average value.
 *
 * This function simply puts average value of all the @data_field values (both
 * masked and unmasked) into points in @data_field lying under points where
 * @mask_field values are nonzero.
 *
 * In most cases you probably want to use
 * gwy_data_field_correct_average_unmasked() instead.
 **/
void
gwy_data_field_correct_average(GwyDataField *data_field,
                               GwyDataField *mask_field)
{
    gdouble avg;
    gint i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(!mask_field || (GWY_IS_DATA_FIELD(mask_field)
                                     && mask_field->xres == data_field->xres
                                     && mask_field->yres == data_field->yres));
    avg = gwy_data_field_get_avg(data_field);

    for (i = 0; i < (data_field->xres * data_field->yres); i++) {
        if (mask_field->data[i])
            data_field->data[i] = avg;
    }

    gwy_data_field_invalidate(mask_field);
}

/**
 * gwy_data_field_correct_average_unmasked:
 * @data_field: A data field.
 * @mask_field: Mask of places to be corrected.
 *
 * Fills data under mask with the average value of unmasked data.
 *
 * This function calculates the average value of all unmasked pixels in
 * @data_field and then fills all the masked pixels with this average value.
 * It is useful as the first rough step of correction of data under the mask.
 *
 * If all data are masked the field is filled with zeroes.
 *
 * Since: 2.44
 **/
void
gwy_data_field_correct_average_unmasked(GwyDataField *data_field,
                                        GwyDataField *mask_field)
{
    gdouble avg;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(!mask_field || (GWY_IS_DATA_FIELD(mask_field)
                               && mask_field->xres == data_field->xres
                               && mask_field->yres == data_field->yres));

    avg = gwy_data_field_area_get_avg_mask(data_field, mask_field,
                                           GWY_MASK_EXCLUDE,
                                           0, 0,
                                           data_field->xres, data_field->yres);
    if (gwy_isnan(avg) || gwy_isinf(avg)) {
        gwy_data_field_clear(data_field);
        return;
    }
    gwy_data_field_area_fill_mask(data_field, mask_field, GWY_MASK_INCLUDE,
                                  0, 0, data_field->xres, data_field->yres,
                                  avg);
}

/**
 * gwy_data_field_unrotate_find_corrections:
 * @derdist: Angular derivation distribution (normally obrained from
 *           gwy_data_field_slope_distribution()).
 * @correction: Corrections for particular symmetry types will be stored
 *              here (indexed by GwyPlaneSymmetry). @correction[0] contains
 *              the most probable correction.  All angles are in radians.
 *
 * Finds rotation corrections.
 *
 * Rotation correction is computed for for all symmetry types.
 * In addition an estimate is made about the prevalent one.
 *
 * Returns: The estimate type of prevalent symmetry.
 **/
GwyPlaneSymmetry
gwy_data_field_unrotate_find_corrections(GwyDataLine *derdist,
                                         gdouble *correction)
{
    static const guint symm[] = { 2, 3, 4, 6 };
    GwyPlaneSymmetry guess, t;
    gint nder;
    gsize j, m;
    gdouble avg, max, total, phi;
    const gdouble *der;
    gdouble sint[G_N_ELEMENTS(symm)], cost[G_N_ELEMENTS(symm)];

    nder = gwy_data_line_get_res(derdist);
    der = gwy_data_line_get_data_const(derdist);
    avg = gwy_data_line_get_avg(derdist);
    gwy_data_line_add(derdist, -avg);

    guess = GWY_SYMMETRY_AUTO;
    max = -G_MAXDOUBLE;
    for (j = 0; j < G_N_ELEMENTS(symm); j++) {
        m = symm[j];
        compute_fourier_coeffs(nder, der, m, sint+j, cost+j);
        phi = atan2(-sint[j], cost[j]);
        total = sqrt(sint[j]*sint[j] + cost[j]*cost[j]);

        gwy_debug("sc%d = (%f, %f), total%d = (%f, %f)",
                  m, sint[j], cost[j], m, total, 180.0/G_PI*phi);

        phi /= 2*G_PI*m;
        phi = unrotate_refine_correction(derdist, m, phi);
        t = sizeof("Die, die GCC warning!");
        /*
         *             range from             smallest possible
         *  symmetry   compute_correction()   range                ratio
         *    m        -1/2m .. 1/2m
         *
         *    2        -1/4  .. 1/4           -1/8  .. 1/8         1/2
         *    3        -1/6  .. 1/6           -1/12 .. 1/12        1/2
         *    4        -1/8  .. 1/8           -1/8  .. 1/8 (*)     1
         *    6        -1/12 .. 1/12          -1/12 .. 1/12        1
         *
         *  (*) not counting rhombic
         */
        switch (m) {
            case 2:
            t = GWY_SYMMETRY_PARALLEL;
            /* align with any x or y */
            if (phi >= 0.25/m)
                phi -= 0.5/m;
            else if (phi <= -0.25/m)
                phi += 0.5/m;
            correction[t] = phi;
            total /= 1.25;
            break;

            case 3:
            t = GWY_SYMMETRY_TRIANGULAR;
            /* align with any x or y */
            if (phi >= 0.125/m)
                phi -= 0.25/m;
            else if (phi <= -0.125/m)
                phi += 0.25/m;
            correction[t] = phi;
            break;

            case 4:
            t = GWY_SYMMETRY_SQUARE;
            correction[t] = phi;
            /* decide square/rhombic */
            phi += 0.5/m;
            if (phi > 0.5/m)
                phi -= 1.0/m;
            t = GWY_SYMMETRY_RHOMBIC;
            correction[t] = phi;
            if (fabs(phi) > fabs(correction[GWY_SYMMETRY_SQUARE]))
                t = GWY_SYMMETRY_SQUARE;
            total /= 1.4;
            break;

            case 6:
            t = GWY_SYMMETRY_HEXAGONAL;
            correction[t] = phi;
            break;

            default:
            g_assert_not_reached();
            break;
        }

        if (total > max) {
            max = total;
            guess = t;
        }
    }
    gwy_data_line_add(derdist, avg);
    g_assert(guess != GWY_SYMMETRY_AUTO);
    gwy_debug("SELECTED: %d", guess);
    correction[GWY_SYMMETRY_AUTO] = correction[guess];

    for (j = 0; j < GWY_SYMMETRY_LAST; j++) {
        gwy_debug("FINAL %d: (%f, %f)", j, correction[j], 360*correction[j]);
        correction[j] *= 2.0*G_PI;
    }

    return guess;
}

static void
compute_fourier_coeffs(gint nder, const gdouble *der,
                       guint symmetry,
                       gdouble *st, gdouble *ct)
{
    guint i;
    gdouble q, sint, cost;

    q = 2*G_PI/nder*symmetry;
    sint = cost = 0.0;
    for (i = 0; i < nder; i++) {
        sint += sin(q*(i + 0.5))*der[i];
        cost += cos(q*(i + 0.5))*der[i];
    }

    *st = sint;
    *ct = cost;
}

/**
 * unrotate_refine_correction:
 * @derdist: Angular derivation distribution (as in Slope dist. graph).
 * @m: Symmetry.
 * @phi: Initial correction guess (in the range 0..1!).
 *
 * Compute correction assuming symmetry @m and initial guess @phi.
 *
 * Returns: The correction (again in the range 0..1!).
 **/
static gdouble
unrotate_refine_correction(GwyDataLine *derdist,
                           guint m, gdouble phi)
{
    gdouble sum, wsum;
    const gdouble *der;
    guint i, j, nder;

    nder = gwy_data_line_get_res(derdist);
    der = gwy_data_line_get_data_const(derdist);

    phi -= floor(phi) + 1.0;
    sum = wsum = 0.0;
    for (j = 0; j < m; j++) {
        gdouble low = (j + 5.0/6.0)/m - phi;
        gdouble high = (j + 7.0/6.0)/m - phi;
        gdouble s, w;
        guint ilow, ihigh;

        ilow = (guint)floor(low*nder);
        ihigh = (guint)floor(high*nder);
        gwy_debug("[%u] peak %u low = %f, high = %f, %u, %u",
                  m, j, low, high, ilow, ihigh);
        s = w = 0.0;
        for (i = ilow; i <= ihigh; i++) {
            s += (i + 0.5)*der[i % nder];
            w += der[i % nder];
        }

        s /= nder*w;
        gwy_debug("[%u] peak %u center: %f", m, j, 360*s);
        sum += (s - (gdouble)j/m)*w*w;
        wsum += w*w;
    }
    phi = sum/wsum;
    gwy_debug("[%u] FITTED phi = %f (%f)", m, phi, 360*phi);
    phi = fmod(phi + 1.0, 1.0/m);
    if (phi > 0.5/m)
        phi -= 1.0/m;
    gwy_debug("[%u] MINIMIZED phi = %f (%f)", m, phi, 360*phi);

    return phi;
}

/**
 * gwy_data_field_sample_distorted:
 * @source: Source data field.
 * @dest: Destination data field.
 * @coords: Array of @source coordinates with the same number of items as
 *          @dest, ordered as data field data.
 *          See gwy_data_field_distort() for coordinate convention discussion.
 * @interp: Interpolation type to use.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with @GWY_EXTERIOR_FIXED_VALUE.
 *
 * Resamples a data field in an arbitrarily distorted manner.
 *
 * Each item in @coords corresponds to one pixel in @dest and gives the
 * coordinates in @source defining the value to set in this pixel.
 *
 * Since: 2.45
 **/
void
gwy_data_field_sample_distorted(GwyDataField *source,
                                GwyDataField *dest,
                                const GwyXY *coords,
                                GwyInterpolationType interp,
                                GwyExteriorType exterior,
                                gdouble fill_value)
{
    gwy_data_field_distort_internal(source, dest, interp, exterior, fill_value,
                                    coords, NULL, NULL);
}

/**
 * gwy_data_field_distort:
 * @source: Source data field.
 * @dest: Destination data field.
 * @invtrans: Inverse transform function, that is the transformation from
 *            new coordinates to old coordinates.   It gets
 *            (@j+0.5, @i+0.5), where @i and @j are the new row and column
 *            indices, passed as the input coordinates.  The output coordinates
 *            should follow the same convention.  Unless a special exterior
 *            handling is required, the transform function does not need to
 *            concern itself with coordinates being outside of the data.
 * @user_data: Pointer passed as @user_data to @invtrans.
 * @interp: Interpolation type to use.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with @GWY_EXTERIOR_FIXED_VALUE.
 *
 * Distorts a data field in the horizontal plane.
 *
 * Note the transform function @invtrans is the inverse transform, in other
 * words it calculates the old coordinates from the new coordinates (the
 * transform would not be uniquely defined the other way round).
 *
 * Since: 2.5
 **/
void
gwy_data_field_distort(GwyDataField *source,
                       GwyDataField *dest,
                       GwyCoordTransform2DFunc invtrans,
                       gpointer user_data,
                       GwyInterpolationType interp,
                       GwyExteriorType exterior,
                       gdouble fill_value)
{
    gwy_data_field_distort_internal(source, dest, interp, exterior, fill_value,
                                    NULL, invtrans, user_data);
}

static void
gwy_data_field_distort_internal(GwyDataField *source,
                                GwyDataField *dest,
                                GwyInterpolationType interp,
                                GwyExteriorType exterior,
                                gdouble fill_value,
                                const GwyXY *coords,
                                GwyCoordTransform2DFunc invtrans,
                                gpointer user_data)
{
    GwyDataField *coeffield;
    gdouble *data, *coeff;
    const gdouble *cdata;
    gint xres, yres, newxres, newyres;
    gint newi, newj, oldi, oldj, i, j, ii, jj, suplen, sf, st;
    gdouble x, y, v;
    gboolean vset, warned = FALSE;

    g_return_if_fail(GWY_IS_DATA_FIELD(source));
    g_return_if_fail(GWY_IS_DATA_FIELD(dest));
    g_return_if_fail(coords || invtrans);
    g_return_if_fail(!coords || !invtrans);

    suplen = gwy_interpolation_get_support_size(interp);
    g_return_if_fail(suplen > 0);
    coeff = g_newa(gdouble, suplen*suplen);
    sf = -((suplen - 1)/2);
    st = suplen/2;

    xres = gwy_data_field_get_xres(source);
    yres = gwy_data_field_get_yres(source);
    newxres = gwy_data_field_get_xres(dest);
    newyres = gwy_data_field_get_yres(dest);

    if (gwy_interpolation_has_interpolating_basis(interp))
        coeffield = g_object_ref(source);
    else {
        coeffield = gwy_data_field_duplicate(source);
        gwy_interpolation_resolve_coeffs_2d(xres, yres, xres,
                                            gwy_data_field_get_data(coeffield),
                                            interp);
    }

    data = gwy_data_field_get_data(dest);
    cdata = gwy_data_field_get_data_const(coeffield);

    for (newi = 0; newi < newyres; newi++) {
        for (newj = 0; newj < newxres; newj++) {
            if (invtrans)
                invtrans(newj + 0.5, newi + 0.5, &x, &y, user_data);
            else {
                x = coords->x;
                y = coords->y;
                coords++;
            }

            vset = FALSE;
            x -= 0.5;
            y -= 0.5;
            if (y > yres || x > xres || y < 0.0 || x < 0.0) {
                switch (exterior) {
                    case GWY_EXTERIOR_BORDER_EXTEND:
                    x = CLAMP(x, 0, xres);
                    y = CLAMP(y, 0, yres);
                    break;

                    case GWY_EXTERIOR_MIRROR_EXTEND:
                    /* Mirror extension is what the interpolation code does
                     * by default */
                    break;

                    case GWY_EXTERIOR_PERIODIC:
                    x = (x > 0) ? fmod(x, xres) : fmod(x, xres) + xres;
                    y = (y > 0) ? fmod(y, yres) : fmod(y, yres) + yres;
                    break;

                    case GWY_EXTERIOR_FIXED_VALUE:
                    v = fill_value;
                    vset = TRUE;
                    break;

                    case GWY_EXTERIOR_UNDEFINED:
                    continue;
                    break;

                    default:
                    if (!warned) {
                        g_warning("Unsupported exterior type, "
                                  "assuming undefined");
                        warned = TRUE;
                    }
                    continue;
                    break;
                }
            }
            if (!vset) {
                oldi = (gint)floor(y);
                y -= oldi;
                oldj = (gint)floor(x);
                x -= oldj;
                for (i = sf; i <= st; i++) {
                    ii = (oldi + i + 2*st*yres) % (2*yres);
                    if (G_UNLIKELY(ii >= yres))
                        ii = 2*yres-1 - ii;
                    for (j = sf; j <= st; j++) {
                        jj = (oldj + j + 2*st*xres) % (2*xres);
                        if (G_UNLIKELY(jj >= xres))
                            jj = 2*xres-1 - jj;
                        coeff[(i - sf)*suplen + j - sf] = cdata[ii*xres + jj];
                    }
                }
                v = gwy_interpolation_interpolate_2d(x, y, suplen, coeff,
                                                     interp);
            }
            data[newj + newxres*newi] = v;
        }
    }

    g_object_unref(coeffield);
}

/**
 * gwy_data_field_affine:
 * @source: Source data field.
 * @dest: Destination data field.
 * @invtrans: Inverse transform, that is the transformation from
 *            new pixel coordinates to old pixel coordinates, represented as
 *            (@j+0.5, @i+0.5), where @i and @j are the row and column
 *            indices.  It is represented as a six-element array [@axx, @axy,
 *            @ayx, @ayy, @bx, @by] where @axy is the coefficient from @x to
 *            @y.
 * @interp: Interpolation type to use.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with @GWY_EXTERIOR_FIXED_VALUE.
 *
 * Performs an affine transformation of a data field in the horizontal plane.
 *
 * Note the transform @invtrans is the inverse transform, in other
 * words it calculates the old coordinates from the new coordinates.  This
 * way even degenerate (non-invertible) transforms can be meaningfully used.
 * Also note that the (column, row) coordinate system is left-handed.
 *
 * Since: 2.34
 **/
void
gwy_data_field_affine(GwyDataField *source,
                      GwyDataField *dest,
                      const gdouble *invtrans,
                      GwyInterpolationType interp,
                      GwyExteriorType exterior,
                      gdouble fill_value)
{
    GwyDataField *coeffield;
    gdouble *data, *coeff;
    const gdouble *cdata;
    gint xres, yres, newxres, newyres;
    gint newi, newj, oldi, oldj, i, j, ii, jj, suplen, sf, st;
    gdouble x, y, v;
    gdouble axx, axy, ayx, ayy, bx, by;
    gboolean vset, warned = FALSE;

    g_return_if_fail(GWY_IS_DATA_FIELD(source));
    g_return_if_fail(GWY_IS_DATA_FIELD(dest));
    g_return_if_fail(invtrans);

    axx = invtrans[0];
    axy = invtrans[1];
    ayx = invtrans[2];
    ayy = invtrans[3];
    bx = invtrans[4];
    by = invtrans[5];

    suplen = gwy_interpolation_get_support_size(interp);
    g_return_if_fail(suplen > 0);
    coeff = g_newa(gdouble, suplen*suplen);
    sf = -((suplen - 1)/2);
    st = suplen/2;

    xres = gwy_data_field_get_xres(source);
    yres = gwy_data_field_get_yres(source);
    newxres = gwy_data_field_get_xres(dest);
    newyres = gwy_data_field_get_yres(dest);

    if (gwy_interpolation_has_interpolating_basis(interp))
        coeffield = g_object_ref(source);
    else {
        coeffield = gwy_data_field_duplicate(source);
        gwy_interpolation_resolve_coeffs_2d(xres, yres, xres,
                                            gwy_data_field_get_data(coeffield),
                                            interp);
    }

    data = gwy_data_field_get_data(dest);
    cdata = gwy_data_field_get_data_const(coeffield);

    /* Incorporate the half-pixel shifts to bx and by */
    bx += 0.5*(axx + axy - 1.0);
    by += 0.5*(ayx + ayy - 1.0);
    for (newi = 0; newi < newyres; newi++) {
        for (newj = 0; newj < newxres; newj++) {
            x = axx*newj + axy*newi + bx;
            y = ayx*newj + ayy*newi + by;
            vset = FALSE;
            if (y > yres || x > xres || y < 0.0 || x < 0.0) {
                switch (exterior) {
                    case GWY_EXTERIOR_BORDER_EXTEND:
                    x = CLAMP(x, 0, xres);
                    y = CLAMP(y, 0, yres);
                    break;

                    case GWY_EXTERIOR_MIRROR_EXTEND:
                    /* Mirror extension is what the interpolation code does
                     * by default */
                    break;

                    case GWY_EXTERIOR_PERIODIC:
                    x = (x > 0) ? fmod(x, xres) : fmod(x, xres) + xres;
                    y = (y > 0) ? fmod(y, yres) : fmod(y, yres) + yres;
                    break;

                    case GWY_EXTERIOR_FIXED_VALUE:
                    v = fill_value;
                    vset = TRUE;
                    break;

                    case GWY_EXTERIOR_UNDEFINED:
                    continue;
                    break;

                    default:
                    if (!warned) {
                        g_warning("Unsupported exterior type, "
                                  "assuming undefined");
                        warned = TRUE;
                    }
                    continue;
                    break;
                }
            }
            if (!vset) {
                oldi = (gint)floor(y);
                y -= oldi;
                oldj = (gint)floor(x);
                x -= oldj;
                for (i = sf; i <= st; i++) {
                    ii = (oldi + i + 2*st*yres) % (2*yres);
                    if (G_UNLIKELY(ii >= yres))
                        ii = 2*yres-1 - ii;
                    for (j = sf; j <= st; j++) {
                        jj = (oldj + j + 2*st*xres) % (2*xres);
                        if (G_UNLIKELY(jj >= xres))
                            jj = 2*xres-1 - jj;
                        coeff[(i - sf)*suplen + j - sf] = cdata[ii*xres + jj];
                    }
                }
                v = gwy_interpolation_interpolate_2d(x, y, suplen, coeff,
                                                     interp);
            }
            data[newj + newxres*newi] = v;
        }
    }

    g_object_unref(coeffield);
}

/**
 * gwy_data_line_correct_laplace:
 * @data_line: A data line.
 * @mask_field: Mask of places to be corrected.
 *
 * Fills missing values in a data line using Laplace data correction.
 *
 * Both data lines must have the same number of values.
 *
 * For one-dimensional data the missing data interpolation is explicit.
 * Interior missing segments are filled with linear dependence between the edge
 * points.  Missing segments with one end open are filled with the edge value.
 *
 * Returns: %TRUE if the line contained any data at all.  If there are no data
 *          the %FALSE is returned and @data_line is filled with zeros.
 *
 * Since: 2.45
 **/
gboolean
gwy_data_line_correct_laplace(GwyDataLine *data_line,
                              GwyDataLine *mask_line)
{
    gint start = -1, i, res;
    const gdouble *m;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), FALSE);
    g_return_val_if_fail(GWY_IS_DATA_LINE(mask_line), FALSE);
    g_return_val_if_fail(data_line->res == mask_line->res, FALSE);

    res = data_line->res;
    m = mask_line->data;
    for (i = 0; i < res; i++) {
        if (start == -1) {
            if (m[i] > 0.0)
                start = i;
        }
        else {
            if (!(m[i] > 0.0)) {
                interpolate_segment(data_line, start, i-1);
                start = -1;
            }
        }
    }

    if (start == 0) {
        gwy_data_line_clear(data_line);
        return FALSE;
    }

    if (start != -1)
        interpolate_segment(data_line, start, res-1);

    return TRUE;
}

static void
interpolate_segment(GwyDataLine *data_line, gint from, gint to)
{
    gint i, res = data_line->res;
    gdouble *d = data_line->data;
    gdouble zl, zr;

    g_assert(to < res-1 || from > 0);

    if (from == 0) {
        zr = d[to+1];
        for (i = from; i <= to; i++)
            d[i] = zr;
    }
    else if (to == res-1) {
        zl = d[from-1];
        for (i = from; i <= to; i++)
            d[i] = zl;
    }
    else {
        zl = d[from-1]/(to - from + 2);
        zr = d[to+1]/(to - from + 2);
        for (i = from; i <= to; i++)
            d[i] = zr*(i+1) + zl*(to+1 - from - i);
    }
}

/**
 * gwy_data_field_mark_scars:
 * @data_field: A data field to find scars in.
 * @result: A data field to store the result to (it is resized to match
 *              @data_field).
 * @threshold_high: Miminum relative step for scar marking, must be positive.
 * @threshold_low: Definite relative step for scar marking, must be at least
 *                 equal to @threshold_high.
 * @min_scar_len: Minimum length of a scar, shorter ones are discarded
 *                (must be at least one).
 * @max_scar_width: Maximum width of a scar, must be at least one.
 * @negative: %TRUE to detect negative scars, %FALSE to positive.
 *
 * Find and marks scars in a data field.
 *
 * Scars are linear horizontal defects, consisting of shifted values.
 * Zero or negative values in @result siginify normal data, positive
 * values siginify samples that are part of a scar.
 *
 * Since: 2.46
 **/
void
gwy_data_field_mark_scars(GwyDataField *data_field,
                          GwyDataField *result,
                          gdouble threshold_high,
                          gdouble threshold_low,
                          gdouble min_scar_len,
                          gdouble max_scar_width,
                          gboolean negative)
{
    gint xres, yres, i, j, k;
    gdouble rms;
    const gdouble *d;
    gdouble *m;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(result));
    xres = data_field->xres;
    yres = data_field->yres;
    d = data_field->data;
    gwy_data_field_resample(result, xres, yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_clear(result);
    m = gwy_data_field_get_data(result);

    min_scar_len = MAX(min_scar_len, 1);
    max_scar_width = MIN(max_scar_width, yres - 2);
    threshold_high = MAX(threshold_high, threshold_low);
    if (min_scar_len > xres || max_scar_width < 1 || threshold_low <= 0.0)
        return;

    /* compute `vertical rms' */
    rms = 0.0;
    for (i = 0; i < yres-1; i++) {
        const gdouble *row = d + i*xres;

        for (j = 0; j < xres; j++) {
            gdouble z = row[j] - row[j + xres];

            rms += z*z;
        }
    }
    rms = sqrt(rms/(xres*yres));
    if (rms == 0.0)
        return;

    /* initial scar search */
    for (i = 0; i < yres - (max_scar_width + 1); i++) {
        for (j = 0; j < xres; j++) {
            gdouble top, bottom;
            const gdouble *row = d + i*xres + j;

            if (negative) {
                top = row[0];
                bottom = row[xres];
                for (k = 1; k <= max_scar_width; k++) {
                    top = MIN(row[0], row[xres*(k + 1)]);
                    bottom = MAX(bottom, row[xres*k]);
                    if (top - bottom >= threshold_low*rms)
                        break;
                }
                if (k <= max_scar_width) {
                    gdouble *mrow = m + i*xres + j;

                    while (k) {
                        mrow[k*xres] = (top - row[k*xres])/rms;
                        k--;
                    }
                }
            }
            else {
                bottom = row[0];
                top = row[xres];
                for (k = 1; k <= max_scar_width; k++) {
                    bottom = MAX(row[0], row[xres*(k + 1)]);
                    top = MIN(top, row[xres*k]);
                    if (top - bottom >= threshold_low*rms)
                        break;
                }
                if (k <= max_scar_width) {
                    gdouble *mrow = m + i*xres + j;

                    while (k) {
                        mrow[k*xres] = (row[k*xres] - bottom)/rms;
                        k--;
                    }
                }
            }
        }
    }
    /* expand high threshold to neighbouring low threshold */
    for (i = 0; i < yres; i++) {
        gdouble *mrow = m + i*xres;

        for (j = 1; j < xres; j++) {
            if (mrow[j] >= threshold_low && mrow[j-1] >= threshold_high)
                mrow[j] = threshold_high;
        }
        for (j = xres-1; j > 0; j--) {
            if (mrow[j-1] >= threshold_low && mrow[j] >= threshold_high)
                mrow[j-1] = threshold_high;
        }
    }
    /* kill too short segments, clamping result along the way */
    for (i = 0; i < yres; i++) {
        gdouble *mrow = m + i*xres;

        k = 0;
        for (j = 0; j < xres; j++) {
            if (mrow[j] >= threshold_high) {
                mrow[j] = 1.0;
                k++;
                continue;
            }
            if (k && k < min_scar_len) {
                while (k) {
                    mrow[j-k] = 0.0;
                    k--;
                }
            }
            mrow[j] = 0.0;
            k = 0;
        }
        if (k && k < min_scar_len) {
            while (k) {
                mrow[j-k] = 0.0;
                k--;
            }
        }
    }
}

/************************** Documentation ****************************/

/**
 * SECTION:correct
 * @title: correct
 * @short_description: Data correction
 **/

/**
 * GwyCoordTransform2DFunc:
 * @x: Old x coordinate.
 * @y: Old y coordinate.
 * @px: Location to store new x coordinate.
 * @py: Location to store new y coordinate.
 * @user_data: User data passed to the caller function.
 *
 * The type of two-dimensional coordinate transform function.
 *
 * Since: 2.5
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
