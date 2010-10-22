/*
 *  @(#) $Id: grain_deposit.c 8476 2007-08-25 13:51:01Z yeti-dn $
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 michaelablahutovaUSA
 */



#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/grains.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#include <math.h>
#include <stdlib.h>
#include <time.h>

#define DEPOSIT_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400
};

enum {
    RESPONSE_RESET   = 1,
    RESPONSE_PREVIEW = 2
};

typedef struct {
    gdouble size;
//    gdouble width;
    gdouble coverage;
    gdouble revise;
    /* interface only */
    gboolean computed;
} DepositArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *view;
    GtkObject *size;
    GtkWidget *value_size;
    GwySIValueFormat *format_size;
//    GtkObject *width;
//    GtkWidget *value_width;
//    GwySIValueFormat *format_width;
    GtkObject *coverage;
    GtkObject *revise;
    GwyContainer *mydata;
    GwyDataField *old_dfield;
    DepositArgs *args;

    gboolean in_init;
} DepositControls;

static gboolean    module_register            (void);
static void        deposit                 (GwyContainer *data,
                                               GwyRunType run);
static void        deposit_dialog                (DepositArgs *args,
                                               GwyContainer *data,
                                               GwyDataField *dfield,
                                               gint id);
static void        deposit_dialog_update_controls(DepositControls *controls,
                                               DepositArgs *args);
static void        deposit_dialog_update_values  (DepositControls *controls,
                                               DepositArgs *args);
static void        update_threshold_size     (DepositControls *controls);
//static void        update_threshold_width     (DepositControls *controls);
static void        deposit_invalidate            (DepositControls *controls);
static void        deposit_invalidate2           (gpointer whatever,
                                               DepositControls *controls);
static void        preview                    (DepositControls *controls,
                                               DepositArgs *args);
static void        mask_process               (GwyDataField *dfield,
                                               GwyDataField *maskfield,
                                               DepositArgs *args);
static void        deposit_load_args             (GwyContainer *container,
                                               DepositArgs *args);
static void        deposit_save_args             (GwyContainer *container,
                                               DepositArgs *args);
static void        deposit_sanitize_args         (DepositArgs *args);


static const DepositArgs deposit_defaults = {
    0,
    0,
    0,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Simulate spherical particle deposition."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("deposit",
                              (GwyProcessFunc)&deposit,
                              N_("/_Synthetic/_Deposit particles..."),
                              GWY_STOCK_GRAINS,
                              DEPOSIT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Deposit spherical particles"));

    return TRUE;
}

static void
deposit(GwyContainer *data, GwyRunType run)
{
    DepositArgs args;
    GwyDataField *dfield;
    gint id;

    g_return_if_fail(run & DEPOSIT_RUN_MODES);
    deposit_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    deposit_dialog(&args, data, dfield, id);
    deposit_save_args(gwy_app_settings_get(), &args);
}

