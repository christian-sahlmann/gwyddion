/*
 *  @(#) $Id$
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

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libprocess/hough.h>
#include <libprocess/grains.h>
#include <libprocess/arithmetic.h>

static void bresenhams_line           (GwyDataField *dfield,
                                       gint x1,
                                       gint x2,
                                       gint y1_,
                                       gint y2_,
                                       gdouble value);
static void bresenhams_line_polar     (GwyDataField *dfield,
                                       gdouble rho,
                                       gdouble theta,
                                       gdouble value);
static void bresenhams_circle         (GwyDataField *dfield,
                                       gdouble r,
                                       gint col,
                                       gint row,
                                       gdouble value);
static void bresenhams_circle_gradient(GwyDataField *dfield,
                                       gdouble r,
                                       gint col,
                                       gint row,
                                       gdouble value,
                                       gdouble angle);

static void
add_point(GwyDataField *result,
          gint rho, gint theta,
          gdouble value, gint hsize)
{
    gint col, row;
    gdouble *rdata, coeff;

    rdata = gwy_data_field_get_data(result);
    for (col = MAX(0, rho-hsize); col < MIN(result->xres, rho+hsize); col++) {
        for (row = MAX(0, theta-hsize); row < MIN(result->yres, theta+hsize); row++) {
            if (hsize)
                coeff = 1 + hypot(col-rho, row-theta);
            else
                coeff = 1;
            rdata[col + row*result->xres] += value/coeff;
        }
    }
}

void
gwy_data_field_hough_line(GwyDataField *dfield,
                               GwyDataField *x_gradient,
                               GwyDataField *y_gradient,
                               GwyDataField *result,
                               gint hwidth,
                               gboolean overlapping)
{
    gint k, col, row, xres, yres, rxres, ryres;
    gdouble rho, theta, rhostep, thetastep, *data, gradangle = 0, threshold;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    rxres = gwy_data_field_get_xres(result); /*rho*/
    ryres = gwy_data_field_get_yres(result); /*theta*/

    if ((x_gradient && xres != gwy_data_field_get_xres(x_gradient))
        || (x_gradient && yres != gwy_data_field_get_yres(x_gradient))
        || (y_gradient && xres != gwy_data_field_get_xres(y_gradient))
        || (y_gradient && yres != gwy_data_field_get_yres(y_gradient))) {
        g_warning("Hough: input fields must be of same size (or null).\n");
        return;
    }


    if (overlapping)
        thetastep = 2*G_PI/(gdouble)ryres;
    else
        thetastep = G_PI/(gdouble)ryres;
    rhostep = 2*sqrt(xres*xres+yres*yres)/(gdouble)rxres;

    gwy_data_field_fill(result, 0);
    data = gwy_data_field_get_data(dfield);
    for (col = 0; col < xres; col++) {
        for (row = 0; row < yres; row++) {
            if (dfield->data[col + row*xres] > 0) {
                if (x_gradient && y_gradient) {
                    gradangle = atan2(y_gradient->data[col + row*xres],
                                      x_gradient->data[col + row*xres]);
                    if (gradangle < 0)
                        gradangle += G_PI;
                    if (!overlapping)
                        gradangle += G_PI/4;
                }
                for (k = 0; k < result->yres; k++) {
                    theta = (gdouble)k*thetastep;
                    if (!overlapping)
                        theta += G_PI/4;

                    threshold = 1.0;

                    /*if (data[col + row*xres]) printf("%g %g\n", theta, gradangle);
                    if (x_gradient && y_gradient && !(fabs(theta-gradangle)<threshold)) continue;*/


                    rho = ((gdouble)col)*cos(theta) + ((gdouble)row)*sin(theta);

                    add_point(result,
                              (gint)((gdouble)(rho/rhostep)+(gdouble)rxres/2),
                              k,
                              data[col + row*xres],
                              hwidth);

                }
            }
        }
    }
    gwy_data_field_set_xreal(result, 2*sqrt(xres*xres+yres*yres));
    if (!overlapping)
        gwy_data_field_set_yreal(result, 2*G_PI);
    else
        gwy_data_field_set_yreal(result, G_PI);

}

