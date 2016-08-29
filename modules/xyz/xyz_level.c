/*
 *  $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/surface.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-xyz.h>
#include <app/gwyapp.h>

#define XYZLEVEL_RUN_MODES (GWY_RUN_INTERACTIVE | GWY_RUN_IMMEDIATE)

typedef enum {
    XYZ_LEVEL_SUBTRACT = 0,
    XYZ_LEVEL_ROTATE = 1,
} XYZLevelType;

typedef struct {
    XYZLevelType method;
    gboolean update_all;
} XYZLevelArgs;

typedef struct {
    XYZLevelArgs *args;
    GtkWidget *dialogue;
    GSList *method;
    GtkWidget *update_all;
} XYZLevelControls;

static gboolean module_register   (void);
static void     xyzfixzero        (GwyContainer *data,
                                   GwyRunType run);
static void     xyzzeromean       (GwyContainer *data,
                                   GwyRunType run);
static void     xyzlevel          (GwyContainer *data,
                                   GwyRunType run);
static gboolean xyzlevel_dialogue (XYZLevelArgs *arg);
static void     method_changed    (GtkToggleButton *toggle,
                                   XYZLevelControls *controls);
static void     update_all_changed(GtkToggleButton *toggle,
                                   XYZLevelControls *controls);
static void     xyzlevel_do       (GwySurface *surface,
                                   GwyContainer *data,
                                   gint id,
                                   const XYZLevelArgs *args);
static void     level_rotate_xyz  (GwySurface *surface,
                                   gdouble bx,
                                   gdouble by,
                                   const GwyXYZ *c);
static void     rotate_xyz        (GwySurface *surface,
                                   const GwyXYZ *u,
                                   const GwyXYZ *c,
                                   gdouble phi);
static void     find_plane_coeffs (GwySurface *surface,
                                   gdouble *a,
                                   gdouble *bx,
                                   gdouble *by,
                                   GwyXYZ *c);
static void     xyzlevel_load_args(GwyContainer *container,
                                   XYZLevelArgs *args);
static void     xyzlevel_save_args(GwyContainer *container,
                                   XYZLevelArgs *args);

static const XYZLevelArgs xyzlevel_defaults = {
    XYZ_LEVEL_ROTATE, TRUE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Simple XYZ data leveling."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_xyz_func_register("xyz_fix_zero",
                          (GwyXYZFunc)&xyzfixzero,
                          N_("/Fix _Zero"),
                          NULL,
                          XYZLEVEL_RUN_MODES,
                          GWY_MENU_FLAG_XYZ,
                          N_("Shift minimum data value to zero"));
    gwy_xyz_func_register("xyz_zero_mean",
                          (GwyXYZFunc)&xyzzeromean,
                          N_("/Zero _Mean Value"),
                          NULL,
                          XYZLEVEL_RUN_MODES,
                          GWY_MENU_FLAG_XYZ,
                          N_("Shift mean data value to zero"));
    gwy_xyz_func_register("xyz_fixzero",
                          (GwyXYZFunc)&xyzlevel,
                          N_("/Plane _Level..."),
                          NULL,
                          XYZLEVEL_RUN_MODES,
                          GWY_MENU_FLAG_XYZ,
                          N_("Level data by mean plane correction"));

    return TRUE;
}

static void
xyzfixzero(GwyContainer *data, G_GNUC_UNUSED GwyRunType run)
{
    GwySurface *surface = NULL;
    GQuark quark;
    gint id;
    GwyXYZ *xyz;
    guint k, n;
    gdouble zmin, zmax;

    gwy_app_data_browser_get_current(GWY_APP_SURFACE, &surface,
                                     GWY_APP_SURFACE_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_SURFACE(surface));

    quark = gwy_app_get_surface_key_for_id(id);
    gwy_app_undo_qcheckpointv(data, 1, &quark);

    gwy_surface_get_min_max(surface, &zmin, &zmax);
    xyz = gwy_surface_get_data(surface);
    n = gwy_surface_get_npoints(surface);
    for (k = 0; k < n; k++)
        xyz[k].z -= zmin;

    gwy_surface_data_changed(surface);
}

/* FIXME: We should use mean weighted by area.  But that must wait until we
 * can do such thing... */
