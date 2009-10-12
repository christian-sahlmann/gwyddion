/*
 *  @(#) $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
#include <string.h>
#include <math.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/delaunay.h>

enum {
    UNDEF = G_MAXUINT,
    /* The work space and work queue can be resized, just choose some
     * reasonable initial more-or-less upper bound. */
    WORKSPACE = 32,
    QUEUE = 128,
    /* Tunables */
    NEIGHBOURS = 8,
    WSPACE_CACHE_SIZE = 128,
    WSPACE_CACHE_BLOCK = 8,
};

#define CELL_SIDE 6.0

#define get_point(points, point_size, i) \
    (const Point*)((const gchar*)(points) + (i)*(point_size))

#define get_point_xyz(points, point_size, i) \
    (const PointXYZ*)((const gchar*)(points) + (i)*(point_size))

/* Representation of input data, they must be typecastable to this but can
 * contain any more data in the structs. */
typedef GwyDelaunayPointXY Point;
typedef GwyDelaunayPointXYZ PointXYZ;

/* One neighbour of the currently processed point, contains various
 * pre-calculated quantities. */
typedef struct {
    /* Radial coordinates with respect to the origin, i.e. the point we are
     * finding the neighbours of. */
    gdouble r;
    gdouble phi;
    /* Intersection times (i.e. t-coordinates) with the previous and next line.
     * G_MAXDOUBLE and -G_MAXDOUBLE is used for infinities. */
    gdouble tprev;
    gdouble tnext;
    /* Point id */
    guint id;
} WorkSpacePoint;

/* Define our own resizable arrays to inline all the operations. */

/* The neighbourhood of one point, with pre-calculated quantities.  Too
 * large to keep for all points, but we use a cache.  */
typedef struct {
    Point origin;
    WorkSpacePoint *data;
    guint len;
    guint size;
} WorkSpace;

/* The size and associativity is controlled by compile-time constants.
 * Point ids are a separate from the workspaces since we want to scan the
 * points quickly without loading the workspaces into CPU cache lines. */
typedef struct {
    WorkSpace *wspace[WSPACE_CACHE_SIZE];
    guint id[WSPACE_CACHE_SIZE];
} WorkSpaceCache;

/* Queue of points we need to process after adding a new point to the
 * triangulation.  Points before pos have been already processed (and success
 * then contains whether the were actually updated), points after pos are yet
 * to be processed. */
typedef struct {
    guint *id;
    gboolean *success;
    guint pos;
    guint len;
    guint size;
} WorkQueue;

/* List of points.  We transform the input points to this as we reorder them
 * anyway. */
typedef struct {
    guint npoints;          /* Number of points */
    Point *points;          /* The points */
    guint *orig_index;      /* Map from our ids to original point numbers. */
} PointList;

/* Information about blocks of neighbours in Triangulation. */
typedef struct {
    guint pos;
    guint len;
    guint size;
} NeighbourBlock;

/* Triangulation state. */
typedef struct {
    guint npoints;   /* Point currently in the triangulation */
    NeighbourBlock *blocks;   /* Blocks of neighbours in neighbours[] */
    guint *neighbours;   /* Indices of neighbours points */
    guint nsize;     /* Allocated size of neighbours[] */
    guint nlen;      /* Used space in neighbours[] */
    /* Used only in interpolation */
    guint blen;      /* Number of boundary points */
    guint *boundary; /* The boundary points */
    guint *bindex;   /* Point position in boundary[] or UNDEF */
} Triangulation;

typedef struct {
    Point centre;      /* Point in the side centre */
    Point outernormal; /* Outer normal of the side */
    gdouble norm;      /* Scalar product of the vector from cente to the
                          opposite side with outernormal */
} TriangleSide;

typedef struct {
    TriangleSide sa, sb, sc;    /* Triangle sides, opposite to a, b, c */
    const PointXYZ *a, *b, *c;  /* Vertices */
    gdouble da, db, dc;         /* Signed distances of a point from the side,
                                   depends on the point being considered. */
    guint ia, ib, ic;           /* Ids of the triangle vertices */
} Triangle;

/*
static void dump_neighbours(const Triangulation *triangulation);
static void dump_triangulation(const Triangulation *triangulation);
static void dump_points(const Triangulation *triangulation,
                        guint npoints, gconstpointer points, gsize point_size);
static guint test_reflexivity(const Triangulation *triangulation);
*/

/* Estimate how big block we want to allocate if we have @n neighbours.
 * Returns a multiple of NEIGHBOURS. */
static inline guint
block_size(guint n)
{
    guint size = MAX(n + 2, NEIGHBOURS);
    return (size + NEIGHBOURS-1)/NEIGHBOURS*NEIGHBOURS;
}

static inline void
block_clear(guint *block,
            guint len)
{
    memset(block, 0xff, len*sizeof(guint));
}

static inline guint
coords_to_grid_index(guint xres,
                     guint yres,
                     gdouble step,
                     gdouble x,
                     gdouble y)
{
    guint ix, iy;

    ix = (gint)floor(x/step);
    if (G_UNLIKELY(ix == xres))
        ix--;

    iy = (gint)floor(y/step);
    if (G_UNLIKELY(iy == yres))
        iy--;

    /* Go zig-zag through the cells */
    if (iy % 2)
        ix = xres-1 - ix;

    return iy*xres + ix;
}

static inline void
index_accumulate(guint *index_array,
                 guint n)
{
    guint i;

    for (i = 1; i <= n; i++)
        index_array[i] += index_array[i-1];
}

static inline void
index_rewind(guint *index_array,
             guint n)
{
    guint i;

    for (i = n; i; i--)
        index_array[i] = index_array[i-1];
    index_array[0] = 0;
}

/* Try to increase locality of the point list by sorting it to grid cells
 * and then taking the points cell-by-cell.  Also reduces the workind set size
 * by constructing a list of plain Points instead of whatever might the
 * caller's representation be. */
