/*
 *  @(#) $Id$
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

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/correlation.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#include <stdio.h>

#define MERGE_RUN_MODES \
    (GWY_RUN_INTERACTIVE)

typedef enum {
    GWY_MERGE_DIRECTION_UP,
    GWY_MERGE_DIRECTION_DOWN,
    GWY_MERGE_DIRECTION_RIGHT,
    GWY_MERGE_DIRECTION_LEFT,
    GWY_MERGE_DIRECTION_LAST
} GwyMergeDirectionType;

typedef enum {
    GWY_MERGE_MODE_CORRELATE,
    GWY_MERGE_MODE_NONE,
    GWY_MERGE_MODE_LAST
} GwyMergeModeType;

typedef enum {
    GWY_MERGE_BOUNDARY_FIRST,
    GWY_MERGE_BOUNDARY_SECOND,
    GWY_MERGE_BOUNDARY_SMOOTH,
    GWY_MERGE_BOUNDARY_LAST
} GwyMergeBoundaryType;


typedef struct {
    GwyDataWindow *win1;
    GwyDataWindow *win2;
    GwyMergeDirectionType direction;
    GwyMergeModeType mode;
    GwyMergeBoundaryType boundary;
} MergeArgs;

typedef struct {
    GtkWidget *geometry;
    GtkWidget *mode;
    GtkWidget *boundary;
} MergeControls;

static gboolean   module_register               (const gchar *name);
static gboolean   merge                  (GwyContainer *data,
                                                 GwyRunType run);
static GtkWidget* merge_window_construct (MergeArgs *args);
static void       merge_data_cb          (GtkWidget *item);
static gboolean   merge_check            (MergeArgs *args,
                                                 GtkWidget *merge_window);
static gboolean   merge_do               (MergeArgs *args);
static GtkWidget* merge_data_option_menu (GwyDataWindow **operand);
static void       merge_direction_cb     (GtkWidget *combo,
                                          MergeArgs *args);
static void       merge_mode_cb          (GtkWidget *combo,
                                          MergeArgs *args);
static void       merge_boundary_cb      (GtkWidget *combo,
                                          MergeArgs *args);


static void       merge_load_args        (GwyContainer *settings,
                                          MergeArgs *args);
static void       merge_save_args        (GwyContainer *settings,
                                          MergeArgs *args);
static void       merge_sanitize_args    (MergeArgs *args);

static gboolean   get_score_iteratively(GwyDataField *data_field,
                                        GwyDataField *kernel_field,
                                        GwyDataField *score,
                                        MergeArgs *args);
static void       find_score_maximum   (GwyDataField *correlation_score, 
                                        gint *max_col, 
                                        gint *max_row);
static void       merge_boundary       (GwyDataField *dfield1, 
                                        GwyDataField *dfield2, 
                                        GwyDataField *result,
                                        GdkRectangle res_rect, 
                                        GdkPoint f1_pos, 
                                        GdkPoint f2_pos,
                                        GwyMergeDirectionType direction,
                                        gdouble zshift);


static const GwyEnum directions[] = {
    { N_("Up"),            GWY_MERGE_DIRECTION_UP },
    { N_("Down"),          GWY_MERGE_DIRECTION_DOWN },
    { N_("Right"),         GWY_MERGE_DIRECTION_RIGHT },
    { N_("Left"),          GWY_MERGE_DIRECTION_LEFT },
};

static const GwyEnum modes[] = {
    { N_("Correlation"),   GWY_MERGE_MODE_CORRELATE },
    { N_("None"),          GWY_MERGE_MODE_NONE },
};

static const GwyEnum boundaries[] = {
    { N_("First operand"),   GWY_MERGE_BOUNDARY_FIRST  },
    { N_("Second operand"),  GWY_MERGE_BOUNDARY_SECOND },
    { N_("Smooth"),          GWY_MERGE_BOUNDARY_SMOOTH },
};



static const MergeArgs merge_defaults = {
    NULL, NULL, GWY_MERGE_DIRECTION_RIGHT, GWY_MERGE_MODE_CORRELATE, GWY_MERGE_BOUNDARY_SMOOTH,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Dilates data with given tip."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo merge_func_info = {
        "merge",
        N_("/M_ultidata/_Merge..."),
        (GwyProcessFunc)&merge,
        MERGE_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &merge_func_info);

    return TRUE;
}

/* FIXME: we ignore the Container argument and use current data window */
static gboolean
merge(GwyContainer *data, GwyRunType run)
{
    GtkWidget *merge_window;
    GwyContainer *settings;
    MergeArgs args;
    gboolean ok = FALSE;

    g_return_val_if_fail(run & MERGE_RUN_MODES, FALSE);
    settings = gwy_app_settings_get();
    merge_load_args(settings, &args);

    args.win1 = args.win2 = gwy_app_data_window_get_current();
    g_assert(gwy_data_window_get_data(args.win1) == data);
    merge_window = merge_window_construct(&args);
    gtk_window_present(GTK_WINDOW(merge_window));

    do {
        switch (gtk_dialog_run(GTK_DIALOG(merge_window))) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(merge_window);
            ok = TRUE;
            break;

            case GTK_RESPONSE_OK:
            ok = merge_check(&args, merge_window);
            if (ok) {
                gtk_widget_destroy(merge_window);
                merge_do(&args);
                merge_save_args(settings, &args);
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    return FALSE;
}

static GtkWidget*
merge_window_construct(MergeArgs *args)
{
    GtkWidget *dialog, *table, *omenu, *label, *combo;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Merge data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(2, 10, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /***** First operand *****/
    label = gtk_label_new_with_mnemonic(_("_First operand:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = merge_data_option_menu(&args->win1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    row++;

    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("_Second operand:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = merge_data_option_menu(&args->win2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    row++;

    /*Parameters*/
    combo = gwy_enum_combo_box_new(directions, G_N_ELEMENTS(directions),
                                   G_CALLBACK(merge_direction_cb), args,
                                   args->direction, TRUE);
    gwy_table_attach_hscale(table, row, _("_Put second operand:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    combo = gwy_enum_combo_box_new(modes, G_N_ELEMENTS(modes),
                                   G_CALLBACK(merge_mode_cb), args,
                                   args->mode, TRUE);
    gwy_table_attach_hscale(table, row, _("_Align second operand:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    combo = gwy_enum_combo_box_new(boundaries, G_N_ELEMENTS(boundaries),
                                   G_CALLBACK(merge_boundary_cb), args,
                                   args->boundary, TRUE);
    gwy_table_attach_hscale(table, row, _("_Boundary treatment:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    
    gtk_widget_show_all(dialog);

    return dialog;
}

static GtkWidget*
merge_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(merge_data_cb),
                                        NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}


static void
merge_data_cb(GtkWidget *item)
{
    GtkWidget *menu;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);

    p = g_object_get_data(G_OBJECT(item), "data-window");
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}


static gboolean
merge_check(MergeArgs *args,
               GtkWidget *merge_window)
{
    GtkWidget *dialog;
    GwyContainer *data;
    GwyDataField *dfield1, *dfield2;
    GwyDataWindow *operand1, *operand2;

    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(operand1)
                         && GWY_IS_DATA_WINDOW(operand2),
                         FALSE);

    data = gwy_data_window_get_data(operand1);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    data = gwy_data_window_get_data(operand2);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    if (fabs((dfield1->xreal/dfield1->xres)
             /(dfield2->xreal/dfield2->xres) - 1) > 0.01
        || fabs((dfield1->yreal/dfield1->yres)
                /(dfield2->yreal/dfield2->yres) - 1) > 0.01) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(merge_window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK,
                                    _("Images have diferent resolution/range ratios. "
                                      "They cannot be merged in this module version."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;
    }
    return TRUE;
}

static gboolean
merge_do(MergeArgs *args)
{
    GtkWidget *data_window;
    GwyContainer *data;
    GwyDataField *dfield1, *dfield2;
    GwyDataField *correlation_data, *correlation_kernel, *correlation_score;
    GwyDataField *result;
    GwyDataWindow *operand1, *operand2;
    GdkRectangle cdata, kdata, res_rect;
    GdkPoint f1_pos, f2_pos;
    gint max_col, max_row;
    gint newxres, newyres;
    gdouble zshift;
    gint xshift, yshift;

    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(operand1 != NULL && operand2 != NULL, FALSE);

    data = gwy_data_window_get_data(operand1);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    data = gwy_data_window_get_data(operand2);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    /*result fields - after computation result should be at dfield */
    data = gwy_container_duplicate_by_prefix(data,
                                             "/0/data",
                                             "/0/base/palette",
                                             "/0/select",
                                             NULL);
    result = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    /*cut data for correlation*/
    switch (args->direction) {
        case GWY_MERGE_DIRECTION_UP:
        gwy_data_field_invert(dfield1, TRUE, FALSE, FALSE);
        gwy_data_field_invert(dfield2, TRUE, FALSE, FALSE);
        case GWY_MERGE_DIRECTION_DOWN:
        cdata.x = 0;
        cdata.y = dfield1->yres - (dfield1->yres/3);
        cdata.width = dfield1->xres;
        cdata.height = dfield1->yres/3;
        kdata.width = cdata.width/2;
        kdata.height = cdata.height/3;
        kdata.x = dfield2->xres/2 - kdata.width/2;
        kdata.y = 0;
        break;

        case GWY_MERGE_DIRECTION_LEFT: /*TODO rewrite this really ugly hack*/
        gwy_data_field_invert(dfield1, FALSE, TRUE, FALSE);
        gwy_data_field_invert(dfield2, FALSE, TRUE, FALSE);
        case GWY_MERGE_DIRECTION_RIGHT:
        cdata.x = dfield1->xres - (dfield1->xres/3);
        cdata.y = 0;
        cdata.width = dfield1->xres/3;
        cdata.height = dfield1->yres;
        kdata.width = cdata.width/3;
        kdata.height = cdata.height/2;
        kdata.x = 0;
        kdata.y = dfield2->yres/2 - kdata.height/2;
        break;

        default:
        g_assert_not_reached();
        break;
    }
    
    correlation_data = gwy_data_field_area_extract(dfield1, 
                                                   cdata.x,
                                                   cdata.y,
                                                   cdata.width,
                                                   cdata.height);
    correlation_kernel = gwy_data_field_area_extract(dfield2, 
                                                   kdata.x,
                                                   kdata.y,
                                                   kdata.width,
                                                   kdata.height);
    correlation_score = gwy_data_field_new_alike(correlation_data, FALSE);
    
    /*get appropriate correlation score*/
    get_score_iteratively(correlation_data, correlation_kernel, 
                          correlation_score, args);
    find_score_maximum(correlation_score, &max_col, &max_row);

    /*enlarge result field to fit the new data*/
    switch (args->direction) {
        case GWY_MERGE_DIRECTION_UP:
        case GWY_MERGE_DIRECTION_DOWN:
        newxres = fabs((max_col - kdata.width/2) - kdata.x) + MAX(dfield1->xres, dfield2->xres);
        newyres = cdata.y + (max_row - kdata.height/2) + dfield2->yres;

        gwy_data_field_resample(result, newxres, newyres, GWY_INTERPOLATION_NONE);
        gwy_data_field_set_xreal(result, gwy_data_field_get_xreal(result)*(gdouble)newxres/dfield2->xres);
        gwy_data_field_set_yreal(result, gwy_data_field_get_yreal(result)*(gdouble)newyres/dfield2->yres);
        
        gwy_data_field_fill(result, 
                            MIN(gwy_data_field_get_min(dfield1),
                                gwy_data_field_get_min(dfield2)));
        zshift = gwy_data_field_area_get_avg(correlation_data, 
                                            max_col - kdata.width/2,
                                            max_row - kdata.height/2,
                                            kdata.width,
                                            kdata.height)
            - gwy_data_field_get_avg(correlation_kernel);
        
        /*fill the result with both data fields*/
        xshift = MAX(0, -(max_col - cdata.width/2));
        if (args->boundary == GWY_MERGE_BOUNDARY_SMOOTH || args->boundary == GWY_MERGE_BOUNDARY_FIRST)
        {
            if (!gwy_data_field_area_copy(dfield1, result,
                             0, 0, dfield1->xres, dfield1->yres,
                             xshift, 0))
                g_warning("Error while putting field 1 at: %d %d\n", xshift, 0);
        
            if (!gwy_data_field_area_copy(dfield2, result,
                             0, 0, dfield2->xres, dfield2->yres,
                             xshift + ((max_col - kdata.width/2) - kdata.x),
                             cdata.y + (max_row - kdata.height/2)))
                g_warning("Error while putting field 2 at: %d %d\n", 
                    xshift + ((max_col - kdata.width/2) - kdata.x),
                    cdata.y + (max_row - kdata.height/2));

            gwy_data_field_area_add(result, 
                                xshift + ((max_col - kdata.width/2) - kdata.x),
                                cdata.y + (max_row - kdata.height/2),
                                xshift + ((max_col - kdata.width/2) - kdata.x) + dfield2->xres,
                                cdata.y + (max_row - kdata.height/2) + dfield2->yres, 
                                zshift);
        }
        else
        {
            if (!gwy_data_field_area_copy(dfield2, result,
                             0, 0, dfield2->xres, dfield2->yres,
                             xshift + ((max_col - kdata.width/2) - kdata.x),
                             cdata.y + (max_row - kdata.height/2)))
                g_warning("Error while putting field 2 at: %d %d\n", 
                    xshift + ((max_col - kdata.width/2) - kdata.x),
                    cdata.y + (max_row - kdata.height/2));

            gwy_data_field_area_add(result, 
                                xshift + ((max_col - kdata.width/2) - kdata.x),
                                cdata.y + (max_row - kdata.height/2),
                                xshift + ((max_col - kdata.width/2) - kdata.x) + dfield2->xres,
                                cdata.y + (max_row - kdata.height/2) + dfield2->yres, 
                                zshift);
            if (!gwy_data_field_area_copy(dfield1, result,
                             0, 0, dfield1->xres, dfield1->yres,
                             xshift, 0))
                g_warning("Error while putting field 1 at: %d %d\n", xshift, 0);
          }
        

        /*adjust boundary to be as smooth as possible*/
        if (args->boundary == GWY_MERGE_BOUNDARY_SMOOTH)
        {
            res_rect.x = 0;
            res_rect.width = result->xres;
            res_rect.y = cdata.y;
            res_rect.height = dfield1->yres - res_rect.y;
            f1_pos.x = -xshift;
            f1_pos.y = res_rect.y;
            f2_pos.x = -(xshift + ((max_col - kdata.width/2) - kdata.x));
            f2_pos.y = kdata.y - (max_row - kdata.height/2);

            merge_boundary(dfield1, dfield2, result,
                       res_rect, f1_pos, f2_pos,
                       args->direction, zshift);
        }
        if (args->direction == GWY_MERGE_DIRECTION_UP)
        {
            gwy_data_field_invert(dfield1, TRUE, FALSE, FALSE);
            gwy_data_field_invert(dfield2, TRUE, FALSE, FALSE);
            gwy_data_field_invert(result, TRUE, FALSE, FALSE);
            
        }
        break;

        case GWY_MERGE_DIRECTION_LEFT:
        case GWY_MERGE_DIRECTION_RIGHT:
        newxres = cdata.x + (max_col - kdata.width/2) + dfield2->xres;
        newyres = fabs((max_row - kdata.height/2) - kdata.y) + MAX(dfield1->yres, dfield2->yres);

        gwy_data_field_resample(result, newxres, newyres, GWY_INTERPOLATION_NONE);
        gwy_data_field_set_xreal(result, gwy_data_field_get_xreal(result)*(gdouble)newxres/dfield2->xres);
        gwy_data_field_set_yreal(result, gwy_data_field_get_yreal(result)*(gdouble)newyres/dfield2->yres);
        
        gwy_data_field_fill(result, 
                            MIN(gwy_data_field_get_min(dfield1),
                                gwy_data_field_get_min(dfield2)));
        zshift = gwy_data_field_area_get_avg(correlation_data, 
                                            max_col - kdata.width/2,
                                            max_row - kdata.height/2,
                                            kdata.width,
                                            kdata.height)
            - gwy_data_field_get_avg(correlation_kernel);
        
        /*fill the result with both data fields*/
        yshift = MAX(0, -(max_row - cdata.height/2));
        if (args->boundary == GWY_MERGE_BOUNDARY_SMOOTH || args->boundary == GWY_MERGE_BOUNDARY_FIRST)
        {
            if (!gwy_data_field_area_copy(dfield1, result,
                             0, 0, dfield1->xres, dfield1->yres,
                             0, yshift))
                g_warning("Error while putting field 1 at: %d %d\n", 0, yshift);
        
            if (!gwy_data_field_area_copy(dfield2, result,
                             0, 0, dfield2->xres, dfield2->yres,
                             cdata.x + (max_col - kdata.width/2), 
                             yshift + ((max_row - kdata.height/2) - kdata.y)))
                g_warning("Error while putting field 2 at: %d %d\n", 
                    cdata.x + (max_col - kdata.width/2),
                    yshift + ((max_row - kdata.height/2) - kdata.y));

            gwy_data_field_area_add(result, 
                                cdata.x + (max_col - kdata.width/2),
                                yshift + ((max_row - kdata.height/2) - kdata.y),
                                cdata.x + (max_col - kdata.width/2) + dfield2->xres, 
                                yshift + ((max_row - kdata.height/2) - kdata.y) + dfield2->yres,
                                zshift);
        }
        else
        {
            if (!gwy_data_field_area_copy(dfield2, result,
                             0, 0, dfield2->xres, dfield2->yres,
                             cdata.x + (max_col - kdata.width/2), 
                             yshift + ((max_row - kdata.height/2) - kdata.y)))
                g_warning("Error while putting field 2 at: %d %d\n", 
                    cdata.x + (max_col - kdata.width/2),
                    yshift + ((max_row - kdata.height/2) - kdata.y));

            gwy_data_field_area_add(result, 
                                cdata.x + (max_col - kdata.width/2),
                                yshift + ((max_row - kdata.height/2) - kdata.y),
                                cdata.x + (max_col - kdata.width/2) + dfield2->xres, 
                                yshift + ((max_row - kdata.height/2) - kdata.y) + dfield2->yres,
                                zshift);
             
            if (!gwy_data_field_area_copy(dfield1, result,
                             0, 0, dfield1->xres, dfield1->yres,
                             0, yshift))
                g_warning("Error while putting field 1 at: %d %d\n", 0, yshift);
         }
        

        /*adjust boundary to be as smooth as possible*/
        if (args->boundary == GWY_MERGE_BOUNDARY_SMOOTH)
        {
            res_rect.x = cdata.x;
            res_rect.width = dfield1->xres - res_rect.x;
            res_rect.y = 0;
            res_rect.height = result->yres;
            f1_pos.x = res_rect.x;
            f1_pos.y = -yshift;
            f2_pos.x = kdata.x - (max_col - kdata.width/2);
            f2_pos.y = -(yshift + ((max_row - kdata.height/2) - kdata.y));

            merge_boundary(dfield1, dfield2, result,
                       res_rect, f1_pos, f2_pos,
                       args->direction, zshift);
        }
        if (args->direction == GWY_MERGE_DIRECTION_LEFT)
        {
            gwy_data_field_invert(dfield1, FALSE, TRUE, FALSE);
            gwy_data_field_invert(dfield2, FALSE, TRUE, FALSE);
            gwy_data_field_invert(result, FALSE, TRUE, FALSE);
            
        }
        break;
        default:
        g_assert_not_reached();
        break;
 
    }
      
    /*set right output */
    if (result) {
        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    }
    
    g_object_unref(data);
    g_object_unref(correlation_data);
    g_object_unref(correlation_kernel);
    g_object_unref(correlation_score);
    
    return TRUE;
}

static gboolean      
get_score_iteratively(GwyDataField *data_field, GwyDataField *kernel_field,
                      GwyDataField *score, MergeArgs *args)
{
    /*compute crosscorelation */
    GwyComputationStateType state;
    gint iteration = 0;

    state = GWY_COMPUTATION_STATE_INIT;
    gwy_app_wait_start(GTK_WIDGET(args->win1), "Initializing...");
    do {
        gwy_data_field_correlate_iteration(data_field, kernel_field,
                                                score,
                                                &state,
                                                &iteration);
        gwy_app_wait_set_message("Correlating...");
        if (!gwy_app_wait_set_fraction
                (iteration/(gdouble)(gwy_data_field_get_xres(data_field) - 
                                     (gwy_data_field_get_xres(kernel_field))/2)))
        {
            return FALSE;
        }

    } while (state != GWY_COMPUTATION_STATE_FINISHED);
    gwy_app_wait_finish();

    return TRUE;
}

static void
find_score_maximum(GwyDataField *correlation_score, gint *max_col, gint *max_row)
{
    gint i, n, maxi;
    gdouble max = -G_MAXDOUBLE;
    gdouble *data;

    n = gwy_data_field_get_xres(correlation_score)*gwy_data_field_get_yres(correlation_score);
    data = gwy_data_field_get_data(correlation_score);
    
    for (i=0; i<n; i++)
        if (max < data[i]) {
            max = data[i];
            maxi = i;
        }
        
    *max_row = (gint)floor(maxi/gwy_data_field_get_xres(correlation_score));
    *max_col = maxi - (*max_row)*gwy_data_field_get_xres(correlation_score);
}

static inline gboolean
gwy_data_field_inside(GwyDataField *data_field, gint i, gint j)
{
    if (i >= 0 && j >= 0 && i < data_field->xres && j < data_field->yres)
        return TRUE;
    else
        return FALSE;
}


static void
merge_boundary(GwyDataField *dfield1, GwyDataField *dfield2, GwyDataField *result,
               GdkRectangle res_rect, GdkPoint f1_pos, GdkPoint f2_pos, 
               GwyMergeDirectionType direction, gdouble zshift)
{
    gint col, row;
    gdouble weight;

    for (col = 0; col < res_rect.width; col++)
    {
        for (row = 0; row < res_rect.height; row++)
        {
            if (!gwy_data_field_inside(dfield1, col + f1_pos.x, row + f1_pos.y))
                continue;
            if (!gwy_data_field_inside(dfield2, col + f2_pos.x, row + f2_pos.y))
                continue;
           
            if (direction == GWY_MERGE_DIRECTION_LEFT || direction == GWY_MERGE_DIRECTION_RIGHT)
                weight = (gdouble)col/(gdouble)res_rect.width;
            else 
                weight = (gdouble)row/(gdouble)res_rect.height;
           
            gwy_data_field_set_val(result, 
                                   col + res_rect.x, 
                                   row + res_rect.y,
                                   (1 - weight)*gwy_data_field_get_val(dfield1, 
                                                                 col + f1_pos.x,
                                                                 row + f1_pos.y)
                                   + weight*(zshift + gwy_data_field_get_val(dfield2,
                                                                       col + f2_pos.x,
                                                                       row + f2_pos.y)));
        }
    }
}

static void       
merge_direction_cb(GtkWidget *combo, MergeArgs *args)
{
    args->direction = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void       
merge_mode_cb(GtkWidget *combo, MergeArgs *args)
{
    args->mode = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void       
merge_boundary_cb(GtkWidget *combo, MergeArgs *args)
{
    args->boundary = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}



static const gchar direction_key[]      = "/module/merge/direction";
static const gchar mode_key[]      = "/module/merge/mode";
static const gchar boundary_key[]      = "/module/merge/boundary";


static void
merge_sanitize_args(MergeArgs *args)
{
    args->direction = MIN(args->direction, GWY_MERGE_DIRECTION_LAST - 1);
    args->mode = MIN(args->mode, GWY_MERGE_MODE_LAST - 1);
    args->boundary = MIN(args->boundary, GWY_MERGE_BOUNDARY_LAST - 1);
}

static void
merge_load_args(GwyContainer *settings,
                   MergeArgs *args)
{

    *args = merge_defaults;
    gwy_container_gis_enum_by_name(settings, direction_key, &args->direction);
    gwy_container_gis_enum_by_name(settings, mode_key, &args->mode);
    gwy_container_gis_enum_by_name(settings, boundary_key, &args->boundary);
    merge_sanitize_args(args);
}

static void
merge_save_args(GwyContainer *settings,
                   MergeArgs *args)
{
    gwy_container_set_enum_by_name(settings, direction_key, args->direction);
    gwy_container_set_enum_by_name(settings, mode_key, args->mode);
    gwy_container_set_enum_by_name(settings, boundary_key, args->boundary);
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

