/*
 *  @(#) $Id# 
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

/* XXX: extreme brain damage
 * This file contains code common to the plugin proxies.
 * Should be definitely solved better... */

#ifdef __PLUGIN_PROXY_COMMON_C__
#error Cannot be included twice!
#else
#define __PLUGIN_PROXY_COMMON_C__

static FILE*          text_dump_export           (GwyContainer *data,
                                                  gchar **filename);
static void           dump_export_meta_cb        (gpointer hkey,
                                                  GValue *value,
                                                  FILE *fh);
static void           dump_export_data_field     (GwyDataField *dfield,
                                                  const gchar *name,
                                                  FILE *fh);
static GwyContainer*  text_dump_import           (GwyContainer *old_data,
                                                  gchar *buffer,
                                                  gsize size);
static gint           str_to_run_modes           (const gchar *str);
static const char*    run_mode_to_str            (gint run);
static gchar*         next_line                  (gchar **buffer);

static FILE*
text_dump_export(GwyContainer *data, gchar **filename)
{
    GwyDataField *dfield;
    GError *err = NULL;
    FILE *fh;
    gint fd;

    fd = g_file_open_tmp("gwydXXXXXXXX", filename, &err);
    if (fd < 0) {
        g_warning("Cannot create a temporary file: %s", err->message);
        return NULL;
    }
    fh = fdopen(fd, "wb");
    gwy_container_foreach(data, "/meta", (GHFunc)dump_export_meta_cb, fh);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    dump_export_data_field(dfield, "/0/data", fh);
    if (gwy_container_contains_by_name(data, "/0/mask")) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/mask"));
        dump_export_data_field(dfield, "/0/mask", fh);
    }
    fflush(fh);

    return fh;
}

static void
dump_export_meta_cb(gpointer hkey, GValue *value, FILE *fh)
{
    GQuark quark = GPOINTER_TO_UINT(hkey);
    const gchar *key;

    key = g_quark_to_string(quark);
    g_return_if_fail(key);
    g_return_if_fail(G_VALUE_TYPE(value) == G_TYPE_STRING);
    fprintf(fh, "%s=%s\n", key, g_value_get_string(value));
}

static void
dump_export_data_field(GwyDataField *dfield, const gchar *name, FILE *fh)
{
    gint xres, yres;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    fprintf(fh, "%s/xres=%d\n", name, xres);
    fprintf(fh, "%s/yres=%d\n", name, yres);
    fprintf(fh, "%s/xreal=%.16g\n", name, gwy_data_field_get_xreal(dfield));
    fprintf(fh, "%s/yreal=%.16g\n", name, gwy_data_field_get_yreal(dfield));
    fprintf(fh, "%s=[\n[", name);
    fflush(fh);
    fwrite(dfield->data, sizeof(gdouble), xres*yres, fh);
    fwrite("]]\n", 1, 3, fh);
    fflush(fh);
}