static void
build_compact_point_list(PointList *pointlist,
                         guint npoints,
                         gconstpointer points,
                         gsize point_size)
{
    const Point *pt;
    gdouble xmin, xmax, ymin, ymax, xreal, yreal, step, xr, yr;
    guint i, xres, yres, ncells, ig, pos;
    guint *cell_index;

    pointlist->npoints = npoints;
    pointlist->points = g_new(Point, npoints);
    pointlist->orig_index = g_new(guint, npoints);

    pt = get_point(points, point_size, 0);
    xmin = xmax = pt->x;
    ymin = ymax = pt->y;
    for (i = 1; i < npoints; i++) {
        pt = get_point(points, point_size, i);

        if (pt->x < xmin)
            xmin = pt->x;
        else if (pt->x > xmax)
            xmax = pt->x;

        if (pt->y < ymin)
            ymin = pt->y;
        else if (pt->y > ymax)
            ymax = pt->y;
    }

    xreal = xmax - xmin;
    yreal = ymax - ymin;
    xr = xreal/sqrt(npoints)*CELL_SIDE;
    yr = yreal/sqrt(npoints)*CELL_SIDE;

    if (xr <= yr) {
        xres = (guint)ceil(xreal/xr);
        step = xreal/xres;
        yres = (guint)ceil(yreal/step);
    }
    else {
        yres = (guint)ceil(yreal/yr);
        step = yreal/yres;
        xres = (guint)ceil(xreal/step);
    }

    ncells = xres*yres;
    cell_index = g_new0(guint, ncells + 1);

    for (i = 0; i < npoints; i++) {
        pt = get_point(points, point_size, i);
        ig = coords_to_grid_index(xres, yres, step, pt->x - xmin, pt->y - ymin);
        cell_index[ig]++;
    }

    index_accumulate(cell_index, xres*yres);
    index_rewind(cell_index, xres*yres);

    for (i = 0; i < npoints; i++) {
        pt = get_point(points, point_size, i);
        ig = coords_to_grid_index(xres, yres, step, pt->x - xmin, pt->y - ymin);
        pos = cell_index[ig];
        pointlist->orig_index[pos] = i;
        pointlist->points[pos] = *pt;
        cell_index[ig]++;
    }

    g_free(cell_index);
}

static inline void
get_workspace_point(WorkSpacePoint *wpt,
                    const PointList *pointlist,
                    guint id,
                    const Point *origin)
{
    Point pt;

    wpt->id = id;
    pt = pointlist->points[id];
    pt.x -= origin->x;
    pt.y -= origin->y;
    wpt->r = hypot(pt.x, pt.y);
    g_assert(wpt->r != 0);
    wpt->phi = atan2(pt.y, pt.x);
    /* Lines start open-ended */
    wpt->tprev = -G_MAXDOUBLE;
    wpt->tnext = G_MAXDOUBLE;
}

static inline void
work_space_ensure_size(WorkSpace *wspace,
                      guint len)
{
    if (G_UNLIKELY(len > wspace->size)) {
        wspace->size *= 2;
        wspace->data = g_renew(WorkSpacePoint, wspace->data, wspace->size);
    }
}

static inline void
work_space_insert(WorkSpace *wspace,
                  guint i,
                  const WorkSpacePoint *wpt)
{
    guint j;

    work_space_ensure_size(wspace, wspace->len + 1);
    for (j = wspace->len; j > i; j--)
        wspace->data[j] = wspace->data[j-1];

    wspace->data[j] = *wpt;
    wspace->len++;
}

static inline void
work_space_remove(WorkSpace *wspace,
                  guint i,
                  guint *iref)
{
    guint j;

    g_assert(i < wspace->len);

    for (j = i+1; j < wspace->len; j++)
        wspace->data[j-1] = wspace->data[j];

    wspace->len--;

    if (!iref || *iref <= i)
        return;

    (*iref)--;
}

/* Does not work if p->phi == q->phi, but this is handled specially by caller */
static inline void
intersection_times(const WorkSpacePoint *p, gdouble *tp,
                   const WorkSpacePoint *q, gdouble *tq)
{
    gdouble dphi = q->phi - p->phi;
    gdouble cdphi, sdphi;

    /* Intersection on the wrong side -> make the lines open-ended. */
    if (dphi > G_PI || (dphi < 0 && dphi > -G_PI)) {
        *tp = G_MAXDOUBLE;
        *tq = -G_MAXDOUBLE;
        return;
    }

    cdphi = cos(dphi);
    sdphi = sin(dphi);
    *tp = (q->r - p->r*cdphi)/sdphi;
    *tq = (q->r*cdphi - p->r)/sdphi;
}

/* Returns %TRUE if @pt lies on the right side of line from @a to @b. */
static inline gboolean
point_on_right_side(const Point *a, const Point *b, const Point *pt)
{
    Point c, v;

    c.x = pt->x - 0.5*(a->x + b->x);
    c.y = pt->y - 0.5*(a->y + b->y);
    v.x = b->x - a->x;
    v.y = b->y - a->y;

    return c.x*v.y - c.y*v.x >= 0.0;
}

/* Returns TRUE if point @x should be inserted between @p and @q, possibly
 * shadowing one of them (not checked here).  If it is so, the intersection
 * times are updated.  If FALSE is returned, the intersection times of @x
 * are undefined. */
static inline gboolean
to_be_inserted(WorkSpacePoint *p,
               WorkSpacePoint *x,
               WorkSpacePoint *q)
{
    gdouble tp, tq;

    /* Special-case adding the second point.  This always succeeds unless the
     * new point is exactly in the same direction as @p == @q. */
    if (p == q) {
        if (G_UNLIKELY(x->phi == p->phi)) {
            /* Silently replace @p with @x, in any case, only one point
             * remains again. */
            if (x->r < p->r)
                *p = *x;
            return FALSE;
        }
        /* Otherwise determine which comes first by examining the angle
         * between them.  The other ends are open so keep the intersection
         * times at infinities.  */
        /* FIXME: What if the points are exactly *opposite* each other? */
        if (fmod(x->phi - p->phi + 2*G_PI, 2*G_PI) > G_PI)
            intersection_times(x, &x->tnext, q, &q->tprev);
        else
            intersection_times(p, &p->tnext, x, &x->tprev);

        return TRUE;
    }

    /* Point in exactly the same direction as @p or @q */
    if (G_UNLIKELY(x->phi == p->phi)) {
        if (x->r < p->r) {
            intersection_times(x, &x->tnext, q, &q->tprev);
            p->tnext = x->tprev = -G_MAXDOUBLE;
            return TRUE;
        }
        return FALSE;
    }

    if (G_UNLIKELY(x->phi == q->phi)) {
        if (x->r < q->r) {
            intersection_times(p, &p->tnext, x, &x->tprev);
            q->tprev = x->tnext = G_MAXDOUBLE;
            return TRUE;
        }
        return FALSE;
    }

    /* The common case: all three are different and in different directions. */
    intersection_times(p, &tp, x, &x->tprev);
    intersection_times(x, &x->tnext, q, &tq);
    if (tp <= p->tnext && tq >= q->tprev) {
        p->tnext = tp;
        q->tprev = tq;
        return TRUE;
    }
    return FALSE;
}

/* Returns TRUE if the point @x should be removed as redundant.  This is
 * determined from intersection times that are assumed to have been updated
 * before.  If it is so, intersection times of @p and @q are updated. */
static inline gboolean
to_be_removed(WorkSpacePoint *p,
              WorkSpacePoint *x,
              WorkSpacePoint *q)
{
    if (x->tnext >= x->tprev)
        return FALSE;

    intersection_times(p, &p->tnext, q, &q->tprev);
    return TRUE;
}

