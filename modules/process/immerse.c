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
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#define IMMERSE_RUN_MODES \
    (GWY_RUN_INTERACTIVE)

typedef enum {
    GWY_IMMERSE_LEVELING_LARGE,
    GWY_IMMERSE_LEVELING_DETAIL,
    GWY_IMMERSE_LEVELING_NONE,
    GWY_IMMERSE_LEVELING_LAST
} GwyImmerseLevelingType;

typedef enum {
    GWY_IMMERSE_MODE_CORRELATE,
    GWY_IMMERSE_MODE_NONE,
    GWY_IMMERSE_MODE_LAST
} GwyImmerseModeType;

typedef enum {
    GWY_IMMERSE_SAMPLING_UP,
    GWY_IMMERSE_SAMPLING_DOWN,
    GWY_IMMERSE_SAMPLING_LAST
} GwyImmerseSamplingType;


typedef struct {
    GwyDataWindow *win1;
    GwyDataWindow *win2;
    GwyImmerseLevelingType leveling;
    GwyImmerseModeType mode;
    GwyImmerseSamplingType sampling;
} ImmerseArgs;

typedef struct {
    GtkWidget *leveling;
    GtkWidget *mode;
    GtkWidget *sampling;
} ImmerseControls;

static gboolean   module_register               (const gchar *name);
static gboolean   immerse                  (GwyContainer *data,
                                                 GwyRunType run);
static GtkWidget* immerse_window_construct (ImmerseArgs *args);
static void       immerse_data_cb          (GtkWidget *item);
static gboolean   immerse_check            (ImmerseArgs *args,
                                                 GtkWidget *immerse_window);
static gboolean   immerse_do               (ImmerseArgs *args);
static GtkWidget* immerse_data_option_menu (GwyDataWindow **operand);
static void       immerse_leveling_cb     (GtkWidget *combo,
                                          ImmerseArgs *args);
static void       immerse_mode_cb         (GtkWidget *combo,
                                          ImmerseArgs *args);
static void       immerse_sampling_cb        (GtkWidget *combo,
                                          ImmerseArgs *args);


static void       immerse_load_args        (GwyContainer *settings,
                                          ImmerseArgs *args);
static void       immerse_save_args        (GwyContainer *settings,
                                          ImmerseArgs *args);
static void       immerse_sanitize_args    (ImmerseArgs *args);

static gboolean   get_score_iteratively(GwyDataField *data_field,
                                        GwyDataField *kernel_field,
                                        GwyDataField *score,
                                        ImmerseArgs *args);
static void       find_score_maximum   (GwyDataField *correlation_score,
                                        gint *max_col,
                                        gint *max_row);


static const GwyEnum levelings[] = {
    { N_("Large image"),   GWY_IMMERSE_LEVELING_LARGE },
    { N_("Detail image"),  GWY_IMMERSE_LEVELING_DETAIL },
    { N_("None"),          GWY_IMMERSE_LEVELING_NONE },
};

static const GwyEnum modes[] = {
    { N_("Correlation"),   GWY_IMMERSE_MODE_CORRELATE },
    { N_("None"),          GWY_IMMERSE_MODE_NONE },
};

static const GwyEnum samplings[] = {
    { N_("Upsample large image"), GWY_IMMERSE_SAMPLING_UP },
    { N_("Downsample detail"),    GWY_IMMERSE_SAMPLING_DOWN },
};


static const ImmerseArgs immerse_defaults = {
    NULL, NULL, GWY_IMMERSE_LEVELING_DETAIL, GWY_IMMERSE_MODE_CORRELATE, GWY_IMMERSE_SAMPLING_UP
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Immerse high resolution detail into overall image."),
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
    static GwyProcessFuncInfo immerse_func_info = {
        "immerse",
        N_("/M_ultidata/_Immerse..."),
        (GwyProcessFunc)&immerse,
        IMMERSE_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &immerse_func_info);

    return TRUE;
}

