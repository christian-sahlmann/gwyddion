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

struct _GwyFunctionUse {
    gdouble delta_local;
    gdouble delta_global;
    GArray *funcs;
};

static GwyFunctionUse *process_use_info  = NULL;

/**
 * gwy_func_use_info_compare:
 * @a: First #GwyFunctionUseInfo pointer.
 * @b: Second #GwyFunctionUseInfo pointer.
 *
 * Compares two func use infos for sorting.
 *
 * Returns: -1 if @a is used more than @b, 1 if @b is used more than @a, 0 if
 *          they are used the same.
 **/
static gint
gwy_func_use_info_compare(gconstpointer a,
                          gconstpointer b)
{
    const GwyFunctionUseInfo *ainfo, *binfo;
    gdouble aw, bw;

    ainfo = (const GwyFunctionUseInfo*)a;
    binfo = (const GwyFunctionUseInfo*)b;
    aw = ainfo->global + ainfo->local;
    bw = binfo->global + binfo->local;
    if (aw < bw)
        return 1;
    else if (aw > bw)
        return -1;
    return 0;
}

/**
 * gwy_func_use_sort_up:
 * @functions: #GArray with function use info.
 * @pos: Position of function whose usage count has increased (or which was
 *       newly inserted).
 *
 * Moves a function whose usage increased towards the head.
 **/
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
    GArray *funcs;
    GQuark quark;
    guint i, found;

    /* Find function info, using linear search (but most used funcs are
     * near the head). */
    funcs = functions->funcs;
    quark = g_quark_from_string(name);
    found = (guint)-1;
    for (i = 0; i < funcs->len; i++) {
        GwyFunctionUseInfo *info = &g_array_index(funcs, GwyFunctionUseInfo, i);

        if (info->quark == quark) {
            found = i;
            info->local += functions->delta_local;
            info->global += functions->delta_global;
        }
        info->local /= 1.0 + functions->delta_local;
        info->global /= 1.0 + functions->delta_global;
    }

    if (found == (guint)-1) {
        GwyFunctionUseInfo info;

        found = funcs->len;
        info.quark = quark;
        info.local = functions->delta_local/(1.0 + functions->delta_local);
        info.global = functions->delta_global/(1.0 + functions->delta_global);
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
    GwyFunctionUseInfo *info;

    if (i >= functions->funcs->len)
        return NULL;

    info = &g_array_index(functions->funcs, GwyFunctionUseInfo, i);
    return g_quark_to_string(info->quark);
}

GwyFunctionUse*
gwy_func_use_new(void)
{
    GwyFunctionUse *functions;

    functions = g_new(GwyFunctionUse, 1);
    functions->funcs = g_array_new(FALSE, FALSE, sizeof(GwyFunctionUseInfo));
    functions->delta_global = FUNC_DELTA_GLOBAL;
    functions->delta_local = FUNC_DELTA_LOCAL;

    return functions;
}

void
gwy_func_use_free(GwyFunctionUse *functions)
{
    g_array_free(functions->funcs, TRUE);
    g_free(functions);
}

GwyFunctionUse*
gwy_func_use_load(const gchar *filename)
{
    GwyFunctionUse *functions;
    GwyFunctionUseInfo info;
    GArray *funcs;
    gchar *buffer, *line, *p, *val;
    gsize len;

    functions = gwy_func_use_new();
    if (!g_file_get_contents(filename, &buffer, &len, NULL))
        return functions;

    funcs = functions->funcs;
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

    g_array_sort(funcs, &gwy_func_use_info_compare);

    return functions;
}

void
gwy_func_use_save(GwyFunctionUse *functions,
                  const gchar *filename)
{
    GArray *funcs;
    gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
    guint i;
    FILE *fh;

    fh = g_fopen(filename, "w");
    if (!fh)
        return;

    funcs = functions->funcs;
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