static gboolean
try_to_add_point(const PointList *pointlist,
                 guint id,
                 WorkSpace *wspace)
{
    WorkSpacePoint *wdata;
    WorkSpacePoint wpt;
    guint i, inext, iprev, ifar, not_removed;

    wdata = wspace->data;
    get_workspace_point(&wpt, pointlist, id, &wspace->origin);

    /* Not points yet, just assign it */
    if (wspace->len == 0) {
        wdata[wspace->len++] = wpt;
        return TRUE;
    }

    /* Find the would-be-neighbours */
    for (inext = 0; inext < wspace->len; inext++) {
        if (wpt.phi <= wdata[inext].phi)
            break;
    }
    i = inext;

    /* Is the point in their shadow? */
    inext = inext % wspace->len;
    iprev = (inext + wspace->len - 1) % wspace->len;
    if (!to_be_inserted(wdata + iprev, &wpt, wdata + inext))
        return FALSE;

    /* No, insert it */
    work_space_insert(wspace, i, &wpt);
    inext = (i + 1) % wspace->len;
    iprev = (i + wspace->len - 1) % wspace->len;
    wdata = wspace->data;    /* Might have changed! */

    /* The new point can make some previously inserted points redundant */
    not_removed = 0;
    while (wspace->len > 2) {
        /* Try to remove the following point */
        ifar = (inext + 1) % wspace->len;
        if (to_be_removed(wdata + i, wdata + inext, wdata + ifar)) {
            work_space_remove(wspace, inext, &i);
            inext = (i + 1) % wspace->len;
            iprev = (i + wspace->len - 1) % wspace->len;
            not_removed = 0;
        }
        else if (++not_removed == 2)
            break;

        /* Try to remove the preceeding point */
        ifar = (iprev + wspace->len - 1) % wspace->len;
        if (to_be_removed(wdata + ifar, wdata + iprev, wdata + i)) {
            work_space_remove(wspace, iprev, &i);
            inext = (i + 1) % wspace->len;
            iprev = (i + wspace->len - 1) % wspace->len;
            not_removed = 0;
        }
        else if (++not_removed == 2)
            break;
    }

    return TRUE;
}

static void
work_space_init(WorkSpace *wspace)
{
    gwy_clear(wspace, 1);
    wspace->size = WORKSPACE;
    wspace->data = g_new(WorkSpacePoint, wspace->size);
}

static void
work_space_destroy(WorkSpace *wspace)
{
    g_free(wspace->data);
}

/* Put back the wspace for the neighbourhood of a given point to
 * triangulation->neighbours[]. */
static void
work_space_construct(const Triangulation *triangulation,
                     const PointList *pointlist,
                     guint id,
                     WorkSpace *wspace)
{
    const NeighbourBlock *nb;
    WorkSpacePoint *wdata;
    guint j;
    const guint *neighbours;

    nb = triangulation->blocks + id;
    neighbours = triangulation->neighbours + nb->pos;

    work_space_ensure_size(wspace, nb->len);
    wspace->origin = pointlist->points[id];
    wspace->len = 0;
    wdata = wspace->data;
    for (j = 0; j < nb->len; j++)
        get_workspace_point(wdata + wspace->len++, pointlist,
                            neighbours[j], &wspace->origin);
    if (wspace->len < 2)
        return;

    for (j = 1; j < wspace->len; j++)
        intersection_times(wdata + j-1, &wdata[j-1].tnext,
                           wdata + j, &wdata[j].tprev);
    j = wspace->len - 1;
    intersection_times(wdata + j, &wdata[j].tnext,
                       wdata, &wdata[0].tprev);
}

static WorkSpaceCache*
work_space_cache_new(void)
{
    WorkSpaceCache *wscache;

    wscache = g_new0(WorkSpaceCache, 1);
    block_clear(wscache->id, WSPACE_CACHE_SIZE);

    return wscache;
}

static void
work_space_cache_free(WorkSpaceCache *wscache)
{
    guint i;

    for (i = 0; i < WSPACE_CACHE_SIZE; i++) {
        if (wscache->id[i] != UNDEF) {
            work_space_destroy(wscache->wspace[i]);
            g_free(wscache->wspace[i]);
        }
    }
    g_free(wscache);
}

/* The caller *promises* to fill the returned value with workspace for @id on
 * failure! */
static WorkSpace*
work_space_cache_get(WorkSpaceCache *wscache,
                     guint id)
{
    WorkSpace **wspaceblock, *wspace;
    guint *idblock;
    guint i, blockid;

    blockid = id % (WSPACE_CACHE_SIZE/WSPACE_CACHE_BLOCK);
    idblock = wscache->id + WSPACE_CACHE_BLOCK*blockid;
    wspaceblock = wscache->wspace + WSPACE_CACHE_BLOCK*blockid;

    /* Try to locate the workspace in the cache. */
    for (i = 0; i < WSPACE_CACHE_BLOCK; i++) {
        if (idblock[i] == id)
            return wspaceblock[i];
    }

    /* If we don't find it, get rid of the last in the block, reset it and
     * return that. */
    wspace = wspaceblock[WSPACE_CACHE_BLOCK - 1];
    for (i = WSPACE_CACHE_BLOCK-1; i; i--) {
        wspaceblock[i] = wspaceblock[i-1];
        idblock[i] = idblock[i-1];
    }

    /* At the begining, we might also need to create a new work space. */
    if (G_UNLIKELY(!wspace)) {
        wspace = g_new(WorkSpace, 1);
        work_space_init(wspace);
    }
    else {
        /* This indicates the necessity to reconstruct it afresh. */
        wspace->len = 0;
    }
    wspaceblock[0] = wspace;
    idblock[0] = id;

    return wspace;
}

/* The slow path for wspace reintegration into triangulation->neighbours[].
 * Anything from reclaiming space in the next block to making neighbours[]
 * bigger can happen. */
static guint*
relocate_block(Triangulation *triangulation,
               NeighbourBlock *nb,
               guint len)
{
    guint j, remaining, newsize;
    guint *neighbours;

    /* Try to find space after the end of the current block.  We know
     * the blocks sizes are multiples of NEIGHBOURS, so only check if
     * the first item is UNDEF. */
    neighbours = triangulation->neighbours + nb->pos;
    newsize = block_size(len + 2);
    remaining = triangulation->nsize - nb->pos;
    for (j = nb->size;
         j < remaining && neighbours[j] == UNDEF && j < newsize;
         j++)
        ;

    if (j < len) {
        /* Must relocate the current block */
        if (triangulation->nlen + newsize > triangulation->nsize) {
            /* Must reallocate triangulation->neighbours */
            j = triangulation->nsize;
            triangulation->nsize = block_size(3*j/2);
            triangulation->neighbours = g_renew(guint,
                                                triangulation->neighbours,
                                                triangulation->nsize);
            neighbours = triangulation->neighbours + nb->pos;
            block_clear(triangulation->neighbours + j,
                        triangulation->nsize - j);
        }
        block_clear(neighbours, nb->len);
        nb->pos = triangulation->nlen;
        nb->size = newsize;
        nb->len = 0;
        triangulation->nlen += newsize;
        neighbours = triangulation->neighbours + nb->pos;
    }
    else {
        nb->size = j;
        triangulation->nlen = MAX(triangulation->nlen, nb->pos + j);
    }

    return neighbours;
}

