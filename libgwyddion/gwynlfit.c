/*
 *  @(#) $Id$
 *  Copyright (C) 2000-2003 Martin Siler.
 *  Copyright (C) 2004 David Necas (Yeti).
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

#include <string.h>
#include <gwymacros.h>
#include <gwymath.h>
#include "gwynlfit.h"

/* Side step constant for numerical differentiation in gwy_math_nlfit_derive()
 */
#define FitSqrtMachEps  1e-4

/* Konstanta pro rozhodnuti o ukonceni fitovaciho cyklu podle perlativniho rozdilu 
 * rezidualnilnich souctu mezi jednotlivymi kroky
 */
#define EPS 1e-16

/* Lower symmetric part indexing */
/* i MUST be greater or equal than j */
#define SLi(a, i, j) a[(i)*((i) + 1)/2 + (j)]

static gboolean gwy_math_choleski_decompose (gint dim,
                                             gdouble *a);
static void     gwy_math_choleski_solve     (gint dim,
                                             gdouble *a,
                                             gdouble *b);
static gdouble  gwy_math_nlfit_residua      (GwyNLFitter *nlfit,
                                             gint n_dat,
                                             gdouble *x,
                                             gdouble *y,
                                             gdouble *weight,
                                             gint n_par,
                                             gdouble *param,
                                             gpointer user_data,
                                             gdouble *resid);

/* XXX: brain damage:
 * all the functions whose names end with `1' use 1-based arrays since they
 * were originally written in Pascal (!)
 * never expose then to outer world, we must pretend everything is 0-based
 * eventually everything will be actually rewritten to base 0 */
static gboolean gwy_math_sym_matrix_invert1(gint n,
                                            gdouble *a);


/**
 * gwy_math_nlfit_new:
 * @ff: The fitted function.
 * @df: The derivation of fitted function. You can use gwy_math_nlfit_derive()
 *      computing the derivation numerically, when you don't know the
 *      derivation explicitely.
 *
 * Creates a new Marquardt-Levenberg nonlinear fitter for function @ff.
 *
 * Returns: The newly created fitter.
 *
 * Since: 1.1.
 **/
GwyNLFitter*
gwy_math_nlfit_new(GwyNLFitFunc ff, GwyNLFitDerFunc df)
{
    GwyNLFitter *nlfit;

    nlfit = g_new0(GwyNLFitter, 1);
    nlfit->fmarq = ff;
    nlfit->dmarq = df;
    nlfit->mfi = 1.0;
    nlfit->mdec = 0.4;
    nlfit->minc = 10.0;
    nlfit->mtol = 1e-6;
    nlfit->maxiter = 50;
    nlfit->eval = FALSE;
    nlfit->dispersion = -1;
    nlfit->covar = NULL;

    return nlfit;
}

/**
 * gwy_math_nlfit_free:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 *
 * Completely frees a Marquardt-Levenberg nonlinear fitter.
 *
 * Since: 1.1.
 **/
void
gwy_math_nlfit_free(GwyNLFitter *nlfit)
{
    g_free(nlfit->covar);
    nlfit->covar = NULL;
    g_free(nlfit);
}

/**
 * gwy_math_nlfit_fit:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @n_dat: The number of data points in @x, @y, @w.
 * @x: Array of independent variable values.
 * @y: Array of dependent variable values.
 * @weight: Array of weights associated to each data point.
 * @n_par: The nuber of parameters.
 * @param: Array of parameters (of size @n_par).  Note the parameters must
 *         be initialized to reasonably near values.
 * @user_data: Any pointer that will be passed to the function and derivation
 *             as @user_data.
 *
 * Performs a nonlinear fit of @nlfit function on (@x,@y) data.
 *
 * Returns: The final residual sum, a negative number in the case of failure.
 *
 * Since: 1.1.
 **/
