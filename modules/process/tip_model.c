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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/tip.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define TIP_MODEL_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_WITH_DEFAULTS)



GwyEnum tip_type[] = {
        { N_("Pyramide (general)"),   GWY_TIP_PYRAMIDE    },
        { N_("Contact etched"),       GWY_TIP_CONTACT     },
        { N_("Noncontact etched"),    GWY_TIP_NONCONTACT  },
        { N_("Delta function"),       GWY_TIP_DELTA       },
};

/* Data for this function. */
typedef struct {
    gint nsides;
    gdouble angle;
    gdouble radius;
    gdouble theta;
    GwyTipType type;
    GwyDataWindow *win;
} TipModelArgs;

typedef struct {
    GtkWidget *view;
    GtkWidget *data;
    GtkWidget *type;
    GtkWidget *radius;
    GtkObject *angle;
    GtkObject *theta;
    GtkObject *nsides;
    GtkWidget *labsize;
    GtkObject *slope;
    GtkWidget *spin_angle;
    GtkWidget *spin_theta;
    GtkWidget *spin_nsides;
    GwyContainer *tip;
    GwyContainer *vtip;
    gint vxres;
    gint vyres;
    gboolean tipdone;
} TipModelControls;

static gboolean    module_register            (const gchar *name);
static gboolean    tip_model                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    tip_model_dialog                 (TipModelArgs *args, GwyContainer *data);
static void        tip_model_dialog_update_controls(TipModelControls *controls,
                                               TipModelArgs *args);
static void        tip_model_dialog_update_values  (TipModelControls *controls,
                                               TipModelArgs *args);
static void        preview                    (TipModelControls *controls,
                                               TipModelArgs *args);
static void        tip_model_do                (TipModelArgs *args, TipModelControls *controls);
static void        tip_process                 (TipModelArgs *args, TipModelControls *controls);
static void        tip_model_load_args              (GwyContainer *container,
                                               TipModelArgs *args);
static void        tip_model_save_args              (GwyContainer *container,
                                               TipModelArgs *args);
static void        tip_model_sanitize_args         (TipModelArgs *args);
static GtkWidget*  tip_model_data_option_menu      (GwyDataWindow **operand);
static void        tip_model_data_cb(GtkWidget *item);

static void        tip_type_cb     (GtkWidget *item,
                                             TipModelArgs *args);
static GtkWidget*  create_preset_menu        (GCallback callback,
                                              gpointer cbdata,
                                              gint current);
static void        tip_update                (TipModelControls *controls,
                                              TipModelArgs *args);
static void        radius_changed_cb          (GwyValUnit *valunit,
                                               TipModelArgs *args);
static void        tip_model_dialog_abandon   (TipModelControls *controls);


TipModelArgs tip_model_defaults = {
    4,
    54.73561032,
    200e-9,
    0,
    0,
    NULL,
};

TipModelControls *pcontrols;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "tip_model",
    N_("Model SPM tip"),
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
    static GwyProcessFuncInfo tip_model_func_info = {
        "tip_model",
        N_("/_Tip/_Model Tip..."),
        (GwyProcessFunc)&tip_model,
        TIP_MODEL_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &tip_model_func_info);

    return TRUE;
}

static gboolean
tip_model(GwyContainer *data, GwyRunType run)
{
    TipModelArgs args;
    gboolean ok = FALSE;

    g_assert(run & TIP_MODEL_RUN_MODES);

    if (run == GWY_RUN_WITH_DEFAULTS)
        args = tip_model_defaults;
    else
        tip_model_load_args(gwy_app_settings_get(), &args);
    args.win = gwy_app_data_window_get_current();
    ok = (run != GWY_RUN_MODAL) || tip_model_dialog(&args, data);
    if (run == GWY_RUN_MODAL)
        tip_model_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    return ok;
}


