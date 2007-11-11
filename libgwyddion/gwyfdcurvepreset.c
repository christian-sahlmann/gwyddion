
/*
 *  @(#) $Id$
 *  Copyright (C) 2000-2003 Martin Siler.
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwyfdcurvepreset.h>
#include "gwyddioninternal.h"

static GwyFDCurvePreset*
gwy_fd_curve_preset_new_static(const GwyNLFitPresetBuiltin *data);

G_DEFINE_TYPE(GwyFDCurvePreset, gwy_fd_curve_preset, GWY_TYPE_NLFIT_PRESET)

#if 0
/******************* argento ********************************/
static gdouble
argento_func(gdouble x,
             G_GNUC_UNUSED gint n_param,
             const gdouble *b,
             G_GNUC_UNUSED gpointer user_data,
             gboolean *fres)
{
    /*xc, R, H, gamma*/
    *fres = TRUE;
    return b[1] + (b[3]*b[2]*b[2]*(1 - sin(b[4]))*(b[2]*sin(b[4]) - (x-b[0])*sin(b[4]) - b[2] - (x-b[0])))
                            /(6*(x-b[0])*(x-b[0])*((x-b[0]) + b[2] - b[2]*sin(b[4]))*((x-b[0]) + b[2] - b[2]*sin(b[4])))
                                      - (b[3]*tan(b[4])*((x-b[0])*sin(b[4]) + b[2]*sin(b[4]) + b[2]*cos(2*b[4])))
                                      /(6*cos(b[4])*((x-b[0]) + b[2] - b[2]*sin(b[4]))*(((x-b[0]) + b[2] - b[2]*sin(b[4]))));
}

static void
argento_guess(gint n_dat,
              const gdouble *x,
              const gdouble *y,
              gdouble *param,
              gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/100;
    param[2] = 20e-9;
    param[3] = 2e-20;
    param[4] = 1;

    *fres = TRUE;
}

/******************* parzanette ********************************/
static gdouble
parzanette_func(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *b,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    /*xc, R, H, gamma, h1, L*/
    *fres = TRUE;
    return b[1] -(b[3]/6)*((b[5]*b[5]*(3*b[2]*(x-b[0]) + b[2]*b[5] - (x-b[0])*b[5]))/((x-b[0])*(x-b[0])*pow(((x-b[0])+b[5]), 3))
            + (b[6]*b[6])/(pow(((x-b[0])+b[5]), 3))
            + (4*tan(b[4]))/(G_PI)*(b[6]+tan(b[4])*((x-b[0])+b[5]))/(pow(((x-b[0])+b[5]), 2)));
}

static void
parzanette_guess(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/20;
    param[2] = 20e-9;
    param[3] = 2e-21;
}
#endif


/******************* sszanette ********************************/
static gdouble
sszanette_func(gdouble x,
               G_GNUC_UNUSED gint n_param,
               const gdouble *b,
               G_GNUC_UNUSED gpointer user_data,
               gboolean *fres)
{
    /*xc, R, H */
    *fres = TRUE;
    return b[1] -b[3]/6*(b[2]*b[2]*b[2] * (b[2]+2*(x-b[0])))/((x-b[0])*(x-b[0]) * pow(((x-b[0]) + b[2]), 3));
}

static void
sszanette_guess(gint n_dat,
                const gdouble *x,
                const gdouble *y,
                gdouble *param,
                gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/20;
    param[2] = 20e-9;
    param[3] = 2e-21;
    *fres = TRUE;
}

/******************* pyrzanette ********************************/
static gdouble
pyrzanette_func(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *b,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    /*xc, R, H, gamma*/
    *fres = TRUE;
    return b[1] -2*b[3]*(tan(b[4])*tan(b[4]))/3/G_PI/(x-b[0]);
}

static void
pyrzanette_guess(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/100;
    param[2] = 20e-9;
    param[3] = 2e-20;
    param[4] = 1;
    *fres = TRUE;
}

