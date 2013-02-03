/*
 *  $Id: gxyzf.c 14315 2012-11-18 14:25:33Z yeti-dn $
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-USERGUIDE]
 * GwyXYZ data
 * .gxyzf .dat
 * Read[1]
 * [1] GwyXYZ data are interpolated to a regular grid upon import.
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libprocess/triangulation.h>
#include <libdraw/gwypixfield.h>
#include <libdraw/gwygradient.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>
#include <app/settings.h>

#include "err.h"

#define EPSREL 1e-7

/* Use smaller cell sides than the triangulation algorithm as we only need them
 * for identical point detection and border extension. */
#define CELL_SIDE 1.6

#define MAGIC "Gwyddion XYZ Field 1.0\n"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION ".gxyzf"

enum {
    PREVIEW_SIZE = 240,
    UNDEF = G_MAXUINT
};

enum {
    GWY_INTERPOLATION_FIELD = -1,
};

typedef enum {
    GXYZF_IRREGULAR = 0,
    GXYZF_REGULAR_X = 1,   /* X is fast axis */
    GXYZF_REGULAR_Y = 2,   /* Y is fast axis */
} GxyzfRegularType;

typedef struct {
    /* XXX: Not all values of interpolation and exterior are possible. */
    GwyInterpolationType interpolation;
    GwyExteriorType exterior;
    gchar *xy_units;
    gchar *z_units;
    gint xres;
    gint yres;
    gboolean xydimeq;
    gboolean xymeasureeq;
    /* Interface only */
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
} GxyzfArgs;

typedef struct {
    GArray *points;
    guint norigpoints;
    guint nbasepoints;
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
    gdouble step;
    gdouble zmin;
    gdouble zmax;
    GxyzfRegularType regular;
    guint regular_xres;
    guint regular_yres;
    gdouble xstep;
    gdouble ystep;
} GxyzfFile;

typedef struct {
    GxyzfArgs *args;
    GxyzfFile *rfile;
    GwyGradient *gradient;
    gboolean in_update;
} GxyzfControls;

typedef struct {
    guint *id;
    guint pos;
    guint len;
    guint size;
} WorkQueue;

static gboolean      module_register        (void);
static gint          gxyzf_detect          (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer* gxyzf_load            (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static GwyDataField* gxyzf_do              (GxyzfFile *rfile,
                                             const GxyzfArgs *args,
                                             GError **error);
static void          fill_field_x           (const GwyTriangulationPointXYZ *points,
                                             GwyDataField *dfield);
static void          fill_field_y           (const GwyTriangulationPointXYZ *points,
                                             GwyDataField *dfield);
static void          interpolate_field      (guint npoints,
                                             const GwyTriangulationPointXYZ *points,
                                             GwyDataField *dfield);
static void          extend_borders         (GxyzfFile *rfile,
                                             const GxyzfArgs *args,
                                             gdouble epsrel);
static void          gxyzf_free            (GxyzfFile *rfile);
static GArray*       read_points            (const guchar *p, gsize size);
static void          initialize_ranges      (const GxyzfFile *rfile,
                                             GxyzfArgs *args);
static void          analyse_points         (GxyzfFile *rfile,
                                             double epsrel);
static gboolean      check_regular_grid     (GxyzfFile *rfile);
static void          gxyzf_load_args       (GwyContainer *container,
                                             GxyzfArgs *args);
static void          gxyzf_save_args       (GwyContainer *container,
                                             GxyzfArgs *args);

static const GxyzfArgs gxyzf_defaults = {
    GWY_INTERPOLATION_LINEAR, GWY_EXTERIOR_MIRROR_EXTEND,
    NULL, NULL,
    500, 500,
    TRUE, TRUE,
    /* Interface only */
    0.0, 0.0, 0.0, 0.0
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports raw XYZ data files."),
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("gxyzf",
                           N_("GwyXYZ data files"),
                           (GwyFileDetectFunc)&gxyzf_detect,
                           (GwyFileLoadFunc)&gxyzf_load,
                           NULL,
                           NULL);
    /* We provide a detection function, but the loading method tries a bit
     * harder, so let the user choose explicitly. */
    gwy_file_func_set_is_detectable("gxyzf", TRUE);

    return TRUE;
}

static gint
gxyzf_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 100;
}

static guint
read_pixel_size(GHashTable *hash,
                const gchar *key,
                GError **error)
{
    gchar *value;
    guint size;

    if (!(value = g_hash_table_lookup(hash, key))) {
        err_MISSING_FIELD(error, key);
        return 0;
    }
    size = atoi(g_hash_table_lookup(hash, key));
    if (err_DIMENSION(error, size))
        return 0;

    return size;
}


static GwyContainer*
gxyzf_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *settings, *container = NULL;
    GwyDataField *dfield;
    GwyTextHeaderParser parser;
    GHashTable *hash = NULL;
    GxyzfArgs args;
    GxyzfFile rfile;
    guchar *p, *value, *buffer = NULL, *header = NULL;
    gsize size;
    GError *err = NULL;
    const guchar *datap;

    gboolean ok;
    GwySIUnit **units;
    GwySIUnit *unit;
    gint nchan, i;

    /* Someday we can load XYZ data with default settings */
    if (mode != GWY_RUN_INTERACTIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_INTERACTIVE,
                    _("Raw XYZ data import must be run as interactive."));
        return NULL;
    }

    gwy_clear(&rfile, 1);

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    if (size < MAGIC_SIZE || memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
                err_FILE_TYPE(error, "Gwyddion Simple Field");
                        goto fail;
                            }