gdouble
gwy_math_nlfit_fit(GwyNLFitter *nlfit,
                   gint n_dat,
                   gdouble *x,
                   gdouble *y,
                   gdouble *weight,
                   gint n_par,
                   gdouble *param,
                   gpointer user_data)
{

    gdouble mlambda = 1e-4;
    gdouble sumr = G_MAXDOUBLE;
    gdouble sumr1;
    gdouble *der;
    gdouble *v;
    gdouble *xr;
    gdouble *saveparam;
    gdouble *resid;
    gdouble *a;
    gdouble *save_a;
    gint covar_size = n_par*(n_par + 1)/2;
    gint i, j, k;
    gint miter = 0;
    gboolean step1 = TRUE;
    gboolean end = FALSE;

    g_free(nlfit->covar);
    nlfit->covar = NULL;

    resid = g_new(gdouble, n_dat);
    sumr1 = gwy_math_nlfit_residua(nlfit, n_dat, x, y, weight,
                                   n_par, param, user_data, resid);
    sumr = sumr1;

    if (!nlfit->eval) {
        g_warning("Initial residua evaluation failed");
        g_free(resid);
        return -1;
    }

    der = g_new(gdouble, n_par);
    v = g_new(gdouble, n_par);
    xr = g_new(gdouble, n_par);
    saveparam = g_new(gdouble, n_par);
    a = g_new(gdouble, covar_size);
    save_a = g_new(gdouble, covar_size);

    do {
        gboolean is_pos_def = FALSE;
        gboolean first_pass = TRUE;
        gint count = 0;

        if (step1) {
            mlambda *= nlfit->mdec;
            sumr = sumr1;

            for (i = 0; i < covar_size; i++)
                a[i] = 0;
            for (i = 0; i < n_par; i++)
                v[i] = 0;

            /* J'J and J'r computation */
            for (i = 0; i < n_dat; i++) {
                nlfit->dmarq(i, x, n_par, param, nlfit->fmarq,
                            user_data, der, &nlfit->eval);
                if (!nlfit->eval)
                    break;
                for (j = 0; j < n_par; j++) {
                    gint diag = j*(j + 1)/2;

                    /* for J'r */
                    v[j] += weight[i]* der[j] * resid[i];
                    for (k = 0; k <= j; k++)    /* for J'J */
                        a[diag + k] += weight[i] * der[j] * der[k];
                }
            }
            if (nlfit->eval) {
                memcpy(save_a, a, covar_size*sizeof(gdouble));
                memcpy(saveparam, param, n_par*sizeof(gdouble));
            }
            else
                break;
        }
        while (!is_pos_def) {
            if (!first_pass)
                memcpy(a, save_a, covar_size*sizeof(gdouble));
            else
                first_pass = FALSE;

            for (j = 0; j < n_par; j++) {
                /* Doplneni diagonalnich prvku */
                gint diag = j*(j + 3)/2;

                a[diag] = save_a[diag]*(1.0 + mlambda) + nlfit->mfi*mlambda;
                xr[j] = -v[j];
            }
            /* Choleskeho rozklad J'J v A*/
            is_pos_def = gwy_math_choleski_decompose(n_par, a);
            if (!is_pos_def) {
                /* Provede zvetseni "lambda"*/
                mlambda *= nlfit->minc;
                if (mlambda == 0.0)
                    mlambda = nlfit->mtol;
            }
        }
        gwy_math_choleski_solve(n_par, a, xr);

        for (i = 0; i < n_par; i++) {
            param[i] = saveparam[i] + xr[i];
            if (fabs(param[i] - saveparam[i]) == 0)
                count++;
        }
        if (count == n_par)
            break;

        sumr1 = gwy_math_nlfit_residua(nlfit, n_dat,
                                        x, y, weight,
                                        n_par, param,
                                        user_data, resid);
        if ((sumr1 == 0)
            || (miter > 2 && fabs((sumr - sumr1)/sumr1) < EPS))
            end = TRUE;
        if (!nlfit->eval || sumr1 >= sumr) {
            /* Provede zvetseni "lambda"*/
            mlambda *= nlfit->minc;
            if (mlambda == 0.0)
                mlambda = nlfit->mtol;
            step1 = FALSE;
        }
        else
            step1 = TRUE;

        if (++miter >= nlfit->maxiter)
            break;
    } while (!end);

    sumr1 = sumr;

    if (nlfit->eval) {
        if (gwy_math_sym_matrix_invert1(n_par, save_a-1)) {
            nlfit->covar = g_memdup(save_a, covar_size*sizeof(gdouble));
            nlfit->dispersion = sumr/(n_dat - n_par);
        }
        /* XXX: else what? */
    }

    g_free(save_a);
    g_free(a);
    g_free(resid);
    g_free(saveparam);
    g_free(xr);
    g_free(v);
    g_free(der);

    return sumr;
}


