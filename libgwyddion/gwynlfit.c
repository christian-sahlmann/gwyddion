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

#include <gwymacros.h>
#include <gwymath.h>
#include "gwynlfit.h"

/* Konstanta pro vypocet kroku stranou pri derivovani ve funkce marq_deriv
 */
#define FitSqrtMachEps  1e-4

/* Konstanta pro rozhodnuti o ukonceni fitovaciho cyklu podle perlativniho rozdilu 
 * rezidualnilnich souctu mezi jednotlivymi kroky
 */
#define EPS 1e-16

/* index in lower triangular sym. matrix (FIXME: a matrix cannot be both
 * lower triangular and symmetric...) */
#define IndexLongMat(i, j) \
    ((j) > (i) ? (((j) - 1)*(j)/2 + (i)) : ((i) - 1)*(i)/2 + (j))

/* XXX: brain damage:
 * all the functions whose names end with `1' use 1-based arrays since they
 * were originally written in Pascal (!)
 * never expose then to outer world, we must pretend everything is 0-based */
static void     gwy_math_choleski_solve1(gint dimA,
                                         gdouble *A,
                                         gdouble *B);
static gboolean gwy_math_choleski_decompose1(gint dimA,
                                             gdouble *A);
static gboolean gwy_math_sym_matrix_invert1(gint N,
                                            gdouble *A);

static gdouble gwy_math_nlfit_residua1(GwyNLFitState *ms,
                                       gint n_dat,
                                       gdouble *x,
                                       gdouble *y,
                                       gdouble *weight,
                                       gint n_par,
                                       gdouble *param,
                                       gpointer user_data,
                                       gdouble *resid);


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
GwyNLFitState*
gwy_math_nlfit_new(GwyNLFitFunc ff, GwyNLFitDerFunc df)
{
    GwyNLFitState *ms;

    ms = g_new0(GwyNLFitState, 1);
    ms->fmarq = ff;
    ms->dmarq = df;
    ms->mfi = 1.0;
    ms->mdec = 0.4;
    ms->minc = 10.0;
    ms->mtol = 1e-6;
    ms->maxiter = 50;
    ms->eval = FALSE;
    ms->dispersion = -1;
    ms->covar = NULL;

    return ms;
}

/**
 * gwy_math_nlfit_free:
 * @ms: A Marquardt-Levenberg nonlinear fitter.
 *
 * Completely frees a Marquardt-Levenberg nonlinear fitter.
 **/
void
gwy_math_nlfit_free(GwyNLFitState *ms)
{
    g_free(ms->covar);
    ms->covar = NULL;
    g_free(ms);
}

/**
 * gwy_math_nlfit_fit:
 * @ms: A Marquardt-Levenberg nonlinear fitter.
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
 * Performs a nonlinear fit of @ms function on (@x,@y) data.
 *
 * Returns: The final residual sum, a negative number in the case of failure.
 **/
