/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

/**
 * [FILE-MAGIC-USERGUIDE]
 * Raw XYZ data
 * .xyz .dat
 * Read[1]
 * [1] XYZ data are interpolated to a regular grid upon import.
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/delaunay.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define EPSREL 1e-8

/* Use smaller cell sides than the triangulation algorithm as we only need them
 * for identical point detection and border extension. */
#define CELL_SIDE 1.6

enum {
    UNDEF = G_MAXUINT
};

typedef struct {
    /* XXX: Not all values of interpolation and exterior are possible. */
    GwyInterpolationType interpolation;
    GwyExteriorType exterion;
    gchar *xy_units;
    gchar *z_units;
    /* Interface only */
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
} RawXYZArgs;

typedef struct {
    GArray *points;
    guint norigpoints;
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
    gdouble zmin;
    gdouble zmax;
} RawXYZFile;

typedef struct {
    RawXYZArgs *args;
    GtkWidget *dialog;
} RawXYZControls;

typedef struct {
    guint *id;
    guint pos;
    guint len;
    guint size;
} WorkQueue;

static gboolean      module_register(void);
static GwyContainer* rawxyz_load    (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static void          rawxyz_free    (RawXYZFile *rfile);
static GArray*       read_points    (gchar *p);
static void          analyse_points (RawXYZFile *rfile,
                                     double epsrel);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports raw XYZ files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("rawxyz",
                           N_("Raw XYZ files"),
                           NULL,
                           (GwyFileLoadFunc)&rawxyz_load,
                           NULL,
                           NULL);

    return TRUE;
}

static GwyContainer*
rawxyz_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    RawXYZFile rfile;
    gchar *buffer = NULL;
    gsize size;
    GError *err = NULL;

    gwy_clear(&rfile, 1);

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    rfile.points = read_points(buffer);
    g_free(buffer);
    if (!rfile.points->len) {
        err_NO_DATA(error);
        goto fail;
    }

    analyse_points(&rfile, EPSREL);

fail:
    rawxyz_free(&rfile);

    return container;
}

static void
rawxyz_free(RawXYZFile *rfile)
{
    g_array_free(rfile->points, TRUE);
}

static GArray*
read_points(gchar *p)
{
    GArray *points;
    gchar *line, *end;

    points = g_array_new(FALSE, FALSE, sizeof(GwyDelaunayPointXYZ));
    for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
        GwyDelaunayPointXYZ pt;

        if (!line[0] || line[0] == '#')
            continue;

        if (!(pt.x = g_ascii_strtod(line, &end)) && end == line)
            continue;
        line = end;
        if (!(pt.y = g_ascii_strtod(line, &end)) && end == line)
            continue;
        line = end;
        if (!(pt.z = g_ascii_strtod(line, &end)) && end == line)
            continue;

        g_array_append_val(points, pt);
    }

    return points;
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

static void
work_queue_init(WorkQueue *queue)
{
    queue->size = 64;
    queue->len = 0;
    queue->id = g_new(guint, queue->size);
}

static void
work_queue_destroy(WorkQueue *queue)
{
    g_free(queue->id);
}

static void
work_queue_add(WorkQueue *queue,
               guint id)
{
    if (G_UNLIKELY(queue->len == queue->size)) {
        queue->size *= 2;
        queue->id = g_renew(guint, queue->id, queue->size);
    }
    queue->id[queue->len] = id;
    queue->len++;
}

static void
work_queue_ensure(WorkQueue *queue,
                  guint id)
{
    guint i;

    for (i = 0; i < queue->len; i++) {
        if (queue->id[i] == i)
            return;
    }
    work_queue_add(queue, id);
}

static inline gdouble
point_dist2(const GwyDelaunayPointXYZ *p,
            const GwyDelaunayPointXYZ *q)
{
    gdouble dx = p->x - q->x;
    gdouble dy = p->y - q->y;

    return dx*dx + dy*dy;
}

static gboolean
maybe_add_point(WorkQueue *pointqueue,
                const GwyDelaunayPointXYZ *newpoints,
                guint id,
                gdouble eps2)
{
    guint i;

    for (i = 0; i < pointqueue->pos; i++) {
        if (point_dist2(newpoints + id, newpoints + pointqueue->id[i]) < eps2) {
            GWY_SWAP(guint, pointqueue->id[i], pointqueue->id[pointqueue->pos]);
            pointqueue->pos++;
            return TRUE;
        }
    }
    return FALSE;
}

/* Calculate coordinate ranges and ensure points are more than epsrel*cellside
 * appart where cellside is the side of equivalent-area square for one point. */
