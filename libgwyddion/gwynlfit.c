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

#include <string.h>
#include "gwymacros.h"
#include "gwymath.h"
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

/* Preset */
struct _GwyNLFitParam {
    const char *name;
    const char *unit;
    double default_init;
};

static gboolean gwy_math_choleski_decompose (gint dim,
                                             gdouble *a);
static void     gwy_math_choleski_solve     (gint dim,
                                             gdouble *a,
                                             gdouble *b);
static gdouble  gwy_math_nlfit_residua      (GwyNLFitter *nlfit,
                                             gint n_dat,
                                             const gdouble *x,
                                             const gdouble *y,
                                             const gdouble *weight,
                                             gint n_param,
                                             const gdouble *param,
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
 * @weight: Array of weights associated to each data point.  Since 1.2 it can
 *          be %NULL, weight of 1 is then used for all data.
 * @n_param: The nuber of parameters.
 * @param: Array of parameters (of size @n_param).  Note the parameters must
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
                   const gdouble *x,
                   const gdouble *y,
                   const gdouble *weight,
                   gint n_param,
                   gdouble *param,
                   gpointer user_data)
{
    /* the compiler would initialize it with zeroes, but this looks better... */
    static const gboolean fixed_fixed[] = {
        FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
    };
    gboolean *fixed;
    gdouble residua;

    g_return_val_if_fail(n_param > 0, -1.0);
    if ((guint)n_param < G_N_ELEMENTS(fixed_fixed))
        return gwy_math_nlfit_fit_with_fixed(nlfit, n_dat, x, y, weight,
                                             n_param, param, fixed_fixed,
                                             user_data);

    fixed = g_new0(gboolean, n_param);
    residua = gwy_math_nlfit_fit_with_fixed(nlfit, n_dat, x, y, weight,
                                            n_param, param, fixed,
                                            user_data);
    g_free(fixed);

    return residua;
}

/**
 * gwy_math_nlfit_fit_with_fixed:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @n_dat: The number of data points in @x, @y, @w.
 * @x: Array of independent variable values.
 * @y: Array of dependent variable values.
 * @weight: Array of weights associated to each data point.  Can be %NULL,
 *          weight of 1 is then used for all data.
 * @n_param: The nuber of parameters.
 * @param: Array of parameters (of size @n_param).  Note the parameters must
 *         be initialized to reasonably near values.
 * @fixed_param: Which parameters should be treated as fixed (set corresponding
 *               element to %TRUE for them).
 * @user_data: Any pointer that will be passed to the function and derivation
 *
 * Performs a nonlinear fit of @nlfit function on (@x,@y) data, allowing
 * some fixed parameters.
 *
 * Returns: The final residual sum, a negative number in the case of failure.
 *
 * Since: 1.2.
 **/