#if 0
/******************* tpyrzanette ********************************/
static gdouble
tpyrzanette_func(gdouble x,
                 G_GNUC_UNUSED gint n_param,
                 const gdouble *b,
                 G_GNUC_UNUSED gpointer user_data,
                 gboolean *fres)
{
    /*xc, R, H, gamma, L, */
    *fres = TRUE;
    return b[1] - 2*b[3]*b[5]*b[5]/(x-b[0])*(x-b[0])*(x-b[0])
           * (1 + (tan(b[4])*(x-b[0]))/b[5] +
           (tan(b[4])*(x-b[0])*tan(b[4])*(x-b[0]))/b[5]/b[5]);
}

static void
tpyrzanette_guess(gint n_dat,
                  const gdouble *x,
                  const gdouble *y,
                  gdouble *param,
                  gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/100;
    param[2] = 20e-9;
    param[3] = 2e-20;
    param[4] = 1;
    param[5] = 20e-9;
}
#endif

/******************* sphcapella ********************************/
static gdouble
sphcapella_func(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *b,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    /*xc, R, H */
    *fres = TRUE;
    return b[1] -b[3]*b[2]/6/(x-b[0])/(x-b[0]) ;
}

static void
sphcapella_guess(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/20;
    param[2] = 20e-9;
    param[3] = 2e-21;

    *fres = TRUE;
}

/******************* sphsphcapella ********************************/
static gdouble
sphsphcapella_func(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *b,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    /*xc, R, H */
    *fres = TRUE;
    return b[1] -b[4]*b[3]*b[2]/6/(x-b[0])/(x-b[0])/(b[2]+b[3]) ;
}

static void
sphsphcapella_guess(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/20;
    param[2] = 20e-9;
    param[3] = 20e-9;
    param[4] = 2e-21;

    *fres = TRUE;
}

/******************* conecapella ********************************/
static gdouble
conecapella_func(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *b,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    /*xc, R, H */
    *fres = TRUE;
    return b[1] -tan(b[2])*tan(b[2])*b[3]/6/(x-b[0]) ;
}

static void
conecapella_guess(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/20;
    param[2] = 0.5;
    param[3] = 200e-21;

    *fres = TRUE;
}

/******************* cylindercapella ********************************/
static gdouble
cylindercapella_func(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *b,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    /*xc, R, H */
    *fres = TRUE;
    return b[1] -b[3]*b[2]*b[2]/6/(x-b[0])/(x-b[0])/(x-b[0]) ;
}

static void
cylindercapella_guess(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/20;
    param[2] = 20e-9;
    param[3] = 2e-23;

    *fres = TRUE;
}

/******************* paraboloidcapella ********************************/
static gdouble
parcapella_func(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *b,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    /*xc, R, H */
    *fres = TRUE;
    return b[1] -b[4]*b[2]*b[2]/b[3]/12/(x-b[0])/(x-b[0]) ;
}

static void
parcapella_guess(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/20;
    param[2] = 20e-9;
    param[3] = 150e-9;
    param[4] = 2e-21;

    *fres = TRUE;
}



/******************* sphtiptap ********************************/
static gdouble
sphtiptap_func(gdouble x,
               G_GNUC_UNUSED gint n_param,
               const gdouble *b,
               G_GNUC_UNUSED gpointer user_data,
               gboolean *fres)
{
    /*xc, R, H., xc */
    *fres = TRUE;
    return b[1] - b[3]*b[2]/6/((x-b[0])-b[4])/((x-b[0])-b[4]);
}

static void
sphtiptap_guess(gint n_dat,
                const gdouble *x,
                const gdouble *y,
                gdouble *param,
                gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/20;
    param[2] = 20e-9;
    param[3] = 2e-21;
    param[4] = 0;
    *fres = TRUE;
}

#if 0
/******************* sphxu ********************************/
static gdouble
sphxu_func(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
     /*xc, R, H, sigma */
    *fres = TRUE;
    return b[1] - b[3]*b[2]/12*(1/(x-b[0])/(x-b[0]) - 1/15*pow(b[4], 6)/pow((x-b[0]), 8));
}

static void
sphxu_guess(gint n_dat,
            const gdouble *x,
            const gdouble *y,
            gdouble *param,
            gboolean *fres)
{
    gint i;
    gdouble xmin = x[0], xmax = x[n_dat - 1];

    param[1] = y[0]/n_dat;

    for (i = 1; i < n_dat; i++) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        param[1] += y[i]/n_dat;
    }
    param[0] = xmin - (xmax-xmin)/20;
    param[2] = 20e-9;
    param[3] = 2e-21;
    param[4] = 1;
    *fres = TRUE;
}