static void
xyzzeromean(GwyContainer *data, G_GNUC_UNUSED GwyRunType run)
{
    GwySurface *surface = NULL;
    GQuark quark;
    gint id;
    GwyXYZ *xyz;
    guint k, n;
    gdouble zmean = 0.0;

    gwy_app_data_browser_get_current(GWY_APP_SURFACE, &surface,
                                     GWY_APP_SURFACE_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_SURFACE(surface));

    quark = gwy_app_get_surface_key_for_id(id);
    gwy_app_undo_qcheckpointv(data, 1, &quark);

    xyz = gwy_surface_get_data(surface);
    n = gwy_surface_get_npoints(surface);
    for (k = 0; k < n; k++)
        zmean += xyz[k].z;
    zmean /= n;
    for (k = 0; k < n; k++)
        xyz[k].z -= zmean;

    gwy_surface_data_changed(surface);
}

static void
xyzlevel(GwyContainer *data, GwyRunType run)
{
    XYZLevelArgs args;

    GwyContainer *settings;
    GwySurface *surface = NULL;
    gboolean ok = TRUE;
    gint id;

    g_return_if_fail(run & XYZLEVEL_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_SURFACE, &surface,
                                     GWY_APP_SURFACE_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_SURFACE(surface));

    settings = gwy_app_settings_get();
    xyzlevel_load_args(settings, &args);

    if (run == GWY_RUN_INTERACTIVE)
        ok = xyzlevel_dialogue(&args);

    xyzlevel_save_args(settings, &args);

    if (ok)
        xyzlevel_do(surface, data, id, &args);
}

static gboolean
xyzlevel_dialogue(XYZLevelArgs *args)
{
    static const GwyEnum methods[] = {
        { N_("Plane subtraction"), XYZ_LEVEL_SUBTRACT, },
        { N_("Rotation"),          XYZ_LEVEL_ROTATE,   },
    };

    GtkWidget *dialogue, *label, *check;
    GtkTable *table;
    XYZLevelControls controls;
    gint row, response;

    gwy_clear(&controls, 1);
    controls.args = args;

    dialogue = gtk_dialog_new_with_buttons(_("Level XYZ Data"), NULL, 0,
                                           GTK_STOCK_CANCEL,
                                           GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_OK,
                                           GTK_RESPONSE_OK,
                                           NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialogue), GTK_RESPONSE_OK);
    gwy_help_add_to_xyz_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);
    controls.dialogue = dialogue;

    table = GTK_TABLE(gtk_table_new(4, 4, FALSE));
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialogue)->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("Method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.method = gwy_radio_buttons_create(methods, G_N_ELEMENTS(methods),
                                               G_CALLBACK(method_changed),
                                               &controls,
                                               args->method);
    row = gwy_radio_buttons_attach_to_table(controls.method, table, 3, row);

    check = gtk_check_button_new_with_mnemonic(_("Update X and Y of _all"
                                                 "compatible data"));
    controls.update_all = check;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), args->update_all);
    gtk_widget_set_sensitive(check, args->method == XYZ_LEVEL_ROTATE);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(update_all_changed), &controls);
    gtk_table_attach(table, controls.update_all, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    gtk_widget_show_all(dialogue);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialogue));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialogue);
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

    gtk_widget_destroy(dialogue);

    return TRUE;
}

static void
method_changed(GtkToggleButton *toggle,
               XYZLevelControls *controls)
{
    XYZLevelArgs *args = controls->args;

    if (toggle && !gtk_toggle_button_get_active(toggle))
        return;

    args->method = gwy_radio_buttons_get_current(controls->method);
    gtk_widget_set_sensitive(controls->update_all,
                             args->method == XYZ_LEVEL_ROTATE);
}