gdouble
gwy_math_nlfit_fit_with_fixed(GwyNLFitter *nlfit,
                              gint n_dat,
                              const gdouble *x,
                              const gdouble *y,
                              const gdouble *weight,
                              gint n_param,
                              gdouble *param,
                              const gboolean *fixed_param,
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
    gdouble *w = NULL;
    gdouble *save_a;
    gint *var_param_id;
    gint covar_size;
    gint i, j, k;
    gint n_var_param;
    gint miter = 0;
    gboolean step1 = TRUE;
    gboolean end = FALSE;

    g_return_val_if_fail(nlfit, -1.0);
    g_return_val_if_fail(n_param > 0, -1.0);
    g_return_val_if_fail(n_dat > n_param, -1.0);
    g_return_val_if_fail(x && y && param && fixed_param, -1.0);

    g_free(nlfit->covar);
    nlfit->covar = NULL;

    if (!weight) {
        w = g_new(gdouble, n_dat);
        for (i = 0; i < n_dat; i++)
            w[i] = 1.0;
        weight = w;
    }

    resid = g_new(gdouble, n_dat);
    sumr1 = gwy_math_nlfit_residua(nlfit, n_dat, x, y, weight,
                                   n_param, param, user_data, resid);
    sumr = sumr1;

    if (!nlfit->eval) {
        g_warning("Initial residua evaluation failed");
        g_free(w);
        g_free(resid);
        return -1;
    }

    /* find non-fixed parameters and map all -> non-fixed */
    n_var_param = 0;
    var_param_id = g_new(gint, n_param);
    for (i = 0; i < n_param; i++)
        var_param_id[i] = fixed_param[i] ? -1 : n_var_param++;

    if (!n_var_param) {
        g_free(w);
        g_free(var_param_id);
        g_free(resid);
        return sumr;
    }
    covar_size = n_var_param*(n_var_param + 1)/2;

    der = g_new(gdouble, n_param);  /* because ->dmarq() computes all */
    v = g_new(gdouble, n_var_param);
    xr = g_new(gdouble, n_var_param);
    saveparam = g_new(gdouble, n_param);
    a = g_new(gdouble, covar_size);
    save_a = g_new(gdouble, covar_size);

    do {
        gboolean is_pos_def = FALSE;
        gboolean first_pass = TRUE;
        gint count = 0;

        if (step1) {
            mlambda *= nlfit->mdec;
            sumr = sumr1;

            memset(a, 0, covar_size*sizeof(gdouble));
            memset(v, 0, n_var_param*sizeof(gdouble));

            /* J'J and J'r computation */
            for (i = 0; i < n_dat; i++) {
                /* FIXME: this unnecessarilly computes all derivations */
                nlfit->dmarq(i, x, n_param, param, nlfit->fmarq,
                             user_data, der, &nlfit->eval);
                if (!nlfit->eval)
                    break;
                for (j = 0; j < n_param; j++) {
                    gint jid, diag;

                    if ((jid = var_param_id[j]) < 0)
                        continue;
                    diag = jid*(jid + 1)/2;

                    /* for J'r */
                    v[jid] += weight[i] * der[j] * resid[i];
                    for (k = 0; k <= j; k++) {   /* for J'J */
                        gint kid = var_param_id[k];

                        if (kid >= 0)
                            a[diag + kid] += weight[i] * der[j] * der[k];
                    }
                }
            }
            if (nlfit->eval) {
                memcpy(save_a, a, covar_size*sizeof(gdouble));
                memcpy(saveparam, param, n_param*sizeof(gdouble));
            }
            else
                break;
        }
        while (!is_pos_def) {
            if (!first_pass)
                memcpy(a, save_a, covar_size*sizeof(gdouble));
            else
                first_pass = FALSE;

            for (j = 0; j < n_var_param; j++) {
                /* Add diagonal elements */
                gint diag = j*(j + 3)/2;

                a[diag] = save_a[diag]*(1.0 + mlambda) + nlfit->mfi*mlambda;
                xr[j] = -v[j];
            }
            /* Choleski decompoation J'J in A*/
            is_pos_def = gwy_math_choleski_decompose(n_var_param, a);
            if (!is_pos_def) {
                /* Increase lambda */
                mlambda *= nlfit->minc;
                if (mlambda == 0.0)
                    mlambda = nlfit->mtol;
            }
        }
        gwy_math_choleski_solve(n_var_param, a, xr);

        for (i = 0; i < n_param; i++) {
            if (var_param_id[i] < 0)
                continue;
            param[i] = saveparam[i] + xr[i];
            if (fabs(param[i] - saveparam[i]) == 0)
                count++;
        }
        if (count == n_var_param)
            break;

        sumr1 = gwy_math_nlfit_residua(nlfit, n_dat,
                                       x, y, weight,
                                       n_param, param,
                                       user_data, resid);
        if ((sumr1 == 0)
            || (miter > 2 && fabs((sumr - sumr1)/sumr1) < EPS))
            end = TRUE;
        if (!nlfit->eval || sumr1 >= sumr) {
            /* Increase lambda */
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
        if (gwy_math_sym_matrix_invert1(n_var_param, save_a-1)) {
            /* stretch the matrix to span over fixed params too */
            nlfit->covar = g_new(gdouble, n_param*(n_param + 1)/2);
            for (i = 0; i < n_param; i++) {
                gint iid = var_param_id[i];

                for (j = 0; j < i; j++) {
                    gint jid = var_param_id[j];

                    if (iid < 0 || jid < 0)
                        SLi(nlfit->covar, i, j) = 0.0;
                    else
                        SLi(nlfit->covar, i, j) = SLi(save_a, iid, jid);
                }
                if (iid < 0)
                    SLi(nlfit->covar, i, j) = 1.0;
                else
                    SLi(nlfit->covar, i, i) = SLi(save_a, iid, iid);
            }
            nlfit->dispersion = sumr/(n_dat - n_var_param);
        }
        else
            /* XXX: else what? */
            g_warning("Cannot invert covariance matrix");
    }

    g_free(save_a);
    g_free(a);
    g_free(saveparam);
    g_free(xr);
    g_free(v);
    g_free(der);
    g_free(var_param_id);
    g_free(resid);
    g_free(w);

    return sumr;
}


/**
 * gwy_math_nlfit_derive:
 * @i: Index in @data_x where to compute the derivation.
 * @x: x-data as passed to gwy_math_nlfit_fit().
 * @n_param: The nuber of parameters.
 * @param: Array of parameters (of size @n_param).
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
                      const gdouble *x,
                      gint n_param,
                      gdouble *param,
                      GwyNLFitFunc ff,
                      gpointer user_data,
                      gdouble *deriv,
                      gboolean *dres)
{
    gdouble save_par_j, hj;
    gdouble left, right;
    gint j;

    for (j = 0; j < n_param; j++) {
        hj = (fabs(param[j]) + FitSqrtMachEps) * FitSqrtMachEps;
        save_par_j = param[j];
        param[j] -= hj;
        left = ff(x[i], n_param, param, user_data, dres);
        if (!dres) {
            param[j] = save_par_j;
            return;
        }

        param[j] += 2 * hj;
        right = ff(x[i], n_param, param, user_data, dres);
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
                       const gdouble *x,
                       const gdouble *y,
                       const gdouble *weight,
                       gint n_param,
                       const gdouble *param,
                       gpointer user_data,
                       gdouble *resid)
{
    gdouble s = 0;
    gint i;

    nlfit->eval = TRUE;
    for (i = 0; i < n_dat && nlfit->eval; i++) {
        resid[i] = nlfit->fmarq(x[i], n_param, param, user_data, &nlfit->eval)
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
/*********************** gaussian *****************************/
static gdouble
fit_gauss(gdouble x,
          G_GNUC_UNUSED gint n_param,
          const gdouble *b,
          G_GNUC_UNUSED gpointer user_data,
          gboolean *fres)
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

static void
guess_gauss(gdouble *x,
            gdouble *y,
            gint n_dat,
            gdouble *param,
            G_GNUC_UNUSED gpointer user_data,
            gboolean *fres)
{
    gint i;

    param[0] = 0;
    param[1] = G_MAXDOUBLE;
    param[2] = -G_MAXDOUBLE;
    for (i = 0; i < n_dat; i++) {
        param[0] += x[i]/(gdouble)n_dat;
        if (param[1] > y[i])
            param[1] = y[i];
        if (param[2] < y[i])
            param[2] = y[i];
    }
    param[2] -= param[1];

    param[3] = (x[n_dat-1] - x[0])/4;

    *fres = TRUE;
}

static void
scale_gauss(gdouble *param,
            gdouble xscale,
            gdouble yscale,
            gint dir)
{
    if (dir == 1)
    {
        param[0] /= xscale;
        param[1] /= yscale;
        param[2] /= yscale;
        param[3] /= xscale;

    }
    else
    {
        param[0] *= xscale;
        param[1] *= yscale;
        param[2] *= yscale;
        param[3] *= xscale;
    }

}

/******************** gaussian PSDF ***************************/
static gdouble
fit_gauss_psdf(gdouble x,
               G_GNUC_UNUSED gint n_param,
               const gdouble *b,
               G_GNUC_UNUSED gpointer user_data,
               gboolean *fres)
{
    gdouble c;

    if (b[1] == 0) {
        *fres = FALSE;
        return 0;
    }
    *fres = TRUE;
    c = x*b[1];

    return b[0]*b[0]*b[1]/(2.0*sqrt(G_PI)) * exp(-c*c/4);
}

static void
scale_gauss_psdf(gdouble *param,
                 gdouble xscale,
                 gdouble yscale,
                 gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale*xscale);
        param[1] *= xscale;
    }
    else {
        param[0] *= sqrt(yscale*xscale);
        param[1] /= xscale;
    }
}