static gboolean
tip_model_dialog(TipModelArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *omenu, *vbox;
    TipModelControls controls;
    enum { RESPONSE_RESET = 1,
           RESPONSE_PREVIEW = 2 };
    gint response, col, row;
    GtkObject *layer;
    GtkWidget *hbox;
    GwyDataField *dfield;
    GtkWidget *label;

    dialog = gtk_dialog_new_with_buttons(_("Model Tip"), NULL, 0,
                                         _("_Update Preview"), RESPONSE_PREVIEW,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 3);

    table = gtk_table_new(5, 2, FALSE);
    col = 0;
    row = 0;
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    pcontrols = &controls;
    controls.vxres = 200;
    controls.vyres = 200;

    /* set up initial tip field*/
    controls.tip = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.tip,
                                                             "/0/data"));
    gwy_data_field_fill(dfield, 0);

    /* set up resamplev view data */
    controls.vtip
        = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(controls.tip)));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.vtip,
                                                             "/0/data"));
    gwy_data_field_resample(dfield, controls.vxres, controls.vyres,
                            GWY_INTERPOLATION_ROUND);

    /* set up resampled view */
    controls.view = gwy_data_view_new(controls.vtip);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));

    /* set up tip model controls */
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 4);

    label = gtk_label_new_with_mnemonic(_("Related _data:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 2, 2);

    omenu = tip_model_data_option_menu(&args->win);
    gtk_table_attach(GTK_TABLE(table), omenu, 1, 2, row, row+1,
                     GTK_FILL, 0, 2, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 4);
    row++;

    label = gtk_label_new_with_mnemonic(_("Tip _type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 2, 2);

    controls.type = create_preset_menu(G_CALLBACK(tip_type_cb),
                                       args, args->type);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.type);

    gtk_table_attach(GTK_TABLE(table), controls.type, 1, 2, row, row+1,
                     GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 4);

    controls.nsides = gtk_adjustment_new(args->nsides,
                                         3, 100, 1, 5, 0);
    controls.spin_nsides
        = gwy_table_attach_spinbutton(table, 3, _("_Number of sides:"), " ",
                                        controls.nsides);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.spin_nsides), 0);

    controls.angle = gtk_adjustment_new(args->angle,
                                         0.1, 89.9, 0.1, 1, 0);
    controls.spin_angle
        = gwy_table_attach_spinbutton(table, 4, _("Tip _slope:"), _("deg"),
                                        controls.angle);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.spin_angle), 2);

    controls.theta = gtk_adjustment_new(args->theta,
                                         0, 360, 0.1, 1, 0);
    controls.spin_theta
        = gwy_table_attach_spinbutton(table, 5, _("Tip _rotation:"), _("deg"),
                                        controls.theta);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.spin_theta), 2);

    controls.radius = gwy_val_unit_new(_("Tip _apex radius:"),
                                       gwy_data_field_get_si_unit_xy(dfield));
    g_signal_connect(controls.radius, "value_changed",
                     G_CALLBACK(radius_changed_cb), args);
    gwy_val_unit_set_value(GWY_VAL_UNIT(controls.radius), args->radius);

    gtk_box_pack_start(GTK_BOX(vbox), controls.radius, FALSE, FALSE, 4);

    controls.labsize = gtk_label_new(_("Resolution will be determined "
                                       "according to tip type."));
    gtk_misc_set_alignment(GTK_MISC(controls.labsize), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), controls.labsize, FALSE, FALSE, 4);

    controls.tipdone = FALSE;
    tip_model_dialog_update_controls(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            tip_model_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            tip_model_do(args, &controls);
            break;

            case RESPONSE_RESET:
            *args = tip_model_defaults;
            tip_model_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            tip_model_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    tip_model_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);
    tip_model_dialog_abandon(&controls);

    return TRUE;
}

static void
tip_model_dialog_abandon(TipModelControls *controls)
{
    /*free data of the rescaled tip image*/
    g_object_unref(controls->vtip);

    /*if dialog was cancelled, free also tip data*/
    if (!controls->tipdone) g_object_unref(controls->tip);
}

static GtkWidget*
tip_model_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(tip_model_data_cb),
                                       NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}

static void
tip_model_data_cb(GtkWidget *item)
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
tip_type_cb(GtkWidget *item, TipModelArgs *args)
{
    args->type
        = GPOINTER_TO_INT(g_object_get_data(GTK_OBJECT(item), "tip-preset"));
    tip_model_dialog_update_controls(pcontrols, args);
}


static GtkWidget*
create_preset_menu(GCallback callback,
                   gpointer cbdata,
                   gint current)
{
    static GwyEnum *entries = NULL;
    static gint nentries = 0;

    if (!entries) {
        const GwyTipModelPreset *func;
        gint i;

        nentries = gwy_tip_model_get_npresets();
        entries = g_new(GwyEnum, nentries);
        for (i = 0; i < nentries; i++) {
            entries[i].value = i;
            func = gwy_tip_model_get_preset(i);
            entries[i].name = gwy_tip_model_get_preset_tip_name(func);
        }
    }

    return gwy_option_menu_create(entries, nentries,
                                  "tip-preset", callback, cbdata,
                                  current);
}

static void
tip_model_dialog_update_controls(TipModelControls *controls, TipModelArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->nsides),
                             args->nsides);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->angle),
                             args->angle);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->theta),
                             args->theta);
    switch (args->type)
    {
        case GWY_TIP_PYRAMIDE:
        gtk_widget_set_sensitive(controls->spin_angle, TRUE);
        gtk_widget_set_sensitive(controls->spin_theta, TRUE);
        gtk_widget_set_sensitive(controls->spin_nsides, TRUE);
        break;

        case GWY_TIP_CONTACT:
        gtk_widget_set_sensitive(controls->spin_angle, FALSE);
        gtk_widget_set_sensitive(controls->spin_theta, TRUE);
        gtk_widget_set_sensitive(controls->spin_nsides, FALSE);
        break;

        case GWY_TIP_NONCONTACT:
        gtk_widget_set_sensitive(controls->spin_angle, FALSE);
        gtk_widget_set_sensitive(controls->spin_theta, TRUE);
        gtk_widget_set_sensitive(controls->spin_nsides, FALSE);
        break;

        case GWY_TIP_DELTA:
        gtk_widget_set_sensitive(controls->spin_angle, FALSE);
        gtk_widget_set_sensitive(controls->spin_theta, FALSE);
        gtk_widget_set_sensitive(controls->spin_nsides, FALSE);
        break;

        default:
        break;
    }
}