void
gwy_data_field_hough_line_strenghten(GwyDataField *dfield,
                                          GwyDataField *x_gradient,
                                          GwyDataField *y_gradient,
                                          gint hwidth,
                                          gdouble threshold)
{
    GwyDataField *result, *water;
    gdouble hmax, hmin, threshval, zdata[20];
    gint i;
    gdouble xdata[20], ydata[20];

    result = gwy_data_field_new(3*hypot(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield)),
                                360, 0, 10,
                                FALSE);

    gwy_data_field_hough_line(dfield, x_gradient, y_gradient, result, hwidth,
                              FALSE);

    water = gwy_data_field_duplicate(result);
    gwy_data_field_grains_splash_water(result, water, 2,
                                       0.005*(gwy_data_field_get_max(result)
                                              - gwy_data_field_get_min(result)));

   gwy_debug("%d %d, %g, %g",
             gwy_data_field_get_xres(result),
             gwy_data_field_get_yres(result),
             gwy_data_field_get_min(result),
             gwy_data_field_get_max(result));

    gwy_data_field_get_min_max(water, &hmin, &hmax);

    threshval = hmin + (hmax - hmin)*threshold; /*FIXME do GUI for this parameter*/
    gwy_data_field_get_local_maxima_list(water, xdata, ydata, zdata, 20, 10,
                                         threshval, TRUE);

    for (i = 0; i < 20; i++) {
       if (zdata[i] > threshval) {
           gwy_debug("point: %g %g (of %d %d), xreal: %g  yreal: %g\n",
                     xdata[i], ydata[i],
                     result->xres, result->yres, result->xreal, result->yreal);
           bresenhams_line_polar(dfield,
                                 ((gdouble)xdata[i])*gwy_data_field_get_xreal(result)/((gdouble)gwy_data_field_get_xres(result))
                                                  - gwy_data_field_get_xreal(result)/2.0,
                                      ((gdouble)ydata[i])*G_PI/((gdouble)gwy_data_field_get_yres(result)) + G_PI/4,
                                      1);
        }
    }
    g_object_unref(water);
}

void
gwy_data_field_hough_circle(GwyDataField *dfield,
                               GwyDataField *x_gradient,
                               GwyDataField *y_gradient,
                               GwyDataField *result,
                               gdouble radius)
{
    gint col, row, xres, yres;
    gdouble angle = 0.0;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    if ((x_gradient && xres != gwy_data_field_get_xres(x_gradient))
        || (x_gradient && yres != gwy_data_field_get_yres(x_gradient))
        || (y_gradient && xres != gwy_data_field_get_xres(y_gradient))
        || (y_gradient && yres != gwy_data_field_get_yres(y_gradient))) {
        g_warning("Hough: input fields must be of same size (or null).\n");
        return;
    }

    gwy_data_field_fill(result, 0);
    for (col = 0; col < xres; col++) {
        for (row = 0; row < yres; row++) {
            if (dfield->data[col + row*xres] > 0) {
                if (x_gradient && y_gradient)
                    angle = atan2(y_gradient->data[col + row*xres],
                                  x_gradient->data[col + row*xres]);

                if (x_gradient && y_gradient)
                    bresenhams_circle_gradient(result, radius, col, row, 1,
                                               angle);
                else
                    bresenhams_circle(result, radius, col, row, 1);
            }
        }
    }

}

void
gwy_data_field_hough_circle_strenghten(GwyDataField *dfield,
                                          GwyDataField *x_gradient,
                                          GwyDataField *y_gradient,
                                          gdouble radius,
                                          gdouble threshold)
{
    GwyDataField *result, *buffer;
    gdouble hmax, hmin, threshval, zdata[200];
    gint i;
    gdouble xdata[200], ydata[200];

    result = gwy_data_field_new_alike(dfield, FALSE);

    gwy_data_field_hough_circle(dfield, x_gradient, y_gradient, result, radius);




    gwy_data_field_get_min_max(result, &hmin, &hmax);
    threshval = hmax + (hmax - hmin)*threshold; /*FIXME do GUI for this parameter*/
    gwy_data_field_get_local_maxima_list(result, xdata, ydata, zdata, 200, 2,
                                         threshval, TRUE);

    buffer = gwy_data_field_duplicate(dfield);
    gwy_data_field_fill(buffer, 0);

    for (i = 0; i < 200; i++) {
        if (zdata[i] > threshval) {
                bresenhams_circle(buffer, (gint)radius, xdata[i], ydata[i], 1);
        }
    }
    gwy_data_field_threshold(buffer, 1, 0, 2);
    gwy_data_field_sum_fields(dfield, dfield, buffer);

}

