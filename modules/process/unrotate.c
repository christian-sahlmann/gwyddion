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

typedef enum {
    UNROTATE_DETECT = 0,
    UNROTATE_PARALLEL,
    UNROTATE_TRIANGULAR,
    UNROTATE_SQUARE,
    UNROTATE_RHOMBIC,
    UNROTATE_HEXAGONAL,
    UNROTATE_LAST
} UnrotateSymmetry;

/* Data for this function. */
typedef struct {
    GwyInterpolationType interp;
    UnrotateSymmetry symmetry;
} UnrotateArgs;

typedef struct {
    GtkWidget *interp;
    GtkWidget *symmetry;
    GtkWidget *symmlabel;
    GtkWidget *corrlabel;
    UnrotateArgs *args;
    UnrotateSymmetry guess;
    gdouble *correction;
} UnrotateControls;

static gboolean         module_register          (const gchar *name);
static gboolean         unrotate                 (GwyContainer *data,
                                                  GwyRunType run);
static void             compute_angle_dist       (GwyDataField *dfield,
                                                  gint nder,
                                                  gdouble *der);
static UnrotateSymmetry find_all_corrections     (gint nder,
                                                  gdouble *der,
                                                  gdouble *correction);
static gdouble          compute_correction       (gint nder,
                                                  gdouble *der,
                                                  guint m,
                                                  gdouble phi);
static gboolean         unrotate_dialog          (UnrotateArgs *args,
                                                  gdouble *correction,
                                                  UnrotateSymmetry guess);
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

GwyEnum unrotate_symmetry[] = {
    { "Detected",   UNROTATE_DETECT     },
    { "Parallel",   UNROTATE_PARALLEL   },
    { "Triangular", UNROTATE_TRIANGULAR },
    { "Square",     UNROTATE_SQUARE     },
    { "Rhombic",    UNROTATE_RHOMBIC    },
    { "Hexagonal",  UNROTATE_HEXAGONAL  },
};