/*introduced from gsf*/
    p = buffer + MAGIC_SIZE;
    datap = memchr(p, '\0', size - (p - buffer));
    if (!datap) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated."));
        printf("ojoj\n");
        goto fail;
    }
    header = g_strdup(p);
    datap += 4 - ((datap - buffer) % 4);

    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    if (!(hash = gwy_text_header_parse(header, &parser, NULL, NULL))) {
        g_propagate_error(error, err);
        printf("ojojojojoj\n");
        goto fail;
    }

    nchan = read_pixel_size(hash, "NChannels", error);
    units = (GwySIUnit **)g_malloc(nchan*sizeof(GwySIUnit*));

    value = g_hash_table_lookup(hash, "XYUnits");
    unit = gwy_si_unit_new(value);

    printf("xyunit %s\n", gwy_si_unit_get_string(unit, GWY_SI_UNIT_FORMAT_PLAIN));

    for (i=0; i<nchan; i++) {
       value = g_hash_table_lookup(hash, "ZUnits");
       units[i] = gwy_si_unit_new(value);

       printf("zunit %d: %s\n", i,  gwy_si_unit_get_string(units[i], GWY_SI_UNIT_FORMAT_PLAIN));
    }

    printf("header size %d, total size %d\n", datap-buffer, size);

    rfile.points = read_points(datap, (size - (datap-buffer))/8);
    g_free(buffer);
    if (!rfile.points->len) {
        err_NO_DATA(error);
        goto fail;
    }

    settings = gwy_app_settings_get();
    gxyzf_load_args(settings, &args);
    analyse_points(&rfile, EPSREL);
    initialize_ranges(&rfile, &args);

    dfield = gxyzf_do(&rfile, &args, error);
    if (!dfield) printf("something failed, no dfield\n");

    
    gxyzf_save_args(settings, &args);
    //if (!ok) {
    //    err_CANCELLED(error);
    //    goto fail;
   // }

    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup("Regularized XYZ"));
    }
    
fail:
    //gxyzf_free(&rfile);

    return container;

}