static inline gint
signum(gint x)
{
    if (x < 0) {
        return -1;
    }
    if (x > 0) {
        return 1;
    }
    return 0;
}

void
gwy_data_field_hough_polar_line_to_datafield(GwyDataField *dfield,
                      gdouble rho, gdouble theta,
                     gint *px1, gint *px2, gint *py1, gint *py2)
{
     gint x_top, x_bottom, y_left, y_right;
     gboolean x1set = FALSE;

     x_top = (gint)(rho/cos(theta));
     x_bottom = (gint)((rho - dfield->yres*sin(theta))/cos(theta));
     y_left = (gint)(rho/sin(theta));
     y_right = (gint)((rho - dfield->xres*cos(theta))/sin(theta));

     if (x_top >= 0 && x_top < dfield->xres) {
         *px1 = x_top;
         *py1 = 0;
         x1set = TRUE;
     }
     if (x_bottom >= 0 && x_bottom < dfield->xres) {
         if (x1set) {
             *px2 = x_bottom;
             *py2 = dfield->yres - 1;
         }
         else {
             *px1 = x_bottom;
             *py1 = dfield->yres - 1;
             x1set = TRUE;
         }
     }
     if (y_left >= 0 && y_left < dfield->yres) {
         if (x1set) {
             *px2 = 0;
             *py2 = y_left;
         }
         else {
             *px1 = 0;
             *py1 = y_left;
             x1set = TRUE;
         }
     }
     if (y_right >= 0 && y_right < dfield->yres) {
         *px2 = dfield->xres - 1;
         *py2 = y_right;
     }
     if (!x1set) {
         g_warning("line does not intersect image");
         return;
     }
}

static void
bresenhams_line_polar(GwyDataField *dfield,
                      gdouble rho, gdouble theta, gdouble value)
{
     gint px1, px2, py1, py2;

     gwy_data_field_hough_polar_line_to_datafield(dfield, rho, theta,
                                                  &px1, &px2, &py1, &py2);
     bresenhams_line(dfield, px1, px2, py1, py2, value);
}

static void
bresenhams_line(GwyDataField *dfield,
                gint x1, gint x2,
                gint y1_, gint y2_,
                gdouble value)
{
     gint i, dx, dy, sdx, sdy, dxabs, dyabs;
     gint x, y, px, py;

     dx = x2 - x1;
     dy = y2_ - y1_;
     dxabs = (gint)fabs(dx);
     dyabs = (gint)fabs(dy);
     sdx = signum(dx);
     sdy = signum(dy);
     x = dyabs>>1;
     y = dxabs>>1;
     px = x1;
     py = y1_;

     dfield->data[px + py*dfield->xres] = value;

     if (dxabs >= dyabs) {
         for (i = 0; i < dxabs; i++) {
             y += dyabs;
             if (y >= dxabs) {
                 y -= dxabs;
                 py += sdy;
             }
             px += sdx;
             dfield->data[px + py*dfield->xres] = value;
         }
     }
     else {
         for (i = 0; i < dyabs; i++) {
             x += dxabs;
             if (x >= dyabs) {
                 x -= dyabs;
                 px += sdx;
             }
             py += sdy;
             dfield->data[px + py*dfield->xres] = value;
         }
     }
}

static inline void
plot_pixel_safe(GwyDataField *dfield, gint idx, gdouble value)
{
    if (idx > 0 && idx < dfield->xres*dfield->yres)
        dfield->data[idx] += value;
}

static void
bresenhams_circle(GwyDataField *dfield,
                  gdouble r,
                  gint col, gint row,
                  gdouble value)
{
    gdouble n = 0.0, invradius=1.0/r;
    gint dx = 0, dy = r-1;
    gint dxoffset, dyoffset;
    gint offset = col + row*dfield->xres;

    while (dx <= dy) {
         dxoffset = dfield->xres*dx;
         dyoffset = dfield->xres*dy;
         plot_pixel_safe(dfield, offset + dy - dxoffset, value);
         plot_pixel_safe(dfield, offset + dx - dyoffset, value);
         plot_pixel_safe(dfield, offset - dx - dyoffset, value);
         plot_pixel_safe(dfield, offset - dy - dxoffset, value);
         plot_pixel_safe(dfield, offset - dy + dxoffset, value);
         plot_pixel_safe(dfield, offset - dx + dyoffset, value);
         plot_pixel_safe(dfield, offset + dx + dyoffset, value);
         plot_pixel_safe(dfield, offset + dy + dxoffset, value);
         dx++;
         n += invradius;
         dy = r * sin(acos(n));
     }
}

