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

#include <math.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/tip.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define TIP_BLIND_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)



/* Data for this function. */
typedef struct {
    gdouble height;
    gdouble radius;
    GwyTipType type;
    GwyDataWindow *win;
} TipBlindArgs;

typedef struct {
    GtkWidget *view;
    GtkWidget *data;
    GtkWidget *type;
    GtkWidget *radius;
    GtkObject *slope;
    GwyContainer *tip;
    GwyContainer *surface;
} TipBlindControls;

static gboolean    module_register            (const gchar *name);
static gboolean    tip_blind                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    tip_blind_dialog                 (TipBlindArgs *args, GwyContainer *data);
static void        tip_blind_dialog_update_controls(TipBlindControls *controls,
                                               TipBlindArgs *args);
static void        tip_blind_dialog_update_values  (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void        preview                    (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void        tip_blind_do                (TipBlindControls *controls, TipBlindArgs *args);
static void        tip_process                 (TipBlindControls *controls, TipBlindArgs *args); 
static void        tip_blind_load_args              (GwyContainer *container,
                                               TipBlindArgs *args);
static void        tip_blind_save_args              (GwyContainer *container,
                                               TipBlindArgs *args);
static void        tip_blind_sanitize_args         (TipBlindArgs *args);
static GtkWidget*  tip_blind_data_option_menu      (GwyDataWindow **operand);
static void        tip_blind_data_cb(GtkWidget *item);

static void        tip_type_cb     (GtkWidget *item,
                                             TipBlindArgs *args);
static GtkWidget*  create_preset_menu        (GCallback callback,
                                              gpointer cbdata,
                                              gint current);
                                              

TipBlindArgs tip_blind_defaults = {
    0,
    0,
    0,
    NULL,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "tip_blind",
    "Blind reconstruction of SPM tip",
    "Petr Klapetek <petr@klapetek.cz>",
    "1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo tip_blind_func_info = {
        "tip_blind",
        "/_Tip operations/_Blind reconstruction...",
        (GwyProcessFunc)&tip_blind,
        TIP_BLIND_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &tip_blind_func_info);

    return TRUE;
}

static gboolean
tip_blind(GwyContainer *data, GwyRunType run)
{
    TipBlindArgs args;
    gboolean ok = FALSE;

    g_assert(run & TIP_BLIND_RUN_MODES);
    
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = tip_blind_defaults;
    else
        tip_blind_load_args(gwy_app_settings_get(), &args);

    ok = (run != GWY_RUN_MODAL) || tip_blind_dialog(&args, data);
    if (run == GWY_RUN_MODAL)
        tip_blind_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    tip_blind_do(&args, data);

    return ok;
}


static gboolean
tip_blind_dialog(TipBlindArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *omenu, *vbox;
    TipBlindControls controls;
    enum { RESPONSE_RESET = 1,
           RESPONSE_PREVIEW = 2 };
    gint response, col, row;
    GtkObject *layer;
    GtkWidget *hbox;
    GwyDataField *dfield;
    GtkWidget *label;

    dialog = gtk_dialog_new_with_buttons(_("Model tip"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("Update preview"), RESPONSE_PREVIEW,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 3);

    table = gtk_table_new(3, 2, FALSE);
    col = 0; 
    row = 0;
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.surface = data;    
    controls.tip = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.tip, "/0/data"));
    gwy_data_field_fill(dfield, 0);

    controls.view = gwy_data_view_new(controls.tip);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));

    
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 4);

    label = gtk_label_new(_("Related data:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1, GTK_FILL, 0, 2, 2);

 
    omenu = tip_blind_data_option_menu(&args->win);
    gtk_table_attach(GTK_TABLE(table), omenu, 1, 2, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 4);
    row++;
    
    label = gtk_label_new(_("Tip type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1, GTK_FILL, 0, 2, 2);


    controls.radius = gwy_val_unit_new("Tip apex radius: ", gwy_data_field_get_si_unit_xy(dfield));
    gtk_box_pack_start(GTK_BOX(vbox), controls.radius, FALSE, FALSE, 4);                                                   

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            tip_blind_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = tip_blind_defaults;
            tip_blind_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            tip_blind_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    tip_blind_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);

    return TRUE;
}


static GtkWidget*
tip_blind_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(tip_blind_data_cb),
                                       NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}

static void
tip_blind_data_cb(GtkWidget *item)
{
    GtkWidget *menu;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);

    p = g_object_get_data(G_OBJECT(item), "data-window");
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}

static void        
tip_type_cb     (GtkWidget *item, TipBlindArgs *args)
{
    args->type =
                GPOINTER_TO_INT(g_object_get_data(item, "tip-preset"));    
}


static void
tip_blind_dialog_update_controls(TipBlindControls *controls, TipBlindArgs *args)
{
}

static void
tip_blind_dialog_update_values(TipBlindControls *controls, TipBlindArgs *args)
{
}


static void
preview(TipBlindControls *controls, TipBlindArgs *args)
{
}

static void
tip_blind_do(TipBlindControls *controls, TipBlindArgs *args)
{
    
    tip_process(controls, args);
}

static void
tip_process(TipBlindControls *controls, TipBlindArgs *args)
{
}

static const gchar *mergetype_key = "/module/tip_blind_height/merge_type";

static void
tip_blind_sanitize_args(TipBlindArgs *args)
{
}

static void
tip_blind_load_args(GwyContainer *container,
               TipBlindArgs *args)
{
    *args = tip_blind_defaults;
    args->type = 0;
    /*
    gwy_container_gis_double_by_name(container, slope_key, &args->slope);
                                   
    tip_blind_sanitize_args(args);
    */
}

static void
tip_blind_save_args(GwyContainer *container,
               TipBlindArgs *args)
{
    /*
    gwy_container_set_boolean_by_name(container, inverted_key, args->inverted);
    */
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