/******************* sphcappakarinen ********************************/
static gdouble
sphcappakarinen_func(gdouble x,
                     G_GNUC_UNUSED gint n_param,
                     const gdouble *b,
                     G_GNUC_UNUSED gpointer user_data,
                     gboolean *fres)
{
    /*R, gamma, theta1, theta2*/
    *fres = TRUE;
    return  b[1] - 2*b[2]*G_PI*b[0]*(cos(b[3]) + cos(b[4]));
}

static void
sphcappakarinen_guess(gint n_dat,
                      const gdouble *x,
                      const gdouble *y,
                      gdouble *param,
                      gboolean *fres)
{
    gint i;

    param[0] = 0;
    param[1] = 0;

    for (i = 0; i < n_dat; i++)
        param[1] += y[i]/n_dat;
}

/******************* spheastman ********************************/
static gdouble
sphcapeastman_func(gdouble x,
                   G_GNUC_UNUSED gint n_param,
                   const gdouble *b,
                   G_GNUC_UNUSED gpointer user_data,
                   gboolean *fres)
{
    /* xc, R, gamma, theta, d*/
    *fres = TRUE;
    return b[1] - 2*b[3]*G_PI*b[2]*cos(b[4])/(1+(x-b[0])/b[5]);
}

static void
sphcapeastman_guess(gint n_dat,
                    const gdouble *x,
                    const gdouble *y,
                    gdouble *param,
                    gboolean *fres)
{
    gint i;

    param[0] = 0;
    param[1] = 0;

    for (i = 0; i < n_dat; i++)
        param[1] += y[i]/n_dat;

    *fres = TRUE;
}

/******************* sphcapheinz ********************************/
static gdouble
sphcapheinz_func(gdouble x,
                 G_GNUC_UNUSED gint n_param,
                 const gdouble *b,
                 G_GNUC_UNUSED gpointer user_data,
                 gboolean *fres)
{
    /*R, gamma, theta*/
    *fres = TRUE;
    return b[1] - 4*b[2]*G_PI*b[0]*cos(b[3]);
}

static void
sphcapheinz_guess(gint n_dat,
                  const gdouble *x,
                  const gdouble *y,
                  gdouble *param,
                  gboolean *fres)
{
    gint i;

    param[0] = 0;
    param[1] = 0;

    for (i = 0; i < n_dat; i++)
        param[1] += y[i]/n_dat;
}

/******************* sphesheinz ********************************/
static gdouble
sphesheinz_func(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *b,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    /*xc, R, sigma1, sigma2, epsilon, debye, lambda*/
    *fres = TRUE;
    return  b[1] - 4*G_PI*b[2]*b[7]*b[3]*b[4]/b[5]*exp(-(x-b[0])/b[6]) ;
}

static void
sphesheinz_guess(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;

    param[0] = 0;
    param[1] = 0;

    for (i = 0; i < n_dat; i++)
        param[1] += y[i]/n_dat;
}

/******************* hsphhertz ********************************/
static gdouble
hsphhertz_func(gdouble x,
               G_GNUC_UNUSED gint n_param,
               const gdouble *b,
               G_GNUC_UNUSED gpointer user_data,
               gboolean *fres)
{
    /*xc, yc, R, E, nu*/
    *fres = TRUE;
    return  b[1] + (b[0] - x)*sqrt(b[0] - x)*2*b[3]*sqrt(b[2])/(3*(1-b[4]*b[4]));
}

static void
hsphhertz_guess(gint n_dat,
                const gdouble *x,
                const gdouble *y,
                gdouble *param,
                gboolean *fres)
{
    gint i;

    param[0] = 0;
    param[1] = 0;

    for (i = 0; i < n_dat; i++)
        param[1] += y[i]/n_dat;
}
#endif

/************************** presets ****************************/


/*xc, R, H, gamma*/
static const GwyNLFitParam argento_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "H", 1, 1, },
    { "gamma", 0, 0, },
};