static void
reintegrate_workspace(Triangulation *triangulation,
                      guint id,
                      WorkSpace *wspace)
{
    NeighbourBlock *nb;
    guint *neighbours;
    guint j;

    nb = triangulation->blocks + id;
    neighbours = triangulation->neighbours + nb->pos;
    if (wspace->len > nb->size)
        neighbours = relocate_block(triangulation, nb, wspace->len);
    nb->len = wspace->len;
    /* Copy the new id list to the block */
    for (j = 0; j < wspace->len; j++)
        neighbours[j] = wspace->data[j].id;
    /* Fill the remainder with UNDEFs, until we hit the first UNDEF. */
    while (j < nb->size && neighbours[j] != UNDEF)
        neighbours[j++] = UNDEF;
}

static inline guint
find_neighbour(const guint *neighbours,
               guint len,
               guint id)
{
    guint i;

    for (i = 0; i < len; i++) {
        if (neighbours[i] == id)
            return i;
    }
    return UNDEF;
}

static inline guint
next_neighbour(const guint *neighbours,
               guint len,
               guint i)
{
    i++;
    return neighbours[i == len ? 0 : i];
}

static inline guint
prev_neighbour(const guint *neighbours,
               guint len,
               guint i)
{
    return neighbours[i == 0 ? len-1 : i-1];
}

/* This assumes a counter-clockwise triangle */
static inline void
make_triangle_side(TriangleSide *side,
                   const PointXYZ *from,
                   const PointXYZ *to,
                   const PointXYZ *opposite)
{
    side->centre.x = 0.5*(to->x + from->x);
    side->centre.y = 0.5*(to->y + from->y);
    side->outernormal.x = to->y - from->y;
    side->outernormal.y = from->x - to->x;
    side->norm = (opposite->x - side->centre.x)*side->outernormal.x
                 + (opposite->y - side->centre.y)*side->outernormal.y;
}

/* This assumes a counter-clockwise triangle */
static void
make_triangle(Triangle *triangle,
              gconstpointer points, gsize point_size)
{
    /* XXX: In the triangulation algoritm, the points are in fact only XY,
     * but the Z members are never accessed so the typecast is all right. */
    triangle->a = get_point_xyz(points, point_size, triangle->ia);
    triangle->b = get_point_xyz(points, point_size, triangle->ib);
    triangle->c = get_point_xyz(points, point_size, triangle->ic);

    make_triangle_side(&triangle->sa, triangle->b, triangle->c, triangle->a);
    make_triangle_side(&triangle->sb, triangle->c, triangle->a, triangle->b);
    make_triangle_side(&triangle->sc, triangle->a, triangle->b, triangle->c);
}

/* Positive for inside, negative for outside.  Normalized to the distance of
 * the opposite triangle point -- directly usable for interpolation. */
static inline gdouble
side_point_distance(const TriangleSide *side,
                    const Point *pt)

{
    return ((pt->x - side->centre.x)*side->outernormal.x
            + (pt->y - side->centre.y)*side->outernormal.y)/side->norm;
}

static gboolean
triangle_contains_point(Triangle *triangle,
                        const Point *pt)
{
    /* Do not terminate permaturely, the caller will typically examine da, db,
     * and dc to determine what to do next if the point is not inside. */
    triangle->da = side_point_distance(&triangle->sa, pt);
    triangle->db = side_point_distance(&triangle->sb, pt);
    triangle->dc = side_point_distance(&triangle->sc, pt);

    return triangle->da >= 0 && triangle->db >= 0 && triangle->dc >= 0;
}

/* If TRUE is returned, then a neighbour on the other side was found and the
 * triangle has become clockwise.  If TRUE is returnes, then opposite is
 * unchanged and the triangle is kept counter-clockwise. */
static gboolean
find_the_other_neighbour(const Triangulation *triangulation,
                         gconstpointer points, gsize point_size,
                         guint from,
                         guint to,
                         guint *opposite)
{
    NeighbourBlock *nb;
    const Point *a, *b, *c;
    guint to_prev, from_next;
    const guint *neighbours;

    nb = triangulation->blocks + from;
    neighbours = triangulation->neighbours + nb->pos;
    to_prev = prev_neighbour(neighbours, nb->len,
                             find_neighbour(neighbours, nb->len, to));

    nb = triangulation->blocks + to;
    neighbours = triangulation->neighbours + nb->pos;
    from_next = next_neighbour(neighbours, nb->len,
                               find_neighbour(neighbours, nb->len, from));

    /* Now there are some silly few-point special cases.  If @opposite is in
     * the centre of a triangle formed by @from, @to and the newly found point,
     * then we have an apparent match but it is not the point we are looking
     * for.  Check that the points really lies on the opposite side. */
    if (from_next != to_prev || from_next == *opposite)
        return FALSE;

    a = get_point(points, point_size, from);
    b = get_point(points, point_size, to);
    c = get_point(points, point_size, to_prev);
    /*
    g_assert(!point_on_right_side(a, b,
                                  get_point(points, point_size, *opposite)));
                                  */

    if (!point_on_right_side(a, b, c))
        return FALSE;

    *opposite = to_prev;
    return TRUE;
}

static inline gboolean
move_triangle_a(const Triangulation *triangulation,
                gconstpointer points, gsize point_size,
                Triangle *triangle)
{
    if (find_the_other_neighbour(triangulation, points, point_size,
                                 triangle->ib, triangle->ic, &triangle->ia)) {
        GWY_SWAP(guint, triangle->ib, triangle->ic);
        GWY_SWAP(const PointXYZ*, triangle->b, triangle->c);
        return TRUE;
    }
    return FALSE;
}

static inline gboolean
move_triangle_b(const Triangulation *triangulation,
                gconstpointer points, gsize point_size,
                Triangle *triangle)
{
    if (find_the_other_neighbour(triangulation, points, point_size,
                                 triangle->ic, triangle->ia, &triangle->ib)) {
        GWY_SWAP(guint, triangle->ic, triangle->ia);
        GWY_SWAP(const PointXYZ*, triangle->c, triangle->a);
        return TRUE;
    }
    return FALSE;
}

static inline gboolean
move_triangle_c(const Triangulation *triangulation,
                gconstpointer points, gsize point_size,
                Triangle *triangle)
{
    if (find_the_other_neighbour(triangulation, points, point_size,
                                 triangle->ia, triangle->ib, &triangle->ic)) {
        GWY_SWAP(guint, triangle->ia, triangle->ib);
        GWY_SWAP(const PointXYZ*, triangle->a, triangle->b);
        return TRUE;
    }
    return FALSE;
}