/**
 * gwy_math_nlfit_derive:
 * @i: Index in @data_x where to compute the derivation.
 * @x: x-data as passed to gwy_math_nlfit_fit().
 * @n_par: The nuber of parameters.
 * @param: Array of parameters (of size @n_par).
 * @ff: The fitted function.
 * @user_data: User data as passed to gwy_math_nlfit_fit().
 * @deriv: Array where the put the result to.
 * @dres: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Numerically computes the partial derivations of @ff
 *
 * Since: 1.1.
 **/
void
gwy_math_nlfit_derive(gint i,
                      gdouble *x,
                      gint n_par,
                      gdouble *param,
                      GwyNLFitFunc ff,
                      gpointer user_data,
                      gdouble *deriv,
                      gboolean *dres)
{
    gdouble save_par_j, hj;
    gdouble left, right;
    gint j;

    for (j = 0; j < n_par; j++) {
        hj = (fabs(param[j]) + FitSqrtMachEps) * FitSqrtMachEps;
        save_par_j = param[j];
        param[j] -= hj;
        left = ff(x[i], n_par, param, user_data, dres);
        if (!dres) {
            param[j] = save_par_j;
            return;
        }

        param[j] += 2 * hj;
        right = ff(x[i], n_par, param, user_data, dres);
        if (!dres) {
            param[j] = save_par_j;
            return;
        }

        deriv[j] = (right - left)/2/hj;
        param[j] = save_par_j;
    }
}

static gdouble
gwy_math_nlfit_residua(GwyNLFitter *nlfit,
                       gint n_dat,
                       gdouble *x,
                       gdouble *y,
                       gdouble *weight,
                       gint n_par,
                       gdouble *param,
                       gpointer user_data,
                       gdouble *resid)
{
    gdouble s = 0;
    gint i;

    nlfit->eval = TRUE;
    for (i = 0; i < n_dat && nlfit->eval; i++) {
        resid[i] = nlfit->fmarq(x[i], n_par, param, user_data, &nlfit->eval)
                   - y[i];
        s += resid[i] * resid[i] * weight[i];
    }
    return s;
}

/**
 * gwy_math_nlfit_get_max_iterations:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 *
 * Returns the maximum number of iterations of nonlinear fitter @nlfit.
 *
 * Returns: The maximum number of iterations.
 *
 * Since: 1.1.
 **/
gint
gwy_math_nlfit_get_max_iterations(GwyNLFitter *nlfit)
{
    return nlfit->maxiter;
}

/**
 * gwy_math_nlfit_set_max_iterations:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @maxiter: The maximum number of iterations.
 *
 * Sets the maximum number of iterations for nonlinear fitter @nlfit.
 *
 * Since: 1.1.
 **/
void
gwy_math_nlfit_set_max_iterations(GwyNLFitter *nlfit,
                                  gint maxiter)
{
    g_return_if_fail(maxiter > 0);
    nlfit->maxiter = maxiter;
}


/**
 * gwy_math_nlfit_get_sigma:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @par: Parameter index.
 *
 * Returns the standard deviation of parameter number @par.
 *
 * This function makes sense only after a successful fit.
 *
 * Returns: The SD of @par-th parameter.
 *
 * Since: 1.1.
 **/
