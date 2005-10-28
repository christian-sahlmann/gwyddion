/*
 *  @(#) $Id$
 *  Copyright (C) 2000-2003 Martin Siler.
 *  Copyright (C) 2004,2005 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwynlfit.h>

/* Side step constant for numerical differentiation in gwy_math_nlfit_derive()
 */
#define FitSqrtMachEps  1e-5

/* Constant for decision to stop fitting cycle due to relative difference
 * in residual sum of squares between subsequent steps.
 */
#define EPS 1e-16

/* Lower symmetric part indexing */
/* i (row) MUST be greater or equal than j (column) */
#define SLi(a, i, j) a[(i)*((i) + 1)/2 + (j)]

static gdouble  gwy_math_nlfit_residua      (GwyNLFitter *nlfit,
                                             gint n_dat,
                                             const gdouble *x,
                                             const gdouble *y,
                                             const gdouble *weight,
                                             gint n_param,
                                             const gdouble *param,
                                             gpointer user_data,
                                             gdouble *resid);

/* XXX: publish in 2.0? */
static gboolean gwy_math_sym_matrix_invert(gint n,
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
 * @n_dat: The number of data points in @x, @y.
 * @x: Array of independent variable values.
 * @y: Array of dependent variable values.
 * @n_param: The nuber of parameters.
 * @param: Array of parameters (of size @n_param).  Note the parameters must
 *         be initialized to reasonably near values.
 * @user_data: Any pointer that will be passed to the function and derivation
 *             as @user_data.
 *
 * Performs a nonlinear fit of @nlfit function on (@x,@y) data.
 *
 * Returns: The final residual sum, a negative number in the case of failure.
 **/
gdouble
gwy_math_nlfit_fit(GwyNLFitter *nlfit,
                   gint n_dat,
                   const gdouble *x,
                   const gdouble *y,
                   gint n_param,
                   gdouble *param,
                   gpointer user_data)
{
    return gwy_math_nlfit_fit_full(nlfit, n_dat, x, y, NULL,
                                   n_param, param, NULL, NULL, user_data);
}

/**
 * gwy_math_nlfit_fit_with_fixed:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @n_dat: The number of data points in @x, @y, @weight.
 * @x: Array of independent variable values.
 * @y: Array of dependent variable values.
 * @weight: Array of weights associated to each data point.  Can be %NULL,
 *          weight of 1 is then used for all data.
 * @n_param: The nuber of parameters.
 * @param: Array of parameters (of size @n_param).  Note the parameters must
 *         be initialized to reasonably near values.
 * @fixed_param: Which parameters should be treated as fixed (set corresponding
 *               element to %TRUE for them).  May be %NULL if all parameters
 *               are variable.
 * @link_map: Map of linked parameters.  One of linked parameters is master,
 *            Values in this array are indices of corresponding master
 *            parameter for each parameter (for independent parameters set
 *            @link_map[i] == i).   May be %NULL if all parameter are
 *            independent.
 * @user_data: Any pointer that will be passed to the function and derivation
 *
 * Performs a nonlinear fit of @nlfit function on (@x,@y) data, allowing
 * some fixed parameters.
 *
 * Initial values of linked (dependent) parameters are overwritten by master
 * values, their @fixed_param property is ignored and master's property
 * controls whether all are fixed or all variable.
 *
 * Returns: The final residual sum, a negative number in the case of failure.
 **/
gdouble
gwy_math_nlfit_fit_full(GwyNLFitter *nlfit,
                        gint n_dat,
                        const gdouble *x,
                        const gdouble *y,
                        const gdouble *weight,
                        gint n_param,
                        gdouble *param,
                        const gboolean *fixed_param,
                        const gint *link_map,
                        gpointer user_data)
{
    gdouble mlambda = 1e-4;
    gdouble sumr = G_MAXDOUBLE;
    gdouble sumr1;
    gdouble *der, *v, *xr, *saveparam, *resid, *a, *save_a;
    gdouble *w = NULL;
    gint *var_param_id;
    gboolean *fixed;
    gint covar_size;
    gint i, j, k;
    gint n_var_param;
    gint miter = 0;
    gboolean step1 = TRUE;
    gboolean end = FALSE;

    g_return_val_if_fail(nlfit, -1.0);
    g_return_val_if_fail(n_param > 0, -1.0);
    g_return_val_if_fail(n_dat > n_param, -1.0);
    g_return_val_if_fail(x && y && param, -1.0);

    g_free(nlfit->covar);
    nlfit->covar = NULL;

    /* Use defaults for param specials, if not specified */
    if (!weight) {
        w = g_new(gdouble, n_dat);
        for (i = 0; i < n_dat; i++)
            w[i] = 1.0;
        weight = w;
    }
    if (!link_map) {
        gint *l;

        l = g_newa(gint, n_param);
        for (i = 0; i < n_param; i++)
            l[i] = i;
        link_map = l;
    }

    /* Sync slave param values with master */
    for (i = 0; i < n_param; i++) {
        if (link_map[i] != i)
            param[i] = param[link_map[i]];
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
    for (i = 0; i < n_param; i++) {
        if (fixed_param && fixed_param[link_map[i]])
            var_param_id[i] = -1;
        else {
            if (link_map[i] == i) {
                var_param_id[i] = n_var_param;
                n_var_param++;
            }
        }
        gwy_debug("var_param_id[%d] = %d", i, var_param_id[i]);
    }
    /* assign master var_param_id to slaves in second pass, as it may have
     * higher id than slave */
    for (i = 0; i < n_param; i++) {
        if (link_map[i] != i)
            var_param_id[i] = var_param_id[link_map[i]];
    }

    if (!n_var_param) {
        g_free(w);
        g_free(var_param_id);
        g_free(resid);
        return sumr;
    }

    /* Resolve which params are fixed, taking links into account.  We
     * cannot modify fixed_param, so create a new array. */
    if (!fixed_param)
        fixed = NULL;
    else {
        fixed = g_new0(gboolean, n_param);
        for (i = 0; i < n_param; i++) {
            fixed[i] = fixed_param[link_map[i]];
        }
    }

    covar_size = n_var_param*(n_var_param + 1)/2;

    der = g_new(gdouble, n_param);  /* because ->dmarq() computes all */
    v = g_new(gdouble, n_var_param);
    xr = g_new(gdouble, n_var_param);
    saveparam = g_new(gdouble, n_param);
    a = g_new(gdouble, covar_size);
    save_a = g_new(gdouble, covar_size);

    /* The actual minizmation */
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
                nlfit->dmarq(x[i], n_param, param, fixed, nlfit->fmarq,
                             user_data, der, &nlfit->eval);
                if (!nlfit->eval)
                    break;

                /* acummulate derivations by slave parameters in master */
                for (j = 0; j < n_param; j++) {
                    if (link_map[j] != j)
                        der[link_map[j]] += der[j];
                }

                for (j = 0; j < n_param; j++) {
                    gint jid, diag;

                    /* Only variable master parameters matter */
                    if ((jid = var_param_id[j]) < 0 || link_map[j] != j)
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

        /* Move master params along the solved gradient */
        for (i = 0; i < n_param; i++) {
            if (var_param_id[i] < 0 || link_map[i] != i)
                continue;
            param[i] = saveparam[i] + xr[var_param_id[i]];
            if (fabs(param[i] - saveparam[i]) == 0)
                count++;
        }
        /* Sync slave params with master */
        for (i = 0; i < n_param; i++) {
            if (var_param_id[i] >= 0 && link_map[i] != i)
                param[i] = param[link_map[i]];
        }
        if (count == n_var_param)
            break;

        /* See what the new residua is */
        sumr1 = gwy_math_nlfit_residua(nlfit, n_dat,
                                       x, y, weight,
                                       n_param, param,
                                       user_data, resid);
        /* Good, we've finished */
        if ((sumr1 == 0)
            || (miter > 2 && fabs((sumr - sumr1)/sumr1) < EPS))
            end = TRUE;
        /* Overshoot, increase lambda */
        if (!nlfit->eval || sumr1 >= sumr) {
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

    /* Parameter errors computation */
    if (nlfit->eval) {
        if (gwy_math_sym_matrix_invert(n_var_param, save_a)) {
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
        else {
            /* XXX: else what? */
            g_warning("Cannot invert covariance matrix");
            sumr = -1.0;
        }
    }

    g_free(save_a);
    g_free(a);
    g_free(saveparam);
    g_free(xr);
    g_free(v);
    g_free(der);
    g_free(fixed);
    g_free(var_param_id);
    g_free(resid);
    g_free(w);

    return sumr;
}


/**
 * gwy_math_nlfit_derive:
 * @x: The value to compute the derivation at.
 * @n_param: The nuber of parameters.
 * @param: Array of parameters (of size @n_param).
 * @fixed_param: Which parameters should be treated as fixed (corresponding
 *               entries are set to %TRUE).
 * @ff: The fitted function.
 * @user_data: User data as passed to gwy_math_nlfit_fit().
 * @deriv: Array where the put the result to.
 * @dres: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Numerically computes the partial derivations of @ff
 **/
void
gwy_math_nlfit_derive(gdouble x,
                      gint n_param,
                      const gdouble *param,
                      const gboolean *fixed_param,
                      GwyNLFitFunc ff,
                      gpointer user_data,
                      gdouble *deriv,
                      gboolean *dres)
{
    gdouble *param_tmp;
    gdouble hj, left, right;
    gint j;

    param_tmp = g_newa(gdouble, n_param);
    memcpy(param_tmp, param, n_param*sizeof(gdouble));

    for (j = 0; j < n_param; j++) {
        if (fixed_param && fixed_param[j])
            continue;

        hj = (fabs(param_tmp[j]) + FitSqrtMachEps) * FitSqrtMachEps;
        param_tmp[j] -= hj;
        left = ff(x, n_param, param_tmp, user_data, dres);
        if (!dres)
            return;

        param_tmp[j] += 2 * hj;
        right = ff(x, n_param, param_tmp, user_data, dres);
        if (!dres)
            return;

        deriv[j] = (right - left)/2/hj;
        param_tmp[j] = param[j];
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
 * gwy_math_sym_matrix_invert:
 * @n: Matrix size.
 * @a: Lower-left part of symmetric, positive definite matrix.
 *
 * Inverts a positive definite matrix in place.
 *
 * Returns: Whether the matrix invesion succeeded.
 **/
static gboolean
gwy_math_sym_matrix_invert(gint n, gdouble *a)
{

    gint q = 0, m;
    gdouble s, t;
    gdouble *x;
    gint k, i, j;

    x = g_newa(gdouble, n);
    for (k = n-1; k >= 0; k--) {
        s = a[0];
        if (s <= 0)
            return FALSE;
        m = 0;
        for (i = 0; i < n-1; i++) {
            q = m+1;
            m += i+2;
            t = a[q];
            x[i] = -t/s;      /* note use temporary x */
            if (i >= k)
                x[i] = -x[i];
            for (j = q; j < m; j++)
                a[j - (i+1)] = a[j+1] + t * x[j - q];
        }
        a[m] = 1.0/s;
        for (i = 0; i < n-1; i++)
            a[q + i] = x[i];
    }

    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwynlfit
 * @title: GwyNLFitter
 * @short_description: Marquardt-Levenberg nonlinear least square fitter
 * @see_also: #GwyNLFitPreset
 *
 * A new Marquardt-Levenberg nonlinear least square fitter can be created with
 * gwy_math_nlfit_new(), specifying the function to fit (as #GwyNLFitFunc) and
 * its derivation (as #GwyNLFitDerFunc). For functions for whose analytic
 * derivation is not available or very impractical, gwy_math_nlfit_derive()
 * (computing the derivation numerically) can be used instead.
 *
 * A fitter can be then repeatedly used on different data either in
 * gwy_math_nlfit_fit(), or gwy_math_nlfit_fit_with_fixed() when there are some
 * fixed parameters. Arbitrary additional (non-fitting) parameters can be
 * passed to the fited function in <parameter>user_data</parameter>.
 *
 * After a successfull fit additional fit information can be obtained with
 * gwy_math_nlfit_get_dispersion(), gwy_math_nlfit_get_correlations(),
 * gwy_math_nlfit_get_sigma(). Note these functions may be used only after a
 * successfull fit. When a fitter is no longer needed, it should be freed with
 * gwy_math_nlfit_free().
 *
 * Several common functions are also available as fitting presets that can be
 * fitted with gwy_math_nlfit_fit_preset(). Each one can be identified by a
 * unique name or a numeric id (the latter one may however change between
 * releases) the number of presets can be obtained with
 * gwy_math_nlfit_get_npresets(). Preset properties can be obtained with
 * functions like gwy_math_nlfit_get_preset_nparams() or
 * gwy_math_nlfit_get_preset_formula().
 **/

/**
 * GwyNLFitFunc:
 * @x: The value to compute the function at.
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
 * @x: x-data as passed to gwy_math_nlfit_fit().
 * @n_param: The number of parameters (size of @param).
 * @param: Parameters.
 * @fixed_param: Which parameters should be treated as fixed (corresponding
 *               entries are set to %TRUE).
 * @deriv: Array where the @n_param partial derivations by each parameter are
 *         to be stored.
 * @fmarq: The fitting function.
 * @user_data: User data as passed to gwy_math_nlfit_fit().
 * @dres: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Fitting function partial derivation type.
 */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