static void
bresenhams_circle_gradient(GwyDataField *dfield, gdouble r, gint col, gint row,
                           gdouble value, gdouble gradient)
{
    gdouble n = 0.0, invradius = 1.0/r;
    gint dx = 0, dy = r-1;
    gint dxoffset, dyoffset, i;
    gdouble diff;
    gint offset = col + row*dfield->xres;
    gdouble multoctant[8];

    for (i = 0; i < 8; i++)
    {
        diff = fabs((G_PI*(gdouble)i/4.0 - G_PI) - gradient);
        if (diff > G_PI)
            diff = 2*G_PI - diff;
        multoctant[i] = G_PI - diff;
    }

    while (dx <= dy) {
         dxoffset = dfield->xres*dx;
         dyoffset = dfield->xres*dy;
         plot_pixel_safe(dfield, offset + dy - dxoffset, value*multoctant[0]);
         plot_pixel_safe(dfield, offset + dx - dyoffset, value*multoctant[1]);
         plot_pixel_safe(dfield, offset - dx - dyoffset, value*multoctant[2]);
         plot_pixel_safe(dfield, offset - dy - dxoffset, value*multoctant[3]);
         plot_pixel_safe(dfield, offset - dy + dxoffset, value*multoctant[4]);
         plot_pixel_safe(dfield, offset - dx + dyoffset, value*multoctant[5]);
         plot_pixel_safe(dfield, offset + dx + dyoffset, value*multoctant[6]);
         plot_pixel_safe(dfield, offset + dy + dxoffset, value*multoctant[7]);
         dx++;
         n += invradius;
         dy = r * sin(acos(n));
     }
}


static gint
find_smallest_index(gdouble *data, gint n)
{
    gint i, imin = 0;
    gdouble valmin = G_MAXDOUBLE;

    for (i = 0; i < n; i++) {
        if (data[i] < valmin) {
            imin = i;
            valmin = data[i];
        }
    }
    return imin;

}

static gint
find_isthere(gdouble *xdata, gdouble *ydata,
             gint mcol, gint mrow, gdouble skip, gint n)
{
    gint i;

    for (i = 0; i < n; i++) {
        if (sqrt((xdata[i]-mcol)*(xdata[i]-mcol)
                 + (ydata[i]-mrow)*(ydata[i]-mrow)) < skip)
            return i;
    }
    return -1;
}