gdouble
gwy_math_nlfit_get_sigma(GwyNLFitter *nlfit, gint par)
{
    g_return_val_if_fail(nlfit->covar, G_MAXDOUBLE);

    return sqrt(nlfit->dispersion * SLi(nlfit->covar, par, par));
}

/**
 * gwy_math_nlfit_get_dispersion:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 *
 * Returns the residual sum divided by the number of degrees of freedom.
 *
 * This function makes sense only after a successful fit.
 *
 * Returns: The dispersion.
 *
 * Since: 1.1.
 **/
gdouble
gwy_math_nlfit_get_dispersion(GwyNLFitter *nlfit)
{
    g_return_val_if_fail(nlfit->covar, G_MAXDOUBLE);
    return nlfit->dispersion;
}

/**
 * gwy_math_nlfit_get_correlations:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @par1: First parameter index.
 * @par2: Second parameter index.
 *
 * Returns the correlation coefficient between @par1-th and @par2-th parameter.
 *
 * This function makes sense only after a successful fit.
 *
 * Returns: The correlation coefficient.
 *
 * Since: 1.1.
 **/
gdouble
gwy_math_nlfit_get_correlations(GwyNLFitter *nlfit, gint par1, gint par2)
{
    gdouble Pom;

    g_return_val_if_fail(nlfit->covar, G_MAXDOUBLE);

    if (par1 == par2)
        return 1.0;
    if (par1 < par2)
        GWY_SWAP(gint, par1, par2);

    Pom = SLi(nlfit->covar, par1, par1) * SLi(nlfit->covar, par2, par2);
    if (Pom == 0) {
        g_warning("Zero element in covar matrix");
        return G_MAXDOUBLE;
    }

    return SLi(nlfit->covar, par1, par2)/sqrt(Pom);
}


/**
 * gwy_math_choleski_decompose:
 * @dim: The dimension of @a.
 * @a: Lower triangular part of a symmetrix matric, stored by rows, i.e.,
 *     a = [a_00 a_10 a_11 a_20 a_21 a_22 a_30 ...].
 *
 * Decomposes a symmetric positive definite matrix in place.
 *
 * Returns: Whether the matrix was really positive definite.  If %FALSE,
 *          the decomposition failed and @a contains garbage.
 **/
static gboolean
gwy_math_choleski_decompose(gint dim, gdouble *a)
{
    gint i, j, k;
    gdouble s, r;

    for (k = 0; k < dim; k++) {
        /* diagonal element */
        s = SLi(a, k, k);
        for (i = 0; i < k; i++)
            s -= SLi(a, k, i) * SLi(a, k, i);
        if (s <= 0.0)
            return FALSE;
        SLi(a, k, k) = s = sqrt(s);

        /* nondiagonal elements */
        for (j = k+1; j < dim; j++) {
            r = SLi(a, j, k);
            for (i = 0; i < k; i++)
                r -= SLi(a, k, i) * SLi(a, j, i);
            SLi(a, j, k) = r/s;
        }
    }

    return TRUE;
}

/**
 * gwy_math_choleski_solve:
 * @dim: The dimension of @a.
 * @a: Lower triangular part of Choleski decomposition as computed
 *     by gwy_math_choleski_decompose().
 * @b: Right hand side vector.  Is is modified in place, on return it contains
 *     the solution.
 *
 * Solves a system of linear equations with predecomposed symmetric positive
 * definite matrix @a and right hand side @b.
 **/
static void
gwy_math_choleski_solve(gint dim, gdouble *a, gdouble *b)
{
    gint i, j;

    /* back-substitution with the lower triangular matrix */
    for (j = 0; j < dim; j++) {
        for (i = 0; i < j; i++)
            b[j] -= SLi(a, j, i)*b[i];
        b[j] /= SLi(a, j, j);
    }

    /* back-substitution with the upper triangular matrix */
    for (j = dim-1; j >= 0; j--) {
        for (i = j+1; i < dim; i++)
            b[j] -= SLi(a, i, j)*b[i];
        b[j] /= SLi(a, j, j);
    }
}

