/*
 *  @(#) $Id$
 *  Copyright (C) 2000-2003 Martin Siler.
 *  Copyright (C) 2004-2016 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
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

typedef struct {
    GwyNLFitter *fitter;
    GwySetFractionFunc set_fraction;
    GwySetMessageFunc set_message;
    GwyNLFitIdxFunc func_idx;
    GwyNLFitIdxDiffFunc diff_idx;
} GwyNLFitterPrivate;

static void                gwy_math_nlfit_init    (GwyNLFitter *nlfit);
static gdouble             gwy_math_nlfit_fit_real(GwyNLFitter *nlfit,
                                                   guint ndata,
                                                   const gdouble *x,
                                                   const gdouble *y,
                                                   const gdouble *weight,
                                                   guint nparam,
                                                   gdouble *param,
                                                   const gboolean *fixed_param,
                                                   const gint *link_map,
                                                   gpointer user_data);
static gdouble             gwy_math_nlfit_residua (GwyNLFitFunc func,
                                                   GwyNLFitIdxFunc func_idx,
                                                   guint ndata,
                                                   const gdouble *x,
                                                   const gdouble *y,
                                                   const gdouble *weight,
                                                   guint nparam,
                                                   const gdouble *param,
                                                   gpointer user_data,
                                                   gdouble *resid,
                                                   gboolean *success);
static GwyNLFitterPrivate* find_private_data      (GwyNLFitter *fitter,
                                                   gboolean do_create);
static void                free_private_data      (GwyNLFitter *fitter);

static GList *private_fitter_data = NULL;

/**
 * gwy_math_nlfit_new:
 * @func: The fitted function.
 * @diff: The derivative of fitted function.
 *
 * Creates a new Marquardt-Levenberg nonlinear fitter for function with
 * a real-valued independent variable.
 *
 * See gwy_math_nlfit_new_idx() for more complex scenarios.
 *
 * You can use gwy_math_nlfit_diff() computing the derivative numerically,
 * when you do not know the derivatives explicitely.  Since 2.46 passing %NULL
 * as @diff has the same effect, i.e. the fitter will automatically use
 * numerical differentiation.
 *
 * Returns: The newly created fitter.
 **/
GwyNLFitter*
gwy_math_nlfit_new(GwyNLFitFunc func, GwyNLFitDerFunc diff)
{
    GwyNLFitter *nlfit;

    nlfit = g_new0(GwyNLFitter, 1);
    gwy_math_nlfit_init(nlfit);
    nlfit->fmarq = func;
    nlfit->dmarq = diff ? diff : gwy_math_nlfit_diff;

    return nlfit;
}

/**
 * gwy_math_nlfit_new_idx:
 * @func: The fitted function.
 * @diff: The derivative of fitted function.
 *
 * Creates a new Marquardt-Levenberg nonlinear fitter for opaque indexed data.
 *
 * As only the data index is passed to the functions, using this interface
 * permits fitting more complex functions.  The abscissa can be arbitrary,
 * for instance a vector, as it is not seen by the fitter.  Similarly,
 * vector-valued functions can be emulated by mapping tuples of indices to the
 * vector components.
 *
 * You can pass %NULL as @diff to use automatically numerical differentiation
 * when you do not know the derivatives explicitely.  Note that this means you
 * cannot use weighting.  If you want weighting you need to pass your own
 * @diff function that performs the weighting (it can utilise the
 * gwy_math_nlfit_diff_idx() helper).
 *
 * Returns: The newly created fitter.
 *
 * Since: 2.46
 **/
GwyNLFitter*
gwy_math_nlfit_new_idx(GwyNLFitIdxFunc func, GwyNLFitIdxDiffFunc diff)
{
    GwyNLFitterPrivate *priv;
    GwyNLFitter *nlfit;

    nlfit = g_new0(GwyNLFitter, 1);
    gwy_math_nlfit_init(nlfit);
    priv = find_private_data(nlfit, TRUE);
    priv->func_idx = func;
    priv->diff_idx = diff;

    return nlfit;
}