/* Initializes @triangle to any valid triangle containing point @hint. */
static void
make_valid_triangle(const Triangulation *triangulation,
                    gconstpointer points, gsize point_size,
                    Triangle *triangle,
                    guint hint)
{
    NeighbourBlock *nb;
    const Point *a = get_point(points, point_size, hint);
    const Point *b, *c;
    gdouble phib, phic;
    const guint *neighbours;
    guint i;

    triangle->ia = hint;
    nb = triangulation->blocks + hint;
    neighbours = triangulation->neighbours + nb->pos;

    for (i = 0; i < nb->len; i++) {
        triangle->ib = neighbours[i];
        triangle->ic = next_neighbour(neighbours, nb->len, i);

        b = get_point(points, point_size, triangle->ib);
        phib = atan2(b->y - a->y, b->x - a->x);
        c = get_point(points, point_size, triangle->ic);
        phic = atan2(c->y - a->y, c->x - a->x);

        if (fmod(phic - phib + 2*G_PI, 2*G_PI) < G_PI) {
            make_triangle(triangle, points, point_size);
            return;
        }
    }

    g_assert_not_reached();
}

/* Calculate the intersection of dividing lines of the corner angles at a and b
 * in boundary point sequence p, a, b, n. */
static void
find_side_section(const Point *p,
                  const Point *a,
                  const Point *b,
                  const Point *n,
                  Point *origin)
{
    Point pa, ab, bn, mA, mB;
    gdouble norm, det, rhsa, rhsb;

    pa.x = a->x - p->x;
    pa.y = a->y - p->y;
    norm = hypot(pa.x, pa.y);
    pa.x /= norm;
    pa.y /= norm;

    ab.x = b->x - a->x;
    ab.y = b->y - a->y;
    norm = hypot(ab.x, ab.y);
    ab.x /= norm;
    ab.y /= norm;

    bn.x = n->x - b->x;
    bn.y = n->y - b->y;
    norm = hypot(bn.x, bn.y);
    bn.x /= norm;
    bn.y /= norm;

    mA.x = pa.x + ab.x;
    mA.y = pa.y + ab.y;
    mB.x = ab.x + bn.x;
    mB.y = ab.y + bn.y;
    det = mA.x*mB.y - mA.y*mB.x;

    rhsa = a->x*mA.x + a->y*mA.y;
    rhsb = b->x*mB.x + b->y*mB.y;

    origin->x = (rhsa*mB.y - rhsb*mA.y)/det;
    origin->y = (rhsb*mA.x - rhsa*mB.x)/det;
}

/* A number between [-1, 1] means in the side, smaller means back, larger means
 * forward. */
static gdouble
side_intersection_distance(const PointXYZ *a, const PointXYZ *b,
                           const Point *pt)
{
    Point c, v;

    c.x = pt->x - 0.5*(a->x + b->x);
    c.y = pt->y - 0.5*(a->y + b->y);
    v.x = b->x - a->x;
    v.y = b->y - a->y;

    return 2.0*(c.x*v.x + c.y*v.y)/(v.x*v.x + v.y*v.y);
}

/* Find the side nearest to @pt.  The search must start from a boundary side,
 * if the side is not boundary, FALSE is returned. */
static gboolean
find_nearest_side(const Triangulation *triangulation,
                  gconstpointer points, gsize point_size,
                  guint *pia, guint *pib,
                  const Point *pt)
{
    guint ip, ia, ib, in, blen, iter;
    const guint *bindex, *boundary;
    const Point *p, *a, *b, *n;
    Point origin;
    gdouble phia, phib, phi;
    gboolean forw, back;

    ia = *pia;
    ib = *pib;
    bindex = triangulation->bindex;
    if (bindex[ia] == UNDEF || bindex[ib] == UNDEF)
        return FALSE;

    boundary = triangulation->boundary;
    blen = triangulation->blen;
    ip = boundary[(bindex[ia] + blen-1) % blen];
    in = boundary[(bindex[ib] + 1) % blen];

    p = get_point(points, point_size, ip);
    a = get_point(points, point_size, ia);
    b = get_point(points, point_size, ib);
    n = get_point(points, point_size, in);

    iter = 0;
    while (TRUE) {
        find_side_section(p, a, b, n, &origin);

        phia = atan2(a->y - origin.y, a->x - origin.x);
        phib = atan2(b->y - origin.y, b->x - origin.x);
        phi = atan2(pt->y - origin.y, pt->x - origin.x);

        forw = (fmod(phi - phia + 2*G_PI, 2*G_PI) < G_PI);
        back = (fmod(phib - phi + 2*G_PI, 2*G_PI) < G_PI);

        if (forw && back && point_on_right_side(a, b, pt)) {
            *pia = ia;
            *pib = ib;
            return TRUE;
        }

        if (forw) {
            ip = ia;
            ia = ib;
            ib = in;
            in = boundary[(bindex[ib] + 1) % blen];
            p = a;
            a = b;
            b = n;
            n = get_point(points, point_size, in);
        }
        else {
            in = ib;
            ib = ia;
            ia = ip;
            ip = boundary[(bindex[ia] + blen-1) % blen];
            n = b;
            b = a;
            a = p;
            p = get_point(points, point_size, ip);
        }

        iter++;
        g_assert(iter < blen);
    }

    return FALSE;
}

/* Ensures @triangle contains point @pt.  A relatively quick test if it already
 * contains the point.  If the right triangle is nearby, it is also found
 * reasonably fast. */
static gboolean
ensure_triangle(const Triangulation *triangulation,
                gconstpointer points, gsize point_size,
                Triangle *triangle,
                const Point *pt)
{
    gboolean moved;
    guint iter;

    iter = 0;
    while (!triangle_contains_point(triangle, pt)) {
        if (triangle->da <= triangle->db) {
            if (triangle->da <= triangle->dc)
                moved = move_triangle_a(triangulation, points, point_size,
                                        triangle);
            else
                moved = move_triangle_c(triangulation, points, point_size,
                                        triangle);
        }
        else {
            if (triangle->db <= triangle->dc)
                moved = move_triangle_b(triangulation, points, point_size,
                                        triangle);
            else
                moved = move_triangle_c(triangulation, points, point_size,
                                        triangle);
        }

        if (!moved)
            return FALSE;

        make_triangle(triangle, points, point_size);
        g_assert(iter++ < triangulation->npoints);
    }

    return TRUE;
}