/* inverze symetricke matice */
static gboolean
gwy_math_sym_matrix_invert1(gint n, gdouble *a)
{

    gint q = 0, m;
    gdouble s, t;
    gdouble *x;
    gint k, i, j;

    x = g_new(gdouble, n*(n + 1)/2 + 2);
    for (k = n; k >= 1; k--) {
        s = a[1];
        if (s <= 0) {
            g_free(x);
            return FALSE;
        }
        m = 1;
        for (i = 2; i <= n; i++) {
            q = m;
            m += i;
            t = a[q + 1];
            x[i] = -t/s;      /* note use temporary x */
            if (i > k)
                x[i] = -x[i];
            for (j = q + 2; j <= m; j++)
                a[j - i] = a[j] + t * x[j - q];
        }
        q--;
        a[m] = 1.0/s;
        for (i = 2; i <= n; i++)
            a[q + i] = x[i];
    }
    g_free(x);

    return TRUE;
}


static gdouble
fit_gauss(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
        gdouble c;

            if (b[3] == 0) {
            *fres = FALSE;
            return 0;
       }
       *fres = TRUE;
       c = (x - b[0])/b[3];

       return b[2] * exp(-c*c/2) + b[1];
}

static gdouble
fit_gauss_psdf(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
        gdouble c;

            if (b[3] == 0) {
            *fres = FALSE;
            return 0;
       }
       *fres = TRUE;
       c = x/b[1];

       return b[0]*b[0]*b[1]/(2.0*sqrt(G_PI)) * exp(-c*c/4);
}

static gdouble
fit_gauss_hhcf(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
        gdouble c;

            if (b[1] == 0) {
            *fres = FALSE;
            return 0;
       }
       *fres = TRUE;
       c = x/b[1];

       return 2*b[0]*b[0] * (1 - exp(-c*c));
}

static gdouble
fit_gauss_acf(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
        gdouble c;

            if (b[1] == 0) {
            *fres = FALSE;
            return 0;
       }
       *fres = TRUE;
       c = x/b[1];

       return b[0]*b[0] * exp(-c*c);
}


static gdouble
fit_exp(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
        gdouble c;

            if (b[3] == 0) {
            *fres = FALSE;
            return 0;
       }
       *fres = TRUE;
       c = (x - b[0])/b[3];

       return b[2] * exp(-c/2) + b[1];
}

static gdouble
fit_exp_psdf(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
        gdouble c;

            if (b[3] == 0) {
            *fres = FALSE;
            return 0;
       }
       *fres = TRUE;
       c = x*b[1];

       return b[0]*b[0]*b[1]/(2.0*sqrt(G_PI)) / (1 + c*c);
}

static gdouble
fit_exp_hhcf(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
        gdouble c;

            if (b[1] == 0) {
            *fres = FALSE;
            return 0;
       }
       *fres = TRUE;
       c = x/b[1];

       return 2*b[0]*b[0] * (1 - exp(-c));
}

static gdouble
fit_exp_acf(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
        gdouble c;

            if (b[1] == 0) {
            *fres = FALSE;
            return 0;
       }
       *fres = TRUE;
       c = x/b[1];

       return b[0]*b[0] * exp(-c);
}

static gdouble
fit_poly_0(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
    return b[0];
}


static gdouble
fit_poly_1(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
    return b[0] + b[1]*x;
}

static gdouble
fit_poly_2(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
    return b[0] + b[1]*x + b[2]*x*x;
}

static gdouble
fit_poly_3(gdouble x, G_GNUC_UNUSED gint n_par, gdouble *b,
            G_GNUC_UNUSED gpointer user_data, gboolean *fres)
{
    return b[0] + b[1]*x + b[2]*x*x + b[3]*x*x*x;
}


static const Param gaussexp_pars[]= {
   {"x<sub>0</sub>", " ", 1 },
   {"y<sub>0</sub>", " ", 2 },
   {"a", " ", 3 },
   {"b", " ", 4 },
};

static const Param gaussexp_two_pars[]= {
   {"\xcf\x83", " ", 1 },
   {"T", " ", 2 },
};