static void
gwy_math_nlfit_init(GwyNLFitter *nlfit)
{
    nlfit->mfi = 1.0;
    nlfit->mdec = 0.4;
    nlfit->minc = 10.0;
    nlfit->mtol = 1e-6;
    nlfit->maxiter = 100;
    nlfit->eval = FALSE;
    nlfit->dispersion = -1;
    nlfit->covar = NULL;
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
    free_private_data(nlfit);
    g_free(nlfit->covar);
    nlfit->covar = NULL;
    g_free(nlfit);
}

/**
 * gwy_math_nlfit_fit:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @ndata: The number of data points in @x, @y.
 * @x: Array of independent variable values.
 * @y: Array of dependent variable values.
 * @nparam: The nuber of parameters.
 * @param: Array of parameters (of size @nparam).  Note the parameters must
 *         be initialized to reasonably near values.
 * @user_data: Pointer that will be passed to the function and derivative
 *             as @user_data.
 *
 * Performs a nonlinear fit of simple function on data.
 *
 * Returns: The final residual sum, a negative number in the case of failure.
 **/
gdouble
gwy_math_nlfit_fit(GwyNLFitter *nlfit,
                   gint ndata,
                   const gdouble *x,
                   const gdouble *y,
                   gint nparam,
                   gdouble *param,
                   gpointer user_data)
{
    return gwy_math_nlfit_fit_real(nlfit, ndata, x, y, NULL,
                                   nparam, param, NULL, NULL, user_data);
}

/**
 * gwy_math_nlfit_fit_full:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @ndata: The number of data points in @x, @y, @weight.
 * @x: Array of independent variable values.
 * @y: Array of dependent variable values.
 * @weight: Array of weights associated to each data point (usually equal to 
 *          inverse squares errors).  Can be %NULL, unit weight is then used
 *          for all data.
 * @nparam: The nuber of parameters.
 * @param: Array of parameters (of size @nparam).  Note the parameters must
 *         be initialized to reasonably near values.
 * @fixed_param: Which parameters should be treated as fixed (set corresponding
 *               element to %TRUE for them).  May be %NULL if all parameters
 *               are variable.
 * @link_map: Map of linked parameters.  One of linked parameters is master,
 *            Values in this array are indices of corresponding master
 *            parameter for each parameter (for independent parameters set
 *            @link_map[i] == i).   May be %NULL if all parameter are
 *            independent.
 * @user_data: Pointer that will be passed to the function and derivative
 *
 * Performs a nonlinear fit of simple function on data, allowing some fixed
 * parameters.
 *
 * Initial values of linked (dependent) parameters are overwritten by master
 * values, their @fixed_param property is ignored and master's property
 * controls whether all are fixed or all variable.
 *
 * Returns: The final residual sum, a negative number in the case of failure.
 **/
gdouble
gwy_math_nlfit_fit_full(GwyNLFitter *nlfit,
                        gint ndata,
                        const gdouble *x,
                        const gdouble *y,
                        const gdouble *weight,
                        gint nparam,
                        gdouble *param,
                        const gboolean *fixed_param,
                        const gint *link_map,
                        gpointer user_data)
{
    return gwy_math_nlfit_fit_real(nlfit, ndata, x, y, weight,
                                   nparam, param, fixed_param, link_map,
                                   user_data);
}

/**
 * gwy_math_nlfit_fit_idx:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @ndata: The number of data points in @x, @y, @weight.
 * @nparam: The nuber of parameters.
 * @param: Array of parameters (of size @nparam).  Note the parameters must
 *         be initialized to reasonably near values.
 * @user_data: Pointer that will be passed to the function and derivative
 *
 * Performs a nonlinear fit of function on opaque indexed data.
 *
 * Returns: The final residual sum, a negative number in the case of failure.
 *
 * Since: 2.46
 **/
gdouble
gwy_math_nlfit_fit_idx(GwyNLFitter *nlfit,
                       guint ndata,
                       guint nparam,
                       gdouble *param,
                       gpointer user_data)
{
    return gwy_math_nlfit_fit_real(nlfit, ndata, NULL, NULL, NULL,
                                   nparam, param, NULL, NULL, user_data);
}