/* FIXME: we ignore the Container argument and use current data window */
static gboolean
immerse(GwyContainer *data, GwyRunType run)
{
    GtkWidget *immerse_window;
    GwyContainer *settings;
    ImmerseArgs args;
    gboolean ok = FALSE;

    g_return_val_if_fail(run & IMMERSE_RUN_MODES, FALSE);
    settings = gwy_app_settings_get();
    immerse_load_args(settings, &args);

    args.win1 = args.win2 = gwy_app_data_window_get_current();
    g_assert(gwy_data_window_get_data(args.win1) == data);
    immerse_window = immerse_window_construct(&args);
    gtk_window_present(GTK_WINDOW(immerse_window));

    do {
        switch (gtk_dialog_run(GTK_DIALOG(immerse_window))) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(immerse_window);
            ok = TRUE;
            break;

            case GTK_RESPONSE_OK:
            ok = immerse_check(&args, immerse_window);
            if (ok) {
                gtk_widget_destroy(immerse_window);
                immerse_do(&args);
                immerse_save_args(settings, &args);
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
immerse_window_construct(ImmerseArgs *args)
{
    GtkWidget *dialog, *table, *omenu, *label, *combo;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Immerse data"), NULL, 0,
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
    label = gtk_label_new_with_mnemonic(_("_Large image:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = immerse_data_option_menu(&args->win1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    row++;

    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("_Detail image:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = immerse_data_option_menu(&args->win2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    row++;

    /*Parameters*/
    combo = gwy_enum_combo_box_new(levelings, G_N_ELEMENTS(levelings),
                                   G_CALLBACK(immerse_leveling_cb), args,
                                   args->leveling, TRUE);
    gwy_table_attach_hscale(table, row, _("_Leveling:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    combo = gwy_enum_combo_box_new(modes, G_N_ELEMENTS(modes),
                                   G_CALLBACK(immerse_mode_cb), args,
                                   args->mode, TRUE);
    gwy_table_attach_hscale(table, row, _("_Align detail:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    combo = gwy_enum_combo_box_new(samplings, G_N_ELEMENTS(samplings),
                                   G_CALLBACK(immerse_sampling_cb), args,
                                   args->sampling, TRUE);
    gwy_table_attach_hscale(table, row, _("_Result sampling:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    
    gtk_widget_show_all(dialog);

    return dialog;
}

static GtkWidget*
immerse_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(immerse_data_cb),
                                        NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}


static void
immerse_data_cb(GtkWidget *item)
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
immerse_check(ImmerseArgs *args,
               GtkWidget *immerse_window)
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

    if (dfield1->xreal < dfield2->xreal || dfield1->yreal < dfield2->yreal)
    {
        dialog = gtk_message_dialog_new(GTK_WINDOW(immerse_window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK,
                                    _("Detail image must be smaller (regarding physical size)"
                                       " than large image"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;
    }
    return TRUE;
}

static gboolean
immerse_do(ImmerseArgs *args)
{
    GtkWidget *data_window;
    GwyContainer *data;
    GwyDataField *resampled, *score, *dfield1, *dfield2, *result;
    GwyDataWindow *operand1, *operand2;
    gint max_col, max_row;

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

    if (args->sampling == GWY_IMMERSE_SAMPLING_DOWN)
    {
        resampled = gwy_data_field_new_alike(dfield2, FALSE);
        gwy_data_field_resample(result, dfield1->xres, dfield1->yres, GWY_INTERPOLATION_NONE);
        score = gwy_data_field_new_alike(dfield1, FALSE);
        
        gwy_data_field_copy(dfield1, result, FALSE);
        gwy_data_field_copy(dfield2, resampled, FALSE);
        gwy_data_field_resample(resampled, 
                                dfield1->xres*dfield2->xreal/dfield1->xreal,
                                dfield1->yres*dfield2->yreal/dfield1->yreal,
                                GWY_INTERPOLATION_BILINEAR);

        get_score_iteratively(dfield1, resampled,
                              score, args);
        find_score_maximum(score, &max_col, &max_row);

        gwy_data_field_area_copy(resampled, result, 
                                 0, 0, 
                                 resampled->xres, resampled->yres,
                                 max_col - resampled->xres/2,
                                 max_row - resampled->yres/2);
        g_object_unref(resampled);
    }
    else
    {
        result = gwy_data_field_new_alike(dfield1, FALSE);
        gwy_data_field_copy(dfield1, result, FALSE);
        
        gwy_data_field_resample(result,
                                dfield2->xres*dfield1->xreal/dfield2->xreal,
                                dfield2->yres*dfield1->yreal/dfield2->yreal,
                                GWY_INTERPOLATION_BILINEAR);
        score = gwy_data_field_new_alike(result, FALSE);
        get_score_iteratively(result, dfield2,
                                     score, args);
        find_score_maximum(score, &max_col, &max_row);
        gwy_data_field_area_copy(dfield2, result, 
                                 0, 0, 
                                 resampled->xres, resampled->yres,
                                 max_col - resampled->xres/2,
                                 max_row - resampled->yres/2);
         
    }

    /*set right output */

    if (result) {
        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    }
    g_object_unref(data);
    g_object_unref(score);
    

    return TRUE;
}

static gboolean
get_score_iteratively(GwyDataField *data_field, GwyDataField *kernel_field,
                      GwyDataField *score, ImmerseArgs *args)
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


static void       
immerse_leveling_cb(GtkWidget *combo, ImmerseArgs *args)
{
    args->leveling = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void       
immerse_mode_cb(GtkWidget *combo, ImmerseArgs *args)
{
    args->mode = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void       
immerse_sampling_cb(GtkWidget *combo, ImmerseArgs *args)
{
    args->sampling = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}


static const gchar leveling_key[]      = "/module/immerse/leveling";
static const gchar mode_key[]          = "/module/immerse/mode";
static const gchar sampling_key[]      = "/module/immerse/sampling";


static void
immerse_sanitize_args(ImmerseArgs *args)
{
    args->leveling = MIN(args->leveling, GWY_IMMERSE_LEVELING_LAST - 1);
    args->mode = MIN(args->mode, GWY_IMMERSE_MODE_LAST - 1);
    args->sampling = MIN(args->sampling, GWY_IMMERSE_SAMPLING_LAST - 1);
}

static void
immerse_load_args(GwyContainer *settings,
                   ImmerseArgs *args)
{

    *args = immerse_defaults;
    gwy_container_gis_enum_by_name(settings, leveling_key, &args->leveling);
    gwy_container_gis_enum_by_name(settings, mode_key, &args->mode);
    gwy_container_gis_enum_by_name(settings, sampling_key, &args->sampling);
    immerse_sanitize_args(args);
}

static void
immerse_save_args(GwyContainer *settings,
                   ImmerseArgs *args)
{
    gwy_container_set_enum_by_name(settings, leveling_key, args->leveling);
    gwy_container_set_enum_by_name(settings, mode_key, args->mode);
    gwy_container_set_enum_by_name(settings, sampling_key, args->sampling);
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