static GwyContainer*
text_dump_import(GwyContainer *old_data, gchar *buffer, gsize size)
{
    gchar *val, *key, *pos, *line;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble xreal, yreal;
    gint xres, yres;
    gsize n;

    if (old_data) {
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(old_data)));
        gwy_app_clean_up_data(data);
    }
    else
        data = GWY_CONTAINER(gwy_container_new());

    pos = buffer;
    while ((line = next_line(&pos)) && *line) {
        val = strchr(line, '=');
        if (!val || *line != '/') {
            g_warning("Garbage key: %s", line);
            continue;
        }
        if ((gsize)(val - buffer) + 1 > size) {
            g_critical("Unexpected end of file.");
            goto fail;
        }
        *val = '\0';
        val++;
        if (strcmp(val, "[") != 0) {
            gwy_debug("%s: <%s>=<%s>", __FUNCTION__,
                      line, val);
            if (*val)
                gwy_container_set_string_by_name(data, line, g_strdup(val));
            else
                gwy_container_remove_by_name(data, line);
            continue;
        }

        if (!pos || *pos != '[') {
            g_critical("Unexpected end of file.");
            goto fail;
        }
        pos++;
        if (gwy_container_contains_by_name(data, line))
            dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                     line));
        else
            dfield = NULL;

        /* get datafield parameters from already read values, failing back
         * to values of original data field */
        key = g_strconcat(line, "/xres", NULL);
        if (gwy_container_contains_by_name(data, key))
            xres = atoi(gwy_container_get_string_by_name(data, key));
        else if (dfield)
            xres = gwy_data_field_get_xres(dfield);
        else {
            g_critical("Broken dump doesn't specify data field width.");
            goto fail;
        }
        g_free(key);

        key = g_strconcat(line, "/yres", NULL);
        if (gwy_container_contains_by_name(data, key))
            yres = atoi(gwy_container_get_string_by_name(data, key));
        else if (dfield)
            yres = gwy_data_field_get_yres(dfield);
        else {
            g_critical("Broken dump doesn't specify data field height.");
            goto fail;
        }
        g_free(key);

        key = g_strconcat(line, "/xreal", NULL);
        if (gwy_container_contains_by_name(data, key))
            xreal = g_ascii_strtod(gwy_container_get_string_by_name(data, key),
                                   NULL);
        else if (dfield)
            xreal = gwy_data_field_get_xreal(dfield);
        else {
            g_critical("Broken dump doesn't specify real data field width.");
            xreal = 1;   /* 0 could cause troubles */
        }
        g_free(key);

        key = g_strconcat(line, "/yreal", NULL);
        if (gwy_container_contains_by_name(data, key))
            yreal = g_ascii_strtod(gwy_container_get_string_by_name(data, key),
                                   NULL);
        else if (dfield)
            yreal = gwy_data_field_get_yreal(dfield);
        else {
            g_critical("Broken dump doesn't specify real data field height.");
            yreal = 1;   /* 0 could cause troubles */
        }
        g_free(key);

        dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, xreal, yreal,
                                                   FALSE));

        n = xres*yres*sizeof(gdouble);
        if ((gsize)(pos - buffer) + n + 3 > size) {
            g_critical("Unexpected end of file.");
            goto fail;
        }
        memcpy(dfield->data, pos, n);
        pos += n;
        val = next_line(&pos);
        if (strcmp(val, "]]") != 0) {
            g_critical("Missed end of data field.");
            goto fail;
        }
        gwy_container_remove_by_prefix(data, line);
        gwy_container_set_object_by_name(data, line, G_OBJECT(dfield));
    }
    return data;

fail:
    gwy_container_remove_by_prefix(data, NULL);
    return NULL;
}

static gint
str_to_run_modes(const gchar *str)
{
    gchar **modes;
    gint run = 0;
    gsize i, j;

    modes = g_strsplit(str, " ", 0);
    for (i = 0; modes[i]; i++) {
        for (j = 0; j < G_N_ELEMENTS(run_mode_names); j++) {
            if (strcmp(modes[i], run_mode_names[j].str) == 0) {
                run |= run_mode_names[j].run;
                break;
            }
        }
    }
    g_strfreev(modes);

    return run;
}

static const char*
run_mode_to_str(gint run)
{
    gsize j;

    for (j = 0; j < G_N_ELEMENTS(run_mode_names); j++) {
        if (run & run_mode_names[j].run)
            return run_mode_names[j].str;
    }

    g_assert_not_reached();
    return "";
}

static gchar*
next_line(gchar **buffer)
{
    gchar *p, *q;

    if (!buffer || !*buffer)
        return NULL;

    q = *buffer;
    p = strchr(*buffer, '\n');
    if (p) {
        *buffer = p+1;
        *p = '\0';
    }
    else
        *buffer = NULL;

    return q;
}

#endif /* __PLUGIN_PROXY_COMMON_C__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