static GwyDataField*
gxyzf_do(GxyzfFile *rfile,
          const GxyzfArgs *args,
          GError **error)
{
    GArray *points = rfile->points;
    GwySIUnit *unitxy, *unitz;
    GwyDataField *dfield;
    gint xypow10, zpow10, xres, yres;
    gboolean ok = TRUE;
    gdouble mag;

    xres = ((rfile->regular == GXYZF_IRREGULAR)
            ? args->xres : rfile->regular_xres);
    yres = ((rfile->regular == GXYZF_IRREGULAR)
            ? args->yres : rfile->regular_yres);

    unitxy = gwy_si_unit_new_parse(args->xy_units, &xypow10);
    mag = pow10(xypow10);
    unitz = gwy_si_unit_new_parse(args->z_units, &zpow10);
    dfield = gwy_data_field_new(xres, yres,
                                args->xmax - args->xmin,
                                args->ymax - args->ymin,
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, unitxy);
    gwy_data_field_set_si_unit_z(dfield, unitz);
    gwy_data_field_set_xoffset(dfield, args->xmin);
    gwy_data_field_set_yoffset(dfield, args->ymin);
    g_object_unref(unitxy);
    g_object_unref(unitz);

    if (rfile->regular == GXYZF_REGULAR_X)
        fill_field_x((const GwyTriangulationPointXYZ*)points->data, dfield);
    else if (rfile->regular == GXYZF_REGULAR_Y)
        fill_field_y((const GwyTriangulationPointXYZ*)points->data, dfield);
    else if ((gint)args->interpolation == GWY_INTERPOLATION_FIELD) {
        extend_borders(rfile, args, EPSREL);
        interpolate_field(points->len,
                          (const GwyTriangulationPointXYZ*)points->data,
                          dfield);
    }
    else {
        GwyTriangulation *triangulation = gwy_triangulation_new();

        extend_borders(rfile, args, EPSREL);
        ok = (gwy_triangulation_triangulate(triangulation,
                                            points->len, points->data,
                                            sizeof(GwyTriangulationPointXYZ))
              && gwy_triangulation_interpolate(triangulation,
                                               args->interpolation, dfield));
        g_object_unref(triangulation);
    }

    if (!ok) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("XYZ data regularization failed due to numerical "
                      "instability."));
        g_object_unref(dfield);
        return NULL;
    }

    /* Fix the scales according to real units. */
    gwy_data_field_set_xreal(dfield, mag*(args->xmax - args->xmin));
    gwy_data_field_set_yreal(dfield, mag*(args->ymax - args->ymin));
    gwy_data_field_set_xoffset(dfield, mag*args->xmin);
    gwy_data_field_set_yoffset(dfield, mag*args->ymin);
    gwy_data_field_multiply(dfield, pow10(zpow10));

    return dfield;
}

static void
fill_field_x(const GwyTriangulationPointXYZ *points,
             GwyDataField *dfield)
{
    gint xres = gwy_data_field_get_xres(dfield);
    gint yres = gwy_data_field_get_yres(dfield);
    gdouble *d = gwy_data_field_get_data(dfield);
    gint i;

    for (i = 0; i < xres*yres; i++)
        d[i] = points[i].z;
}

static void
fill_field_y(const GwyTriangulationPointXYZ *points,
             GwyDataField *dfield)
{
    gint xres = gwy_data_field_get_xres(dfield);
    gint yres = gwy_data_field_get_yres(dfield);
    gdouble *d = gwy_data_field_get_data(dfield);
    gint i, j;

    for (j = 0; j < xres; j++) {
        for (i = 0; i < yres; i++) {
            d[i*xres + j] = points[j*yres + i].z;
        }
    }
}