static void
guess_gauss_psdf(gdouble *x,
                 gdouble *y,
                 gint n_dat,
                 gdouble *param,
                 G_GNUC_UNUSED gpointer user_data,
                 gboolean *fres)
{
    gint i;

    param[1] = 50/x[n_dat-1];

    param[0] = 0;
    for (i = 0; i < n_dat; i++)
        param[0] += x[1]*y[i];

    *fres = param[0] >= 0;
    param[0] = sqrt(param[0]);
}


/******************* gaussian HHCF ********************************/
static gdouble
fit_gauss_hhcf(gdouble x,
               G_GNUC_UNUSED gint n_param,
               const gdouble *b,
               G_GNUC_UNUSED gpointer user_data,
               gboolean *fres)
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

static void
guess_gauss_hhcf(gdouble *x,
                 gdouble *y,
                 gint n_dat,
                 gdouble *param,
                 G_GNUC_UNUSED gpointer user_data,
                 gboolean *fres)
{
    gint i;

    param[0] = 0;
    for (i = (n_dat/2); i < n_dat; i++)
    {
        param[0] += y[i]/(n_dat/2);
    }
    param[0] = sqrt(param[0]);
    param[1] = x[n_dat-1]/50;

    *fres = TRUE;
}

static void
scale_gauss_hhcf(gdouble *param,
                 gdouble xscale,
                 gdouble yscale,
                 gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale);
        param[1] /= xscale;
    }
    else {
        param[0] *= sqrt(yscale);
        param[1] *= xscale;
    }
}