static void
table_attach_threshold(GtkWidget *table, gint *row, const gchar *name,
                       GtkObject **adj, gdouble value,
                       gpointer data)
{
    GtkWidget *spin;

    *adj = gtk_adjustment_new(value, 0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_hscale(table, *row, name, "%", *adj,
                                   GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(deposit_invalidate), data);
    (*row)++;
}



static void
deposit_dialog(DepositArgs *args,
            GwyContainer *data,
            GwyDataField *dfield,
            gint id)
{
    GtkWidget *dialog, *table, *hbox, *label, *pivot;
    DepositControls controls;
    gint response;
    GwyPixmapLayer *layer;
    gint row, newid;
    GwyDataField *rfield;

    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Deposit spherical particles"), NULL, 0,
                                         NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Start"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_duplicate_by_prefix(data,
                                                        "/0/data/",
                                                        "/0/base/palette",
                                                        "/0/base/range-type",
                                                        "/0/base/", NULL);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", gwy_data_field_duplicate(dfield));
    controls.old_dfield = gwy_data_field_duplicate(dfield);

    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(10, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Deposition parameters:")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    table_attach_threshold(table, &row, _("_Size"),
                           &controls.size, args->size,
                           &controls);
    pivot = gwy_table_hscale_get_scale(controls.size);

    controls.value_size = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.value_size), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.value_size,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.size, "value-changed",
                             G_CALLBACK(update_threshold_size), &controls);
    gwy_widget_sync_sensitivity(pivot, controls.value_size);

    controls.format_size
        = gwy_data_field_get_value_format_z(dfield, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            NULL);
    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label), controls.format_size->units);
    gtk_table_attach(GTK_TABLE(table), label,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gwy_widget_sync_sensitivity(pivot, label);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /*
    table_attach_threshold(table, &row, _("_Distribution width"),
                           &controls.width, args->width,
                           &controls);
    pivot = gwy_table_hscale_get_scale(controls.width);

    controls.value_width = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.value_width), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.value_width,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.width, "value-changed",
                             G_CALLBACK(update_threshold_width), &controls);
    gwy_widget_sync_sensitivity(pivot, controls.value_width);

    controls.format_width
        = gwy_data_field_get_value_format_z(dfield, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            NULL);
    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label), controls.format_width->units);
    gtk_table_attach(GTK_TABLE(table), label,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gwy_widget_sync_sensitivity(pivot, label);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;
    */


    table_attach_threshold(table, &row, _("_Coverage:"),
                           &controls.coverage, args->coverage,
                           &controls);

    table_attach_threshold(table, &row, _("_Revise after settle"),
                           &controls.revise, args->revise,
                           &controls);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);


    deposit_invalidate(&controls);
    update_threshold_size(&controls);
    //update_threshold_width(&controls);

    /* finished initializing, allow instant updates */
    controls.in_init = FALSE;

    /* show initial preview if instant updates are on */
    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            deposit_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            gwy_si_unit_value_format_free(controls.format_size);
            //gwy_si_unit_value_format_free(controls.format_width);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = deposit_defaults;
            deposit_dialog_update_controls(&controls, args);
            controls.in_init = TRUE;
            preview(&controls, args);
            controls.in_init = FALSE;
            break;

            case RESPONSE_PREVIEW:
            deposit_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    deposit_dialog_update_values(&controls, args);
    gwy_app_sync_data_items(controls.mydata, data, 0, id, FALSE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    gtk_widget_destroy(dialog);
    gwy_si_unit_value_format_free(controls.format_size);
    //gwy_si_unit_value_format_free(controls.format_width);

    if (args->computed) {
        rfield = gwy_container_get_object_by_name(controls.mydata, "/0/data");

        newid = gwy_app_data_browser_add_data_field(rfield,
                                                    data, TRUE);
        gwy_app_sync_data_items(data, data,
          id, newid, FALSE,
          GWY_DATA_ITEM_GRADIENT,
          GWY_DATA_ITEM_MASK_COLOR,
          GWY_DATA_ITEM_REAL_SQUARE,
          GWY_DATA_ITEM_SELECTIONS,
          0);
        gwy_app_set_data_field_title(data, newid, _("Particles"));

        g_object_unref(controls.mydata);
    }
}

static void
deposit_dialog_update_controls(DepositControls *controls,
                            DepositArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size),
                             args->size);
//    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->width),
//                             args->width);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->coverage),
                             args->coverage);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->revise),
                             args->revise);
}

static void
deposit_dialog_update_values(DepositControls *controls,
                          DepositArgs *args)
{
    args->size
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->size));
//    args->width
//        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->width));
    args->coverage
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->coverage));
    args->revise
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->revise));


}

static void
deposit_invalidate(DepositControls *controls)
{
    controls->args->computed = FALSE;

}

static void
update_threshold_size(DepositControls *controls)
{
    gdouble v;
    gchar *s;
    gdouble min = 0;
    gdouble max = 500e-9; //FIXME

    v = GTK_ADJUSTMENT(controls->size)->value/100.0;
    v = (1.0 - v)*min + v*max;
    s = g_strdup_printf("%.*f",
                        controls->format_size->precision,
                        v/controls->format_size->magnitude);
    gtk_label_set_markup(GTK_LABEL(controls->value_size), s);
    g_free(s);
}