static void
interpolate_field(guint npoints,
                  const GwyTriangulationPointXYZ *points,
                  GwyDataField *dfield)
{
    gdouble xoff, yoff, qx, qy;
    guint xres, yres, i, j, k;
    gdouble *d;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xoff = gwy_data_field_get_xoffset(dfield);
    yoff = gwy_data_field_get_yoffset(dfield);
    qx = gwy_data_field_get_xreal(dfield)/xres;
    qy = gwy_data_field_get_yreal(dfield)/yres;
    d = gwy_data_field_get_data(dfield);

    for (i = 0; i < yres; i++) {
        gdouble y = yoff + qy*(i + 0.5);

        for (j = 0; j < xres; j++) {
            gdouble x = xoff + qx*(j + 0.5);
            gdouble w = 0.0;
            gdouble s = 0.0;

            for (k = 0; k < npoints; k++) {
                const GwyTriangulationPointXYZ *pt = points + k;
                gdouble dx = x - pt->x;
                gdouble dy = y - pt->y;
                gdouble r2 = dx*dx + dy*dy;

                r2 *= r2;
                if (G_UNLIKELY(r2 == 0.0)) {
                    s = pt->z;
                    w = 1.0;
                    break;
                }

                r2 = 1.0/r2;
                w += r2;
                s += r2*pt->z;
            }
            *(d++) = s/w;
        }
    }
}

static void
extend_borders(GxyzfFile *rfile,
               const GxyzfArgs *args,
               gdouble epsrel)
{
    gdouble xmin, xmax, ymin, ymax, xreal, yreal, eps;
    guint i;

    g_array_set_size(rfile->points, rfile->nbasepoints);

    if (args->exterior == GWY_EXTERIOR_BORDER_EXTEND)
        return;

    xreal = rfile->xmax - rfile->xmin;
    yreal = rfile->ymax - rfile->ymin;
    xmin = args->xmin - 2*rfile->step;
    xmax = args->xmax + 2*rfile->step;
    ymin = args->ymin - 2*rfile->step;
    ymax = args->ymax + 2*rfile->step;
    eps = epsrel*rfile->step;

    /* Extend the field according to requester boder extension, however,
     * create at most 3 full copies (4 halves and 4 quarters) of the base set.
     * Anyone asking for more is either clueless or malicious. */
    for (i = 0; i < rfile->nbasepoints; i++) {
        const GwyTriangulationPointXYZ *pt
            = &g_array_index(rfile->points, GwyTriangulationPointXYZ, i);
        GwyTriangulationPointXYZ pt2;
        gdouble txl, txr, tyt, tyb;
        gboolean txlok, txrok, tytok, tybok;

        pt2.z = pt->z;
        if (args->exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
            txl = 2.0*rfile->xmin - pt->x;
            tyt = 2.0*rfile->ymin - pt->y;
            txr = 2.0*rfile->xmax - pt->x;
            tyb = 2.0*rfile->ymax - pt->y;
            txlok = pt->x - rfile->xmin < 0.5*xreal;
            tytok = pt->y - rfile->ymin < 0.5*yreal;
            txrok = rfile->xmax - pt->x < 0.5*xreal;
            tybok = rfile->ymax - pt->y < 0.5*yreal;
        }
        else if (args->exterior == GWY_EXTERIOR_PERIODIC) {
            txl = pt->x - xreal;
            tyt = pt->y - yreal;
            txr = pt->x + xreal;
            tyb = pt->y + yreal;
            txlok = rfile->xmax - pt->x < 0.5*xreal;
            tytok = rfile->ymax - pt->y < 0.5*yreal;
            txrok = pt->x - rfile->xmin < 0.5*xreal;
            tybok = pt->y - rfile->ymin < 0.5*yreal;
        }
        else {
            g_assert_not_reached();
        }

        txlok = txlok && (txl >= xmin && txl <= xmax
                          && fabs(txl - rfile->xmin) > eps);
        tytok = tytok && (tyt >= ymin && tyt <= ymax
                          && fabs(tyt - rfile->ymin) > eps);
        txrok = txrok && (txr >= ymin && txr <= xmax
                          && fabs(txr - rfile->xmax) > eps);
        tybok = tybok && (tyb >= ymin && tyb <= xmax
                          && fabs(tyb - rfile->ymax) > eps);

        if (txlok) {
            pt2.x = txl;
            pt2.y = pt->y - eps;
            g_array_append_val(rfile->points, pt2);
        }
        if (txlok && tytok) {
            pt2.x = txl + eps;
            pt2.y = tyt - eps;
            g_array_append_val(rfile->points, pt2);
        }
        if (tytok) {
            pt2.x = pt->x + eps;
            pt2.y = tyt;
            g_array_append_val(rfile->points, pt2);
        }
        if (txrok && tytok) {
            pt2.x = txr + eps;
            pt2.y = tyt + eps;
            g_array_append_val(rfile->points, pt2);
        }
        if (txrok) {
            pt2.x = txr;
            pt2.y = pt->y + eps;
            g_array_append_val(rfile->points, pt2);
        }
        if (txrok && tybok) {
            pt2.x = txr - eps;
            pt2.y = tyb + eps;
            g_array_append_val(rfile->points, pt2);
        }
        if (tybok) {
            pt2.x = pt->x - eps;
            pt2.y = tyb;
            g_array_append_val(rfile->points, pt2);
        }
        if (txlok && tybok) {
            pt2.x = txl - eps;
            pt2.y = tyb - eps;
            g_array_append_val(rfile->points, pt2);
        }
    }
}

