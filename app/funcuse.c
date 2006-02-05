/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <app/gwyapp.h>

#define FUNC_DELTA_LOCAL (G_LN2/8.0)
#define FUNC_DELTA_GLOBAL (G_LN2/240.0)

typedef struct {
    GQuark quark;
    gdouble global;
    gdouble local;
} GwyFunctionUseInfo;

static GwyFunctionUse *process_use_info  = NULL;

static void
gwy_func_use_sort_up(GArray *functions,
                     guint pos)
{
    GwyFunctionUseInfo info;
    gdouble wi, w;

    if (!pos)
        return;

    info = g_array_index(functions, GwyFunctionUseInfo, pos);
    w = info.global + info.local;
    do {
        wi = (g_array_index(functions, GwyFunctionUseInfo, pos-1).global
              + g_array_index(functions, GwyFunctionUseInfo, pos-1).local);
        if (wi >= w)
            break;

        g_array_index(functions, GwyFunctionUseInfo, pos)
            = g_array_index(functions, GwyFunctionUseInfo, pos-1);
        pos--;
    } while (pos);

    g_array_index(functions, GwyFunctionUseInfo, pos) = info;
}

void
gwy_func_use_add(GwyFunctionUse *functions,
                 const gchar *name)
{
    GArray *funcs = (GArray*)functions;
    GQuark quark;
    guint i, found;

    /* Find function info, using linear search (but most used funcs are
     * near the head). */
    quark = g_quark_from_string(name);
    found = (guint)-1;
    for (i = 0; i < funcs->len; i++) {
        GwyFunctionUseInfo *info = &g_array_index(funcs, GwyFunctionUseInfo, i);

        if (info->quark == quark) {
            found = i;
            info->local += FUNC_DELTA_LOCAL;
            info->global += FUNC_DELTA_GLOBAL;
        }
        info->local /= 1.0 + FUNC_DELTA_LOCAL;
        info->global /= 1.0 + FUNC_DELTA_GLOBAL;
    }

    if (found == (guint)-1) {
        GwyFunctionUseInfo info;

        found = funcs->len;
        info.quark = quark;
        info.local = FUNC_DELTA_LOCAL/(1.0 + FUNC_DELTA_LOCAL);
        info.global = FUNC_DELTA_GLOBAL/(1.0 + FUNC_DELTA_GLOBAL);
        g_array_append_val(funcs, info);
    }

    gwy_func_use_sort_up(funcs, found);

    g_printerr("Funcs head:");
    for (i = 0; i < MIN(6, funcs->len); i++) {
        GwyFunctionUseInfo *info = &g_array_index(funcs, GwyFunctionUseInfo, i);

        g_printerr(" %s(%0.3f)",
                   g_quark_to_string(info->quark),
                   info->local + info->global);
    }
    g_printerr("\n");
}

const gchar*
gwy_func_use_get(GwyFunctionUse *functions,
                 guint i)
{
    GArray *funcs = (GArray*)functions;
    GwyFunctionUseInfo *info;

    if (i >= funcs->len)
        return NULL;

    info = &g_array_index(funcs, GwyFunctionUseInfo, i);
    return g_quark_to_string(info->quark);
}

GwyFunctionUse*
gwy_func_use_new(void)
{
    return (GwyFunctionUse*)g_array_new(FALSE, FALSE,
                                        sizeof(GwyFunctionUseInfo));
}

void
gwy_func_use_free(GwyFunctionUse *functions)
{
    GArray *funcs = (GArray*)functions;

    g_array_free(funcs, TRUE);
}

GwyFunctionUse*
gwy_func_use_load(const gchar *filename)
{
    GwyFunctionUseInfo info;
    GArray *funcs;
    gchar *buffer, *line, *p, *val;
    gsize len;

    funcs = g_array_new(FALSE, FALSE, sizeof(GwyFunctionUseInfo));
    if (!g_file_get_contents(filename, &buffer, &len, NULL))
        return (GwyFunctionUse*)funcs;

    p = buffer;
    info.local = 0.0;
    while ((line = gwy_str_next_line(&p))) {
        g_strstrip(line);
        val = strchr(line, ' ');
        if (!val)
            continue;
        *val = '\0';
        info.global = g_ascii_strtod(val+1, NULL);
        if (!info.global)
            continue;
        info.quark = g_quark_from_string(line);
        g_array_append_val(funcs, info);
    }
    g_free(buffer);

    return (GwyFunctionUse*)funcs;
}

void
gwy_func_use_save(GwyFunctionUse *functions,
                  const gchar *filename)
{
    GArray *funcs = (GArray*)functions;
    gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
    guint i;
    FILE *fh;

    fh = g_fopen(filename, "w");
    if (!fh)
        return;

    for (i = 0; i < funcs->len; i++) {
        GwyFunctionUseInfo *info;

        info = &g_array_index(funcs, GwyFunctionUseInfo, i);
        g_ascii_dtostr(buf, sizeof(buf), info->global);
        fprintf(fh, "%s %s\n", g_quark_to_string(info->quark), buf);
    }

    fclose(fh);
}

gchar*
gwy_func_use_get_filename(const gchar *type)
{
    gchar *name, *path;

    g_return_val_if_fail(gwy_strisident(type, NULL, NULL), NULL);
    name = g_strconcat("funcuse-", type, NULL);
    path = g_build_filename(gwy_get_user_dir(), name, NULL);
    g_free(name);

    return path;
}

GwyFunctionUse*
gwy_process_func_get_use(void)
{
    gchar *filename;

    if (!process_use_info) {
        filename = gwy_func_use_get_filename("process");
        process_use_info = gwy_func_use_load(filename);
        g_free(filename);
    }

    return process_use_info;
}

void
gwy_process_func_save_use(void)
{
    gchar *filename;

    if (!process_use_info)
        return;

    filename = gwy_func_use_get_filename("process");
    gwy_func_use_save(process_use_info, filename);
    g_free(filename);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