/****************    gaussian ACF  *****************************************/
static gdouble
fit_gauss_acf(gdouble x,
              G_GNUC_UNUSED gint n_param,
              const gdouble *b,
              G_GNUC_UNUSED gpointer user_data,
              gboolean *fres)
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

static void
guess_gauss_acf(gdouble *x,
                gdouble *y,
                gint n_dat,
                gdouble *param,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    param[0] = sqrt(y[1]);
    param[1] = x[n_dat-1]/50;

    *fres = y[1] >= 0;
}

static void
scale_gauss_acf(gdouble *param,
                gdouble xscale,
                gdouble yscale,
                gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale);
        param[1] /= xscale;
    }
    else {
        param[0] *= sqrt(yscale);
        param[1] *= xscale;
    }
}


/**************** exponential ************************************/
static gdouble
fit_exp(gdouble x,
        G_GNUC_UNUSED gint n_param,
        const gdouble *b,
        G_GNUC_UNUSED gpointer user_data,
        gboolean *fres)
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

static void
guess_exp(gdouble *x,
          gdouble *y,
          gint n_dat,
          gdouble *param,
          G_GNUC_UNUSED gpointer user_data,
          gboolean *fres)
{
    gint i;

    param[0] = 0;
    param[1] = G_MAXDOUBLE;
    param[2] = -G_MAXDOUBLE;
    for (i = 0; i < n_dat; i++) {
        param[0] += x[i]/(gdouble)n_dat;
        if (param[1] > y[i])
            param[1] = y[i];
        if (param[2] < y[i])
            param[2] = y[i];
    }
    param[2] -= param[1];

    param[3] = (x[n_dat-1] - x[0])/4;

    *fres = TRUE;
}

static void
scale_exp(gdouble *param,
          gdouble xscale,
          gdouble yscale,
          gint dir)
{
    if (dir == 1) {
        param[0] /= xscale;
        param[1] /= yscale;
        param[2] /= yscale;
        param[3] /= xscale;

    }
    else {
        param[0] *= xscale;
        param[1] *= yscale;
        param[2] *= yscale;
        param[3] *= xscale;
    }

}

/**************** exponential PSDF **************************/
static gdouble
fit_exp_psdf(gdouble x,
             G_GNUC_UNUSED gint n_param,
             const gdouble *b,
             G_GNUC_UNUSED gpointer user_data,
             gboolean *fres)
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

static void
scale_exp_psdf(gdouble *param,
               gdouble xscale,
               gdouble yscale,
               gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale*xscale);
        param[1] *= xscale;
    }
    else {
        param[0] *= sqrt(yscale*xscale);
        param[1] /= xscale;
    }
}

static void
guess_exp_psdf(gdouble *x,
               gdouble *y,
               gint n_dat,
               gdouble *param,
               G_GNUC_UNUSED gpointer user_data,
               gboolean *fres)
{
    gint i;

    param[1] = 50/x[n_dat-1];

    param[0] = 0;
    for (i = 0; i < n_dat; i++)
        param[0] += x[1]*y[i];

    *fres = param[0] >= 0;
    param[0] = sqrt(param[0]);
}

/***************** exponential HHCF ********************************/
static gdouble
fit_exp_hhcf(gdouble x,
             G_GNUC_UNUSED gint n_param,
             const gdouble *b,
             G_GNUC_UNUSED gpointer user_data,
             gboolean *fres)
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

static void
guess_exp_hhcf(gdouble *x,
               gdouble *y,
               gint n_dat,
               gdouble *param,
               G_GNUC_UNUSED gpointer user_data,
               gboolean *fres)
{
    gint i;

    param[0] = 0;
    for (i = (n_dat/2); i < n_dat; i++)
        param[0] += y[i]/(n_dat/2);

    *fres = param[0] >= 0;
    param[0] = sqrt(param[0]);
    param[1] = x[n_dat-1]/50;
}

static void
scale_exp_hhcf(gdouble *param,
               gdouble xscale,
               gdouble yscale,
               gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale);
        param[1] /= xscale;
    }
    else {
        param[0] *= sqrt(yscale);
        param[1] *= xscale;
    }
}