static void
analyse_points(RawXYZFile *rfile,
               double epsrel)
{
    WorkQueue cellqueue, pointqueue;
    GwyDelaunayPointXYZ *points, *newpoints, *pt;
    gdouble xreal, yreal, eps, eps2, xr, yr, step;
    guint npoints, i, j, ig, xres, yres, ncells, oldpos;
    guint *cell_index;

    /* Calculate data ranges */
    npoints = rfile->points->len;
    points = (GwyDelaunayPointXYZ*)rfile->points->data;
    rfile->xmin = rfile->xmax = points[0].x;
    rfile->ymin = rfile->ymax = points[0].y;
    rfile->zmin = rfile->zmax = points[0].z;
    for (i = 0; i < npoints; i++) {
        pt = points + i;

        if (pt->x < rfile->xmin)
            rfile->xmin = pt->x;
        else if (pt->x > rfile->xmax)
            rfile->xmax = pt->x;

        if (pt->y < rfile->ymin)
            rfile->ymin = pt->y;
        else if (pt->y > rfile->ymax)
            rfile->ymax = pt->y;

        if (pt->z < rfile->zmin)
            rfile->zmin = pt->z;
        else if (pt->z > rfile->zmax)
            rfile->zmax = pt->z;
    }

    xreal = rfile->xmax - rfile->xmin;
    yreal = rfile->ymax - rfile->ymin;

    /* Make a virtual grid */
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
    eps = epsrel*step;
    eps2 = eps*eps;

    ncells = xres*yres;
    cell_index = g_new0(guint, ncells + 1);

    for (i = 0; i < npoints; i++) {
        pt = points + i;
        ig = coords_to_grid_index(xres, yres, step,
                                  pt->x - rfile->xmin, pt->y - rfile->ymin);
        cell_index[ig]++;
    }

    index_accumulate(cell_index, xres*yres);
    index_rewind(cell_index, xres*yres);
    newpoints = g_new(GwyDelaunayPointXYZ, npoints);

    /* Sort points by cell */
    for (i = 0; i < npoints; i++) {
        pt = points + i;
        ig = coords_to_grid_index(xres, yres, step,
                                  pt->x - rfile->xmin, pt->y - rfile->ymin);
        newpoints[cell_index[ig]] = *pt;
        cell_index[ig]++;
    }

    /* Find groups of identical (i.e. closer than epsrel) points we need to
     * merge.  We collapse all merged points to that with the lowest id.
     * Closeness must be transitive so the group must be gathered iteratively
     * until it no longer grows. */
    work_queue_init(&pointqueue);
    work_queue_init(&cellqueue);
    g_array_set_size(rfile->points, 0);
    for (i = 0; i < npoints; i++) {
        /* Ignore merged points */
        if (newpoints[i].z == G_MAXDOUBLE)
            continue;

        pointqueue.len = 0;
        cellqueue.len = 0;
        work_queue_add(&pointqueue, i);
        pointqueue.pos = 1;
        oldpos = 0;

        do {
            /* Update the list of cells to process.  Most of the time this is
             * no-op. */
            while (oldpos < pointqueue.pos) {
                gdouble x, y;
                guint ix, iy;

                pt = newpoints + pointqueue.id[oldpos];
                x = (pt->x - rfile->xmin)/step;
                ix = (guint)floor(x);
                x -= ix;
                y = (pt->y - rfile->ymin)/step;
                ix = (guint)floor(y);
                y -= ix;

                if (ix < xres && iy < yres)
                    work_queue_ensure(&cellqueue, iy*xres + ix);
                if (ix > 0 && iy < yres && x <= eps)
                    work_queue_ensure(&cellqueue, iy*xres + ix-1);
                if (ix < xres && iy > 0 && y <= eps)
                    work_queue_ensure(&cellqueue, (iy - 1)*xres + ix);
                if (ix > 0 && iy > 0 && x < eps && y <= eps)
                    work_queue_ensure(&cellqueue, (iy - 1)*xres + ix-1);
                if (ix+1 < xres && iy < xres && 1-x <= eps)
                    work_queue_ensure(&cellqueue, iy*xres + ix+1);
                if (ix < xres && iy+1 < xres && 1-y <= eps)
                    work_queue_ensure(&cellqueue, (iy + 1)*xres + ix);
                if (ix+1 < xres && iy+1 < xres && 1-x <= eps && 1-y <= eps)
                    work_queue_ensure(&cellqueue, (iy + 1)*xres + ix+1);

                oldpos++;
            }

            /* Process all points from the cells and check if they belong to
             * the currently merged group. */
            while (cellqueue.pos < cellqueue.len) {
                for (i = cell_index[j]; i < cell_index[j+1]; i++) {
                    if (newpoints[i].z != G_MAXDOUBLE)
                        work_queue_add(&pointqueue, i);
                }
                cellqueue.pos++;
            }

            /* Compare all not-in-group points with all group points, adding
             * them to the group on success. */
            for (i = pointqueue.pos; i < pointqueue.len; i++)
                maybe_add_point(&pointqueue, newpoints, i, eps2);
        } while (oldpos != pointqueue.pos);

        /* Calculate the representant of all contributing points. */
        {
            GwyDelaunayPointXYZ avg = { 0.0, 0.0, 0.0 };

            for (i = 0; i < pointqueue.pos; i++) {
                pt = newpoints + pointqueue.id[i];
                avg.x += pt->x;
                avg.y += pt->y;
                avg.z += pt->z;
                pt->z = G_MAXDOUBLE;
            }
            avg.x /= pointqueue.pos;
            avg.y /= pointqueue.pos;
            avg.z /= pointqueue.pos;
            g_array_append_val(rfile->points, avg);
        }
    }

    work_queue_destroy(&cellqueue);
    work_queue_destroy(&pointqueue);

    g_free(cell_index);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