static void
compactify(const Triangulation *triangulation,
           const PointList *pointlist,
           GwyDelaunayTriangulation *gwytri)
{
    const NeighbourBlock *nb;
    const guint *neighbours, *orig_index;
    guint *dest;
    guint i, j, iorig;

    orig_index = pointlist->orig_index;

    /* Construct the back-mapped point_index */
    g_assert(triangulation->npoints == pointlist->npoints);
    gwytri->npoints = triangulation->npoints;
    gwytri->index = g_new0(guint, gwytri->npoints+1);
    for (i = 0; i < triangulation->npoints; i++) {
        iorig = orig_index[i];
        gwytri->index[iorig] = triangulation->blocks[i].len;
    }

    index_accumulate(gwytri->index, gwytri->npoints);
    index_rewind(gwytri->index, gwytri->npoints);

    /* Fill neighbours with back-mapped neighbour indices */
    gwytri->size = gwytri->index[gwytri->npoints];
    gwytri->neighbours = g_new(guint, gwytri->size);
    for (i = 0; i < triangulation->npoints; i++) {
        iorig = orig_index[i];
        nb = triangulation->blocks + i;
        neighbours = triangulation->neighbours + nb->pos;
        dest = gwytri->neighbours + gwytri->index[iorig];
        for (j = 0; j < nb->len; j++)
            dest[j] = orig_index[neighbours[j]];
    }
}

/* Create a triangle from the first three points.
 * FIXME: If they form a straight line, this will fail. */
static void
create_first_triangle(const PointList *pointlist,
                      Triangulation *triangulation,
                      WorkSpace *wspace)
{
    NeighbourBlock *nb;
    guint i, j;

    for (i = 0; i < MIN(pointlist->npoints, 3); i++) {
        wspace->len = 0;
        wspace->origin = pointlist->points[i];
        for (j = 0; j < MIN(pointlist->npoints, 3); j++) {
            if (j != i)
                try_to_add_point(pointlist, j, wspace);
        }
        nb = triangulation->blocks + i;
        nb->pos = i*NEIGHBOURS;
        nb->size = NEIGHBOURS;
        nb->len = 0;
        reintegrate_workspace(triangulation, i, wspace);
    }
    triangulation->npoints = i;
    triangulation->nlen = i*NEIGHBOURS;
}

static void
work_queue_init(WorkQueue *queue)
{
    queue->size = QUEUE;
    queue->len = 0;
    queue->id = g_new(guint, queue->size);
    queue->success = g_new(gboolean, queue->size);
}

static void
work_queue_destroy(WorkQueue *queue)
{
    g_free(queue->id);
    g_free(queue->success);
}

static void
work_queue_add(WorkQueue *queue,
               guint id)
{
    if (G_UNLIKELY(queue->len == queue->size)) {
        queue->size *= 2;
        queue->id = g_renew(guint, queue->id, queue->size);
        queue->success = g_renew(gboolean, queue->success,
                                     queue->size);
    }
    queue->id[queue->len] = id;
    queue->success[queue->len] = FALSE;
    queue->len++;
}

static Triangulation*
triangulation_new_from_pointlist(const PointList *pointlist)
{
    Triangulation *triangulation;

    triangulation = g_new0(Triangulation, 1);
    /* A reasonable estimate */
    triangulation->nsize = 2*NEIGHBOURS*pointlist->npoints;
    triangulation->blocks = g_new(NeighbourBlock, pointlist->npoints);
    triangulation->neighbours = g_new(guint, triangulation->nsize);
    block_clear(triangulation->neighbours, triangulation->nsize);

    return triangulation;
}

static Triangulation*
triangulation_new_from_finalized(GwyDelaunayTriangulation *gwytri)
{
    Triangulation *triangulation;
    NeighbourBlock *nb;
    guint i;

    triangulation = g_new0(Triangulation, 1);
    triangulation->npoints = gwytri->npoints;
    triangulation->neighbours = gwytri->neighbours;
    triangulation->nsize = triangulation->nlen = gwytri->size;
    triangulation->blocks = g_new(NeighbourBlock, triangulation->npoints);
    for (i = 0; i < triangulation->npoints; i++) {
        nb = triangulation->blocks + i;
        nb->pos = gwytri->index[i];
        nb->size = nb->len = gwytri->index[i+1] - gwytri->index[i];
    }

    return triangulation;
}

static void
triangulation_free(Triangulation *triangulation)
{
    g_free(triangulation->blocks);
    g_free(triangulation->neighbours);
    g_free(triangulation->boundary);
    g_free(triangulation->bindex);
    g_free(triangulation);
}

static Triangulation*
triangulate(const PointList *pointlist)
{
    Triangulation *triangulation;
    WorkSpaceCache *wscache;
    WorkSpace *wspace;
    Triangle triangle;
    WorkQueue queue;
    guint i, j;

    triangulation = triangulation_new_from_pointlist(pointlist);
    wscache = work_space_cache_new();
    work_queue_init(&queue);

    /* create_first_triangle() ends up with point 2 in the work space. */
    wspace = work_space_cache_get(wscache, 2);
    create_first_triangle(pointlist, triangulation, wspace);

    for (i = 3; i < pointlist->npoints; i++) {
        NeighbourBlock *nb;
        gboolean in;

        /* Make a valid triangle.  The neighbour updates might turn a
         * previously valid triangle to an invalid one. */
        make_valid_triangle(triangulation, pointlist->points, sizeof(Point),
                            &triangle, i-1);
        /* Find the enclosing or the nearest (for outside points) triangle */
        in = ensure_triangle(triangulation, pointlist->points, sizeof(Point),
                             &triangle, pointlist->points + i);
        /* Put the three points to the queue as their neighbourhood needs
         * updating. */
        queue.pos = queue.len = 0;
        work_queue_add(&queue, triangle.ia);
        work_queue_add(&queue, triangle.ib);
        work_queue_add(&queue, triangle.ic);

        while (queue.pos < queue.len) {
            const guint *neighbours;
            guint id = queue.id[queue.pos];
            guint ni, k;

            wspace = work_space_cache_get(wscache, id);
            if (!wspace->len)
                work_space_construct(triangulation, pointlist, id, wspace);

            if (try_to_add_point(pointlist, i, wspace)) {
                /* If we added point i among the neighbours of point id, any
                 * of the id's neighbours may also need updating. */
                nb = triangulation->blocks + id;
                neighbours = triangulation->neighbours + nb->pos;
                for (j = 0; j < nb->len; j++) {
                    ni = neighbours[j];
                    for (k = 0; k < queue.len; k++) {
                        if (k == queue.pos)
                            continue;
                        if (queue.id[k] == ni)
                            break;
                    }
                    if (k == queue.len)
                        work_queue_add(&queue, ni);
                }
                queue.success[queue.pos] = TRUE;

                reintegrate_workspace(triangulation, id, wspace);
            }
            /* Move id from todo part to processed part. */
            queue.pos++;
        }

        //g_print("%u\n", queue.len);

        /* Now take points where insertion succeeded and construct the
         * neighbourhood of i of them */

        /* Point i cannot be in the cache yet, but we will have a workspace to
         * fill with its neighbours. */
        wspace = work_space_cache_get(wscache, i);
        wspace->origin = pointlist->points[i];
        for (j = 0; j < queue.len; j++) {
            if (queue.success[j]) {
                in = try_to_add_point(pointlist, queue.id[j], wspace);
                /* Reflexivity */
                g_assert(in);
            }
        }

        /* Append an empty zero-length block, reintegrate_workspace() will fix
         * it to the required size. */
        nb = triangulation->blocks + i;
        nb->pos = triangulation->nlen;
        nb->size = 0;
        nb->len = 0;
        triangulation->npoints++;
        reintegrate_workspace(triangulation, i, wspace);
    }

    work_space_cache_free(wscache);
    work_queue_destroy(&queue);

    return triangulation;
}