static void
update_all_changed(GtkToggleButton *toggle,
                   XYZLevelControls *controls)
{
    controls->args->update_all = gtk_toggle_button_get_active(toggle);
}

static void
xyzlevel_do(GwySurface *surface,
            GwyContainer *data,
            gint id,
            const XYZLevelArgs *args)
{
    GQuark otherquark, quark = gwy_app_get_surface_key_for_id(id);
    GQuark *allquarks = NULL;
    GwySurface *othersurface;
    GwyXYZ *xyz, c;
    const GwyXYZ *newxyz;
    guint k, n, kq, nq = 0;
    gdouble a, bx, by;
    gint *ids = NULL;

    if (args->method == XYZ_LEVEL_SUBTRACT) {
        gwy_app_undo_qcheckpointv(data, 1, &quark);
        find_plane_coeffs(surface, &a, &bx, &by, &c);
        xyz = gwy_surface_get_data(surface);
        n = gwy_surface_get_npoints(surface);
        for (k = 0; k < n; k++)
            xyz[k].z -= a + bx*xyz[k].x + by*xyz[k].y;
        gwy_surface_data_changed(surface);

        return;
    }

    if (args->update_all) {
        ids = gwy_app_data_browser_get_xyz_ids(data);
        for (kq = 0; ids[kq] > -1; kq++) {
            if (ids[kq] == id) {
                ids[nq++] = ids[kq];
            }
            else {
                otherquark = gwy_app_get_surface_key_for_id(ids[kq]);
                othersurface = gwy_container_get_object(data, otherquark);
                if (gwy_surface_xy_is_compatible(surface, othersurface))
                    ids[nq++] = ids[kq];
            }
        }
        ids[nq] = -1;

        g_assert(nq);
        allquarks = g_new(GQuark, nq);
        for (kq = 0; kq < nq; kq++)
            allquarks[kq] = gwy_app_get_surface_key_for_id(ids[kq]);
        gwy_app_undo_qcheckpointv(data, nq, allquarks);
        g_free(allquarks);
    }
    else
        gwy_app_undo_qcheckpointv(data, 1, &quark);

    /* find_plane_coeffs() calculates the mean plane in ordinary
     * least-squares sense.  But this is not self-consistent with rotation
     * that should use total least squares.  Perform a few iterations.
     * The procedure converges quadratically because when the mean plane is
     * already close to z=0 rotation and subtraction differ only in the
     * second order.  XXX: But it does not seem to do so? */
    for (k = 0; k < 12; k++) {
        find_plane_coeffs(surface, &a, &bx, &by, &c);
        level_rotate_xyz(surface, bx, by, &c);
        if (k > 1 && sqrt(bx*bx + by*by) < 1e-15)
            break;
    }

    gwy_surface_data_changed(surface);
    if (!args->update_all)
        return;

    newxyz = gwy_surface_get_data_const(surface);
    n = gwy_surface_get_npoints(surface);
    for (kq = 0; kq < nq; kq++) {
        if (ids[kq] == id)
            continue;

        otherquark = gwy_app_get_surface_key_for_id(ids[kq]);
        othersurface = gwy_container_get_object(data, otherquark);
        xyz = gwy_surface_get_data(othersurface);
        for (k = 0; k < n; k++) {
            xyz[k].x = newxyz[k].x;
            xyz[k].y = newxyz[k].y;
        }
        gwy_surface_data_changed(surface);
    }
    g_free(ids);
}

static void
level_rotate_xyz(GwySurface *surface, gdouble bx, gdouble by, const GwyXYZ *c)
{
    gdouble b = sqrt(bx*bx + by*by);
    GwyXYZ u;

    if (!b)
        return;

    u.x = -by/b;
    u.y = bx/b;
    u.z = 0.0;
    rotate_xyz(surface, &u, c, atan2(b, 1.0));
}