/*
static void
update_threshold_width(DepositControls *controls)
{
    gdouble v;
    gchar *s;
    gdouble min = 0;
    gdouble max = 500e-9; //FIXME

    v = GTK_ADJUSTMENT(controls->width)->value/100.0;
    v = (1.0 - v)*min + v*max;
    s = g_strdup_printf("%.*f",
                        controls->format_width->precision,
                        v/controls->format_width->magnitude);
    gtk_label_set_markup(GTK_LABEL(controls->value_width), s);
    g_free(s);
}
*/




static void
deposit_invalidate2(G_GNUC_UNUSED gpointer whatever, DepositControls *controls)
{
    deposit_invalidate(controls);
}


/*integrate over some volume around particle (ax, ay, az), if there is substrate, add this to potential*/
gdouble
integrate_lj_substrate(GwyDataField *lfield, gdouble ax, gdouble ay, gdouble az, gdouble size)
{
    /*make l-j only from idealistic substrate now*/
    gdouble zval, sigma, dist;
    sigma = 1.2*size; //empiric

    zval = gwy_data_field_get_val(lfield, CLAMP(gwy_data_field_rtoi(lfield, ax), 0, gwy_data_field_get_xres(lfield)-1), 
                                          CLAMP(gwy_data_field_rtoi(lfield, ay), 0, gwy_data_field_get_yres(lfield)-1));

    dist = sqrt((az-zval)*(az-zval));
    //printf("%g %g \n", dist, (pow(sigma, 12)/45.0/pow(dist, 9) - pow(sigma, 6)/6.0/pow(dist, 3)));
    return 1e18*(pow(sigma, 12)/45.0/pow(dist, 9) - pow(sigma, 6)/6.0/pow(dist, 3));
}


/*lj potential between two particles*/
gdouble
get_lj_potential_spheres(gdouble ax, gdouble ay, gdouble az, gdouble bx, gdouble by, gdouble bz, gdouble size)
{
    gdouble sigma = 1.7*size;
    gdouble dist = ((ax-bx)*(ax-bx)
        + (ay-by)*(ay-by)
        + (az-bz)*(az-bz));

   // printf("%g %g %g %g -> %g\n", dist, pow(sigma, 12), pow(dist, 6), pow(dist, 3), pow(sigma, 12)/pow(dist, 6) - pow(sigma, 6)/pow(dist, 3));
    return 1e-5*(pow(sigma, 12)/pow(dist, 6) - pow(sigma, 6)/pow(dist, 3)); //0.1
}

/*synchronize reals to integers, create datafield with all the particles in their actual position*/
static void
showit(GwyDataField *lfield, GwyDataField *dfield, gdouble *rdisizes, gdouble *rx, gdouble *ry, gdouble *rz, gint *xdata, gint *ydata, gint ndata, 
       gint oxres, gdouble oxreal, gint oyres, gdouble oyreal, gint add, gint xres, gint yres)
{
    gint i, m, n;
    gdouble sum, surface, lsurface, csurface;
    gint disize;
    gdouble rdisize;


    //FIXME, add z also
    //
    for (i=0; i<ndata; i++)
    {
        xdata[i] = oxres*(rx[i]/oxreal);
        ydata[i] = oyres*(ry[i]/oyreal);

        if (xdata[i]<0 || ydata[i]<0 || xdata[i]>=xres || ydata[i]>=yres) continue;
        if (rz[i]>(gwy_data_field_get_val(lfield, xdata[i], ydata[i])+6*rdisizes[i])) continue;

        csurface = gwy_data_field_get_val(lfield, xdata[i], ydata[i]);
        disize = (gint)((gdouble)oxres*rdisizes[i]/oxreal);
        rdisize = rdisizes[i];

        for (m=(xdata[i]-disize); m<(xdata[i]+disize); m++) 
        
        {
                for (n=(ydata[i]-disize); n<(ydata[i]+disize); n++)
                {
                    if (m<0 || n<0 || m>=xres || n>=yres) continue;

                    if (m>=add && n>=add && m<(xres-add) && n<(yres-add)) {
                        surface = gwy_data_field_get_val(dfield, m-add, n-add);
                        lsurface = gwy_data_field_get_val(lfield, m, n);
                        

                        if ((sum=(disize*disize - (xdata[i]-m)*(xdata[i]-m) - (ydata[i]-n)*(ydata[i]-n)))>0)
                        {
                            //surface = MAX(lsurface, csurface + (sqrt(sum) + disize)*oxreal/(double)oxres);
                            surface = MAX(lsurface, rz[i] + sqrt(sum)*oxreal/(double)oxres);
                            gwy_data_field_set_val(lfield, m, n, surface);           
                        }
                    } 

                }   
            } 
    }
}
 