gdouble
gwy_math_nlfit_fit(GwyNLFitState *ms,
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

    gint covar_size = n_par*(n_par + 1)/2 + 1;

    gdouble *der;
    gdouble *v;
    gdouble *xr;
    gdouble *saveparam;
    gdouble *resid;

    gdouble *a;
    gdouble *save_a;
    gboolean Step1 = TRUE;
    gboolean end = FALSE;

    gint i, j, k;

    gdouble sumr1;
    gint miter = 0;

    /* XXX: brain damage:
     * we have to shift the arrays by one to simulate 1-based indexing,
     * param has to shifted back when passed back to user! */
    x -= 1;
    y -= 1;
    weight -= 1;
    param -= 1;

    resid = g_new(gdouble, n_dat+1);
    sumr1 = gwy_math_nlfit_residua1(ms, n_dat, x, y, weight, n_par, param,
                                    user_data, resid);
    sumr = sumr1;

    if (ms->eval == FALSE) {
        g_warning("Initial residua evaluation failed");
        g_free(resid);
        return -1;
    }

    der = g_new(gdouble, n_par+1);
    v = g_new(gdouble, n_par+1);
    xr = g_new(gdouble, n_par+1);
    saveparam = g_new(gdouble, n_par+1);
    a = g_new(gdouble, covar_size);
    save_a = g_new(gdouble, covar_size);

    if (ms->eval) {
        do {                    /* Hlavni iteracni cyklus*/
            if (Step1) {
                mlambda *= ms->mdec;
                sumr = sumr1;

                for (i = 0; i < covar_size; i++)
                    a[i] = 0;
                for (i = 0; i <= n_par; i++)
                    v[i] = 0;

                /* Vypocet J'J a J'r*/
                for (i = 1; i <= n_dat; i++) {
                    /* XXX: has to shift things back */
                    ms->dmarq(i-1, x+1, n_par, param+1, ms->fmarq,
                              user_data, der+1, &ms->eval);
                    if (!ms->eval)
                        break;
                    for (j = 1; j <= n_par; j++) {
                        gint q = j * (j - 1)/2;

                        v[j] += weight[i] * der[j] * resid[i]; /* Do vypoctu J'r*/
                        for (k = 1; k <= j; k++)    /* Do vypoctu J'J*/
                            a[q + k] += weight[i] * der[j] * der[k];
                    }
                }
                if (ms->eval) {
                    for (i = 1; i < covar_size; i++)
                        save_a[i] = a[i];
                    for (i = 1; i <= n_par; i++)
                        saveparam[i] = param[i];
                }
                else
                    end = TRUE;
            }
            if (!end) {
                gboolean posdef = FALSE;
                gboolean first = TRUE;      /*  first indikuje 1.pruchod*/
                gint count = 0;

                while (!posdef) {
                    if (!first)
                        for (i = 0; i < covar_size; i++)
                            a[i] = save_a[i];
                    else
                        first = FALSE;
                    for (j = 1; j <= n_par; j++) {
                        /* Doplneni diagonalnich prvku */
                        gint q = j * (j + 1)/2;        /* Index diag.prvku*/

                        a[q] = save_a[q] * (1.0 + mlambda) + ms->mfi * mlambda;
                        xr[j] = -v[j];
                    }
                    posdef = gwy_math_choleski_decompose1(n_par, a);   /* Choleskeho rozklad J'J v A*/
                    if (!posdef) {
                        mlambda *= ms->minc;     /* Provede zvetseni "lambda"*/
                        if (mlambda == 0.)
                            mlambda = ms->mtol;
                    }
                }
                gwy_math_choleski_solve1(n_par, a, xr);

                for (i = 1; i <= n_par; i++) {
                    param[i] = saveparam[i] + xr[i];
                    if (fabs(param[i] - saveparam[i]) == 0)
                        count++;
                }
                if (count == n_par)
                    end = TRUE;
                else {
                    sumr1 = gwy_math_nlfit_residua1(ms, n_dat, x, y, weight,
                                                    n_par, param,
                                                    user_data, resid);
                    if ((sumr1 == 0)
                        || (miter > 2 && fabs((sumr - sumr1)/sumr1) < EPS))
                        end = TRUE;
                    if (!ms->eval || sumr1 >= sumr) {
                        mlambda *= ms->minc;     /* Provede zvetseni "lambda"*/
                        if (mlambda == 0.0)
                            mlambda = ms->mtol;
                        Step1 = FALSE;
                    }
                    else
                        Step1 = TRUE;
                }
            }

            if (++miter >= ms->maxiter)
                end = TRUE;
        } while (!end);

    }
    sumr1 = sumr;

    g_free(ms->covar);
    ms->covar = NULL;

    if (ms->eval) {
        if (gwy_math_sym_matrix_invert1(n_par, save_a)) {
            ms->covar = g_new0(gdouble, covar_size);
            for (i = 0; i < covar_size; i++)
                ms->covar[i] = save_a[i];
            ms->dispersion = sumr/(n_dat - n_par);
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
 * Numerically compute the partial derivations of @ff
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
gwy_math_nlfit_residua1(GwyNLFitState *ms,
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

    ms->eval = TRUE;
    for (i = 1; (i <= n_dat) && (ms->eval == TRUE); i++) {
        resid[i] = ms->fmarq(x[i], n_par, param+1, user_data, &ms->eval) - y[i];
        s += resid[i] * resid[i] * weight[i];
    }
    return s;
}

gdouble
gwy_math_nlfit_get_sigma(GwyNLFitState *ms, gint par)
{
    g_return_val_if_fail(ms->covar, G_MAXDOUBLE);

    par++;
    return sqrt(ms->dispersion * ms->covar[IndexLongMat(par, par)]);
}

gdouble
gwy_math_nlfit_get_correlations(GwyNLFitState *ms, gint par1, gint par2)
{
    gdouble Pom;

    g_return_val_if_fail(ms->covar, G_MAXDOUBLE);

    if (par1 == par2)
        return 1.0;

    par1++;
    par2++;
    Pom = ms->covar[IndexLongMat(par1, par1)]
          * ms->covar[IndexLongMat(par2, par2)];
    if (Pom == 0) {
        g_warning("Zero element in covar matrix");
        return G_MAXDOUBLE;
    }

    return ms->covar[IndexLongMat(par1, par2)]/sqrt(Pom);
}


/* choleskeho rozklad*/
static gboolean
gwy_math_choleski_decompose1(gint dimA, gdouble *A)
{
    gint i, j, k, m, q;
    gdouble s;

    for (j = 1; j <= dimA; j++) {
        q = j * (j + 1)/2;
        if (j > 1) {
            for (i = j; i <= dimA; i++) {
                m = i * (i - 1)/2 + j;
                s = A[m];
                for (k = 1; k < j; k++)
                    s -= A[m - k] * A[q - k];
                A[m] = s;
            }
        }
        if (A[q] <= 0)
            return FALSE;

        s = sqrt(A[q]);
        for (i = j; i <= dimA; i++) {
            m = i * (i - 1)/2 + j;
            A[m] /= s;
        }
    }

    return TRUE;
}

/*reseni soustavy choleskeho metodou */
static void
gwy_math_choleski_solve1(gint dimA, gdouble *A, gdouble *B)
{
    gint i, j, pom, q;

    B[1] /= A[1];
    if (dimA > 1) {
        q = 1;
        for (i = 2; i <= dimA; i++) {
            for (j = 1; j <= i - 1; j++)
                B[i] -= A[++q] * B[j];
            B[i] /= A[++q];
        }
    }
    pom = dimA * (dimA + 1)/2;
    B[dimA] /= A[pom];
    if (dimA > 1) {
        for (i = dimA; i >= 2; i--) {
            q = i * (i - 1)/2;
            for (j = 1; j <= i - 1; j++)
                B[j] -= B[i] * A[q + j];
            B[i - 1] /= A[q];
        }
    }
}

/* inverze symetricke matice */
static gboolean
gwy_math_sym_matrix_invert1(gint N, gdouble *A)
{

    gint Q = 0, M;
    gdouble S, T;
    gdouble *X;
    gint K, I, J;

    X = g_new(gdouble, N*(N + 1)/2 + 2);
    for (K = N; K >= 1; K--) {
        S = A[1];
        if (S <= 0) {
            g_free(X);
            return FALSE;
        }
        M = 1;
        for (I = 2; I <= N; I++) {
            Q = M;
            M += I;
            T = A[Q + 1];
            X[I] = -T/S;      /* note use temporary X*/
            if (I > K)
                X[I] = -X[I];
            for (J = Q + 2; J <= M; J++)
                A[J - I] = A[J] + T * X[J - Q];
        }
        Q--;
        A[M] = 1.0/S;
        for (I = 2; I <= N; I++)
            A[Q + I] = X[I];
    }
    g_free(X);

    return TRUE;
}

/************************** Documentation ****************************/

/**
 * GwyNLFitFunc:
 * @x: hodnota, kde se pocita
 * @n_par: pocet parametru
 * @b: parametry, zacinaji od 1 do n_par vcetne
 * @fres: uspesnost vypoctu.
 *
 * Fitting function type.
 *
 * Returns: The value at @x.
 */

/**
 * GwyNLFitDerFunc:
 * i: index prvku ve kterem se derivace pocita
 * dat_x: pole s hodnotami x, pocitaji se derivace v x[i]
 * n_par: pocet parametru
 * b: parametry, zacinaji od 1 do n_par vcetne
 * d: vystupni pole s derivacemi podle jednotlivych parametru
 * fmarq: fitovaci funkce
 * dres: uspesnost vypoctu
 *
 * Fitting function partial derivation type.
 */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