/* XXX: Cannot it be written readably? */
static gdouble
find_nmax(GwyDataField *dfield, gint *mcol, gint *mrow)
{

    if ((*mcol) > 0
        && dfield->data[(*mcol) - 1 + (*mrow)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres]) {
        (*mcol) -= 1;
        find_nmax(dfield, mcol, mrow);
    }

    if ((*mrow) > 0
        && dfield->data[(*mcol) + ((*mrow) - 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres]) {
        (*mrow) -= 1;
        find_nmax(dfield, mcol, mrow);
    }

    if ((*mcol) < (dfield->xres - 1)
        && dfield->data[(*mcol) + 1 + (*mrow)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres]) {
        (*mcol) += 1;
        find_nmax(dfield, mcol, mrow);
    }

    if ((*mrow) < (dfield->yres - 1)
        && dfield->data[(*mcol) + ((*mrow) + 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres]) {
        (*mrow) += 1;
        find_nmax(dfield, mcol, mrow);
    }

    if ((*mcol) > 0
        && (*mrow) > 0
        && dfield->data[(*mcol) - 1 + ((*mrow) - 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres]) {
        (*mcol) -= 1;
        (*mrow) -= 1;
        find_nmax(dfield, mcol, mrow);
    }

    if ((*mcol) > 0 && (*mrow) < (dfield->yres - 1)
        && dfield->data[(*mcol) - 1 + ((*mrow) + 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres]) {
        (*mcol) -= 1;
        (*mrow) += 1;
        find_nmax(dfield, mcol, mrow);
    }

    if ((*mcol) < (dfield->xres - 1) && (*mrow) > 0
        && dfield->data[(*mcol) + 1 + ((*mrow) - 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres]) {
        (*mcol) += 1;
        (*mrow) -= 1;
        find_nmax(dfield, mcol, mrow);
    }

    if ((*mcol) < (dfield->xres - 1)
        && (*mrow) < (dfield->yres - 1)
        && dfield->data[(*mcol) + 1 + ((*mrow) + 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres]) {
        (*mcol) += 1;
        (*mrow) += 1;
        find_nmax(dfield, mcol, mrow);
    }

    return dfield->data[(*mcol) + (*mrow)*dfield->xres];
}

static void
get_local_maximum(GwyDataField *dfield,
                  gint mcol, gint mrow,
                  gdouble *xval, gdouble *yval)
{
    gint xres, yres;
    gdouble zm, zp, z0;
    const gdouble *data;

    xres = dfield->xres;
    yres = dfield->yres;
    data = dfield->data;
    z0 = dfield->data[xres*mrow + mcol];
    if (mcol > 0 && mcol < xres-1) {
        zm = data[mrow*xres + mcol-1];
        zp = data[mrow*xres + mcol+1];
        if ((zm + zp - 2*z0) == 0)
            *xval = (gdouble)mcol;
        else {
            *xval = (gdouble)mcol + (zm - zp)/(zm + zp - 2*z0)/2.0;
        }
    }
    else
        *xval = (gdouble)mcol;

    if (mrow > 0 && mrow < yres-1) {
        zm = data[(mrow - 1)*xres + mcol];
        zp = data[(mrow + 1)*xres + mcol];
        if ((zm + zp - 2*z0) == 0)
            *yval = (gdouble)mrow;
        else
            *yval = (gdouble)mrow + (zm - zp)/(zm + zp - 2*z0)/2.0;
    }
    else
        *yval = (gdouble)mrow;
}


gint
gwy_data_field_get_local_maxima_list(GwyDataField *dfield,
                                     gdouble *xdata,
                                     gdouble *ydata,
                                     gdouble *zdata,
                                     gint ndata,
                                     gint skip,
                                     gdouble threshold,
                                     gboolean subpixel)
{
    gint col, row, mcol, mrow, i, count, xres, yres;
    gdouble xval, yval;
    gdouble value;

    for (i = 0; i < ndata; i++) {
        xdata[i] = 0;
        ydata[i] = 0;
        zdata[i] = -G_MAXDOUBLE;
    }

    count = 0;
    xres = dfield->xres;
    yres = dfield->yres;
    for (row = 0; row < yres; row++) {
        for (col = 0; col < xres; col++) {
            if ((value = dfield->data[row*xres + col]) > threshold) {
                mcol = col;
                mrow = row;

                i = find_isthere(xdata, ydata, mcol, mrow, skip, count);
                if (i == -1)
                    i = find_smallest_index(zdata, ndata);

                if (zdata[i] < value)  {
                    if (subpixel) {
                        get_local_maximum(dfield, mcol, mrow,
                                          &xval, &yval);
                        zdata[i] = value;
                        xdata[i] = xval;
                        ydata[i] = yval;
                        count = MAX(i + 1, count);
                    }
                    else {
                        zdata[i] = value;
                        xdata[i] = (gdouble)mcol;
                        ydata[i] = (gdouble)mrow;
                        count = MAX(i + 1, count);
                    }
                    continue;
                }
            }
        }
    }

    return count;
}

void
gwy_data_field_hough_datafield_line_to_polar(gint px1,
                                             gint px2,
                                             gint py1,
                                             gint py2,
                                             gdouble *rho,
                                             gdouble *theta)
{
    gdouble k, q;

    k = (py2 - py1)/(gdouble)(px2 - px1);
    q = py1 - (py2 - py1)/(gdouble)(px2 - px1)*px1;

    *rho = q/sqrt(k*k + 1);
    *theta = asin(1/sqrt(k*k + 1));

    /*printf("line: p1 (%d, %d), p2 (%d, %d), k=%g q=%g rho=%g theta=%g\n",
           px1, py1, px2, py2, k, q, *rho, *theta);*/

}


/************************** Documentation ****************************/

/**
 * SECTION:hough
 * @title: hough
 * @short_description: Hough transform
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