static void
preview(DepositControls *controls,
        DepositArgs *args)
{
    GwyDataField *dfield, *lfield, *zlfield, *zdfield;
    gint xres, yres, oxres, oyres;
    gint add, i, ii, m, k;
    gdouble size, width;
    gint xdata[10000];
    gint ydata[10000];
    gdouble disizes[10000];
    gdouble rdisizes[10000];
    gdouble rx[10000];
    gdouble ry[10000];
    gdouble rz[10000];
    gdouble ax[10000];
    gdouble ay[10000];
    gdouble az[10000];
    gdouble vx[10000];
    gdouble vy[10000];
    gdouble vz[10000];
    gdouble fx[10000];
    gdouble fy[10000];
    gdouble fz[10000];
     gint xpos, ypos, ndata, too_close;
    gdouble disize, mdisize;
    gdouble xreal, yreal, oxreal, oyreal;
    gdouble diff;
    gdouble mass = 1;
    gint presetval;
    gint nloc, maxloc = 1;
    gint max = 5000000;
    gdouble rxv, ryv, rzv, timestep = 3e-7; //5e-7

    deposit_dialog_update_values(controls, args);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    gwy_container_set_object_by_name(controls->mydata, "/0/data", gwy_data_field_duplicate(controls->old_dfield));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                                      "/0/data"));

    if (controls->in_init)
    {
        gwy_data_field_data_changed(dfield); 
        while (gtk_events_pending())
            gtk_main_iteration();
        return;
    }


    oxres = gwy_data_field_get_xres(dfield);
    oyres = gwy_data_field_get_yres(dfield);
    oxreal = gwy_data_field_get_xreal(dfield);
    oyreal = gwy_data_field_get_yreal(dfield);
    diff = oxreal/oxres/10;


    size = args->size*5e-9;
   // width = args->width*5e-9 + 2*size; //increased manually to fill boundaries
    width = 2*size;
    add = gwy_data_field_rtoi(dfield, size + width);
    mdisize = gwy_data_field_rtoi(dfield, size);
    xres = oxres + 2*add;
    yres = oyres + 2*add;
    xreal = oxreal + 2*(size+width);
    yreal = oyreal + 2*(size+width);