/*xc, R, H, gamma, h1, L*/
static const GwyNLFitParam parzanette_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "H", 1, 1, },
    { "gamma", 0, 0, },
    { "h1", 1, 0, },
    { "L", 1, 0, },
};

static const GwyNLFitParam sszanette_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "H", 1, 1, },
};


static const GwyNLFitParam pyrzanette_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "H", 1, 1, },
    { "gamma", 0, 0, },
};

static const GwyNLFitParam tpyrzanette_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "H", 1, 1, },
    { "gamma", 0, 0, },
    { "L", 1, 0, },
};

static const GwyNLFitParam sphcapella_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "H", 1, 1, },
};

static const GwyNLFitParam sphsphcapella_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "R1", 1, 0, },
    { "R2", 1, 0, },
    { "H", 1, 1, },
};

static const GwyNLFitParam conecapella_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "theta", 0, 0, },
    { "H", 1, 1, },
};

static const GwyNLFitParam parcapella_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "l_xy", 1, 0, },
    { "l_z", 1, 0, },
    { "H", 1, 1, },
};



static const GwyNLFitParam sphtiptap_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "H", 1, 1, },
    { "xi", 1, 0, },
};

static const GwyNLFitParam sphxu_params[] = {
    { "xc", 1, 0, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "H", 1, 1, },
    { "sigma", 1, 0, },
};

static const GwyNLFitParam sphcappakarinen_params[] = {
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "gamma", 0, 0, },
    { "theta1", 1, 0, },
    { "theta2", 1, 0, },
};

static const GwyNLFitParam sphcapeastman_params[] = {
    { "xc", 0, 1, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "gamma", 0, 0, },
    { "theta", 1, 0, },
    { "d", 1, 0, },
};

static const GwyNLFitParam sphcapheinz_params[] = {
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "gamma", 0, 0, },
    { "theta", 1, 0, },

};

static const GwyNLFitParam sphesheinz_params[] = {
    { "xc", 0, 1, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "sigma1", 1, 0, },
    { "sigma2", 1, 0, },
    { "epsilon", 1, 0, },
    { "debye", 1, 0, },
    { "lambda", 1, 0, },
};

static const GwyNLFitParam hsphhertz_params[] = {
    { "xc", 0, 1, },
    { "yc", 0, 1, },
    { "R", 1, 0, },
    { "E", 1, 0, },
    { "nu", 1, 0, },
};