static void
gxyzf_free(GxyzfFile *rfile)
{
    g_array_free(rfile->points, TRUE);
}

static GArray*
read_points(const guchar *p, gsize size)
{
    GArray *points;
    gint i;

    printf("sizeof buffer %d\n", size);
    points = g_array_new(FALSE, FALSE, sizeof(GwyTriangulationPointXYZ));
    for (i=0; i<(size/3); i++) {
        GwyTriangulationPointXYZ pt;

        pt.x = gwy_get_gdouble_le(&p);
        pt.y = gwy_get_gdouble_le(&p);
        pt.z = gwy_get_gdouble_le(&p);


        g_array_append_val(points, pt);
    }

    printf("Loaded %d points\n", points->len);
    return points;
}

static gdouble
round_with_base(gdouble x, gdouble base)
{
    gint s;

    s = (x < 0) ? -1 : 1;
    x = fabs(x)/base;
    if (x <= 1.0)
        return GWY_ROUND(10.0*x)/10.0*s*base;
    else if (x <= 2.0)
        return GWY_ROUND(5.0*x)/5.0*s*base;
    else if (x <= 5.0)
        return GWY_ROUND(2.0*x)/2.0*s*base;
    else
        return GWY_ROUND(x)*s*base;
}

static void
round_to_nice(gdouble *minval, gdouble *maxval)
{
    gdouble range = *maxval - *minval;
    gdouble base = pow10(floor(log10(range)));

    *minval = round_with_base(*minval, base);
    *maxval = round_with_base(*maxval, base);
}

static void
initialize_ranges(const GxyzfFile *rfile,
                  GxyzfArgs *args)
{
    args->xmin = rfile->xmin;
    args->xmax = rfile->xmax;
    args->ymin = rfile->ymin;
    args->ymax = rfile->ymax;
    if (rfile->regular == GXYZF_IRREGULAR) {
        round_to_nice(&args->xmin, &args->xmax);
        round_to_nice(&args->ymin, &args->ymax);
    }
    else {
        args->xres = rfile->regular_xres;
        args->yres = rfile->regular_yres;
    }
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
        if (queue->id[i] == id)
            return;
    }
    work_queue_add(queue, id);
}

static inline gdouble
point_dist2(const GwyTriangulationPointXYZ *p,
            const GwyTriangulationPointXYZ *q)
{
    gdouble dx = p->x - q->x;
    gdouble dy = p->y - q->y;

    return dx*dx + dy*dy;
}

