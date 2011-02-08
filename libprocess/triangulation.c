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
#ifdef DEBUG
#include <stdio.h>
#endif
#include <string.h>
#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/triangulation.h>

/*
 * Some identities for planar triangulations
 * v ... number of vertices
 * h ... number of edges
 * t ... number of triangles
 * b ... number of boundary edges
 *
 *    t = h - (v - 1)
 *    b = 3*(v - 1) - h
 *
 * They are expressed using v and h because that's what we normally have
 * available: the number of points and the size of neigbours[] (where each
 * edge is counted twice).
 */

enum {
    UNDEF = GWY_TRIANGULATION_NONE,
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

#define get_vpoint(tri, i) \
    ((i) >= tri->npoints \
     ? (const Point*)(tri->vpoints + ((i) - tri->npoints)) \
     : (const Point*)((const gchar*)(tri->points) + (i)*(tri->point_size)))

#define get_vpoint_xyz(tri, i) \
    ((i) >= tri->npoints \
     ? (const PointXYZ*)(tri->vpoints + ((i) - tri->npoints)) \
     : (const PointXYZ*)((const gchar*)(tri->points) + (i)*(tri->point_size)))

/* Representation of input data, they must be typecastable to this but can
 * contain any more data in the structs. */
typedef GwyTriangulationPointXY Point;
typedef GwyTriangulationPointXYZ PointXYZ;

/* Triangulation private data.  More or less representation of the result as
 * the in-progress data is represented by Triangulator. */
typedef struct {
    guint npoints;
    guint nsize;
    guint blen;
    guint nvpoints;
    guint nvoronoi;
    guint point_size;
    gconstpointer points;
    Point *vpoints;
    guint *index;
    guint *neighbours;
    guint *boundary;
    guint *bindex;
    guint *vindex;
    guint *voronoi;
} Triangulation;

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

/* Information about blocks of neighbours in Triangulator. */
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
} Triangulator;

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

#ifdef DEBUG
static void dump_neighbours(const Triangulator *triangulator);
static void dump_triangulator(const Triangulator *triangulator);
static void dump_points_(const Triangulator *triangulator,
                        guint npoints, gconstpointer points, gsize point_size);
static void dump_points(const Triangulation *triangulation,
                        gconstpointer points, gsize point_size);
static void dump_voronoi(const Triangulation *triangulation,
                         gconstpointer points, gsize point_size);
static guint test_reflexivity(const Triangulator *triangulator);

G_GNUC_UNUSED
static void
dump_block(const gchar *info,
            guint id, const guint *nindex, const guint *neighbours)
{
    guint i;

    g_printerr("[%u]%s", id, info);
    for (i = nindex[id]; i < nindex[id+1]; i++)
        g_printerr(" %u", neighbours[i]);
    g_printerr("\n");
}
#endif

#define GWY_TRIANGULATION_GET_PRIVATE(obj)  \
   (G_TYPE_INSTANCE_GET_PRIVATE((obj), GWY_TYPE_TRIANGULATION, GwyTriangulationPrivate))

typedef Triangulation GwyTriangulationPrivate;

static void gwy_triangulation_finalize(GObject *object);

G_DEFINE_TYPE(GwyTriangulation, gwy_triangulation, G_TYPE_OBJECT)

static void
gwy_triangulation_class_init(GwyTriangulationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(GwyTriangulationPrivate));

    gobject_class->finalize = gwy_triangulation_finalize;
}

static void
gwy_triangulation_init(G_GNUC_UNUSED GwyTriangulation *object)
{
}

static void
gwy_triangulation_finalize(GObject *object)
{
    Triangulation *triangulation = GWY_TRIANGULATION_GET_PRIVATE(object);

    g_free(triangulation->index);
    g_free(triangulation->neighbours);
    g_free(triangulation->boundary);
    g_free(triangulation->bindex);
    g_free(triangulation->vpoints);
    g_free(triangulation->vindex);
    g_free(triangulation->voronoi);
    /* Don't own that but leave no pointers behind. */
    triangulation->points = NULL;
}

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
free_point_list(PointList *pointlist)
{
    g_free(pointlist->orig_index);
    g_free(pointlist->points);
}

static inline gboolean
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
    if (G_UNLIKELY(wpt->r == 0.0))
        return FALSE;
    wpt->phi = atan2(pt.y, pt.x);
    /* Lines start open-ended */
    wpt->tprev = -G_MAXDOUBLE;
    wpt->tnext = G_MAXDOUBLE;

    return TRUE;
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

