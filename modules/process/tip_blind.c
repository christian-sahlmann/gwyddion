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
    gint xres;
    gint yres;
    gdouble thresh;
    gboolean use_boundaries;
    GwyDataWindow *win;
} TipBlindArgs;

typedef struct {
    GtkWidget *view;
    GtkWidget *data;
    GtkWidget *type;
    GtkWidget *threshold;
    GtkWidget *boundaries;
    GwyContainer *tip;
    GwyContainer *vtip;
    GwyContainer *surface;
    GtkObject *xres;
    GtkObject *yres;
    gint vxres;
    gint vyres;
} TipBlindControls;

static gboolean    module_register            (const gchar *name);
static gboolean    tip_blind                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    tip_blind_dialog                 (TipBlindArgs *args, GwyContainer *data);
static void        tip_blind_dialog_update_controls(TipBlindControls *controls,
                                               TipBlindArgs *args);
static void        tip_blind_dialog_update_values  (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void        reset                      (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void        partial                    (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void        full                        (TipBlindControls *controls,
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

static GtkWidget*  create_preset_menu        (GCallback callback,
                                              gpointer cbdata,
                                              gint current);
 
static void        width_changed_cb          (GtkAdjustment *adj,
                                               TipBlindArgs  *args);
static void        height_changed_cb         (GtkAdjustment *adj,
                                               TipBlindArgs  *args);
static void        thresh_changed_cb         (GwyValUnit *valunit,
                                             TipBlindArgs *args);
static void        bound_changed_cb          (GtkToggleButton *button,
                                              TipBlindArgs *args);
static void        tip_update                (TipBlindControls *controls,
                                              TipBlindArgs *args);

TipBlindArgs tip_blind_defaults = {
    100,
    100,
    0.000001,
    FALSE,
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
    GtkWidget *dialog, *table, *omenu, *vbox, *spin;
    TipBlindControls controls;
    enum { RESPONSE_RESET = 1,
           RESPONSE_PARTIAL = 2,
           RESPONSE_FULL = 3};
    gint response, col, row;
    GtkObject *layer;
    GtkWidget *hbox;
    GwyDataField *dfield;
    GtkWidget *label;

    dialog = gtk_dialog_new_with_buttons(_("Model tip"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("Partial estimation"), RESPONSE_PARTIAL,
                                         _("Full estimation"), RESPONSE_FULL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 3);

    table = gtk_table_new(5, 2, FALSE);
    col = 0; 
    row = 0;    
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,            
                       FALSE, FALSE, 4);

    controls.vxres = 200;
    controls.vyres = 200;
    
    controls.surface = data;    
    controls.tip = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.tip, "/0/data"));
    gwy_data_field_resample(dfield, args->xres, args->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_fill(dfield, 0);

    controls.vtip = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(controls.tip)));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.vtip, "/0/data"));
    gwy_data_field_resample(dfield, controls.vxres, controls.vyres, GWY_INTERPOLATION_NONE);

    controls.view = gwy_data_view_new(controls.vtip);
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
    
    label = gtk_label_new(_("Estimated tip size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1, GTK_FILL, 0, 2, 2);

    controls.xres = gtk_adjustment_new(args->xres,
                                                    1, 10000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 2, _("Width:"), _("px"),
                                               controls.xres);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_object_set_data(G_OBJECT(controls.xres), "controls", &controls);
    g_signal_connect(controls.xres, "value_changed",
                                     G_CALLBACK(width_changed_cb), args);
                                                                                                                                                                     
    controls.yres = gtk_adjustment_new(args->yres,
                                        1, 10000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 3, _("Height:"), _("px"),
                                            controls.yres);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_object_set_data(G_OBJECT(controls.yres), "controls", &controls);
    g_signal_connect(controls.yres, "value_changed",
                                          G_CALLBACK(height_changed_cb), args);
   
                  
    controls.threshold = gwy_val_unit_new("Threshold ",
                                       gwy_data_field_get_si_unit_z(dfield));
    gwy_val_unit_set_value(GWY_VAL_UNIT(controls.threshold), args->thresh);
             g_signal_connect(GWY_VAL_UNIT(controls.threshold), "value_changed",
                                                   G_CALLBACK(thresh_changed_cb), args);
                                                                                                                                 
    gtk_box_pack_start(GTK_BOX(vbox), controls.threshold,
                                                      FALSE, FALSE, 4);
                 
    controls.boundaries
                = gtk_check_button_new_with_label(_("Use boundaries"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.boundaries),
                                                 args->use_boundaries);
    g_signal_connect(controls.boundaries, "toggled", G_CALLBACK(bound_changed_cb), args);
     
    gtk_box_pack_start(GTK_BOX(vbox), controls.boundaries,
                       FALSE, FALSE, 4);
 
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
            tip_blind_dialog_update_controls(&controls, args);
            reset(&controls, args);
            break;

            case RESPONSE_PARTIAL:
            tip_blind_dialog_update_values(&controls, args);
            partial(&controls, args);
            break;

            case RESPONSE_FULL:
            tip_blind_dialog_update_values(&controls, args);
            full(&controls, args);
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
width_changed_cb(GtkAdjustment *adj,
                    TipBlindArgs *args)
{
    args->xres =  gtk_adjustment_get_value(adj);
}

static void
height_changed_cb(GtkAdjustment *adj,
                    TipBlindArgs *args)
{
    args->yres =  gtk_adjustment_get_value(adj);
}

static void
thresh_changed_cb(GwyValUnit *valunit,
                  TipBlindArgs *args)
{
    args->thresh = gwy_val_unit_get_value(valunit);
}

static void
bound_changed_cb(GtkToggleButton *button,
                 TipBlindArgs *args)
{
    args->use_boundaries = gtk_toggle_button_get_active(button);
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
reset(TipBlindControls *controls, TipBlindArgs *args)
{
    GwyDataField *tipfield;
    
    tipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->tip, "/0/data"));
    gwy_data_field_fill(tipfield, 0);

    tip_update(controls, args);
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}

static void
partial(TipBlindControls *controls, TipBlindArgs *args)
{
    GwyDataField *tipfield, *surface;

    tipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->tip, "/0/data"));
    surface = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->surface, "/0/data"));

    tipfield = gwy_tip_estimate_partial(tipfield, surface, args->thresh, args->use_boundaries);

    tip_update(controls, args);
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}

static void
full(TipBlindControls *controls, TipBlindArgs *args)
{
    GwyDataField *tipfield, *surface;

    tipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->tip, "/0/data"));
    surface = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->surface, "/0/data"));

    tipfield = gwy_tip_estimate_full(tipfield, surface, args->thresh, args->use_boundaries);

    tip_update(controls, args);
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}

static void
tip_update(TipBlindControls *controls, TipBlindArgs *args)
{
    GwyDataField *tipfield, *vtipfield, *buffer;
    
    tipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->tip, "/0/data"));
    buffer = GWY_DATA_FIELD(gwy_serializable_duplicate(tipfield));
    gwy_data_field_resample(buffer, controls->vxres, controls->vyres, GWY_INTERPOLATION_ROUND);

    vtipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->vtip, "/0/data"));
    
    gwy_data_field_copy(buffer, vtipfield);
    
    g_object_unref(buffer);
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