static const GwyNLFitPresetBuiltin fitting_presets[] = {
    /*FIXME vdw FDcurve fitting presets start here*/
   /* {
        "vdW: parametric",
        "<i>f</i>(<i>x</i>) "
            "= (<i>HR</i><sup>2</sup>(1 - sin(<i>γ</i>))(<i>R</i>" 
            "sin(<i>γ</i>)  - (<i>x</i>-<i>x<sub>c</sub></i>) sin(<i>γ</i>)"
            " - <i>R</i> - (<i>x</i>-<i>x<sub>c</sub></i>))) "
            "/(6(<i>x</i>-<i>x<sub>c</sub></i>)<sup>2</sup>((<i>x</i>-<i>x<sub>c</sub></i>)"
            " + <i>R</i> - <i>R</i>sin(<i>γ</i>))<sup>2</sup>)"
            " - (<i>H</i>tan(<i>γ</i>)((<i>x</i>-<i>x<sub>c</sub></i>)"
            "sin(<i>γ</i>) + <i>R</i>sin(<i>γ</i>) + <i>R</i>cos(2<i>γ</i>)))"
            ".../(6*cos(<i>γ</i>)*((<i>x</i>-<i>x<sub>c</sub></i>) + <i>R</i>"
            " - <i>R</i>sin(<i>γ</i>))<sup>2</sup>)",
        &argento_func,
        NULL,
        &argento_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(argento_params),
        argento_params,
    },*/
/*    {
        "parzanette",
        "<i>f</i>(<i>x</i>) "
        "=  -(H/6)((h1<sup>2</sup>(3 R (x-xc) + R*h1 - (x-xc) h1))/((x-xc)<sup>2</sup> ((x-xc)+h1)<sup>3</sup>)"
            "+ (L*L)/(((x-xc)+h1)**3) "
            "+ (4*tan(γ))/(3.141593)*(L+tan(γ)*((x-xc)+h1))/(((x-xc)+h1)**2))",
        &parzanette_func,
        NULL,
        &parzanette_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(parzanette_params),
        parzanette_params,
    },*/
    {
        "vdW: semisphere",
        "<i>f</i>(<i>x</i>) "
        "= -<i>H</i>/6 (<i>R</i><sup>3</sup>(<i>R</i>+2(<i>x</i>-<i>x<sub>c</sub></i>)))"
        "/((<i>x</i>-<i>x<sub>c</sub></i>)<sup>2</sup>((<i>x</i>-<i>x<sub>c</sub></i>)"
        " + <i>R</i>)<sup>3</sup>)",
        &sszanette_func,
        NULL,
        &sszanette_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(sszanette_params),
        sszanette_params,
    },
     {
        "vdW: pyramide",
        "<i>f</i>(<i>x</i>) "
        "= -2<i>H</i> (tan(<i>γ</i>)<sup>2</sup>)/3/Pi/(<i>x</i>-<i>x<sub>c</sub></i>) ",
        &pyrzanette_func,
        NULL,
        &pyrzanette_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(pyrzanette_params),
        pyrzanette_params,
    },
/*     {
        "tpyrzanette",
        "<i>f</i>(<i>x</i>) "
        "= -2HL<sup>2</sup>/(x-xc)<sup>3</sup> * (1 + (tan(γ)(x-xc))/L + (tan(γ)(x-xc))<sup>2</sup>)/L<sup>2</sup>)",
        &tpyrzanette_func,
        NULL,
        &tpyrzanette_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(tpyrzanette_params),
        tpyrzanette_params,
    },*/
     {
        "vdW: sphere",
        "<i>f</i>(<i>x</i>) "
        "= -<i>HR</i>/6/(<i>x</i>-<i>x<sub>c</sub></i>)<sup>2</sup> ",
        &sphcapella_func,
        NULL,
        &sphcapella_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(sphcapella_params),
        sphcapella_params,
    },
     {
        "vdW: offset sphere",
        "<i>f</i>(<i>x</i>) "
        "= -<i>HR</i>/6/((<i>x</i>-<i>x<sub>c</sub></i>)-<i>ξ</i>)<sup>2</sup>",
        &sphtiptap_func,
        NULL,
        &sphtiptap_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(sphtiptap_params),
        sphtiptap_params,
    },
     {
        "vdW: two spheres",
        "<i>f</i>(<i>x</i>) "
        "= -<i>HR<sub>1</sub>R<sub>2</sub></i>/6/(<i>x</i>-<i>x<sub>c</sub></i>)(R<sub>1</sub> + R<sub>2</sub>)<sup>2</sup> ",
        &sphsphcapella_func,
        NULL,
        &sphsphcapella_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(sphsphcapella_params),
        sphsphcapella_params,
    },
     {
        "vdW: cone",
        "<i>f</i>(<i>x</i>) "
        "= -<i>H tan<sup>2</sup>(theta)</i>/6/(<i>x</i>-<i>x<sub>c</sub></i>)",
        &conecapella_func,
        NULL,
        &conecapella_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(conecapella_params),
        conecapella_params,
    },
     {
        "vdW: cylinder",
        "<i>f</i>(<i>x</i>) "
        "= -<i>HR<sup>2</sup></i>/6/(<i>x</i>-<i>x<sub>c</sub></i>)<sup>3</sup> ",
        &cylindercapella_func,
        NULL,
        &cylindercapella_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(sphcapella_params),
        sphcapella_params,
    },
     {
        "vdW: paraboloid",
        "<i>f</i>(<i>x</i>) "
        "= -<i>Hl<sub>xy</sub><sup>2</sup></i>/12/(<i>x</i>-<i>x<sub>c</sub></i>)<sup>2</sup> ",
        &parcapella_func,
        NULL,
        &parcapella_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(parcapella_params),
        parcapella_params,
    },
     /*    {
        "vdW: sphere3",
        "<i>f</i>(<i>x</i>) "
        "= -<i>HR</i>/12 (1/(<i>x</i>-<i>x<sub>c</sub></i>)<sup>2</sup>"
        " - 1/15 <i>σ</i><sup>6</sup>/(<i>x</i>-<i>x<sub>c</sub></i>)<sup>8</sup>) ",
        &sphxu_func,
        NULL,
        &sphxu_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(sphxu_params),
        sphxu_params,
    },*/
/*      {
        "sphcappakarinen",
        "<i>f</i>(<i>x</i>) "
        "= -2 γ Pi R (cos(theta1) + cos(theta2))",
        &sphcappakarinen_func,
        NULL,
        &sphcappakarinen_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(sphcappakarinen_params),
        sphcappakarinen_params,
    },
      {
        "sphcapeastman",
        "<i>f</i>(<i>x</i>) "
        "= -2 γ Pi R cos(theta)/(1+(x-xc)/d)",
        &sphcapeastman_func,
        NULL,
        &sphcapeastman_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(sphcapeastman_params),
        sphcapeastman_params,
    },
      {
        "sphcapheinz",
        "<i>f</i>(<i>x</i>) "
        "= -4 γ Pi R cos(theta)",
        &sphcapheinz_func,
        NULL,
        &sphcapheinz_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(sphcapheinz_params),
        sphcapheinz_params,
    },
      {
        "sphesheinz",
        "<i>f</i>(<i>x</i>) "
        "= -4 Pi R lambda σ1 σ2/epsilon exp(-(x-xc)/debye)",
        &sphesheinz_func,
        NULL,
        &sphesheinz_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(sphesheinz_params),
        sphesheinz_params,
    },
     {
        "hsphhertz",
        "<i>f</i>(<i>x</i>) "
        "= (xc - x)<sup>3/2</sup>2 E sqrt(R)/(3 (1-nu<sup>2</sup>))",
        &hsphhertz_func,
        NULL,
        &hsphhertz_guess,
        NULL,
        NULL,
        NULL,
        G_N_ELEMENTS(hsphhertz_params),
        hsphhertz_params,
    },*/
};


