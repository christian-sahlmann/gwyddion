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

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define UNROTATE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

enum {
    PREVIEW_SIZE = 120
};

/* Data for this function. */
typedef struct {
    GwyInterpolationType interp;
    GwyPlaneSymmetry symmetry;
} UnrotateArgs;

typedef struct {
    GtkWidget *interp;
    GtkWidget *symmetry;
    GtkWidget *symmlabel;
    GtkWidget *corrlabel;
    GtkWidget *data_view;
    GwyContainer *data;
    UnrotateArgs *args;
    GwyPlaneSymmetry guess;
    gdouble *correction;
} UnrotateControls;

static gboolean         module_register          (const gchar *name);
static gboolean         unrotate                 (GwyContainer *data,
                                                  GwyRunType run);
static gboolean         unrotate_dialog          (UnrotateArgs *args,
                                                  GwyContainer *data,
                                                  gdouble *correction,
                                                  GwyPlaneSymmetry guess);
static void             unrotate_dialog_update   (UnrotateControls *controls,
                                                  UnrotateArgs *args);
static void             unrotate_symmetry_cb     (GtkWidget *item,
                                                  UnrotateControls *controls);
static void             unrotate_interp_cb       (GtkWidget *item,
                                                  UnrotateControls *controls);
static void             load_args                (GwyContainer *container,
                                                  UnrotateArgs *args);
static void             save_args                (GwyContainer *container,
                                                  UnrotateArgs *args);
static void             sanitize_args            (UnrotateArgs *args);

GwyEnum unrotate_symmetry[] = {
    { N_("Detected"),   GWY_SYMMETRY_AUTO       },
    { N_("Parallel"),   GWY_SYMMETRY_PARALLEL   },
    { N_("Triangular"), GWY_SYMMETRY_TRIANGULAR },
    { N_("Square"),     GWY_SYMMETRY_SQUARE     },
    { N_("Rhombic"),    GWY_SYMMETRY_RHOMBIC    },
    { N_("Hexagonal"),  GWY_SYMMETRY_HEXAGONAL  },
};

UnrotateArgs unrotate_defaults = {
    GWY_INTERPOLATION_BILINEAR,
    GWY_SYMMETRY_AUTO,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Rotates data to make characteristic directions parallel "
       "with x or y axis."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo unrotate_func_info = {
        "unrotate",
        N_("/_Correct Data/_Unrotate..."),
        (GwyProcessFunc)&unrotate,
        UNROTATE_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &unrotate_func_info);

    return TRUE;
}

static gboolean
unrotate(GwyContainer *data, GwyRunType run)
{
    enum { nder = 4800 };
    GtkWidget *data_window;
    GwyDataField *dfield;
    UnrotateArgs args;
    gdouble correction[GWY_SYMMETRY_LAST];
    GwyPlaneSymmetry symm;
    GwyDataLine *derdist;
    gdouble phi;
    gboolean ok;

    g_return_val_if_fail(run & UNROTATE_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = unrotate_defaults;
    else
        load_args(gwy_app_settings_get(), &args);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    derdist = GWY_DATA_LINE(gwy_data_line_new(nder, 2*G_PI, FALSE));
    gwy_data_field_slope_distribution(dfield, derdist, 5);
    symm = gwy_data_field_unrotate_find_corrections(derdist, correction);
    g_object_unref(derdist);

    ok = (run != GWY_RUN_MODAL)
         || unrotate_dialog(&args, data, correction, symm);
    if (run == GWY_RUN_MODAL)
        save_args(gwy_app_settings_get(), &args);
    if (ok) {
        if (args.symmetry)
            symm = args.symmetry;
        phi = 180.0/G_PI*correction[symm];
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
        gwy_app_clean_up_data(data);
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));

        gwy_data_field_rotate(dfield, phi, args.interp);
        if (gwy_container_gis_object_by_name(data, "/0/mask",
                                             (GObject**)&dfield))
            gwy_data_field_rotate(dfield, phi, args.interp);
        if (gwy_container_gis_object_by_name(data, "/0/show",
                                             (GObject**)&dfield))
            gwy_data_field_rotate(dfield, phi, args.interp);
        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    }

    return FALSE;
}