static inline gboolean
ccw_angle_convex(gdouble phi1, gdouble phi2)
{
    return fmod(phi2 - phi1 + 2*G_PI, 2*G_PI) <= G_PI;
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
        if (ccw_angle_convex(p->phi, x->phi))
            intersection_times(p, &p->tnext, x, &x->tprev);
        else
            intersection_times(x, &x->tnext, q, &q->tprev);

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
    if (!get_workspace_point(&wpt, pointlist, id, &wspace->origin)) {
        wspace->len = 0;
        return FALSE;
    }

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

static inline void
work_space_init(WorkSpace *wspace)
{
    gwy_clear(wspace, 1);
    wspace->size = WORKSPACE;
    wspace->data = g_new(WorkSpacePoint, wspace->size);
}

static inline void
work_space_destroy(WorkSpace *wspace)
{
    g_free(wspace->data);
}

/* Put back the wspace for the neighbourhood of a given point to
 * triangulator->neighbours[]. */
static void
work_space_construct(const Triangulator *triangulator,
                     const PointList *pointlist,
                     guint id,
                     WorkSpace *wspace)
{
    const NeighbourBlock *nb;
    WorkSpacePoint *wdata;
    guint j;
    const guint *neighbours;

    nb = triangulator->blocks + id;
    neighbours = triangulator->neighbours + nb->pos;

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

static inline WorkSpaceCache*
work_space_cache_new(void)
{
    WorkSpaceCache *wscache;

    wscache = g_new0(WorkSpaceCache, 1);
    block_clear(wscache->id, WSPACE_CACHE_SIZE);

    return wscache;
}

static inline void
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

/* The slow path for wspace reintegration into triangulator->neighbours[].
 * Anything from reclaiming space in the next block to making neighbours[]
 * bigger can happen. */
static guint*
relocate_block(Triangulator *triangulator,
               NeighbourBlock *nb,
               guint len)
{
    guint j, remaining, newsize;
    guint *neighbours;

    /* Try to find space after the end of the current block.  We know
     * the blocks sizes are multiples of NEIGHBOURS, so only check if
     * the first item is UNDEF. */
    neighbours = triangulator->neighbours + nb->pos;
    newsize = block_size(len + 2);
    remaining = triangulator->nsize - nb->pos;
    for (j = nb->size;
         j < remaining && neighbours[j] == UNDEF && j < newsize;
         j++)
        ;

    if (j < len) {
        /* Must relocate the current block */
        if (triangulator->nlen + newsize > triangulator->nsize) {
            /* Must reallocate triangulator->neighbours */
            j = triangulator->nsize;
            triangulator->nsize = block_size(3*j/2);
            triangulator->neighbours = g_renew(guint,
                                                triangulator->neighbours,
                                                triangulator->nsize);
            neighbours = triangulator->neighbours + nb->pos;
            block_clear(triangulator->neighbours + j,
                        triangulator->nsize - j);
        }
        block_clear(neighbours, nb->len);
        nb->pos = triangulator->nlen;
        nb->size = newsize;
        nb->len = 0;
        triangulator->nlen += newsize;
        neighbours = triangulator->neighbours + nb->pos;
    }
    else {
        nb->size = j;
        triangulator->nlen = MAX(triangulator->nlen, nb->pos + j);
    }

    return neighbours;
}

static void
reintegrate_workspace(Triangulator *triangulator,
                      guint id,
                      WorkSpace *wspace)
{
    NeighbourBlock *nb;
    guint *neighbours;
    guint j;

    nb = triangulator->blocks + id;
    neighbours = triangulator->neighbours + nb->pos;
    if (wspace->len > nb->size)
        neighbours = relocate_block(triangulator, nb, wspace->len);
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
 * triangle has become clockwise.  If TRUE is returned, then @opposite is
 * unchanged and the triangle is kept counter-clockwise. */
static gboolean
find_the_other_neighbour_(const Triangulator *triangulator,
                          gconstpointer points, gsize point_size,
                          guint from,
                          guint to,
                          guint *opposite)
{
    NeighbourBlock *nb;
    const Point *a, *b, *c;
    guint to_prev, from_next;
    const guint *neighbours;

    nb = triangulator->blocks + from;
    neighbours = triangulator->neighbours + nb->pos;
    to_prev = prev_neighbour(neighbours, nb->len,
                             find_neighbour(neighbours, nb->len, to));

    nb = triangulator->blocks + to;
    neighbours = triangulator->neighbours + nb->pos;
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
move_triangle_a_(const Triangulator *triangulator,
                 gconstpointer points, gsize point_size,
                 Triangle *triangle)
{
    if (find_the_other_neighbour_(triangulator, points, point_size,
                                  triangle->ib, triangle->ic, &triangle->ia)) {
        GWY_SWAP(guint, triangle->ib, triangle->ic);
        GWY_SWAP(const PointXYZ*, triangle->b, triangle->c);
        return TRUE;
    }
    return FALSE;
}

static inline gboolean
move_triangle_b_(const Triangulator *triangulator,
                 gconstpointer points, gsize point_size,
                 Triangle *triangle)
{
    if (find_the_other_neighbour_(triangulator, points, point_size,
                                  triangle->ic, triangle->ia, &triangle->ib)) {
        GWY_SWAP(guint, triangle->ic, triangle->ia);
        GWY_SWAP(const PointXYZ*, triangle->c, triangle->a);
        return TRUE;
    }
    return FALSE;
}

static inline gboolean
move_triangle_c_(const Triangulator *triangulator,
                 gconstpointer points, gsize point_size,
                 Triangle *triangle)
{
    if (find_the_other_neighbour_(triangulator, points, point_size,
                                  triangle->ia, triangle->ib, &triangle->ic)) {
        GWY_SWAP(guint, triangle->ia, triangle->ib);
        GWY_SWAP(const PointXYZ*, triangle->a, triangle->b);
        return TRUE;
    }
    return FALSE;
}

/* Initializes @triangle to any valid triangle containing point @hint. */
static void
make_valid_triangle(const guint *neighbours, guint len,
                    gconstpointer points, gsize point_size,
                    Triangle *triangle,
                    guint hint)
{
    const Point *a = get_point(points, point_size, hint);
    const Point *b, *c;
    gdouble phib, phic;
    guint i;

    triangle->ia = hint;
    for (i = 0; i < len; i++) {
        triangle->ib = neighbours[i];
        triangle->ic = next_neighbour(neighbours, len, i);

        b = get_point(points, point_size, triangle->ib);
        phib = atan2(b->y - a->y, b->x - a->x);
        c = get_point(points, point_size, triangle->ic);
        phic = atan2(c->y - a->y, c->x - a->x);

        if (ccw_angle_convex(phib, phic)) {
            make_triangle(triangle, points, point_size);
            return;
        }
    }

    g_assert_not_reached();
}

/* If TRUE is returned, then a neighbour on the other side was found and the
 * triangle has become clockwise.  If TRUE is returned, then @opposite is
 * unchanged and the triangle is kept counter-clockwise. */
static gboolean
find_the_other_neighbour(const Triangulation *triangulation,
                         guint from,
                         guint to,
                         guint *opposite)
{
    const Point *a, *b, *c;
    guint to_prev, from_next, pos, len;
    const guint *neighbours;

    pos = triangulation->index[from];
    len = triangulation->index[from + 1] - pos;
    neighbours = triangulation->neighbours + pos;
    to_prev = prev_neighbour(neighbours, len,
                             find_neighbour(neighbours, len, to));

    pos = triangulation->index[to];
    len = triangulation->index[to + 1] - pos;
    neighbours = triangulation->neighbours + pos;
    from_next = next_neighbour(neighbours, len,
                               find_neighbour(neighbours, len, from));

    /* Now there are some silly few-point special cases.  If @opposite is in
     * the centre of a triangle formed by @from, @to and the newly found point,
     * then we have an apparent match but it is not the point we are looking
     * for.  Check that the points really lies on the opposite side. */
    if (from_next != to_prev || from_next == *opposite)
        return FALSE;

    a = get_point(triangulation->points, triangulation->point_size, from);
    b = get_point(triangulation->points, triangulation->point_size, to);
    c = get_point(triangulation->points, triangulation->point_size, to_prev);
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
move_triangle_a(const Triangulation *triangulation, Triangle *triangle)
{
    if (find_the_other_neighbour(triangulation,
                                 triangle->ib, triangle->ic, &triangle->ia)) {
        GWY_SWAP(guint, triangle->ib, triangle->ic);
        GWY_SWAP(const PointXYZ*, triangle->b, triangle->c);
        return TRUE;
    }
    return FALSE;
}

static inline gboolean
move_triangle_b(const Triangulation *triangulation, Triangle *triangle)
{
    if (find_the_other_neighbour(triangulation,
                                 triangle->ic, triangle->ia, &triangle->ib)) {
        GWY_SWAP(guint, triangle->ic, triangle->ia);
        GWY_SWAP(const PointXYZ*, triangle->c, triangle->a);
        return TRUE;
    }
    return FALSE;
}

static inline gboolean
move_triangle_c(const Triangulation *triangulation, Triangle *triangle)
{
    if (find_the_other_neighbour(triangulation,
                                 triangle->ia, triangle->ib, &triangle->ic)) {
        GWY_SWAP(guint, triangle->ia, triangle->ib);
        GWY_SWAP(const PointXYZ*, triangle->a, triangle->b);
        return TRUE;
    }
    return FALSE;
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

    p = get_point(triangulation->points, triangulation->point_size, ip);
    a = get_point(triangulation->points, triangulation->point_size, ia);
    b = get_point(triangulation->points, triangulation->point_size, ib);
    n = get_point(triangulation->points, triangulation->point_size, in);

    iter = 0;
    while (TRUE) {
        find_side_section(p, a, b, n, &origin);

        phia = atan2(a->y - origin.y, a->x - origin.x);
        phib = atan2(b->y - origin.y, b->x - origin.x);
        phi = atan2(pt->y - origin.y, pt->x - origin.x);

        forw = ccw_angle_convex(phia, phi);
        back = ccw_angle_convex(phi, phib);

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
            n = get_point(triangulation->points, triangulation->point_size, in);
        }
        else {
            in = ib;
            ib = ia;
            ia = ip;
            ip = boundary[(bindex[ia] + blen-1) % blen];
            n = b;
            b = a;
            a = p;
            p = get_point(triangulation->points, triangulation->point_size, ip);
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
ensure_triangle_(const Triangulator *triangulator,
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
                moved = move_triangle_a_(triangulator, points, point_size,
                                         triangle);
            else
                moved = move_triangle_c_(triangulator, points, point_size,
                                         triangle);
        }
        else {
            if (triangle->db <= triangle->dc)
                moved = move_triangle_b_(triangulator, points, point_size,
                                         triangle);
            else
                moved = move_triangle_c_(triangulator, points, point_size,
                                         triangle);
        }

        if (!moved)
            return FALSE;

        make_triangle(triangle, points, point_size);
        if (G_UNLIKELY(iter++ == triangulator->npoints)) {
            triangle->ia = triangle->ib = triangle->ic = UNDEF;
            return FALSE;
        }
    }

    return TRUE;
}

/* Ensures @triangle contains point @pt.  A relatively quick test if it already
 * contains the point.  If the right triangle is nearby, it is also found
 * reasonably fast. */
static gboolean
ensure_triangle(const Triangulation *triangulation,
                Triangle *triangle,
                const Point *pt)
{
    gboolean moved;
    guint iter;

    iter = 0;
    while (!triangle_contains_point(triangle, pt)) {
        if (triangle->da <= triangle->db) {
            if (triangle->da <= triangle->dc)
                moved = move_triangle_a(triangulation, triangle);
            else
                moved = move_triangle_c(triangulation, triangle);
        }
        else {
            if (triangle->db <= triangle->dc)
                moved = move_triangle_b(triangulation, triangle);
            else
                moved = move_triangle_c(triangulation, triangle);
        }

        if (!moved)
            return FALSE;

        make_triangle(triangle,
                      triangulation->points, triangulation->point_size);
        if (G_UNLIKELY(iter++ == triangulation->npoints)) {
            triangle->ia = triangle->ib = triangle->ic = UNDEF;
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
compactify(const Triangulator *triangulator,
           const PointList *pointlist,
           Triangulation *triangulation)
{
    const NeighbourBlock *nb;
    const guint *neighbours, *orig_index;
    guint *dest;
    guint i, j, iorig;

    if (G_UNLIKELY(triangulation->nsize % 2 != 0))
        return FALSE;

    orig_index = pointlist->orig_index;

    /* Construct the back-mapped point_index */
    g_assert(triangulator->npoints == pointlist->npoints);
    triangulation->npoints = triangulator->npoints;
    triangulation->index = g_renew(guint, triangulation->index,
                                   triangulation->npoints+1);
    for (i = 0; i < triangulator->npoints; i++) {
        iorig = orig_index[i];
        triangulation->index[iorig] = triangulator->blocks[i].len;
    }

    index_accumulate(triangulation->index, triangulation->npoints);
    index_rewind(triangulation->index, triangulation->npoints);

    /* Fill neighbours with back-mapped neighbour indices */
    triangulation->nsize = triangulation->index[triangulation->npoints];
    triangulation->neighbours = g_renew(guint, triangulation->neighbours,
                                        triangulation->nsize);
    for (i = 0; i < triangulator->npoints; i++) {
        iorig = orig_index[i];
        nb = triangulator->blocks + i;
        neighbours = triangulator->neighbours + nb->pos;
        dest = triangulation->neighbours + triangulation->index[iorig];
        for (j = 0; j < nb->len; j++)
            dest[j] = orig_index[neighbours[j]];
    }

    return TRUE;
}

/* Create a triangle from the first three points.
 * FIXME: If they form a straight line, this will fail. */
static gboolean
create_first_triangle(const PointList *pointlist,
                      Triangulator *triangulator,
                      WorkSpace *wspace)
{
    NeighbourBlock *nb;
    guint i, j, n3;

    n3 = MIN(pointlist->npoints, 3);
    for (i = 0; i < n3; i++) {
        wspace->len = 0;
        wspace->origin = pointlist->points[i];
        for (j = 0; j < n3; j++) {
            if (j != i)
                try_to_add_point(pointlist, j, wspace);
        }
        if (wspace->len + 1 != n3)
            return FALSE;
        nb = triangulator->blocks + i;
        nb->pos = i*NEIGHBOURS;
        nb->size = NEIGHBOURS;
        nb->len = 0;
        reintegrate_workspace(triangulator, i, wspace);
    }
    triangulator->npoints = i;
    triangulator->nlen = i*NEIGHBOURS;

    return TRUE;
}

static inline void
work_queue_init(WorkQueue *queue)
{
    queue->size = QUEUE;
    queue->len = 0;
    queue->id = g_new(guint, queue->size);
    queue->success = g_new(gboolean, queue->size);
}

static inline void
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

static Triangulator*
triangulator_new_from_pointlist(const PointList *pointlist)
{
    Triangulator *triangulator;

    triangulator = g_new0(Triangulator, 1);
    /* A reasonable estimate */
    triangulator->nsize = 2*NEIGHBOURS*pointlist->npoints;
    triangulator->blocks = g_new(NeighbourBlock, pointlist->npoints);
    triangulator->neighbours = g_new(guint, triangulator->nsize);
    block_clear(triangulator->neighbours, triangulator->nsize);

    return triangulator;
}

static void
triangulator_free(Triangulator *triangulator)
{
    g_free(triangulator->blocks);
    g_free(triangulator->neighbours);
    g_free(triangulator);
}

static Triangulator*
triangulate(const PointList *pointlist)
{
    Triangulator *triangulator;
    WorkSpaceCache *wscache;
    WorkSpace *wspace;
    Triangle triangle;
    WorkQueue queue;
    guint i, j;

    triangulator = triangulator_new_from_pointlist(pointlist);
    wscache = work_space_cache_new();
    work_queue_init(&queue);

    /* create_first_triangle() ends up with point 2 in the work space. */
    wspace = work_space_cache_get(wscache, 2);
    if (!create_first_triangle(pointlist, triangulator, wspace))
        goto fail;

    for (i = 3; i < pointlist->npoints; i++) {
        NeighbourBlock *nb;
        gboolean in;

        /* Make a valid triangle.  The neighbour updates might turn a
         * previously valid triangle to an invalid one. */
        nb = triangulator->blocks + (i - 1);
        make_valid_triangle(triangulator->neighbours + nb->pos, nb->len,
                            pointlist->points, sizeof(Point),
                            &triangle, i-1);
        /* Find the enclosing or the nearest (for outside points) triangle */
        in = ensure_triangle_(triangulator, pointlist->points, sizeof(Point),
                              &triangle, pointlist->points + i);
        if (G_UNLIKELY(triangle.ia == UNDEF))
            goto fail;
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
                work_space_construct(triangulator, pointlist, id, wspace);

            /* FIXME: This permits spontaneous loss of relfectivity.
             * We should update the neighbours selfconsitently.  That is, we
             * know the only new point that can appear is @i.  Hence @i's
             * neighbourhood simply consists of points in the queue that has @i
             * as their neihgbour.  When try_to_add_point() removes a shadowed
             * point we should immediately remove the opposite neighbour
             * relation from neighbours[]. */
            /* Even then we fail miserably in ambiguous cases when we typically
             * make both diagonals parts of the triangulator. */
            if (try_to_add_point(pointlist, i, wspace)) {
                if (G_UNLIKELY(!wspace->len))
                    goto fail;
                /* If we added point i among the neighbours of point id, any
                 * of the id's neighbours may also need updating. */
                nb = triangulator->blocks + id;
                neighbours = triangulator->neighbours + nb->pos;
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

                reintegrate_workspace(triangulator, id, wspace);
            }
            /* Move id from todo part to processed part. */
            queue.pos++;
        }

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
                if (G_UNLIKELY(!in || !wspace->len))
                    goto fail;
            }
        }

        /* Append an empty zero-length block, reintegrate_workspace() will fix
         * it to the required size. */
        nb = triangulator->blocks + i;
        nb->pos = triangulator->nlen;
        nb->size = 0;
        nb->len = 0;
        triangulator->npoints++;
        reintegrate_workspace(triangulator, i, wspace);
    }

    work_space_cache_free(wscache);
    work_queue_destroy(&queue);

    return triangulator;

fail:
    work_space_cache_free(wscache);
    work_queue_destroy(&queue);
    triangulator_free(triangulator);

    return NULL;
}

static gboolean
find_boundary(Triangulation *triangulation,
              gconstpointer points, gsize point_size)
{
    guint i, imin, bsize, expected_blen, pos, len;
    const Point *pt;
    const guint *neighbours;
    gdouble xmin;

    triangulation->bindex = g_renew(guint, triangulation->bindex,
                                    triangulation->npoints + 1);
    /* We promise GwyTriangulationData.index[size] always exists... */
    triangulation->bindex[triangulation->npoints] = UNDEF;
    block_clear(triangulation->bindex, triangulation->npoints);
    /* If the triangulation is correct this formula holds, see the identities
     * near the start of the file.  */
    expected_blen = 3*(triangulation->npoints - 1) - triangulation->nsize/2;
    bsize = expected_blen;
    triangulation->boundary = g_renew(guint, triangulation->boundary, bsize);
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
        return TRUE;

    /* Its neighbour with the lowest direction angle is the first side,
     * Since the points are already sorted this way it suffices to take
     * neighbours[0].  */
    neighbours = triangulation->neighbours + triangulation->index[imin];
    imin = neighbours[0];
    triangulation->bindex[imin] = triangulation->blen;
    triangulation->boundary[triangulation->blen++] = imin;
    if (triangulation->npoints == 2)
        return TRUE;

    /* The remaining points are always next to the previous boundary point */
    while (TRUE) {
        i = triangulation->boundary[triangulation->blen-2];
        pos = triangulation->index[imin];
        len = triangulation->index[imin+1] - pos;
        neighbours = triangulation->neighbours + pos;
        i = next_neighbour(neighbours, len, find_neighbour(neighbours, len, i));
        if (i == triangulation->boundary[0])
            return triangulation->blen == bsize;
        if (G_UNLIKELY(triangulation->blen == bsize))
            return FALSE;
        imin = i;
        triangulation->bindex[imin] = triangulation->blen;
        triangulation->boundary[triangulation->blen++] = imin;
    }

    g_assert_not_reached();
    return FALSE;
}

/* Calculate circumcircle centre for a triangle that is counter-clockwise at
 * point @a.  Use the trick with shifting the origin to point @a to simplify
 * the formulas. */
static gboolean
circumcircle_centre(const Point *a,
                    const Point *b,
                    const Point *c,
                    Point *pt)
{
    Point ca, ba;
    gdouble phib, phic, det, ba2, ca2;

    ba.x = b->x - a->x;
    ba.y = b->y - a->y;
    ca.x = c->x - a->x;
    ca.y = c->y - a->y;
    phib = atan2(ba.y, ba.x);
    phic = atan2(ca.y, ca.x);
    if (!ccw_angle_convex(phib, phic))
        return FALSE;

    ba2 = ba.x*ba.x + ba.y*ba.y;
    ca2 = ca.x*ca.x + ca.y*ca.y;
    det = 2*(ba.y*ca.x - ba.x*ca.y);
    /* XXX */
    if (!det)
        return FALSE;

    pt->x = a->x + (ba.y*ca2 - ca.y*ba2)/det;
    pt->y = a->y + (ca.x*ba2 - ba.x*ca2)/det;
    return TRUE;
}

static gboolean
add_point_id(const Triangulation *triangulation,
             guint i,
             guint ni,
             guint *vneighbours,
             guint toadd)
{
    guint pos, len, j;

    pos = triangulation->index[i];
    len = triangulation->index[i+1] - pos;
    j = find_neighbour(triangulation->neighbours + pos, len, ni);
    if (G_UNLIKELY(j == UNDEF || vneighbours[j] != UNDEF))
        return FALSE;
    vneighbours[j] = toadd;
    return TRUE;
}

static gboolean
add_common_neighbour(guint *vneighbours,
                     const guint *vindex,
                     guint ignore,
                     guint ia, guint ib,
                     guint addat)
{
    guint i, j, ni, nj;

    ia = vneighbours[ia];
    ib = vneighbours[ib];

    for (i = vindex[ia]; i < vindex[ia+1]; i++) {
        ni = vneighbours[i];
        if (ni == ignore || ni == UNDEF)
            continue;
        for (j = vindex[ib]; j < vindex[ib+1]; j++) {
            nj = vneighbours[j];
            if (nj == ni) {
                vneighbours[addat] = nj;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static gboolean
add_infinity_neighbour(guint *vneighbours,
                       const guint *vindex,
                       guint ignore,
                       guint ia,
                       guint addat)
{
    guint i, ni;

    ia = vneighbours[ia];

    for (i = vindex[ia]; i < vindex[ia+1]; i++) {
        ni = vneighbours[i];
        if (ni == ignore || ni == UNDEF)
            continue;
        if (vindex[ni+1] - vindex[ni] == 5) {
            vneighbours[addat] = ni;
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
delaunay_to_voronoi(Triangulation *triangulation,
                    gconstpointer points, gsize point_size)
{
    const Point *a, *b, *c;
    Point *vpoints;
    Point pt;
    guint *voronoi, *vindex, *neighbours, *remaining;
    guint i, j, n, ni, next, prev, nvpoints, nvoronoi, pos, len, vpos, vm1;
    guint blen;
    gdouble h, xmin, xmax, ymin, ymax, far_away;

    blen = triangulation->blen;
    /* This is exact if counting also the vertices in infinities (the formula
     * is more understandably t+b).  See the identities at the begining of the
     * file. */
    vm1 = triangulation->npoints - 1;
    nvpoints = triangulation->nvpoints = 2*vm1;
    vpoints = triangulation->vpoints = g_renew(Point, triangulation->vpoints,
                                               nvpoints);
    /* Voronoi points in infinity have only 5 neighbours but boundary Delaunay
     * points will gain one neigbour more, so this should be exact. */
    nvoronoi = triangulation->nvoronoi = 12*vm1 + triangulation->nsize;
    voronoi = triangulation->voronoi= g_renew(guint, triangulation->voronoi,
                                              nvoronoi);
    block_clear(voronoi, nvoronoi);

    /* We know exactly how many neighbours a Delaunay point will have so,
     * prefill the index. */
    vindex = triangulation->vindex = g_renew(guint, triangulation->vindex,
                                             triangulation->npoints
                                             + nvpoints + 1);
    vpos = 0;
    for (n = 0; n < triangulation->npoints; n++) {
        vindex[n] = vpos;
        vpos += triangulation->index[n+1] - triangulation->index[n];
        /* Boundary points will gain one neighbour as there are two Voronoi
         * points in infinity. */
        if (triangulation->bindex[n] != UNDEF)
            vpos++;
    }
    /* We know the exact number of edges from original points. */
    if (G_UNLIKELY(vpos != 3*vm1 + triangulation->nsize/2))
        return FALSE;

    /* Now the Voronoi points.  In the first pass, create the points and
     * resolve Delaunay neighbours.  Mutual Voronoi point relations will be
     * resolved later.  */
    xmin = ymin = G_MAXDOUBLE;
    xmax = ymax = -G_MAXDOUBLE;
    for (i = 0; i < triangulation->npoints; i++) {
        a = get_point(points, point_size, i);
        pos = triangulation->index[i];
        len = triangulation->index[i+1] - pos;
        neighbours = triangulation->neighbours + pos;
        prev = neighbours[len - 1];
        for (j = 0; j < len; j++) {
            next = neighbours[j];
            if (prev > i && next > i) {
                b = get_point(points, point_size, prev);
                c = get_point(points, point_size, next);
                if (circumcircle_centre(a, b, c, &pt)) {
                    if (pt.x < xmin)
                        xmin = pt.x;
                    if (pt.x > xmax)
                        xmax = pt.x;
                    if (pt.y < ymin)
                        ymin = pt.y;
                    if (pt.y > ymax)
                        ymax = pt.y;
                    /* Add a new Voronoi point and make a, b and c its
                     * neighbours. */
                    if (G_UNLIKELY(n - triangulation->npoints == nvpoints))
                        return FALSE;
                    vpoints[n - triangulation->npoints] = pt;
                    vindex[n] = vpos;
                    voronoi[vpos + 1] = i;
                    voronoi[vpos + 3] = prev;
                    voronoi[vpos + 5] = next;
                    vpos += 6;   /* Make space for the Voronoi neighbours. */
                    /* Conversely, add it to the neighbourhood of a, b and c. */
                    if (!add_point_id(triangulation, i, prev,
                                      voronoi + vindex[i], n)
                        || !add_point_id(triangulation, prev, next,
                                         voronoi + vindex[prev], n)
                        || !add_point_id(triangulation, next, i,
                                         voronoi + vindex[next], n))
                        return FALSE;
                    n++;
                }
            }
            prev = next;
        }
    }
    /* Base the notion of what is sufficiently far away on the inner Voronoi
     * points.  They can be relatively far away too as the boundary triangles
     * tend to be quite flat. */
    far_away = 10.0*hypot(xmax - xmin, ymax - ymin);

    /* Compactify the two free positions in neighbourhoods of boundary points.
     * One is always at the end now because the new neighbourhood is one item
     * longer but we did not take this into account.  The two free positions
     * correspond to the infinity points of Voronoi grid and they should come
     * together.  Then we can really place the first infinity neighbour to the
     * position of the previous point in neighbours[] and the second infinity
     * point to the following position. */
    for (j = 0; j < blen; j++) {
        i = triangulation->boundary[j];
        pos = triangulation->vindex[i];
        len = triangulation->vindex[i+1] - pos;
        g_assert(len > 2);
        neighbours = triangulation->voronoi + pos;
        ni = find_neighbour(neighbours, len-1, UNDEF);
        g_assert(ni != UNDEF);
        for (i = len-1; i > ni+1; i--)
            neighbours[i] = neighbours[i-1];
        neighbours[i] = UNDEF;
    }

    /* Continuing the first pass for the boundary points. */
    remaining = g_new(guint, blen);
    for (j = 0; j < blen; j++) {
        i = triangulation->boundary[j];
        next = triangulation->boundary[(j + 1) % blen];
        a = get_point(points, point_size, i);
        b = get_point(points, point_size, next);
        /* The point in infinity is in fact somewhere far away from the
         * centre of a-b line in the direction of the outer normal. */
        pt.x = b->y - a->y;
        pt.y = a->x - b->x;
        h = far_away/hypot(pt.x, pt.y);
        pt.x = h*pt.x + 0.5*(a->x + b->x);
        pt.y = h*pt.y + 0.5*(a->y + b->y);
        /* Add a new Voronoi point and make a and b its neighbours. */
        if (G_UNLIKELY(n - triangulation->npoints == nvpoints)) {
            g_free(remaining);
            return FALSE;
        }
        vpoints[n - triangulation->npoints] = pt;
        vindex[n] = vpos;
        /* The neighbours need to be in the reverse order because we look from
         * the outside (infinity) now. */
        voronoi[vpos + 1] = next;
        voronoi[vpos + 3] = i;
        vpos += 5;   /* Make space for the Voronoi neighbours. */
        /* Conversely, add it to the neighbourhood of b and remember it for
         * adding to the neighbourhood of a.  We only know the position when
         * we have a point and the previous one so we would need another.  To
         * preserve mental sanity, just remember the id now and add it later.
         */
        if (!add_point_id(triangulation, next, i, voronoi + vindex[next], n)) {
            g_free(remaining);
            return FALSE;
        }
        remaining[j] = n;
        n++;
    }

    if (G_UNLIKELY(vpos != nvoronoi
                   || n != triangulation->npoints + nvpoints)) {
        g_free(remaining);
        return FALSE;
    }
    vindex[n] = vpos;

    for (j = 0; j < blen; j++) {
        i = triangulation->boundary[j];
        ni = find_neighbour(voronoi + vindex[i], vindex[i+1] - vindex[i],
                            UNDEF);
        g_assert(ni != UNDEF);
        voronoi[vindex[i] + ni] = remaining[j];
    }
    g_free(remaining);

    /* Now we have created all the Voronoi points so we can add Voronoi
     * neighbours of Voronoi points. */
    for (i = triangulation->npoints;
         i < triangulation->npoints + nvpoints;
         i++) {
        vpos = vindex[i];
        if (vindex[i+1] - vpos == 5) {
            if (!add_common_neighbour(voronoi, vindex, i, vpos+1, vpos+3,
                                      vpos+2)
                || !add_infinity_neighbour(voronoi, vindex, i, vpos+1, vpos)
                || !add_infinity_neighbour(voronoi, vindex, i, vpos+3, vpos+4))
                return FALSE;
        }
        else {
            if (!add_common_neighbour(voronoi, vindex, i, vpos+1, vpos+3,
                                      vpos+2)
                || !add_common_neighbour(voronoi, vindex, i, vpos+3, vpos+5,
                                         vpos+4)
                || !add_common_neighbour(voronoi, vindex, i, vpos+5, vpos+1,
                                         vpos))
                return FALSE;
        }
    }

    return TRUE;
}

static void
make_triangulation_empty(Triangulation *triangulation)
{
    triangulation->npoints = 0;
    triangulation->points = NULL;
}

/**
 * gwy_triangulation_new:
 *
 * Creates a new triangulation.
 *
 * Returns: A new empty triangulation.
 *
 * Since: 2.18
 **/
GwyTriangulation*
gwy_triangulation_new(void)
{
    return g_object_newv(GWY_TYPE_TRIANGULATION, 0, NULL);
}

/**
 * gwy_triangulation_triangulate:
 * @triangulation: Triangulation.
 * @npoints: Number of points.
 * @points: Array of points.  They must be typecastable to
 *          #GwyTriangulationPointXY for triangulation and to
 *          #GwyTriangulationPointXYZ for interpolation.  However, they can be
 *          larger than that.  The actual struct size is indicated by
 *          @point_size.
 * @point_size: Size of point struct, in bytes.
 *
 * Finds Delaunay and Voronoi triangulations for a set of points in plane.
 *
 * The triangulation might not work in numerically unstable cases.  At present
 * this includes various ambiguous cases with neighbour points on straight
 * lines or circles.  Also, no points in the input set may coincide.
 *
 * It is possible to call this method successively on several different sets
 * of points to triangulate each separately.  Note that pointers in data
 * returned by methods such as gwy_triangulation_delaunay() become invalid
 * then.
 *
 * Returns: %TRUE on success, %FALSE on failure.  On failure the triangulation
 *          is empty.
 *
 * Since: 2.18
 **/
gboolean
gwy_triangulation_triangulate(GwyTriangulation *object,
                              guint npoints,
                              gconstpointer points,
                              gsize point_size)
{
    Triangulation *triangulation;
    Triangulator *triangulator;
    PointList pointlist;
    gboolean ok;

    g_return_val_if_fail(GWY_IS_TRIANGULATION(object), FALSE);
    triangulation = GWY_TRIANGULATION_GET_PRIVATE(object);
    make_triangulation_empty(triangulation);
    g_return_val_if_fail(point_size >= sizeof(Point), FALSE);

    triangulation->point_size = point_size;
    triangulation->points = points;
    build_compact_point_list(&pointlist, npoints, points, point_size);
    ok = ((triangulator = triangulate(&pointlist))
          && compactify(triangulator, &pointlist, triangulation)
          && find_boundary(triangulation, points, point_size)
          && delaunay_to_voronoi(triangulation, points, point_size));

    triangulator_free(triangulator);
    free_point_list(&pointlist);
    if (!ok)
        make_triangulation_empty(triangulation);

    return ok;
}

static inline gdouble
edist2_xyz_xy(const PointXYZ *p, const Point *q)
{
    gdouble dx = p->x - q->x, dy = p->y - q->y;

    return dx*dx + dy*dy;
}

/* If TRUE is returned, then a neighbour on the other side was found and the
 * triangle has become clockwise.  If TRUE is returned, then @opposite is
 * unchanged and the triangle is kept counter-clockwise. */
static gboolean
find_the_other_vneighbour(const Triangulation *triangulation,
                          guint from,
                          guint to,
                          guint *opposite)
{
    const Point *a, *b, *c;
    guint to_prev, from_next, pos, len;
    const guint *neighbours;

    pos = triangulation->vindex[from];
    len = triangulation->vindex[from + 1] - pos;
    neighbours = triangulation->voronoi + pos;
    to_prev = prev_neighbour(neighbours, len,
                             find_neighbour(neighbours, len, to));

    pos = triangulation->vindex[to];
    len = triangulation->vindex[to + 1] - pos;
    neighbours = triangulation->voronoi + pos;
    from_next = next_neighbour(neighbours, len,
                               find_neighbour(neighbours, len, from));

    /* Now there are some silly few-point special cases.  If @opposite is in
     * the centre of a triangle formed by @from, @to and the newly found point,
     * then we have an apparent match but it is not the point we are looking
     * for.  Check that the points really lies on the opposite side. */
    if (from_next != to_prev || from_next == *opposite)
        return FALSE;

    a = get_vpoint(triangulation, from);
    b = get_vpoint(triangulation, to);
    c = get_vpoint(triangulation, to_prev);
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
move_vtriangle_a(const Triangulation *triangulation, Triangle *vtriangle)
{
    if (find_the_other_vneighbour(triangulation, vtriangle->ib, vtriangle->ic,
                                  &vtriangle->ia)) {
        GWY_SWAP(guint, vtriangle->ib, vtriangle->ic);
        GWY_SWAP(const PointXYZ*, vtriangle->b, vtriangle->c);
        return TRUE;
    }
    return FALSE;
}

static inline gboolean
move_vtriangle_b(const Triangulation *triangulation, Triangle *vtriangle)
{
    if (find_the_other_vneighbour(triangulation, vtriangle->ic, vtriangle->ia,
                                  &vtriangle->ib)) {
        GWY_SWAP(guint, vtriangle->ic, vtriangle->ia);
        GWY_SWAP(const PointXYZ*, vtriangle->c, vtriangle->a);
        return TRUE;
    }
    return FALSE;
}

static inline gboolean
move_vtriangle_c(const Triangulation *triangulation, Triangle *vtriangle)
{
    if (find_the_other_vneighbour(triangulation, vtriangle->ia, vtriangle->ib,
                                  &vtriangle->ic)) {
        GWY_SWAP(guint, vtriangle->ia, vtriangle->ib);
        GWY_SWAP(const PointXYZ*, vtriangle->a, vtriangle->b);
        return TRUE;
    }
    return FALSE;
}

/* This assumes a counter-clockwise triangle */
static void
make_vtriangle(Triangle *triangle, const Triangulation *triangulation)
{
    /* XXX: In the triangulation algoritm, the points are in fact only XY,
     * but the Z members are never accessed so the typecast is all right. */
    triangle->a = get_vpoint_xyz(triangulation, triangle->ia);
    triangle->b = get_vpoint_xyz(triangulation, triangle->ib);
    triangle->c = get_vpoint_xyz(triangulation, triangle->ic);

    make_triangle_side(&triangle->sa, triangle->b, triangle->c, triangle->a);
    make_triangle_side(&triangle->sb, triangle->c, triangle->a, triangle->b);
    make_triangle_side(&triangle->sc, triangle->a, triangle->b, triangle->c);
}

/* Ensures @triangle contains point @pt.  A relatively quick test if it already
 * contains the point.  If the right triangle is nearby, it is also found
 * reasonably fast. */
static gboolean
ensure_vtriangle(const Triangulation *triangulation,
                 Triangle *vtriangle,
                 const Point *pt)
{
    gboolean moved;
    guint iter;

    iter = 0;
    while (!triangle_contains_point(vtriangle, pt)) {
        if (vtriangle->da <= vtriangle->db) {
            if (vtriangle->da <= vtriangle->dc)
                moved = move_vtriangle_a(triangulation, vtriangle);
            else
                moved = move_vtriangle_c(triangulation, vtriangle);
        }
        else {
            if (vtriangle->db <= vtriangle->dc)
                moved = move_vtriangle_b(triangulation, vtriangle);
            else
                moved = move_vtriangle_c(triangulation, vtriangle);
        }

        if (!moved)
            return FALSE;

        make_vtriangle(vtriangle, triangulation);
        if (G_UNLIKELY(iter++ == triangulation->nvpoints)) {
            vtriangle->ia = vtriangle->ib = vtriangle->ic = UNDEF;
            return FALSE;
        }
    }

    return TRUE;
}

/* Initializes @vtriangle to any valid triangle containing point @hint. */
static void
make_valid_vtriangle(const Triangulation *triangulation,
                     Triangle *vtriangle,
                     guint hint)
{
    const Point *a = get_vpoint(triangulation, hint);
    const Point *b, *c;
    const guint *neighbours;
    gdouble phib, phic;
    guint i, len;

    vtriangle->ia = i = hint;
    neighbours = triangulation->voronoi + triangulation->vindex[i];
    len = triangulation->vindex[i+1] - triangulation->vindex[i];
    for (i = 0; i < len; i++) {
        vtriangle->ib = neighbours[i];
        vtriangle->ic = next_neighbour(neighbours, len, i);

        b = get_vpoint(triangulation, vtriangle->ib);
        phib = atan2(b->y - a->y, b->x - a->x);
        c = get_vpoint(triangulation, vtriangle->ic);
        phic = atan2(c->y - a->y, c->x - a->x);

        if (ccw_angle_convex(phib, phic)) {
            make_vtriangle(vtriangle, triangulation);
            return;
        }
    }

    g_assert_not_reached();
}

static gboolean
interpolate_round(Triangulation *triangulation,
                  Triangle *vtriangle,
                  const Point *pt,
                  gdouble *value)
{
    const PointXYZ *p = NULL;

    ensure_vtriangle(triangulation, vtriangle, pt);
    if (G_UNLIKELY(vtriangle->ia == UNDEF)) {
        *value = 0.0;
        return FALSE;
    }

    if (vtriangle->ia < triangulation->npoints)
        p = get_point_xyz(triangulation->points, triangulation->point_size,
                          vtriangle->ia);
    else if (vtriangle->ib < triangulation->npoints)
        p = get_point_xyz(triangulation->points, triangulation->point_size,
                          vtriangle->ib);
    else if (vtriangle->ic < triangulation->npoints)
        p = get_point_xyz(triangulation->points, triangulation->point_size,
                          vtriangle->ic);

    if (p) {
        *value = p->z;
        return TRUE;
    }
    else {
        *value = 0.0;
        return FALSE;
    }
}

static inline gdouble
tinterpolate_linear(const Triangle *triangle)
{
    gdouble wsum = triangle->da + triangle->db + triangle->dc;

    return (triangle->da*triangle->a->z + triangle->db*triangle->b->z
            + triangle->dc*triangle->c->z)/wsum;
}

static inline gdouble
sinterpolate1_linear(gconstpointer points, gsize point_size,
                     guint ia, guint ib,
                     const Point *pt)
{
    const PointXYZ *a = get_point_xyz(points, point_size, ia);
    const PointXYZ *b = get_point_xyz(points, point_size, ib);
    gdouble d = side_intersection_distance(a, b, pt);

    if (d <= -1.0)
        return a->z;
    else if (d >= 1.0)
        return b->z;

    return 0.5*((d + 1.0)*b->z + (1.0 - d)*a->z);
}

static inline gdouble
sinterpolate_linear(const Triangulation *triangulation,
                    const Triangle *triangle, const Point *pt)
{
    guint ia, ib;

    ia = triangle->ia;
    ib = triangle->ib;
    if (find_nearest_side(triangulation, &ia, &ib, pt))
        goto success;

    ia = triangle->ib;
    ib = triangle->ic;
    if (find_nearest_side(triangulation, &ia, &ib, pt))
        goto success;

    ia = triangle->ic;
    ib = triangle->ia;
    if (find_nearest_side(triangulation, &ia, &ib, pt))
        goto success;

    g_assert_not_reached();
    return 0.0;

success:
    return sinterpolate1_linear(triangulation->points,
                                triangulation->point_size,
                                ia, ib, pt);
}

static gboolean
interpolate_linear(Triangulation *triangulation,
                   Triangle *triangle,
                   const Point *pt,
                   gdouble *value)
{
    if (ensure_triangle(triangulation, triangle, pt))
        *value = tinterpolate_linear(triangle);
    else {
        if (G_UNLIKELY(triangle->ia == UNDEF))
            return FALSE;
        *value = sinterpolate_linear(triangulation, triangle, pt);
    }
    return TRUE;
}

/**
 * gwy_triangulation_interpolate:
 * @triangulation: Triangulation.
 * @interpolation: Interpolation to use.  Only @GWY_INTERPOLATION_ROUND and
 *                 @GWY_INTERPOLATION_LINEAR are implemented.  Is is an error
 *                 to pass any other interpolation type.
 * @dfield: Data field to fill with interpolated values.
 *
 * Regularizes XYZ data to a grid, represented by a data field.
 *
 * The area and resolution of the regular grid is given by the dimensions and
 * offsets of @dfield.
 *
 * Returns: %TRUE if the interpolation succeeds, %FALSE on failure, e.g. due to
 *          numerical errors.  In the latter case the contents of @dfield is
 *          undefined.
 *
 * Since: 2.18
 **/
gboolean
gwy_triangulation_interpolate(GwyTriangulation *object,
                              GwyInterpolationType interpolation,
                              GwyDataField *dfield)
{
    Triangulation *triangulation;
    guint xres, yres, i, j;
    gdouble qx, qy, xoff, yoff;
    gdouble *d;
    Triangle triangle;
    gboolean ok = FALSE;
    Point pt;

    g_return_val_if_fail(GWY_IS_TRIANGULATION(object), FALSE);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);
    triangulation = GWY_TRIANGULATION_GET_PRIVATE(object);
    g_return_val_if_fail(triangulation->point_size >= sizeof(PointXYZ), FALSE);
    g_return_val_if_fail(interpolation == GWY_INTERPOLATION_LINEAR
                         || interpolation == GWY_INTERPOLATION_ROUND, FALSE);

    if (interpolation == GWY_INTERPOLATION_LINEAR)
        make_valid_triangle(triangulation->neighbours, triangulation->index[1],
                            triangulation->points, triangulation->point_size,
                            &triangle, 0);
    else
        make_valid_vtriangle(triangulation, &triangle, 0);

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
            if (interpolation == GWY_INTERPOLATION_LINEAR)
                ok = interpolate_linear(triangulation, &triangle, &pt, d);
            else
                ok = interpolate_round(triangulation, &triangle, &pt, d);
            if (!ok)
                goto fail;
            d++;
        }
    }
    ok = TRUE;

fail:
    gwy_data_field_invalidate(dfield);

    return ok;
}

/**
 * gwy_triangulation_data_free:
 * @triangulation_data: Raw triangulation data.
 *
 * Frees raw triangulation data.
 *
 * This function should be used to free triangulation data returned by
 * gwy_triangulation_delaunay() and similar.  It does not free the array
 * members as they are owned by the triangulation object.
 *
 * Since: 2.18
 **/
void
gwy_triangulation_data_free(GwyTriangulationData *triangulation_data)
{
    g_free(triangulation_data);
    /* The rest is owned by the object. */
}

/**
 * gwy_triangulation_delaunay:
 * @triangulation: Triangulation.
 *
 * Obtains the Delaunay triangulation data.
 *
 * Notes to the fields in the returned struct:
 *
 * @npoints equals to the number of points passed to
 * gwy_triangulation_triangulate().
 *
 * Returns: Newly clreated #GwyTriangulationData that must be freed with
 *          gwy_triangulation_data_free() when no longer used.  The data within
 *          is owned by @triangulation, see #GwyTriangulationData.
 *
 * Since: 2.18
 **/
GwyTriangulationData*
gwy_triangulation_delaunay(GwyTriangulation *object)
{
    Triangulation *triangulation;
    GwyTriangulationData *data = NULL;

    g_return_val_if_fail(GWY_IS_TRIANGULATION(object), NULL);
    triangulation = GWY_TRIANGULATION_GET_PRIVATE(object);
    if (triangulation->npoints) {
        data = g_new(GwyTriangulationData, 1);
        data->npoints = triangulation->npoints;
        data->size = triangulation->nsize;
        data->index = triangulation->index;
        data->neighbours = triangulation->neighbours;
    }

    return data;
}

/**
 * gwy_triangulation_boundary:
 * @triangulation: Triangulation.
 *
 * Obtains the boundary, i.e. convex hull, of Delaunay triangulation.
 *
 * Notes to the fields in the returned struct:
 *
 * @npoints equals to the number of points passed to
 * gwy_triangulation_triangulate().
 *
 * @size is the boundary length.
 *
 * @index[] contains point indices in the boundary for points on the boundary;
 * and %GWY_TRIANGULATION_NONE for points not on the boundary.
 *
 * @neighbours[] lists sequentially the boundary points.
 *
 * Returns: Newly clreated #GwyTriangulationData that must be freed with
 *          gwy_triangulation_data_free() when no longer used.  The data within
 *          is owned by @triangulation, see #GwyTriangulationData.
 *
 * Since: 2.18
 **/
GwyTriangulationData*
gwy_triangulation_boundary(GwyTriangulation *object)
{
    Triangulation *triangulation;
    GwyTriangulationData *data = NULL;

    g_return_val_if_fail(GWY_IS_TRIANGULATION(object), NULL);
    triangulation = GWY_TRIANGULATION_GET_PRIVATE(object);
    if (triangulation->npoints) {
        data = g_new(GwyTriangulationData, 1);
        data->npoints = triangulation->npoints;
        data->size = triangulation->blen;
        data->index = triangulation->bindex;
        data->neighbours = triangulation->boundary;
    }

    return data;
}

/**
 * gwy_triangulation_voronoi:
 * @triangulation: Triangulation.
 * @nvpoints: Location to store the number of new Voronoi triangulation points,
 *            or %NULL.
 * @vpoints: Location to store pointer to the Voronoi triangulation points,
 *           or %NULL.
 *
 * Obtains the Voronoi triangulation data.
 *
 * Notes to the fields in the returned struct:
 *
 * @npoints equals to the number of Delaunay triangulation points passed to
 * gwy_triangulation_triangulate() plus the number of points in the Voronoi
 * triangulation, @nvpoints.
 *
 * @index[] is the usual index of blocks in @neighbours, however,
 * point indices smaller than the number of Delaunay points correspond to the
 * Delaunay points, point indices equal or larger correspond to points in
 * @vpoints (it is necessary to subtract the number of original points to
 * obtain the real position in @vpoints).
 *
 * @neighbours[] contains the neighbour blocks, with above caveats about
 * point numbering.
 *
 * Returns: Newly clreated #GwyTriangulationData that must be freed with
 *          gwy_triangulation_data_free() when no longer used.  The data within
 *          is owned by @triangulation, see #GwyTriangulationData.
 *
 * Since: 2.18
 **/
GwyTriangulationData*
gwy_triangulation_voronoi(GwyTriangulation *object,
                          guint *nvpoints,
                          const GwyTriangulationPointXY **vpoints)
{
    Triangulation *triangulation;
    GwyTriangulationData *data = NULL;

    g_return_val_if_fail(GWY_IS_TRIANGULATION(object), NULL);
    triangulation = GWY_TRIANGULATION_GET_PRIVATE(object);
    if (triangulation->npoints) {
        data = g_new(GwyTriangulationData, 1);
        /* This is the size of index[] which is the sum of original and
         * Voronoi triangulation points. */
        data->npoints = triangulation->nvpoints + triangulation->npoints;
        data->size = triangulation->nvoronoi;
        data->index = triangulation->vindex;
        data->neighbours = triangulation->voronoi;
        if (nvpoints)
            *nvpoints = triangulation->nvpoints;
        if (vpoints)
            *vpoints = triangulation->vpoints;
    }
    else {
        if (nvpoints)
            *nvpoints = 0;
        if (vpoints)
            *vpoints = NULL;
    }

    return data;
}

#ifdef DEBUG
G_GNUC_UNUSED
static void
dump_neighbours(const Triangulator *triangulator)
{
    guint i, j;

    for (i = 0; i < triangulator->nlen; i++) {
        for (j = 0; j < triangulator->npoints; j++) {
            if (triangulator->blocks[j].pos == i)
                g_print("(%u)", j);
        }

        if (triangulator->neighbours[i] == UNDEF)
            g_print(".");
        else
            g_print("%u", triangulator->neighbours[i]);
        g_print(" ");
    }
    g_print("\n");
}

G_GNUC_UNUSED
static void
dump_triangulator(const Triangulator *triangulator)
{
    NeighbourBlock *nb;
    guint i, j;

    for (i = 0; i < triangulator->npoints; i++) {
        nb = triangulator->blocks + i;
        g_print("%u:", i);
        for (j = 0; j < nb->len; j++)
            g_print(" %u", triangulator->neighbours[nb->pos + j]);
        g_print("\n");
    }
}

G_GNUC_UNUSED
static void
dump_points_(const Triangulator *triangulator,
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
    for (i = 0; i < triangulator->npoints; i++) {
        nb = triangulator->blocks + i;
        neighbours = triangulator->neighbours + nb->pos;
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
static void
dump_points(const Triangulation *triangulation,
            gconstpointer points, gsize point_size)
{
    guint i, j, ni, pos, len;
    const guint *neighbours;
    FILE *fh;

    fh = fopen("points.dat", "w");
    for (i = 0; i < triangulation->npoints; i++) {
        const Point *pt = get_point(points, point_size, i);
        fprintf(fh, "%u %g %g\n", i, pt->x, pt->y);
    }
    fclose(fh);

    fh = fopen("arrows.gpi", "w");
    for (i = 0; i < triangulation->npoints; i++) {
        pos = triangulation->index[i];
        len = triangulation->index[i+1] - pos;
        neighbours = triangulation->neighbours + pos;
        for (j = 0; j < len; j++) {
            ni = neighbours[j];
            if (ni > i) {
                const Point *pt1 = get_point(points, point_size, i);
                const Point *pt2 = get_point(points, point_size, ni);
                fprintf(fh, "set arrow from %g,%g to %g,%g nohead ls 2\n",
                        pt1->x, pt1->y, pt2->x, pt2->y);
            }
        }
    }
    for (j = 0; j < triangulation->blen; j++) {
        i = triangulation->boundary[j];
        ni = triangulation->boundary[(j + 1) % triangulation->blen];
        {
            const Point *pt1 = get_point(points, point_size, i);
            const Point *pt2 = get_point(points, point_size, ni);
            fprintf(fh, "set arrow from %g,%g to %g,%g nohead ls 6\n",
                    pt1->x, pt1->y, pt2->x, pt2->y);
        }
    }
    fclose(fh);
}

G_GNUC_UNUSED
static void
dump_voronoi(const Triangulation *triangulation,
             gconstpointer points, gsize point_size)
{
    guint i, j, ni, pos, len, npts;
    const guint *neighbours;
    FILE *fh;

    npts = triangulation->npoints;

    fh = fopen("vpoints.dat", "w");
    for (i = 0; i < triangulation->nvpoints; i++) {
        const Point *pt = triangulation->vpoints + i;
        fprintf(fh, "%u %g %g\n", i, pt->x, pt->y);
    }
    fclose(fh);

    fh = fopen("varrows.gpi", "w");
    for (i = 0; i < triangulation->nvpoints; i++) {
        pos = triangulation->vindex[i + npts];
        len = triangulation->vindex[i+1 + npts] - pos;
        neighbours = triangulation->voronoi + pos;
        for (j = 0; j < len; j++) {
            ni = neighbours[j];
            if (ni < npts) {
                const Point *pt1 = triangulation->vpoints + i;
                const Point *pt2 = get_point(points, point_size, ni);
                fprintf(fh, "set arrow from %g,%g to %g,%g nohead ls 5\n",
                        pt1->x, pt1->y, pt2->x, pt2->y);
            }
            else {
                ni -= npts;
                if (ni > i) {
                    const Point *pt1 = triangulation->vpoints + i;
                    const Point *pt2 = triangulation->vpoints + ni;
                    fprintf(fh, "set arrow from %g,%g to %g,%g nohead ls 4\n",
                            pt1->x, pt1->y, pt2->x, pt2->y);
                }
            }
        }
    }
    fclose(fh);
}

G_GNUC_UNUSED
static guint
test_reflexivity(const Triangulator *triangulator)
{
    NeighbourBlock *nb, *nbn;
    guint i, j, count;
    const guint *neighbours, *neighboursn;

    count = 0;
    for (i = 0; i < triangulator->npoints; i++) {
        nb = triangulator->blocks + i;
        neighbours = triangulator->neighbours + nb->pos;
        for (j = 0; j < nb->len; j++) {
            nbn = triangulator->blocks + neighbours[j];
            neighboursn = triangulator->neighbours + nb->pos;
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
 * SECTION:triangulation
 * @title: triangulation
 * @short_description: Delaunay and Voronoi triangulation and interpolation
 **/

/**
 * GWY_TRIANGULATION_NONE:
 *
 * Point index value representing no point.
 *
 * Since: 2.18
 **/

/**
 * GwyTriangulationPointXY:
 * @x: X-coordinate.
 * @y: Y-coordinate.
 *
 * Representation of a point in plane for triangulation.
 *
 * Since: 2.18
 **/

/**
 * GwyTriangulationPointXYZ:
 * @x: X-coordinate.
 * @y: Y-coordinate.
 * @z: Z-coordinate, i.e. the value in point (@x,@y).
 *
 * Representation of a point in plane with associated value for interpolation.
 *
 * Since: 2.18
 **/

/**
 * GwyTriangulationData:
 * @npoints: Number of points in the set, also detrmines the size of @index.
 * @size: The length of @neighbours.
 * @index: Array of size @npoints+1 defining the blocks of neighbours in
 *         @neighbours.  The block for point @i starts at @index[@i] and ends
 *         one element before @index[@i+1].  Hence the last of @index is equal
 *         to @size.
 * @neighbours: Neighbours of each point, represented as indices into some
 *              array (which array, that depends on what kind of data it is).
 *              The points in each block are sorted counter-clockwise.
 *
 * Representation of raw triangulation data.
 *
 * Members @index and @neighbours are owned by the #GwyTriangulation object
 * that provided this data and remain valid only until this object is destroyed
 * or used to perform another triangulation.
 *
 * The exact interpretation of individual parts depends on what kind of
 * triangulation data it is and may differ a bit from the general description
 * provided here.  See the descriptions of individual methods returning
 * #GwyTriangulationData.
 *
 * Since: 2.18
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