/*************** exponential ACF ************************************/
static gdouble
fit_exp_acf(gdouble x,
            G_GNUC_UNUSED gint n_param,
            const gdouble *b,
            G_GNUC_UNUSED gpointer user_data,
            gboolean *fres)
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

static void
guess_exp_acf(gdouble *x,
              gdouble *y,
              gint n_dat,
              gdouble *param,
              G_GNUC_UNUSED gpointer user_data,
              gboolean *fres)
{
    param[0] = sqrt(y[1]);
    param[1] = x[n_dat-1]/50;

    *fres = y[1] >= 0;
}

static void
scale_exp_acf(gdouble *param,
              gdouble xscale,
              gdouble yscale,
              gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale);
        param[1] /= xscale;
    }
    else {
        param[0] *= sqrt(yscale);
        param[1] *= xscale;
    }
}


/**************   polynomial 0th order ********************************/
static gdouble
fit_poly_0(G_GNUC_UNUSED gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    *fres = TRUE;

    return b[0];
}

static void
guess_poly_0(gdouble *x,
             gdouble *y,
             gint n_dat,
             gdouble *param,
             G_GNUC_UNUSED gpointer user_data,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 0, param);
    *fres = TRUE;
}

static void
scale_poly_0(gdouble *param,
             G_GNUC_UNUSED gdouble xscale,
             gdouble yscale,
             gint dir)
{
    if (dir == 1)
        param[0] /= yscale;
    else
        param[0] *= yscale;
}


/*************** polynomial 1st order ********************************/
static gdouble
fit_poly_1(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    *fres = TRUE;

    return b[0] + x*b[1];
}

static void
guess_poly_1(gdouble *x,
             gdouble *y,
             gint n_dat,
             gdouble *param,
             G_GNUC_UNUSED gpointer user_data,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 1, param);
    *fres = TRUE;
}

static void
scale_poly_1(gdouble *param,
             gdouble xscale,
             gdouble yscale,
             gint dir)
{
    if (dir == 1) {
        param[0] /= yscale;
        param[1] /= yscale/xscale;
    }
    else {
        param[0] *= yscale;
        param[1] *= yscale/xscale;
    }
}

/************* polynomial 2nd order **********************************/
static gdouble
fit_poly_2(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    *fres = TRUE;

    return b[0] + x*(b[1] + x*b[2]);
}

static void
guess_poly_2(gdouble *x,
             gdouble *y,
             gint n_dat,
             gdouble *param,
             G_GNUC_UNUSED gpointer user_data,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 2, param);
    *fres = TRUE;
}

static void
scale_poly_2(gdouble *param,
             gdouble xscale,
             gdouble yscale,
             gint dir)
{
    if (dir == 1) {
        param[0] /= yscale;
        param[1] /= yscale/xscale;
        param[2] /= yscale/xscale/xscale;
    }
    else {
        param[0] *= yscale;
        param[1] *= yscale/xscale;
        param[2] *= yscale/xscale/xscale;
    }
}

/************** polynomial 3rd order *****************************/
static gdouble
fit_poly_3(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    *fres = TRUE;

    return b[0] + x*(b[1] + x*(b[2] + x*b[3]));
}

static void
guess_poly_3(gdouble *x,
             gdouble *y,
             gint n_dat,
             gdouble *param,
             G_GNUC_UNUSED gpointer user_data,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 3, param);
    *fres = TRUE;
}

static void
scale_poly_3(gdouble *param,
             gdouble xscale,
             gdouble yscale,
             gint dir)
{
    if (dir == 1) {
        param[0] /= yscale;
        param[1] /= yscale/xscale;
        param[2] /= yscale/xscale/xscale;
        param[3] /= yscale/xscale/xscale/xscale;
    }
    else {
        param[0] *= yscale;
        param[1] *= yscale/xscale;
        param[2] *= yscale/xscale/xscale;
        param[3] *= yscale/xscale/xscale/xscale;
    }
}


/******************** preset default weights *************************/

static void
weights_constant(G_GNUC_UNUSED gdouble *x,
                 G_GNUC_UNUSED gdouble *y,
                 gint n_dat,
                 gdouble *weight,
                 G_GNUC_UNUSED gpointer user_data)
{
    gint i;

    for (i = 0; i < n_dat; i++)
        weight[i] = 1;
}

static void
weights_linear_decrease(G_GNUC_UNUSED gdouble *x,
                        G_GNUC_UNUSED gdouble *y,
                        gint n_dat,
                        gdouble *weight,
                        G_GNUC_UNUSED gpointer user_data)
{
    gint i;

    for (i = 0; i < n_dat; i++)
        weight[i] = 1 - (gdouble)i/(gdouble)n_dat;
}


