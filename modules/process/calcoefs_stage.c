/*
 *  @(#) $Id: stage.c 8929 2008-12-31 13:40:16Z yeti-dn $
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/datafield.h>
#include <libprocess/gwyprocess.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define STAGE_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    NARGS = 4
};

enum {
    STAGE_OK   = 0,
    STAGE_DATA = 1,
    STAGE_EXPR = 2
};

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyDataObjectId objects[NARGS];
    gchar *name[NARGS];
    guint pos[NARGS];
} StageArgs;

typedef struct {
    StageArgs *args;
    GtkWidget *dialog;
    GtkWidget *data[NARGS];
} StageControls;

static gboolean     module_register           (void);
static void         stage                (GwyContainer *data,
                                               GwyRunType run);
static gboolean     stage_dialog         (StageArgs *args);
static void         stage_data_cb        (GwyDataChooser *chooser,
                                               StageControls *controls);
static const gchar* stage_check          (StageArgs *args);
static void         stage_do             (StageArgs *args);
static void         flip_xy              (GwyDataField *source, 
                                          GwyDataField *dest, 
                                          gboolean minor);


static const gchar default_expression[] = "d1 - d2";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Digital AFM data recalibration"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("stage",
                              (GwyProcessFunc)&stage,
                              N_("/Cali_bration/_3D Calibration/_Get From Stage map..."),
                              NULL,
                              STAGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Stage error mapping"));

    return TRUE;
}

void
stage(GwyContainer *data, GwyRunType run)
{
    StageArgs args;
    guint i;
    GwyContainer *settings;
    gint id;

    g_return_if_fail(run & STAGE_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id, 0);

    settings = gwy_app_settings_get();
    for (i = 0; i < NARGS; i++) {
        args.objects[i].data = data;
        args.objects[i].id = id;
    }

    if (stage_dialog(&args)) {
        stage_do(&args);
    }
}

static gboolean
stage_dialog(StageArgs *args)
{
    StageControls controls;
    GtkWidget *dialog, *table, *chooser,  *label;
    guint i, row, response;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Stage Error Map"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4 + NARGS, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(_("Operands:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    args->name[0] = g_strdup_printf("Original");
    args->name[1] = g_strdup_printf("Rotated");
    args->name[2] = g_strdup_printf("Shifted");
    args->name[3] = g_strdup_printf("Detail");
      for (i = 0; i < NARGS; i++) {
        //args->name[i] = g_strdup_printf("d_%d", i+1);
        label = gtk_label_new_with_mnemonic(args->name[i]);
        gwy_strkill(args->name[i], "_");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);

        chooser = gwy_data_chooser_new_channels();
        gwy_data_chooser_set_active(GWY_DATA_CHOOSER(chooser),
                                    args->objects[i].data, args->objects[i].id);
        g_signal_connect(chooser, "changed",
                         G_CALLBACK(stage_data_cb), &controls);
        g_object_set_data(G_OBJECT(chooser), "index", GUINT_TO_POINTER(i));
        gtk_table_attach(GTK_TABLE(table), chooser, 1, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), chooser);
        controls.data[i] = chooser;

        row++;
    }
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
stage_data_cb(GwyDataChooser *chooser,
                   StageControls *controls)
{
    StageArgs *args;
    guint i;

    args = controls->args;
    i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(chooser), "index"));
    args->objects[i].data = gwy_data_chooser_get_active(chooser,
                                                        &args->objects[i].id);
}

void
get_object_list(GwyDataField *data, GwyDataField *kernel, gdouble threshold, 
                gdouble *xs, gdouble *ys, gint *nobjects, GwyCorrelationType type)
{
    GwyDataField *score = gwy_data_field_new_alike(data, 0);
    GwyDataField *retfield;
    gdouble *sdata, *maxval, min, max;
    gint i, *grains, *maxpos, ngrains;

    gwy_data_field_correlate(data, kernel, score, type);
    max = gwy_data_field_get_max(score);
    min = gwy_data_field_get_min(score);

    retfield = gwy_data_field_duplicate(score);
    gwy_data_field_threshold(retfield, threshold, 0.0, 1.0); 

    grains = (gint *)g_malloc(gwy_data_field_get_xres(score)*gwy_data_field_get_yres(score)*sizeof(int));
    ngrains = gwy_data_field_number_grains(retfield, grains);

    maxpos = (gint *) g_malloc(ngrains*sizeof(gint));
    maxval = (gdouble *) g_malloc(ngrains*sizeof(gdouble));
    sdata = gwy_data_field_get_data(score);

    for (i=0; i<ngrains; i++) maxval[i] = -G_MAXDOUBLE;
    
    //find correlation maximum of each grain
    for (i=0; i<(gwy_data_field_get_xres(score)*gwy_data_field_get_yres(score)); i++)
    {
        if (grains[i]!=0) {
            if (maxval[grains[i]-1]<sdata[i]) {
                maxval[grains[i]-1]=sdata[i];
                maxpos[grains[i]-1]=i;
            }
        }
    }
    //return correlation maxima (x, y), TODO do this in future with subpixel precision;
    *nobjects = MIN(*nobjects, ngrains);
    for (i=0; i<(*nobjects); i++) {
        ys[i] = (int)(maxpos[i]/gwy_data_field_get_xres(retfield));
        xs[i] = maxpos[i] - ys[i]*gwy_data_field_get_xres(retfield);
    }

    g_object_unref(score);
    g_object_unref(retfield);
    g_free(grains);
    g_free(maxpos);
    g_free(maxval);

}

void fill_matrix(gdouble *xs, gdouble *ys, gint n, gint tl, 
                 gdouble xxshift, gdouble xyshift,
                 gdouble yxshift, gdouble yyshift, 
                 GwyDataField *x_matrix, GwyDataField *y_matrix, gint nn)
{
    gint i, j, k, pos;
    gdouble tlx = xs[tl];
    gdouble tly = ys[tl];
    gdouble idxpos, idypos;
    gdouble min, dist;

    for (j=0; j<nn; j++)
    {
        for (i=0; i<nn; i++)
        {
            //determine ideal position
            idxpos = tlx + i*xxshift + j*yxshift;
            idypos = tly + i*xyshift + j*yyshift;

            //find closest point
            min = G_MAXDOUBLE;
            for (k=0; k<n; k++) {
                if ((dist = (xs[k]-idxpos)*(xs[k]-idxpos) + (ys[k]-idypos)*(ys[k]-idypos))<min) {
                    min = dist;
                    pos = k;
                }
            }
            gwy_data_field_set_val(x_matrix, i, j, xs[pos]);
            gwy_data_field_set_val(y_matrix, i, j, ys[pos]);
//            printf("Point %d %d, idpos %g %g, found pos %g %g\n", 
//                   i, j, idxpos, idypos, xs[pos], ys[pos]);
        }
    }


}

gdouble 
get_prod_grid(GwyDataField *a, GwyDataField *b, gdouble period)
{
    gint i, j;
    gint xres = gwy_data_field_get_xres(a);
    gint yres = gwy_data_field_get_yres(a);
    gdouble suma, sumb;
    gint shift = -xres/2;

    suma = sumb = 0;
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            suma += gwy_data_field_get_val(a, i, j)*((i+shift)*period) - gwy_data_field_get_val(b, i, j)*((j+shift)*period);
            sumb += period*period*((i+shift)*(i+shift) + (j+shift)*(j+shift));
        }
    }

    return suma/sumb;
}

void
stage_calibration(GwyDataField *x_orig, GwyDataField *y_orig, 
                  GwyDataField *x_shif, GwyDataField *y_shif, 
                  GwyDataField *x_rot, GwyDataField *y_rot, gdouble period)
{
    GwyDataField *v0x, *v0y, *u0x, *u0y, *v1x, *v1y, *u1x, *u1y, *v2x, *v2y, *u2x, *u2y;
    GwyDataField *P, *Q, *pr, *pi, *qr, *qi, *ar, *ai, *br, *bi;
    GwyDataField *W, *T, *Nu, *Sigma, *Tau, *wr, *wi, *tr, *ti, *sigmar, *sigmai, *taur, *taui, *nur, *nui, *gammar, *gammai;
    GwyDataField *Fxr, *Fxi, *Fyr, *Fyi, *Gx, *Gy;
    gdouble theta0, t0x, t0y, theta1, t1x, t1y, rstage, ostage, xix, xiy, xit, sumA, sumB, sumC;
    gdouble *zx, *zy, arg;
    
    gint i, j, shift;
    gint xres = gwy_data_field_get_xres(x_orig);
    gint yres = gwy_data_field_get_yres(y_orig);
    gdouble sumoscale, sumrscale, suma, sumb;
    shift = -xres/2;

    v0x = gwy_data_field_new_alike(x_orig, FALSE);
    v0y = gwy_data_field_new_alike(y_orig, FALSE);
    u0x = gwy_data_field_new_alike(x_orig, FALSE);
    u0y = gwy_data_field_new_alike(y_orig, FALSE);
    v1x = gwy_data_field_new_alike(x_orig, FALSE);
    v1y = gwy_data_field_new_alike(y_orig, FALSE);
    u1x = gwy_data_field_new_alike(x_orig, FALSE);
    u1y = gwy_data_field_new_alike(y_orig, FALSE);
    v2x = gwy_data_field_new_alike(x_orig, FALSE);
    v2y = gwy_data_field_new_alike(y_orig, FALSE);
    u2x = gwy_data_field_new_alike(x_orig, FALSE);
    u2y = gwy_data_field_new_alike(y_orig, FALSE);
    P = gwy_data_field_new_alike(x_orig, FALSE);
    Q = gwy_data_field_new_alike(x_orig, FALSE);
    pr = gwy_data_field_new_alike(x_orig, FALSE);
    qr = gwy_data_field_new_alike(x_orig, FALSE);
    pi = gwy_data_field_new_alike(x_orig, FALSE);
    qi = gwy_data_field_new_alike(x_orig, FALSE);
    ar = gwy_data_field_new_alike(x_orig, FALSE);
    br = gwy_data_field_new_alike(x_orig, FALSE);
    ai = gwy_data_field_new_alike(x_orig, FALSE);
    bi = gwy_data_field_new_alike(x_orig, FALSE);

    W = gwy_data_field_new_alike(x_orig, FALSE);
    T = gwy_data_field_new_alike(x_orig, FALSE);
    Nu = gwy_data_field_new_alike(x_orig, FALSE);
    Sigma = gwy_data_field_new_alike(x_orig, FALSE);
    Tau = gwy_data_field_new_alike(x_orig, FALSE);
    wr = gwy_data_field_new_alike(x_orig, FALSE);
    wi = gwy_data_field_new_alike(x_orig, FALSE);
    tr = gwy_data_field_new_alike(x_orig, FALSE);
    ti = gwy_data_field_new_alike(x_orig, FALSE);
    sigmar = gwy_data_field_new_alike(x_orig, FALSE);
    sigmai = gwy_data_field_new_alike(x_orig, FALSE);
    taur = gwy_data_field_new_alike(x_orig, FALSE);
    taui = gwy_data_field_new_alike(x_orig, FALSE);
    nur = gwy_data_field_new_alike(x_orig, FALSE);
    nui = gwy_data_field_new_alike(x_orig, FALSE);
    gammar = gwy_data_field_new_alike(x_orig, FALSE);
    gammai = gwy_data_field_new_alike(x_orig, FALSE);

    Fxr = gwy_data_field_new_alike(x_orig, FALSE);
    Fyr = gwy_data_field_new_alike(x_orig, FALSE);
    Fxi = gwy_data_field_new_alike(x_orig, FALSE);
    Fyi = gwy_data_field_new_alike(x_orig, FALSE);
    Gx = gwy_data_field_new_alike(x_orig, FALSE);
    Gy = gwy_data_field_new_alike(x_orig, FALSE);



    zx = (gdouble *) g_malloc(xres*sizeof(gdouble));
    zy = (gdouble *) g_malloc(xres*sizeof(gdouble));

    /***********************************   step 1  ************************************/
    //create v0x, v0y
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(v0x, i, j, gwy_data_field_get_val(x_orig, i, j)-((i+shift)*period));
            gwy_data_field_set_val(v0y, i, j, gwy_data_field_get_val(y_orig, i, j)-((j+shift)*period));
        }
    }

    //eq 25a, 25b, 26
    t0x = gwy_data_field_get_avg(v0x);
    t0y = gwy_data_field_get_avg(v0y);
    theta0 = get_prod_grid(v0y, v0x, period);
    printf("Original pre: ts: %g %g, theta %g\n", t0x, t0y, theta0);

    //eq 27a, b, tested that ts and theta itself compensates shift and rotation to zero
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(u0x, i, j, gwy_data_field_get_val(v0x, i, j) - t0x + theta0*period*(j+shift));
            gwy_data_field_set_val(u0y, i, j, gwy_data_field_get_val(v0y, i, j) - t0y - theta0*period*(i+shift));
        }
    }
 
   //check the aligned data, just for sure:
    t0x = gwy_data_field_get_avg(u0x);
    t0y = gwy_data_field_get_avg(u0y);
    theta0 = get_prod_grid(u0y, u0x, period);
    printf("Original post: ts: %g %g, theta %g\n", t0x, t0y, theta0);

    /***********************************   step 2  ************************************/
    //create v1x, v1y
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(v1x, i, j, gwy_data_field_get_val(x_rot, i, j)-((i+shift)*period));
            gwy_data_field_set_val(v1y, i, j, gwy_data_field_get_val(y_rot, i, j)-((j+shift)*period));
        }
    }
    t1x = gwy_data_field_get_avg(v1x);
    t1y = gwy_data_field_get_avg(v1y);
    theta1 = get_prod_grid(v1y, v1x, period);
    printf("Rotated pre: ts: %g %g, theta %g\n", t1x, t1y, theta1);

    //eq 27a, b, tested that ts and theta itself compensates shift and rotation to zero
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(u1x, i, j, gwy_data_field_get_val(v1x, i, j) - t1x + theta1*period*(j+shift));
            gwy_data_field_set_val(u1y, i, j, gwy_data_field_get_val(v1y, i, j) - t1y - theta1*period*(i+shift));
        }
    }
   //check the aligned data, just for sure:
    t1x = gwy_data_field_get_avg(u1x);
    t1y = gwy_data_field_get_avg(u1y);
    theta1 = get_prod_grid(u1y, u1x, period);
    printf("Rotated post: ts: %g %g, theta %g\n", t1x, t1y, theta1);

    
    /***********************************   step 3  ************************************/
  
    sumoscale = sumrscale = sumb = 0;
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            sumoscale += gwy_data_field_get_val(u0x, i, j)*((j+shift)*period) + gwy_data_field_get_val(u0y, i, j)*((i+shift)*period);
            sumrscale += -gwy_data_field_get_val(u1x, i, j)*((j+shift)*period) - gwy_data_field_get_val(u1y, i, j)*((i+shift)*period);
            sumb += period*period*((i+shift)*(i+shift) + (j+shift)*(j+shift));
        }
    }
 
    theta0 = get_prod_grid(u1x, u1y, period);
    theta1 = get_prod_grid(u0x, u0y, period);
    ostage = 0.5*(theta1 + sumoscale/sumb);
    rstage = 0.5*(theta0 + sumrscale/sumb);

    printf("Nonorthogonality: %g,   scale difference %g\n", ostage, rstage);
   
    /***********************************   step 4  ************************************/

    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(P, i, j, -2*ostage*((j+shift)*period) - 2*rstage*((i+shift)*period) 
                                   + gwy_data_field_get_val(u0x, i, j) - gwy_data_field_get_val(u1y, i, j));
            gwy_data_field_set_val(Q, i, j, -2*ostage*((i+shift)*period) + 2*rstage*((j+shift)*period) 
                                   + gwy_data_field_get_val(u0y, i, j) + gwy_data_field_get_val(u1x, i, j));
         }
    } 

    gwy_data_field_2dfft_raw(P, NULL, pr, pi, GWY_TRANSFORM_DIRECTION_FORWARD);
    gwy_data_field_2dfft_raw(Q, NULL, qr, qi, GWY_TRANSFORM_DIRECTION_FORWARD);


    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(ar, i, j,  0.5*(gwy_data_field_get_val(pr, i, j)        //FIXME swap i j for br
                                                 + gwy_data_field_get_val(qr, i, yres-j-1)));
            gwy_data_field_set_val(br, i, yres-j-1,  0.5*(-gwy_data_field_get_val(pr, i, j)
                                                 + gwy_data_field_get_val(qr, i, yres-j-1)));
        }
    }
  
    printf("End of step 4: real part of a,b:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("(%g, %g)  ", gwy_data_field_get_val(ar, i, j), gwy_data_field_get_val(br, i, j));
        }
        printf("\n");
    } 


    /***********************************   step 5  ************************************/
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(u2x, i, j, gwy_data_field_get_val(x_shif, i, j)-((i+shift)*period));
            gwy_data_field_set_val(u2y, i, j, gwy_data_field_get_val(y_shif, i, j)-((j+shift)*period));
        }
    }

    /***********************************   step 6  ************************************/

    for (i=0; i<xres; i++) zx[i] = zy[i] = 0;

    for (i=0; i<(xres-1); i++)
    {
        for (j=0; j<yres; j++)
        {
            zx[i] += gwy_data_field_get_val(u2x, i, j) - gwy_data_field_get_val(u0x, i, j);
            zy[i] += gwy_data_field_get_val(u2y, i, j) - gwy_data_field_get_val(u0y, i, j);
        }
    } 

    printf("Step 6: zxs:\n");
    for (i=0; i<xres; i++)
    {
        printf("%g   %g\n  ", zx[i], zy[i]);
    }
    xix = 0;
    sumb = 0;
    for (i=0; i<(xres-1); i++)
    {
        suma = 0;
        for (j=(i+1); j<xres; j++) suma += (j+shift)*period;

        xix += zx[i]*suma;
        sumb += suma;
    }
    printf("xix: %g, sumb %g\n", xix, sumb);
    xix/=sumb;

    printf("xix: %g\n", xix);

    /***********************************   step 7  ************************************/

    //eq 66
    for (i=0; i<(xres-1); i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(W, i, j, gwy_data_field_get_val(u2x, i, j) - gwy_data_field_get_val(u0x, i, j));
            gwy_data_field_set_val(T, i, j, gwy_data_field_get_val(u2y, i, j) - gwy_data_field_get_val(u0y, i, j));
            gwy_data_field_set_val(Nu, i, j, 1);
        }
    } 
    for (j=0; j<yres; j++) 
    {
        suma = sumb = 0;
        for (i=0; i<(xres-1); i++) {
            suma += gwy_data_field_get_val(u2x, i, j) - gwy_data_field_get_val(u0x, i, j);
            sumb += gwy_data_field_get_val(u2y, i, j) - gwy_data_field_get_val(u0y, i, j);
        }

        gwy_data_field_set_val(W, xres-1, j, -suma);
        gwy_data_field_set_val(T, xres-1, j, -sumb);
        gwy_data_field_set_val(Nu, xres-1, j, -(xres-1));
    }
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(Sigma, i, j, -gwy_data_field_get_val(Nu, i, j)*(j+shift)*period);
            gwy_data_field_set_val(Tau, i, j, (i+shift)*period);
        }
    } 
    /*output all arrays*/
    printf("W matrix:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("%g  ", gwy_data_field_get_val(W, i, j));
        }
        printf("\n");
    } 
    printf("\n");
 
    printf("T matrix:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("%g  ", gwy_data_field_get_val(T, i, j));
        }
        printf("\n");
    } 
    printf("\n");
 
    printf("Nu matrix:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("%g  ", gwy_data_field_get_val(Nu, i, j));
        }
        printf("\n");
    } 
    printf("\n");
    printf("Sigma matrix:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("%g  ", gwy_data_field_get_val(Sigma, i, j));
        }
        printf("\n");
    } 
    printf("\n");
    printf("Tau matrix:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("%g  ", gwy_data_field_get_val(Tau, i, j));
        }
        printf("\n");
    } 
    printf("\n");

    //eq 68
    gwy_data_field_resample(W, 256, 256, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_2dfft_raw(W, NULL, wr, wi, GWY_TRANSFORM_DIRECTION_FORWARD);

    gwy_data_field_resample(T, 256, 256, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_2dfft_raw(T, NULL, tr, ti, GWY_TRANSFORM_DIRECTION_FORWARD);

    gwy_data_field_resample(Nu, 256, 256, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_2dfft_raw(Nu, NULL, nur, nui, GWY_TRANSFORM_DIRECTION_FORWARD);

    gwy_data_field_resample(Sigma, 256, 256, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_2dfft_raw(Sigma, NULL, sigmar, sigmai, GWY_TRANSFORM_DIRECTION_FORWARD);

    gwy_data_field_resample(Tau, 256, 256, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_2dfft_raw(Tau, NULL, taur, taui, GWY_TRANSFORM_DIRECTION_FORWARD);

    /*output all arrays*/
    gwy_data_field_2dfft_humanize(wr);
    gwy_data_field_2dfft_humanize(wi);
    gwy_data_field_2dfft_humanize(tr);
    gwy_data_field_2dfft_humanize(ti);
    gwy_data_field_2dfft_humanize(nur);
    gwy_data_field_2dfft_humanize(nui);
    gwy_data_field_2dfft_humanize(sigmar);
    gwy_data_field_2dfft_humanize(sigmai);
    gwy_data_field_2dfft_humanize(taur);
    gwy_data_field_2dfft_humanize(taui);

    gwy_data_field_resample(wr, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(wi, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(tr, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(ti, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(nur, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(nui, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(taur, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(taui, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(sigmar, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(sigmai, xres, yres, GWY_INTERPOLATION_BILINEAR);

    printf("w fft components:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("(%g, %g)  ", gwy_data_field_get_val(wr, i, j), gwy_data_field_get_val(wi, i, j));
        }
        printf("\n");
    } 
    printf("\n");
    printf("t fft components:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("(%g, %g)  ", gwy_data_field_get_val(tr, i, j), gwy_data_field_get_val(ti, i, j));
        }
        printf("\n");
    }
    printf("\n"); 
    printf("nu fft components:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("(%g, %g)  ", gwy_data_field_get_val(nur, i, j), gwy_data_field_get_val(nui, i, j));
        }
        printf("\n");
    } 
    printf("\n");
    printf("sigma fft components:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("(%g, %g)  ", gwy_data_field_get_val(sigmar, i, j), gwy_data_field_get_val(sigmai, i, j));
        }
        printf("\n");
    } 
    printf("\n");
    printf("tau fft components:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("(%g, %g)  ", gwy_data_field_get_val(taur, i, j), gwy_data_field_get_val(taui, i, j));
        }
        printf("\n");
    } 


    //eq 69
    printf("gamma:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            arg = 2*G_PI*(i+shift)/xres;
            gwy_data_field_set_val(gammar, i, j, (cos(arg)-1)/(2-2*cos(arg)));
            gwy_data_field_set_val(gammai, i, j, (sin(arg))/(2-2*cos(arg)));
            printf("(%g %g)  ",  gwy_data_field_get_val(gammar, i, j), gwy_data_field_get_val(gammai, i, j));
        }
        printf("\n");
    } 
 
    printf("determining xi_theta: (69a)\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("%g   ", (gwy_data_field_get_val(ar, i, j)/gwy_data_field_get_val(gammar, i, j) - gwy_data_field_get_val(wr, i, j))
                                 /gwy_data_field_get_val(sigmar, i, j));
        }
        printf("\n");
    }
    printf("determining xi_theta: (69b)\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("%g   ", (gwy_data_field_get_val(br, i, j)/gwy_data_field_get_val(gammar, i, j) - gwy_data_field_get_val(tr, i, j))
                                 /gwy_data_field_get_val(taur, i, j));
        }
        printf("\n");
    }
    xit = 1.2; //FIXME

    //eq 62  
    sumA = sumB = sumC = 0;
    for (i=0; i<(xres-1); i++)
    {
        suma = 0;
        for (j=(i+1); j<xres; j++) suma += (i+shift)*period;

        sumA += suma;
        sumB += (i+shift)*period*suma;
        sumC += zy[i]*suma;
    }

    xiy = -(xres*sumB*xit + sumC)/xres/sumA;

    printf("xitheta %g, xiy %g\n", xit, xiy);
 

    /****************************** step 8 ************************************/
    //eq 69, only imaginary part
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(ai, i, j,  gwy_data_field_get_val(gammai, i, j)*
                                   (gwy_data_field_get_val(wr, i, j) + xix*gwy_data_field_get_val(nur, i, j) + xit*gwy_data_field_get_val(sigmar, i, j))
                                   + gwy_data_field_get_val(gammar, i, j)*
                                   (gwy_data_field_get_val(wi, i, j) + xix*gwy_data_field_get_val(nui, i, j) + xit*gwy_data_field_get_val(sigmai, i, j)));

            gwy_data_field_set_val(bi, i, j,  gwy_data_field_get_val(gammai, i, j)*
                                   (gwy_data_field_get_val(tr, i, j) + xiy*gwy_data_field_get_val(nur, i, j) + xit*gwy_data_field_get_val(taur, i, j))
                                   + gwy_data_field_get_val(gammar, i, j)*
                                   (gwy_data_field_get_val(ti, i, j) + xiy*gwy_data_field_get_val(nui, i, j) + xit*gwy_data_field_get_val(taui, i, j)));
         }
    }
    
    for (j=0; j<yres; j++)
    {
        gwy_data_field_set_val(ai, -shift, j,  0.25*(gwy_data_field_get_val(pr, -shift, j)        //FIXME swap i j for br or move up to real part
                                               + gwy_data_field_get_val(qr, -shift, yres-j-1)));
        gwy_data_field_set_val(bi, -shift, j,  0.25*(-gwy_data_field_get_val(pr, -shift, j)
                                                      + gwy_data_field_get_val(qr, -shift, yres-j-1)));
    }

    printf("End of step 8: imaginary part of a,b:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("(%g, %g)  ", gwy_data_field_get_val(ai, i, j), gwy_data_field_get_val(bi, i, j));
        }
        printf("\n");
    } 

    /****************************** step 9 ************************************/
    //eq 38

    gwy_data_field_2dfft_dehumanize(ar);
    gwy_data_field_2dfft_dehumanize(ai);
    gwy_data_field_2dfft_dehumanize(br);
    gwy_data_field_2dfft_dehumanize(bi);

    gwy_data_field_resample(ar, 256, 256, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(ai, 256, 256, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_2dfft_raw(ar, ai, Fxr, Fxi, GWY_TRANSFORM_DIRECTION_BACKWARD);

    gwy_data_field_resample(br, 256, 256, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(bi, 256, 256, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_2dfft_raw(br, bi, Fyr, Fyi, GWY_TRANSFORM_DIRECTION_BACKWARD);

    gwy_data_field_resample(Fxr, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(Fxi, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(Fyr, xres, yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(Fyi, xres, yres, GWY_INTERPOLATION_BILINEAR);
  

    /****************************** step 10 ************************************/    
    //eq 11
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            gwy_data_field_set_val(Gx, i, j, (j+shift)*period*ostage + (i+shift)*period*rstage + gwy_data_field_get_val(Fxr, i, j));
            gwy_data_field_set_val(Gy, i, j, (i+shift)*period*ostage - (j+shift)*period*rstage + gwy_data_field_get_val(Fyr, i, j));
        }
    } 

    printf("End of step 10, final matrix Gx, Gy:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            printf("(%g, %g)  ", gwy_data_field_get_val(Gx, i, j), gwy_data_field_get_val(Gy, i, j));
        }
        printf("\n");
    } 






 }

static void
stage_do(StageArgs *args)
{
    GwyContainer *data;
    GwyDataField *original, *shifted, *rotated, *rback, *detail, *score;
    gdouble *xs, *ys;
    GwyDataField *x_orig, *y_orig, *x_shifted, *y_shifted, *x_rotated, *y_rotated, *x_unrotated, *y_unrotated;
    gint i, j, newid, ndat, mdat, noriginal, nshifted, nrotated;
    gdouble xxshift, xyshift, yxshift, yyshift, avxshift, avyshift;
    gdouble xmult, ymult;
    gdouble tlmin, nextmin, boundary;
    gdouble original_tlx, original_tly;
    gdouble sn, cs, icor, jcor, x, y;
    gint tl, present, next, nx, ny, nn, xres, yres;
    gdouble period = 0;
    GQuark quark;

    printf("starting\n");

    data = args->objects[0].data;
    quark = gwy_app_get_data_key_for_id(args->objects[0].id);
    original = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    data = args->objects[1].data;
    quark = gwy_app_get_data_key_for_id(args->objects[1].id);
    rotated = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    data = args->objects[2].data;
    quark = gwy_app_get_data_key_for_id(args->objects[2].id);
    shifted = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    data = args->objects[3].data;
    quark = gwy_app_get_data_key_for_id(args->objects[3].id);
    detail = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    //________________________________________________________original____________________________________________
    //find objects on original
    noriginal = 100;
    xs = (gdouble *)g_malloc(noriginal*sizeof(gdouble));
    ys = (gdouble *)g_malloc(noriginal*sizeof(gdouble));
    get_object_list(original, detail, 0.8, xs, ys, &noriginal, GWY_CORRELATION_NORMAL);
    printf("%d object locations in original\n", noriginal);
    /*for (i=0; i<noriginal; i++)
    {
         printf("%d %g %g\n", i, xs[i], ys[i]);
    }
     printf("_____________________\n");*/

    //create matrix of NxN object positions for original image, skip left edge positions; determine xshift
    
    //determine size of array first:
    //find top left object
    tl = 0;
    tlmin = G_MAXDOUBLE;
    for (i=0; i<noriginal; i++) {
        if ((xs[i]*ys[i])<tlmin) {
            tlmin = (xs[i]*ys[i]);
            tl = i;
        }
    }
    original_tlx = xs[tl];
    original_tly = ys[tl];
    printf("top left object is %g %g\n", xs[tl], ys[tl]);

    present = tl;
    //determine number of objects in x direction and xshift. Discriminate objects at the right edge
    nx = 0;
    boundary = 0;
    xyshift = avyshift = 0;
    xxshift = avxshift = 0;
    do {
        //find next closest object in x direction
        nextmin = G_MAXDOUBLE;
        for (i=0; i<noriginal; i++) {
            if (i==present || xs[i]<=xs[present]) continue; 
            if (nx>0 && fabs(ys[i]-ys[present])>(0.5*fabs(xxshift))) {
                //printf("too far in y (%d  %g > %g) \n", i, fabs(ys[i]-ys[present]), (0.5*fabs(yshift))); 
                continue;
            }
            if (((xs[i]-xs[present]) + (ys[i]-ys[present])*(ys[i]-ys[present]))<nextmin) {
                nextmin = ((xs[i]-xs[present]) + (ys[i]-ys[present])*(ys[i]-ys[present]));
                next = i;
                xxshift = xs[next] - xs[present];
                xyshift = ys[next] - ys[present];
                boundary = 1.1*xxshift;
            }
        }
        if (nextmin!=G_MAXDOUBLE) {
            nx++;
            //printf("next object to the left is %g %g, shift %g %g\n", xs[next], ys[next], xxshift, xyshift);
            present = next;
            avxshift += xxshift;
            avyshift += xyshift;
        } else break;

    } while (nextmin!=G_MAXDOUBLE);
    printf("Original: found %d objects in x direction, average shift is %g %g\n", nx+1, avxshift/nx, avyshift/nx);
   
    present = tl;
    //determine number of objects in x direction and xshift, 
    ny = 0;
    yyshift = 0;
    yxshift = 0;
    do {
        //find next closest object in y direction
        nextmin = G_MAXDOUBLE;
        for (i=0; i<noriginal; i++) {
            if (i==present || ys[i]<=ys[present]) continue; 
            if (ny>0 && fabs(xs[i]-xs[present])>(0.5*fabs(yyshift))) {
                //printf("too far in y (%d  %g > %g) \n", i, fabs(ys[i]-ys[present]), (0.5*fabs(yshift))); 
                continue;
            }
            if (((ys[i]-ys[present]) + (xs[i]-xs[present])*(xs[i]-xs[present]))<nextmin) {
                nextmin = ((ys[i]-ys[present]) + (xs[i]-xs[present])*(xs[i]-xs[present]));
                next = i;
                yxshift = xs[next] - xs[present];
                yyshift = ys[next] - ys[present];
            }
        }
        if (nextmin!=G_MAXDOUBLE) {
            ny++;
            //printf("next object to the bottom is %g %g, shift %g %g\n", xs[next], ys[next], yxshift, yyshift);
            present = next;
        } else break;

    } while (nextmin!=G_MAXDOUBLE);
    printf("Original: found %d objects in y direction\n", ny+1);

    //determine final number of objects, it must be odd and same in both the directions
    nn = 2*((int)(MIN(nx+1, ny+1)/2))+1;
    if (nn>(nx+1) || nn>(ny+1)) nn-=2;

    printf("I will use matrix of %d x %d calibration points\n", nn, nn);
 
    //allocate matrices
    x_orig = gwy_data_field_new(nn, nn, nn, nn, TRUE);
    y_orig = gwy_data_field_new_alike(x_orig, TRUE);
    x_shifted = gwy_data_field_new_alike(x_orig, TRUE);
    y_shifted = gwy_data_field_new_alike(x_orig, TRUE);
    x_rotated = gwy_data_field_new_alike(x_orig, TRUE);
    y_rotated = gwy_data_field_new_alike(x_orig, TRUE);
    x_unrotated = gwy_data_field_new_alike(x_orig, TRUE);
    y_unrotated = gwy_data_field_new_alike(x_orig, TRUE);


    //fill matrix of original
    fill_matrix(xs, ys, noriginal, tl, xxshift, xyshift, yxshift, yyshift, x_orig, y_orig, nn);

    //_________________________________________________shifted___________________________________________________________
    //find objects on shifted image
    nshifted = 100;
    get_object_list(shifted, detail, 0.8, xs, ys, &nshifted, GWY_CORRELATION_NORMAL);
    printf("%d object locations in shifted\n", nshifted);
/*    for (i=0; i<nshifted; i++)
      {
          printf("%d %g %g\n", i, xs[i], ys[i]);
      }
      printf("_____________________\n");
      */

    //find top left object again, note that it should be shifted by one period to the right
    tl = 0;
    tlmin = G_MAXDOUBLE;
    for (i=0; i<noriginal; i++) {
        if (((original_tlx-xs[i]+xxshift)*(original_tlx-xs[i]+xxshift) + (original_tly-ys[i]+xyshift)*(original_tly-ys[i]+xyshift))<tlmin) {
            tlmin = ((original_tlx-xs[i]+xxshift)*(original_tlx-xs[i]+xxshift) + (original_tly-ys[i]+xyshift)*(original_tly-ys[i]+xyshift));
            tl = i;
        }
    }
//    printf("top left object is %g %g\n", xs[tl], ys[tl]);
    fill_matrix(xs, ys, nshifted, tl, xxshift, xyshift, yxshift, yyshift, x_shifted, y_shifted, nn);
 

    //______________________________________________________________rotated____________________________________________
    //rotate rotated back
    nrotated = 100;
    rback = gwy_data_field_new(gwy_data_field_get_yres(rotated), gwy_data_field_get_xres(rotated), 10, 10, 0);
    flip_xy(rotated, rback, FALSE);
    get_object_list(rback, detail, 0.8, xs, ys, &nrotated, GWY_CORRELATION_NORMAL);
    printf("%d object locations in rotated\n", nrotated);
    /*for (i=0; i<nrotated; i++)
    {
        printf("%d %g %g\n", i, xs[i], ys[i]);
    }
    printf("_____________________\n");*/
    
    //create matrix of NxN objects on rotated image, assign data as if the center of original and rotated match
    //find top left object again
    tl = 0;
    tlmin = G_MAXDOUBLE;
    for (i=0; i<nrotated; i++) {
        if ((xs[i]*ys[i])<tlmin) {
            tlmin = (xs[i]*ys[i]);
            tl = i;
        }
    }
//    printf("top left object is %g %g\n", xs[tl], ys[tl]);
    fill_matrix(xs, ys, nrotated, tl, xxshift, xyshift, yxshift, yyshift, x_unrotated, y_unrotated, nn);

    xres = yres = nn;
    x = y = 0;

    for (i = 0; i < xres; i++) {
        for (j = 0; j < yres; j++) {
            gwy_data_field_set_val(x_rotated, i, j, gwy_data_field_get_val(y_unrotated, i, j));
            gwy_data_field_set_val(y_rotated, i, j, gwy_data_field_get_xres(rotated) - gwy_data_field_get_val(x_unrotated, i, j));
 //           printf("Point %d %d pos %g %g\n", i, j, x, y);
        }
    }

    //make real coordinates from pixel ones
    xmult = gwy_data_field_get_xreal(original)/gwy_data_field_get_xres(original);
    ymult = gwy_data_field_get_yreal(original)/gwy_data_field_get_yres(original);
    gwy_data_field_multiply(x_orig, xmult);
    gwy_data_field_multiply(x_rotated, xmult);
    gwy_data_field_multiply(x_shifted, xmult);
    gwy_data_field_multiply(y_orig, ymult);
    gwy_data_field_multiply(y_rotated, ymult);
    gwy_data_field_multiply(y_shifted, ymult);

    //move center of each matrix by half size in both direction, so the axis intersection is in the center of image

    gwy_data_field_add(x_orig, -gwy_data_field_get_xreal(original)/2);
    gwy_data_field_add(y_orig, -gwy_data_field_get_yreal(original)/2);
    gwy_data_field_add(x_shifted, -gwy_data_field_get_xreal(original)/2);
    gwy_data_field_add(y_shifted, -gwy_data_field_get_yreal(original)/2);
    gwy_data_field_add(x_rotated, -gwy_data_field_get_xreal(original)/2);
    gwy_data_field_add(y_rotated, -gwy_data_field_get_yreal(original)/2);



    //now we have matrices for all three data and we can throw all data away.
    //output all of the for debug:
    printf("Original matrix:\n");
    for (j = 0; j < yres; j++) {
        for (i = 0; i < xres; i++) {
            printf("(%g,%g)  ", gwy_data_field_get_val(x_orig, i, j), gwy_data_field_get_val(y_orig, i, j));
        }
        printf("\n");
    } 
    printf("Shifted matrix:\n");
    for (j = 0; j < yres; j++) {
        for (i = 0; i < xres; i++) {
            printf("(%g,%g)  ", gwy_data_field_get_val(x_shifted, i, j), gwy_data_field_get_val(y_shifted, i, j));
        }
        printf("\n");
    } 
    //printf("Unotated matrix:\n");
    //for (j = 0; j < yres; j++) {
    //    for (i = 0; i < xres; i++) {
    //        printf("(%g,%g)  ", gwy_data_field_get_val(x_unrotated, i, j), gwy_data_field_get_val(y_unrotated, i, j));
    //    }
    //    printf("\n");
    //} 
    printf("Rotated matrix:\n");
    for (j = 0; j < yres; j++) {
        for (i = 0; i < xres; i++) {
            printf("(%g,%g)  ", gwy_data_field_get_val(x_rotated, i, j), gwy_data_field_get_val(y_rotated, i, j));
        }
        printf("\n");
    } 

     /*  - create v0x, v0y field from original image matrix*/
    period = sqrt(xxshift*xxshift + xyshift*xyshift); //FIXME this must be known experimentally
    stage_calibration(x_orig, y_orig, x_shifted, y_shifted, x_rotated, y_rotated, period); 

    /*
    newid = gwy_app_data_browser_add_data_field(score, data, TRUE);
    g_object_unref(score);
    gwy_app_set_data_field_title(data, newid, _("Score"));
    gwy_app_sync_data_items(data, data, args->objects[0].id, newid, FALSE,
                                               GWY_DATA_ITEM_GRADIENT, 0);
    */


}

//note that this is taken from basicops.c
static void
flip_xy(GwyDataField *source, GwyDataField *dest, gboolean minor)
{
    gint xres, yres, i, j;
    gdouble *dd;
    const gdouble *sd;

    xres = gwy_data_field_get_xres(source);
    yres = gwy_data_field_get_yres(source);
    gwy_data_field_resample(dest, yres, xres, GWY_INTERPOLATION_NONE);
    sd = gwy_data_field_get_data_const(source);
    dd = gwy_data_field_get_data(dest);
    if (minor) {
        for (i = 0; i < xres; i++) {
            for (j = 0; j < yres; j++) {
                dd[i*yres + j] = sd[j*xres + (xres - 1 - i)];
            }
        }
    }
    else {
        for (i = 0; i < xres; i++) {
            for (j = 0; j < yres; j++) {
                dd[i*yres + (yres - 1 - j)] = sd[j*xres + i];
            }
        }
    }
    gwy_data_field_set_xreal(dest, gwy_data_field_get_yreal(source));
    gwy_data_field_set_yreal(dest, gwy_data_field_get_xreal(source));
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