static void
gwy_fd_curve_preset_class_init(GwyFDCurvePresetClass *klass)
{
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);

    parent_class = GWY_RESOURCE_CLASS(gwy_fd_curve_preset_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);
    res_class->name = "fdcurvepresets";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    gwy_inventory_forget_order(res_class->inventory);
}

static void
gwy_fd_curve_preset_init(GwyFDCurvePreset *preset)
{
    gwy_debug_objects_creation(G_OBJECT(preset));
}

static GwyFDCurvePreset*
gwy_fd_curve_preset_new_static(const GwyNLFitPresetBuiltin *data)
{
    GwyNLFitPreset *preset;

    preset = g_object_new(GWY_TYPE_FD_CURVE_PRESET, "is-const", TRUE, NULL);
    preset->builtin = data;
    g_string_assign(GWY_RESOURCE(preset)->name, data->name);

    return (GwyFDCurvePreset*)preset;
}

void
_gwy_fd_curve_preset_class_setup_presets(void)
{
    GwyResourceClass *klass;
    GwyFDCurvePreset *preset;
    guint i;

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_FD_CURVE_PRESET);

    for (i = 0; i < G_N_ELEMENTS(fitting_presets); i++) {
        preset = gwy_fd_curve_preset_new_static(fitting_presets + i);
        gwy_inventory_insert_item(klass->inventory, preset);
        g_object_unref(preset);
    }
    gwy_inventory_restore_order(klass->inventory);

    /* The presets added a reference so we can safely unref it again */
    g_type_class_unref(klass);
}

/**
 * gwy_fd_curve_presets:
 *
 * Gets inventory with all the FD curve presets.
 *
 * Returns: FD curve preset inventory.
 *
 * Since: 2.7
 **/
GwyInventory*
gwy_fd_curve_presets(void)
{
    return
    GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_FD_CURVE_PRESET))->inventory;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyfdcurvepreset
 * @title: GwyFDCurvePreset
 * @short_description: Force-distance curve fitting presets
 * @see_also: #GwyNLFitPreset
 *
 * Force-distance curve fitting presets are a particular subtype of non-linear
 * fitting presets.  They have their own class and inventory, but they are
 * functionally identical to #GwyNLFitPreset<!-- -->s.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