/************************** presets ****************************/

static const GwyNLFitParam gaussexp_pars[]= {
   {"x<sub>0</sub>", " ", 1 },
   {"y<sub>0</sub>", " ", 2 },
   {"a", " ", 3 },
   {"b", " ", 4 },
};

static const GwyNLFitParam gaussexp_two_pars[]= {
   {"\xcf\x83", " ", 1 },
   {"T", " ", 2 },
};


static const GwyNLFitParam poly0_pars[]= {
   {"a", " ", 1 },
};

static const GwyNLFitParam poly1_pars[]= {
   {"a", " ", 1 },
   {"b", " ", 2 },
};

static const GwyNLFitParam poly2_pars[]= {
   {"a", " ", 1 },
   {"b", " ", 2 },
   {"c", " ", 3 },
};

static const GwyNLFitParam poly3_pars[]= {
   {"a", " ", 1 },
   {"b", " ", 2 },
   {"c", " ", 3 },
   {"d", " ", 4 },
};


static const GwyNLFitPreset fitting_presets[] = {
    {
        "Gaussian",
        "Gaussian",
        "f(x) = y<sub>0</sub> + a*exp(-(b*(x-x<sub>0</sub>))<sup>2</sup>)",
        &fit_gauss,
        NULL,
        &guess_gauss,
        &scale_gauss,
        &weights_constant,
        4,
        gaussexp_pars,
        NULL
    },
    {
        "Gaussian (PSDF)",
        "Gaussian",
        "f(x) = (\xcf\x83<sup>2</sup>T)/(2)*exp(-(x*T/2)<sup>2</sup>)",
        &fit_gauss_psdf,
        NULL,
        &guess_gauss_psdf,
        &scale_gauss_psdf,
        &weights_constant,
        2,
        gaussexp_two_pars,
        NULL
    },
    {
        "Gaussian (ACF)",
        "Gaussian",
        "f(x) = \xcf\x83<sup>2</sup>exp(-(x/T)<sup>2</sup>)",
        &fit_gauss_acf,
        NULL,
        &guess_gauss_acf,
        &scale_gauss_acf,
        &weights_linear_decrease,
        2,
        gaussexp_two_pars,
        NULL
    },
    {
        "Gaussian (HHCF)",
        "Gaussian",
        "f(x) =  2*\xcf\x83<sup>2</sup>(1 - exp(-(x/T)<sup>2</sup>))",
        &fit_gauss_hhcf,
        NULL,
        &guess_gauss_hhcf,
        &scale_gauss_hhcf,
        &weights_linear_decrease,
        2,
        gaussexp_two_pars,
        NULL
    },
    {
        "Exponential",
        "Exponential",
        "f(x) = y<sub>0</sub> + a*exp(-(b*(x-x<sub>0</sub>)))",
        &fit_exp,
        NULL,
        &guess_exp,
        &scale_exp,
        &weights_constant,
        4,
        gaussexp_pars,
        NULL
    },
    {
        "Exponential (PSDF)",
        "Exponential",
        "f(x) = (\xcf\x83<sup>2</sup>T)/(2)/(1+((x/T)<sup>2</sup>))))",
        &fit_exp_psdf,
        NULL,
        &guess_exp_psdf,
        &scale_exp_psdf,
        &weights_constant,
        2,
        gaussexp_two_pars,
        NULL
    },
    {
        "Exponential (ACF)",
        "Exponential",
        "f(x) = \xcf\x83<sup>2</sup>exp(-(x/T))",
        &fit_exp_acf,
        NULL,
        &guess_exp_acf,
        &scale_exp_acf,
        &weights_linear_decrease,
        2,
        gaussexp_two_pars,
        NULL
    },
    { 
        "Exponential (HHCF)",
        "Exponential",
        "f(x) =  2*\xcf\x83<sup>2</sup>(1 - exp(-(x/T)))",
        &fit_exp_hhcf,
        NULL,
        &guess_exp_hhcf,
        &scale_exp_hhcf,
        &weights_linear_decrease,
        2,
        gaussexp_two_pars,
        NULL
    },
    {
        "Polynom (order 0)",
        "Polynom",
        "f(x) = a",
        &fit_poly_0,
        NULL,
        &guess_poly_0,
        &scale_poly_0,
        &weights_constant,
        1,
        poly0_pars,
        NULL
    },
    {
        "Polynom (order 1)",
        "Polynom",
        "f(x) = a + b*x",
        &fit_poly_1,
        NULL,
        &guess_poly_1,
        &scale_poly_1,
        &weights_constant,
        2,
        poly1_pars,
        NULL
    },
    {
        "Polynom (order 2)",
        "Polynom",
        "f(x) = a + b*x + c*x<sup>2</sup>",
        &fit_poly_2,
        NULL,
        &guess_poly_2,
        &scale_poly_2,
        &weights_constant,
        3,
        poly2_pars,
        NULL
    },
    {
        "Polynom (order 3)",
        "Polynom",
        "f(x) = a + b*x + c*x<sup>2</sup> + d*x<sup>3</sup>",
        &fit_poly_3,
        NULL,
        &guess_poly_3,
        &scale_poly_3,
        &weights_constant,
        4,
        poly3_pars,
        NULL
    },
};