/**
 * gwy_delaunay_triangulation_free:
 * @triangulation: Delaunay triangulation.
 *
 * Frees a triangulation created by gwy_delaunay_triangulate().
 *
 * Since: 2.18
 **/
void
gwy_delaunay_triangulation_free(GwyDelaunayTriangulation *triangulation)
{
    g_free(triangulation->index);
    g_free(triangulation->neighbours);
    g_free(triangulation);
}

/**
 * gwy_delaunay_triangulate:
 * @npoints: Number of points.
 * @points: Array of points.  They must be typecastable to @GwyDelaunayPointXY,
 *          however, they can be larger than that.  The actual struct size
 *          is indicated by @point_size.
 * @point_size: Size of point struct, in bytes.
 *
 * Finds Delaunay triangulation for a set of points in plane.
 *
 * The triangulation might not work in numerically unstable cases.  Also, no
 * points in the input set may coincide.
 *
 * Returns: A newly created triangulation, %NULL on failure.
 *
 * Since: 2.18
 **/
GwyDelaunayTriangulation*
gwy_delaunay_triangulate(guint npoints, gconstpointer points, gsize point_size)
{
    GwyDelaunayTriangulation *gwytri;
    Triangulation *triangulation;
    PointList pointlist;

    g_return_val_if_fail(point_size >= sizeof(Point), NULL);

    build_compact_point_list(&pointlist, npoints, points, point_size);
    triangulation = triangulate(&pointlist);
    gwytri = g_new0(GwyDelaunayTriangulation, 1);
    compactify(triangulation, &pointlist, gwytri);

    triangulation_free(triangulation);
    g_free(pointlist.orig_index);
    g_free(pointlist.points);

    return gwytri;
}

static void
find_boundary(Triangulation *triangulation,
              gconstpointer points, gsize point_size)
{
    const NeighbourBlock *nb;
    guint i, imin, bsize;
    gdouble xmin, x;
    const Point *pt, *pt2;
    const guint *neighbours;

    triangulation->bindex = g_new(guint, triangulation->npoints);
    for (i = 0; i < triangulation->npoints; i++)
        triangulation->bindex[i] = UNDEF;
    bsize = 6*ceil(sqrt(triangulation->npoints));
    triangulation->boundary = g_new(guint, bsize);
    triangulation->blen = 0;

    /* The leftmost point must lie on the boundary */
    pt = get_point(points, point_size, 0);
    xmin = pt->x;
    imin = 0;
    for (i = 1; i < triangulation->npoints; i++) {
        pt = get_point(points, point_size, i);
        if (pt->x < xmin) {
            imin = i;
            xmin = pt->x;
        }
    }
    triangulation->bindex[imin] = triangulation->blen;
    triangulation->boundary[triangulation->blen++] = imin;
    if (triangulation->npoints == 1)
        return;

    /* Its neighbour with the lowest direction angle is the first side,
     * FIXME: In principle, the points are already sorted this way so taking
     * neighbours[0] should suffice.  */
    nb = triangulation->blocks + imin;
    neighbours = triangulation->neighbours + nb->pos;
    pt = get_point(points, point_size, imin);
    pt2 = get_point(points, point_size, neighbours[0]);
    xmin = atan2(pt2->y - pt->y, pt2->x - pt->x);
    imin = 0;
    for (i = 1; i < nb->len; i++) {
        pt2 = get_point(points, point_size, neighbours[i]);
        x = atan2(pt2->y - pt->y, pt2->x - pt->x);
        if (x < xmin) {
            imin = i;
            xmin = x;
        }
    }
    imin = neighbours[imin];
    triangulation->bindex[imin] = triangulation->blen;
    triangulation->boundary[triangulation->blen++] = imin;
    if (triangulation->npoints == 2)
        return;

    /* The remaining points are always next to the previous boundary point */
    while (TRUE) {
        i = triangulation->boundary[triangulation->blen-2];
        nb = triangulation->blocks + imin;
        neighbours = triangulation->neighbours + nb->pos;
        i = next_neighbour(neighbours, nb->len,
                           find_neighbour(neighbours, nb->len, i));
        if (i == triangulation->boundary[0])
            return;
        if (triangulation->blen == bsize) {
            bsize *= 2;
            triangulation->boundary = g_renew(guint, triangulation->boundary,
                                              bsize);
        }
        imin = i;
        triangulation->bindex[imin] = triangulation->blen;
        triangulation->boundary[triangulation->blen++] = imin;
    }
}

static inline gdouble
interpolate_linear(const Triangle *triangle)
{
    gdouble wsum = triangle->da + triangle->db + triangle->dc;

    return (triangle->da*triangle->a->z + triangle->db*triangle->b->z
            + triangle->dc*triangle->c->z)/wsum;
}

static inline gdouble
interpolate_side(gconstpointer points, gsize point_size,
                 guint ia, guint ib,
                 const Point *pt)
{
    const PointXYZ *a, *b;
    gdouble d;

    a = get_point_xyz(points, point_size, ia);
    b = get_point_xyz(points, point_size, ib);
    d = side_intersection_distance(a, b, pt);

    if (d <= -1.0)
        return a->z;
    else if (d >= 1.0)
        return b->z;

    return 0.5*((d + 1.0)*b->z + (1.0 - d)*a->z);
}

static inline gdouble
interpolate_sidelinear(const Triangulation *triangulation,
                       gconstpointer points, gsize point_size,
                       const Triangle *triangle, const Point *pt)
{
    guint ia, ib;

    ia = triangle->ia;
    ib = triangle->ib;
    if (find_nearest_side(triangulation, points, point_size, &ia, &ib, pt))
        return interpolate_side(points, point_size, ia, ib, pt);

    ia = triangle->ib;
    ib = triangle->ic;
    if (find_nearest_side(triangulation, points, point_size, &ia, &ib, pt))
        return interpolate_side(points, point_size, ia, ib, pt);

    ia = triangle->ic;
    ib = triangle->ia;
    if (find_nearest_side(triangulation, points, point_size, &ia, &ib, pt))
        return interpolate_side(points, point_size, ia, ib, pt);

    g_assert_not_reached();
    return 0.0;
}