static void
tip_model_dialog_update_values(TipModelControls *controls, TipModelArgs *args)
{
    args->nsides = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->nsides));
    args->angle = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->angle));
    args->theta = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->theta));
}

static void
tip_update(TipModelControls *controls, TipModelArgs *args)
{
   GwyDataField *tipfield, *vtipfield, *buffer;
   tipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->tip,
                                                              "/0/data"));

   buffer = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(tipfield)));
   gwy_data_field_resample(buffer, controls->vxres, controls->vyres,
                           GWY_INTERPOLATION_ROUND);

   vtipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->vtip,
                                                               "/0/data"));
   gwy_data_field_copy(buffer, vtipfield);
   g_object_unref(buffer);
}


static void
preview(TipModelControls *controls, TipModelArgs *args)
{
    tip_process(args, controls);
    tip_update(controls, args);

    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}

static void
tip_model_do(TipModelArgs *args, TipModelControls *controls)
{
    GtkWidget *data_window;
    tip_process(args, controls);

    data_window = gwy_app_data_window_create(controls->tip);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    controls->tipdone = TRUE;
}

static void
tip_process(TipModelArgs *args, TipModelControls *controls)
{
    GwyTipModelPreset *preset;
    GwyDataField *dfield;
    GwyDataField *sfield;
    GwyContainer *data;
    gchar label[64];
    gint xres, yres;
    gdouble xstep, ystep;
    gdouble params[2];

    preset = gwy_tip_model_get_preset(args->type);
    if (preset == NULL) return;

    tip_model_dialog_update_values(controls, args);

    /*guess x and y size*/
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->tip,
                                                             "/0/data"));
    data = gwy_data_window_get_data(args->win);
    sfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    gwy_data_field_set_xreal(dfield,
                             gwy_data_field_get_xreal(sfield)
                             /gwy_data_field_get_xres(sfield)
                             *gwy_data_field_get_xres(dfield));
    gwy_data_field_set_yreal(dfield,
                             gwy_data_field_get_yreal(sfield)
                             /gwy_data_field_get_yres(sfield)
                             *gwy_data_field_get_yres(dfield));

    params[0] = args->nsides;
    params[1] = args->angle*G_PI/180;
    preset->guess(sfield,
                  gwy_data_field_get_max(sfield)
                  - gwy_data_field_get_min(sfield),
                  args->radius, params, &xres, &yres);

    /*process tip*/
    /*FIXME this must be solved within guess functions*/
    xres = CLAMP(xres, 20, 1000);
    yres = CLAMP(yres, 20, 1000);

    g_snprintf(label, sizeof(label), _("Tip resolution: %d x %d pixels"),
               xres, yres);
    gtk_label_set_text(GTK_LABEL(controls->labsize), label);

    xstep = dfield->xreal/dfield->xres;
    ystep = dfield->yreal/dfield->yres;
    gwy_data_field_resample(dfield, xres, yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_set_xreal(dfield, xstep*xres);
    gwy_data_field_set_yreal(dfield, ystep*yres);

    preset->func(dfield,
                 gwy_data_field_get_max(sfield)
                 - gwy_data_field_get_min(sfield),
                 args->radius, args->theta*G_PI/180, params);
    tip_update(controls, args);
}

static void
radius_changed_cb(GwyValUnit *valunit, TipModelArgs *args)
{
    args->radius = gwy_val_unit_get_value(valunit);
}

static const gchar *nsides_key = "/module/tip_model/nsides";
static const gchar *angle_key = "/module/tip_model/angle";
static const gchar *radius_key = "/module/tip_model/radius";
static const gchar *theta_key = "/module/tip_model/theta";
static const gchar *type_key = "/module/tip_model/tip_type";

static void
tip_model_sanitize_args(TipModelArgs *args)
{
    args->nsides = CLAMP(args->nsides, 3, 100);
    args->type = CLAMP(args->type, GWY_TIP_PYRAMIDE, GWY_TIP_DELTA);
}

static void
tip_model_load_args(GwyContainer *container,
               TipModelArgs *args)
{
    *args = tip_model_defaults;
    args->type = 0;

    gwy_container_gis_int32_by_name(container, nsides_key, &args->nsides);
    gwy_container_gis_double_by_name(container, angle_key, &args->angle);
    gwy_container_gis_double_by_name(container, theta_key, &args->theta);
    gwy_container_gis_double_by_name(container, radius_key, &args->radius);
    gwy_container_gis_int32_by_name(container, type_key, &args->type);

    tip_model_sanitize_args(args);
}

static void
tip_model_save_args(GwyContainer *container,
               TipModelArgs *args)
{

    gwy_container_set_int32_by_name(container, nsides_key, args->nsides);
    gwy_container_set_double_by_name(container, angle_key, args->angle);
    gwy_container_set_double_by_name(container, radius_key, args->radius);
    gwy_container_set_double_by_name(container, theta_key, args->theta);
    gwy_container_set_int32_by_name(container, type_key, args->type);
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