UnrotateArgs unrotate_defaults = {
    GWY_INTERPOLATION_BILINEAR,
    UNROTATE_DETECT,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "unrotate",
    "Rotates data to make main directions parallel with x or y axis.",
    "Yeti <yeti@gwyddion.net>",
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
    static GwyProcessFuncInfo unrotate_func_info = {
        "unrotate",
        "/_Correct data/_Unrotate...",  /* FIXME: a less silly name? */
        (GwyProcessFunc)&unrotate,
        UNROTATE_RUN_MODES,
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
    gdouble correction[UNROTATE_LAST];
    UnrotateSymmetry symm;
    gdouble *der;
    gdouble phi;
    gboolean ok;

    g_return_val_if_fail(run & UNROTATE_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = unrotate_defaults;
    else
        load_args(gwy_app_settings_get(), &args);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    der = g_new(gdouble, nder);
    compute_angle_dist(dfield, nder, der);
    symm = find_all_corrections(nder, der, correction);
    g_free(der);

    ok = (run != GWY_RUN_MODAL) || unrotate_dialog(&args, correction, symm);
    if (ok) {
        if (args.symmetry)
            symm = args.symmetry;
        phi = 360*correction[symm];
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

        if (run != GWY_RUN_WITH_DEFAULTS)
            save_args(gwy_app_settings_get(), &args);
    }

    return FALSE;
}

static void
compute_angle_dist(GwyDataField *dfield,
                   gint nder,
                   gdouble *der)
{
    enum { kernel_size = 5 };

    gdouble *data;
    gdouble bx, by, phi;
    gint xres, yres;
    gint col, row, iphi;

    data = gwy_data_field_get_data(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    memset(der, 0, nder*sizeof(gdouble));
    for (row = 0; row + kernel_size < yres; row++) {
        for (col = 0; col + kernel_size < xres; col++) {
            gwy_data_field_area_fit_plane(dfield, col, row,
                                          kernel_size, kernel_size,
                                          NULL, &bx, &by);
            phi = atan2(by, bx);
            iphi = (gint)floor(nder*(phi + G_PI)/(2.0*G_PI));
            iphi = CLAMP(iphi, 0, nder-1);
            der[iphi] += sqrt(bx*bx + by*by);
        }
    }
}

/**
 * find_correction:
 * @nder: Size of @der.
 * @der: Angular derivation distribution (as in Slope dist. graph).
 * @correction: Corrections for particular symmetry types will be stored
 *              here (indexed by UnrotateSymmetry). @correction[0] contains
 *              the most probable correction.
 *              XXX: The values are in 0..1 range!  NOT degrees or radians!
 *
 * Find corrections for all possible symmetries and guess which one should
 * be used.
 *
 * Returns: The guessed type of symmetry.
 **/
static UnrotateSymmetry
find_all_corrections(gint nder,
                     gdouble *der,
                     gdouble *correction)
{
    static const guint symm[] = { 2, 3, 4, 6 };
    UnrotateSymmetry guess, t;
    gint i;
    gsize j, m;
    gdouble x, avg, max, total, phi;
    gdouble sint[G_N_ELEMENTS(symm)], cost[G_N_ELEMENTS(symm)];

    avg = 0.0;
    for (i = 0; i < nder; i++)
        avg += der[i];
    avg /= nder;

    guess = UNROTATE_DETECT;
    max = -G_MAXDOUBLE;
    for (j = 0; j < G_N_ELEMENTS(symm); j++) {
        m = symm[j];
        sint[j] = cost[j] = 0.0;
        for (i = 0; i < nder; i++) {
            x = 2*G_PI*(i + 0.5)/nder;

            sint[j] += sin(m*x)*(der[i] - avg);
            cost[j] += cos(m*x)*(der[i] - avg);
        }

        phi = atan2(-sint[j], cost[j]);
        total = sqrt(sint[j]*sint[j] + cost[j]*cost[j]);

        gwy_debug("sc%d = (%f, %f), total%d = (%f, %f)",
                  m, sint[j], cost[j], m, total, 180.0/G_PI*phi);

        phi /= 2*G_PI*m;
        phi = compute_correction(nder, der, m, phi);
        t = sizeof("Die, die GCC warning!");
        switch (m) {
            case 2:
            t = UNROTATE_PARALLEL;
            correction[t] = phi;
            total /= 1.3;
            break;

            case 3:
            t = UNROTATE_TRIANGULAR;
            correction[t] = phi;
            break;

            case 4:
            t = UNROTATE_SQUARE;
            correction[t] = phi;
            phi += 0.5/m;
            if (phi > 0.5/m)
                phi -= 1.0/m;
            t = UNROTATE_RHOMBIC;
            correction[t] = phi;
            if (fabs(phi) > fabs(correction[UNROTATE_SQUARE]))
                t = UNROTATE_SQUARE;
            total /= 1.4;
            break;

            case 6:
            t = UNROTATE_HEXAGONAL;
            correction[t] = phi;
            break;

            default:
            g_assert_not_reached();
            break;
        }

        if (total > max) {
            max = total;
            guess = t;
        }
    }
    g_assert(guess != UNROTATE_DETECT);
    gwy_debug("SELECTED: %s",
              gwy_enum_to_string(guess, unrotate_symmetry,
                                 G_N_ELEMENTS(unrotate_symmetry)));
    correction[UNROTATE_DETECT] = correction[guess];

    return guess;
}

/**
 * compute_correction:
 * @nder: Size of @der.
 * @der: Angular derivation distribution (as in Slope dist. graph).
 * @m: Symmetry.
 * @phi: Initial correction guess (in the range 0..1!).
 *
 * Compute correction assuming symmetry @m and initial guess @phi.
 *
 * Returns: The correction (again in the range 0..1!).
 **/
static gdouble
compute_correction(gint nder, gdouble *der,
                   guint m, gdouble phi)
{
    gdouble sum, wsum;
    guint i, j;

    phi -= floor(phi) + 1.0;
    sum = wsum = 0.0;
    for (j = 0; j < m; j++) {
        gdouble low = (j + 5.0/6.0)/m - phi;
        gdouble high = (j + 7.0/6.0)/m - phi;
        gdouble s, w;
        guint ilow, ihigh;

        ilow = (guint)floor(low*nder);
        ihigh = (guint)floor(high*nder);
        gwy_debug("[%u] peak %u low = %f, high = %f, %u, %u",
                  m, j, low, high, ilow, ihigh);
        s = w = 0.0;
        for (i = ilow; i <= ihigh; i++) {
            s += (i + 0.5)*der[i % nder];
            w += der[i % nder];
        }

        s /= nder*w;
        gwy_debug("[%u] peak %u center: %f", m, j, 360*s);
        sum += (s - (gdouble)j/m)*w*w;
        wsum += w*w;
    }
    phi = sum/wsum;
    gwy_debug("[%u] FITTED phi = %f (%f)", m, phi, 360*phi);
    phi = fmod(phi + 1.0, 1.0/m);
    if (phi > 0.5/m)
        phi -= 1.0/m;
    gwy_debug("[%u] MINIMIZED phi = %f (%f)", m, phi, 360*phi);

    return phi;
}

static gboolean
unrotate_dialog(UnrotateArgs *args,
                gdouble *correction,
                UnrotateSymmetry guess)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *label;
    UnrotateControls controls;
    const gchar *s;
    gint response;
    gint row;

    controls.correction = correction;
    controls.guess = guess;
    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Correct Rotation"), NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(4, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    controls.symmetry
        = gwy_option_menu_create(unrotate_symmetry,
                                 G_N_ELEMENTS(unrotate_symmetry), "symmetry",
                                 G_CALLBACK(unrotate_symmetry_cb), &controls,
                                 args->symmetry);
    gwy_table_attach_row(table, row, _("Dominant _symmetry:"), "",
                         controls.symmetry);
    row++;

    label = gtk_label_new(_("Assume symmetry:"));
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

    label = gtk_label_new(_("Rotation correction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("deg"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    controls.corrlabel = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.corrlabel), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.corrlabel,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.interp
        = gwy_option_menu_interpolation(G_CALLBACK(unrotate_interp_cb),
                                        &controls, args->interp);
    gwy_table_attach_row(table, row, _("_Interpolation type:"), "",
                         controls.interp);

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
    UnrotateSymmetry symm;

    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
    gwy_option_menu_set_history(controls->symmetry, "symmetry",
                                args->symmetry);

    symm = args->symmetry ? args->symmetry : controls->guess;
    lab = g_strdup_printf("%.2f", 360*controls->correction[symm]);
    gtk_label_set_text(GTK_LABEL(controls->corrlabel), lab);
    g_free(lab);
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
load_args(GwyContainer *container,
          UnrotateArgs *args)
{
    *args = unrotate_defaults;

    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, symmetry_key, &args->symmetry);
    args->symmetry = MIN(args->symmetry, UNROTATE_LAST-1);
}

static void
save_args(GwyContainer *container,
          UnrotateArgs *args)
{
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, symmetry_key, args->symmetry);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