/**
 * gwy_delaunay_interpolate:
 * @triangulation: Delaunay triangulation.
 * @points: Array of points.  They must be typecastable to
 *          @GwyDelaunayPointXYZ, however they can be larger than that.
 *          The actual struct size is indicated by @point_size.  Generally,
 *          this must be the same array as passed to
 *          gwy_delaunay_triangulate().
 * @point_size: Size of point struct, in bytes.
 * @interpolation: Interpolation to use.  Only @GWY_INTERPOLATION_ROUND and
 *                 @GWY_INTERPOLATION_BILINEAR are implemented.  Is is an error
 *                 to pass any other interpolation type.
 * @dfield: Data field to fill with interpolated values.
 *
 * Regularizes XYZ data to a grid, represented by a data field.
 *
 * The area and resolution of the regular grid is given by the dimensions and
 * offsets of @dfield.
 *
 * Since: 2.18.
 **/
void
gwy_delaunay_interpolate(GwyDelaunayTriangulation *gwytri,
                         gconstpointer points,
                         gsize point_size,
                         G_GNUC_UNUSED GwyInterpolationType interpolation,
                         GwyDataField *dfield)
{
    Triangulation *triangulation;
    guint xres, yres, i, j;
    gdouble qx, qy, xoff, yoff;
    gdouble *d;
    Triangle triangle;
    Point pt;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(point_size >= sizeof(PointXYZ));

    triangulation = triangulation_new_from_finalized(gwytri);
    find_boundary(triangulation, points, point_size);

    make_valid_triangle(triangulation, points, point_size, &triangle, 0);

    xres = dfield->xres;
    yres = dfield->yres;
    xoff = dfield->xoff;
    yoff = dfield->yoff;
    qx = dfield->xreal/dfield->xres;
    qy = dfield->yreal/dfield->yres;
    d = dfield->data;

    for (i = 0; i < yres; i++) {
        pt.y = yoff + qy*(i + 0.5);
        for (j = 0; j < xres; j++) {
            pt.x = xoff + qx*(j + 0.5);
            if (ensure_triangle(triangulation, points, point_size,
                                &triangle, &pt))
                *d = interpolate_linear(&triangle);
            else
                *d = interpolate_sidelinear(triangulation, points, point_size,
                                            &triangle, &pt);
            d++;
        }
    }

    triangulation->neighbours = NULL;   /* Owned by gwytri */
    triangulation_free(triangulation);

    gwy_data_field_invalidate(dfield);
}

#if 0
G_GNUC_UNUSED
static void
dump_neighbours(const Triangulation *triangulation)
{
    guint i, j;

    for (i = 0; i < triangulation->nlen; i++) {
        for (j = 0; j < triangulation->npoints; j++) {
            if (triangulation->blocks[j].pos == i)
                g_print("(%u)", j);
        }

        if (triangulation->neighbours[i] == UNDEF)
            g_print(".");
        else
            g_print("%u", triangulation->neighbours[i]);
        g_print(" ");
    }
    g_print("\n");
}

G_GNUC_UNUSED
static void
dump_triangulation(const Triangulation *triangulation)
{
    NeighbourBlock *nb;
    guint i, j;

    for (i = 0; i < triangulation->npoints; i++) {
        nb = triangulation->blocks + i;
        g_print("%u:", i);
        for (j = 0; j < nb->len; j++)
            g_print(" %u", triangulation->neighbours[nb->pos + j]);
        g_print("\n");
    }
}

G_GNUC_UNUSED
static void
dump_points(const Triangulation *triangulation,
            guint npoints, gconstpointer points, gsize point_size)
{
    NeighbourBlock *nb;
    guint i, j, ni;
    const guint *neighbours;
    FILE *fh;

    fh = fopen("points.dat", "w");
    for (i = 0; i < npoints; i++) {
        const Point *pt = get_point(points, point_size, i);
        fprintf(fh, "%u %g %g\n", i, pt->x, pt->y);
    }
    fclose(fh);

    fh = fopen("arrows.gpi", "w");
    //fprintf(fh, "set xtics %g\n", grid->dx);
    //fprintf(fh, "set ytics %g\n", grid->dy);
    for (i = 0; i < triangulation->npoints; i++) {
        nb = triangulation->blocks + i;
        neighbours = triangulation->neighbours + nb->pos;
        for (j = 0; j < nb->len; j++) {
            ni = neighbours[j];
            if (ni > i) {
                const Point *pt1 = get_point(points, point_size, i);
                const Point *pt2 = get_point(points, point_size, ni);
                fprintf(fh, "set arrow from %g,%g to %g,%g nohead ls 2\n",
                        pt1->x, pt1->y, pt2->x, pt2->y);
            }
        }
    }
    fclose(fh);
}

G_GNUC_UNUSED
static guint
test_reflexivity(const Triangulation *triangulation)
{
    NeighbourBlock *nb, *nbn;
    guint i, j, count;
    const guint *neighbours, *neighboursn;

    count = 0;
    for (i = 0; i < triangulation->npoints; i++) {
        nb = triangulation->blocks + i;
        neighbours = triangulation->neighbours + nb->pos;
        for (j = 0; j < nb->len; j++) {
            nbn = triangulation->blocks + neighbours[j];
            neighboursn = triangulation->neighbours + nb->pos;
            if (find_neighbour(neighboursn, nbn->len, i) == UNDEF) {
                g_printerr("Point %u has neighbour %u "
                           "but the reverse does not hold.\n",
                           i, neighbours[j]);
                count++;
            }
        }
    }
    return count;
}
#endif

/************************** Documentation ****************************/

/**
 * SECTION:delaunay
 * @title: Delaunay
 * @short_description: Delaunay triangulation and interpolation
 **/

/**
 * GwyDelaunayPointXY:
 * @x: X-coordinate.
 * @y: Y-coordinate.
 *
 * Representation of a point in plane for triangulation.
 *
 * Since: 2.18
 **/

/**
 * GwyDelaunayPointXYZ:
 * @x: X-coordinate.
 * @y: Y-coordinate.
 * @z: Z-coordinate, i.e. the value in point (@x,@y).
 *
 * Representation of a point in plane with associated value for interpolation.
 *
 * Since: 2.18
 **/

/**
 * GwyDelaunayTriangulation:
 * @npoints: The number of points, this is equal to the number of points
 *           passed to gwy_delaunay_triangulate().
 * @size: Size of @neighbours array, also equal to the last item of @index.
 * @index: Positions where lists of neighbours for individual points start in
 *         @neighbours.  The array has @npoints+1 elements so, the neighbours
 *         of point @i are at positions @index[@i] to @index[@i+1]-1.
 * @neighbours: Lists of neigbours for invididual points, packed in a one large
 *              array.
 *
 * Delaunay triangulation representation.
 *
 * Since: 2.18
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