//    printf("For field of size %g and particle nominak %g, real %g (%g), the final size will change from %d to %d\n",
//           gwy_data_field_get_xreal(dfield), args->size, size, disize, oxres, xres);

    /*make copy of datafield, with mirrored boundaries*/
    lfield = gwy_data_field_new(xres, yres, 
                                gwy_data_field_itor(dfield, xres), 
                                gwy_data_field_jtor(dfield, yres),
                                TRUE);
    gwy_data_field_area_copy(dfield, lfield, 0, 0, oxres, oyres, add, add);

    gwy_data_field_invert(dfield, 1, 0, 0);
    gwy_data_field_area_copy(dfield, lfield, 0, oyres-add-1, oxres, add, add, 0);
    gwy_data_field_area_copy(dfield, lfield, 0, 0, oxres, add, add, yres-add-1);
    gwy_data_field_invert(dfield, 1, 0, 0);

    gwy_data_field_invert(dfield, 0, 1, 0);
    gwy_data_field_area_copy(dfield, lfield, oxres-add-1, 0, add, oyres, 0, add);
    gwy_data_field_area_copy(dfield, lfield, 0, 0, add, oyres, xres-add-1, add);
    gwy_data_field_invert(dfield, 0, 1, 0);

    gwy_data_field_invert(dfield, 1, 1, 0);
    gwy_data_field_area_copy(dfield, lfield, oxres-add-1, oyres-add-1, add, add, 0, 0);
    gwy_data_field_area_copy(dfield, lfield, 0, 0, add, add, xres-add-1, yres-add-1);
    gwy_data_field_area_copy(dfield, lfield, oxres-add-1, 0, add, add, 0, yres-add-1);
    gwy_data_field_area_copy(dfield, lfield, 0, oyres-add-1, add, add, xres-add-1, 0);
    gwy_data_field_invert(dfield, 1, 1, 0);

    zlfield = gwy_data_field_duplicate(lfield);
    zdfield = gwy_data_field_duplicate(dfield);
    /*determine number of spheres necessary for given coverage*/

    for (i=0; i<10000; i++) 
    {
        ax[i] = ay[i] = az[i] = vx[i] = vy[i] = vz[i] = 0;
    }
    
    srand ( time(NULL) );

    ndata = 0;
 
    /* for test only */
    /*
    disize = mdisize;
    
    xpos = oxres/2 - 2*disize;
    ypos = oyres/2;
    xdata[ndata] = xpos;
    ydata[ndata] = ypos;
    disizes[ndata] = disize;
    rdisizes[ndata] = size;
    rx[ndata] = (gdouble)xpos*oxreal/(gdouble)oxres;
    ry[ndata] = (gdouble)ypos*oyreal/(gdouble)oyres;
    rz[ndata] = 2.0*gwy_data_field_get_val(lfield, xpos, ypos) + rdisizes[ndata];
    ndata++;

    xpos = oxres/2 + 2*disize;
    ypos = oyres/2;
    xdata[ndata] = xpos;
    ydata[ndata] = ypos;
    disizes[ndata] = disize;
    rdisizes[ndata] = size;
    rx[ndata] = (gdouble)xpos*oxreal/(gdouble)oxres;
    ry[ndata] = (gdouble)ypos*oyreal/(gdouble)oyres;
    rz[ndata] = 2.0*gwy_data_field_get_val(lfield, xpos, ypos) + rdisizes[ndata];
    ndata++;
    */
    /*end of test*/

    i = 0;
    presetval = args->coverage*10;
    while (ndata < presetval && i<max)
    {
        //disize = mdisize*(0.8+(double)(rand()%20)/40.0);   
        disize = mdisize;

        xpos = disize+(rand()%(xres-2*(gint)(disize+1))) + 1;
        ypos = disize+(rand()%(yres-2*(gint)(disize+1))) + 1;
        i++;
        

        {
            too_close = 0;

            /*sync real to integer positions*/
            for (k=0; k<ndata; k++)
            {
                if (((xpos-xdata[k])*(xpos-xdata[k]) + (ypos-ydata[k])*(ypos-ydata[k]))<(4*disize*disize))
                {
                    too_close = 1;
                    break;
                }
            }
            if (too_close) continue;
            if (ndata>=10000) {
                break;
            }

            xdata[ndata] = xpos;
            ydata[ndata] = ypos;
            disizes[ndata] = disize;
            rdisizes[ndata] = size;
            rx[ndata] = (gdouble)xpos*oxreal/(gdouble)oxres;
            ry[ndata] = (gdouble)ypos*oyreal/(gdouble)oyres;
            //printf("surface at %g, particle size %g\n", gwy_data_field_get_val(lfield, xpos, ypos), rdisizes[ndata]);
            rz[ndata] = 1.0*gwy_data_field_get_val(lfield, xpos, ypos) + rdisizes[ndata]; //2
            ndata++;
        }
    };