/**
 * gwy_math_nlfit_fit_idx_full:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @ndata: The number of data points in @x, @y, @weight.
 * @nparam: The nuber of parameters.
 * @param: Array of parameters (of size @nparam).  Note the parameters must
 *         be initialized to reasonably near values.
 * @fixed_param: Which parameters should be treated as fixed (set corresponding
 *               element to %TRUE for them).  May be %NULL if all parameters
 *               are variable.
 * @link_map: Map of linked parameters.  One of linked parameters is master,
 *            Values in this array are indices of corresponding master
 *            parameter for each parameter (for independent parameters set
 *            @link_map[i] == i).   May be %NULL if all parameter are
 *            independent.
 * @user_data: Pointer that will be passed to the function and derivative
 *
 * Performs a nonlinear fit of function on opaque indexed data, allowing some
 * fixed parameters.
 *
 * Initial values of linked (dependent) parameters are overwritten by master
 * values, their @fixed_param property is ignored and master's property
 * controls whether all are fixed or all variable.
 *
 * Returns: The final residual sum, a negative number in the case of failure.
 *
 * Since: 2.46
 **/
gdouble
gwy_math_nlfit_fit_idx_full(GwyNLFitter *nlfit,
                            guint ndata,
                            guint nparam,
                            gdouble *param,
                            const gboolean *fixed_param,
                            const gint *link_map,
                            gpointer user_data)
{
    return gwy_math_nlfit_fit_real(nlfit, ndata, NULL, NULL, NULL,
                                   nparam, param, fixed_param, link_map,
                                   user_data);
}