/* create a smaller copy of data */
static GwyContainer*
create_preview_data(GwyContainer *data)
{
    GwyContainer *preview;
    GObject *dfield;
    gint xres, yres;
    gdouble zoomval;

    preview = GWY_CONTAINER(gwy_container_duplicate_by_prefix(data,
                                                              "/0/data",
                                                              "/0/base/palette",
                                                              NULL));
    dfield = gwy_container_get_object_by_name(preview, "/0/data");
    xres = gwy_data_field_get_xres(GWY_DATA_FIELD(dfield));
    yres = gwy_data_field_get_yres(GWY_DATA_FIELD(dfield));
    zoomval = (gdouble)PREVIEW_SIZE/MAX(xres, yres);
    gwy_data_field_resample(GWY_DATA_FIELD(dfield), xres*zoomval, yres*zoomval,
                            GWY_INTERPOLATION_BILINEAR);
    dfield = gwy_serializable_duplicate(dfield);
    gwy_container_set_object_by_name(preview, "/0/show", dfield);
    g_object_unref(dfield);

    return preview;
}

static gboolean
unrotate_dialog(UnrotateArgs *args,
                GwyContainer *data,
                gdouble *correction,
                GwyPlaneSymmetry guess)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *label, *hbox;
    GtkObject *layer;
    UnrotateControls controls;
    const gchar *s;
    gint response;
    gint row;

    controls.correction = correction;
    controls.guess = guess;
    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Correct Rotation"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    table = gtk_table_new(4, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Structure</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new(_("Detected:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    s = gwy_enum_to_string(guess,
                           unrotate_symmetry, G_N_ELEMENTS(unrotate_symmetry));
    label = gtk_label_new(_(s));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.symmetry
        = gwy_option_menu_create(unrotate_symmetry,
                                 G_N_ELEMENTS(unrotate_symmetry), "symmetry",
                                 G_CALLBACK(unrotate_symmetry_cb), &controls,
                                 args->symmetry);
    gwy_table_attach_row(table, row, _("_Assume:"), NULL, controls.symmetry);
    row++;

    label = gtk_label_new(_("Correction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    controls.corrlabel = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.corrlabel), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.corrlabel,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.interp
        = gwy_option_menu_interpolation(G_CALLBACK(unrotate_interp_cb),
                                        &controls, args->interp);
    gwy_table_attach_row(table, row, _("_Interpolation type:"), "",
                         controls.interp);

    controls.data = create_preview_data(data);
    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view),
                                 GWY_PIXMAP_LAYER(layer));
    gtk_box_pack_start(GTK_BOX(hbox), controls.data_view, FALSE, FALSE, 8);

    unrotate_dialog_update(&controls, args);

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

            case RESPONSE_RESET:
            *args = unrotate_defaults;
            unrotate_dialog_update(&controls, args);
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
unrotate_dialog_update(UnrotateControls *controls,
                       UnrotateArgs *args)
{
    gchar *lab;
    GwyPlaneSymmetry symm;
    GwyDataField *dfield, *rfield;
    GwyContainer *data;
    gdouble phi;

    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
    gwy_option_menu_set_history(controls->symmetry, "symmetry",
                                args->symmetry);

    symm = args->symmetry ? args->symmetry : controls->guess;
    phi = 180.0/G_PI*controls->correction[symm];
    lab = g_strdup_printf("%.2f %s", phi, _("deg"));
    gtk_label_set_text(GTK_LABEL(controls->corrlabel), lab);
    g_free(lab);

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->data_view));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    rfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/show"));
    gwy_data_field_area_copy(dfield, rfield, 0, 0,
                             gwy_data_field_get_xres(dfield),
                             gwy_data_field_get_yres(dfield),
                             0, 0);
    gwy_data_field_rotate(rfield, phi, args->interp);
    gwy_data_view_update(GWY_DATA_VIEW(controls->data_view));
}

static void
unrotate_symmetry_cb(GtkWidget *item, UnrotateControls *controls)
{
    controls->args->symmetry
        = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "symmetry"));
    unrotate_dialog_update(controls, controls->args);

}

static void
unrotate_interp_cb(GtkWidget *item, UnrotateControls *controls)
{
    controls->args->interp
        = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item),
                                             "interpolation-type"));
    unrotate_dialog_update(controls, controls->args);
}

static const gchar *interp_key = "/module/unrotate/interp";
static const gchar *symmetry_key = "/module/unrotate/symmetry";

static void
sanitize_args(UnrotateArgs *args)
{
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->symmetry = MIN(args->symmetry, GWY_SYMMETRY_LAST-1);
}

static void
load_args(GwyContainer *container,
          UnrotateArgs *args)
{
    *args = unrotate_defaults;

    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, symmetry_key, &args->symmetry);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          UnrotateArgs *args)
{
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, symmetry_key, args->symmetry);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