//    if (i==max) printf("Maximum reached, only %d particles depositd instead of %d\n", ndata, presetval);
//    else printf("%d particles depositd\n", ndata);

    /*refresh shown data and integer positions (necessary in md calculation)*/
    gwy_data_field_copy(zlfield, lfield, 0);
    showit(lfield, zdfield, rdisizes, rx, ry, rz, xdata, ydata, ndata, 
                  oxres, oxreal, oyres, oyreal, add, xres, yres);


    gwy_data_field_area_copy(lfield, dfield, add, add, oxres, oyres, 0, 0);
    gwy_data_field_data_changed(dfield);


    for (i=0; i<(20*args->revise); i++)
    {
//        printf("###### step %d of %d ##########\n", i, (gint)(20*args->revise));

        /*try to add some particles if necessary, do this only for first half of molecular dynamics*/
        if (ndata<presetval && i<(10*args->revise)) {
            ii = 0;
            nloc = 0;
            
            while (ndata < presetval && ii<(max/1000) && nloc<maxloc)
            {
                disize = mdisize;
                xpos = disize+(rand()%(xres-2*(gint)(disize+1))) + 1;
                ypos = disize+(rand()%(yres-2*(gint)(disize+1))) + 1;
                ii++;


                {
                    too_close = 0;

                    rxv = ((gdouble)xpos*oxreal/(gdouble)oxres);
                    ryv = ((gdouble)ypos*oyreal/(gdouble)oyres);
                    rzv = gwy_data_field_get_val(zlfield, xpos, ypos) + 5*size;
                    
                    for (k=0; k<ndata; k++)
                    {
                        if (((rxv-rx[k])*(rxv-rx[k]) 
                             + (ryv-ry[k])*(ryv-ry[k])
                             + (rzv-rz[k])*(rzv-rz[k]))<(4.0*size*size))
                        {
                            too_close = 1;
                            break;
                        }
                    }
                    if (too_close) continue;
                    if (ndata>=10000) {
//                        printf("Maximum reached!\n");
                        break;
                    }

                    xdata[ndata] = xpos;
                    ydata[ndata] = ypos;
                    disizes[ndata] = disize;
                    rdisizes[ndata] = size;
                    rx[ndata] = rxv;
                    ry[ndata] = ryv;
                    rz[ndata] = rzv;
                    vz[ndata] = -0.01;
                    ndata++;
                    nloc++;

                }
            };
//            if (ii==(max/100)) printf("Maximum reached, only %d particles now present instead of %d\n", ndata, presetval);
//            else printf("%d particles now at surface\n", ndata);
            
        }

        /*test succesive LJ steps on substrate (no relaxation)*/
        for (k=0; k<ndata; k++)
        {
            fx[k] = fy[k] = fz[k] = 0;
            /*calculate forces for all particles on substrate*/

            if (gwy_data_field_rtoi(lfield, rx[k])<0 
                || gwy_data_field_rtoj(lfield, ry[k])<0 
                || gwy_data_field_rtoi(lfield, rx[k])>=xres
                || gwy_data_field_rtoj(lfield, ry[k])>=yres)
                continue;

            for (m=0; m<ndata; m++)
            {
               
                if (m==k) continue;

            //    printf("(%g %g %g) on (%g %g %g)\n", rx[m], ry[m], rz[m], rx[k], ry[k], rz[k]);
                fx[k] -= (get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k]+diff, ry[k], rz[k], gwy_data_field_itor(dfield, disizes[k]))
                              -get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k]-diff, ry[k], rz[k], gwy_data_field_itor(dfield, disizes[k])))/2/diff; 
                fy[k] -= (get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k], ry[k]+diff, rz[k], gwy_data_field_itor(dfield, disizes[k]))
                              -get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k], ry[k]-diff, rz[k], gwy_data_field_itor(dfield, disizes[k])))/2/diff; 
                fz[k] -= (get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k], ry[k], rz[k]+diff, gwy_data_field_itor(dfield, disizes[k]))
                              -get_lj_potential_spheres(rx[m], ry[m], rz[m], rx[k], ry[k], rz[k]-diff, gwy_data_field_itor(dfield, disizes[k])))/2/diff; 

            }
                
            fx[k] -= (integrate_lj_substrate(zlfield, rx[k]+diff, ry[k], rz[k], rdisizes[k]) 
                    - integrate_lj_substrate(zlfield, rx[k]-diff, ry[k], rz[k], rdisizes[k]))/2/diff;
            fy[k] -= (integrate_lj_substrate(zlfield, rx[k], ry[k]-diff, rz[k], rdisizes[k]) 
                    - integrate_lj_substrate(zlfield, rx[k], ry[k]+diff, rz[k], rdisizes[k]))/2/diff;
            fz[k] -= (integrate_lj_substrate(zlfield, rx[k], ry[k], rz[k]+diff, rdisizes[k]) 
                    - integrate_lj_substrate(zlfield, rx[k], ry[k], rz[k]-diff, rdisizes[k]))/2/diff;
        } 
        for (k=0; k<ndata; k++)
        {
          if (gwy_data_field_rtoi(lfield, rx[k])<0 
                || gwy_data_field_rtoj(lfield, ry[k])<0 
                || gwy_data_field_rtoi(lfield, rx[k])>=xres
                || gwy_data_field_rtoj(lfield, ry[k])>=yres)
                continue;

            /*move all particles*/
            rx[k] += vx[k]*timestep + 0.5*ax[k]*timestep*timestep;
            vx[k] += 0.5*ax[k]*timestep;
            ax[k] = fx[k]/mass;
            vx[k] += 0.5*ax[k]*timestep;
            vx[k] *= 0.9;
            if (fabs(vx[k])>0.01) vx[k] = 0; //0.2

            ry[k] += vy[k]*timestep + 0.5*ay[k]*timestep*timestep;
            vy[k] += 0.5*ay[k]*timestep;
            ay[k] = fy[k]/mass;
            vy[k] += 0.5*ay[k]*timestep;
            vy[k] *= 0.9;
            if (fabs(vy[k])>0.01) vy[k] = 0; //0.2

            rz[k] += vz[k]*timestep + 0.5*az[k]*timestep*timestep;
            vz[k] += 0.5*az[k]*timestep;
            az[k] = fz[k]/mass;
            vz[k] += 0.5*az[k]*timestep;
            vz[k] *= 0.9;
            if (fabs(vz[k])>0.01) vz[k] = 0;

            if (rx[k]<=gwy_data_field_itor(dfield, disizes[k])) rx[k] = gwy_data_field_itor(dfield, disizes[k]);
            if (ry[k]<=gwy_data_field_itor(dfield, disizes[k])) ry[k] = gwy_data_field_itor(dfield, disizes[k]);
            if (rx[k]>=(xreal-gwy_data_field_itor(dfield, disizes[k]))) rx[k] = xreal-gwy_data_field_itor(dfield, disizes[k]);
            if (ry[k]>=(yreal-gwy_data_field_itor(dfield, disizes[k]))) ry[k] = yreal-gwy_data_field_itor(dfield, disizes[k]);

        }
        

        gwy_data_field_copy(zlfield, lfield, 0);
        showit(lfield, zdfield, rdisizes, rx, ry, rz, xdata, ydata, ndata, 
               oxres, oxreal, oyres, oyreal, add, xres, yres);
        


        gwy_data_field_area_copy(lfield, dfield, add, add, oxres, oyres, 0, 0);
        gwy_data_field_data_changed(dfield); 
        while (gtk_events_pending())
                    gtk_main_iteration();

       
    }


    gwy_data_field_area_copy(lfield, dfield, add, add, oxres, oyres, 0, 0);

    gwy_data_field_data_changed(dfield);
    args->computed = TRUE;
    gwy_object_unref(lfield);
    gwy_object_unref(zlfield);
    gwy_object_unref(zdfield);

}

