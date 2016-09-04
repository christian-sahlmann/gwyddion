/*
 *  $Id$
 *  Copyright (C) 2009-2016 David Necas (Yeti).
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

/**
 * [FILE-MAGIC-USERGUIDE]
 * XYZ data
 * .xyz .dat
 * Read Export
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/surface.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define EXTENSION ".xyz"

typedef struct {
    gchar *xy_units;
    gchar *z_units;
} RawXYZArgs;

typedef struct {
    RawXYZArgs *args;
    GwySurface *surface;
    GtkWidget *dialog;
    GtkWidget *xmin;
    GtkWidget *xmax;
    GtkWidget *xunit;
    GtkWidget *ymin;
    GtkWidget *ymax;
    GtkWidget *yunit;
    GtkWidget *zmin;
    GtkWidget *zmax;
    GtkWidget *zunit;
    GtkWidget *xy_units;
    GtkWidget *z_units;
} RawXYZControls;

static gboolean      module_register (void);
static gint          rawxyz_detect   (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* rawxyz_load     (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static gboolean      rawxyz_dialog   (RawXYZArgs *arg,
                                      GwySurface *surface);
static void          xyunits_changed (RawXYZControls *controls,
                                      GtkEntry *entry);
static void          zunits_changed  (RawXYZControls *controls,
                                      GtkEntry *entry);
static gint          construct_units (RawXYZControls *controls,
                                      GtkTable *table,
                                      gint row);
static void          construct_range (GtkTable *table,
                                      const gchar *name,
                                      gint row,
                                      GtkWidget **from,
                                      GtkWidget **to,
                                      GtkWidget **unit);
static GwySurface*   read_xyz_points (gchar *p);
static void          rawxyz_load_args(GwyContainer *container,
                                      RawXYZArgs *args);
static void          rawxyz_save_args(GwyContainer *container,
                                      RawXYZArgs *args);

static const RawXYZArgs rawxyz_defaults = {
    NULL, NULL,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports raw XYZ data files."),
    "Yeti <yeti@gwyddion.net>",
    "3.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("rawxyz",
                           N_("XYZ data files"),
                           (GwyFileDetectFunc)&rawxyz_detect,
                           (GwyFileLoadFunc)&rawxyz_load,
                           NULL,
                           NULL);
    /* We provide a detection function, but the loading method tries a bit
     * harder, so let the user choose explicitly. */
    gwy_file_func_set_is_detectable("rawxyz", FALSE);

    return TRUE;
}

static gint
rawxyz_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    const gchar *s;
    gchar *end;
    guint i;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    s = fileinfo->head;
    for (i = 0; i < 6; i++) {
        g_ascii_strtod(s, &end);
        if (end == s) {
            /* If we encounter garbage at the first line, give it a one more
             * chance. */
            if (i || !(s = strchr(s, '\n')))
                return 0;
            goto next_line;
        }
        s = end;
        while (g_ascii_isspace(*s) || *s == ';' || *s == ',')
             s++;
        g_ascii_strtod(s, &end);
        if (end == s)
            return 0;
        s = end;
        while (g_ascii_isspace(*s) || *s == ';' || *s == ',')
             s++;
        g_ascii_strtod(s, &end);
        if (end == s)
            return 0;

        s = end;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s != '\n' && *s != '\r')
            return 0;

next_line:
        do {
            s++;
        } while (g_ascii_isspace(*s));
    }

    return 50;
}

static GwyContainer*
rawxyz_load(const gchar *filename,
            GwyRunType mode,
            GError **error)
{
    GwyContainer *settings, *container = NULL;
    GwySurface *surface = NULL;
    RawXYZArgs args;
    GwySIUnit *unit;
    gint power10;
    gdouble q;
    gchar *buffer = NULL;
    gsize size;
    GError *err = NULL;
    gboolean ok;
    guint k;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    surface = read_xyz_points(buffer);
    g_free(buffer);
    if (!surface->n) {
        err_NO_DATA(error);
        goto fail;
    }

    settings = gwy_app_settings_get();
    rawxyz_load_args(settings, &args);
    if (mode == GWY_RUN_INTERACTIVE) {
        ok = rawxyz_dialog(&args, surface);
        rawxyz_save_args(settings, &args);
        if (!ok) {
            err_CANCELLED(error);
            goto fail;
        }
    }

    unit = gwy_si_unit_new_parse(args.xy_units, &power10);
    if (power10) {
        q = pow10(power10);
        for (k = 0; k < surface->n; k++) {
            surface->data[k].x *= q;
            surface->data[k].y *= q;
        }
        gwy_surface_invalidate(surface);
    }
    gwy_serializable_clone(G_OBJECT(unit),
                           G_OBJECT(gwy_surface_get_si_unit_xy(surface)));

    unit = gwy_si_unit_new_parse(args.z_units, &power10);
    if (power10) {
        q = pow10(power10);
        for (k = 0; k < surface->n; k++)
            surface->data[k].z *= q;
        gwy_surface_invalidate(surface);
    }
    gwy_serializable_clone(G_OBJECT(unit),
                           G_OBJECT(gwy_surface_get_si_unit_z(surface)));

    container = gwy_container_new();
    gwy_container_set_object(container, gwy_app_get_surface_key_for_id(0),
                             surface);
    gwy_app_xyz_title_fall_back(container, 0);
    gwy_file_xyz_import_log_add(container, 0, NULL, filename);

fail:
    g_free(args.xy_units);
    g_free(args.z_units);
    gwy_object_unref(surface);

    return container;
}