/**
 * gwy_math_nlfit_get_npresets:
 *
 * Returns the number of available NL fitter presets.
 *
 * Returns: The number of presets.
 *
 * Since: 1.2.
 **/
gint
gwy_math_nlfit_get_npresets(void)
{
    return (gint)G_N_ELEMENTS(fitting_presets);
}

/**
 * gwy_math_nlfit_get_preset:
 * @preset_id: NL fitter preset number.
 *
 * Returns NL fitter preset number @preset_id.
 *
 * Presets are numbered sequentially from 0 to gwy_math_nlfit_get_npresets()-1.
 * The numbers are not guaranteed to be constants, use preset names as unique
 * identifiers.
 *
 * Returns: Preset number @preset_id.  Note the returned value must not be
 *          modified or freed.
 *
 * Since: 1.2.
 **/
G_CONST_RETURN GwyNLFitPreset*
gwy_math_nlfit_get_preset(gint preset_id)
{
    g_return_val_if_fail(preset_id >= 0
                         && preset_id < (gint)G_N_ELEMENTS(fitting_presets),
                         NULL);

    return fitting_presets + preset_id;
}

/**
 * gwy_math_nlfit_get_preset_by_name:
 * @name: NL fitter preset name.
 *
 * Returns NL fitter preset whose name is @name.
 *
 * Returns: Preset @name, %NULL if not found.  Note the returned value must
 *          not be modified or freed.
 *
 * Since: 1.2.
 **/
G_CONST_RETURN GwyNLFitPreset*
gwy_math_nlfit_get_preset_by_name(const gchar *name)
{
    gsize i;

    for (i = 0; i < G_N_ELEMENTS(fitting_presets); i++) {
        if (strcmp(name, fitting_presets[i].function_name) == 0)
            return fitting_presets + i;
    }
    return NULL;
}

/**
 * gwy_math_nlfit_get_preset_id:
 * @preset: A NL fitter function preset.
 *
 * Returns the id of a NL fitter preset.
 *
 * Returns: The preset number.
 *
 * Since: 1.2.
 **/
gint
gwy_math_nlfit_get_preset_id(const GwyNLFitPreset* preset)
{
    /* XXX: some sanity check? */
    return preset - fitting_presets;
}

/**
 * gwy_math_nlfit_get_preset_value:
 * @preset: A NL fitter function preset.
 * @params: Preset parameter values.
 * @x: The point to compute value at.
 *
 * Return preset function value in point @x with parameters @params.
 *
 * Returns: The function value.
 *
 * Since: 1.2.
 **/
gdouble
gwy_math_nlfit_get_preset_value(const GwyNLFitPreset* preset,
                                gdouble *params, gdouble x)
{
    gboolean res;

    return (preset->function)(x, preset->nparams, params, NULL, &res);
}

/**
 * gwy_math_nlfit_get_preset_name:
 * @preset: A NL fitter function preset.
 *
 * Return preset name (its unique identifier).
 *
 * Returns: The preset name.
 *
 * Since: 1.2.
 **/
G_CONST_RETURN gchar*
gwy_math_nlfit_get_preset_name(const GwyNLFitPreset* preset)
{
    return preset->function_name;
}

/**
 * gwy_math_nlfit_get_preset_formula:
 * @preset: A NL fitter function preset.
 *
 * Returns function formula of @preset (with Pango markup).
 *
 * Returns: The preset function formula.
 *
 * Since: 1.2.
 **/
G_CONST_RETURN gchar*
gwy_math_nlfit_get_preset_formula(const GwyNLFitPreset* preset)
{
    return preset->function_formula;
}