static const Param poly0_pars[]= {
   {"a", " ", 1 },
};

static const Param poly1_pars[]= {
   {"a", " ", 1 },
   {"b", " ", 2 },
};


static const GwyNLFitPresetFunction fitting_presets[] = {
    { "Gaussian", 
       "f(x) = y<sub>0</sub> + a*exp(-(b*(x-x<sub>0</sub>))<sup>2</sup>)",
       &fit_gauss,
       NULL,
       NULL,
       4,
       gaussexp_pars
    },
    { "Gaussian (PSDF)", 
       "f(x) = (\xcf\x83<sup>2</sup>T)/(2)*exp(-(b*(x-x<sub>0</sub>))<sup>2</sup>)",
       &fit_gauss,
       NULL,
       NULL,
       2,
       gaussexp_two_pars
    },
    { "Gaussian (ACF)", 
       "f(x) = y<sub>0</sub> + a*exp(-(b*(x-x<sub>0</sub>))<sup>2</sup>)",
       &fit_gauss,
       NULL,
       NULL,
       2,
       gaussexp_two_pars
    },
    { "Gaussian (HHCF)", 
       "f(x) = y<sub>0</sub> + a*exp(-(b*(x-x<sub>0</sub>))<sup>2</sup>)",
       &fit_gauss,
       NULL,
       NULL,
       2,
       gaussexp_two_pars
    },
    { "Exponential", 
       "f(x) = y<sub>0</sub> + a*exp(-(b*(x-x<sub>0</sub>)))",
       &fit_exp,
       NULL,
       NULL,
       4,
       gaussexp_pars
    },
    { "Polynom (order 0)", 
       "f(x) = a",
       &fit_poly_0,
       NULL,
       NULL,
       1,
       poly0_pars    
    },
    { "Polynom (order 1)", 
       "f(x) = a + b*x",
       &fit_poly_1,
       NULL,
       NULL,
       2,
       poly1_pars
    }
};

GwyNLFitPresetFunction* gwy_math_nlfit_get_preset(GwyNLFitPresetType type)
{

    return (fitting_presets + type);
}

gdouble gwy_math_nlfit_get_function_value(GwyNLFitPresetFunction* function, gdouble *params, gdouble x)
{
    gboolean res;
    return (function->function)(x, function->nparams, params, NULL, &res);
}

gchar *gwy_math_nlfit_get_function_name(GwyNLFitPresetFunction* function)
{
    return g_strdup(function->function_name);
}

gchar *gwy_math_nlfit_get_function_equation(GwyNLFitPresetFunction* function)
{
    return g_strdup(function->function_equation);
}

gchar *gwy_math_nlfit_get_function_param_name(GwyNLFitPresetFunction* function, gint param)
{
    Param *par;
    par = function->param + param;
    return g_strdup(par->name);
}

gdouble gwy_math_nlfit_get_function_param_default(GwyNLFitPresetFunction* function, gint param)
{
    Param *par; 
    par = function->param + param;        
    return par->default_init;
}

gint gwy_math_nlfit_get_function_nparams(GwyNLFitPresetFunction* function)
{
    return function->nparams;
}


/************************** Documentation ****************************/

/**
 * GwyNLFitFunc:
 * @x: The value to compute the function in.
 * @n_par: The number of parameters (size of @param).
 * @param: Parameters.
 * @user_data: User data as passed to gwy_math_nlfit_fit().
 * @fres: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Fitting function type.
 *
 * Returns: The value at @x.
 */

/**
 * GwyNLFitDerFunc:
 * @i: The index of the value in @x to compute the derivation in.
 * @x: x-data as passed to gwy_math_nlfit_fit().
 * @n_par: The number of parameters (size of @param).
 * @param: Parameters.
 * @deriv: Array where the @n_par partial derivations by each parameter are
 *         to be stored.
 * @fmarq: The fitting function.
 * @user_data: User data as passed to gwy_math_nlfit_fit().
 * @dres: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Fitting function partial derivation type.
 */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
