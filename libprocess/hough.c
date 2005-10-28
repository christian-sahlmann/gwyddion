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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libprocess/hough.h>
#include <libprocess/arithmetic.h>

static void bresenhams_line           (GwyDataField *dfield,
                                       gint x1,
                                       gint x2,
                                       gint y1,
                                       gint y2,
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
    for (col = MAX(0, rho-hsize); col < MIN(result->xres, rho+hsize); col++)
    {
        for (row = MAX(0, theta-hsize); row < MIN(result->yres, theta+hsize); row++)
        {
            if (hsize) coeff = 1 + sqrt((col-rho)*(col-rho) + (row-theta)*(row-theta));
            else coeff = 1;
            rdata[col + row*result->xres] += value/coeff;
        }
    }
}


void
gwy_data_field_hough_line(GwyDataField *dfield,
                               GwyDataField *x_gradient,
                               GwyDataField *y_gradient,
                               GwyDataField *result,
                               gint hwidth)
{
    gint k, col, row, xres, yres, rxres, ryres;
    gdouble rho, theta, rhostep, thetastep, *data, gradangle, gradangle0, gradangle2, gradangle3, threshold;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    rxres = gwy_data_field_get_xres(result); /*rho*/
    ryres = gwy_data_field_get_yres(result); /*theta*/

    if ((x_gradient && xres != gwy_data_field_get_xres(x_gradient)) ||
        (x_gradient && yres != gwy_data_field_get_yres(x_gradient)) ||
        (y_gradient && xres != gwy_data_field_get_xres(y_gradient)) ||
        (y_gradient && yres != gwy_data_field_get_yres(y_gradient)))
    {
        g_warning("Hough: input fields must be of same size (or null).\n");
        return;
    }


    thetastep = 2*G_PI/(gdouble)ryres;
    rhostep = 2.0*sqrt(xres*xres+yres*yres)/(gdouble)rxres;

    gwy_data_field_fill(result, 0);
    data = gwy_data_field_get_data(dfield);
    for (col = 0; col < xres; col++)
    {
        for (row = 0; row < yres; row++)
        {
            if (dfield->data[col + row*xres] > 0)
            {
                if (x_gradient && y_gradient)
                {
                    /*TODO fix this maximally stupid angle wrapping*/
                    gradangle = G_PI/2.0 + atan2(y_gradient->data[col + row*xres], x_gradient->data[col + row*xres]);
                    gradangle2 = gradangle + G_PI;
                    gradangle0 = gradangle - G_PI;
                    gradangle3 = gradangle + 2*G_PI;
                }
                for (k = 0; k < result->yres; k++)
                {
                    theta = (gdouble)k*thetastep;

                    threshold = 0.1;
                    if (x_gradient && y_gradient && !(fabs(theta-gradangle)<threshold || fabs(theta-gradangle2)<threshold
                        || fabs(theta-gradangle3)<threshold || fabs(theta-gradangle0)<threshold)) continue;


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
    gwy_data_field_set_yreal(result, 2*G_PI);

}

void
gwy_data_field_hough_line_strenghten(GwyDataField *dfield,
                                          GwyDataField *x_gradient,
                                          GwyDataField *y_gradient,
                                          gint hwidth,
                                          gdouble threshold)
{
    GwyDataField *result;
    gdouble hmax, hmin, threshval, zdata[200];
    gint i, xdata[200], ydata[200];

    result = gwy_data_field_new(sqrt(gwy_data_field_get_xres(dfield)*gwy_data_field_get_xres(dfield)
                             +gwy_data_field_get_yres(dfield)*gwy_data_field_get_yres(dfield)),
                             360, 0, 10,
                             FALSE);

    gwy_data_field_hough_line(dfield, x_gradient, y_gradient, result, hwidth);

    hmax = gwy_data_field_get_max(result);
    hmin = gwy_data_field_get_min(result);
    threshval = hmax + (hmax - hmin)*threshold; /*FIXME do GUI for this parameter*/

    gwy_data_field_get_local_maxima_list(result, xdata, ydata, zdata, 200, 2);

    for (i = 0; i < 200; i++)
    {
        if (zdata[i]>threshval && (ydata[i]<result->yres/4 || ydata[i]>=3*result->yres/4)) {
                bresenhams_line_polar(dfield,
                                      ((gdouble)xdata[i])*result->xreal/((gdouble)result->xres) - result->xreal/2,
                                      ((gdouble)ydata[i])*G_PI*2.0/((gdouble)result->yres),
                                      1);
        }
    }
}

void
gwy_data_field_hough_circle(GwyDataField *dfield,
                               GwyDataField *x_gradient,
                               GwyDataField *y_gradient,
                               GwyDataField *result,
                               gdouble radius)
{
    gint col, row, xres, yres, rxres, ryres;
    gdouble *data, angle;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    rxres = gwy_data_field_get_xres(result); /*rho*/
    ryres = gwy_data_field_get_yres(result); /*theta*/

    if ((x_gradient && xres != gwy_data_field_get_xres(x_gradient)) ||
        (x_gradient && yres != gwy_data_field_get_yres(x_gradient)) ||
        (y_gradient && xres != gwy_data_field_get_xres(y_gradient)) ||
        (y_gradient && yres != gwy_data_field_get_yres(y_gradient)))
    {
        g_warning("Hough: input fields must be of same size (or null).\n");
        return;
    }

    gwy_data_field_fill(result, 0);
    data = gwy_data_field_get_data(result);
    for (col = 0; col < xres; col++)
    {
        for (row = 0; row < yres; row++)
        {
            if (dfield->data[col + row*xres] > 0)
            {
                if (x_gradient && y_gradient)
                angle = atan2(y_gradient->data[col + row*xres], x_gradient->data[col + row*xres]);

                if (x_gradient && y_gradient) bresenhams_circle_gradient(result, radius, col, row, 1, angle);
                else bresenhams_circle(result, radius, col, row, 1);
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
    gint i, xdata[200], ydata[200];

    result = gwy_data_field_new_alike(dfield, FALSE);

    gwy_data_field_hough_circle(dfield, x_gradient, y_gradient, result, radius);

    hmax = gwy_data_field_get_max(result);
    hmin = gwy_data_field_get_min(result);
    threshval = hmax + (hmax - hmin)*threshold; /*FIXME do GUI for this parameter*/

    gwy_data_field_get_local_maxima_list(result, xdata, ydata, zdata, 200, 2);

    buffer = gwy_data_field_duplicate(dfield);
    gwy_data_field_fill(buffer, 0);

    for (i = 0; i < 200; i++)
    {
        if (zdata[i]>threshval) {
                bresenhams_circle(buffer,
                                      (gint)radius,
                                      xdata[i],
                                      ydata[i],
                                      1);

        }
    }
    gwy_data_field_threshold(buffer, 1, 0, 2);
    gwy_data_field_sum_fields(dfield, dfield, buffer);

}

static gint
signum(gint x)
{
    if (x<0) {
        return -1;
    }
    else if (x==0) {
        return 0;
    }
    return 1;
}



static void
bresenhams_line_polar(GwyDataField *dfield, gdouble rho, gdouble theta, gdouble value)
{
     gint x_top, x_bottom, y_left, y_right;
     gint x1, x2, y1, y2;
     gboolean x1set = FALSE;

     x_top = (gint)(rho/cos(theta));
     x_bottom = (gint)((rho - dfield->yres*sin(theta))/cos(theta));
     y_left = (gint)(rho/sin(theta));
     y_right = (gint)((rho - dfield->xres*cos(theta))/sin(theta));

     if (x_top >= 0 && x_top < dfield->xres)
     {
         x1 = x_top;
         y1 = 0;
         x1set = TRUE;
     }
     if (x_bottom >= 0 && x_bottom < dfield->xres)
     {
         if (x1set) {
             x2 = x_bottom;
             y2 = dfield->yres - 1;
         }
         else {
             x1 = x_bottom;
             y1 = dfield->yres - 1;
             x1set = TRUE;
         }
     }
     if (y_left >= 0 && y_left < dfield->yres)
     {
         if (x1set) {
             x2 = 0;
             y2 = y_left;
         }
         else {
             x1 = 0;
             y1 = y_left;
             x1set = TRUE;
         }
     }
     if (y_right >= 0 && y_right < dfield->yres)
     {
         x2 = dfield->xres - 1;
         y2 = y_right;
     }
     if (!x1set) {
         g_warning("line does not intersect image\n");
         return;
     }

     bresenhams_line(dfield, x1, x2, y1, y2, value);
}

static void
bresenhams_line(GwyDataField *dfield, gint x1, gint x2, gint y1, gint y2, gdouble value)
{
     gint i, dx, dy, sdx, sdy, dxabs, dyabs;
     gint x, y, px, py;

     dx = x2 - x1;
     dy = y2 - y1;
     dxabs = (gint)fabs(dx);
     dyabs = (gint)fabs(dy);
     sdx = signum(dx);
     sdy = signum(dy);
     x = dyabs>>1;
     y = dxabs>>1;
     px = x1;
     py = y1;

     dfield->data[px + py*dfield->xres] = value;

     if (dxabs >= dyabs)
     {
         for(i=0; i<dxabs; i++)
         {
             y += dyabs;
             if (y >= dxabs)
             {
                 y -= dxabs;
                 py += sdy;
             }
             px += sdx;
             dfield->data[px + py*dfield->xres] = value;
         }
     }
     else
     {
         for(i=0; i<dyabs; i++)
         {
             x += dxabs;
             if (x >= dyabs)
             {
                 x -= dyabs;
                 px += sdx;
             }
             py += sdy;
             dfield->data[px + py*dfield->xres] = value;
         }
     }
}

static void
plot_pixel_safe(GwyDataField *dfield, gint idx, gdouble value)
{
    if (idx > 0 && idx < dfield->xres*dfield->yres) dfield->data[idx] += value;
}

static void
bresenhams_circle(GwyDataField *dfield, gdouble r, gint col, gint row, gdouble value)
{
    gdouble n=0,invradius=1/(gdouble)r;
    gint dx = 0, dy = r-1;
    gint dxoffset, dyoffset;
    gint offset = col + row*dfield->xres;

    while (dx<=dy)
    {
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
    gdouble n=0,invradius=1/(gdouble)r;
    gint dx = 0, dy = r-1;
    gint dxoffset, dyoffset, i;
    gdouble diff;
    gint offset = col + row*dfield->xres;

    gdouble multoctant[8];
    for (i = 0; i<8; i++)
    {
        diff = fabs((G_PI*(gdouble)i/4.0 - G_PI) - gradient);
        if (diff > G_PI) diff = 2*G_PI - diff;
        multoctant[i] = G_PI - diff;
    }

    while (dx<=dy)
    {
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


gint
find_smallest_index(gdouble *data, gint n)
{
    gint i, imin;
    gdouble valmin = G_MAXDOUBLE;

    for (i=0; i<n; i++)
    {
        if (data[i] < valmin) {
            imin = i;
            valmin = data[i];
        }
    }
    return imin;

}

gboolean
find_isthere(gint *xdata, gint *ydata, gint mcol, gint mrow, gint n)
{
    gint i;
    for (i=0; i<n; i++)
    {
        if (xdata[i] == mcol && ydata[i] == mrow) return TRUE;
    }
    return FALSE;
}

gdouble
find_nmax(GwyDataField *dfield, gint *mcol, gint *mrow)
{

    if ((*mcol) > 0 && dfield->data[(*mcol) - 1 + (*mrow)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) -= 1;
        find_nmax(dfield, mcol, mrow);
    }
    if ((*mrow) > 0 && dfield->data[(*mcol) + ((*mrow) - 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mrow) -= 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mcol) < (dfield->xres - 1) &&
         dfield->data[(*mcol) + 1 + (*mrow)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) += 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mrow) < (dfield->yres - 1) &&
         dfield->data[(*mcol) + ((*mrow) + 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mrow) += 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mcol) > 0  && (*mrow) > 0 &&
         dfield->data[(*mcol) - 1 + ((*mrow) - 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) -= 1;
        (*mrow) -= 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mcol) > 0 && (*mrow) < (dfield->yres - 1) &&
         dfield->data[(*mcol) - 1 + ((*mrow) + 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) -= 1;
        (*mrow) += 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mcol) < (dfield->xres - 1) && (*mrow) > 0 &&
         dfield->data[(*mcol) + 1 + ((*mrow) - 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) += 1;
        (*mrow) -= 1;
        find_nmax(dfield, mcol, mrow);
    }
     if ((*mcol) < (dfield->xres - 1) &&
         (*mrow) < (dfield->yres - 1) && dfield->data[(*mcol) + 1 + ((*mrow) + 1)*dfield->xres] > dfield->data[(*mcol) + (*mrow)*dfield->xres])
    {
        (*mcol) += 1;
        (*mrow) += 1;
        find_nmax(dfield, mcol, mrow);
    }

    return dfield->data[(*mcol) + (*mrow)*dfield->xres];
}



void
gwy_data_field_get_local_maxima_list(GwyDataField *dfield,
                                          gint *xdata,
                                          gint *ydata,
                                          gdouble *zdata,
                                          gint ndata,
                                          gint skip)
{
    gint col, row, mcol, mrow, i;
    gdouble value;

    for (i=0; i<ndata; i++)
    {
        xdata[i] = 0;
        ydata[i] = 0;
        zdata[i] = -G_MAXDOUBLE;
    }

    for (col=0; col<dfield->xres; col += 1 + skip)
    {
        for (row=0; row<dfield->yres; row+=2)
        {
            mcol = col;
            mrow = row;
            value = find_nmax(dfield, &mcol, &mrow);

            if (find_isthere(xdata, ydata, mcol, mrow, ndata)) continue;

            i = find_smallest_index(zdata, ndata);
            if (zdata[i] < value) {
                zdata[i] = value;
                xdata[i] = mcol;
                ydata[i] = mrow;
            }
        }
    }

}

/************************** Documentation ****************************/

/**
 * SECTION:hough
 * @title: hough
 * @short_description: Hough transform
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