static gboolean
rawxyz_dialog(RawXYZArgs *args,
              GwySurface *surface)
{
    GtkWidget *dialog, *label;
    GtkTable *table;
    RawXYZControls controls;
    gint row, response;
    gchar *s;

    controls.args = args;
    controls.surface = surface;

    dialog = gtk_dialog_new_with_buttons(_("Import XYZ Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_file_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    table = GTK_TABLE(gtk_table_new(6, 5, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(table),
                       TRUE, TRUE, 0);
    row = 0;

    label = gtk_label_new(_("Number of points:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    s = g_strdup_printf("%u", surface->n);
    label = gtk_label_new(s);
    g_free(s);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    construct_range(table, _("X-range:"), row++,
                    &controls.xmin, &controls.xmax, &controls.xunit);
    construct_range(table, _("Y-range:"), row++,
                    &controls.ymin, &controls.ymax, &controls.yunit);
    construct_range(table, _("Z-range:"), row++,
                    &controls.zmin, &controls.zmax, &controls.zunit);
    gtk_table_set_row_spacing(table, row-1, 8);

    row = construct_units(&controls, table, row);
    g_signal_connect_swapped(controls.xy_units, "changed",
                             G_CALLBACK(xyunits_changed), &controls);
    g_signal_connect_swapped(controls.z_units, "changed",
                             G_CALLBACK(zunits_changed), &controls);
    gtk_entry_set_text(GTK_ENTRY(controls.xy_units), args->xy_units);
    gtk_entry_set_text(GTK_ENTRY(controls.z_units), args->z_units);
    xyunits_changed(&controls, GTK_ENTRY(controls.xy_units));
    zunits_changed(&controls, GTK_ENTRY(controls.z_units));

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
construct_range(GtkTable *table, const gchar *name, gint row,
                GtkWidget **from, GtkWidget **to, GtkWidget **unit)
{
    GtkWidget *label;

    label = gtk_label_new(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_FILL, 0, 0, 0);

    *from = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 1, 2, row, row+1,
                     GTK_FILL, 0, 0, 0);

    label = gtk_label_new("–");
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_FILL, 0, 0, 0);

    *to = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 3, 4, row, row+1,
                     GTK_FILL, 0, 0, 0);

    *unit = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 4, 5, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
}

static gint
construct_units(RawXYZControls *controls,
                GtkTable *table,
                gint row)
{
    RawXYZArgs *args = controls->args;
    GtkWidget *label;

    label = gtk_label_new_with_mnemonic(_("_Lateral units:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls->xy_units = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls->xy_units);
    gtk_entry_set_text(GTK_ENTRY(controls->xy_units), args->xy_units);
    gtk_entry_set_width_chars(GTK_ENTRY(controls->xy_units), 6);
    gtk_table_attach(table, controls->xy_units, 1, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Value units:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls->z_units = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls->z_units);
    gtk_entry_set_text(GTK_ENTRY(controls->z_units), args->z_units);
    gtk_entry_set_width_chars(GTK_ENTRY(controls->z_units), 6);
    gtk_table_attach(table, controls->z_units, 1, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    return row;
}

static void
update_range_lables(GtkWidget *from, GtkWidget *to, GtkWidget *unit,
                    gdouble min, gdouble max, const gchar *unitstring)
{
    GwySIValueFormat *vf;
    GwySIUnit *siunit;
    gint power10;
    gchar *s;

    siunit = gwy_si_unit_new_parse(unitstring, &power10);
    min *= pow10(power10);
    max *= pow10(power10);
    vf = gwy_si_unit_get_format_with_digits(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            MAX(fabs(min), fabs(max)), 3,
                                            NULL);
    s = g_strdup_printf("%.*f", vf->precision, min/vf->magnitude);
    gtk_label_set_markup(GTK_LABEL(from), s);
    g_free(s);
    s = g_strdup_printf("%.*f", vf->precision, max/vf->magnitude);
    gtk_label_set_markup(GTK_LABEL(to), s);
    g_free(s);
    gtk_label_set_markup(GTK_LABEL(unit), vf->units);
    gwy_si_unit_value_format_free(vf);
    g_object_unref(siunit);
}

static void
xyunits_changed(RawXYZControls *controls,
                GtkEntry *entry)
{
    RawXYZArgs *args = controls->args;
    gdouble xmin, xmax, ymin, ymax;
    gchar *s;

    s = args->xy_units;
    args->xy_units = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, G_MAXINT);
    g_free(s);
    gwy_surface_get_xrange(controls->surface, &xmin, &xmax);
    update_range_lables(controls->xmin, controls->xmax, controls->xunit,
                        xmin, xmax, args->xy_units);
    gwy_surface_get_yrange(controls->surface, &ymin, &ymax);
    update_range_lables(controls->ymin, controls->ymax, controls->yunit,
                        ymin, ymax, args->xy_units);
}

static void
zunits_changed(RawXYZControls *controls,
               GtkEntry *entry)
{
    RawXYZArgs *args = controls->args;
    gdouble zmin, zmax;
    gchar *s;

    s = args->z_units;
    args->z_units = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, G_MAXINT);
    g_free(s);
    gwy_surface_get_min_max(controls->surface, &zmin, &zmax);
    update_range_lables(controls->zmin, controls->zmax, controls->zunit,
                        zmin, zmax, args->z_units);
}

static gchar
figure_out_comma_fix_char(const gchar *line)
{
    gchar *comma, *end;

    /* Not a number, try again. */
    if (!g_ascii_strtod(line, &end) && end == line)
        return 0;

    /* There are decimal dots => POSIX. */
    if (strchr(line, '.'))
        return ' ';

    /* There are no commas => POSIX. */
    comma = strchr(line, ',');
    if (!comma)
        return ' ';

    /* There are spaces after commas => POSIX. */
    if (g_regex_match_simple(",[ \t]", line, G_REGEX_NO_AUTO_CAPTURE, 0))
        return ' ';

    /* There is a contiguous block of digits and commas => POSIX. */
    if (g_regex_match_simple("[0-9],[0-9]+,[0-9]", line,
                             G_REGEX_NO_AUTO_CAPTURE, 0))
        return ' ';

    /* There are commas and may actually be inside numbers.  Assume the decimal
     * separator is coma. */
    return '.';
}

static GwySurface*
read_xyz_points(gchar *p)
{
    GwySurface *surface;
    GArray *points;
    gchar *line, *end;
    char comma_fix_char = 0;

    points = g_array_new(FALSE, FALSE, sizeof(GwyXYZ));
    for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
        GwyXYZ pt;

        if (!line[0] || line[0] == '#')
            continue;

        if (!comma_fix_char) {
            comma_fix_char = figure_out_comma_fix_char(line);
            if (!comma_fix_char)
                continue;
        }

        for (end = line; *end; end++) {
            if (*end == ';')
                *end = ' ';
            else if (*end == ',')
                *end = comma_fix_char;
        }

        if (!(pt.x = g_ascii_strtod(line, &end)) && end == line)
            continue;
        line = end;
        while (g_ascii_isspace(*line))
             line++;
        if (!(pt.y = g_ascii_strtod(line, &end)) && end == line)
            continue;
        line = end;
        while (g_ascii_isspace(*line))
             line++;
        if (!(pt.z = g_ascii_strtod(line, &end)) && end == line)
            continue;

        g_array_append_val(points, pt);
    }

    surface = gwy_surface_new_from_data((GwyXYZ*)points->data, points->len);
    g_array_free(points, TRUE);

    return surface;
}

static const gchar xy_units_key[] = "/module/rawxyz/xy-units";
static const gchar z_units_key[]  = "/module/rawxyz/z-units";

static void
rawxyz_load_args(GwyContainer *container,
                 RawXYZArgs *args)
{
    *args = rawxyz_defaults;

    gwy_container_gis_string_by_name(container, xy_units_key,
                                     (const guchar**)&args->xy_units);
    gwy_container_gis_string_by_name(container, z_units_key,
                                     (const guchar**)&args->z_units);

    args->xy_units = g_strdup(args->xy_units ? args->xy_units : "");
    args->z_units = g_strdup(args->z_units ? args->z_units : "");
}

static void
rawxyz_save_args(GwyContainer *container,
                 RawXYZArgs *args)
{
    gwy_container_set_const_string_by_name(container, xy_units_key,
                                           args->xy_units);
    gwy_container_set_const_string_by_name(container, z_units_key,
                                           args->z_units);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