static gboolean
maybe_add_point(WorkQueue *pointqueue,
                const GwyTriangulationPointXYZ *newpoints,
                guint ii,
                gdouble eps2)
{
    const GwyTriangulationPointXYZ *pt;
    guint i;

    pt = newpoints + pointqueue->id[ii];
    for (i = 0; i < pointqueue->pos; i++) {
        if (point_dist2(pt, newpoints + pointqueue->id[i]) < eps2) {
            GWY_SWAP(guint,
                     pointqueue->id[ii], pointqueue->id[pointqueue->pos]);
            pointqueue->pos++;
            return TRUE;
        }
    }
    return FALSE;
}

/* Calculate coordinate ranges and ensure points are more than epsrel*cellside
 * appart where cellside is the side of equivalent-area square for one point. */
static void
analyse_points(GxyzfFile *rfile,
               double epsrel)
{
    WorkQueue cellqueue, pointqueue;
    GwyTriangulationPointXYZ *points, *newpoints, *pt;
    gdouble xreal, yreal, eps, eps2, xr, yr, step;
    guint npoints, i, ii, j, ig, xres, yres, ncells, oldpos;
    guint *cell_index;

    /* Calculate data ranges */
    npoints = rfile->norigpoints = rfile->points->len;
    points = (GwyTriangulationPointXYZ*)rfile->points->data;
    rfile->xmin = rfile->xmax = points[0].x;
    rfile->ymin = rfile->ymax = points[0].y;
    rfile->zmin = rfile->zmax = points[0].z;
    for (i = 1; i < npoints; i++) {
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

    if (check_regular_grid(rfile))
        return;

    xreal = rfile->xmax - rfile->xmin;
    yreal = rfile->ymax - rfile->ymin;

    printf("real size appears to be %g %g\n", xreal, yreal);

    if (xreal == 0.0 || yreal == 0.0) {
        g_warning("All points lie on a line, we are going to crash.");
    }

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
    rfile->step = step;
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
    newpoints = g_new(GwyTriangulationPointXYZ, npoints);

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
        cellqueue.pos = 0;
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
                iy = (guint)floor(y);
                y -= iy;

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
                j = cellqueue.id[cellqueue.pos];
                for (ii = cell_index[j]; ii < cell_index[j+1]; ii++) {
                    if (newpoints[ii].z != G_MAXDOUBLE)
                        work_queue_add(&pointqueue, ii);
                }
                cellqueue.pos++;
            }

            /* Compare all not-in-group points with all group points, adding
             * them to the group on success. */
            for (ii = pointqueue.pos; ii < pointqueue.len; ii++)
                maybe_add_point(&pointqueue, newpoints, ii, eps2);
        } while (oldpos != pointqueue.pos);

        /* Calculate the representant of all contributing points. */
        {
            GwyTriangulationPointXYZ avg = { 0.0, 0.0, 0.0 };

            for (ii = 0; ii < pointqueue.pos; ii++) {
                pt = newpoints + pointqueue.id[ii];
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
    g_free(newpoints);

    rfile->nbasepoints = rfile->points->len;
}

static gboolean
check_regular_grid(GxyzfFile *rfile)
{
    GwyTriangulationPointXYZ *pt1, *pt2;
    gdouble xstep, ystep, xeps, yeps;
    guint xres, yres, i, j;

    rfile->regular = GXYZF_IRREGULAR;

    if (rfile->points->len < 4)
        return FALSE;

    pt1 = &g_array_index(rfile->points, GwyTriangulationPointXYZ, 0);
    pt2 = &g_array_index(rfile->points, GwyTriangulationPointXYZ, 1);
    if (pt1->x == pt2->x) {
        for (i = 2; i < rfile->points->len; i++) {
            pt2 = &g_array_index(rfile->points, GwyTriangulationPointXYZ, i);
            if (pt2->x != pt1->x)
                break;
        }
        yres = rfile->regular_yres = i;
        xres = rfile->regular_xres = rfile->points->len/yres;
        rfile->regular = GXYZF_REGULAR_Y;
    }
    else if (pt1->y == pt2->y) {
        for (j = 2; j < rfile->points->len; j++) {
            pt2 = &g_array_index(rfile->points, GwyTriangulationPointXYZ, j);
            if (pt2->y != pt1->y)
                break;
        }
        xres = rfile->regular_xres = j;
        yres = rfile->regular_yres = rfile->points->len/xres;
        rfile->regular = GXYZF_REGULAR_X;
    }
    else
        return FALSE;

    if (rfile->points->len % xres
        || rfile->points->len % yres
        || xres < 2
        || yres < 2) {
        rfile->regular = GXYZF_IRREGULAR;
        return FALSE;
    }

    pt2 = &g_array_index(rfile->points, GwyTriangulationPointXYZ,
                         rfile->points->len-1);
    xstep = rfile->xstep = (pt2->x - pt1->x)/(xres - 1);
    ystep = rfile->ystep = (pt2->y - pt1->y)/(yres - 1);
    xeps = 0.05*fabs(xstep);
    yeps = 0.05*fabs(ystep);

    if (rfile->regular == GXYZF_REGULAR_X) {
        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                pt2 = &g_array_index(rfile->points, GwyTriangulationPointXYZ,
                                     i*xres + j);
                if (fabs(pt2->x - pt1->x - j*xstep) > xeps
                    || fabs(pt2->y - pt1->y - i*ystep) > yeps) {
                    rfile->regular = GXYZF_IRREGULAR;
                    return FALSE;
                }
            }
        }
    }
    else {
        for (j = 0; j < xres; j++) {
            for (i = 0; i < yres; i++) {
                pt2 = &g_array_index(rfile->points, GwyTriangulationPointXYZ,
                                     j*yres + i);
                if (fabs(pt2->x - pt1->x - j*xstep) > xeps
                    || fabs(pt2->y - pt1->y - i*ystep) > yeps) {
                    rfile->regular = GXYZF_IRREGULAR;
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

static const gchar exterior_key[]      = "/module/gxyzf/exterior";
static const gchar interpolation_key[] = "/module/gxyzf/interpolation";
static const gchar xy_units_key[]      = "/module/gxyzf/xy-units";
static const gchar z_units_key[]       = "/module/gxyzf/z-units";

static void
gxyzf_sanitize_args(GxyzfArgs *args)
{
    if (args->interpolation != GWY_INTERPOLATION_ROUND
        && (gint)args->interpolation != GWY_INTERPOLATION_FIELD)
        args->interpolation = GWY_INTERPOLATION_LINEAR;
    if (args->exterior != GWY_EXTERIOR_MIRROR_EXTEND
        && args->exterior != GWY_EXTERIOR_PERIODIC)
        args->exterior = GWY_EXTERIOR_BORDER_EXTEND;
}

static void
gxyzf_load_args(GwyContainer *container,
                 GxyzfArgs *args)
{
    *args = gxyzf_defaults;

    gwy_container_gis_enum_by_name(container, interpolation_key,
                                   &args->interpolation);
    gwy_container_gis_enum_by_name(container, exterior_key, &args->exterior);
    gwy_container_gis_string_by_name(container, xy_units_key,
                                     (const guchar**)&args->xy_units);
    gwy_container_gis_string_by_name(container, z_units_key,
                                     (const guchar**)&args->z_units);

    gxyzf_sanitize_args(args);
    args->xy_units = g_strdup(args->xy_units ? args->xy_units : "");
    args->z_units = g_strdup(args->z_units ? args->z_units : "");
}

static void
gxyzf_save_args(GwyContainer *container,
                 GxyzfArgs *args)
{
    gwy_container_set_enum_by_name(container, interpolation_key,
                                   args->interpolation);
    gwy_container_set_enum_by_name(container, exterior_key, args->exterior);
    gwy_container_set_string_by_name(container, xy_units_key,
                                     g_strdup(args->xy_units));
    gwy_container_set_string_by_name(container, z_units_key,
                                     g_strdup(args->z_units));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