static const gchar size_key[]  = "/module/grain_deposit/size";
static const gchar width_key[]  = "/module/grain_deposit/width";
static const gchar coverage_key[]   = "/module/grain_deposit/coverage";
static const gchar revise_key[]    = "/module/grain_deposit/revise";

static void
deposit_sanitize_args(DepositArgs *args)
{
    args->size = CLAMP(args->size, 0.0, 100.0);
//    args->width = CLAMP(args->width, 0.0, 100.0);
    args->coverage = CLAMP(args->coverage, 0.0, 100.0);
    args->revise = CLAMP(args->revise, 0.0, 100.0);
}

static void
deposit_load_args(GwyContainer *container,
               DepositArgs *args)
{
    *args = deposit_defaults;

    gwy_container_gis_double_by_name(container, size_key, &args->size);
//    gwy_container_gis_double_by_name(container, width_key, &args->width);
    gwy_container_gis_double_by_name(container, coverage_key, &args->coverage);
    gwy_container_gis_double_by_name(container, revise_key, &args->revise);
    deposit_sanitize_args(args);
}

static void
deposit_save_args(GwyContainer *container,
               DepositArgs *args)
{
    gwy_container_set_double_by_name(container, size_key, args->size);
//    gwy_container_set_double_by_name(container, width_key, args->width);
    gwy_container_set_double_by_name(container, coverage_key, args->coverage);
    gwy_container_set_double_by_name(container, revise_key, args->revise);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