/**
 * gwy_math_nlfit_get_preset_param_name:
 * @preset: A NL fitter function preset.
 * @param: A parameter number.
 *
 * Returns the name of parameter number @param of preset @preset.
 *
 * The name may contain Pango markup.
 *
 * Returns: The name of parameter @param.
 *
 * Since: 1.2.
 **/
G_CONST_RETURN gchar*
gwy_math_nlfit_get_preset_param_name(const GwyNLFitPreset* preset,
                                     gint param)
{
    const GwyNLFitParam *par;

    g_return_val_if_fail(param >= 0 && param < preset->nparams, NULL);
    par = preset->param + param;

    return par->name;
}

/**
 * gwy_math_nlfit_get_preset_param_default:
 * @preset: A NL fitter function preset.
 * @param: A parameter number.
 *
 * Returns a suitable constant default parameter value.
 *
 * It is usually better to do an educated guess of initial parameter value.
 *
 * Returns: The default parameter value.
 *
 * Since: 1.2.
 **/
gdouble
gwy_math_nlfit_get_preset_param_default(const GwyNLFitPreset* preset,
                                        gint param)
{
    const GwyNLFitParam *par;

    g_return_val_if_fail(param >= 0 && param < preset->nparams, G_MAXDOUBLE);
    par = preset->param + param;

    return par->default_init;
}

/**
 * gwy_math_nlfit_get_preset_nparams:
 * @preset: A NL fitter function preset.
 *
 * Return the number of parameters of @preset.
 *
 * Returns: The number of function parameters.
 *
 * Since: 1.2.
 **/
gint
gwy_math_nlfit_get_preset_nparams(const GwyNLFitPreset* preset)
{
    return preset->nparams;
}

/**
 * gwy_math_nlfit_fit_preset:
 * @preset: 
 * @n_dat: 
 * @x: 
 * @y: 
 * @n_param: 
 * @param: 
 * @err: 
 * @fixed_param: 
 * @user_data: 
 *
 * 
 *
 * Returns:
 *
 * Since: 1.2.
 **/
GwyNLFitter*
gwy_math_nlfit_fit_preset(const GwyNLFitPreset* preset,
                          gint n_dat, const gdouble *x, const gdouble *y,
                          gint n_param,
                          gdouble *param, gdouble *err,
                          const gboolean *fixed_param,
                          gpointer user_data)
{
    GwyNLFitter *fitter;
    gdouble xscale, yscale;
    gdouble *weight;
    gdouble *xsc, *ysc;  /* rescaled x and y */
    gint i;

    /* recompute data to be reasonably scaled */
    xscale = 0;
    yscale = 0;
    for (i = 0; i < n_dat; i++) {
        xscale += fabs(x[i]);
        yscale += fabs(y[i]);
    }
    xscale /= n_dat;
    yscale /= n_dat;
    xsc = g_new(gdouble, n_dat);
    ysc = g_new(gdouble, n_dat);
    for (i = 0; i < n_dat; i++) {
        xsc[i] = x[i]/xscale;
        ysc[i] = y[i]/yscale;

    }
    preset->parameter_scale(param, xscale, yscale, 1);

    /*use numerical derivation if necessary*/
    if (preset->function_derivation == NULL)
        fitter = gwy_math_nlfit_new(preset->function,
                                    gwy_math_nlfit_derive);
    else
        fitter = gwy_math_nlfit_new(preset->function,
                                    preset->function_derivation);

    /*load default weights for given function type*/
    weight = (gdouble *)g_malloc(n_dat*sizeof(gdouble));
    preset->set_default_weights(xsc, ysc, n_dat, weight, NULL);

    gwy_math_nlfit_fit_with_fixed(fitter, n_dat, xsc, ysc, weight,
                                  n_param, param, fixed_param, user_data);

    if (fitter->covar)
    {
        for (i = 0; i < n_param; i++)
            err[i] = gwy_math_nlfit_get_sigma(fitter, i);
    }
    /*recompute parameters to be scaled as original data*/
    preset->parameter_scale(param, xscale, yscale, -1);
    if (fitter->covar) preset->parameter_scale(err, xscale, yscale, -1);

    g_free(ysc);
    g_free(xsc);
    g_free(weight);

    return fitter;
}

/************************** Documentation ****************************/

/**
 * GwyNLFitFunc:
 * @x: The value to compute the function in.
 * @n_param: The number of parameters (size of @param).
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
 * @n_param: The number of parameters (size of @param).
 * @param: Parameters.
 * @deriv: Array where the @n_param partial derivations by each parameter are
 *         to be stored.
 * @fmarq: The fitting function.
 * @user_data: User data as passed to gwy_math_nlfit_fit().
 * @dres: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Fitting function partial derivation type.
 */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