static gdouble
gwy_math_nlfit_fit_real(GwyNLFitter *nlfit,
                        guint ndata,
                        const gdouble *x,
                        const gdouble *y,
                        const gdouble *weight,
                        guint nparam,
                        gdouble *param,
                        const gboolean *fixed_param,
                        const gint *link_map,
                        gpointer user_data)
{
    GwyNLFitterPrivate *priv;
    GwySetFractionFunc set_fraction = NULL;
    GwySetMessageFunc set_message = NULL;
    GwyNLFitFunc func;
    GwyNLFitDerFunc diff;
    GwyNLFitIdxFunc func_idx;
    GwyNLFitIdxDiffFunc diff_idx;
    gdouble mlambda = 1e-4;
    gdouble sumr1, sumr = G_MAXDOUBLE;
    gdouble *der = NULL, *v = NULL, *xr = NULL, *w = NULL,
            *saveparam = NULL, *origparam = NULL, *resid = NULL,
            *a = NULL, *save_a = NULL;
    guint *var_param_id = NULL, *lmap = NULL;
    gboolean *fixed = NULL;
    guint covar_size;
    guint i, j, k;
    guint n_var_param;
    guint miter = 0;
    gboolean step1 = TRUE;
    gboolean end = FALSE;

    g_return_val_if_fail(nlfit, -1.0);
    g_return_val_if_fail(param || !nparam, -1.0); /* handle zero nparam later */

    priv = find_private_data(nlfit, TRUE);
    set_fraction = priv->set_fraction;
    set_message = priv->set_message;
    func = nlfit->fmarq;
    diff = nlfit->dmarq;
    func_idx = priv->func_idx;
    diff_idx = priv->diff_idx;

    g_free(nlfit->covar);
    nlfit->covar = NULL;
    nlfit->dispersion = -1.0;

    if (ndata < nparam)
        return -1.0;

    g_return_val_if_fail((x && y && func && diff) || func_idx, -1.0);
    g_return_val_if_fail(!func_idx || (!x && !y && !weight && !func), -1.0);

    if (set_message)
        set_message(_("Initial residua evaluation..."));
    if (set_fraction)
        set_fraction(0.0);

    /* Calculate square roots of weights because they are easily split into
     * the expressions.  The indexed interface already does it right. */
    if (weight) {
        w = g_new(gdouble, ndata);
        for (i = 0; i < ndata; i++)
            w[i] = sqrt(fmax(weight[i], 0.0));
    }

    /* Use defaults for param specials, if not specified */
    if (!link_map) {
        lmap = g_new(gint, nparam);
        for (i = 0; i < nparam; i++)
            lmap[i] = i;
        link_map = lmap;
    }

    /* Sync slave param values with master */
    origparam = g_memdup(param, nparam*sizeof(gdouble));
    for (i = 0; i < nparam; i++) {
        if (link_map[i] != i)
            param[i] = param[link_map[i]];
    }

    resid = g_new(gdouble, ndata);
    sumr1 = gwy_math_nlfit_residua(func, func_idx, ndata, x, y, w,
                                   nparam, param, user_data, resid,
                                   &nlfit->eval);
    sumr = sumr1;

    if (!nlfit->eval) {
        g_warning("Initial residua evaluation failed");
        sumr = -1.0;
        goto fail;
    }

    if (nparam == 0)
        goto fail;

    if (set_fraction && !set_fraction(1.0/(nlfit->maxiter + 1))) {
        nlfit->eval = FALSE;
        sumr = -2.0;
        goto fail;
    }

    if (set_message)
        set_message(_("Fitting..."));

    /* find non-fixed parameters and map all -> non-fixed */
    n_var_param = 0;
    var_param_id = g_new(guint, nparam);
    for (i = 0; i < nparam; i++) {
        if (fixed_param && fixed_param[link_map[i]])
            var_param_id[i] = G_MAXUINT;
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
    for (i = 0; i < nparam; i++) {
        if (link_map[i] != i)
            var_param_id[i] = var_param_id[link_map[i]];
    }

    if (!n_var_param)
        goto fail;

    /* Resolve which params are fixed, taking links into account.  We
     * cannot modify fixed_param, so create a new array. */
    if (!fixed_param)
        fixed = NULL;
    else {
        fixed = g_new0(gboolean, nparam);
        for (i = 0; i < nparam; i++)
            fixed[i] = fixed_param[link_map[i]];
    }

    covar_size = n_var_param*(n_var_param + 1)/2;

    der = g_new(gdouble, nparam);  /* because diff() computes all */
    v = g_new(gdouble, n_var_param);
    xr = g_new(gdouble, n_var_param);
    saveparam = g_new(gdouble, nparam);
    a = g_new(gdouble, covar_size);
    save_a = g_new(gdouble, covar_size);

    /* The actual minizmation */
    do {
        gboolean is_pos_def = FALSE;
        gboolean first_pass = TRUE;
        guint count = 0;

        if (step1) {
            mlambda *= nlfit->mdec;
            sumr = sumr1;

            gwy_clear(a, covar_size);
            gwy_clear(v, n_var_param);

            /* J'J and J'r computation */
            for (i = 0; i < ndata; i++) {
                if (diff_idx) {
                    diff_idx(i, param, fixed_param, func_idx, user_data, der,
                             &nlfit->eval);
                }
                else if (diff) {
                    diff(x[i], nparam, param, fixed, func, user_data, der,
                         &nlfit->eval);
                }
                else {
                    gwy_math_nlfit_diff_idx(i, nparam, param, fixed,
                                            func_idx, user_data, der,
                                            &nlfit->eval);
                }

                if (!nlfit->eval)
                    break;

                /* This should be done only for the real-function interface;
                 * but that is also the only case when @w can be non-NULL. */
                if (w) {
                    for (j = 0; j < nparam; j++)
                        der[j] *= w[i];
                }

                /* acummulate derivatives by slave parameters in master */
                for (j = 0; j < nparam; j++) {
                    if (link_map[j] != j)
                        der[link_map[j]] += der[j];
                }

                for (j = 0; j < nparam; j++) {
                    guint jid, diag;

                    /* Only variable master parameters matter */
                    if ((jid = var_param_id[j]) == G_MAXUINT || link_map[j] != j)
                        continue;
                    diag = jid*(jid + 1)/2;

                    /* for J'r */
                    v[jid] += der[j] * resid[i];
                    for (k = 0; k <= j; k++) {   /* for J'J */
                        gint kid = var_param_id[k];

                        if (kid != G_MAXUINT)
                            a[diag + kid] += der[j] * der[k];
                    }
                }
            }
            if (nlfit->eval) {
                memcpy(save_a, a, covar_size*sizeof(gdouble));
                memcpy(saveparam, param, nparam*sizeof(gdouble));
            }
            else {
                sumr = -1.0;
                break;
            }
        }
        while (!is_pos_def) {
            if (!first_pass)
                memcpy(a, save_a, covar_size*sizeof(gdouble));
            else
                first_pass = FALSE;

            for (j = 0; j < n_var_param; j++) {
                /* Add diagonal elements */
                guint diag = j*(j + 3)/2;

                /* This used to be there.  But it breaks the scaling because
                 * mfi is just a number while a[] elements scale with the
                 * param derivatives.
                 * a[diag] = save_a[diag]*(1.0 + mlambda) + nlfit->mfi*mlambda;
                 */
                if (G_UNLIKELY(save_a[diag] == 0.0))
                    a[diag] = nlfit->mfi*mlambda;
                else
                    a[diag] = save_a[diag]*(1.0 + mlambda);

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
        for (i = 0; i < nparam; i++) {
            if (var_param_id[i] == G_MAXUINT || link_map[i] != i)
                continue;
            param[i] = saveparam[i] + xr[var_param_id[i]];
            if (fabs(param[i] - saveparam[i]) == 0)
                count++;
        }
        /* Sync slave params with master */
        for (i = 0; i < nparam; i++) {
            if (var_param_id[i] != G_MAXUINT && link_map[i] != i)
                param[i] = param[link_map[i]];
        }
        if (count == n_var_param)
            break;

        /* See what the new residua is */
        sumr1 = gwy_math_nlfit_residua(func, func_idx, ndata, x, y, weight,
                                       nparam, param, user_data, resid,
                                       &nlfit->eval);
        /* Catch failed evaluation even if it's not reported. */
        if (gwy_isinf(sumr1) || gwy_isnan(sumr1)) {
            nlfit->eval = FALSE;
            sumr = -1.0;
            break;
        }

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

        if (set_fraction && !set_fraction((gdouble)miter/nlfit->maxiter)) {
            nlfit->eval = FALSE;
            sumr = -2.0;
            break;
        }
    } while (!end);

    /* Parameter errors computation */
    if (nlfit->eval) {
        if (gwy_math_choleski_invert(n_var_param, save_a)) {
            /* stretch the matrix to span over fixed params too */
            nlfit->covar = g_new(gdouble, nparam*(nparam + 1)/2);
            for (i = 0; i < nparam; i++) {
                gint iid = var_param_id[i];

                for (j = 0; j < i; j++) {
                    gint jid = var_param_id[j];

                    if (iid == G_MAXDOUBLE || jid == G_MAXDOUBLE)
                        SLi(nlfit->covar, i, j) = 0.0;
                    else
                        SLi(nlfit->covar, i, j) = SLi(save_a, iid, jid);
                }
                if (iid < 0)
                    SLi(nlfit->covar, i, j) = 1.0;
                else
                    SLi(nlfit->covar, i, i) = SLi(save_a, iid, iid);
            }
            nlfit->dispersion = sumr/(ndata - n_var_param);
        }
        else {
            /* XXX: else what? */
            //g_warning("Cannot invert covariance matrix");
            sumr = -1.0;
            g_free(nlfit->covar);
            nlfit->covar = NULL;
        }
    }

    for (i = 0; i < nparam; i++) {
        if (gwy_isinf(param[i]) || gwy_isnan(param[i])) {
            sumr = -1.0;
            g_free(nlfit->covar);
            nlfit->covar = NULL;
            memcpy(param, origparam, nparam*sizeof(gdouble));
            break;
        }
    }

    if (nlfit->covar) {
        for (i = 0; i < nparam*(nparam + 1)/2; i++) {
             if (gwy_isinf(nlfit->covar[i]) || gwy_isnan(nlfit->covar[i])) {
                 sumr = -1.0;
                 g_free(nlfit->covar);
                 nlfit->covar = NULL;
                 memcpy(param, origparam, nparam*sizeof(gdouble));
                 break;
             }
        }
    }

fail:
    g_free(origparam);
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
    g_free(lmap);

    return sumr;
}

/**
 * gwy_math_nlfit_derive:
 * @x: The value to compute the derivative at.
 * @nparam: The nuber of parameters.
 * @param: Array of parameters (of size @nparam).
 * @fixed_param: Which parameters should be treated as fixed (corresponding
 *               entries are set to %TRUE).
 * @func: The fitted function.
 * @user_data: User data to be passed to @func.
 * @diff: Array where the put the result to.
 * @success: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Numerically computes the partial derivatives of a function.
 *
 * This is a legacy name for function gwy_math_nlfit_diff().
 **/
void
gwy_math_nlfit_derive(gdouble x,
                      gint nparam,
                      const gdouble *param,
                      const gboolean *fixed_param,
                      GwyNLFitFunc func,
                      gpointer user_data,
                      gdouble *diff,
                      gboolean *success)
{
    return gwy_math_nlfit_diff(x, nparam, param, fixed_param, func, user_data,
                               diff, success);
}

/**
 * gwy_math_nlfit_diff:
 * @x: The value to compute the derivative at.
 * @nparam: The nuber of parameters.
 * @param: Array of parameters (of size @nparam).
 * @fixed_param: Which parameters should be treated as fixed (corresponding
 *               entries are set to %TRUE).
 * @func: The fitted function.
 * @user_data: User data to be passed to @func.
 * @der: Array where the put the result to.
 * @success: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Numerically computes the partial derivatives of a function.
 *
 * Since: 2.46
 **/
void
gwy_math_nlfit_diff(gdouble x,
                    gint nparam,
                    const gdouble *param,
                    const gboolean *fixed_param,
                    GwyNLFitFunc func,
                    gpointer user_data,
                    gdouble *der,
                    gboolean *success)
{
    gdouble *param_tmp;
    gdouble hj, left, right;
    gint j;

    param_tmp = g_newa(gdouble, nparam);
    memcpy(param_tmp, param, nparam*sizeof(gdouble));

    for (j = 0; j < nparam; j++) {
        if (fixed_param && fixed_param[j]) {
            der[j] = 0.0;
            continue;
        }

        hj = (fabs(param_tmp[j]) + FitSqrtMachEps) * FitSqrtMachEps;
        param_tmp[j] -= hj;
        left = func(x, nparam, param_tmp, user_data, success);
        if (!*success)
            return;

        param_tmp[j] += 2 * hj;
        right = func(x, nparam, param_tmp, user_data, success);
        if (!*success)
            return;

        der[j] = (right - left)/2/hj;
        param_tmp[j] = param[j];
    }
}

/**
 * gwy_math_nlfit_diff_idx:
 * @i: Data index from the set {0, 1, 2, ..., ndata-1}.
 * @nparam: The nuber of parameters.
 * @param: Array of parameters (of size @nparam).
 * @fixed_param: Which parameters should be treated as fixed (corresponding
 *               entries are set to %TRUE).
 * @func: The fitted function.
 * @user_data: User data to be passed to @func.
 * @der: Array where the put the result to.
 * @success: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Numerically computes the partial derivatives of an opaque function.
 *
 * This function cannot be passed as derivative calculation function to
 * gwy_math_nlfit_new_idx() because it is not of the #GwyNLFitIdxDiffFunc.
 * Just pass %NULL to gwy_math_nlfit_new_idx() if you want automatic
 * numerical derivatives.
 *
 * You can employ this function in your own #GwyNLFitIdxDiffFunc function
 * if you need to modify the derivatives somehow, for instance to apply
 * weighting.
 *
 * Since: 2.46
 **/
void
gwy_math_nlfit_diff_idx(guint i,
                        gint nparam,
                        const gdouble *param,
                        const gboolean *fixed_param,
                        GwyNLFitIdxFunc func,
                        gpointer user_data,
                        gdouble *der,
                        gboolean *success)
{
    gdouble *param_tmp;
    gdouble hj, left, right;
    gint j;

    param_tmp = g_newa(gdouble, nparam);
    memcpy(param_tmp, param, nparam*sizeof(gdouble));

    for (j = 0; j < nparam; j++) {
        if (fixed_param && fixed_param[j]) {
            der[j] = 0.0;
            continue;
        }

        hj = (fabs(param_tmp[j]) + FitSqrtMachEps) * FitSqrtMachEps;
        param_tmp[j] -= hj;
        left = func(i, param_tmp, user_data, success);
        if (!*success)
            return;

        param_tmp[j] += 2 * hj;
        right = func(i, param_tmp, user_data, success);
        if (!*success)
            return;

        der[j] = (right - left)/2/hj;
        param_tmp[j] = param[j];
    }
}

static gdouble
gwy_math_nlfit_residua(GwyNLFitFunc func,
                       GwyNLFitIdxFunc func_idx,
                       guint ndata,
                       const gdouble *x,
                       const gdouble *y,
                       const gdouble *weight,
                       guint nparam,
                       const gdouble *param,
                       gpointer user_data,
                       gdouble *resid,
                       gboolean *success)
{
    gdouble s = 0.0;
    gboolean ok = TRUE;
    guint i;

    g_return_val_if_fail(func || func_idx, -1.0);

    if (func_idx) {
        for (i = 0; i < ndata && ok; i++) {
            resid[i] = func_idx(i, param, user_data, &ok);
            s += resid[i] * resid[i];
        }
    }
    else {
        for (i = 0; i < ndata && ok; i++) {
            resid[i] = func(x[i], nparam, param, user_data, &ok) - y[i];
            if (weight)
                resid[i] *= weight[i];
            s += resid[i] * resid[i];
        }
    }
    *success = ok;

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
 * gwy_math_nlfit_succeeded:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 *
 * Obtains the status of the last fitting.
 *
 * Fitting failure can be (and usually should be) also determined by checking
 * for negative return value of gwy_math_nlfit_fit() or
 * gwy_math_nlfit_fit_full().  This function allows to test it later.
 *
 * Returns: %TRUE if the last fitting suceeded, %FALSE if it failed.
 *
 * Since: 2.7
 **/
gboolean
gwy_math_nlfit_succeeded(GwyNLFitter *nlfit)
{
    if ((!nlfit->covar && nlfit->dispersion >= 0.0)
        || (nlfit->covar && nlfit->dispersion < 0.0)) {
        g_warning("Covar and dispersion do not agree on whether the fit "
                  "was successful.");
        return FALSE;
    }

    return nlfit->covar != NULL;
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
 * This function can be used only after a successful fit.
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
 * This function can be used only after a successful fit.
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
 * gwy_math_nlfit_set_callbacks:
 * @nlfit: A Marquardt-Levenberg nonlinear fitter.
 * @set_fraction: Function that sets fraction to output (or %NULL).
 * @set_message: Function that sets message to output (or %NULL).
 *
 * Sets callbacks reporting a non-linear least squares fitter progress.
 *
 * Since: 2.46
 **/
void
gwy_math_nlfit_set_callbacks(GwyNLFitter *fitter,
                             GwySetFractionFunc set_fraction,
                             GwySetMessageFunc set_message)
{
    GwyNLFitterPrivate *priv;

    priv = find_private_data(fitter, TRUE);
    priv->set_fraction = set_fraction;
    priv->set_message = set_message;
}

static GwyNLFitterPrivate*
find_private_data(GwyNLFitter *fitter, gboolean do_create)
{
    GwyNLFitterPrivate *priv;
    GList *l;

    for (l = private_fitter_data; l; l = g_list_next(private_fitter_data)) {
        priv = (GwyNLFitterPrivate*)l->data;
        if (priv->fitter == fitter)
            return priv;
    }

    if (!do_create)
        return NULL;

    priv = g_new0(GwyNLFitterPrivate, 1);
    priv->fitter = fitter;
    private_fitter_data = g_list_prepend(private_fitter_data, priv);
    return priv;
}

static void
free_private_data(GwyNLFitter *fitter)
{
    GwyNLFitterPrivate *priv;
    GList *l;

    for (l = private_fitter_data; l; l = g_list_next(private_fitter_data)) {
        priv = (GwyNLFitterPrivate*)l->data;
        if (priv->fitter == fitter) {
            private_fitter_data = g_list_delete_link(private_fitter_data, l);
            g_free(priv);
            return;
        }
    }
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
 * its derivative (as #GwyNLFitDerFunc). For functions for whose analytic
 * derivative is not available or very impractical, gwy_math_nlfit_derive()
 * (computing the derivative numerically) can be used instead.
 *
 * A fitter can be then repeatedly used on different data either in
 * gwy_math_nlfit_fit(), or gwy_math_nlfit_fit_full() when there are some
 * fixed or linked parameters.  Arbitrary additional (non-fitting) parameters
 * can be passed to the fited function in @user_data.
 *
 * After a successfull fit additional fit information can be obtained with
 * gwy_math_nlfit_get_dispersion(), gwy_math_nlfit_get_correlations(),
 * gwy_math_nlfit_get_sigma(). Note these functions may be used only after a
 * successfull fit. When a fitter is no longer needed, it should be freed with
 * gwy_math_nlfit_free().
 *
 * Several common functions are also available as fitting presets that can be
 * fitted with gwy_nlfit_preset_fit().  See #GwyNLFitPreset for details.
 **/

/**
 * GwyNLFitFunc:
 * @x: The value to compute the function at.
 * @nparam: The number of parameters (size of @param).
 * @param: Parameters.
 * @user_data: User data as passed to gwy_math_nlfit_fit().
 * @success: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Fitting function type for real-valued independent variables.
 *
 * Returns: The function value at @x.
 */

/**
 * GwyNLFitDerFunc:
 * @x: x-data as passed to gwy_math_nlfit_fit().
 * @nparam: The number of parameters (size of @param).
 * @param: Parameters.
 * @fixed_param: Which parameters should be treated as fixed (corresponding
 *               entries are set to %TRUE).
 * @func: The fitted function.
 * @user_data: User data as passed to gwy_math_nlfit_fit().
 * @der: Array where the @nparam partial derivatives by each parameter are
 *       to be stored.
 * @success: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Fitting function partial derivative type for real-valued independent
 * variables.
 */

/**
 * GwyNLFitIdxFunc:
 * @i: Data index from the set {0, 1, 2, ..., ndata-1}.
 * @param: Parameters.
 * @user_data: User data as passed to gwy_math_nlfit_fit().
 * @success: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Fitting function type for opaque indexed data.
 *
 * Note that unlike #GwyNLFitFunc which returns just the function value this
 * function must return the <emphasis>difference</emphasis> between the
 * function value and fitted data value.  When opaque data are fitted the
 * fitter does not know what the data values are.
 *
 * The function must take care of weighting.  The difference should be
 * multiplied by the inverse of the (unsquared) estimated error of the
 * @i-th data point.  Not multiplying by anything correspond to using the
 * default unit weights.
 *
 * Returns: Difference between the function value and data in the @i-th data
 * point.
 *
 * Since: 2.46
 **/

/**
 * GwyNLFitIdxDiffFunc:
 * @i: Data index from the set {0, 1, 2, ..., ndata-1}.
 * @param: Parameters.
 * @fixed_param: Which parameters should be treated as fixed (corresponding
 *               entries are set to %TRUE).
 * @func: The fitted function.
 * @user_data: User data as passed to gwy_math_nlfit_fit().
 * @der: Array where the @nparam partial derivatives by each parameter are
 *         to be stored.
 * @success: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Fitting function partial derivatives type for opaque indexed data.
 *
 * The function must take care of weighting.  The derivatives should be
 * multiplied by the inverse of the (unsquared) estimated error of the
 * @i-th data point.  Not multiplying by anything correspond to using the
 * default unit weights.
 *
 * Since: 2.46
 **/

/**
 * GwyNLFitter:
 * @fmarq: Evaluates the fitted function.
 * @dmarq: Evaluates derivatives of the fitted function.
 * @maxiter: Maximum number of iteration.
 * @eval: %TRUE if last evaluation succeeded.
 * @covar: Covariance matrix (set upon successful fit).
 * @dispersion: Mean residual sum of squares per point, set to -1 on failure.
 * @mfi: Lambda parameter is multiplied by it.  Probably keep at 1.
 * @mdec: Decrease of lambda parameter after an unsuccessful step.
 * @minc: Increase of lambda parameter after a successful step.
 * @mtol: If lambda parameter becomes zero it is set to this value.
 *
 * Non-linear least-squares fitter.
 *
 * Most of the fields should be considered private.  Examining @eval, @covar
 * and @dispersion can be useful, as well as setting @maxiter, @mdec or @minc.
 * The rest is better left untouched.
 */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