static void
rotate_xyz(GwySurface *surface, const GwyXYZ *u, const GwyXYZ *c, gdouble phi)
{
    GwyXYZ *xyz = gwy_surface_get_data(surface);
    guint k, n = gwy_surface_get_npoints(surface);
    gdouble cphi = cos(phi), sphi = sin(phi);
    gdouble axx = cphi + u->x*u->x*(1.0 - cphi);
    gdouble axy = u->x*u->y*(1.0 - cphi) - u->z*sphi;
    gdouble axz = u->x*u->z*(1.0 - cphi) + u->y*sphi;
    gdouble ayx = u->y*u->x*(1.0 - cphi) + u->z*sphi;
    gdouble ayy = cphi + u->y*u->y*(1.0 - cphi);
    gdouble ayz = u->y*u->z*(1.0 - cphi) - u->x*sphi;
    gdouble azx = u->z*u->x*(1.0 - cphi) - u->y*sphi;
    gdouble azy = u->z*u->y*(1.0 - cphi) + u->x*sphi;
    gdouble azz = cphi + u->z*u->z*(1.0 - cphi);

    for (k = 0; k < n; k++) {
        gdouble x = xyz[k].x - c->x;
        gdouble y = xyz[k].y - c->y;
        gdouble z = xyz[k].z - c->z;

        xyz[k].x = axx*x + axy*y + axz*z + c->x;
        xyz[k].y = ayx*x + ayy*y + ayz*z + c->y;
        xyz[k].z = azx*x + azy*y + azz*z + c->z;
    }
}

static void
find_plane_coeffs(GwySurface *surface, gdouble *a, gdouble *bx, gdouble *by,
                  GwyXYZ *c)
{
    const GwyXYZ *xyz = gwy_surface_get_data_const(surface);
    guint k, n = gwy_surface_get_npoints(surface);
    gdouble sx, sy, sz, sxx, sxy, syy, sxz, syz, D;

    sx = sy = sz = 0.0;
    for (k = 0; k < n; k++) {
        sx += xyz[k].x;
        sy += xyz[k].y;
        sz += xyz[k].z;
    }
    sx /= n;
    sy /= n;
    sz /= n;

    sxx = sxy = syy = sxz = syz = 0.0;
    for (k = 0; k < n; k++) {
        gdouble x = xyz[k].x - sx;
        gdouble y = xyz[k].y - sy;
        gdouble z = xyz[k].z;

        sxx += x*x;
        syy += y*y;
        sxy += x*y;
        sxz += x*z;
        syz += y*z;
    }

    D = sxx*syy - sxy*sxy;
    *bx = (syy*sxz - sxy*syz)/D;
    *by = (sxx*syz - sxy*sxz)/D;
    *a = -(sx*(*bx) + sy*(*by));
    c->x = sx;
    c->y = sy;
    c->z = sz;
    gwy_debug("b %g %g", *bx, *by);
}

static const gchar method_key[]     = "/module/xyz_level/method";
static const gchar update_all_key[] = "/module/xyz_level/update_all";

static void
xyzlevel_sanitize_args(XYZLevelArgs *args)
{
    args->method = CLAMP(args->method, XYZ_LEVEL_SUBTRACT, XYZ_LEVEL_ROTATE);
    args->update_all = !!args->update_all;
}

static void
xyzlevel_load_args(GwyContainer *container,
                   XYZLevelArgs *args)
{
    *args = xyzlevel_defaults;

    gwy_container_gis_enum_by_name(container, method_key, &args->method);
    gwy_container_gis_boolean_by_name(container, update_all_key,
                                      &args->update_all);
    xyzlevel_sanitize_args(args);
}

static void
xyzlevel_save_args(GwyContainer *container,
                   XYZLevelArgs *args)
{
    gwy_container_set_enum_by_name(container, method_key, args->method);
    gwy_container_set_boolean_by_name(container, update_all_key,
                                      args->update_all);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
